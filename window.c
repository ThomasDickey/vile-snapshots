/*
 * Window management. Some of the functions are internal, and some are
 * attached to keys that the user actually types.
 *
 * $Id: window.c,v 1.137 2025/01/26 17:08:26 tom Exp $
 */

#include	"estruct.h"
#include	"edef.h"

#if OPT_PERL || OPT_TCL
/* Window id to assign to w_id field for next visible window allocated */
static WIN_ID w_id_next = 1;

/* Fake windows are given a window id of 0 */
#define FAKE_WINDOW_ID 0
#endif

/*
 * Unlink the given window-pointer from the list
 */
static void
unlink_window(WINDOW *thewp)
{
    WINDOW *p, *q;

    for (p = wheadp, q = NULL; p != NULL; q = p, p = p->w_wndp)
	if (p == thewp) {
	    if (q != NULL)
		q->w_wndp = p->w_wndp;
	    else
		wheadp = p->w_wndp;
	    break;
	}
}

static WINDOW *
alloc_WINDOW(void)
{
    WINDOW *wp;

    beginDisplay();
    if ((wp = typecalloc(WINDOW)) == NULL) {
	(void) no_memory("WINDOW");
    }
    endofDisplay();
    return wp;
}

static void
free_WINDOW(WINDOW *wp)
{
    if (wp != NULL) {
	beginDisplay();
	if (wp == wminip)
	    wminip = NULL;
	free(wp);
	endofDisplay();
    }
}

/*
 * Set the current window (and associated current buffer).
 */
int
set_curwp(WINDOW *wp)
{
    if (wp == NULL)
	return (FALSE);
    if (wp == curwp)
	return (TRUE);
    curwp = wp;
#if !WINMARK
    /* FIXME: this wouldn't be necessary if MK were stored per-buffer */
    MK = nullmark;
#endif
    make_current(curwp->w_bufp);
    upmode();
    updatelistbuffers();
    return (TRUE);
}

/*
 * Adjust a LINE pointer forward by the given number of screen-rows, limited by
 * the end of the buffer.
 */
static LINE *
adjust_forw(WINDOW *wp, LINE *lp, int n)
{
    int i;
    LINE *dlp;
    for (i = n; i > 0 && (lp != win_head(wp));) {
	if ((i -= line_height(wp, lp)) < 0)
	    break;
	dlp = lforw(lp);
	if (dlp == win_head(wp))
	    break;
	lp = dlp;
    }
    return lp;
}

/*
 * Adjust a LINE pointer backward by the given number of screen-rows, limited by
 * the end of the buffer.
 */
static LINE *
adjust_back(WINDOW *wp, LINE *lp, int n)
{
    int i;
    LINE *dlp;
    for (i = n; i > 0 && (lp != win_head(wp));) {
	if ((i -= line_height(wp, lp)) < 0)
	    break;
	dlp = lback(lp);
	if (dlp == win_head(wp))
	    break;
	lp = dlp;
    }
    return lp;
}

/*
 * Reposition dot's line to line "n" of the window. If the argument is
 * positive, it is that line. If it is negative it is that line from the
 * bottom. If it is 0 the window is centered around dot (this is what
 * the standard redisplay code does). Defaults to 0.
 */
int
reposition(int f, int n)
{
    if (f) {
	long an;
	/* clamp the value at the size of the window */
	an = absol((long) n);
	if (an > curwp->w_ntrows)
	    curwp->w_force = (int) (curwp->w_ntrows * (n / an));
	else
	    curwp->w_force = n;
	curwp->w_flag |= WFFORCE;
    }
    return update(TRUE);
}

/*
 * Refresh the screen. With no argument, it just does the refresh. With an
 * argument it recenters "." in the current window.
 */
/* ARGSUSED */
int
vile_refresh(int f, int n GCC_UNUSED)
{
    if (f == FALSE) {
	sgarbf = TRUE;
    } else {
	curwp->w_force = 0;	/* Center dot. */
	curwp->w_flag |= WFFORCE;
    }

    return (TRUE);
}

/*
 * The command makes the next window (next => down the screen) the current
 * window. There are no real errors, although the command does nothing if
 * there is only 1 window on the screen.
 *
 * with an argument this command finds the <n>th window from the top
 */
int
nextwind(int f, int n)
{
    WINDOW *wp;
    int nwindows;		/* total number of windows */

    if (f) {

	/* first count the # of windows */
	nwindows = 0;
	for_each_visible_window(wp)
	    nwindows++;

	/* if the argument is negative, it is the nth window
	   from the bottom of the screen                        */
	if (n < 0)
	    n = nwindows + n + 1;

	/* if an argument, give them that window from the top */
	if (n > 0 && n <= nwindows) {
	    wp = wheadp;
	    while (--n != 0) {
		if (wp == NULL) {
		    mlforce("[Window number out of range]");
		    return (FALSE);
		}
		wp = wp->w_wndp;
	    }
	} else {
	    mlforce("[Window number out of range]");
	    return (FALSE);
	}
    } else {
	if ((wp = curwp->w_wndp) == NULL)
	    wp = wheadp;
    }
    return set_curwp(wp);
}

