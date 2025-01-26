/*
 * The functions in this file are a general set of line management utilities.
 * They are the only routines that touch the text. They also touch the buffer
 * and window structures, to make sure that the necessary updating gets done.
 * There are routines in this file that handle the kill register too. It isn't
 * here for any good reason.
 *
 * Note that this code only updates the dot and mark values in the window list.
 * Since all the code acts on the current window, the buffer that we are
 * editing must be being displayed, which means that "b_nwnd" is non zero,
 * which means that the dot and mark values in the buffer headers are nonsense.
 *
 * $Id: line.c,v 1.241 2025/01/26 14:25:36 tom Exp $
 */

/* #define POISON */
#ifdef POISON
#define poison(p,s) (void)memset((char *)p, 0xdf, s)
#else
#define poison(p,s)
#endif

#include "estruct.h"
#include "edef.h"

#include <assert.h>

#define roundlenup(n) (((size_t) (n) + NBLOCK - 1) & (size_t)~(NBLOCK-1))

static int doput(int f, int n, int after, REGIONSHAPE shape);
static int ldelnewline(void);
static int PutChar(int n, REGIONSHAPE shape);

#if OPT_SHOW_REGS && OPT_UPBUFF
static int will_relist_regs;
static int show_Registers(BUFFER *);
#define relist_registers() \
    if (!will_relist_regs) { \
	will_relist_regs = TRUE; \
	update_scratch(REGISTERS_BufName, show_Registers); \
    }
#else
#define relist_registers()
#endif

/*
 * Test the 'report' threshold, returning true if the argument is above it.
 */
int
do_report(L_NUM value)
{
    if (value < 0)
	value = -value;
    return (global_g_val(GVAL_REPORT) > 0
	    && global_g_val(GVAL_REPORT) <= value);
}

static LINE *
alloc_LINE(BUFFER *bp)
{
    LINE *lp = bp->b_freeLINEs;

    if (lp != NULL) {
	bp->b_freeLINEs = lp->l_nxtundo;
    } else {
	if ((lp = typealloc(LINE)) == NULL) {
	    (void) no_memory("LINE");
	}
	TRACE2(("alloc_LINE %p\n", lp));
    }
    return lp;
}

/*
 * This routine allocates a block of memory large enough to hold a LINE
 * containing "used" characters. The block is always rounded up a bit. Return
 * a pointer to the new block, or NULL if there isn't any memory left. Print a
 * message in the message line if no space.
 */
/*ARGSUSED*/
LINE *
lalloc(int used, BUFFER *bp)
{
    LINE *lp;
    size_t size;

    beginDisplay();
    /* lalloc(-1) is used by undo for placeholders */
    if (used < 0) {
	size = 0;
    } else {
	size = roundlenup(used);
    }
    if ((lp = alloc_LINE(bp)) != NULL) {
	lvalue(lp) = NULL;
	if (size && (lvalue(lp) = castalloc(char, size)) == NULL) {
	    (void) no_memory("LINE text");
	    poison(lp, sizeof(*lp));
	    FreeAndNull(lp);
	} else {
	    lp->l_size = size;
#if !SMALLER
	    lp->l_number = 0;
#endif
	    llength(lp) = used;
	    lsetclear(lp);
	    lp->l_nxtundo = NULL;
#if OPT_LINE_ATTRS
	    lp->l_attrs = NULL;
#endif
	}
    }
    endofDisplay();
    return lp;
}

/*ARGSUSED*/
void
lfree(LINE *lp, BUFFER *bp)
{
    beginDisplay();
    if (lisreal(lp))
	ltextfree(lp, bp);

    /* if the buffer doesn't have its own block of LINEs, or this
       one isn't in that range, free it */
    if (!bp->b_LINEs || lp < bp->b_LINEs || lp >= bp->b_LINEs_end) {
	TRACE2(("lfree(%p)\n", lp));
	poison(lp, sizeof(*lp));
	free((char *) lp);
    } else {
	/* keep track of freed buffer LINEs here */
	lp->l_nxtundo = bp->b_freeLINEs;
	bp->b_freeLINEs = lp;
#ifdef POISON
	/* catch references hard */
	set_lback(lp, (LINE *) 1);
	set_lforw(lp, (LINE *) 1);
	lvalue(lp) = (char *) 1;
	lp->l_size = llength(lp) = LINENOTREAL;
#endif
    }
    endofDisplay();
}

/*ARGSUSED*/
void
ltextfree(LINE *lp, BUFFER *bp)
{
    UCHAR *ltextp;

    beginDisplay();
    ltextp = (UCHAR *) lvalue(lp);
    if (ltextp) {
	lvalue(lp) = NULL;
	if (bp->b_ltext) {	/* could it be in the big range? */
	    if (ltextp < bp->b_ltext || ltextp >= bp->b_ltext_end) {
		poison(ltextp, lp->l_size);
		free((char *) ltextp);
	    }			/* else {
				   could keep track of freed big range text here;
				   } */
	} else {
	    poison(ltextp, lp->l_size);
	    free((char *) ltextp);
	}
    }
    /* else nothing to free */
#if OPT_LINE_ATTRS
    FreeAndNull(lp->l_attrs);
#endif
    endofDisplay();
}

/*
 * Delete line "lp".  Fix all of the links that might point at it (they are
 * moved to offset 0 of the next line.  Unlink the line from whatever buffer it
 * might be in.  The buffers are updated too; the magic conditions described in
 * the above comments don't hold here.
 *
 * Memory is not released unless the buffer is marked noundoable, so line can
 * be saved in undo stacks.
 */
void
lremove(BUFFER *bp, LINE *lp)
{
    WINDOW *wp;
    LINE *line_after;
    MARK mark_after;

    if (lp == buf_head(bp))
	return;

    line_after = lforw(lp);
    mark_after.l = line_after;
    mark_after.o = 0;

#if !WINMARK
    if (MK.l == lp)
	MK = mark_after;
#endif
    for_each_window(wp) {
	if (wp->w_line.l == lp)
	    wp->w_line.l = line_after;
	if (wp->w_dot.l == lp)
	    wp->w_dot = mark_after;
#if WINMARK
	if (wp->w_mark.l == lp)
	    wp->w_mark = mark_after;
#endif
#if 0
	if (wp->w_lastdot.l == lp)
	    wp->w_lastdot = mark_after;
#endif
    }
    if (bp->b_nwnd == 0) {
	if (bp->b_wline.l == lp)
	    bp->b_wline = mark_after;
	if (bp->b_dot.l == lp)
	    bp->b_dot = mark_after;
#if WINMARK
	if (bp->b_mark.l == lp)
	    bp->b_mark = mark_after;
#endif
#if 0
	if (bp->b_lastdot.l == lp)
	    bp->b_lastdot = mark_after;
#endif
    }
#if 0
    if (bp->b_nmmarks != NULL) {	/* fix the named marks */
	int i;
	for (i = 0; i < NMARKS; i++) {
	    if (bp->b_nmmarks[i].l == lp)
		bp->b_nmmarks[i] = nullmark;
	}
    }
#endif
#if OPT_VIDEO_ATTRS
    {
	AREGION **rpp = &(bp->b_attribs);
	AREGION *ap;

	while ((ap = *rpp) != NULL) {
	    AREGION **next = &(ap->ar_next);
	    int samestart = (ap->ar_region.r_orig.l == lp);
	    int sameend = (ap->ar_region.r_end.l == lp);

	    if (samestart && sameend) {
		free_attrib2(bp, rpp);
		next = rpp;
	    } else if (samestart) {
		ap->ar_region.r_orig = mark_after;
	    } else if (sameend) {
		ap->ar_region.r_end.l = lback(lp);
		ap->ar_region.r_end.o = llength(ap->ar_region.r_end.l);
	    }
	    rpp = next;
	}
    }
#endif /* OPT_VIDEO_ATTRS */
    set_lforw(lback(lp), lforw(lp));
    set_lback(lforw(lp), lback(lp));

    /*
     * If we've disabled undo stack, we'll have to free the line to avoid
     * a memory leak.
     */
    if (!b_val(bp, MDUNDOABLE)) {
	lfree(lp, bp);
    }
}

