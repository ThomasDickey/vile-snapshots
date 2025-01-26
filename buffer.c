/*
 * Buffer management.
 * Some of the functions are internal,
 * and some are actually attached to user
 * keys. Like everyone else, they set hints
 * for the display system.
 *
 * $Id: buffer.c,v 1.376 2025/01/26 17:07:24 tom Exp $
 */

#include	"estruct.h"
#include	"edef.h"

#define	update_on_chg(bp) (!b_is_temporary(bp) || show_all)

static BUFFER *last_bp;		/* noautobuffer value */
static BUFFER *this_bp;		/* '%' buffer */
static BUFFER *that_bp;		/* '#' buffer */
static int show_all;		/* true iff we show all buffers */
static int updating_list;

static int bfl_num_1st;
static int bfl_num_end;
static int bfl_buf_1st;
static int bfl_buf_end;

/*--------------------------------------------------------------------------*/

int
buffer_in_use(BUFFER *bp)
{
    if (bp->b_inuse) {
	mlforce("[Buffer '%s' is in use]", bp->b_bname);
	return TRUE;
    }
    return FALSE;
}

int
buffer_is_visible(BUFFER *bp)
{
    if (bp->b_nwnd != 0) {	/* Error if on screen.  */
	mlforce("[Buffer '%s' is being displayed]", bp->b_bname);
	return TRUE;
    }
    return FALSE;
}

int
buffer_is_solo(BUFFER *bp)
{
    if ((curbp == bp) && (find_alt() == NULL)) {
	mlforce("[Can't kill buffer '%s']", bp->b_bname);
	return TRUE;
    }
    return FALSE;
}

/*
 * Returns the buffer-list pointer, if it exists.
 */
static BUFFER *
find_BufferList(void)
{
    return find_b_name(BUFFERLIST_BufName);
}

/*
 * Look for a buffer-pointer in the list, to see if it has been delinked yet.
 */
BUFFER *
find_bp(BUFFER *bp1)
{
    if (bp1 != NULL) {
	BUFFER *bp;
	for_each_buffer(bp) {
	    if (bp == bp1)
		return bp;
	}
    }
    return NULL;
}

/*
 * Returns the total number of buffers in the list.
 */
static int
countBuffers(void)
{
    BUFFER *bp;
    int count = 0;
    for_each_buffer(bp)
	count++;
    return count;
}

/*
 * Returns the n'th buffer created
 */
static BUFFER *
find_nth_created(int n)
{
    BUFFER *bp;

    for_each_buffer(bp) {
	if (bp->b_created == n)
	    return bp;
    }
    return NULL;
}

/*
 * Returns the n'th buffer used
 */
static BUFFER *
find_nth_used(int n)
{
    BUFFER *bp;

    for_each_buffer(bp) {
	if (bp->b_last_used == n)
	    return bp;
    }
    return NULL;
}

/*
 * Returns the buffer with the largest value of 'b_last_used'.
 */
static BUFFER *
find_latest(void)
{
    BUFFER *bp, *maxbp = NULL;

    for_each_buffer(bp) {
	if (maxbp == NULL)
	    maxbp = bp;
	else if (maxbp->b_last_used < bp->b_last_used)
	    maxbp = bp;
    }
    return maxbp;
}

/*
 * Check if the given string contains only pathname-characters.
 */
static int
is_pathchars(const char *fname)
{
    int result = TRUE;
    int empty = TRUE;
    while (*fname != EOS) {
	if (!ispath(CharOf(*fname)))
	    break;
	empty = FALSE;
	++fname;
    }
    return result && !empty;
}

/*
 * Look for a filename in the buffer-list
 */
BUFFER *
find_b_file(const char *fname)
{
    BUFFER *bp;
    char nfname[NFILEN];

    if (is_pathchars(fname)) {
	(void) lengthen_path(vl_strncpy(nfname, fname, sizeof(nfname)));
	for_each_buffer(bp) {
	    if (same_fname(nfname, bp, FALSE))
		return bp;
	}
    }
    return NULL;
}

/*
 * Look for a specific buffer number
 */
BUFFER *
find_b_hist(int number)
{
    BUFFER *bp;

    if (number >= 0) {
	for_each_buffer(bp) {
	    if (!b_is_temporary(bp) && (number-- <= 0))
		break;
	}
    } else
	bp = NULL;
    return bp;
}

/*
 * Look for a buffer-number (i.e., one from the buffer-list display)
 */
static BUFFER *
find_b_number(const char *number)
{
    int c = 0;
    while (isDigit(*number))
	c = (c * 10) + (*number++ - '0');
    if (!*number)
	return find_b_hist(c);
    return NULL;
}

/*
 * Find buffer, given (possibly) filename, buffer name or buffer number.
 */
static BUFFER *
find_buffer(const char *name)
{
    BUFFER *bp;

    if ((bp = find_b_name(name)) == NULL	/* Try buffer */
	&& (bp = find_b_file(name)) == NULL	/* ...then filename */
	&& (bp = find_b_number(name)) == NULL) {	/* ...then number */
	return NULL;
    }

    return bp;
}

/*
 * Find buffer, given (possibly) filename, buffer name or buffer number,
 * warning if we do not find it.
 */
BUFFER *
find_any_buffer(const char *name)
{
    BUFFER *bp;

    if ((bp = find_buffer(name)) == NULL)
	mlforce("[No such buffer] %s", name);

    return bp;
}

/*
 * Delete all instances of window pointer for a given buffer pointer
 */
int
zotwp(BUFFER *bp)
{
    WINDOW *wp;
    BUFFER *nbp;
    BUFFER *obp = NULL;
    WINDOW dummy;
    int s = FALSE;

    TRACE((T_CALLED "zotwp(%s)\n", bp ? bp->b_bname : "null"));

    if (bp != NULL) {

	/*
	 * Locate buffer to switch to after deleting windows.  It can't be
	 * the same as the buffer which is being deleted and if it's an
	 * invisible or scratch buffer, it must have been already visible.
	 * From the set of allowable buffers, choose the most recently
	 * used.
	 */
	for_each_buffer(nbp) {
	    if (nbp != bp && (!b_is_temporary(nbp) || nbp->b_nwnd != 0)) {
		if (obp == NULL)
		    obp = nbp;
		else if (obp->b_last_used < nbp->b_last_used)
		    obp = nbp;
	    }
	}

	/* Delete window instances... */
	for_each_window(wp) {
	    if (wp->w_bufp == bp && valid_window(wheadp->w_wndp)) {
		dummy.w_wndp = wp->w_wndp;
		s = delwp(wp);
		wp = &dummy;
	    }
	}
	if (valid_buffer(obp))
	    s = swbuffer(obp);

    }
    returnCode(s);
}

/*
 * Mark a buffer's created member to 0 (unused), adjusting the other buffers
 * so that there are no gaps.
 */
static void
MarkDeleted(BUFFER *bp)
{
    int created = bp->b_created;

    if (created) {
	bp->b_created = 0;
	for_each_buffer(bp) {
	    if (bp->b_created > created)
		bp->b_created -= 1;
	}
    }
}

/*
 * Mark a buffer's last-used member to 0 (unused), adjusting the other buffers
 * so that there are no gaps.
 */
static void
MarkUnused(BUFFER *bp)
{
    int used = bp->b_last_used;

    if (used) {
	bp->b_last_used = 0;
	for_each_buffer(bp) {
	    if (bp->b_last_used > used)
		bp->b_last_used -= 1;
	}
    }
}

/*
 * After 'bclear()', frees remaining storage for a buffer.
 */
static void
FreeBuffer(BUFFER *bp)
{
    int done = FALSE;
    WINDOW *wp;

    beginDisplay();

    if (bp->b_fname != out_of_mem)
	FreeIfNeeded(bp->b_fname);

#if !WINMARK
    if (is_header_line(MK, bp)) {
	MK.l = NULL;
	MK.o = 0;
    }
#endif
    lfree(buf_head(bp), bp);	/* Release header line. */
    if (delink_bp(bp) || is_delinked_bp(bp)) {
	if (curbp == bp)
	    curbp = NULL;

#if OPT_HILITEMATCH
	clobber_save_curbp(bp);
#endif
#if OPT_MULTIBYTE
	FreeIfNeeded(bp->decode_utf_buf);
#endif
#if OPT_PERL || OPT_TCL || OPT_PLUGIN
	api_free_private(bp->b_api_private);
#endif
	done = TRUE;
    }

    for_each_window(wp) {
	if (wp->w_bufp == bp) {
	    wp->w_bufp = NULL;
	}
    }
    if (done)
	free((char *) bp);	/* Release buffer block */

    endofDisplay();
}

/*
 * Adjust buffer-list's last-used member to account for a new buffer.
 */
static void
TrackAlternate(BUFFER *newbp)
{
    BUFFER *bp;

    if (!updating_list) {
	MarkUnused(newbp);
	if (valid_buffer(bp = find_latest())) {
	    newbp->b_last_used = (bp->b_last_used + 1);
	} else {		/* shouldn't happen... */
	    newbp->b_last_used = 1;
	}
    }
}

/* c is an index (counting only visible/nonscratch) into buffer list */
char *
hist_lookup(int c)
{
    BUFFER *bp = find_b_hist(c);

    return valid_buffer(bp) ? bp->b_bname : NULL;
}

/*
 * Run the $buffer-hook procedure, if it is defined.  Note that we must not do
 * this if curwp->w_bufp != curbp, since that would break the use of DOT and MK
 * throughout the program when performing editing operations.
 */
#if OPT_HOOKS
static void
run_buffer_hook(void)
{
    if (!reading_msg_line
	&& valid_buffer(curbp)
	&& valid_window(curwp)
	&& curwp->w_bufp == curbp) {
	run_a_hook(&bufhook);
    }
}
#else
#define run_buffer_hook()	/*EMPTY */
#endif

#define HIST_SHOW 9

/*
 * Display the names of the buffers along with their index numbers on the
 * minibuffer line.  This is normally done in response to "_", i.e.,
 * "historical-buffer".
 *
 * Returns the index of the "#" buffer in the list, so that we can use this in
 * histbuff0() to select "#" when "_" is repeated (toggled).
 */
static int
hist_show(int lo_limit, int hi_limit, int cycle)
{
    int i;
    int first = -1;
    char line[(HIST_SHOW + 1) * (NBUFN + 10)];
    BUFFER *abp = (BUFFER *) 0;

    TRACE((T_CALLED "hist_show(%d..%d) %d\n", lo_limit, hi_limit, cycle));

    if (!global_g_val(GMDABUFF)) {
	abp = find_alt();
    }

    (void) strcpy(line, "");
    for (i = lo_limit; i <= hi_limit; ++i) {
	int j = ((i - lo_limit) + cycle) % (hi_limit + 1 - lo_limit) + lo_limit;
	BUFFER *bp = find_b_hist(j);

	if (bp != NULL && bp != curbp) {
	    if (first < 0)
		first = j;
	    if (!global_g_val(GMDABUFF)) {
		if (abp && abp == bp)
		    first = j;
	    }
	    (void) lsprintf(line + strlen(line), "  %d%s%s %s",
			    j,
			    b_is_changed(bp) ? "*" : "",
			    (abp && abp == bp) ? "#" : "",
			    bp->b_bname);
	}
    }
    if (first >= 0) {
	mlforce("%s", line);
    }
    returnCode(first);
}