int
poswind(int f, int n)
{
    int c;
    int row;
    int s;

    n = need_a_count(f, n, 1);

    if (clexec || isnamedcmd) {
	static char cbuf[20];
	if ((s = mlreply("Position window with cursor at:  ", cbuf,
			 20)) != TRUE)
	    return s;
	c = cbuf[0];
    } else {
	c = keystroke();
	if (ABORTED(c))
	    return ABORT;
    }

    if (strchr("+hHtT\r", c)) {
	row = n;
    } else if (strchr(".mM", c)) {
	row = 0;
    } else if (strchr("-lLbB\r", c)) {
	row = -n;
    } else {
	if (!(clexec || isnamedcmd))
	    kbd_alarm();
	return FALSE;
    }

    return (reposition(TRUE, row));
}

/*
 * This command makes the previous window (previous => up the screen) the
 * current window. There aren't any errors, although the command does not do a
 * lot if there is 1 window.
 */
int
prevwind(int f, int n)
{
    WINDOW *wp1;
    WINDOW *wp2;

    /* if we have an argument, we mean the nth window from the bottom */
    if (f)
	return (nextwind(f, -n));

    wp1 = wheadp;
    wp2 = curwp;

    if (wp1 == wp2)
	wp2 = NULL;

    while (wp1->w_wndp != wp2)
	wp1 = wp1->w_wndp;

    return set_curwp(wp1);
}

/*
 * This command moves the current window down by "arg" lines. Recompute the
 * top line in the window. The move up and move down code is almost completely
 * the same; most of the work has to do with reframing the window, and picking
 * a new dot. We share the code by having "move down" just be an interface to
 * "move up". Magic.
 */
int
mvdnwind(int f, int n)
{
    n = need_a_count(f, n, 1);
    return (mvupwind(TRUE, -n));
}

/*
 * Move the current window up by "arg" lines. Recompute the new top line of
 * the window. Look to see if "." is still on the screen. If it is, you win.
 * If it isn't, then move "." to center it in the new framing of the window
 * (this command does not really move "." (except as above); it moves the
 * frame).
 */
int
mvupwind(int f, int n)
{
    LINE *lp;
    int i;
    int was_n = n;

    lp = curwp->w_line.l;

    n = need_a_count(f, n, 1);

    if (n < 0)
	curwp->w_flag |= WFKILLS;
    else
	curwp->w_flag |= WFINS;

    if (n < 0) {
	while (n++ && lforw(lp) != buf_head(curbp))
	    lp = lforw(lp);
    } else {
	while (n-- && lback(lp) != buf_head(curbp))
	    lp = lback(lp);
    }

    curwp->w_line.l = lp;
    curwp->w_line.o = 0;
    curwp->w_flag |= WFHARD | WFMODE;

    /* is it still in the window */
    for (i = 0; i < curwp->w_ntrows; lp = lforw(lp)) {
	if ((i += line_height(curwp, lp)) > curwp->w_ntrows)
	    break;
	if (lp == DOT.l)
	    return (TRUE);
	if (lforw(lp) == buf_head(curbp))
	    break;
    }
    /*
     * now lp is either just past the window bottom, or it's the last
     * line of the file
     */

    /* preserve the current column */
    if (curgoal < 0)
	curgoal = getccol(FALSE);

    if (was_n < 0)
	DOT.l = curwp->w_line.l;
    else
	DOT.l = lback(lp);
    DOT.o = getgoal(DOT.l);
    return (TRUE);
}

int
mvdnnxtwind(int f, int n)
{
    int status;

    (void) nextwind(FALSE, 1);
    status = mvdnwind(f, n);
    (void) prevwind(FALSE, 1);
    return status;
}

int
mvupnxtwind(int f, int n)
{
    int status;

    (void) nextwind(FALSE, 1);
    status = mvupwind(f, n);
    (void) prevwind(FALSE, 1);
    return status;
}

static int
scroll_sideways(int f, int n)
{
    int original = w_val(curwp, WVAL_SIDEWAYS);

    if (!f) {
	int nominal = term.cols / 2;
	n = (n > 0) ? nominal : -nominal;
    }

    make_local_w_val(curwp, WVAL_SIDEWAYS);
    w_val(curwp, WVAL_SIDEWAYS) += n;

    if (w_val(curwp, WVAL_SIDEWAYS) < 0) {
	if (original == 0)
	    kbd_alarm();
	w_val(curwp, WVAL_SIDEWAYS) = 0;
    }

    if (original != w_val(curwp, WVAL_SIDEWAYS)) {
	TRACE(("scroll_sideways by %d to %d\n",
	       n, w_val(curwp, WVAL_SIDEWAYS)));
	curwp->w_flag |= WFHARD | WFMOVE | WFMODE;
    }

    return TRUE;
}

int
mvrightwind(int f, int n)
{
    return scroll_sideways(f, n);
}

int
mvleftwind(int f, int n)
{
    return scroll_sideways(f, -n);
}

/*
 * This command makes the current window the only window on the screen.
 * Try to set the framing so that "." does not have to move on the
 * display. Some care has to be taken to keep the values of dot and mark in
 * the buffer structures right if the destruction of a window makes a buffer
 * become undisplayed.
 */