/*
 * Remove a line, and free its memory whether or not the buffer is undoable.
 */
void
lremove2(BUFFER *bp, LINE *lp)
{
    lremove(bp, lp);
    if (b_val(bp, MDUNDOABLE)) {
	lfree(lp, bp);
    }
}

/* insert spaces forward into text */
int
insspace(int f, int n)
{
    if (!lins_bytes(n, ' '))
	return FALSE;
    return backchar(f, n);
}

/*
 * Insert string forward into text.  The string may include embedded nulls.
 *
 * If 'tp' is null, treat as ""
 * If 'len' is non-zero, insert exactly this amount.  Pad if needed.
 */
int
lstrinsert(TBUFF *tp, int len)
{
    int rc = TRUE;

    if (len > 0) {
	int n;
	int limit = tb_length0(tp);
	int save_offset = DOT.o;

	if (tp != NULL) {
	    const char *string = tb_values(tp);
	    for (n = 0; n < len; ++n) {
		if (!lins_bytes(1, (n < limit) ? string[n] : ' ')) {
		    rc = FALSE;
		    break;
		}
	    }
	} else {
	    if (!lins_bytes(len, ' ')) {
		rc = FALSE;
	    }
	}
	DOT.o = save_offset;
    }

    return rc;
}

/*
 * replace a single character on a line.  _cannot_ be
 * use to replace a newline.
 */
int
lreplc(LINE *lp, C_NUM off, int c)
{

    if (off == llength(lp))
	return FALSE;

    if (lvalue(lp)[off] != (char) c) {
	CopyForUndo(lp);
	lvalue(lp)[off] = (char) c;

	chg_buff(curbp, WFEDIT);
    }

    return TRUE;
}

#define BumpOff(name, lp, n, doto) if (name.l == lp && name.o > doto) name.o += n

/*
 * Adjust DOT and MK in the given window, which may be the minibuffer.
 */
#if WINMARK
#define after_linsert(wp, lp, n, doto) \
{ \
    if (wp == curwp) { \
	if (wp->w_dot.l == lp) { \
	    wp->w_dot.o += n; \
	} \
    } else { \
	BumpOff (wp->w_dot, lp, n, doto); \
    } \
    BumpOff(wp->w_mark, lp, n, doto); \
    BumpOff(wp->w_lastdot, lp, n, doto); \
}
#else
#define after_linsert(wp, lp, n, doto) \
{ \
    if (wp == curwp) { \
	if (wp->w_dot.l == lp) { \
	    wp->w_dot.o += n; \
	} \
    } else { \
	BumpOff (wp->w_dot, lp, n, doto); \
    } \
    BumpOff(wp->w_lastdot, lp, n, doto); \
}
#endif

/*
 * Insert "n" copies of the byte "c" at the current location of dot. In
 * the easy case all that happens is the text is stored in the line. In the
 * hard case, the line has to be reallocated. When the window list is updated,
 * take special care; I screwed it up once. You always update dot in the
 * current window. You update mark, and a dot in another window, if it is
 * greater than the place where you did the insert. Return TRUE if all is
 * well, and FALSE on errors.
 */
int
lins_bytes(int n, int c)
{
    char *cp1;
    char *cp2;
    LINE *lp1;
    LINE *lp2;
    LINE *lp3;
    int doto;
    int i;
    WINDOW *wp;
    char *ntext;
    size_t nsize;
    int rc = TRUE;
    USHORT changed = 0;

    beginDisplay();

    assert(n >= 0);

    lp1 = DOT.l;		/* Current line         */
    if (n == 0) {
	;
    } else if (lp1 == buf_head(curbp)) {	/* At the end: special  */
	if (DOT.o != 0) {
	    mlforce("BUG: lins_bytes");
	    rc = (FALSE);
	} else if ((lp2 = lalloc(n, curbp)) == NULL) {
	    rc = (FALSE);
	} else {
	    lp3 = lback(lp1);	/* Previous line        */
	    set_lforw(lp3, lp2);	/* Link in              */
	    set_lforw(lp2, lp1);
	    set_lback(lp1, lp2);
	    set_lback(lp2, lp3);
	    (void) memset(lvalue(lp2), c, (size_t) n);

	    TagForUndo(lp2);

	    /* don't move DOT until after tagging for undo */
	    /*  (it's important in an empty buffer) */
	    DOT.l = lp2;
	    DOT.o = n;
	    changed = (WFINS | WFEDIT);
	}
    } else {
	doto = DOT.o;		/* Save for later.      */
	nsize = ((size_t) llength(lp1) + 1 + (size_t) doto + (size_t) n);
	if (nsize > lp1->l_size) {	/* Hard: reallocate     */
	    /* first, create the new image */
	    nsize = roundlenup(nsize);
	    CopyForUndo(lp1);
	    if ((ntext = castalloc(char, nsize)) == NULL) {
		rc = FALSE;
	    } else {
		if (lvalue(lp1) && doto) {	/* possibly NULL if l_size == 0 */
		    assert((size_t) doto < nsize);
		    (void) memcpy(&ntext[0], &lvalue(lp1)[0], (size_t) doto);
		}
		(void) memset(&ntext[doto], c, (size_t) n);
		if (lvalue(lp1)) {
#if OPT_LINE_ATTRS
		    UCHAR *l_attrs = lp1->l_attrs;
		    lp1->l_attrs = NULL;	/* momentarily detach */
#endif
		    if (llength(lp1) > doto) {
			(void) memcpy(&ntext[doto + n],
				      &lvalue(lp1)[doto],
				      ((size_t) llength(lp1) - (size_t) doto));
		    }
		    ltextfree(lp1, curbp);
#if OPT_LINE_ATTRS
		    lp1->l_attrs = l_attrs;	/* reattach */
#endif
		}
		lvalue(lp1) = ntext;
		lp1->l_size = nsize;
		llength(lp1) += n;
		assert((size_t) llength(lp1) <= lp1->l_size);
	    }
	} else {		/* Easy: in place       */
	    CopyForUndo(lp1);
	    changed = (WFEDIT);
	    /* don't use memcpy:  overlapping regions.... */
	    llength(lp1) += n;
	    assert((size_t) llength(lp1) <= lp1->l_size);
	    if (llength(lp1) > (n + doto)) {
		cp2 = &lvalue(lp1)[llength(lp1)];
		cp1 = cp2 - n;
		while (cp1 != &lvalue(lp1)[doto])
		    *--cp2 = *--cp1;
	    }
	    assert(llength(lp1) >= (n + doto));
	    for (i = 0; i < n; ++i) {	/* Add the characters       */
		lvalue(lp1)[doto + i] = (char) c;
	    }
	}
	if (rc != FALSE) {
	    int did_wminip = FALSE;

	    changed = (WFEDIT);
#if ! WINMARK
	    if (MK.l == lp1) {
		if (MK.o > doto)
		    MK.o += n;
	    }
#endif
	    for_each_window(wp) {	/* Update windows       */
		after_linsert(wp, lp1, n, doto);
		if (wp == wminip)
		    did_wminip = TRUE;
	    }
	    if (!did_wminip) {
		after_linsert(wminip, lp1, n, doto);
	    }
	    do_mark_iterate(mp, {
		if (mp->l == lp1) {
		    if (mp->o > doto)
			mp->o += n;
		}
	    });
#if OPT_LINE_ATTRS
	    if (lp1->l_attrs) {
		if (!lattr_shift(curbp, lp1, doto, n)) {
		    rc = FALSE;
		}
	    }
#endif
	}
    }

    if (changed)
	chg_buff(curbp, changed);

    endofDisplay();
    return (rc);
}