static int
valid_history_index(int hist)
{
    BUFFER *bp;
    return ((bp = find_b_hist(hist)) != NULL && bp != curbp);
}

/*
 * Given a buffer, returns any corresponding WINDOW pointer
 */
WINDOW *
bp2any_wp(BUFFER *bp)
{
    WINDOW *wp;
    for_each_visible_window(wp) {
	if (wp->w_bufp == bp)
	    break;
    }
    /*
     * Check for special case where we're running scripts before the screen is
     * initialized, and want to ensure we do not put buffers into wminip.
     */
    if (wp == NULL && !is_vtinit() && wnullp != NULL && wnullp->w_bufp == bp)
	wp = wnullp;
    return wp;
}

/*
 * Worker function for histbuff() and histbuff_to_current_window().
 */
static int
histbuff0(int f, int n, int this_window)
{
    int thiskey, c;
    BUFFER *bp = NULL;
    int cycle = 0;
    int first;
    char *bufn;

    if (f == FALSE) {
	int hi_limit = HIST_SHOW;
	int lo_limit = 1;

	/* verify that we have a list which we can display */
	while (!valid_history_index(hi_limit) && hi_limit > 1) {
	    --hi_limit;
	}
	if (valid_history_index(0))
	    lo_limit = 0;
	if (lo_limit > hi_limit)
	    return FALSE;

	thiskey = lastkey;
	do {
	    if ((first = hist_show(lo_limit, hi_limit, cycle)) < 0)
		return FALSE;
	    c = keystroke();
	    mlerase();
	    if (c == KEY_Tab) {
		if (++cycle >= hi_limit)
		    cycle = 0;
		c = -1;
	    } else if (isBackTab((UINT) c)) {
		if (--cycle < 0)
		    cycle = hi_limit - 1;
		c = -1;
	    } else if (c == thiskey) {
		c = first;
	    } else if (isDigit(c)) {
		c = c - '0';
	    } else {
		if (!isreturn(c))
		    unkeystroke(c);
		return FALSE;
	    }
	} while (c < 0);
    } else {
	c = n;
    }

    if (c >= 0) {
	if ((bufn = hist_lookup(c)) == NULL) {
	    mlwarn("[No such buffer.]");
	    return FALSE;
	}
	/* first assume it is a buffer name, then a file name */
	if ((bp = find_b_name(bufn)) == NULL)
	    return getfile(bufn, TRUE);

    } else if (bp == NULL) {
	mlwarn("[No alternate buffer]");
	return FALSE;
    }

    return swbuffer_lfl(bp, TRUE, this_window);
}

/*
 * Lets the user select a given buffer by its number.
 */
int
histbuff(int f, int n)
{
    int rc;

    if (!clexec)
	vile_is_busy = TRUE;	/* kill autocolor */
    rc = histbuff0(f, n, FALSE);
    if (!clexec)
	vile_is_busy = FALSE;
    return (rc);
}

/*
 * Like histbuff, but puts buffer in current window.  (Even if the
 * buffer is already onscreen in a different window.)
 */
int
histbuff_to_current_window(int f, int n)
{
    int rc;

    if (!clexec)
	vile_is_busy = TRUE;	/* kill autocolor */
    rc = histbuff0(f, n, TRUE);
    if (!clexec)
	vile_is_busy = FALSE;
    return (rc);
}

/*
 * Returns the alternate-buffer pointer, if any
 */
BUFFER *
find_alt(void)
{
    BUFFER *bp;

    if (global_g_val(GMDABUFF)) {
	BUFFER *any_bp = NULL;
	if ((bp = find_bp(curbp)) == NULL)
	    bp = bheadp;
	for (; bp; bp = bp->b_bufp) {
	    if (bp != curbp) {
		/*
		 * If we allow temporary buffers to be selected as the
		 * alternate, we get into the situation where we try
		 * to load the message buffer, which has a NULL filename.
		 * Note that the 'noautobuffer' case, below, never selects
		 * a temporary buffer as the alternate buffer.
		 */
		if (!b_is_temporary(bp))
		    return bp;
	    }
	}
	return any_bp;
    } else {
	BUFFER *last = NULL, *next = find_latest();

	for_each_buffer(bp) {
	    if ((bp != next)
		&& !b_is_temporary(bp)) {
		if (last) {
		    if (last->b_last_used < bp->b_last_used)
			last = bp;
		} else
		    last = bp;
	    }
	}
	return last;
    }
}

/* make '#' buffer in noautobuffer-mode, given filename */
void
imply_alt(char *fname, int copy, int lockfl)
{
    static int nested;

    BUFFER *bp;
    LINE *lp;
    BUFFER *savebp;
    char nfname[NFILEN];
    char *stripped;

    TRACE((T_CALLED "imply_alt(fname=%s, copy=%d, lockfl=%d)\n",
	   NONNULL(fname), copy, lockfl));

    if (interrupted()
	|| isEmpty(fname)
	|| nested)
	returnVoid();

    if ((stripped = is_appendname(fname)) != NULL)
	fname = stripped;

    /* if fname is a pipe cmd, it can be arbitrarily long */
    vl_strncpy(nfname, fname, sizeof(nfname));
    (void) lengthen_path(nfname);

    if (global_g_val(GMDIMPLYBUFF)
	&& valid_buffer(curbp)
	&& curbp->b_fname != NULL
	&& !same_fname(nfname, curbp, FALSE)
	&& !isInternalName(fname)) {
	savebp = curbp;
	if ((bp = find_b_file(nfname)) == NULL) {
	    L_NUM top = -1;
	    L_NUM now = -1;

	    if ((bp = make_bp(fname, 0)) == NULL) {
		mlforce("[Cannot create buffer]");
	    } else {
		/* fill the buffer */
		b_clr_flags(bp, BFINVS | BFCHG);
		b_set_flags(bp, BFIMPLY);
		bp->b_active = TRUE;
		ch_fname(bp, nfname);
		make_local_b_val(bp, MDNEWLINE);
#if OPT_MULTIBYTE
		COPY_B_VAL(bp, curbp, VAL_BYTEORDER_MARK);
		COPY_B_VAL(bp, curbp, VAL_FILE_ENCODING);
#endif
		if (valid_window(curwp) && curwp->w_bufp == curbp) {
		    top = line_no(curbp, curwp->w_line.l);
		    now = line_no(curbp, DOT.l);
		}

		if (copy) {
		    for_each_line(lp, savebp) {
			if (addline(bp, lvalue(lp), llength(lp)) != TRUE) {
			    mlforce("[Copy-buffer failed]");
			    zotbuf(bp);
			    bp = NULL;
			    break;
			}
		    }
		    if (bp != NULL) {
			set_b_val(bp, MDNEWLINE, b_val(savebp, MDNEWLINE));
		    }
		} else {
		    if (readin(fname, lockfl, bp, FALSE) != TRUE) {
			zotbuf(bp);
			bp = NULL;
		    }
		}
	    }

	    /* setup so that buffer-toggle works as in vi (i.e.,
	     * the top/current lines of the screen are the same).
	     */
	    if (bp != NULL) {
		if (now >= 0) {
		    for_each_line(lp, bp) {
			if (--now == 0) {
			    bp->b_dot.l = lp;
			    bp->b_dot.o = curbp->b_dot.o;
			    break;
			}
			if (--top == 0) {
			    bp->b_wline.l = lp;
			}
		    }
		}
	    }
	} else if (copy && !b_is_changed(bp)) {
	    nested++;
	    if (bp->b_active == TRUE) {
		if (bp2readin(bp, TRUE) != TRUE) {
		    zotbuf(bp);
		    bp = NULL;
		}
	    }
	    nested--;
	}

	DisableHook(&bufhook);
	if (bp != NULL) {
	    make_current(bp);
	    infer_majormode(bp);
	}
	make_current(savebp);
	EnableHook(&bufhook);
    }
    returnVoid();
}

/* switch back to the most recent buffer */
/* ARGSUSED */
int
altbuff(int f GCC_UNUSED, int n GCC_UNUSED)
{
    BUFFER *bp;

    TRACE((T_CALLED "altbuff()\n"));
    if ((bp = find_alt()) == NULL) {
	mlwarn("[No alternate filename to substitute for #]");
	returnCode(FALSE);
    } else {
	returnCode(swbuffer(bp));
    }
}

#if OPT_BNAME_CMPL
static int
qs_bname_cmp(const void *a, const void *b)
{
    return strcmp(*(const char *const *) a, (*(const char *const *) b));
}

static void
free_bname_cmpl(char **list)
{
    int n;
    for (n = 0; list[n] != NULL; ++n) {
	free(list[n]);
    }
    free((char *) list);
}

static char **
init_bname_cmpl(void)
{
    size_t used = 0;
    size_t count;
    char **list = NULL;
    BUFFER *bp;

    if ((count = (size_t) countBuffers()) != 0) {
	beginDisplay();
	if ((list = typeallocn(char *, count + 1)) != NULL) {
	    for_each_buffer(bp) {
		list[used] = strmalloc(add_backslashes(bp->b_bname));
		if (list[used] == NULL) {
		    free_bname_cmpl(list);
		    list = NULL;
		    break;
		}
		++used;
	    }
	}
	if (list != NULL) {
	    list[used] = NULL;
	    qsort(list, used, sizeof(char *), qs_bname_cmp);
	}
	endofDisplay();
    }
    return list;
}

int
bname_complete(DONE_ARGS)
{
    size_t cpos = *pos;
    int status = FALSE;
    char **nptr;

    kbd_init();			/* nothing to erase */
    buf[cpos] = EOS;		/* terminate it for us */

    if ((nptr = init_bname_cmpl()) != NULL) {
	status = kbd_complete(PASS_DONE_ARGS, (const char *) nptr, sizeof(*nptr));
	beginDisplay();
	free_bname_cmpl(nptr);
	endofDisplay();
    }
    return status;
}
#endif

/*
 * Prompt for a buffer name (though we may use a filename - it's not a
 * stringent requirement).
 */
int
ask_for_bname(const char *prompt, char *bufn, size_t len)
{
    int status;

    TRACE((T_CALLED "ask_for_bname\n"));
    if (clexec || isnamedcmd) {
#if OPT_BNAME_CMPL
	status = kbd_string(prompt, bufn, len,
			    '\n', KBD_NORMAL | KBD_MAYBEC, bname_complete);
#else
	status = mlreply(prompt, bufn, len);
#endif
    } else if ((status = screen_to_bname(bufn, len)) != TRUE) {
	mlforce("[Nothing selected]");
    }

    returnCode(status);
}

/*
 * Prompt for a buffer name, ignoring if we cannot find it (creating it then).
 */
#define ASK_FOR_NEW_BNAME(prompt, status, bufn, bp) \
    bufn[0] = EOS; \
    if ((status = ask_for_bname(prompt, bufn, sizeof(bufn))) != TRUE \
	&& (status != SORTOFTRUE)) \
	returnCode(status); \
    if ((bp = find_buffer(bufn)) == NULL) { \
	bp = bfind(bufn, 0); \
    }

