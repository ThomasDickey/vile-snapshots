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
 * $Header: /users/source/archives/vile.vcs/RCS/line.c,v 1.146 2002/01/12 19:21:01 tom Exp $
 *
 */

/* #define POISON */
#ifdef POISON
#define poison(p,s) (void)memset((char *)p, 0xdf, s)
#else
#define poison(p,s)
#endif

#include	"estruct.h"
#include	"edef.h"

#define roundlenup(n) ((n+NBLOCK-1) & (UINT)~(NBLOCK-1))

static	int	doput(int f, int n, int after, REGIONSHAPE shape);
static	int	ldelnewline (void);
static	int	PutChar(int n, REGIONSHAPE shape);

#if OPT_SHOW_REGS && OPT_UPBUFF
static	int	will_relist_regs;
static	void	relist_registers (void);
#else
#define relist_registers()
#endif

/*
 * Test the 'report' threshold, returning true if the argument is above it.
 */
int
do_report (L_NUM value)
{
	if (value < 0)
		value = -value;
	return (global_g_val(GVAL_REPORT) > 0
	   &&   global_g_val(GVAL_REPORT) <= value);
}

/*
 * This routine allocates a block of memory large enough to hold a LINE
 * containing "used" characters. The block is always rounded up a bit. Return
 * a pointer to the new block, or NULL if there isn't any memory left. Print a
 * message in the message line if no space.
 */
/*ARGSUSED*/
LINEPTR
lalloc(register int used, BUFFER *bp)
{
	register LINE	*lp;
	register size_t	size;

	/* lalloc(-1) is used by undo for placeholders */
	if (used < 0)  {
		size = 0;
	} else {
		size = roundlenup(used);
	}
	/* see if the buffer LINE block has any */
	if ((lp = bp->b_freeLINEs) != NULL) {
		bp->b_freeLINEs = lp->l_nxtundo;
	} else if ((lp = typealloc(LINE)) == NULL) {
		(void)no_memory("LINE");
		return NULL;
	}
	lp->l_text = NULL;
	if (size && (lp->l_text = castalloc(char,size)) == NULL) {
		(void)no_memory("LINE text");
		poison(lp, sizeof(*lp));
		free((char *)lp);
		return NULL;
	}
	lp->l_size = size;
#if !SMALLER
	lp->l_number = 0;
#endif
	lp->l_used = used;
	lsetclear(lp);
	lp->l_nxtundo = null_ptr;
#if OPT_LINE_ATTRS
	lp->l_attrs = NULL;
#endif
	return lp;
}

/*ARGSUSED*/
void
lfree(register LINEPTR lp, register BUFFER *bp)
{
	if (lisreal(lp))
		ltextfree(lp,bp);

	/* if the buffer doesn't have its own block of LINEs, or this
		one isn't in that range, free it */
	if (!bp->b_LINEs || lp < bp->b_LINEs || lp >= bp->b_LINEs_end) {
		poison(lp, sizeof(*lp));
		free((char *)lp);
	} else {
		/* keep track of freed buffer LINEs here */
		lp->l_nxtundo = bp->b_freeLINEs;
		bp->b_freeLINEs = lp;
#ifdef POISON
		/* catch references hard */
		set_lback(lp, (LINE *)1);
		set_lforw(lp, (LINE *)1);
		lp->l_text = (char *)1;
		lp->l_size = lp->l_used = LINENOTREAL;
#endif
	}
}