/* ARGSUSED */
int
onlywind(int f GCC_UNUSED, int n GCC_UNUSED)
{
    WINDOW *wp;

    wp = wheadp;
    while (wp != NULL) {
	WINDOW *nwp;
	nwp = wp->w_wndp;
	if (wp != curwp) {
	    if (--wp->w_bufp->b_nwnd == 0)
		undispbuff(wp->w_bufp, wp);
	    unlink_window(wp);
	    free_WINDOW(wp);
	}
	wp = nwp;
    }
    wheadp = curwp;
    wheadp->w_wndp = NULL;

    curwp->w_line.l = adjust_back(curwp, curwp->w_line.l, curwp->w_toprow);
    curwp->w_line.o = 0;
    curwp->w_ntrows = term.rows - 2;
    curwp->w_toprow = 0;
    curwp->w_flag |= WFMODE | WFHARD | WFSBAR;
    curwp->w_split_hist = 0;
    return (TRUE);
}

/*
 * Delete the current window
 */

/* ARGSUSED */
int
delwind(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return delwp(curwp);
}

/*
 * We attach to each window structure another field (an unsigned long)
 * which I called w_split_history.  When we split a window, we shift this
 * field left by one bit.  The least significant bit for the upper window
 * is 0, the bottom window's lsb is set to 1.
 *
 * We examine the lsb when deleting windows to decide whether to give up
 * the space for the deleted window to the upper window or lower window.
 * If the lsb is 0, we give it to the lower window.  If it is 1 we give it
 * to the upper window.  If the lsb of the receiving window is different
 * from that of the window being deleted, this means that the two match and
 * so the history is shifted right by one bit for the receiving window.
 * Otherwise we leave the history alone.
 *							kev, 2/94
 *
 * Example:
 *
 * |    |       | 00    | 00    | 00    | 00
 * |    | 0     -       -       -       -
 * |    |       | 01    | 01    | 01    | 01
 * | 0 -->      -   --> -   --> -   --> -   --> |
 * |    |       |       | 10    |       |
 * |    | 1     | 1     -       | 1     |
 * |    |       |       | 11    |       |
 *
 * Full Split   Split   and     Kill    Kill
 * Screen               Again   Again   either  1
 *                              10 or
 *                              11
 */

int
delwp(WINDOW *thewp)
{
    WINDOW *wp;			/* window to receive deleted space */
    int visible = is_visible_window(thewp);

    /* if there is only one window, don't delete it */
    if (wheadp->w_wndp == NULL) {
	mlforce("[Cannot delete the only window]");
	return (FALSE);
    }

    /* find receiving window and give up our space */
    if (thewp->w_wndp != NULL
	&& (thewp == wheadp
	    || ((thewp->w_split_hist & 1) == 0)
	    || !visible)) {
	/* merge with next window down */
	wp = thewp->w_wndp;
	if (visible) {
	    wp->w_line.l = adjust_back(wp, wp->w_line.l,
				       thewp->w_ntrows + 1);
	    wp->w_line.o = 0;
	    wp->w_ntrows += thewp->w_ntrows + 1;
	    wp->w_toprow = thewp->w_toprow;
	    if (wp->w_split_hist & 1)
		wp->w_split_hist >>= 1;
	}
	if (thewp == wheadp) {
	    wheadp = wp;
	} else {
	    WINDOW *pwp = wheadp;
	    while (pwp->w_wndp != thewp)
		pwp = pwp->w_wndp;
	    pwp->w_wndp = wp;
	}
    } else {
	/* find window before thewp in linked list */
	wp = wheadp;
	while (wp->w_wndp != thewp)
	    wp = wp->w_wndp;
	/* add thewp's rows to the next window up */
	wp->w_ntrows += thewp->w_ntrows + 1;

	wp->w_wndp = thewp->w_wndp;	/* make their next window ours */
	if ((wp->w_split_hist & 1) == 0)
	    wp->w_split_hist >>= 1;
    }

    /* get rid of the current window */
    if (visible && --thewp->w_bufp->b_nwnd == 0)
	undispbuff(thewp->w_bufp, thewp);
    if (thewp == curwp) {
	curwp = wp;
	make_current(curwp->w_bufp);
    }
    free_WINDOW(thewp);
    if (visible) {
	upmode();
	wp->w_flag |= WFSBAR | WFHARD;
    }
    return (TRUE);
}

/*
 * Copy the window-traits struct, and adjust the embedded VAL struct so that
 * modes that are local remain so.
 */
void
copy_traits(W_TRAITS * dst, W_TRAITS * src)
{
    *dst = *src;
    copy_mvals(NUM_W_VALUES, dst->w_vals.wv, src->w_vals.wv);
}

W_VALUES *
save_window_modes(BUFFER *bp)
{
    W_VALUES *result = NULL;
    WINDOW *wp;
    UINT n;

    if (bp->b_nwnd != 0) {
	beginDisplay();
	result = typecallocn(W_VALUES, (size_t) bp->b_nwnd);
	if (result != NULL) {
	    n = 0;
	    for_each_window(wp) {
		if (wp->w_bufp == bp) {
		    copy_mvals(NUM_W_VALUES, result[n].wv, wp->w_values.wv);
		    if (++n >= bp->b_nwnd)
			break;
		}
	    }
	}
	endofDisplay();
    }
    return result;
}