#if OPT_MULTIBYTE
/*
 * Insert 'n' copies of (potentially) multibyte character 'c'.  We could get
 * that by reading a \uXXXX sequence for instance, either in the minibuffer
 * or in insert-mode.
 */
int
lins_chars(int n, int c)
{
    int rc = FALSE;
    UCHAR target[MAX_UTF8];
    int nbytes;
    int nn;
    int mapped;

    TRACE((T_CALLED "lins_chars global %d, buffer %d U+%04X\n",
	   global_is_utfXX(),
	   b_is_utfXX(curbp), c));

    if ((c > 127) && b_is_utfXX(curbp)) {
	nbytes = vl_conv_to_utf8(target, (UINT) c, sizeof(target));
    } else if (okCTYPE2(vl_wide_enc) && !vl_mb_is_8bit(c)) {
	nbytes = 1;
	if (b_is_utfXX(curbp)) {
	    nbytes = vl_conv_to_utf8(target, (UINT) c, sizeof(target));
	} else if (vl_ucs_to_8bit(&mapped, c)) {
	    c = mapped;
	}
    } else {
	nbytes = 1;
    }
    if (nbytes > 1) {
	while (n-- > 0) {
	    for (nn = 0; nn < nbytes; ++nn) {
		rc = lins_bytes(1, target[nn]);
		if (rc != TRUE)
		    break;
	    }
	    if (rc != TRUE)
		break;
	}
    } else {
	rc = lins_bytes(n, c);
    }
    returnCode(rc);
}
#endif

/*
 * Insert a newline into the buffer at the current location of dot in the
 * current window. The funny ass-backwards way it does things is not a botch;
 * it just makes the last line in the file not a special case. Return TRUE if
 * everything works out and FALSE on error (memory allocation failure). The
 * update of dot and mark is a bit easier than in the above case, because the
 * split forces more updating.
 */
int
lnewline(void)
{
    int rc = FALSE;
    char *cp1;
    char *cp2;
    LINE *lp1;
    LINE *lp2;
    int doto;
    WINDOW *wp;
    USHORT changed = 0;

    lp1 = DOT.l;		/* Get the address and  */
    doto = DOT.o;		/* offset of "."        */

    if (lp1 == buf_head(curbp)
	&& lforw(lp1) == lp1) {
	/* empty buffer -- just  create empty line */
	lp2 = lalloc(doto, curbp);
	if (lp2 != NULL) {
	    /* put lp2 in below lp1 */
	    set_lforw(lp2, lforw(lp1));
	    set_lforw(lp1, lp2);
	    set_lback(lforw(lp2), lp2);
	    set_lback(lp2, lp1);

	    TagForUndo(lp2);

	    for_each_window(wp) {
		if (wp->w_line.l == lp1)
		    wp->w_line.l = lp2;
		if (wp->w_dot.l == lp1)
		    wp->w_dot.l = lp2;
	    }

	    changed = (WFHARD | WFINS);

	    rc = lnewline();	/* vi really makes _2_ lines */
	}
    } else {
	lp2 = lalloc(doto, curbp);	/* New first half line */
	if (lp2 != NULL) {
	    if (doto > 0) {
		CopyForUndo(lp1);
#if OPT_LINE_ATTRS
		if (lp1->l_attrs != NULL) {
		    if (doto == llength(lp1)) {
			lp2->l_attrs = lp1->l_attrs;
			lp1->l_attrs = NULL;
		    } else {
			UCHAR *newattr
			= typeallocn(UCHAR, (size_t) llength(lp1) + 1);
			int n;

			if (newattr != NULL) {
			    lp2->l_attrs = newattr;
			    lp2->l_attrs[doto] = EOS;
			    for (n = 0; (lp1->l_attrs[n] =
					 lp1->l_attrs[doto + n]) != EOS; ++n) {
				;
			    }
			} else {
			    FreeAndNull(lp1->l_attrs);
			}
		    }
		}
#endif
		cp1 = lvalue(lp1);	/* Shuffle text around  */
		cp2 = lvalue(lp2);
		while (cp1 < &lvalue(lp1)[doto])
		    *cp2++ = *cp1++;
		cp2 = lvalue(lp1);
		while (cp1 < &lvalue(lp1)[llength(lp1)])
		    *cp2++ = *cp1++;
		llength(lp1) -= doto;
	    }
	    /* put lp2 in above lp1 */
	    set_lback(lp2, lback(lp1));
	    set_lback(lp1, lp2);
	    set_lforw(lback(lp2), lp2);
	    set_lforw(lp2, lp1);

	    TagForUndo(lp2);
	    dumpuline(lp1);

#if ! WINMARK
	    if (MK.l == lp1) {
		if (MK.o < doto)
		    MK.l = lp2;
		else
		    MK.o -= doto;
	    }
#endif
	    for_each_window(wp) {
		if (wp->w_line.l == lp1)
		    wp->w_line.l = lp2;
		if (wp->w_dot.l == lp1) {
		    if (wp->w_dot.o < doto)
			wp->w_dot.l = lp2;
		    else
			wp->w_dot.o -= doto;
		}
#if WINMARK
		if (wp->w_mark.l == lp1) {
		    if (wp->w_mark.o < doto)
			wp->w_mark.l = lp2;
		    else
			wp->w_mark.o -= doto;
		}
#endif
		if (wp->w_lastdot.l == lp1) {
		    if (wp->w_lastdot.o < doto)
			wp->w_lastdot.l = lp2;
		    else
			wp->w_lastdot.o -= doto;
		}
	    }
	    do_mark_iterate(mp, {
		if (mp->l == lp1) {
		    if ((mp->o < doto) ||
			(doto == llength(lp2) && llength(lp1) == 0)) {
			mp->l = lp2;
		    } else {
			mp->o -= doto;
		    }
		}
	    });
	    changed = (WFHARD | WFINS);
	    rc = TRUE;
	}
    }

    if (changed) {
	chg_buff(curbp, changed);
    }

    return rc;
}