/*ARGSUSED*/
void
ltextfree(register LINE *lp, register BUFFER *bp)
{
	register UCHAR *ltextp;

	ltextp = (UCHAR *)lp->l_text;
	if (ltextp) {
		lp->l_text = NULL;
		if (bp->b_ltext) { /* could it be in the big range? */
			if (ltextp < bp->b_ltext || ltextp >= bp->b_ltext_end) {
				poison(ltextp, lp->l_size);
				free((char *)ltextp);
			} /* else {
			could keep track of freed big range text here;
			} */
		} else {
			poison(ltextp, lp->l_size);
			free((char *)ltextp);
		}
	} /* else nothing to free */

#if OPT_LINE_ATTRS
	FreeAndNull(lp->l_attrs);
#endif
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
lremove(register BUFFER *bp, register LINEPTR lp)
{
	register WINDOW *wp;
	register LINEPTR point;

	if (lp == buf_head(bp))
		return;

	point = lforw(lp);

#if !WINMARK
	if (MK.l == lp) {
		MK.l = point;
		MK.o = 0;
	}
#endif
	for_each_window(wp) {
		if (wp->w_line.l == lp)
			wp->w_line.l = point;
		if (wp->w_dot.l == lp) {
			wp->w_dot.l  = point;
			wp->w_dot.o  = 0;
		}
#if WINMARK
		if (wp->w_mark.l == lp) {
			wp->w_mark.l = point;
			wp->w_mark.o = 0;
		}
#endif
#if 0
		if (wp->w_lastdot.l == lp) {
			wp->w_lastdot.l = point;
			wp->w_lastdot.o = 0;
		}
#endif
	}
	if (bp->b_nwnd == 0) {
		if (bp->b_dot.l == lp) {
			bp->b_dot.l = point;
			bp->b_dot.o = 0;
		}
#if WINMARK
		if (bp->b_mark.l == lp) {
			bp->b_mark.l = point;
			bp->b_mark.o = 0;
		}
#endif
#if 0
		if (bp->b_lastdot.l == lp) {
			bp->b_lastdot.l = point;
			bp->b_lastdot.o = 0;
		}
#endif
	}
#if 0
	if (bp->b_nmmarks != NULL) { /* fix the named marks */
		int i;
		struct MARK *mp;
		for (i = 0; i < 26; i++) {
			mp = &(bp->b_nmmarks[i]);
			if (mp->p == lp) {
				mp->p = point;
				mp->o = 0;
			}
		}
	}
#endif
#if OPT_VIDEO_ATTRS
	{
	    AREGION *ap = bp->b_attribs;
	    while (ap != NULL) {
		int samestart = (ap->ar_region.r_orig.l == lp);
		int sameend   = (ap->ar_region.r_end.l == lp);
		if (samestart && sameend) {
		    AREGION *tofree = ap;
		    ap = ap->ar_next;
		    free_attrib(bp, tofree);
		}
		else if (samestart) {
		    ap->ar_region.r_orig.l = point;
		    ap->ar_region.r_orig.o = 0;
		    ap = ap->ar_next;
		}
		else if (sameend) {
		    ap->ar_region.r_end.l = lback(lp);
		    ap->ar_region.r_end.o = llength(ap->ar_region.r_end.l);
		    ap = ap->ar_next;
		}
		else
		    ap = ap->ar_next;
	    }
	}
#endif /* OPT_VIDEO_ATTRS */
	set_lforw(lback(lp), lforw(lp));
	set_lback(lforw(lp), lback(lp));

	/*
	 * If we've disable undo stack, we'll have to free the line to avoid
	 * a memory leak.
	 */
	if (!b_val(bp, MDUNDOABLE)) {
		lfree(lp, bp);
	}
}

int
insspace(int f, int n)	/* insert spaces forward into text */
{
	if (!linsert(n, ' '))
		return FALSE;
	return backchar(f, n);
}

int
lstrinsert(	/* insert string forward into text */
const char *s,	/* if NULL, treat as "" */
int len)	/* if non-zero, insert exactly this amount.  pad if needed */
{
	const char *p = s;
	int n, b = 0;
	if (len <= 0)
		n = VL_HUGE;
	else
		n = len;
	while (p && *p && n) {
		b++;
		if (!linsert(1, *p++))
			return FALSE;
		n--;
	}
	if (n && len > 0) {	/* need to pad? */
		if (!linsert(n, ' '))
			return FALSE;
		b += n;
	}

	DOT.o -= b;

	return TRUE;
}


/*
 * replace a single character on a line.  _cannot_ be
 * use to replace a newline.
 */
int
lreplc(LINEPTR lp, C_NUM off, int c)
{

	if (off == llength(lp))
	    return FALSE;

	if (lp->l_text[off] != (char)c) {
	    copy_for_undo(lp);
	    lp->l_text[off] = (char)c;

	    chg_buff(curbp, WFEDIT);
	}

	return TRUE;
}

/*
 * Insert "n" copies of the character "c" at the current location of dot. In
 * the easy case all that happens is the text is stored in the line. In the
 * hard case, the line has to be reallocated. When the window list is updated,
 * take special care; I screwed it up once. You always update dot in the
 * current window. You update mark, and a dot in another window, if it is
 * greater than the place where you did the insert. Return TRUE if all is
 * well, and FALSE on errors.
 */
int
linsert(int n, int c)
{
	register char	*cp1;
	register char	*cp2;
	register LINE	*tmp;
	register LINEPTR lp1;
	register LINEPTR lp2;
	register LINEPTR lp3;
	register int	doto;
	register int	i;
	register WINDOW *wp;
	register char	*ntext;
	size_t	nsize;

	lp1 = DOT.l;				/* Current line		*/
	if (lp1 == buf_head(curbp)) {		/* At the end: special	*/
		if (DOT.o != 0) {
			mlforce("BUG: linsert");
			return (FALSE);
		}
		lp2 = lalloc(n, curbp);		/* Allocate new line	*/
		if (lp2 == null_ptr)
			return (FALSE);

		lp3 = lback(lp1);		/* Previous line	*/
		set_lforw(lp3, lp2);		/* Link in		*/
		set_lforw(lp2, lp1);
		set_lback(lp1, lp2);
		set_lback(lp2, lp3);
		(void)memset(lp2->l_text, c, (size_t)n);

		tag_for_undo(lp2);

		/* don't move DOT until after tagging for undo */
		/*  (it's important in an empty buffer) */
		DOT.l = lp2;
		DOT.o = n;
		chg_buff(curbp, WFINS|WFEDIT);
		return (TRUE);
	}
	doto = DOT.o;				/* Save for later.	*/
	tmp  = lp1;
	nsize = llength(tmp) + n;
	if (nsize > tmp->l_size) {		/* Hard: reallocate	*/
		/* first, create the new image */
		nsize = roundlenup((int)nsize);
		copy_for_undo(lp1);
		if ((ntext=castalloc(char,nsize)) == NULL)
			return (FALSE);
		if (lp1->l_text) /* possibly NULL if l_size == 0 */
			(void)memcpy(&ntext[0], &lp1->l_text[0], (size_t)doto);
		(void)memset(&ntext[doto],   c, (size_t)n);
		if (lp1->l_text) {
#if OPT_LINE_ATTRS
			UCHAR *l_attrs = lp1->l_attrs;
			lp1->l_attrs = 0;	/* momentarily detach */
#endif
			(void)memcpy(&ntext[doto+n], &lp1->l_text[doto],
					(size_t)(lp1->l_used-doto ));
			ltextfree(lp1,curbp);
#if OPT_LINE_ATTRS
			lp1->l_attrs = l_attrs;	/* reattach */
#endif
		}
		lp1->l_text = ntext;
		lp1->l_size = nsize;
		lp1->l_used += n;
	} else {		/* Easy: in place	*/
		copy_for_undo(lp1);
		chg_buff(curbp, WFEDIT);
		tmp = lp1;
		/* don't use memcpy:  overlapping regions.... */
		llength(tmp) += n;
		if (tmp->l_used - n > doto) {
			cp2 = &tmp->l_text[tmp->l_used];
			cp1 = cp2-n;
			while (cp1 != &tmp->l_text[doto])
				*--cp2 = *--cp1;
		}
		for (i=0; i<n; ++i)		/* Add the characters	*/
			tmp->l_text[doto+i] = (char)c;
	}
	chg_buff(curbp, WFEDIT);
#if ! WINMARK
	if (MK.l == lp1) {
		if (MK.o > doto)
			MK.o += n;
	}
#endif
	for_each_window(wp) {			/* Update windows	*/
		if (wp->w_dot.l == lp1) {
			if (wp==curwp || wp->w_dot.o>doto)
				wp->w_dot.o += n;
		}
#if WINMARK
		if (wp->w_mark.l == lp1) {
			if (wp->w_mark.o > doto)
				wp->w_mark.o += n;
		}
#endif
		if (wp->w_lastdot.l == lp1) {
			if (wp->w_lastdot.o > doto)
				wp->w_lastdot.o += n;
		}
	}
	do_mark_iterate(mp,
			if (mp->l == lp1) {
				if (mp->o > doto)
					mp->o += n;
			}
	);
#if OPT_LINE_ATTRS
	if (lp1->l_attrs)
	    lattr_shift(curbp, lp1, doto, n);
#endif
	return (TRUE);
}

/*
 * Insert a newline into the buffer at the current location of dot in the
 * current window. The funny ass-backwards way it does things is not a botch;
 * it just makes the last line in the file not a special case. Return TRUE if
 * everything works out and FALSE on error (memory allocation failure). The
 * update of dot and mark is a bit easier then in the above case, because the
 * split forces more updating.
 */
int
lnewline(void)
{
	register char	*cp1;
	register char	*cp2;
	register LINEPTR lp1;
	register LINEPTR lp2;
	register int	doto;
	register WINDOW *wp;

	lp1  = DOT.l;			/* Get the address and	*/
	doto = DOT.o;			/* offset of "."	*/

	if (lp1 == buf_head(curbp)
	 && lforw(lp1) == lp1) {
		/* empty buffer -- just  create empty line */
		lp2 = lalloc(doto, curbp);
		if (lp2 == null_ptr)
			return (FALSE);
		/* put lp2 in below lp1 */
		set_lforw(lp2, lforw(lp1));
		set_lforw(lp1, lp2);
		set_lback(lforw(lp2), lp2);
		set_lback(lp2, lp1);

		tag_for_undo(lp2);

		for_each_window(wp) {
			if (wp->w_line.l == lp1)
				wp->w_line.l = lp2;
			if (wp->w_dot.l == lp1)
				wp->w_dot.l = lp2;
		}

		chg_buff(curbp, WFHARD|WFINS);

		return lnewline();	/* vi really makes _2_ lines */
	}

	lp2 = lalloc(doto, curbp);	/* New first half line */
	if (lp2 == null_ptr)
		return (FALSE);

	if (doto > 0) {
		register LINE *tmp;

		copy_for_undo(lp1);
		tmp = lp1;
		cp1 = tmp->l_text;	/* Shuffle text around	*/
		cp2 = lp2->l_text;
		while (cp1 != &tmp->l_text[doto])
			*cp2++ = *cp1++;
		cp2 = tmp->l_text;
		while (cp1 != &tmp->l_text[tmp->l_used])
			*cp2++ = *cp1++;
		tmp->l_used -= doto;
	}
	/* put lp2 in above lp1 */
	set_lback(lp2, lback(lp1));
	set_lback(lp1, lp2);
	set_lforw(lback(lp2), lp2);
	set_lforw(lp2, lp1);

	tag_for_undo(lp2);
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
	do_mark_iterate(mp,
			if (mp->l == lp1) {
				if (mp->o < doto)
					mp->l = lp2;
				else
					mp->o -= doto;
			}
	);
#if OPT_LINE_ATTRS
	FreeAndNull(lp1->l_attrs);
#endif
	chg_buff(curbp, WFHARD|WFINS);
	return (TRUE);
}

/*
 * This function deletes "n" bytes, starting at dot. It understands how to deal
 * with end of lines, etc. It returns TRUE if all of the characters were
 * deleted, and FALSE if they were not (because dot ran into the end of the
 * buffer. The "kflag" is TRUE if the text should be put in the kill buffer.
 */
int
ldelete(
B_COUNT nchars,	/* # of chars to delete */
int kflag)	/* put killed text in kill buffer flag */
{
	register char	*cp1;
	register char	*cp2;
	register LINEPTR dotp;
	register LINEPTR nlp;
	register int	doto;
	register int	chunk;
	register WINDOW *wp;
	register int i;
	register int s = TRUE;
	int	len_rs = len_record_sep(curbp);

	lines_deleted = 0;
	while (nchars > 0) {
		dotp = DOT.l;
		doto = DOT.o;
		if (dotp == buf_head(curbp)) { /* Hit end of buffer.*/
			s = FALSE;
			break;
		}
		chunk = dotp->l_used-doto; /* Size of chunk.	*/
		if (chunk > (int)nchars)
			chunk = (int)nchars;
		if (chunk == 0) {		/* End of line, merge.	*/
			/* first take out any whole lines below this one */
			nlp = lforw(dotp);
			while (nlp != buf_head(curbp)
			   &&  line_length(nlp) < nchars) {
				if (kflag) {
					s = kinsert('\n');
					for (i = 0; i < llength(nlp) &&
								s == TRUE; i++)
						s = kinsert(lgetc(nlp,i));
				}
				if (s != TRUE)
					break;
				lremove(curbp, nlp);
				lines_deleted++;
				toss_to_undo(nlp);
				nchars -= line_length(nlp);
				nlp = lforw(dotp);
			}
			if (s != TRUE)
				break;
			s = ldelnewline();
			chg_buff(curbp, WFHARD|WFKILLS);
			if (s != TRUE)
				break;
			if (kflag && (s = kinsert('\n')) != TRUE)
				break;
			nchars -= len_rs;
			lines_deleted++;
			continue;
		}
		copy_for_undo(DOT.l);
		chg_buff(curbp, WFEDIT);

		cp1 = dotp->l_text + doto; /* Scrunch text.	*/
		cp2 = cp1 + chunk;
		if (kflag) {		/* Kill?		*/
			while (cp1 != cp2) {
				if ((s = kinsert(*cp1)) != TRUE)
					break;
				++cp1;
			}
			if (s != TRUE)
				break;
			cp1 = dotp->l_text + doto;
		}
		while (cp2 != dotp->l_text + dotp->l_used)
			*cp1++ = *cp2++;
		dotp->l_used -= chunk;
#if ! WINMARK
		if (MK.l == dotp && MK.o > doto) {
			MK.o -= chunk;
			if (MK.o < doto)
				MK.o = doto;
		}
#endif
		for_each_window(wp) {		/* Fix windows		*/
			if (wp->w_dot.l == dotp
			 && wp->w_dot.o > doto) {
				wp->w_dot.o -= chunk;
				if (wp->w_dot.o < doto)
					wp->w_dot.o = doto;
			}
#if WINMARK
			if (wp->w_mark.l == dotp
			 && wp->w_mark.o > doto) {
				wp->w_mark.o -= chunk;
				if (wp->w_mark.o < doto)
					wp->w_mark.o = doto;
			}
#endif
			if (wp->w_lastdot.l == dotp
			 && wp->w_lastdot.o > doto) {
				wp->w_lastdot.o -= chunk;
				if (wp->w_lastdot.o < doto)
					wp->w_lastdot.o = doto;
			}
		}
		do_mark_iterate(mp,
				if (mp->l == dotp
				 && mp->o > doto) {
					mp->o -= chunk;
					if (mp->o < doto)
						mp->o = doto;
				}
		);
#if OPT_LINE_ATTRS
		lattr_shift(curbp, dotp, doto, -chunk);
#endif
		nchars -= chunk;
	}
	return (s);
}

/* returns pointer to static copy of text from current pos, consisting
 * of chars of type "type"
 */
#if OPT_EVAL
char *
lgrabtext(TBUFF **rp, CHARTYPE type)
{
	int need = llength(DOT.l) + 2;
	tb_alloc(rp, need);
	(void)screen_string(tb_values(*rp), need, type);
	return tb_values(*rp);
}
#endif

#if OPT_EVAL

/*
 * replace the current line with the passed in text
 *
 * np -	new text for the current line
 */
int
lrepltext(CHARTYPE type, const char *np)
{
	int status = TRUE;
	int c;

	TRACE(("lrepltext:%s%lx:%s\n", type ? "word" : "line",
		    (ULONG) type, np));

	if (b_val(curbp,MDVIEW))
		return rdonly();

	mayneedundo();

	if (type != 0) {  /* it's an exact region */
		regionshape = EXACT;
		while (DOT.o < llength(DOT.l)
		  && istype(type, char_at(DOT))) {
			if ((status = forwdelchar(FALSE,1)) != TRUE)
				return(status);
		}
	} else { /* it's the full line */
		regionshape = FULLLINE;
		DOT.o = w_left_margin(curwp);
		if ((status = deltoeol(TRUE, 1)) != TRUE)
			return(status);
		DOT.o = w_left_margin(curwp);
	}

	/* insert passed in chars */
	while ((c = *np++) != 0) {
		if (((c == '\n') ? lnewline() : linsert(1,c)) != TRUE)
			return(FALSE);
	}
	if (type == 0) {
		status = lnewline();
		(void)backline(TRUE, 1);
	}
	return(status);
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
	register LINEPTR lp1;
	register LINEPTR lp2;
	register WINDOW *wp;
	size_t	len, add;

	lp1 = DOT.l;
	len = llength(lp1);
	/* if the current line is empty, remove it */
	if (len == 0) {			/* Blank line.		*/
		toss_to_undo(lp1);
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
		toss_to_undo(lp2);
		lremove(curbp, lp2);
		return (TRUE);
	}
	copy_for_undo(DOT.l);

	/* no room in line above, make room */
	if (add > lp1->l_size - len) {
		char *ntext;
		size_t nsize;
		/* first, create the new image */
		nsize = roundlenup(len + add);
		if ((ntext=castalloc(char, nsize)) == NULL)
			return (FALSE);
		if (lp1->l_text) { /* possibly NULL if l_size == 0 */
			(void)memcpy(&ntext[0], &lp1->l_text[0], len);
			ltextfree(lp1,curbp);
		}
		lp1->l_text = ntext;
		lp1->l_size = nsize;
	}
	(void)memcpy(lp1->l_text + len, lp2->l_text, add);
#if ! WINMARK
	if (MK.l == lp2) {
		MK.l  = lp1;
		MK.o += len;
	}
#endif
	/* check all windows for references to the deleted line */
	for_each_window(wp) {
		if (wp->w_line.l == lp2)
			wp->w_line.l = lp1;
		if (wp->w_dot.l == lp2) {
			wp->w_dot.l  = lp1;
			wp->w_dot.o += len;
		}
#if WINMARK
		if (wp->w_mark.l == lp2) {
			wp->w_mark.l  = lp1;
			wp->w_mark.o += len;
		}
#endif
		if (wp->w_lastdot.l == lp2) {
			wp->w_lastdot.l  = lp1;
			wp->w_lastdot.o += len;
		}
	}
	do_mark_iterate(mp,
			if (mp->l == lp2) {
				mp->l  = lp1;
				mp->o += len;
			}
	);
#if OPT_LINE_ATTRS
	FreeAndNull(lp2->l_attrs);
#endif
	llength(lp1) += add;
	set_lforw(lp1, lforw(lp2));
	set_lback(lforw(lp2), lp1);
	dumpuline(lp1);
	toss_to_undo(lp2);
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
	KILL *kp;	/* ptr to scan kill buffer chunk list */

	/* first, delete all the chunks */
	kbs[n].kbufp = kbs[n].kbufh;
	while (kbs[n].kbufp != NULL) {
		kp = kbs[n].kbufp->d_next;
		free((char *)(kbs[n].kbufp));
		kbs[n].kbufp = kp;
	}

	/* and reset all the kill buffer pointers */
	kbs[n].kbufh = kbs[n].kbufp = NULL;
	kbs[n].kused = 0;
	kbs[n].kbwidth = 0;
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
	kregflag &= ~KNEEDCLEAN;
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
 */
int
kinsert(
int c)		/* character to insert in the kill buffer */
{
	KILL *nchunk;	/* ptr to newly malloced chunk */
	KILLREG *kbp = &kbs[ukb];

	if (kcharpending >= 0) {
		int oc = kcharpending;
		kcharpending = -1;
		kinsert(oc);
	}

	kdone(); /* clean up the (possible) old contents */

	/* check to see if we need a new chunk */
	if (kbp->kused >= KBLOCK || kbp->kbufh == NULL) {
		if ((nchunk = typealloc(KILL)) == NULL)
			return(FALSE);
		if (kbp->kbufh == NULL)	/* set head ptr if first time */
			kbp->kbufh = nchunk;
		/* point the current to this new one */
		if (kbp->kbufp != NULL)
			kbp->kbufp->d_next = nchunk;
		kbp->kbufp = nchunk;
		kbp->kbufp->d_next = NULL;
		kbp->kused = 0;
	}

	/* and now insert the character */
	kbp->kbufp->d_chunk[kbp->kused++] = (char)c;
	kchars++;
	if (c == '\n') {
		klines++;
		if (kregwidth > kbp->kbwidth)
			kbp->kbwidth = kregwidth;
		kregwidth = 0;
	} else {
		kregwidth++;
	}
	return(TRUE);
}

/*
 * Translates the index of a register in kill-buffer list to its name.
 */
int
index2reg(int c)
{
	register int n;

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
	else if (c >= 10 && c < (int)TABLESIZE(kbs))
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
	register int n;

	if (c < 0)
		n = -1;
	else if (isDigit(c))
		n = c - '0';
	else if (isLower(c))
		n = c - 'a' + 10;  /* named buffs are in 10 through 36 */
	else if (isUpper(c))
		n = c - 'A' + 10;
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
		ukb = (short)inx;
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
	static	char	cbuf[2];

	/* take care of incrementing the buffer number, if we're replaying
		a command via 'dot' */
	incr_dot_kregnum();

	if ((status = mlreply_reg("Use named register: ", cbuf, &c, -1)) != TRUE)
		return status;

	i = reg2index(c);
	if (kbm_started(i,FALSE))
		return FALSE;

	/* if we're playing back dot, let its kreg override */
	if (dotcmdactive == PLAY && dotcmdkreg != 0)
		ukb = dotcmdkreg;
	else
		ukb = (short)i;

	if (isUpper(c))
		kregflag |= KAPPEND;

	if (clexec) {
		TBUFF *tok = 0;
		char *name = mac_tokval(&tok);	/* get the next token */
		if (name != 0) {
			status = execute(engl2fnc(name), f, n);
			tb_free(&tok);
		} else {
			status = FALSE;
		}
	} else if (isnamedcmd) {
		status = namedcmd(f,n);
	} else {
		/* get the next command from the keyboard */
		c = kbd_seq();

		/* allow second chance for entering counts */
		do_repeats(&c,&f,&n);

		status = execute(DefaultKeyBinding(c), f, n);
	}

	ukb = 0;
	kregflag = 0;

	return(status);
}

static short lastkb;

/*
 * Returns the index in kbs[] of the next kill-buffer we will write into.
 */
static short
nextkb(void)
{
	short n = lastkb;

	if ((kbs[n].kbflag & (KLINES|KRECT|KAPPEND))
	 && !(kbs[n].kbflag & KYANK)) {
		if (--n < 0) n = 9;
	}
	return n;
}

/* buffers 0 through 9 are circulated automatically for full-line deletes */
/* we re-use one of them until the KLINES flag is on, then we advance */
/* to the next */
void
kregcirculate(int killing)
{
	if (ukb >= 10) /* then the user specified a lettered buffer */
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
			if (kbs[lastkb].kbflag & (KLINES|KAPPEND))
				ukb = (short) ((lastkb + ukb - 1) % 10);
			else
				ukb = (short) ((lastkb + ukb) % 10);
		}
	}
}