void
restore_window_modes(BUFFER *bp, W_VALUES * saved)
{
    if (saved != NULL) {
	WINDOW *wp;
	UINT n = 0;

	beginDisplay();
	for_each_window(wp) {
	    if (wp->w_bufp == bp) {
		copy_mvals(NUM_W_VALUES, wp->w_values.wv, saved[n].wv);
		if (++n >= bp->b_nwnd)
		    break;
	    }
	}
	free(saved);
	endofDisplay();
    }
}

/*
 * Split the current window.  A window smaller than 3 lines cannot be split.
 * An argument of 1 forces the cursor into the upper window, an argument of two
 * forces the cursor to the lower window.  The only other error that is
 * possible is a "malloc" failure allocating the structure for the new window.
 */
static WINDOW *
splitw(int f, int n)
{
    WINDOW *wp;
    LINE *lp;
    int ntru;
    int ntrl;
    int ntrd;
    int pos = n;
    WINDOW *wp1;
    WINDOW *wp2;

    if (curwp->w_ntrows < MINWLNS) {
	mlforce("[Cannot split a %d line window]", curwp->w_ntrows);
	return NULL;
    }

    beginDisplay();
    /* set everything to 0's unless we want nonzero */
    if ((wp = alloc_WINDOW()) != NULL) {
	++curwp->w_bufp->b_nwnd;	/* Displayed twice.     */
	wp->w_bufp = curwp->w_bufp;
	copy_traits(&(wp->w_traits), &(curwp->w_traits));
	ntru = (curwp->w_ntrows - 1) / 2;	/* Upper size           */
	ntrl = (curwp->w_ntrows - 1) - ntru;	/* Lower size           */

	lp = curwp->w_line.l;
	ntrd = 0;
	if (f == FALSE) {
	    /* pop-up should go wherever dot is not */
	    while (lp != DOT.l) {
		ntrd += line_height(wp, lp);
		lp = lforw(lp);
	    }
	    /* ntrd is now the row containing dot */
	    if (ntrd <= ntru) {
		/* Old is upper window. */
		pos = 1;
		/* Adjust the top line if necessary */
		if (ntrd == ntru) {	/* Hit mode line.       */
		    if (ntrl > 1) {
			ntru++;
			ntrl--;
		    } else {
			curwp->w_line.l = lforw(curwp->w_line.l);
			curwp->w_line.o = 0;
		    }
		}
	    } else {
		/* Old is lower window  */
		pos = 2;
	    }
	}

	if (pos == 1) {
	    /* pop-up below */
	    curwp->w_ntrows = ntru;	/* new size */
	    /* insert new window after curwp in window list */
	    wp->w_wndp = curwp->w_wndp;
	    curwp->w_wndp = wp;
	    /* set new window's position and size */
	    wp->w_toprow = curwp->w_toprow + ntru + 1;
	    wp->w_ntrows = ntrl;
	    /* try to keep lower from reframing */
	    wp->w_line.l = adjust_forw(wp, wp->w_line.l, ntru + 1);
	    wp->w_line.o = 0;
	    wp->w_dot.l = wp->w_line.l;
	    wp->w_dot.o = 0;
	    /* update the split history */
	    curwp->w_split_hist <<= 1;
	    wp->w_split_hist = curwp->w_split_hist | 1;
	} else {
	    /* pop-up above  */
	    wp1 = NULL;
	    wp2 = wheadp;
	    while (wp2 != curwp) {
		wp1 = wp2;
		wp2 = wp2->w_wndp;
	    }
	    if (wp1 == NULL)
		wheadp = wp;
	    else
		wp1->w_wndp = wp;
	    wp->w_wndp = curwp;
	    wp->w_toprow = curwp->w_toprow;
	    wp->w_ntrows = ntru;
	    ++ntru;		/* Mode line.           */
	    curwp->w_toprow += ntru;
	    curwp->w_ntrows = ntrl;
	    wp->w_dot.l = wp->w_line.l;
	    /* move upper window dot to bottom line of upper */
	    wp->w_dot.l = adjust_forw(wp, wp->w_dot.l, ntru - 2);
	    wp->w_dot.o = 0;
	    /* adjust lower window topline */
	    curwp->w_line.l = adjust_forw(curwp, curwp->w_line.l, ntru);
	    curwp->w_line.o = 0;
	    /* update the split history */
	    wp->w_split_hist <<= 1;
	    curwp->w_split_hist = wp->w_split_hist | 1;
	}

#if 0
	/* ntrd is now the row containing dot */
	if (((f == FALSE) && (ntrd <= ntru)) || ((f == TRUE) && (n == 1))) {
	    /* Old is upper window. */
	    /* Adjust the top line if necessary */
	    if (ntrd == ntru) {	/* Hit mode line.       */
		if (ntrl > 1) {
		    ntru++;
		    ntrl--;
		} else {
		    curwp->w_line.l = lforw(curwp->w_line.l);
		    curwp->w_line.o = 0;
		}
	    }
	    curwp->w_ntrows = ntru;	/* new size */
	    /* insert new window after curwp in window list */
	    wp->w_wndp = curwp->w_wndp;
	    curwp->w_wndp = wp;
	    /* set new window's position and size */
	    wp->w_toprow = curwp->w_toprow + ntru + 1;
	    wp->w_ntrows = ntrl;
	    /* try to keep lower from reframing */
	    wp->w_line.l = adjust_forw(wp, wp->w_line.l, ntru + 1);
	    wp->w_line.o = 0;
	    wp->w_dot.l = wp->w_line.l;
	    wp->w_dot.o = 0;
	    /* update the split history */
	    curwp->w_split_hist <<= 1;
	    wp->w_split_hist = curwp->w_split_hist | 1;
	} else {
	    /* Old is lower window  */
	    wp1 = NULL;
	    wp2 = wheadp;
	    while (wp2 != curwp) {
		wp1 = wp2;
		wp2 = wp2->w_wndp;
	    }
	    if (wp1 == NULL)
		wheadp = wp;
	    else
		wp1->w_wndp = wp;
	    wp->w_wndp = curwp;
	    wp->w_toprow = curwp->w_toprow;
	    wp->w_ntrows = ntru;
	    ++ntru;		/* Mode line.           */
	    curwp->w_toprow += ntru;
	    curwp->w_ntrows = ntrl;
	    wp->w_dot.l = wp->w_line.l;
	    /* move upper window dot to bottom line of upper */
	    wp->w_dot.l = adjust_forw(wp, wp->w_dot.l, ntru - 2);
	    wp->w_dot.o = 0;
	    /* adjust lower window topline */
	    curwp->w_line.l = adjust_forw(curwp, curwp->w_line.l, ntru);
	    curwp->w_line.o = 0;
	    /* update the split history */
	    wp->w_split_hist <<= 1;
	    curwp->w_split_hist = wp->w_split_hist | 1;
	}
#endif

	curwp->w_flag |= WFMODE | WFHARD | WFSBAR;
	wp->w_flag |= WFMODE | WFHARD;

#if OPT_PERL || OPT_TCL
	wp->w_id = w_id_next;
	++w_id_next;
#endif
    }
    endofDisplay();
    return wp;
}