/*
 * This function deletes bytes, starting at dot.  It understands how to deal
 * with end of lines, etc.  It returns TRUE if all of the bytes were deleted,
 * and FALSE if they were not (because dot ran into the end of the buffer.
 *
 * parameters:
 * 'nbytes' is # of bytes (not multibyte chars!) to delete
 * 'kflag' is true if we put killed text in kill buffer
 */
int
ldel_bytes(B_COUNT nbytes, int kflag)
{
    char *cp1;
    char *cp2;
    LINE *dotp;
    LINE *nlp;
    C_NUM doto;
    B_COUNT uchunk;
    long schunk;
    WINDOW *wp;
    C_NUM i;
    int status = TRUE;
    B_COUNT len_rs = (B_COUNT) len_record_sep(curbp);

    lines_deleted = 0;
    while (nbytes != 0) {
	dotp = DOT.l;
	doto = DOT.o;
	if (dotp == buf_head(curbp)) {	/* Hit end of buffer. */
	    status = FALSE;
	    break;
	}
	if (doto > llength(dotp)) {
	    uchunk = 0;
	} else {
	    uchunk = (B_COUNT) (llength(dotp) - doto);	/* Size of chunk.  */
	}
	if (uchunk > nbytes) {
	    uchunk = nbytes;
	}

	schunk = (long) uchunk;
	if (schunk < 0) {
	    status = FALSE;
	    break;		/* oops: sign-extension */
	}

	if (uchunk == 0) {	/* End of line, merge.  */
	    /* first take out any whole lines below this one */
	    nlp = lforw(dotp);
	    while (nlp != buf_head(curbp)
		   && (line_length(nlp) < nbytes)) {
		if (kflag) {
		    status = kinsert('\n');
		    for (i = 0; i < llength(nlp) &&
			 status == TRUE; i++)
			status = kinsert(lgetc(nlp, i));
		}
		if (status != TRUE)
		    break;
		lremove(curbp, nlp);
		lines_deleted++;
		TossToUndo(nlp);
		assert((B_COUNT) line_length(nlp) <= nbytes);
		if (line_length(nlp) > nbytes)
		    nbytes = 0;
		else
		    nbytes -= line_length(nlp);
		nlp = lforw(dotp);
	    }
	    if (status != TRUE)
		break;
	    status = ldelnewline();
	    chg_buff(curbp, WFHARD | WFKILLS);
	    if (status != TRUE)
		break;
	    if (kflag && (status = kinsert('\n')) != TRUE)
		break;
	    nbytes = (nbytes > len_rs) ? (nbytes - len_rs) : 0;
	    lines_deleted++;
	    continue;
	}
	CopyForUndo(DOT.l);
	chg_buff(curbp, WFEDIT);

	cp1 = lvalue(dotp) + doto;	/* Scrunch text.     */
	cp2 = cp1 + schunk;
	if (kflag) {		/* Kill?                */
	    while (cp1 != cp2) {
		if ((status = kinsert(*cp1)) != TRUE)
		    break;
		++cp1;
	    }
	    if (status != TRUE)
		break;
	    cp1 = lvalue(dotp) + doto;
	}
	while (cp2 != lvalue(dotp) + llength(dotp))
	    *cp1++ = *cp2++;
	llength(dotp) -= (int) schunk;
#if ! WINMARK
	if (MK.l == dotp && MK.o > doto) {
	    MK.o -= (C_NUM) schunk;
	    if (MK.o < doto)
		MK.o = doto;
	}
#endif
	for_each_window(wp) {	/* Fix windows          */
	    if (wp->w_dot.l == dotp
		&& wp->w_dot.o > doto) {
		wp->w_dot.o -= (C_NUM) schunk;
		if (wp->w_dot.o < doto)
		    wp->w_dot.o = doto;
	    }
#if WINMARK
	    if (wp->w_mark.l == dotp
		&& wp->w_mark.o > doto) {
		wp->w_mark.o -= schunk;
		if (wp->w_mark.o < doto)
		    wp->w_mark.o = doto;
	    }
#endif
	    if (wp->w_lastdot.l == dotp
		&& wp->w_lastdot.o > doto) {
		wp->w_lastdot.o -= (C_NUM) schunk;
		if (wp->w_lastdot.o < doto)
		    wp->w_lastdot.o = doto;
	    }
	}
	do_mark_iterate(mp, {
	    if (mp->l == dotp
		&& mp->o > doto) {
		mp->o -= (C_NUM) schunk;
		if (mp->o < doto)
		    mp->o = doto;
	    }
	});
#if OPT_LINE_ATTRS
	if (!lattr_shift(curbp, dotp, doto, (int) -schunk)) {
	    status = FALSE;
	    break;
	}
#endif
	assert(uchunk != 0);
	assert(uchunk <= nbytes);
	nbytes -= uchunk;
    }
    return (status);
}

#if OPT_MULTIBYTE
/*
 * Regions and many other internal calls use byte-counts.  Interactive calls
 * use character-counts.  Use this function for deleting multiple characters.
 */
int
ldel_chars(B_COUNT nchars, int kflag)
{
    LINE *lp = DOT.l;
    int off = DOT.o;
    B_COUNT total = 0;

    while (nchars != 0) {
	int test_bytes = count_bytes(lp, off, (int) nchars);
	int test_chars = count_chars(lp, off, test_bytes);

	total += (B_COUNT) test_bytes;
	if ((B_COUNT) test_chars >= nchars)
	    break;

	lp = lforw(DOT.l);
	off = 0;

	total += (B_COUNT) len_record_sep(curbp);;
	nchars -= (B_COUNT) (1 + test_chars);
    }
    return ldel_bytes(total, kflag);
}
#endif

#if OPT_EVAL
/*
 * replace (part of) the current line with the passed in text, according to
 * chartype.
 *
 * np -	new text for the current line
 */
int
lrepl_ctype(CHARTYPE type, const char *np, int length)
{
    int status;
    int c;
    int n;

    TRACE((T_CALLED "lrepl_ctype:%s%lx:%.*s\n",
	   type ? "word" : "line",
	   (ULONG) type, length, np));

    if ((status = check_editable(curbp)) == TRUE) {

	mayneedundo();

	if (type != 0) {	/* it's an exact region */
	    regionshape = rgn_EXACT;
	    while (DOT.o < llength(DOT.l)
		   && HasCType(type, CharAtDot())) {
		if ((status = forwdelchar(FALSE, 1)) != TRUE)
		    break;
	    }
	} else {		/* it's the full line */
	    regionshape = rgn_FULLLINE;
	    DOT.o = w_left_margin(curwp);
	    if ((status = deltoeol(TRUE, 1)) == TRUE)
		DOT.o = w_left_margin(curwp);
	}

	if (status == TRUE) {
	    /* insert passed in chars */
	    for (n = 0; n < length; ++n) {
		c = np[n];
		if ((status = bputc(c)) != TRUE) {
		    break;
		}
	    }
	    if (type == 0) {
		status = lnewline();
		(void) backline(TRUE, 1);
	    }
	}
    }
    returnCode(status);
}