/*
 * Prompt for a existing-buffer name, warning if we cannot find it.
 */
#define ASK_FOR_OLD_BNAME(prompt, status, bufn, bp) \
    bufn[0] = EOS; \
    if ((status = ask_for_bname(prompt, bufn, sizeof(bufn))) != TRUE \
	&& (status != SORTOFTRUE)) \
	returnCode(status); \
    if ((bp = find_any_buffer(bufn)) == NULL) { \
	returnCode(FALSE); \
    }

/*
 * Attach a buffer to a window. The
 * values of dot and mark come from the buffer
 * if the use count is 0. Otherwise, they come
 * from some other window.
 */
/* ARGSUSED */
int
usebuffer(int f GCC_UNUSED, int n GCC_UNUSED)
{
    BUFFER *bp;
    int s;
    char bufn[NBUFN];

    TRACE((T_CALLED "usebuffer()\n"));
    ASK_FOR_OLD_BNAME("Use buffer: ", s, bufn, bp);
    returnCode(swbuffer(bp));
}

/*
 * Just like "select-buffer", but we will create the buffer if it does not
 * exist.
 */
int
edit_buffer(int f GCC_UNUSED, int n GCC_UNUSED)
{
    BUFFER *bp;
    int s;
    char bufn[NBUFN];

    TRACE((T_CALLED "edit_buffer()\n"));
    ASK_FOR_NEW_BNAME("Edit buffer: ", s, bufn, bp);
    returnCode(swbuffer(bp));
}

#if !SMALLER
/*
 * Set the current window to display the given buffer.
 */
int
set_window(int f GCC_UNUSED, int n GCC_UNUSED)
{
    BUFFER *bp;
    int s;
    char bufn[NBUFN];

    TRACE((T_CALLED "set_window()\n"));
    ASK_FOR_OLD_BNAME("Set window to buffer: ", s, bufn, bp);
    returnCode(swbuffer_lfl(bp, TRUE, TRUE));
}

/*
 * Open a window for the given buffer.
 */
int
popup_buffer(int f GCC_UNUSED, int n GCC_UNUSED)
{
    BUFFER *bp;
    int s;
    char bufn[NBUFN];

    TRACE((T_CALLED "popup_buffer()\n"));
    ASK_FOR_OLD_BNAME("Popup buffer: ", s, bufn, bp);
    if (bp->b_nwnd == 0)
	returnCode(popupbuff(bp));
    returnCode(TRUE);
}

/*
 * Close all windows which display the given buffer.
 */
int
popdown_buffer(int f GCC_UNUSED, int n GCC_UNUSED)
{
    BUFFER *bp;
    int s;
    char bufn[NBUFN];

    TRACE((T_CALLED "popdown_buffer()\n"));
    ASK_FOR_OLD_BNAME("Popdown buffer: ", s, bufn, bp);
    returnCode(zotwp(bp));
}
#endif /* !SMALLER */

void
set_last_bp(BUFFER *bp)
{
    last_bp = bp;
}

/* switch back to the first buffer (i.e., ":rewind") */
/* ARGSUSED */
int
firstbuffer(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int s = histbuff(TRUE, 0);
    if (!global_g_val(GMDABUFF))
	set_last_bp(s ? curbp : NULL);
    return s;
}

/* switch to the next buffer in the buffer list */
/* ARGSUSED */
int
nextbuffer(int f GCC_UNUSED, int n GCC_UNUSED)
{
    BUFFER *bp;
    BUFFER *stopatbp;
    int result = FALSE;

    TRACE((T_CALLED "nextbuffer()\n"));

    if (global_g_val(GMDABUFF)) {	/* go backward thru buffer-list */
	stopatbp = NULL;
	while (stopatbp != bheadp) {
	    /* get the last buffer in the list */
	    bp = bheadp;
	    while (bp != NULL && bp->b_bufp != stopatbp)
		bp = bp->b_bufp;
	    if (bp == NULL)
		break;
	    stopatbp = bp;
	    /* if that one's invisible, keep trying */
	    if (!b_is_invisible(bp))
		break;
	}
	/* we're back to the top -- they were all invisible */
	result = swbuffer(stopatbp);
    } else {			/* go forward thru args-list */
	if (last_bp == NULL) {
	    set_last_bp(curbp);
	}
	if ((bp = last_bp) != NULL) {
	    for (bp = last_bp->b_bufp; bp; bp = bp->b_bufp) {
		if (b_is_argument(bp)) {
		    break;
		}
	    }
	}
	if (bp != NULL) {
	    result = swbuffer(bp);
	} else {
	    mlforce("[No more files to edit]");
	    result = FALSE;
	}
    }
    returnCode(result);
}

#if !SMALLER
/* switch to the previous buffer in the buffer list */
/* ARGSUSED */
int
prevbuffer(int f GCC_UNUSED, int n GCC_UNUSED)
{
    BUFFER *bp;			/* eligible buffer to switch to */
    BUFFER *stopatbp;		/* eligible buffer to switch to */
    int result = FALSE;

    TRACE((T_CALLED "prevbuffer()\n"));

    if (global_g_val(GMDABUFF)) {	/* go forward thru buffer-list */
	if ((bp = curbp) == NULL)
	    bp = bheadp;
	stopatbp = bp;
	while (valid_buffer(bp) && (bp = bp->b_bufp) != NULL) {
	    /* get the next buffer in the list */
	    /* if that one's invisible, skip it and try again */
	    if (!b_is_invisible(bp)) {
		stopatbp = bp;
		break;
	    }
	}
	/* we're back to the top -- they were all invisible */
	result = swbuffer(stopatbp);
    } else {			/* go backward thru args-list */
	if (curbp == NULL) {
	    bp = find_nth_created(1);
	} else {
	    bp = find_nth_created(curbp->b_created - 1);
	}
	if (bp != NULL) {
	    result = swbuffer(bp);
	} else {
	    mlforce("[No more files to edit]");
	}
    }
    returnCode(result);
}
#endif /* !SMALLER */

/* bring nbp to the top of the list, where curbp usually lives */
void
make_current(BUFFER *nbp)
{
    BUFFER *bp;
#if OPT_PROCEDURES
    BUFFER *ocurbp;

    ocurbp = curbp;
#endif

    TrackAlternate(nbp);

    if (!updating_list && global_g_val(GMDABUFF)) {
	if (nbp != bheadp) {	/* remove nbp from the list */
	    bp = bheadp;
	    while (valid_buffer(bp) && bp->b_bufp != nbp)
		bp = bp->b_bufp;
	    if (valid_buffer(bp))
		bp->b_bufp = nbp->b_bufp;

	    /* put it at the head */
	    nbp->b_bufp = bheadp;

	    bheadp = nbp;
	}
	curbp = bheadp;
    } else
	curbp = nbp;

#if OPT_PROCEDURES
    if (curbp != ocurbp) {
	run_buffer_hook();
    }
#endif

#if OPT_TITLE
    set_editor_title();
#endif
}

/* make buffer BP current */
int
swbuffer(BUFFER *bp)
{
    TRACE((T_CALLED "swbuffer(bp=%p)\n", (void *) bp));
    returnCode(swbuffer_lfl(bp, TRUE, FALSE));
}

#if OPT_MODELINE
#define VI_NAMES "\\(vi\\|vile\\|"PROGRAM_NAME"\\)"
static struct {
    int mark;
    const char *expr;
    regexp *prog;
} mls_patterns[] = {

    {
	3, "^.*\\s" VI_NAMES ":\\s*se\\(t\\)\\?\\s\\+\\(.*\\):.*$", NULL
    },
    {
	2, "^.*\\s" VI_NAMES ":\\s*\\(.*\\)\\s*$", NULL
    },
};

static regexp *
mls_regcomp(int n)
{
    regexp *prog = NULL;
    if (n >= 0 && n < (int) TABLESIZE(mls_patterns)) {
	prog = mls_patterns[n].prog;
	if (prog == NULL) {
	    prog = regcomp(mls_patterns[n].expr,
			   strlen(mls_patterns[n].expr), TRUE);
	    mls_patterns[n].prog = prog;
	}
    }
    return prog;
}

#if NO_LEAKS
static void
mls_regfree(int n)
{
    if (n >= 0) {
	if (n < (int) TABLESIZE(mls_patterns)) {
	    if (mls_patterns[n].prog != NULL) {
		free(mls_patterns[n].prog);
		mls_patterns[n].prog = NULL;
	    }
	}
    } else {
	for (n = 0; n < (int) TABLESIZE(mls_patterns); ++n)
	    mls_regfree(n);
    }
}
#endif

static int
found_modeline(LINE *lp, int *first, int *last)
{
    int rc = 0;
    unsigned n;
    int limit = llength(lp);

    if (limit > modeline_limit)
	limit = modeline_limit;

    for (n = 0; n < TABLESIZE(mls_patterns); ++n) {
	regexp *prog = mls_regcomp((int) n);
	if (lregexec(prog, lp, 0, limit, FALSE)) {
	    int j = mls_patterns[n].mark;
	    *first = (int) (prog->startp[j] - prog->startp[0]);
	    *last = (int) (prog->endp[j] - prog->startp[0]);

	    /*
	     * Real vi may have modes that we do not want to set.
	     */
	    if (prog->endp[1] - prog->startp[1] == 2
		&& !strncmp(prog->startp[1], "vi", (size_t) 2))
		rc = 2;
	    else
		rc = 1;
	}
    }
    return rc;
}

static void
do_one_modeline(LINE *lp, int vi, int first, int last)
{
    TBUFF *data = NULL;

    if (lisreal(lp) && first >= 0 && last > first && last <= llength(lp)) {
	tb_sappend(&data, "setl ");

	/*
	 * Trim trailing blanks, e.g., in case we are processing a "set dos"
	 * in a file containing trailing ^M's.
	 */
	while (last > first && isSpace(lgetc(lp, last - 1)))
	    --last;

	tb_bappend(&data, lvalue(lp) + first, (size_t) last - (size_t) first);
	tb_append(&data, EOS);

	in_modeline = vi;
	TPRINTF(("modeline %s:%s\n",
		 ((vi > 1) ? "vi" : PROGRAM_NAME),
		 tb_values(data)));
	docmd(tb_values(data), TRUE, FALSE, 1);
	in_modeline = FALSE;

	tb_free(&data);
    }
}