/* external callable version -- return int instead of (WINDOW *) */
int
splitwind(int f, int n)
{
    return (splitw(f, n)) ? TRUE : FALSE;
}

/*
 * Enlarge the current window. Find the window that loses space. Make sure it
 * is big enough. If so, hack the window descriptions, and ask redisplay to do
 * all the hard work. You don't just set "force reframe" because dot would
 * move.
 */
int
enlargewind(int f, int n)
{
    WINDOW *adjwp;

    if (n < 0)
	return (shrinkwind(f, -n));
    if (wheadp->w_wndp == NULL) {
	mlforce("[Only one window]");
	return (FALSE);
    }
    if ((adjwp = curwp->w_wndp) == NULL) {
	adjwp = wheadp;
	while ((adjwp != NULL) && (adjwp->w_wndp != curwp)) {
	    adjwp = adjwp->w_wndp;
	}
    }
    if (adjwp == NULL || adjwp->w_ntrows <= n) {
	mlforce("[Impossible change]");
	return (FALSE);
    }
    if (curwp->w_wndp == adjwp) {	/* Shrink below.        */
	adjwp->w_line.l = adjust_forw(adjwp, adjwp->w_line.l, n);
	adjwp->w_line.o = 0;
	adjwp->w_toprow += n;
    } else {			/* Shrink above.        */
	curwp->w_line.l = adjust_back(curwp, curwp->w_line.l, n);
	curwp->w_line.o = 0;
	curwp->w_toprow -= n;
    }
    curwp->w_ntrows += n;
    adjwp->w_ntrows -= n;
    curwp->w_flag |= WFMODE | WFHARD | WFINS | WFSBAR;
    adjwp->w_flag |= WFMODE | WFHARD | WFKILLS;
    return (TRUE);
}

/*
 * Shrink the current window. Find the window that gains space. Hack at the
 * window descriptions. Ask the redisplay to do all the hard work.
 */
int
shrinkwind(int f, int n)
{
    WINDOW *adjwp;

    if (n < 0)
	return (enlargewind(f, -n));
    if (wheadp->w_wndp == NULL) {
	mlforce("[Only one window]");
	return (FALSE);
    }
    if ((adjwp = curwp->w_wndp) == NULL) {
	adjwp = wheadp;
	while ((adjwp != NULL) && (adjwp->w_wndp != curwp)) {
	    adjwp = adjwp->w_wndp;
	}
    }
    if (adjwp == NULL || curwp->w_ntrows <= n) {
	mlforce("[Impossible change]");
	return (FALSE);
    }
    if (curwp->w_wndp == adjwp) {	/* Grow below.          */
	adjwp->w_line.l = adjust_back(adjwp, adjwp->w_line.l, n);
	adjwp->w_line.o = 0;
	adjwp->w_toprow -= n;
    } else {			/* Grow above.          */
	curwp->w_line.l = adjust_forw(curwp, curwp->w_line.l, n);
	curwp->w_line.o = 0;
	curwp->w_toprow += n;
    }
    curwp->w_ntrows -= n;
    adjwp->w_ntrows += n;
    curwp->w_flag |= WFMODE | WFHARD | WFKILLS | WFSBAR;
    adjwp->w_flag |= WFMODE | WFHARD | WFINS;
    return (TRUE);
}