/*
 * replace (part of) the current line with the passed in text, according to
 * regular expression.
 *
 * np -	new text for the current line
 */
int
lrepl_regex(REGEXVAL * rexp, const char *np, int length)
{
    int status;
    int c;
    int n;

    TRACE((T_CALLED "lrepl_regex:%s{%.*s}\n",
	   rexp ? NONNULL(rexp->pat) : "",
	   length, np));

    if (rexp == NULL || rexp->reg == NULL) {
	status = FALSE;

    } else if ((status = check_editable(curbp)) == TRUE) {
	regexp *exp = rexp->reg;

	mayneedundo();

	if (lregexec(exp, DOT.l, DOT.o, llength(DOT.l), FALSE)) {
	    int old = (int) (exp->endp[0] - exp->startp[0]);
	    if (old > 0) {
		regionshape = rgn_EXACT;
		status = forwdelchar(TRUE, old);
	    }
	} else {
	    status = FALSE;
	}

	if (status == TRUE) {
	    /* insert passed in chars */
	    for (n = 0; n < length; ++n) {
		c = np[n];
		if ((status = bputc(c)) != TRUE) {
		    break;
		}
	    }
	}
    }
    returnCode(status);
}
#endif

/*
 * Delete a newline. Join the current line with the next line. If the next line
 * is the magic header line always return TRUE; merging the last line with the
 * header line can be thought of as always being a successful operation, even
 * if nothing is done, and this makes the kill buffer work "right". Easy cases
 * can be done by shuffling data around. Hard cases require that lines be moved
 * about in memory. Return FALSE on error and TRUE if all looks ok.
 */
static int
ldelnewline(void)
{
    LINE *lp1;
    LINE *lp2;
    WINDOW *wp;
    C_NUM len;
    C_NUM add;

    lp1 = DOT.l;
    len = llength(lp1);
    /* if the current line is empty, remove it */
    if (len == 0) {		/* Blank line.          */
	TossToUndo(lp1);
	lremove(curbp, lp1);
	return (TRUE);
    }
    lp2 = lforw(lp1);
    /* if the next line is empty, that's "currline\n\n", so we
       remove the second \n by deleting the next line */
    /* but never delete the newline on the last non-empty line */
    if (lp2 == buf_head(curbp))
	return (TRUE);
    else if ((add = llength(lp2)) == 0) {
	/* next line blank? */
	TossToUndo(lp2);
	lremove(curbp, lp2);
	return (TRUE);
    }

    beginDisplay();
    CopyForUndo(DOT.l);

    /* no room in line above, make room */
    if (add > (C_NUM) lp1->l_size - len) {
	char *ntext;
	size_t nsize;
	/* first, create the new image */
	nsize = roundlenup((size_t) len + (size_t) add);
	if ((ntext = castalloc(char, nsize)) == NULL)
	      return (FALSE);
	if (lvalue(lp1)) {	/* possibly NULL if l_size == 0 */
	    (void) memcpy(&ntext[0], &lvalue(lp1)[0], (size_t) len);
	    ltextfree(lp1, curbp);
	}
	lvalue(lp1) = ntext;
	lp1->l_size = nsize;
    }
    (void) memcpy(lvalue(lp1) + len, lvalue(lp2), (size_t) add);
#if ! WINMARK
    if (MK.l == lp2) {
	MK.l = lp1;
	MK.o += len;
    }
#endif
    /* check all windows for references to the deleted line */
    for_each_window(wp) {
	if (wp->w_line.l == lp2)
	    wp->w_line.l = lp1;
	if (wp->w_dot.l == lp2) {
	    wp->w_dot.l = lp1;
	    wp->w_dot.o += len;
	}
#if WINMARK
	if (wp->w_mark.l == lp2) {
	    wp->w_mark.l = lp1;
	    wp->w_mark.o += len;
	}
#endif
	if (wp->w_lastdot.l == lp2) {
	    wp->w_lastdot.l = lp1;
	    wp->w_lastdot.o += len;
	}
    }
    do_mark_iterate(mp, {
	if (mp->l == lp2) {
	    mp->l = lp1;
	    mp->o += len;
	}
    });
#if OPT_LINE_ATTRS
    FreeAndNull(lp2->l_attrs);
#endif
    llength(lp1) += add;
    assert((size_t) llength(lp1) <= lp1->l_size);
    set_lforw(lp1, lforw(lp2));
    set_lback(lforw(lp2), lp1);
    dumpuline(lp1);
    TossToUndo(lp2);
    endofDisplay();

    return (TRUE);
}

static int kcharpending = -1;

/*
 * Delete all of the text saved in the kill buffer. Called by commands when a
 * new kill context is being created. The kill buffer array is released, just
 * in case the buffer has grown to immense size. No errors.
 */
void
ksetup(void)
{
    if ((kregflag & KAPPEND) != 0)
	kregflag = KAPPEND;
    else
	kregflag = KNEEDCLEAN;
    kchars = klines = 0;
    kregwidth = 0;
    kcharpending = -1;

}

static void
free_kbs(int n)
{
    KILL *kp;			/* ptr to scan kill buffer chunk list */

    beginDisplay();

    /* first, delete all the chunks */
    kbs[n].kbufp = kbs[n].kbufh;
    while (kbs[n].kbufp != NULL) {
	kp = kbs[n].kbufp->d_next;
	free((char *) (kbs[n].kbufp));
	kbs[n].kbufp = kp;
    }

    /* and reset all the kill buffer pointers */
    kbs[n].kbufh = kbs[n].kbufp = NULL;
    kbs[n].kused = 0;
    kbs[n].kbwidth = 0;

    endofDisplay();
}

/*
 * clean up the old contents of a kill register.
 * if called from other than kinsert, only does anything in the case where
 * nothing was yanked
 */
void
kdone(void)
{
    if ((kregflag & KNEEDCLEAN) && kbs[ukb].kbufh != NULL) {
	free_kbs(ukb);
	kregwidth = 0;
	kcharpending = -1;
    }
    kregflag = (USHORT) (kregflag & ~KNEEDCLEAN);
    kbs[ukb].kbflag = kregflag;
    relist_registers();
}

int
kinsertlater(int c)
{
    int s = TRUE;
    if (kcharpending >= 0) {
	int oc = kcharpending;
	kcharpending = -1;
	s = kinsert(oc);
    }
    /* try to widen the rectangle, just in case */
    if (kregwidth > kbs[ukb].kbwidth)
	kbs[ukb].kbwidth = kregwidth;
    kcharpending = c;
    return s;
}

/*
 * Insert a character to the kill buffer, allocating new chunks as needed.
 * Return TRUE if all is well, and FALSE on errors.
 *
 * 'c' is the character to insert in the kill buffer.
 */