static	struct	{
	int	started;
	short	next_kb;
	short	last_kb;
	int	save_ukb;
	USHORT	kregflg;
	KILLREG	killreg;
} undo_kill;

static int kb_size(int ii, KILL *kp)
{
	int size = 0;
	if (kp != 0) {
		if (kp->d_next != 0)
			size = kb_size(ii, kp->d_next);
		size += KbSize(ii, kp);
	}
	return size;
}

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
		ukb = 0;	/* was modified by kregcirculate() */
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
	return doput(f, n, FALSE, EXACT);
}

int
putafter(int f, int n)
{
	return doput(f, n, TRUE, EXACT);
}

int
lineputbefore(int f, int n)
{
	return doput(f, n, FALSE, FULLLINE);
}

int
lineputafter(int f, int n)
{
	return doput(f, n, TRUE, FULLLINE);
}

int
rectputbefore(int f, int n)
{
	return doput(f, n, FALSE, RECTANGLE);
}

int
rectputafter(int f, int n)
{
	return doput(f, n, TRUE, RECTANGLE);
}

static int
doput(int f, int n, int after, REGIONSHAPE shape)
{
	int s, oukb;

	if (!f)
		n = 1;

	oukb = ukb;
	kregcirculate(FALSE);	/* cf: 'index2ukb()' */
	if (kbs[ukb].kbufh == NULL) {
		if (ukb != 0)
			mlwarn("[Nothing in register %c]", index2reg(oukb));
		return(FALSE);
	}

	if (shape == EXACT) {
		if ((kbs[ukb].kbflag & (KLINES|KAPPEND)))
			shape = FULLLINE;
		else if (kbs[ukb].kbflag & KRECT)
			shape = RECTANGLE;
		else
			shape = EXACT;
	}

	if (shape == FULLLINE) {
		if (after && !is_header_line(DOT, curbp))
			DOT.l = lforw(DOT.l);
		DOT.o = 0;
	} else {
		if (after && !is_at_end_of_line(DOT))
			forwchar(TRUE,1);
	}

	(void)setmark();
	TRACE(("before PutChar DOT=%d.%d\n", line_no(curbp, DOT.l), DOT.o));
	s = PutChar(n, shape);
	if (s == TRUE)
		swapmark();
	TRACE(("after PutChar DOT=%d.%d, MK=%d.%d\n", line_no(curbp, DOT.l), DOT.o, line_no(curbp, MK.l), MK.o));
	if (is_header_line(DOT, curbp)) {
		DOT.l = lback(DOT.l);
	}
	if (shape == FULLLINE) {
		(void)firstnonwhite(FALSE,1);
	}
	ukb = 0;
	TRACE(("finally doput(DOT=%d.%d, MK=%d.%d) -> %d\n", line_no(curbp, DOT.l), DOT.o, line_no(curbp, MK.l), MK.o, s));
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
		status = linsert(goalcol-reached, ' ');
	} else if (reached > goalcol) {
		/* there must be a tab there. */
		/* pad to hit column we want */
		DOT.o--;
		status = linsert(goalcol % tabstop_val(curbp), ' ');
	}
	return status;
}