/*	Resize the current window to the requested size	*/
int
resize(int f, int n)
{
    int clines;			/* current # of lines in window */

    /* must have a non-default argument, else ignore call */
    if (f == FALSE)
	return (TRUE);

    /* find out what to do */
    clines = curwp->w_ntrows;

    /* already the right size? */
    if (clines == n)
	return (TRUE);

    return (enlargewind(TRUE, n - clines));
}

/*
 * Pick a window for a pop-up. Split the screen if there is only one window.
 * Pick the uppermost window that isn't the current window. An LRU algorithm
 * might be better. Return a pointer, or NULL on error.
 */
WINDOW *
wpopup(void)
{
    WINDOW *wp;
    WINDOW *owp;
    WINDOW *biggest_wp;
    int force = FALSE;
    int pos = 0;
#if OPT_POPUPPOSITIONS
#if OPT_ENUM_MODES
    int gvalpopup_pos = global_g_val(GVAL_POPUP_POSITIONS);
#else
    int gvalpopup_pos = global_g_val(GMDPOPUP_POSITIONS);
#endif
#endif /* OPT_POPUPPOSITIONS */

    owp = curwp;
    wp = biggest_wp = wheadp;	/* Find window to split   */
    while (wp->w_wndp != NULL) {
	wp = wp->w_wndp;
	if (wp->w_ntrows > biggest_wp->w_ntrows)
	    biggest_wp = wp;
    }
    if (biggest_wp->w_ntrows >= MINWLNS) {
	curwp = biggest_wp;
#if OPT_POPUPPOSITIONS
	if (gvalpopup_pos == POPUP_POSITIONS_BOTTOM) {
	    force = TRUE;
	    pos = 1;
	} else if (gvalpopup_pos == POPUP_POSITIONS_TOP) {
	    force = TRUE;
	    pos = 2;
	}
#endif
	wp = splitw(force, pos);	/* according to popup-positions */
	curwp = owp;
    } else {
	/*  biggest_wp was too small  */
	wp = wheadp;		/* Find window to use   */
	while (wp != NULL && wp == curwp)	/* uppermost non-current window */
	    wp = wp->w_wndp;
	if (wp == NULL)
	    wp = wheadp;
    }
    return wp;
}

/*
 * Shrink or expand current window to the number of lines in the buffer.
 * If the buffer is too large, make the window as large as possible using
 * space from window closest in the split history.
 */

void
shrinkwrap(void)
{
    L_NUM nlines;

    if (wheadp->w_wndp == NULL)
	return;			/* only one window */

    nlines = vl_line_count(curwp->w_bufp);
    if (nlines <= 0)
	nlines = 1;

    if (nlines == curwp->w_ntrows)
	return;

    if ((curwp->w_split_hist & 1) == 0 || wheadp == curwp) {
	int nrows, snrows;
	/* give/steal from lower window */
	nrows = curwp->w_ntrows + curwp->w_wndp->w_ntrows - 1;
	/* don't take more than 3/4 of its rows */
	snrows = (nrows * 3) / 4;
	if (nlines > snrows)
	    nlines = snrows;
	resize(TRUE, nlines);
    } else {
	/* give/steal from upper window; need to find upper window */
	WINDOW *wp;
	WINDOW *savewp = curwp;
	int nrows, snrows;
	for (wp = wheadp;
	     wp->w_wndp != curwp && wp->w_wndp != NULL;
	     wp = wp->w_wndp) ;
	if (wp->w_wndp != NULL) {
	    curwp = wp;
	    nrows = curwp->w_ntrows + curwp->w_wndp->w_ntrows - 1;
	    /* don't take more than 3/4 of its rows */
	    snrows = (nrows * 3) / 4;
	    if (nlines > snrows)
		nlines = snrows;
	    resize(TRUE, nrows - nlines + 1);
	    curwp = savewp;
	}
    }
}

int
scrnextup(int f, int n)		/* scroll the next window up (back) a page */
{
    int status;

    (void) nextwind(FALSE, 1);
    status = backhpage(f, n);
    (void) prevwind(FALSE, 1);
    return status;
}

int
scrnextdw(int f, int n)		/* scroll the next window down (forward) a page */
{
    int status;

    (void) nextwind(FALSE, 1);
    status = forwhpage(f, n);
    (void) prevwind(FALSE, 1);
    return status;
}

#if ! SMALLER
/* ARGSUSED */
int
savewnd(int f GCC_UNUSED, int n GCC_UNUSED)	/* save ptr to current window */
{
    swindow = curwp;
    return (TRUE);
}

/* ARGSUSED */
int
restwnd(int f GCC_UNUSED, int n GCC_UNUSED)	/* restore the saved screen */
{
    WINDOW *wp;

    /* find the window */
    for_each_visible_window(wp) {
	if (wp == swindow)
	    return set_curwp(wp);
    }

    mlforce("[No such window exists]");
    return (FALSE);
}
#endif