void
do_modelines(BUFFER *bp)
{
    WINDOW *wp;

    if (b_val(bp, MDMODELINE) &&
	b_val(bp, VAL_MODELINES) > 0 &&
	(wp = bp2any_wp(bp)) != NULL) {
	LINE *lp;
	int once = b_val(bp, VAL_MODELINES);
	int limit = 2 * once;
	int count;
	int rc, first = 0, last = 0;
	BUFFER *save_bp = curbp;
	WINDOW *save_wp = curwp;

	/*
	 * If we're sourcing a file, then curbp is not necessarily the same.
	 * Fix that.
	 */
	curwp = wp;
	curbp = bp;

	bsizes(bp);

	TRACE((T_CALLED "do_modelines(%s) limit %d, size %d\n",
	       bp->b_bname, limit, bp->b_linecount));

	count = 0;
	for (lp = lforw(buf_head(bp)); count < once; lp = lforw(lp)) {
	    --limit;
	    ++count;
	    if ((rc = found_modeline(lp, &first, &last)) != 0) {
		do_one_modeline(lp, rc, first, last);
	    }
	}

	count = 0;
	if (bp->b_linecount < limit) {
	    once = bp->b_linecount - once;
	}
	for (lp = lback(buf_head(bp)); count < once; lp = lback(lp)) {
	    ++count;
	    if ((rc = found_modeline(lp, &first, &last)) != 0) {
		do_one_modeline(lp, rc, first, last);
	    }
	}
	curbp = save_bp;
	curwp = save_wp;
	returnVoid();
    }
}
#endif

static int
suckitin(BUFFER *bp, int copy, int lockfl)
{
    int s = FALSE;

    TRACE((T_CALLED "suckitin(%s, %s)\n", bp->b_bname, copy ? "copy" : "new"));
    if (copy) {
	WINDOW *wp;

	for_each_window(wp) {
	    if (wp->w_bufp == bp && (is_visible_window(wp) || bp->b_active
				     != TRUE))
		copy_traits(&(wp->w_traits), &(bp->b_wtraits));
	}
    }
    curwp->w_flag |= WFMODE | WFHARD;	/* Quite nasty.         */

    if (bp->b_active != TRUE) {	/* buffer not active yet */
	s = bp2readin(bp, lockfl);	/* read and activate it */
    }
#ifdef MDCHK_MODTIME
    else
	s = check_file_changed(bp, bp->b_fname);
#endif
    updatelistbuffers();
    run_buffer_hook();
    returnCode(s);
}

/*
 * Make given buffer 'bp' current'.
 */
int
swbuffer_lfl(BUFFER *bp, int lockfl, int this_window)
{
    int status = TRUE;
    WINDOW *wp;

    TRACE((T_CALLED
	   "swbuffer_lfl(bp=%p, lockfl=%d, this_window=%d) bname='%s'\n",
	   (void *) bp, lockfl, this_window, bp->b_bname));

    if (!bp) {
	mlforce("BUG:  swbuffer passed null bp");
	status = FALSE;
    } else {
	TRACE(("swbuffer(%s) nwnd=%d\n", bp->b_bname, bp->b_nwnd));
	if (curbp == bp
	    && valid_window(curwp)
	    && DOT.l != NULL
	    && curwp->w_bufp == bp) {	/* no switching to be done */

	    if (!b_is_reading(bp) &&
		!bp->b_active) {	/* on second thought, yes there is */
		status = suckitin(bp, TRUE, lockfl);
	    }
	} else {
#if !WINMARK
	    /* Whatever else we do, make sure MK isn't bogus when we leave */
	    MK = nullmark;
#endif

	    if (valid_buffer(curbp)) {
		/* if we'll have to take over this window, and it's the last */
		if ((this_window || bp->b_nwnd == 0)
		    && curbp->b_nwnd != 0 && --(curbp->b_nwnd) == 0) {
#if !SMALLER
		    if (!this_window)
#endif
			undispbuff(curbp, curwp);
		} else if (DOT.l != NULL && (this_window || bp->b_nwnd == 0)) {
		    /* Window is still getting taken over so make a
		       copy of the traits for a possible future
		       set-window */
		    copy_traits(&(curbp->b_wtraits), &(curwp->w_traits));
		}
	    }

	    /* don't let make_current() call the hook -- there's
	       more to be done down below */
	    DisableHook(&bufhook);
	    make_current(bp);	/* sets curbp */
	    EnableHook(&bufhook);

	    bp = curbp;		/* if running the bufhook caused an error,
				   we may be in a different buffer than we
				   thought we were going to */
	    if (bp == NULL) {
		mlforce("BUG:  make_current set null bp");
		status = FALSE;
	    }
#if !SMALLER
	    else if (this_window) {
		/*
		 * Put buffer in this window even if it's already on the screen
		 */
		LINE *lp;
		int trait_matches;

		if (curwp == NULL || curwp->w_bufp == NULL)
		    returnCode(FALSE);
		else if (curwp->w_bufp == bp)
		    returnCode(TRUE);

		if (curwp->w_bufp->b_nwnd == 0) {
		    undispbuff(curwp->w_bufp, curwp);
		} else {
		    copy_traits(&(curwp->w_bufp->b_wtraits),
				&(curwp->w_traits));
		}

		/*
		 * Initialize the window using the saved buffer traits if
		 * possible.  If they don't pass a sanity check, simply
		 * initialize the newest window to be a clone of any other
		 * window on the buffer, or initialize it as we do in
		 * popupbuff().
		 */
		trait_matches = 0;
#if WINMARK
#define TRAIT_MATCHES_NEEDED 5
#else
#define TRAIT_MATCHES_NEEDED 4
#endif
		for_each_line(lp, bp) {
		    if (lp == bp->b_dot.l && llength(lp) > bp->b_dot.o)
			trait_matches++;
#if WINMARK
		    if (lp == bp->b_mark.l && llength(lp) > bp->b_mark.o)
			trait_matches++;
#endif
		    if (lp == bp->b_lastdot.l && llength(lp) > bp->b_lastdot.o)
			trait_matches++;
		    if (lp == bp->b_tentative_lastdot.l && llength(lp) > bp->b_tentative_lastdot.o)
			trait_matches++;
		    if (lp == bp->b_wline.l && llength(lp) > bp->b_wline.o)
			trait_matches++;
		    if (trait_matches == TRAIT_MATCHES_NEEDED)
			break;
		}
		if (trait_matches == TRAIT_MATCHES_NEEDED)
		    copy_traits(&(curwp->w_traits), &(bp->b_wtraits));
		else if ((wp = bp2any_wp(bp)) == NULL)
		    init_window(curwp, bp);
		else
		    clone_window(curwp, wp);

		curwp->w_bufp = bp;
		status = suckitin(bp, (bp->b_nwnd++ == 0), lockfl);
	    } else
#endif /* !SMALLER */
		/* get it already on the screen if possible */
	    if (bp->b_nwnd != 0) {
		/* then it's on the screen somewhere */
		if ((wp = bp2any_wp(bp)) == NULL)
		    mlforce("BUG: swbuffer: wp still NULL");
		curwp = wp;
		upmode();
#ifdef MDCHK_MODTIME
		(void) check_file_changed(bp, bp->b_fname);
#endif
#if OPT_UPBUFF
		if (bp != find_BufferList())
		    updatelistbuffers();
#endif
		run_buffer_hook();
		status = (find_bp(bp) != NULL);
	    } else if (curwp == NULL) {
		status = FALSE;	/* we haven't started displaying yet */
	    } else {
		if (!is_vtinit()) {
		    curwp = wnullp;
		}
		/* oh well, suck it into this window */
		curwp->w_bufp = bp;
		status = suckitin(bp, (bp->b_nwnd++ == 0), lockfl);
	    }
	}
#if OPT_TITLE
	set_editor_title();
#endif
    }
    if (status == TRUE)
	set_last_bp(bp);
    returnCode(status);
}

#if VILE_NEEDED
/* check to ensure any buffer that thinks it's displayed _is_ displayed */
void
buf_win_sanity(void)
{
    BUFFER *bp;
    for_each_buffer(bp) {
	if (bp->b_nwnd != 0) {	/* then it's on the screen somewhere */
	    WINDOW *wp = bp2any_wp(bp);
	    if (!wp) {
		dbgwrite("BUG: swbuffer 1: wp is NULL");
	    }
	}
    }
}
#endif

int
eql_bname(BUFFER *bp, const char *name)
{
    return (cs_strcmp(global_g_val(GMDFILENAME_IC), bp->b_bname, name) == 0);
}

void
undispbuff(BUFFER *bp, WINDOW *wp)
{
    /* get rid of it completely if it's a scratch buffer,
       or it's empty and unmodified */
    if (b_is_scratch(bp)) {
	/* Save the location within the help-file */
	if (eql_bname(bp, HELP_BufName))
	    help_at = line_no(bp, wp->w_dot.l);
	(void) zotbuf(bp);
    } else if (global_g_val(GMDABUFF) &&
	       !b_is_argument(bp) &&
	       !b_is_changed(bp) &&
	       is_empty_buf(bp)
	       && !ffexists(bp->b_fname)) {
	(void) zotbuf(bp);
    } else {			/* otherwise just adjust it off the screen */
	copy_traits(&(bp->b_wtraits), &(wp->w_traits));
    }
}

#if !OPT_MAJORMODE
/* return true iff c-mode is active for this buffer */
static int
cmode_active(BUFFER *bp)
{
    if (is_local_b_val(bp, MDCMOD))
	return is_c_mode(bp);
    else
	return (is_c_mode(bp) && has_C_suffix(bp));
}
#endif

/* return the correct tabstop setting for this buffer */
int
tabstop_val(BUFFER *bp)
{
#if OPT_MAJORMODE
    int i = b_val(bp, VAL_TAB);
#else
    int i = b_val(bp, (cmode_active(bp) ? VAL_C_TAB : VAL_TAB));
#endif
    if (i == 0)
	return 1;
    return i;
}

/* return the correct shiftwidth setting for this buffer */
int
shiftwid_val(BUFFER *bp)
{
#if OPT_MAJORMODE
    int i = b_val(bp, VAL_SWIDTH);
#else
    int i = b_val(bp, (cmode_active(bp) ? VAL_C_SWIDTH : VAL_SWIDTH));
#endif
    if (i == 0)
	return 1;
    return i;
}

#if !OPT_MAJORMODE
int
has_C_suffix(BUFFER *bp)
{
    int s;
    s = nregexec(global_g_val_rexp(GVAL_CSUFFIXES)->reg,
		 bp->b_fname, (char *) 0, 0, -1,
		 global_g_val(GMDFILENAME_IC));
    return s;
}
#endif

int
kill_that_buffer(BUFFER *bp)
{
    int s;
    char bufn[NFILEN];

    /* ...for info-message */
    (void) vl_strncpy(bufn, bp->b_bname, sizeof(bufn));
    if (buffer_is_solo(bp)) {
	return FALSE;
    }

    if (bp->b_nwnd != 0) {	/* then it's on the screen somewhere */
	(void) zotwp(bp);
	if (find_bp(bp) == NULL) {	/* delwp must have zotted us */
	    return FALSE;
	}
    }
    if ((s = zotbuf(bp)) == TRUE)
	mlwrite("Buffer %s gone", bufn);
    return s;
}

static void
free_buffer_list(char **list)
{
    int n;
    for (n = 0; list[n] != NULL; ++n) {
	free(list[n]);
    }
    free(list);
}

/*
 * Make a list of buffer names to process.
 */