int
kinsert(int c)
{
    KILL *nchunk;		/* ptr to newly malloced chunk */
    KILLREG *kbp = &kbs[ukb];
    int rc = TRUE;

    if (kcharpending >= 0) {
	int oc = kcharpending;
	kcharpending = -1;
	kinsert(oc);
    }

    kdone();			/* clean up the (possible) old contents */

    beginDisplay();
    /* check to see if we need a new chunk */
    if (kbp->kused >= KBLOCK || kbp->kbufh == NULL) {
	if ((nchunk = typealloc(KILL)) == NULL) {
	    rc = FALSE;
	} else {
	    if (kbp->kbufh == NULL)	/* set head ptr if first time */
		kbp->kbufh = nchunk;
	    /* point the current to this new one */
	    if (kbp->kbufp != NULL)
		kbp->kbufp->d_next = nchunk;
	    kbp->kbufp = nchunk;
	    kbp->kbufp->d_next = NULL;
	    kbp->kused = 0;
	}
    }

    if (rc != FALSE) {
	/* and now insert the character */
	kbp->kbufp->d_chunk[kbp->kused++] = (UCHAR) c;
	kchars++;
	if (c == '\n') {
	    klines++;
	    if (kregwidth > kbp->kbwidth)
		kbp->kbwidth = kregwidth;
	    kregwidth = 0;
	} else {
	    kregwidth++;
	}
    }
    endofDisplay();
    return (rc);
}

/*
 * Translates the index of a register in kill-buffer list to its name.
 */
int
index2reg(int c)
{
    int n;

    if (c >= 0 && c < 10)
	n = (c + '0');
    else if (c == KEYST_KREG)
	n = KEYST_KCHR;
#if OPT_SELECTIONS
    else if (c == SEL_KREG)
	n = SEL_KCHR;
    else if (c == CLIP_KREG)
	n = CLIP_KCHR;
#endif
    else if (c >= 10 && c < (int) TABLESIZE(kbs))
	n = (c - 10 + 'a');
    else
	n = '?';

    return n;
}

/*
 * Translates the name of a register into the index in kill-buffer list.
 */
int
reg2index(int c)
{
    int n;

    if (c < 0)
	n = -1;
    else if (isDigit(c))
	n = c - '0';
    else if (isLower(c))
	n = c - 'a' + 10;	/* named buffs are in 10 through 36 */
    else if (isUpper(c))
	n = c - 'A' + 10;	/* ignore case of register-name */
#if OPT_SELECTIONS
    else if (c == SEL_KCHR)
	n = SEL_KREG;
    else if (c == CLIP_KCHR)
	n = CLIP_KREG;
#endif
    else if (c == KEYST_KCHR)
	n = KEYST_KREG;
    else if (c == '"')
	n = 0;
    else
	n = -1;

    return n;
}

/*
 * Translates a kill-buffer index into the actual offset into the kill buffer,
 * handling the translation of "1 .. "9
 */
int
index2ukb(int inx)
{
    if (inx >= 0 && inx < 10) {
	short save = ukb;
	ukb = (short) inx;
	kregcirculate(FALSE);
	inx = ukb;
	ukb = save;
    }
    return inx;
}

/* select one of the named registers for use with the following command */
/*  this could actually be handled as a command prefix, in kbd_seq(), much
	the way ^X-cmd and META-cmd are done, except that we need to be
	able to accept any of
		 3"adw	"a3dw	"ad3w
	to delete 3 words into register a.  So this routine gives us an
	easy way to handle the second case.  (The third case is handled in
	operators(), the first in main())
*/
int
usekreg(int f, int n)
{
    int c, i, status;
    static char cbuf[2];

    /* take care of incrementing the buffer number, if we're replaying
       a command via 'dot' */
    incr_dot_kregnum();

    if ((status = mlreply_reg("Use named register: ", cbuf, &c, -1)) != TRUE)
	return status;

    i = reg2index(c);
    if (kbm_started(i, FALSE))
	return FALSE;

    /* if we're playing back dot, let its kreg override */
    if (dotcmdactive == PLAY && dotcmdkreg != 0)
	ukb = dotcmdkreg;
    else
	ukb = (short) i;

    if (isUpper(c))
	kregflag |= KAPPEND;

    if (clexec) {
	TBUFF *tok = NULL;
	char *name = mac_unquotedarg(&tok);	/* get the next token */
	if (name != NULL) {
	    status = execute(engl2fnc(name), f, n);
	    tb_free(&tok);
	} else {
	    status = FALSE;
	}
    } else if (isnamedcmd) {
	status = namedcmd(f, n);
    } else {
	/* get the next command from the keyboard */
	c = kbd_seq();

	/* allow second chance for entering counts */
	do_repeats(&c, &f, &n);

	status = execute(DefaultKeyBinding(c), f, n);
    }

    ukb = 0;
    kregflag = 0;

    return (status);
}

static short lastkb;

/*
 * Returns the index in kbs[] of the next kill-buffer we will write into.
 */
static short
nextkb(void)
{
    short n = lastkb;

    if ((kbs[n].kbflag & (KLINES | KRECT | KAPPEND))
	&& !(kbs[n].kbflag & KYANK)) {
	if (--n < 0)
	    n = 9;
    }
    return n;
}

/* buffers 0 through 9 are circulated automatically for full-line deletes */
/* we re-use one of them until the KLINES flag is on, then we advance */
/* to the next */
void
kregcirculate(int killing)
{
    if (ukb >= 10)		/* then the user specified a lettered buffer */
	return;

    /* we only allow killing into the real "0 */
    /* ignore any other buffer spec */
    if (killing) {
	ukb = lastkb = nextkb();
	kbs[lastkb].kbflag = 0;
    } else {
	/* let 0 pass unmolested -- it is the default */
	if (ukb == 0) {
	    ukb = lastkb;
	} else {
	    /* for the others, if the current "0 has lines in it, it
	       must be `"1', else "1 is `"1'.  get it? */
	    if (kbs[lastkb].kbflag & (KLINES | KAPPEND))
		ukb = (short) ((lastkb + ukb - 1) % 10);
	    else
		ukb = (short) ((lastkb + ukb) % 10);
	}
    }
}

static struct {
    int started;
    short next_kb;
    short last_kb;
    int save_ukb;
    USHORT kregflg;
    KILLREG killreg;
} undo_kill;

/*
 * Kill a region, saving it into the kill-buffer.  If the buffer that we're
 * modifying is not-undoable, save information so that we can restore the
 * kill buffer after we're done.  We use this in filtering, for performance
 * reasons.
 */
int
begin_kill(void)
{
    if ((undo_kill.started = !b_val(curbp, MDUNDOABLE)) != 0) {
	static KILLREG unused;
	short num = nextkb();
	undo_kill.last_kb = lastkb;
	undo_kill.next_kb = num;
	undo_kill.kregflg = kregflag;
	undo_kill.killreg = kbs[num];
	kbs[num] = unused;
    }
    return killregion();
}

void
end_kill(void)
{
    if (undo_kill.started) {
	free_kbs(undo_kill.next_kb);
	kbs[undo_kill.next_kb] = undo_kill.killreg;
	kregflag = undo_kill.kregflg;
	ukb = 0;		/* was modified by kregcirculate() */
	lastkb = undo_kill.last_kb;
#if OPT_SHOW_REGS && OPT_UPBUFF
	will_relist_regs = FALSE;
	relist_registers();
#endif
    }
}

int
putbefore(int f, int n)
{
    return doput(f, n, FALSE, rgn_EXACT);
}

int
putafter(int f, int n)
{
    return doput(f, n, TRUE, rgn_EXACT);
}