/* change the screen height */
int
newlength(int f, int n)
{
    WINDOW *wp, *nextwp, *prevwp;

    if (f == FALSE) {
	mlforce("[No length given]");
	return FALSE;
    }

    if (term.rows == n)
	return (TRUE);

    /* validate */
    if (n < MINWLNS || n > term.maxrows) {
	mlforce("[Bad screen length]");
	return (FALSE);
    }

    if (!wheadp)
	return TRUE;

    /* screen getting bigger, grow the last window */
    if (n > term.rows) {

	/* find end of list */
	for (wp = wheadp; wp->w_wndp; wp = wp->w_wndp) ;

	wp->w_ntrows += n - term.rows;
	wp->w_flag |= WFHARD | WFMODE;

    } else {
	wp = wheadp;
	prevwp = wheadp;
	while (wp) {
	    /* if this one starts beyond new end of screen */
	    if (wp->w_toprow >= n - 2) {

		/* the previous window is now the last */
		prevwp->w_wndp = NULL;

		/* kill rest of windows */
		while (wp) {
		    /* last on-screen reference? */
		    if (--wp->w_bufp->b_nwnd == 0) {
			undispbuff(wp->w_bufp, wp);
		    }

		    /* possibly reset current window */
		    if (wp == curwp) {
			curwp = prevwp;
			make_current(curwp->w_bufp);
		    }
		    /* free this window */
		    nextwp = wp->w_wndp;
		    free_WINDOW(wp);
		    wp = nextwp;
		}

		/* all done */
		break;

	    } else {
		/* if only end of window is beyond new eos */
		if (mode_row(wp) >= n - 3) {
		    wp->w_ntrows = n - wp->w_toprow - 2;
		    wp->w_flag |= WFHARD | WFMODE;
		}
	    }

	    prevwp = wp;
	    wp = wp->w_wndp;
	}
    }

    /* trash screen */
    sgarbf = TRUE;

    term.rows = n;
    return (TRUE);
}

int
newwidth(int f, int n)		/* change width of displayed area */
{
    WINDOW *wp;

    if (f == FALSE) {
	mlforce("[Width required]");
	return FALSE;
    }

    /* validate */
    if (n < 3 || n > term.maxcols) {
	mlforce("[Bad screen width]");
	return (FALSE);
    }

    /* set the new width */
    term.cols = n;

    /* redraw all */
    for_each_visible_window(wp)
	wp->w_flag |= WFHARD | WFMOVE | WFMODE;

    /* trash screen */
    sgarbf = TRUE;

    return (TRUE);
}

#if OPT_EVAL
/* where on screen is current line? */
int
getlinerow(void)
{
    int row;
    LINE *lp;

    row = 1;
    for (lp = curwp->w_line.l; lp != DOT.l; lp = lforw(lp))
	row += line_height(curwp, lp);

    return (row);
}
#endif

void
init_window(WINDOW *wp, BUFFER *bp)
{
    if (valid_buffer(bp)) {
	wp->w_line.l = lforw(buf_head(bp));
	wp->w_line.o = 0;
	wp->w_dot.l = wp->w_line.l;
	wp->w_dot.o = wp->w_line.o;
    } else {
	wp->w_line = nullmark;
	wp->w_dot = nullmark;
    }
#if WINMARK
    wp->w_mark = nullmark;
#endif
    wp->w_lastdot = nullmark;
    wp->w_values = global_w_values;

#if OPT_CACHE_VCOL
    wp->w_traits.w_left_dot = nullmark;
    wp->w_traits.w_left_col = 0;
#endif

    wp->w_flag |= WFMODE | WFHARD;	/* Quite nasty.         */
}

void
clone_window(WINDOW *dst, WINDOW *src)
{
    dst->w_line = src->w_line;
    dst->w_dot = src->w_dot;
#if WINMARK
    dst->w_mark = src->w_mark;
#endif
    dst->w_lastdot = src->w_lastdot;
    dst->w_values = src->w_values;

    dst->w_flag |= WFMODE | WFHARD;	/* Quite nasty.         */
}

static WINDOW *
new_WINDOW(int screen)
{
    WINDOW *wp = typecalloc(WINDOW);

    if (wp == NULL) {
	fprintf(stderr, "Cannot allocate windows\n");
	ExitProgram(BADEXIT);
    } else {
	init_window(wp, (BUFFER *) 0);
	wp->w_ntrows = (screen
			? (term.rows - 2)	/* "-1" for mode line.  */
			: 1);	/* command-line         */
    }
    return wp;
}

/*
 * Initialize all of the windows.
 */