static char **
make_buffer_list(char *bufn)
{
    BUFFER *bp;
    int count = 0;
    int limit = countBuffers();
    char **result = typecallocn(char *, (size_t) limit + 1);

    if (result != NULL) {
#if OPT_FORBUFFERS_CHOICES
#if OPT_GLOB_RANGE
	/*
	 * If it looks like a scratch buffer, try to find exactly that before
	 * possibly getting confused with range expressions, etc.
	 */
	if (global_g_val(GVAL_FOR_BUFFERS) == FB_MIXED
	    && is_scratchname(bufn)
	    && (bp = find_b_name(bufn)) != NULL) {
	    result[count++] = strmalloc(bp->b_bname);
	} else
#endif
	    if (global_g_val(GVAL_FOR_BUFFERS) == FB_MIXED
		|| global_g_val(GVAL_FOR_BUFFERS) == FB_GLOB) {
	    for_each_buffer(bp) {
		if (glob_match_leaf(bp->b_bname, bufn)) {
		    result[count++] = strmalloc(bp->b_bname);
		}
	    }
	} else if (global_g_val(GVAL_FOR_BUFFERS) == FB_REGEX) {
	    regexp *exp;

	    if ((exp = regcomp(bufn, strlen(bufn), TRUE)) != NULL) {
		for_each_buffer(bp) {
		    if (nregexec(exp, bp->b_bname, (char *) 0, 0, -1, FALSE)) {
			result[count++] = strmalloc(bp->b_bname);
		    }
		}
		free(exp);
	    }
	}
	if (count == 0) {
	    if ((bp = find_buffer(bufn)) != NULL) {	/* Try buffer */
		result[count++] = strmalloc(bp->b_bname);
	    }
	}
#else
	if ((bp = find_buffer(bufn)) != 0) {	/* Try buffer */
	    result[count++] = strmalloc(bp->b_bname);
	}
#endif
	if (count == 0) {
	    free_buffer_list(result);
	    result = NULL;
	}
    }
    return result;
}

#if !SMALLER
/*
 * Select a set of buffers with the first parameter, and prompt for a command
 * to execute on each buffer.
 */
int
for_buffers(int f, int n)
{
    BUFFER *bp;
    int s;
    char bufn[NFILEN];
    char command[NSTRING];
    char **list;
    int inlist;
    int updated = 0;

    bufn[0] = EOS;
    if ((s = ask_for_bname("Buffers: ", bufn, sizeof(bufn))) != TRUE)
	return s;

    if ((list = make_buffer_list(bufn)) != NULL) {
	*command = EOS;
	/*
	 * Don't do command-completion, since we want to allow range
	 * expressions, e.g.,
	 *      :fb *.h %trim
	 *
	 * FIXME: split-out logic in execute_named_command() to allow proper
	 * parsing to linespec and command-verb.
	 */
	if ((s = kbd_string("Command: ",
			    command,
			    sizeof(command),
			    '\n',
			    KBD_QUOTES,
			    no_completion)) == TRUE) {
	    char *save_execstr = execstr;
	    int save_clexec = clexec;

	    for (inlist = 0; list[inlist] != NULL; ++inlist) {
		if ((bp = find_b_name(list[inlist])) != NULL
		    && swbuffer(bp)) {
		    char this_command[NSTRING];

		    /*
		     * FIXME: does namedcmd() modify what execstr points to?
		     */
		    execstr = vl_strncpy(this_command, command, sizeof(this_command));
		    clexec = TRUE;

		    if (namedcmd(f, n))
			++updated;
		}
	    }
	    execstr = save_execstr;
	    clexec = save_clexec;
	}
	free_buffer_list(list);
    } else {
	mlforce("[No matching buffers]");
    }

    if (updated) {
	mlforce("[Processed %d buffer%s]", updated, PLURAL(updated));
	s = TRUE;
    }
    return s;
}
#endif /* !SMALLER */

/*
 * Dispose of a buffer, by name.  If this is a screen-command, pick the name up
 * from the screen.  Otherwise, ask for the name.  Look it up (don't get too
 * upset if it isn't there at all!).  Get quite upset if the buffer is being
 * displayed.  Clear the buffer (ask if the buffer has been changed).  Then
 * free the header line and the buffer header.
 *
 * If we are given a repeat-count, try to kill that many buffers.  Killing from
 * names selected from the buffer-list is a special case, because the cursor
 * may be pointing to the buffer-number column.  In that case, we must
 * recompute the buffer-contents.  Otherwise, move the cursor down one line
 * (staying in the same column), so we can pick up the names from successive
 * lines.
 */
int
killbuffer(int f, int n)
{
    BUFFER *bp;
    int s;
    char bufn[NFILEN];
    char **list;
    int inlist;
    int removed = 0;

#if OPT_UPBUFF
    C_NUM save_COL;
    MARK save_DOT;
    MARK save_TOP;

    int animated = (f
		    && (n > 1)
		    && valid_window(curwp)
		    && valid_buffer(curbp)
		    && (curbp == find_BufferList()));
    /*
     * Handle special case if we're pointing in the buffer list to either the
     * column containing current buffer number, or buffer names.  Since we'll
     * remove the line containing that information as the buffer-list is
     * updated, we don't want to move down a line at the same time.
     */
    int special = animated && ((DOT.o >= bfl_num_1st && DOT.o < bfl_num_end) ||
			       (DOT.o >= bfl_buf_1st && DOT.o < bfl_buf_end));

    if (animated && !special) {
	save_COL = getccol(FALSE);
	save_DOT = DOT;
	save_TOP = curwp->w_line;
    } else {
	save_COL = 0;		/* appease gcc */
	save_DOT = nullmark;
	save_TOP = nullmark;
    }
#endif

    n = need_a_count(f, n, 1);

    for_ever {
	bufn[0] = EOS;
	if ((s = ask_for_bname("Kill buffer: ", bufn, sizeof(bufn))) != TRUE)
	    break;

	if ((list = make_buffer_list(bufn)) != NULL) {
	    for (inlist = 0; list[inlist] != NULL; ++inlist) {
		if ((bp = find_b_name(list[inlist])) != NULL
		    && kill_that_buffer(bp)) {
		    ++removed;
		}
	    }
	    free_buffer_list(list);
	}

	if (--n > 0) {
#if OPT_UPBUFF
	    if (special)
		(void) update(TRUE);
	    else
#endif
	    if (!forwline(FALSE, 1))
		break;
	} else
	    break;
    }

    if (removed) {
	mlforce("[Removed %d buffer%s]", removed, PLURAL(removed));
	s = TRUE;
    }
#if OPT_UPBUFF
    if (animated && !special) {
	curgoal = save_COL;
	DOT = save_DOT;
	DOT.o = getgoal(DOT.l);
	curwp->w_line = save_TOP;
	clr_typed_flags(curwp->w_flag, USHORT, WFMOVE);
    }
#endif
    return s;
}

/*
 * Unlink a buffer from the list, adjusting last-used and created counts.
 */
int
delink_bp(BUFFER *bp)
{
    BUFFER *bp1, *bp2;

    bp1 = NULL;			/* Find the header.     */
    bp2 = bheadp;
    while (bp2 != bp) {
	bp1 = bp2;
	bp2 = bp2->b_bufp;
	if (bp2 == NULL)
	    return FALSE;
    }
    bp2 = bp2->b_bufp;		/* Next one in chain.   */
    if (bp1 == NULL)		/* Unlink it.               */
	bheadp = bp2;
    else
	bp1->b_bufp = bp2;
    MarkUnused(bp);
    MarkDeleted(bp);
    if (bp == last_bp)
	last_bp = NULL;
    return TRUE;
}

char *
strip_brackets(char *dst, const char *src)
{
    size_t len;

    if (*src == SCRTCH_LEFT[0])
	src++;
    (void) strcpy(dst, src);
    if ((len = strlen(dst)) != 0
	&& dst[len - 1] == SCRTCH_RIGHT[0]) {
	dst[--len] = EOS;
    }
    return dst;
}

char *
add_brackets(char *dst, const char *src)
{
    if (dst != NULL && src != NULL) {
	size_t len = strlen(src);
	if (len > NBUFN - 3)
	    len = NBUFN - 3;
	dst[0] = SCRTCH_LEFT[0];
	(void) strncpy(&dst[1], src, len);
	(void) strcpy(&dst[len + 1], SCRTCH_RIGHT);
    }
    return dst;
}

/* kill the buffer pointed to by bp */
int
zotbuf(BUFFER *bp)
{
    int status;

    TRACE((T_CALLED "zotbuf(bp=%p)\n", (void *) bp));

    if (find_bp(bp) == NULL)	/* delwp may have zotted us, pointer obsolete */
	returnCode(TRUE);

    TRACE(("zotbuf(%s)\n", bp->b_bname));

    if (buffer_in_use(bp))
	returnCode(FALSE);
    if (buffer_is_visible(bp))	/* Error if on screen.      */
	returnCode(FALSE);
    if (is_fake_window(wheadp)) {
	WINDOW *wp;
	WINDOW dummy;
	/* Not on screen, but a fake window might refer to it.  So
	   delete all such fake windows */
	for_each_window(wp) {
	    if (is_fake_window(wp)
		&& wp->w_bufp == bp && valid_window(wheadp->w_wndp)) {
		dummy.w_wndp = wp->w_wndp;
		(void) delwp(wp);
		wp = &dummy;
	    }
	}
    }
#if OPT_LCKFILES
    /* If Buffer is killed and not locked by other then release own lock */
    if (global_g_val(GMDUSEFILELOCK)) {
	if (bp->b_active)
	    if (!b_val(curbp, MDLOCKED) && !b_val(curbp, MDVIEW))
		release_lock(bp->b_fname);
    }
#endif
    /* Blow text away.      */
    if ((status = bclear(bp)) == TRUE) {
#if OPT_NAMEBST
	if (tb_values(bp->b_procname) != NULL) {
	    delete_namebst(tb_values(bp->b_procname), TRUE, FALSE, 0);
	    tb_free(&(bp->b_procname));
	}
#endif
	FreeBuffer(bp);
	updatelistbuffers();
    }
    returnCode(status);
}

BUFFER *
zotbuf2(BUFFER *bp)
{
    if (zotbuf(bp))
	bp = NULL;
    return bp;
}

/* Rename a buffer given a buffer pointer and new buffer name. */
int
renamebuffer(BUFFER *rbp, char *bufname)
{
    BUFFER *bp;			/* pointer to scan through all buffers */
    char bufn[NBUFN];		/* buffer to hold buffer name */
    WINDOW *wp;

    TRACE((T_CALLED "renamebuffer(%s, %s)\n", rbp->b_bname, bufname));

    if (*mktrimmed(vl_strncpy(bufn, bufname, sizeof(bufn))) == EOS)
	returnCode(ABORT);

    bp = find_b_name(bufn);

    if (bp == curbp)
	returnCode(ABORT);	/* no change */

    if (valid_buffer(bp))
	returnCode(FALSE);	/* name already in use */

#if OPT_NAMEBST
    if (is_scratchname(rbp->b_bname)) {
	char procname[NBUFN];
	(void) strip_brackets(procname, rbp->b_bname);
	if (search_namebst(procname, 0)
	    && rename_namebst(procname, bufn, 0) != TRUE)
	    returnCode(ABORT);
    }
#endif
    set_bname(rbp, bufn);	/* copy buffer name to structure */

    for_each_visible_window(wp) {
	if (wp->w_bufp == rbp)
	    wp->w_flag |= WFMODE;
    }

    returnCode(TRUE);
}