int
lineputbefore(int f, int n)
{
    return doput(f, n, FALSE, rgn_FULLLINE);
}

int
lineputafter(int f, int n)
{
    return doput(f, n, TRUE, rgn_FULLLINE);
}

int
rectputbefore(int f, int n)
{
    return doput(f, n, FALSE, rgn_RECTANGLE);
}

int
rectputafter(int f, int n)
{
    return doput(f, n, TRUE, rgn_RECTANGLE);
}

static int
doput(int f, int n, int after, REGIONSHAPE shape)
{
    int s, oukb;

    n = need_a_count(f, n, 1);

    oukb = ukb;
    kregcirculate(FALSE);	/* cf: 'index2ukb()' */
    if (kbs[ukb].kbufh == NULL) {
	if (ukb != 0)
	    mlwarn("[Nothing in register %c]", index2reg(oukb));
	return (FALSE);
    }

    if (shape == rgn_EXACT) {
	if ((kbs[ukb].kbflag & (KLINES | KAPPEND)))
	    shape = rgn_FULLLINE;
	else if (kbs[ukb].kbflag & KRECT)
	    shape = rgn_RECTANGLE;
	else
	    shape = rgn_EXACT;
    }

    if (shape == rgn_FULLLINE) {
	if (after && !is_header_line(DOT, curbp))
	    DOT.l = lforw(DOT.l);
	DOT.o = b_left_margin(curbp);
    } else {
	if (after && !is_at_end_of_line(DOT))
	    forwchar(TRUE, 1);
    }

    (void) setmark();
    TRACE(("before PutChar DOT=%d.%d\n",
	   line_no(curbp, DOT.l), DOT.o));
    s = PutChar(n, shape);
    if (s == TRUE)
	swapmark();
    TRACE(("after PutChar DOT=%d.%d, MK=%d.%d\n",
	   line_no(curbp, DOT.l),
	   DOT.o, line_no(curbp, MK.l), MK.o));
    if (is_header_line(DOT, curbp)) {
	DOT.l = lback(DOT.l);
    }
    if (shape == rgn_FULLLINE) {
	(void) firstnonwhite(FALSE, 1);
    }
    ukb = 0;
    TRACE(("finally doput(DOT=%d.%d, MK=%d.%d) -> %d\n",
	   line_no(curbp, DOT.l), DOT.o,
	   line_no(curbp, MK.l), MK.o, s));
    return (s);
}

/* designed to be used with the result of "getoff()", which returns
 *	the offset of the character whose column is "close" to a goal.
 *	it may be to the left if the line is too short, or to the right
 *	if the column is spanned by a tab character.
 */
static int
force_text_at_col(C_NUM goalcol, C_NUM reached)
{
    int status = TRUE;
    if (reached < goalcol) {
	/* pad out to col */
	DOT.o = llength(DOT.l);
	status = lins_bytes(goalcol - reached, ' ');
    } else if (reached > goalcol) {
	/* there must be a tab there. */
	/* pad to hit column we want */
	backchar_to_bol(TRUE, 1);
	status = lins_bytes(goalcol % tabstop_val(curbp), ' ');
    }
    return status;
}

static int
next_line_at_col(C_NUM col, C_NUM * reachedp)
{
    int s = TRUE;
    if (is_last_line(DOT, curbp)) {
	DOT.o = llength(DOT.l);
	if (lnewline() != TRUE)
	    return FALSE;
    } else {
	DOT.l = lforw(DOT.l);
    }
    DOT.o = getoff(col, reachedp);
    return s;
}

/*
 * Put text back from the kill register.
 */
static int
PutChar(int n, REGIONSHAPE shape)
{
    int c;
    int i;
    int status, wasnl, suppressnl, first;
    L_NUM before;
    C_NUM col = 0, width = 0;
    C_NUM reached = 0;
    int checkpad = FALSE;
    char *sp;			/* pointer into string to insert */
    KILL *kp;			/* pointer into kill register */

    if (n < 0)
	return FALSE;

    /* make sure there is something to put */
    if (kbs[ukb].kbufh == NULL)
	return TRUE;		/* not an error, just nothing */

    status = TRUE;
    before = vl_line_count(curbp);
    suppressnl = FALSE;
    wasnl = FALSE;
    first = TRUE;

    /* for each time.... */
    while (n--) {
	kp = kbs[ukb].kbufh;
	if (shape == rgn_RECTANGLE) {
	    width = kbs[ukb].kbwidth;
	    col = getcol(DOT, FALSE);
	}
	while (kp != NULL) {
	    i = (int) KbSize(ukb, kp);
	    sp = (char *) kp->d_chunk;
	    if (shape == rgn_RECTANGLE) {
		while (i-- > 0) {
		    c = *sp++;
		    if (width == 0 || c == '\n') {
			if (checkpad) {
			    status = force_text_at_col(
							  col, reached);
			    if (status != TRUE)
				break;
			    checkpad = FALSE;
			}
			if (width && lins_bytes(width, ' ')
			    != TRUE) {
			    status = FALSE;
			    break;
			}
			if (next_line_at_col(col, &reached)
			    != TRUE) {
			    status = FALSE;
			    break;
			}
			checkpad = TRUE;
			width = kbs[ukb].kbwidth;
		    }
		    if (c == '\n') {
			continue;	/* did it already */
		    } else {
			if (checkpad) {
			    status = force_text_at_col(col, reached);
			    if (status != TRUE)
				break;
			    checkpad = FALSE;
			}
			width--;

			if (is_header_line(DOT, curbp)) {
			    suppressnl = TRUE;
			}
			if (lins_bytes(1, c) != TRUE) {
			    status = FALSE;
			    break;
			}
			wasnl = FALSE;
		    }
		}
	    } else {		/* not rectangle */
		while (i-- > 0) {
		    /*
		     * If we're inserting into a non-empty buffer, will trim
		     * the extra newline.  Check for the special case where
		     * we're inserting just a newline - that could be a case
		     * where the current line is already on the undo stack, and
		     * we cannot free it a second time.
		     */
		    if (first && is_header_line(DOT, curbp)) {
			if ((i > 0) || (*sp != '\n'))
			    suppressnl = TRUE;
		    }
		    first = FALSE;
		    if (*sp == '\n') {
			sp++;
			if (lnewline() != TRUE) {
			    status = FALSE;
			    break;
			}
			wasnl = TRUE;
		    } else {
			char *dp;
			char *ep = sp + 1;
			/* Find end of line or end of kill buffer */
			while (i > 0 && *ep != '\n') {
			    i--;
			    ep++;
			}
			/* Open up space in current line */
			status = lins_bytes((int) (ep - sp), ' ');
			if (status != TRUE)
			    break;
			dp = lvalue(DOT.l)
			    + DOT.o - (int) (ep - sp);
			/* Copy killbuf portion to the line */
			while (sp < ep) {
			    *dp++ = *sp++;
			}
			wasnl = FALSE;
		    }
		}
	    }
	    if (status != TRUE)
		break;
	    kp = kp->d_next;
	}
	if (status != TRUE)
	    break;
	if (wasnl) {
	    if (suppressnl) {
		if (ldelnewline() != TRUE) {
		    status = FALSE;
		    break;
		}
	    }
	} else {
	    if (shape == rgn_FULLLINE && !suppressnl) {
		if (lnewline() != TRUE) {
		    status = FALSE;
		    break;
		}
	    }
	}
    }
    curwp->w_flag |= WFHARD;
    (void) line_report(before);

    return status;
}