void
winit(int screen)
{
    WINDOW *wp;

    TRACE((T_CALLED "winit(%d)\n", screen));

    wp = new_WINDOW(screen);	/* First window         */
    wheadp = wp;
    curwp = wp;

#if OPT_PERL || OPT_TCL
    wp->w_id = w_id_next;
    ++w_id_next;
#endif

    if (screen) {
	(void) bsizes(bminip);	/* FIXME */
	TRACE(("winit delinking bminip, %d lines, %ld bytes\n",
	       bminip->b_linecount,
	       bminip->b_bytecount));
	(void) delink_bp(bminip);
#if OPT_MULTIBYTE
	(void) delink_bp(btempp);
#endif
    } else {
	/* create a dummy window, to hold buffers before display is active */
	wnullp = new_WINDOW(1);

	/* create the command-buffer */
	TRACE(("winit creating bminip & wminip\n"));
	wminip = wp;
	bminip = wp->w_bufp = bfind("", BFINVS);
	b_set_scratch(bminip);
	addline(bminip, "", 0);
	wminip->w_dot = bminip->b_dot;

	make_local_w_val(wminip, WMDNUMBER);
	set_w_val(wminip, WMDNUMBER, FALSE);

	make_local_w_val(wminip, WMDLIST);
	set_w_val(wminip, WMDLIST, TRUE);
#ifdef WMDLINEWRAP
	make_local_w_val(wminip, WMDLINEBREAK);
	set_w_val(wminip, WMDLINEBREAK, FALSE);
	make_local_w_val(wminip, WMDLINEWRAP);
	set_w_val(wminip, WMDLINEWRAP, FALSE);
#endif
#if OPT_MULTIBYTE
	btempp = bfind("\tUTF-8", BFINVS);
#endif
    }
    returnVoid();
}

#if OPT_SEL_YANK || OPT_PERL || OPT_COLOR || OPT_EVAL || OPT_DEBUGMACROS
/*
 * Allocate a fake window so that we can yank a selection even if the buffer
 * containing the selection is not attached to any window.
 *
 * curwp is set to the new fake window.  A pointer to the old curwp is returned
 * for a later call to pop_fake_win() which will restore curwp.
 */
WINDOW *
push_fake_win(BUFFER *bp)
{
    WINDOW *oldwp = NULL;
    WINDOW *wp;

    if (valid_buffer(bp) && wheadp != NULL) {
	if ((wp = alloc_WINDOW()) != NULL) {
	    oldwp = curwp;
	    curwp = wp;
	    curwp->w_bufp = bp;
	    if ((wp = bp2any_wp(bp)) == NULL)
		copy_traits(&(curwp->w_traits), &(bp->b_wtraits));
	    else
		copy_traits(&(curwp->w_traits), &(wp->w_traits));
	    curwp->w_flag = 0;
	    curwp->w_force = 0;
	    curwp->w_toprow = wheadp->w_toprow - 2;	/* should be negative */
	    curwp->w_ntrows = 1;
	    curwp->w_wndp = wheadp;
#if OPT_PERL || OPT_TCL
	    curwp->w_id = FAKE_WINDOW_ID;
#endif
	    wheadp = curwp;
	    curbp = curwp->w_bufp;
	}
    }
    return oldwp;
}

/*
 * kill top fake window allocated by alloc_fake_win;
 *
 * Set curwp to the oldwp parameter.  We may have saved curbp in oldbp, so try
 * to restore curbp to that value.  But we may not, so try restoring it to the
 * buffer associated with the new curwp.  In either case, check that the buffer
 * still exists in case the bracketed code clobbered the buffer.
 *
 * Return 0 if no fake window popped, else return the buffer pointer
 * of the popped window.
 *
 */
BUFFER *
pop_fake_win(WINDOW *oldwp, BUFFER *oldbp)
{
    WINDOW *wp;
    BUFFER *bp;

    curwp = oldwp;
    if (find_bp(oldbp) != NULL)
	curbp = oldbp;
    else if (find_bp(oldwp->w_bufp) != NULL)
	curbp = oldwp->w_bufp;

    wp = wheadp;
    if (wp->w_toprow >= 0)
	return NULL;		/* not a fake window */

    bp = wp->w_bufp;
    /* unlink and free the fake window */
    wheadp = wp->w_wndp;
    free_WINDOW(wp);
    return bp;
}

#endif

#if OPT_PERL
/* Find and return the window with the given window id.  Return NULL
   if not found */
WINDOW *
id2win(WIN_ID id)
{
    WINDOW *wp;
    for_each_visible_window(wp) {
	if (wp->w_id == id)
	    return wp;
    }
    return NULL;
}

/* Return the window id associated with the given window */
WIN_ID
win2id(WINDOW *wp)
{
    return wp->w_id;
}

/* Given an index N, return the window associated with that index.  I.e,
   index 0 returns the first visible window, index 1 returns the second,
   etc. */
WINDOW *
index2win(int idx)
{
    WINDOW *wp;
    for_each_visible_window(wp) {
	if (idx-- == 0)
	    return wp;
    }
    return NULL;
}

#endif

#if OPT_PERL || DISP_NTWIN

/* Given a window pointer, return the index corresponding to that
   window.  I.e, the first visible window corresponds to 0, the second
   to 1, etc. Return -1 if the window in question is not found. */
int
win2index(WINDOW *wp_to_find)
{
    WINDOW *wp;
    int idx = 0;
    for_each_visible_window(wp) {
	if (wp == wp_to_find)
	    return idx;
	idx++;
    }
    return -1;
}

#endif

/* For memory-leak testing (only!), releases all display storage. */
#if NO_LEAKS
void
wp_leaks(void)
{
    WINDOW *wp;

    beginDisplay();
    while ((wp = wheadp) != NULL) {
	wp = wp->w_wndp;
	free_WINDOW(wheadp);
	if (wp != wheadp)
	    wheadp = wp;
	else
	    break;
    }
    free_WINDOW(wminip);
    endofDisplay();
}
#endif