static int
rename_specific_buffer(BUFFER *bp)
{
    static char bufn[NBUFN];	/* buffer to hold buffer name */
    const char *prompt = "New name for buffer: ";
    int status;

    /* prompt for and get the new buffer name */
    do {
	if (mlreply(prompt, bufn, (UINT) sizeof(bufn)) != TRUE)
	    return (FALSE);
	prompt = "That name's been used.  New name: ";
	status = renamebuffer(bp, bufn);
	if (status == ABORT)
	    return (FALSE);
    } while (status == FALSE);

    b_clr_scratch(bp);		/* if renamed, we must want it */
    mlerase();
    updatelistbuffers();
    return (TRUE);
}

/* Rename the current buffer */
/* ARGSUSED */
int
namebuffer(int f GCC_UNUSED, int n GCC_UNUSED)
{
    TRACE((T_CALLED "namebuffer()\n"));
    returnCode(rename_specific_buffer(curbp));
}

/*
 * Rename a buffer, may be the current one.
 */
int
name_a_buffer(int f GCC_UNUSED, int n GCC_UNUSED)
{
    BUFFER *bp;
    int s;
    char bufn[NBUFN];

    TRACE((T_CALLED "name_a_buffer()\n"));
    ASK_FOR_OLD_BNAME("Rename buffer: ", s, bufn, bp);
    returnCode(rename_specific_buffer(bp));
}

/* create or find a window, and stick this buffer in it.  when
	done, we own the window and the buffer, but they are _not_
	necessarily curwp and curbp */
int
popupbuff(BUFFER *bp)
{
    WINDOW *wp;

    TRACE((T_CALLED "popupbuff(%s) nwnd=%d\n", bp->b_bname, bp->b_nwnd));
    if (!valid_buffer(curbp)) {
	curbp = bp;		/* possibly at startup time */
	curwp->w_bufp = curbp;
	++curbp->b_nwnd;
    } else if (bp->b_nwnd == 0) {	/* Not on screen yet. */
	if ((wp = wpopup()) == NULL)
	    return FALSE;
	if (--wp->w_bufp->b_nwnd == 0)
	    undispbuff(wp->w_bufp, wp);
	wp->w_bufp = bp;
	++bp->b_nwnd;
    }

    for_each_window(wp) {
	if (wp->w_bufp == bp) {
	    init_window(wp, bp);
	}
    }
    returnCode(swbuffer(bp));
}

/*
 * Invoked after changing mode 'autobuffer', this relinks the list of buffers
 * sorted according to the mode: by creation or last-used order.
 */
void
sortlistbuffers(void)
{
    BUFFER *bp, *newhead = NULL;
    int c;

    if (global_g_val(GMDABUFF)) {
	c = 1;
	while (valid_buffer(bp = find_nth_used(c++))) {
	    bp->b_relink = newhead;
	    newhead = bp;
	}
    } else {
	c = countBuffers();
	while (valid_buffer(bp = find_nth_created(c--))) {
	    bp->b_relink = newhead;
	    newhead = bp;
	}
    }

    for (bp = newhead; bp; bp = bp->b_relink)
	bp->b_bufp = bp->b_relink;
    bheadp = newhead;

    updatelistbuffers();
}

/*
 * List all of the active buffers.  First update the special buffer that holds
 * the list.  Next make sure at least 1 window is displaying the buffer list,
 * splitting the screen if this is what it takes.  Lastly, repaint all of the
 * windows that are displaying the list.  A numeric argument causes it to
 * toggle listing invisible buffers; a subsequent argument forces it to a
 * normal listing.
 */
int
togglelistbuffers(int f, int n)
{
    int status;
    BUFFER *bp;

    /* if it doesn't exist, create it */
    if ((bp = find_BufferList()) == NULL) {
	status = listbuffers(f, n);
	/* if user supplied argument, re-create */
    } else if (f) {
	status = listbuffers(!show_all, n);
    } else {
	/* if it does exist, delete the window, which in turn
	   will kill the BFSCRTCH buffer */
	status = zotwp(bp);
    }

    return status;
}

/*
 * Return the status flags ordered so the most important one for [Buffer List]
 * is first.  We use the whole string in $bflags though.
 */
void
buffer_flags(char *dst, BUFFER *bp)
{
    if (valid_buffer(bp)) {
	if (b_is_directory(bp))
	    *dst++ = ('d');
	if (b_is_scratch(bp))
	    *dst++ = ('s');
	if (!(bp->b_active))
	    *dst++ = ('u');
	if (b_is_implied(bp))
	    *dst++ = ('a');
	if (b_is_invisible(bp))
	    *dst++ = ('i');
	if (b_is_changed(bp))
	    *dst++ = ('m');
    }
    *dst = EOS;
}

/*
 * Track/emit footnotes for 'makebufflist()', showing only the ones we use.
 */
#if !SMALLER
static void
footnote(int c)
{
    /* *INDENT-OFF* */
    static struct {
	const char *name;
	int flag;
    } table[] = {
	{ "automatic",	0 },
	{ "directory",	0 },
	{ "invisible",	0 },
	{ "modified",	0 },
	{ "scratch",	0 },
	{ "unread",	0 },
    };
    /* *INDENT-ON* */

    size_t j, next;

    for (j = next = 0; j < TABLESIZE(table); j++) {
	if (c != 0) {
	    if (table[j].name[0] == c) {
		bputc(c);
		table[j].flag = TRUE;
		break;
	    }
	} else if (table[j].flag) {
	    bprintf("%s '%c'%s %s",
		    next ? "," : "notes:", table[j].name[0],
		    next ? "" : " is", table[j].name);
	    next++;
	    table[j].flag = 0;
	}
    }
    if (next)
	bputc('\n');
}
#define	MakeNote(c)	footnote(c)
#define	ShowNotes()	footnote(0)
#else
#define	MakeNote(c)	bputc(c)
#define	ShowNotes()
#endif /* !SMALLER */

/*
 * This routine rebuilds the text in the buffer that holds the buffer list.  It
 * is called by the list buffers command.  Return TRUE if everything works.
 * Return FALSE if there is an error (if there is no memory).  The variable
 * 'show_all' (set if the command had a repeat-count) indicates whether
 * hidden buffers should be on the list.
 */
/* ARGSUSED */
static void
makebufflist(int unused GCC_UNUSED, void *dummy GCC_UNUSED)
{
    BUFFER *bp;
    LINE *curlp;		/* entry corresponding to buffer-list */
    int nbuf = 0;		/* no. of buffers */
    int this_or_that;
    char temp[NFILEN];

    curlp = NULL;
    bfl_num_1st = -1;
    bfl_num_end = -1;
    bfl_buf_1st = -1;
    bfl_buf_end = -1;

    if (this_bp == NULL)
	this_bp = curbp;
    if (this_bp == that_bp)
	that_bp = find_alt();

    bprintf("      %7s %*s %s\n", "Size", NBUFN - 1, "Buffer name",
	    "Contents");
    bpadc(' ', 6);
    bpadc('-', 7);
    bputc(' ');
    bpadc('-', NBUFN - 1);
    bputc(' ');
    bpadc('-', term.cols - DOT.o);
    bputc('\n');

    /* output the list of buffers */
    for_each_buffer(bp) {
	/* skip those buffers which don't get updated when changed */
#if OPT_UPBUFF
	if (!update_on_chg(bp)) {
	    continue;
	}
#endif

	/* output status flag (e.g., has the file been read in?) */
	buffer_flags(temp, bp);
	if (*temp != EOS)
	    MakeNote(*temp);
	else
	    bputc(' ');

	this_or_that = (bp == this_bp)
	    ? EXPC_THIS
	    : (bp == that_bp)
	    ? EXPC_THAT
	    : ' ';

	if (b_is_temporary(bp)) {
	    bprintf("   %c ", this_or_that);
	} else {
	    char bufnum[NSTRING];

	    bfl_num_1st = DOT.o;
	    sprintf(bufnum, "%3d", nbuf++);
	    bputsn_xcolor(bufnum, -1, XCOLOR_NUMBER);
	    bfl_num_end = DOT.o;
	    bprintf("%c ", this_or_that);
	}

	(void) bsizes(bp);
	bprintf("%7lu ", bp->b_bytecount);

	bfl_buf_1st = DOT.o;
	bprintf("%.*s ", NBUFN - 1, bp->b_bname);
	bfl_buf_end = DOT.o;
	{
	    char *p;

	    if ((p = bp->b_fname) != NULL)
		p = shorten_path(vl_strncpy(temp, p, sizeof(temp)), TRUE);

	    if (p != NULL)
		bprintf("%s", p);
	}
	bprintf("\n");
	if (bp == curbp)
	    curlp = lback(lback(buf_head(curbp)));
    }
    ShowNotes();
    bprintf("             %*s %s", NBUFN - 1, "Current dir:",
	    current_directory(FALSE));

    /* show the actual size of the buffer-list */
    if (curlp != NULL) {
	(void) bsizes(curbp);
	(void) lsprintf(temp, "%7lu", curbp->b_bytecount);
	(void) memcpy(lvalue(curlp) + 6, temp, strlen(temp));
    }
}

#if OPT_UPBUFF
/*
 * (Re)compute the contents of the buffer-list.  Use the flag 'updating_list'
 * as a semaphore to avoid adjusting the last used/created indices while
 * cycling over the list of buffers.
 */
/*ARGSUSED*/
static int
show_BufferList(BUFFER *bp GCC_UNUSED)
{
    int status;
    if ((status = ((!updating_list++) != 0)) != FALSE) {
	this_bp = curbp;
	that_bp = find_alt();
	status = liststuff(BUFFERLIST_BufName, FALSE,
			   makebufflist, 0, (void *) 0);
    }
    updating_list--;
    return status;
}

/*
 * If the list-buffers window is visible, update it after operations that
 * would modify the list.
 */
void
updatelistbuffers(void)
{
    show_mark_is_set(0);
    update_scratch(BUFFERLIST_BufName, show_BufferList);
}

/* mark a scratch/temporary buffer for update */
void
update_scratch(const char *name, UpBuffFunc func)
{
    BUFFER *bp = find_b_name(name);

    if (valid_buffer(bp)) {
	bp->b_upbuff = func;
	b_set_obsolete(bp);
    }
}
#endif /* OPT_UPBUFF */

/* ARGSUSED */
int
listbuffers(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int status;
#if OPT_UPBUFF

    show_all = f;		/* save this to use in automatic updating */
    if (find_BufferList() != NULL) {
	updatelistbuffers();
	return TRUE;
    }
    this_bp = NULL;
    that_bp = curbp;
    status = liststuff(BUFFERLIST_BufName, FALSE,
		       makebufflist, 0, (void *) 0);
    b_clr_obsolete(curbp);
#else
    show_all = f;
    this_bp = 0;
    that_bp = curbp;
    status = liststuff(BUFFERLIST_BufName, FALSE,
		       makebufflist, 0, (void *) 0);
#endif
    return status;
}