static int lastreg = -1;

/* ARGSUSED */
int
execkreg(int f, int n)
{
    int c, j, jj, status;
    KILL *kp;			/* pointer into kill register */
    static char cbuf[2];
    int kbcount, whichkb;
    int i;
    char *sp;
    KILL *tkp;

    n = need_a_count(f, n, 1);

    if (n <= 0)
	return TRUE;

    if ((status = mlreply_reg("Execute register: ", cbuf, &c, lastreg)) != TRUE)
	return status;

    /* disallow execution of the characters we're recording */
    if (c == KEYST_KCHR) {
	mlwarn("[Error: cannot execute %c-register]", c);
	return FALSE;
    }

    j = reg2index(c);
    if (kbm_started(j, TRUE))
	return FALSE;

    lastreg = c;
    relist_registers();

    /* make sure there is something to execute */
    do {
	jj = index2ukb(j);
	if (jj < 0 || jj >= NKREGS)
	    return FALSE;
	kp = kbs[jj].kbufh;
	if (kp == NULL)
	    return TRUE;	/* not an error, just nothing */

	/* count the kchunks */
	kbcount = 0;
	tkp = kp;
	while (tkp != NULL) {
	    kbcount++;
	    tkp = tkp->d_next;
	}
	/* process them in reverse order */
	while (kbcount > 0) {
	    whichkb = kbcount;
	    tkp = kp;
	    while (--whichkb > 0) {
		if ((tkp = tkp->d_next) == NULL) {
		    kbcount = 0;
		    break;	/* quit on error */
		}
	    }
	    if (tkp != NULL) {
		i = (int) KbSize(jj, tkp);
		sp = (char *) tkp->d_chunk + i - 1;
		while (i--) {
		    mapungetc((int) ((UINT) (*sp--) | YESREMAP));
		}
	    }
	    kbcount--;
	}
    } while (--n > 0);
    return TRUE;
}

/* ARGSUSED */
int
loadkreg(int f, int n GCC_UNUSED)
{
    int s;
    char respbuf[NFILEN];

    ksetup();
    *respbuf = EOS;
    s = mlreply_no_opts("Load register with: ",
			respbuf, (UINT) sizeof(respbuf));
    if (s != TRUE)
	return FALSE;
    if (f)
	kregflag |= KLINES;
    for (s = 0; s < NFILEN; s++) {
	if (!respbuf[s])
	    break;
	if (!kinsert(respbuf[s]))
	    break;
    }
    kdone();
    return TRUE;
}

/* Show the contents of the kill-buffers */
#if OPT_SHOW_REGS
#define	REGS_PREFIX	12	/* non-editable portion of the display */

#if OPT_UPBUFF
static int show_all_chars;
#endif

/*ARGSUSED*/
static void
makereglist(
	       int iflag,	/* list nonprinting chars flag */
	       void *dummy GCC_UNUSED)
{
    KILL *kp;
    int i, ii, j, c;
    UCHAR *p;
    int any;

#if OPT_UPBUFF
    show_all_chars = iflag;
#endif
    b_set_left_margin(curbp, REGS_PREFIX);
    any = (reg2index(lastreg) >= 0);
    if (any)
	bprintf("last=%c", lastreg);

    for (i = 0; (size_t) i < TABLESIZE(kbs); i++) {
	short save = ukb;

	ii = index2ukb(i);
	if ((kp = kbs[ii].kbufh) != NULL) {
	    int first = FALSE;
	    if (any++) {
		bputc('\n');
		lsettrimmed(lback(DOT.l));
	    }
	    if (i > 0) {
		bprintf("%c:", index2reg(i));
	    } else {
		bprintf("%s", "(unnamed)");
	    }
	    bpadc(' ', REGS_PREFIX - DOT.o);
	    do {
		j = (int) KbSize(ii, kp);
		p = kp->d_chunk;

		while (j-- > 0) {
		    if (first) {
			first = FALSE;
			bpadc(' ', REGS_PREFIX - DOT.o);
		    }
		    c = *p++;
		    if (isPrint(c) || !iflag) {
			bputc(c);
		    } else if (c != '\n') {
			bputc('^');
			bputc((int) toalpha(c));
		    }
		    if (c == '\n') {
			first = TRUE;
			any = 0;
		    } else
			any = 1;
		}
	    } while ((kp = kp->d_next) != NULL);
	}
	if (i < 10)
	    ukb = save;
    }
    lsettrimmed(DOT.l);
}

/*ARGSUSED*/
int
showkreg(int f, int n GCC_UNUSED)
{
    will_relist_regs = FALSE;
    return liststuff(REGISTERS_BufName, FALSE,
		     makereglist, f, (void *) 0);
}

#if OPT_UPBUFF
static int
show_Registers(BUFFER *bp)
{
    b_clr_obsolete(bp);
    return showkreg(show_all_chars, 1);
}
#endif /* OPT_UPBUFF */

#endif /* OPT_SHOW_REGS */

#if OPT_REGS_CMPL
KBD_OPTIONS
regs_kbd_options(void)
{
    return KBD_MAYBEC;
}

static int
reg_has_data(int src)
{
    int rc = FALSE;
    int j = reg2index(src);
    if (j >= 0 ||
	(regs_kbd_default && (src == UNAME_KCHR))) {
	int jj = index2ukb(j);
	if (jj >= 0 && jj < NKREGS) {
	    if (kbs[jj].kbufh) {
		rc = TRUE;
	    }
	}
    }
    return rc;
}

/*
 * Register names are a single ASCII character.
 */
static const char **
init_regs_cmpl(char *buf, size_t cpos)
{
    int dst, src;
    char **result = typeallocn(char *, 96);
    if (result != NULL) {
	for (dst = 0, src = 32; src < 127; ++src) {
	    if (isUpper(src))	/* register names are caseless */
		continue;
	    if (reg_has_data(src)) {
		if (cpos == 0 || *buf == src) {
		    char value[2];
		    value[0] = (char) src;
		    value[1] = 0;
		    result[dst++] = strmalloc(value);
		}
	    }
	}
	result[dst] = NULL;
    }
    return (const char **) result;
}

int
regs_completion(DONE_ARGS)
{
    size_t cpos = *pos;
    int status = FALSE;
    const char **nptr;

    kbd_init();			/* nothing to erase */
    buf[cpos] = EOS;		/* terminate it for us */

    beginDisplay();
    if ((nptr = init_regs_cmpl(buf, cpos)) != NULL) {
	status = kbd_complete(PASS_DONE_ARGS, (const char *) nptr, sizeof(*nptr));
	free(TYPECAST(char *, nptr));
    }
    endofDisplay();
    return status;
}
#endif

/* For memory-leak testing (only!), releases all kill-buffer storage. */
#if NO_LEAKS
void
kbs_leaks(void)
{
    for (ukb = 0; ukb < (int) TABLESIZE(kbs); ukb++) {
	ksetup();
	kdone();
    }
}
#endif