static int
next_line_at_col(C_NUM col, C_NUM *reachedp)
{
	int s = TRUE;
	if (is_last_line(DOT,curbp)) {
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
	register int	c;
	register int	i;
	int status, wasnl, suppressnl;
	L_NUM before;
	C_NUM col = 0, width = 0;
	C_NUM reached = 0;
	int checkpad = FALSE;
	register char	*sp;	/* pointer into string to insert */
	KILL *kp;		/* pointer into kill register */

	if (n < 0)
		return FALSE;

	/* make sure there is something to put */
	if (kbs[ukb].kbufh == NULL)
		return TRUE;		/* not an error, just nothing */

	status = TRUE;
	before = vl_line_count(curbp);
	suppressnl = FALSE;
	wasnl = FALSE;

	/* for each time.... */
	while (n--) {
		kp = kbs[ukb].kbufh;
		if (shape == RECTANGLE) {
			width = kbs[ukb].kbwidth;
			col = getcol(DOT, FALSE);
		}
#define SLOWPUT 0
#if SLOWPUT
		while (kp != NULL) {
			i = KbSize(ukb,kp);
			sp = (char *)kp->d_chunk;
			while (i--) {
				c = *sp++;
				if (shape == RECTANGLE) {
				    if (width == 0 || c == '\n') {
					    if (checkpad) {
						    status = force_text_at_col(
							    col, reached);
						    if (status != TRUE)
							    break;
						    checkpad = FALSE;
					    }
					    if (width && linsert(width, ' ')
								!= TRUE) {
						    status = FALSE;
						    break;
					    }
					    if (next_line_at_col(col,&reached)
							    != TRUE) {
						    status = FALSE;
						    break;
					    }
					    checkpad = TRUE;
					    width = kbs[ukb].kbwidth;
				    }
				    if (c == '\n') {
					    continue; /* did it already */
				    } else {
					    if (checkpad) {
						status = force_text_at_col(
								col, reached);
						if (status != TRUE)
							break;
						checkpad = FALSE;
					    }
					    width--;

					    if (is_header_line(DOT,curbp))
						    suppressnl = TRUE;
					    if (linsert(1, c) != TRUE) {
						    status = FALSE;
						    break;
					    }
					    wasnl = FALSE;
				    }
				} else { /* not rectangle */
				    if (c == '\n') {
					    if (lnewline() != TRUE) {
						    status = FALSE;
						    break;
					    }
					    wasnl = TRUE;
				    } else {
					    if (is_header_line(DOT,curbp))
						    suppressnl = TRUE;
					    if (linsert(1, c) != TRUE) {
						    status = FALSE;
						    break;
					    }
					    wasnl = FALSE;
				    }
				}
			}
			if (status != TRUE)
				break;
			kp = kp->d_next;
		}
#else /* SLOWPUT */
		while (kp != NULL) {
			i = KbSize(ukb,kp);
			sp = (char *)kp->d_chunk;
			if (shape == RECTANGLE) {
				while (i--) {
				    c = *sp++;
				    if (width == 0 || c == '\n') {
					    if (checkpad) {
						    status = force_text_at_col(
							    col, reached);
						    if (status != TRUE)
							    break;
						    checkpad = FALSE;
					    }
					    if (width && linsert(width, ' ')
								!= TRUE) {
						    status = FALSE;
						    break;
					    }
					    if (next_line_at_col(col,&reached)
							    != TRUE) {
						    status = FALSE;
						    break;
					    }
					    checkpad = TRUE;
					    width = kbs[ukb].kbwidth;
				    }
				    if (c == '\n') {
					    continue; /* did it already */
				    } else {
					    if (checkpad) {
						status = force_text_at_col(
								col, reached);
						if (status != TRUE)
							break;
						checkpad = FALSE;
					    }
					    width--;

					    if (is_header_line(DOT,curbp))
						    suppressnl = TRUE;
					    if (linsert(1, c) != TRUE) {
						    status = FALSE;
						    break;
					    }
					    wasnl = FALSE;
				    }
				}
			} else { /* not rectangle */
			    while (i-- > 0) {
				if (is_header_line(DOT,curbp))
				    suppressnl = TRUE;
				if (*sp == '\n') {
				    sp++;
				    if (lnewline() != TRUE) {
					status = FALSE;
					break;
				    }
				    wasnl = TRUE;
				} else {
				    register char *dp;
				    register char *ep = sp+1;
				    /* Find end of line or end of kill buffer */
				    while (i > 0 && *ep != '\n') {
					i--;
					ep++;
				    }
				    /* Open up space in current line */
				    status = linsert((int)(ep - sp), ' ');
				    if (status != TRUE)
					break;
				    dp = DOT.l->l_text
					    + DOT.o - (int)(ep - sp);
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
#endif /* SLOWPUT */
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
			if (shape == FULLLINE && !suppressnl) {
				if (lnewline() != TRUE) {
					status = FALSE;
					break;
				}
			}
		}
	}
	curwp->w_flag |= WFHARD;
	(void)line_report(before);

	return status;
}

static int	lastreg = -1;

/* ARGSUSED */
int
execkreg(int f, int n)
{
	int c, j, jj, status;
	KILL *kp;		/* pointer into kill register */
	static	char	cbuf[2];
	int kbcount, whichkb;
	int i;
	char *sp;
	KILL *tkp;

	if (!f)
		n = 1;
	else if (n <= 0)
		return TRUE;

	if ((status = mlreply_reg("Execute register: ", cbuf, &c, lastreg)) != TRUE)
		return status;

		/* disallow execution of the characters we're recording */
	if (c == KEYST_KCHR) {
		mlwarn("[Error: cannot execute %c-register]", c);
		return FALSE;
	}

	j = reg2index(c);
	if (kbm_started(j,TRUE))
		return FALSE;

	lastreg = c;
	relist_registers();

	/* make sure there is something to execute */
	do {
		jj = index2ukb(j);
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
		while (kbcount) {
			whichkb = kbcount;
			tkp = kp;
			while (--whichkb != 0)
				tkp = tkp->d_next;
			i = KbSize(jj,tkp);
			sp = (char *)tkp->d_chunk+i-1;
			while (i--) {
				mapungetc((*sp--)|YESREMAP);
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
					respbuf, sizeof(respbuf));
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
static	int	show_all_chars;
#endif

/*ARGSUSED*/
static void
makereglist(
int iflag,	/* list nonprinting chars flag */
void *dummy GCC_UNUSED)
{
	register KILL	*kp;
	register int	i, ii, j, c;
	register UCHAR	*p;
	int	any;

#if OPT_UPBUFF
	show_all_chars = iflag;
#endif
	b_set_left_margin(curbp, REGS_PREFIX);
	any = (reg2index(lastreg) >= 0);
	if (any)
		bprintf("last=%c", lastreg);

	for (i = 0; (size_t) i < TABLESIZE(kbs); i++) {
		short	save = ukb;

		ii = index2ukb(i);
		if ((kp = kbs[ii].kbufh) != 0) {
			int first = FALSE;
			if (any++) {
				bputc('\n');
				lsettrimmed(lback(DOT.l));
			}
			if (i > 0) {
				bprintf("%c:%*Q",
					index2reg(i),
					REGS_PREFIX-2, ' ');
			} else {
				bprintf("%*S",
					REGS_PREFIX, "(unnamed)");
			}
			do {
				j = KbSize(ii,kp);
				p = kp->d_chunk;

				while (j-- > 0) {
					if (first) {
						first = FALSE;
						bprintf("%*Q", REGS_PREFIX, ' ');
					}
					c = *p++;
					if (isPrint(c) || !iflag) {
						bputc(c);
					} else if (c != '\n') {
						bputc('^');
						bputc(toalpha(c));
					}
					if (c == '\n') {
						first = TRUE;
						any = 0;
					} else
						any = 1;
				}
			} while ((kp = kp->d_next) != 0);
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
				makereglist, f, (void *)0);
}

#if OPT_UPBUFF


static int
show_Registers(BUFFER *bp)
{
	b_clr_obsolete(bp);
	return showkreg(show_all_chars, 1);
}

static void
relist_registers(void)
{

	if (will_relist_regs)	/* have we already done this? */
		return;

	will_relist_regs = TRUE;

	update_scratch(REGISTERS_BufName, show_Registers);
}
#endif	/* OPT_UPBUFF */

#endif	/* OPT_SHOW_REGS */

/* For memory-leak testing (only!), releases all kill-buffer storage. */
#if NO_LEAKS
void	kbs_leaks(void)
{
	for (ukb = 0; ukb < (int) TABLESIZE(kbs); ukb++) {
		ksetup();
		kdone();
	}
}
#endif