/*
 * This is ": args"
 */
int
vl_set_args(int f, int n)
{
    BUFFER *bp;

    if (end_named_cmd()) {
	return listbuffers(f, n);
    }
    if (any_changed_buf(&bp) != 0) {
	mlforce("Buffer %s is modified.  Use :args! to override",
		bp->b_bname);
	return FALSE;
    }
    return set_files_to_edit("Set arguments: ", FALSE);
}

/*
 * This is ": args!"
 */
int
vl_set_args2(int f, int n)
{
    if (end_named_cmd()) {
	return listbuffers(f, n);
    }
    return set_files_to_edit("Set arguments: ", FALSE);
}

/*
 * The argument "text" points to a string.  Append this line to the buffer.
 * Handcraft the EOL on the end.  Return TRUE if it worked and FALSE if you ran
 * out of room.
 */
int
addline(BUFFER *bp, const char *text, int len)
{
    int status = FALSE;

    if (add_line_at(bp, lback(buf_head(bp)), text, len) == TRUE) {
	/* If "." is at the end, move it to new line  */
	if (sameline(bp->b_dot, bp->b_line))
	    bp->b_dot.l = lback(buf_head(bp));
	status = TRUE;
    }

    return status;
}

/*
 * Add a LINE filled with the given text after the specified LINE.
 */
int
add_line_at(BUFFER *bp, LINE *prevp, const char *text, int len)
{
    LINE *newlp;
    LINE *nextp;
    LINE *lp;
    int ntext;
    int result = FALSE;

    if (len < 0) {
	if (text != NULL) {
	    result = add_line_at(bp, prevp, text, (int) strlen(text));
	} else {
	    result = add_line_at(bp, prevp, "", 0);
	}
    } else {
	nextp = lforw(prevp);
	ntext = len;
	newlp = lalloc(ntext, bp);
	if (newlp != NULL) {

	    lp = newlp;
	    if (ntext > 0)
		(void) memcpy(lvalue(lp), text, (size_t) ntext);

	    /* try to maintain byte/line counts? */
	    if (b_is_counted(bp)) {
		if (nextp == buf_head(bp)) {
		    set_local_b_val(bp, MDNEWLINE, TRUE);
		    bp->b_bytecount += (B_COUNT) (ntext + 1);
		    bp->b_linecount += 1;
#if !SMALLER			/* tradeoff between codesize & data */
		    lp->l_number = bp->b_linecount;
#endif
		} else
		    b_clr_counted(bp);
	    }

	    set_lforw(prevp, newlp);	/* link into the buffer */
	    set_lback(newlp, prevp);
	    set_lback(nextp, newlp);
	    set_lforw(newlp, nextp);

	    result = TRUE;
	}
    }
    return result;
}

/*
 * return lines of a named buffer in sequence, starting at DOT
 */
char *
next_buffer_line(const char *bname)
{
    WINDOW *wp = NULL;
    BUFFER *bp;
    B_COUNT blen;
    static TBUFF *lbuf;

    if ((bp = find_b_name(bname)) == NULL)
	return NULL;

    /* if the buffer is in a window, the buffer DOT
     * is the wrong one to use.  use the first window's DOT.
     */
    if (bp->b_nwnd != 0) {
	for_each_visible_window(wp) {
	    if (wp->w_bufp == bp) {
		bp->b_dot = wp->w_dot;
		break;
	    }
	}
    }

    /* out of text? */
    if (is_header_line(bp->b_dot, bp))
	return NULL;

    /* how much left on the current line? */
    blen = (B_COUNT) llength(bp->b_dot.l);
    if (blen > (B_COUNT) bp->b_dot.o)
	blen -= (B_COUNT) bp->b_dot.o;
    else
	blen = 0;

    tb_init(&lbuf, EOS);
    tb_bappend(&lbuf,
	       lvalue(bp->b_dot.l) + bp->b_dot.o,
	       (size_t) blen);
    tb_append(&lbuf, EOS);

    /* move forward, for next time */
    bp->b_dot.l = lforw(bp->b_dot.l);
    bp->b_dot.o = 0;

    /* again, if it's in a window, put the new DOT back where
     * we got it.
     */
    if (bp->b_nwnd != 0 && wp) {
	wp->w_dot = bp->b_dot;
	wp->w_flag |= WFMOVE;
    }

    return (tb_values(lbuf));
}

/*
 * Look through the list of buffers.  Return TRUE if there are any changed
 * buffers.  Buffers that hold magic internal stuff are not considered; who
 * cares if the list of buffer names is hacked.  Return FALSE if no buffers
 * have been changed.  Return a pointer to the first changed buffer, in case
 * there's only one -- it's useful info then.
 */
int
any_changed_buf(BUFFER **bpp)
{
    BUFFER *bp;
    int cnt = 0;
    if (bpp)
	*bpp = NULL;

    for_each_buffer(bp) {
	if (!b_is_invisible(bp) && b_is_changed(bp)) {
	    if (bpp && !*bpp)
		*bpp = bp;
	    cnt++;
	}
    }
    return (cnt);
}

/* similar to above, for buffers that have not been visited */
int
any_unread_buf(BUFFER **bpp)
{
    BUFFER *bp;
    int cnt = 0;
    if (bpp)
	*bpp = NULL;

    for_each_buffer(bp) {
	if (!b_is_invisible(bp) && !bp->b_active) {
	    if (bpp && !*bpp)
		*bpp = bp;
	    cnt++;
	}
    }
    return (cnt);
}

/*
 * Copies string to a buffer-name, trimming trailing blanks for consistency.
 */
void
set_bname(BUFFER *bp, const char *name)
{
    int j, k;
    char *d;

    (void) strncpy0(bp->b_bname, name, (size_t) NBUFN);

    d = bp->b_bname;
    for (j = 0, k = -1; d[j]; j++) {
	if (!isSpace(d[j]))
	    k = -1;
	else if (k < 0)
	    k = j;
    }
    if (k >= 0)
	d[k] = EOS;
}

/*
 * Look for a buffer-name in the buffer-list.  This assumes that the argument
 * is null-terminated.  If the length is longer than will fit in a b_bname
 * field, then it is probably a filename.
 */
BUFFER *
find_b_name(const char *bname)
{
    BUFFER *bp;
    BUFFER temp;

    set_bname(&temp, bname);	/* make a canonical buffer-name */

    for_each_buffer(bp) {
	if (eql_bname(bp, temp.b_bname))
	    return bp;
    }
    return NULL;
}

/*
 * Find a buffer, by name.  Return a pointer to the BUFFER structure associated
 * with it.  If the buffer is not found, create it.  The "bflag" is the
 * settings for the flags in in buffer.
 */
BUFFER *
bfind(const char *bname, UINT bflag)
{
    BUFFER *bp;
    LINE *lp;
    BUFFER *lastb = NULL;	/* buffer to insert after */
    BUFFER *bp2;

    for_each_buffer(bp) {
	if (eql_bname(bp, bname))
	    return (bp);
	lastb = bp;
    }

    TRACE((T_CALLED "bfind(%s, %u)\n", bname, bflag));

    beginDisplay();

    /* set everything to 0's unless we want nonzero */
    if ((bp = typecalloc(BUFFER)) == NULL) {
	(void) no_memory("BUFFER");
    } else {
	/* set this first, to make it simple to trace */
	set_bname(bp, bname);

	lp = lalloc(0, bp);
	if (lp == NULL) {
	    FreeAndNull(bp);
	    (void) no_memory("BUFFER head");
	} else {

	    /* and set up the other buffer fields */
	    bp->b_values = global_b_values;
	    bp->b_wtraits.w_vals = global_w_values;
	    bp->b_active = FALSE;
	    bp->b_dot.l = lp;
	    bp->b_dot.o = 0;
	    bp->b_wline = bp->b_dot;
	    bp->b_line = bp->b_dot;
#if WINMARK
	    bp->b_mark = nullmark;
#endif
	    bp->b_lastdot = nullmark;
#if OPT_VIDEO_ATTRS
#endif
#if OPT_CURTOKENS
	    set_buf_fname_expr(bp);
#endif
	    bp->b_flag = bflag;
	    bp->b_acount = (short) b_val(bp, VAL_ASAVECNT);
	    bp->b_fname = NULL;
	    ch_fname(bp, "");
	    fileuid_invalidate(bp);
#if OPT_ENCRYPT
	    if (!b_is_temporary(bp)
		&& cryptkey != NULL && *cryptkey != EOS) {
		(void) vl_strncpy(bp->b_cryptkey, cryptkey, sizeof(bp->b_cryptkey));
		set_local_b_val(bp, MDCRYPT, TRUE);
	    } else
		bp->b_cryptkey[0] = EOS;
#endif
	    bp->b_udstks[0] = bp->b_udstks[1] = NULL;
	    bp->b_ulinep = NULL;
	    bp->b_udtail = NULL;
	    bp->b_udlastsep = NULL;

	    b_set_counted(bp);	/* buffer is empty */
	    set_lforw(lp, lp);
	    set_lback(lp, lp);

	    /* append at the end */
	    if (lastb)
		lastb->b_bufp = bp;
	    else
		bheadp = bp;
	    bp->b_bufp = NULL;
	    bp->b_created = countBuffers();

	    for_each_buffer(bp2)
		bp2->b_last_used += 1;
	    bp->b_last_used = 1;

#if OPT_PERL || OPT_TCL
	    bp->b_api_private = NULL;
#endif
	    set_record_sep(bp, (RECORD_SEP) global_b_val(VAL_RECORD_SEP));
	}
    }
    endofDisplay();
    returnPtr(bp);
}

/*
 * Given a filename, set up a buffer pointer to correspond
 */
BUFFER *
make_bp(const char *fname, UINT flags)
{
    BUFFER *bp;
    char bname[NBUFN];

    makename(bname, fname);
    unqname(bname);

    if (valid_buffer(bp = bfind(bname, flags)))
	ch_fname(bp, fname);
    return bp;
}

/*
 * Create a buffer to display the output of various utility commands, e.g.,
 * [Output].
 */
BUFFER *
make_ro_bp(const char *bname, UINT flags)
{
    BUFFER *bp;

    if (valid_buffer(bp = bfind(bname, flags))) {
	b_set_invisible(bp);
	b_clr_scratch(bp);	/* make it nonvolatile */
	bp->b_active = TRUE;
	set_rdonly(bp, non_filename(), MDVIEW);
    }
    return bp;
}

/*
 * This routine blows away all of the text in a buffer.  If the buffer is
 * marked as changed then we ask if it is ok to blow it away; this is to save
 * the user the grief of losing text.  The window chain is nearly always wrong
 * if this gets called; the caller must arrange for the updates that are
 * required.  Return TRUE if everything looks good.
 */
int
bclear(BUFFER *bp)
{
    LINE *lp;

    if (!b_is_temporary(bp)	/* Not invisible or scratch */
	&&b_is_changed(bp)) {	/* Something changed    */
	char ques[30 + NBUFN];

	(void) vl_strncat(vl_strncpy(ques,
				     "Discard changes to ",
				     sizeof(ques)),
			  bp->b_bname,
			  sizeof(ques));
	if (mlyesno(ques) != TRUE)
	    return FALSE;
    }
#if OPT_UPBUFF
    if (bp->b_rmbuff != NULL)
	(bp->b_rmbuff) (bp);
#endif
    b_clr_changed(bp);		/* Not changed          */

    beginDisplay();
    freeundostacks(bp, TRUE);	/* do this before removing lines */
    FreeAndNull(bp->b_nmmarks);	/* free the named marks */
#if OPT_SELECTIONS
    free_attribs(bp);
#endif

    while ((lp = lforw(buf_head(bp))) != buf_head(bp)) {
	lremove2(bp, lp);
    }

    if (bp->b_ulinep != NULL) {
	lfree(bp->b_ulinep, bp);
	bp->b_ulinep = NULL;
    }
    FreeAndNull(bp->b_ltext);
    bp->b_ltext_end = NULL;

    FreeAndNull(bp->b_LINEs);
    bp->b_LINEs_end = NULL;

    bp->b_freeLINEs = NULL;

    bp->b_dot = bp->b_line;	/* Fix "."              */
#if WINMARK
    bp->b_mark = nullmark;	/* Invalidate "mark"    */
#endif
    bp->b_lastdot = nullmark;	/* Invalidate "mark"    */

#if COMPLETE_FILES || COMPLETE_DIRS
    FreeAndNull(bp->b_index_list);
    bp->b_index_size = 0;
    bp->b_index_counter = 0;
#endif

    b_set_counted(bp);
    bp->b_bytecount = 0;
    bp->b_linecount = 0;

    free_local_vals(b_valnames, global_b_values.bv, bp->b_values.bv);
    endofDisplay();

    return (TRUE);
}

/*
 * Update the counts for the # of bytes and lines, to use in various display
 * commands.
 */
int
bsizes(BUFFER *bp)
{
    int code = FALSE;

    if (valid_buffer(curbp)
	&& !b_is_counted(bp)) {
	LINE *lp;		/* current line */
	B_COUNT ending = (B_COUNT) len_record_sep(bp);
	B_COUNT numchars = 0;	/* # of chars in file */
	L_NUM numlines = 0;	/* # of lines in file */

	/* count chars and lines */
	for_each_line(lp, bp) {
	    ++numlines;
	    numchars += ((B_COUNT) llength(lp) + ending);
#if !SMALLER			/* tradeoff between codesize & data */
	    lp->l_number = numlines;
#endif
	}
	if (!b_val(bp, MDNEWLINE)) {
	    if (numchars > ending)
		numchars -= ending;
	    else
		numchars = 0;
	}

	bp->b_bytecount = numchars;
	bp->b_linecount = numlines;

	TRACE(("bsizes %s %s %d lines, %lu bytes\n",
	       bp->b_bname,
	       b_val(bp, MDDOS) ? "dos" : "unix",
	       numlines,
	       (unsigned long) numchars));

	b_set_counted(bp);
	code = TRUE;
    }
    return code;
}

/*
 * Mark a buffer 'changed'
 */
void
chg_buff(BUFFER *bp, unsigned flag)
{
    WINDOW *wp;

    if (is_delinked_bp(bp))
	return;

    b_clr_counted(bp);
    b_match_attrs_dirty(bp);
    if (bp->b_nwnd != 1)	/* Ensure hard.             */
	flag |= WFHARD;
    if (!b_is_changed(bp)) {	/* First change, so     */
	flag |= WFMODE;		/* update mode lines.   */
	b_set_changed(bp);
    }
    b_set_recentlychanged(bp);
#if OPT_UPBUFF
    if (update_on_chg(bp))
	updatelistbuffers();
#endif
    for_each_visible_window(wp) {
	if (wp->w_bufp == bp) {
	    wp->w_flag |= (USHORT) flag;
#ifdef WMDLINEWRAP
	    /* The change may affect the line-height displayed
	     * on the screen.  Assume the worst-case.
	     */
	    if ((flag & WFEDIT) && w_val(wp, WMDLINEWRAP))
		wp->w_flag |= WFHARD;
#endif
	}
    }
}

/*
 * Mark a buffer 'unchanged'
 */
void
unchg_buff(BUFFER *bp, unsigned flag)
{
    WINDOW *wp;

    if (b_is_changed(bp)) {
	if (bp->b_nwnd != 1)	/* Ensure hard.             */
	    flag |= WFHARD;
	flag |= WFMODE;		/* update mode lines.   */
	b_clr_changed(bp);
	b_clr_counted(bp);
	b_set_recentlychanged(bp);
	b_match_attrs_dirty(bp);

	for_each_visible_window(wp) {
	    if (wp->w_bufp == bp)
		wp->w_flag |= (USHORT) flag;
	}
#if OPT_UPBUFF
	if (update_on_chg(bp))
	    updatelistbuffers();
#endif
    }
}

/* unmark the current buffers change flag */
/* ARGSUSED */
int
unmark(int f GCC_UNUSED, int n GCC_UNUSED)
{
    unchg_buff(curbp, 0);
    return (TRUE);
}

/* write all _changed_ buffers */
/* if you get the urge to tinker in here, bear in mind that you need
   to test:  :ww, 1:ww, :wwq, 1:wwq, all with 1 buffer, both modified and
   unmodified, with 2 buffers (0, 1, or both modified), and with errors,
   like either buffer not writeable.  oh yes -- you also need to turn
   on "autowrite", and try ^Z and :!cmd.

   by default, we should have scrolling messages, a pressreturn call, and
   a screen update.

   any numeric argument will suppress the pressreturn call, and therefore
   also the scrolling of the messages and screen update.

   if you're leaving (quitting, or suspending to the shell, you want the
   scrolling messages, but no press-return, and no update.

   if there's a failure, you want a pressreturn, and an update, and you
   need to return non-TRUE so you won't try to quit.  we also switch to
   the buffer that failed in that case.
*/
int
writeallchanged(int f, int n)
{
    return writeall(f, n, !f, FALSE, FALSE, FALSE);
}

int
writeallbuffers(int f, int n)
{
    return writeall(f, n, !f, FALSE, FALSE, TRUE);
}

int
writeall(int f, int n, int promptuser, int leaving, int autowriting, int all)
{
    BUFFER *bp;			/* scanning pointer to buffers */
    BUFFER *nbp;
    BUFFER *oldbp;		/* original current buffer */
    int status = TRUE;
    int count = 0;
    int failure = FALSE;
    int dirtymsgline = FALSE;

    TRACE((T_CALLED
	   "writeall(f=%d, n=%d, promptuser=%d, leaving=%d, autowriting=%d, all=%d)\n",
	   f, n, promptuser, leaving, autowriting, all));

    beginDisplay();
    oldbp = curbp;		/* save in case we fail */

    /*
     * We cannot use for_each_buffer() for this loop, because make_current()
     * will temporarily change the order of the linked list of buffers, putting
     * its argument at the beginning of the list unless autobuffer is set false
     * (in other words, there's the possibility of an infinite loop).  We can't
     * temporarily change autobuffer's setting either, since that changes the
     * list order.  Hence the use of 'nbp'.
     */
    for (bp = bheadp; bp != NULL; bp = nbp) {
	nbp = bp->b_bufp;
	if (!(bp->b_active))
	    /* we don't have it anyway - ignore */
	    continue;
	if (autowriting && !b_val(bp, MDAUTOWRITE))
	    continue;
	if (b_val(bp, MDREADONLY) && (n != SPECIAL_BANG_ARG))
	    /* ignore read-only buffer */
	    continue;
	if (((all && !is_internalname(bp->b_fname))
	     || b_is_changed(bp))
	    && !b_is_invisible(bp)) {
	    make_current(bp);	/* do this so filesave() works */
	    if (dirtymsgline && (promptuser || leaving)) {
		mlforce("\n");
	    }
	    status = filesave(f, n);
	    dirtymsgline = TRUE;
	    failure = (status != TRUE);
	    if (failure) {
#define SAVE_FAILED_FMT "[Save of %s failed]"

		int outlen;
		char tmp[NFILEN];

		mlforce("\n");
		outlen = ((term.cols - 1) -
			  (int) (sizeof(SAVE_FAILED_FMT) - 3));
		mlforce(SAVE_FAILED_FMT,
			path_trunc(is_internalname(bp->b_fname)
				   ? bp->b_bname
				   : bp->b_fname,
				   outlen,
				   tmp,
				   (int) sizeof(tmp)));
		/* dirtymsgline still TRUE */
		break;
#undef SAVE_FAILED_FMT
	    }
	    count++;
	}
    }
    endofDisplay();

    /* shortcut out */
    if (autowriting && !failure && !count)
	returnCode(TRUE);

    /* do we want a press-return message?  */
    if (failure || ((promptuser && !leaving) && count)) {
	if (dirtymsgline)
	    mlforce("\n");
	dirtymsgline = FALSE;
	pressreturn();
    }

    /* get our old buffer back */
    make_current(oldbp);

    /* and then switch to the failed buffer */
    if (failure && !insertmode /* can happen from ^Z and autowrite */ )
	swbuffer(bp);

    /* if we asked "press-return", then we certainly need an update */
    if (failure || ((promptuser && !leaving) && count)) {
	sgarbf = TRUE;
	(void) update(TRUE);
    } else if (dirtymsgline && (promptuser || leaving)) {
	mlforce("\n");
    }

    /* final message -- appears on the message line after the update,
       or as the last line before the post-quit prompt */
    if (count)
	mlforce("[Wrote %d buffer%s]", count, PLURAL(count));
    else
	mlforce("[No buffers written]");

    if (dirtymsgline)
	sgarbf = TRUE;

    returnCode(status);
}

#if OPT_TITLE
void
set_editor_title(void)
{
    static TBUFF *title;
    const char *format;

    if (auto_set_title) {

	/*
	 * We don't use "%b" for current buffer since the current
	 * window may not correspond, depending on how we're invoked.
	 */
	if (tb_length(title_format) != 0)
	    format = tb_values(title_format);
	else
	    format = global_g_val(GMDSWAPTITLE)
		? "%{$cbufname} - %{$progname}"
		: "%{$progname} - %{$cbufname}";
	special_formatter(&title, format, curwp);
	tb_copy(&request_title, title);
    }
}
#endif

/* For memory-leak testing (only!), releases all buffer storage. */
#if NO_LEAKS
static BUFFER *
zap_buffer(BUFFER *bp)
{
    TRACE(("zap_buffer(%s) %p\n", bp->b_bname, (void *) bp));
    if (bp != NULL) {
	b_clr_changed(bp);	/* discard any changes */
	bclear(bp);
	FreeBuffer(bp);
    }
    return NULL;
}

void
bp_leaks(void)
{
    BUFFER *bp;

    TRACE((T_CALLED "bp_leaks()\n"));
    bminip = zap_buffer(bminip);
    btempp = zap_buffer(btempp);
    while ((bp = bheadp) != NULL) {
	(void) zap_buffer(bp);
    }
#if OPT_MODELINE
    mls_regfree(-1);
#endif
    returnVoid();
}
#endif
