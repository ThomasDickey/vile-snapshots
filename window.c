/*
 * Window management. Some of the functions are internal, and some are
 * attached to keys that the user actually types.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/window.c,v 1.68 1996/03/24 13:38:16 pgf Exp $
 *
 */

#include        "estruct.h"
#include	"edef.h"

/*
 * Unlink the given window-pointer from the list
 */
static void
unlink_window(WINDOW *thewp)
{
	register WINDOW *p, *q;

	for (p = wheadp, q = 0; p != 0; q = p, p = p->w_wndp)
		if (p == thewp) {
			if (q != 0)
				q->w_wndp = p->w_wndp;
			else
				wheadp = p->w_wndp;
			break;
		}
}

/*
 * Set the current window (and associated current buffer).
 */
int
set_curwp (WINDOW *wp)
{
	if (wp == curwp)
		return (TRUE);
	curwp = wp;
	make_current(curwp->w_bufp);
	upmode();
	updatelistbuffers();
	return (TRUE);
}

/*
 * Adjust a LINEPTR forward by the given number of screen-rows, limited by
 * the end of the buffer.
 */
static LINEPTR
adjust_forw(WINDOW *wp, LINEPTR lp, int n)
{
	register int i;
	LINEPTR	dlp;
	for (i = n; i > 0 && (lp != win_head(wp)); ) {
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
 * Adjust a LINEPTR backward by the given number of screen-rows, limited by
 * the end of the buffer.
 */
static LINEPTR
adjust_back(WINDOW *wp, LINEPTR lp, int n)
{
	register int i;
	LINEPTR	dlp;
	for (i = n; i > 0 && (lp != win_head(wp)); ) {
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
		int an;
		/* clamp the value at the size of the window */
		an = absol(n);
		if (an > curwp->w_ntrows)
			curwp->w_force = curwp->w_ntrows * (n / an);
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
refresh(int f, int n)
{

	if (f == FALSE) {
		sgarbf = TRUE;
	} else {
	        curwp->w_force = 0;             /* Center dot. */
	        curwp->w_flag |= WFFORCE;
	}

	return (TRUE);
}

/*
 * The command make the next window (next => down the screen) the current
 * window. There are no real errors, although the command does nothing if
 * there is only 1 window on the screen.
 *
 * with an argument this command finds the <n>th window from the top
 *
 */
int
nextwind(int f, int n)
{
	register WINDOW *wp;
	register int nwindows;		/* total number of windows */

	if (f) {

		/* first count the # of windows */
		nwindows = 0;
		for_each_window(wp)
			nwindows++;

		/* if the argument is negative, it is the nth window
		   from the bottom of the screen			*/
		if (n < 0)
			n = nwindows + n + 1;

		/* if an argument, give them that window from the top */
		if (n > 0 && n <= nwindows) {
			wp = wheadp;
			while (--n)
				wp = wp->w_wndp;
		} else {
			mlforce("[Window number out of range]");
			return(FALSE);
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
	register int c;
	register int row;
	int s;

	if (!f)
		n = 1;

	if (clexec || isnamedcmd) {
		static char cbuf[20];
		if ((s=mlreply("Position window with cursor at:  ", cbuf,
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

	return(reposition(TRUE,row));
}

/*
 * This command makes the previous window (previous => up the screen) the
 * current window. There aren't any errors, although the command does not do a
 * lot if there is 1 window.
 */
int
prevwind(int f, int n)
{
	register WINDOW *wp1;
	register WINDOW *wp2;

	/* if we have an argument, we mean the nth window from the bottom */
	if (f)
		return(nextwind(f, -n));

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
	if (!f)
		n = 1;
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
	register LINE  *lp;
	register int    i;
	int             was_n = n;

	lp = curwp->w_line.l;

	if (!f)
		n = 1;

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
	curwp->w_flag |= WFHARD | WFMODE;

	/* is it still in the window */
	for (i = 0; i < curwp->w_ntrows; lp = lforw(lp)) {
		if ((i += line_height(curwp,lp)) > curwp->w_ntrows)
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
	int	status;

	(void)nextwind(FALSE, 1);
	status = mvdnwind(f, n);
	(void)prevwind(FALSE, 1);
	return status;
}

int
mvupnxtwind(int f, int n)
{
	int	status;

	(void)nextwind(FALSE, 1);
	status = mvupwind(f, n);
	(void)prevwind(FALSE, 1);
	return status;
}

static int
scroll_sideways(int f, int n)
{
	int	original = w_val(curwp,WVAL_SIDEWAYS);

	if (!f) {
		int	nominal = term.t_ncol / 2;
		n = (n > 0) ? nominal : -nominal;
	}

	make_local_w_val(curwp,WVAL_SIDEWAYS);
	w_val(curwp, WVAL_SIDEWAYS) += n;

	if (w_val(curwp, WVAL_SIDEWAYS) < 0) {
		if (original == 0)
			kbd_alarm();
		w_val(curwp, WVAL_SIDEWAYS) = 0;
	}

	if (original != w_val(curwp,WVAL_SIDEWAYS))
        	curwp->w_flag  |= WFHARD|WFMOVE|WFMODE;

	return TRUE;
}

int
mvrightwind(int f, int n)
{
	return scroll_sideways(f,n);
}

int
mvleftwind(int f, int n)
{
	return scroll_sideways(f,-n);
}

/*
 * This command makes the current window the only window on the screen.
 * Try to set the framing so that "." does not have to move on the
 * display. Some care has to be taken to keep the values of dot and mark in
 * the buffer structures right if the distruction of a window makes a buffer
 * become undisplayed.
 */
/* ARGSUSED */
int
onlywind(int f, int n)
{
        register WINDOW *wp;

        wp = wheadp;
        while (wp != NULL) {
		register WINDOW *nwp;
		nwp = wp->w_wndp;
        	if (wp != curwp) {
	                if (--wp->w_bufp->b_nwnd == 0)
	                        undispbuff(wp->w_bufp,wp);
			unlink_window(wp);
	                free((char *) wp);
	        }
                wp = nwp;
        }
        wheadp = curwp;
        wheadp->w_wndp = NULL;

        curwp->w_line.l = adjust_back(curwp, curwp->w_line.l, curwp->w_toprow);
        curwp->w_ntrows = term.t_nrow-2;
        curwp->w_toprow = 0;
        curwp->w_flag  |= WFMODE|WFHARD|WFSBAR;
	curwp->w_split_hist = 0;
        return (TRUE);
}

/*
 * Delete the current window
 */

/* ARGSUSED */
int
delwind(int f, int n)
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
	register WINDOW *wp;	/* window to receive deleted space */

	/* if there is only one window, don't delete it */
	if (wheadp->w_wndp == NULL) {
		mlforce("[Cannot delete the only window]");
		return(FALSE);
	}

	/* find receiving window and give up our space */
	if (thewp == wheadp 
	 || ((thewp->w_split_hist & 1) == 0 && thewp->w_wndp)) {
		/* merge with next window down */
		wp = thewp->w_wndp;
                wp->w_line.l = adjust_back(wp, wp->w_line.l,
						thewp->w_ntrows+1);
		wp->w_ntrows += thewp->w_ntrows+1;
		wp->w_toprow = thewp->w_toprow;
		if (thewp == wheadp)
			wheadp = wp;
		else {
			WINDOW *pwp = wheadp;
			while(pwp->w_wndp != thewp)
				pwp = pwp->w_wndp;
			pwp->w_wndp = wp;
		}
		if (wp->w_split_hist & 1)
			wp->w_split_hist >>= 1;
	} else {
		/* find window before thewp in linked list */
		wp = wheadp;
		while(wp->w_wndp != thewp)
			wp = wp->w_wndp;
		/* add thewp's rows to the next window up */
		wp->w_ntrows += thewp->w_ntrows+1;
		
		wp->w_wndp = thewp->w_wndp; /* make their next window ours */
		if ((wp->w_split_hist & 1) == 0)
			wp->w_split_hist >>= 1;
	}

	/* get rid of the current window */
	if (--thewp->w_bufp->b_nwnd == 0)
		undispbuff(thewp->w_bufp,thewp);
	if (thewp == curwp) {
		curwp = wp;
		make_current(curwp->w_bufp);
	}
	free((char *)thewp);
	upmode();
	wp->w_flag |= WFSBAR | WFHARD;
	return(TRUE);
}

/*
 * Copy the window-traits struct, and adjust the embedded VAL struct so that
 * modes that are local remain so.
 */
void
copy_traits(W_TRAITS *dst, W_TRAITS *src)
{
	*dst = *src;
	copy_mvals(NUM_W_VALUES, dst->w_vals.wv, src->w_vals.wv);
}

/*
	Split the current window.  A window smaller than 3 lines cannot be
	split.  An argument of 1 forces the cursor into the upper window, an
	argument of two forces the cursor to the lower window.  The only other
	error that is possible is a "malloc" failure allocating the structure
	for the new window.
 */
static WINDOW *
splitw(int f, int n)
{
        register WINDOW *wp;
        register LINE   *lp;
        register int    ntru;
        register int    ntrl;
        register int    ntrd;
        register WINDOW *wp1;
        register WINDOW *wp2;

        if (curwp->w_ntrows < MINWLNS) {
                mlforce("[Cannot split a %d line window]", curwp->w_ntrows);
                return NULL;
        }
	/* set everything to 0's unless we want nonzero */
        if ((wp = typecalloc(WINDOW)) == NULL) {
		(void)no_memory("WINDOW");
                return NULL;
        }
	++curwp->w_bufp->b_nwnd;	       /* Displayed twice.     */
        wp->w_bufp  = curwp->w_bufp;
        copy_traits(&(wp->w_traits), &(curwp->w_traits));
        ntru = (curwp->w_ntrows-1) / 2;         /* Upper size           */
        ntrl = (curwp->w_ntrows-1) - ntru;      /* Lower size           */

        lp = curwp->w_line.l;
        ntrd = 0;
        while (lp != DOT.l) {
                ntrd += line_height(wp,lp);
                lp = lforw(lp);
        }

	/* ntrd is now the row containing dot */
        if (((f == FALSE) && (ntrd <= ntru)) || ((f == TRUE) && (n == 1))) {
                /* Old is upper window. */
	        /* Adjust the top line if necessary */
                if (ntrd == ntru) {             /* Hit mode line.       */
			if (ntrl > 1) {
				ntru++;
				ntrl--;
			} else {
	                        curwp->w_line.l = lforw(curwp->w_line.l);
			}
		}
                curwp->w_ntrows = ntru; /* new size */
		/* insert new window after curwp in window list */
                wp->w_wndp = curwp->w_wndp;
                curwp->w_wndp = wp;
		/* set new window's position and size */
                wp->w_toprow = curwp->w_toprow+ntru+1;
                wp->w_ntrows = ntrl;
		/* try to keep lower from reframing */
		wp->w_line.l = adjust_forw(wp, wp->w_line.l, ntru+1);
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
                wp->w_wndp   = curwp;
                wp->w_toprow = curwp->w_toprow;
                wp->w_ntrows = ntru;
                ++ntru;                         /* Mode line.           */
                curwp->w_toprow += ntru;
                curwp->w_ntrows  = ntrl;
		wp->w_dot.l = wp->w_line.l;
		/* move upper window dot to bottom line of upper */
		wp->w_dot.l = adjust_forw(wp, wp->w_dot.l, ntru-2);
		wp->w_dot.o = 0;
		/* adjust lower window topline */
		curwp->w_line.l = adjust_forw(curwp, curwp->w_line.l, ntru);
		/* update the split history */
		wp->w_split_hist <<= 1;
		curwp->w_split_hist = wp->w_split_hist | 1;
        }
        curwp->w_flag |= WFMODE|WFHARD|WFSBAR;
        wp->w_flag |= WFMODE|WFHARD;
        return wp;
}

/* external callable version -- return int instead of (WINDOW *) */
int
splitwind(int f, int n)
{
	return (splitw(f,n)) ? TRUE:FALSE;
}

/*
 * Enlarge the current window. Find the window that loses space. Make sure it
 * is big enough. If so, hack the window descriptions, and ask redisplay to do
 * all the hard work. You don't just set "force reframe" because dot would
 * move.
 */
/* ARGSUSED */
int
enlargewind(int f, int n)
{
        register WINDOW *adjwp;

        if (n < 0)
                return (shrinkwind(f, -n));
        if (wheadp->w_wndp == NULL) {
                mlforce("[Only one window]");
                return (FALSE);
        }
        if ((adjwp=curwp->w_wndp) == NULL) {
                adjwp = wheadp;
                while (adjwp->w_wndp != curwp)
                        adjwp = adjwp->w_wndp;
        }
        if (adjwp->w_ntrows <= n) {
                mlforce("[Impossible change]");
                return (FALSE);
        }
        if (curwp->w_wndp == adjwp) {           /* Shrink below.        */
                adjwp->w_line.l  = adjust_forw(adjwp, adjwp->w_line.l, n);
                adjwp->w_toprow += n;
        } else {                                /* Shrink above.        */
                curwp->w_line.l  = adjust_back(curwp, curwp->w_line.l, n);
                curwp->w_toprow -= n;
        }
        curwp->w_ntrows += n;
        adjwp->w_ntrows -= n;
        curwp->w_flag |= WFMODE|WFHARD|WFINS|WFSBAR;
        adjwp->w_flag |= WFMODE|WFHARD|WFKILLS;
        return (TRUE);
}

/*
 * Shrink the current window. Find the window that gains space. Hack at the
 * window descriptions. Ask the redisplay to do all the hard work.
 */
int
shrinkwind(int f, int n)
{
        register WINDOW *adjwp;

        if (n < 0)
                return (enlargewind(f, -n));
        if (wheadp->w_wndp == NULL) {
                mlforce("[Only one window]");
                return (FALSE);
        }
        if ((adjwp=curwp->w_wndp) == NULL) {
                adjwp = wheadp;
                while (adjwp->w_wndp != curwp)
                        adjwp = adjwp->w_wndp;
        }
        if (curwp->w_ntrows <= n) {
                mlforce("[Impossible change]");
                return (FALSE);
        }
        if (curwp->w_wndp == adjwp) {           /* Grow below.          */
                adjwp->w_line.l  = adjust_back(adjwp, adjwp->w_line.l, n);
                adjwp->w_toprow -= n;
        } else {                                /* Grow above.          */
                curwp->w_line.l  = adjust_forw(curwp, curwp->w_line.l, n);
                curwp->w_toprow += n;
        }
        curwp->w_ntrows -= n;
        adjwp->w_ntrows += n;
        curwp->w_flag |= WFMODE|WFHARD|WFKILLS|WFSBAR;
        adjwp->w_flag |= WFMODE|WFHARD|WFINS;
        return (TRUE);
}

/*	Resize the current window to the requested size	*/
int
resize(int f, int n)
{
	int clines;	/* current # of lines in window */
	
	/* must have a non-default argument, else ignore call */
	if (f == FALSE)
		return(TRUE);

	/* find out what to do */
	clines = curwp->w_ntrows;

	/* already the right size? */
	if (clines == n)
		return(TRUE);

	return(enlargewind(TRUE, n - clines));
}

/*
 * Pick a window for a pop-up. Split the screen if there is only one window.
 * Pick the uppermost window that isn't the current window. An LRU algorithm
 * might be better. Return a pointer, or NULL on error.
 */
WINDOW  *
wpopup(void)
{
        register WINDOW *wp;
        register WINDOW *owp;
        register WINDOW *biggest_wp;

	owp = curwp;
        wp = biggest_wp = wheadp;                /* Find window to split   */
        while (wp->w_wndp != NULL) {
                wp = wp->w_wndp;
		if(wp->w_ntrows > biggest_wp->w_ntrows)
			biggest_wp = wp;
	}
	if (biggest_wp->w_ntrows >= MINWLNS) {
	    curwp = biggest_wp;
	    wp = splitw(FALSE,0); /* yes -- choose the unoccupied half */
	    curwp = owp;
	}
	else {
		/*  biggest_wp was too small  */
		wp = wheadp;		/* Find window to use	*/
	        while (wp!=NULL && wp==curwp) /* uppermost non-current window */
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

    nlines = line_count(curwp->w_bufp);
    if (nlines <= 0)
	nlines = 1;

    if (nlines == curwp->w_ntrows)
	return;

    if ((curwp->w_split_hist & 1) == 0 || wheadp == curwp) {
	int nrows, snrows;
	/* give/steal from lower window */
	nrows = curwp->w_ntrows + curwp->w_wndp->w_ntrows - 1;
	/* don't take more than 3/4 of its rows */
	snrows = (nrows*3)/4;
	if (nlines > snrows)
	    nlines = snrows;
	resize(TRUE, nlines);
    }
    else {
	/* give/steal from upper window; need to find upper window */
	register WINDOW *wp;
	WINDOW *savewp = curwp;
	int nrows, snrows;
	for (wp = wheadp; 
	     wp->w_wndp != curwp && wp->w_wndp != NULL; 
	     wp = wp->w_wndp)
	    ;
	curwp = wp;
	nrows = curwp->w_ntrows + curwp->w_wndp->w_ntrows - 1;
	/* don't take more than 3/4 of its rows */
	snrows = (nrows*3)/4;
	if (nlines > snrows)
	    nlines = snrows;
	resize(TRUE, nrows - nlines + 1);
	curwp = savewp;
    }
}

int
scrnextup(int f, int n)	/* scroll the next window up (back) a page */
{
	int	status;

	(void)nextwind(FALSE, 1);
	status = backhpage(f, n);
	(void)prevwind(FALSE, 1);
	return status;
}

int
scrnextdw(int f, int n)	/* scroll the next window down (forward) a page */
{
	int	status;

	(void)nextwind(FALSE, 1);
	status = forwhpage(f, n);
	(void)prevwind(FALSE, 1);
	return status;
}

#if ! SMALLER
/* ARGSUSED */
int
savewnd(int f, int n)	/* save ptr to current window */
{
	swindow = curwp;
	return(TRUE);
}

/* ARGSUSED */
int
restwnd(int f, int n)	/* restore the saved screen */
{
	register WINDOW *wp;

	/* find the window */
	for_each_window(wp) {
		if (wp == swindow)
			return set_curwp(wp);
	}

	mlforce("[No such window exists]");
	return(FALSE);
}
#endif

int
newlength(int f, int n)	/* resize the screen, re-writing the screen */
{
	WINDOW *wp;	/* current window being examined */
	WINDOW *nextwp;	/* next window to scan */
	WINDOW *lastwp;	/* last window scanned */

	if (!f) {
		mlforce("[No length given]");
		return FALSE;
	}

	/* make sure it's in range */
	if (n < MINWLNS || n > term.t_mrow) {
		mlforce("[Screen length out of range]");
		return(FALSE);
	}

	if (term.t_nrow == n)
		return(TRUE);
	else if (term.t_nrow < n) {

		/* go to the last window */
		wp = wheadp;
		while (wp->w_wndp != NULL)
			wp = wp->w_wndp;

		/* and enlarge it as needed */
		wp->w_ntrows = n - wp->w_toprow - 2;
		wp->w_flag |= WFHARD|WFMODE;

	} else {

		/* rebuild the window structure */
		nextwp = wheadp;
		lastwp = NULL;
		while (nextwp != NULL) {
			wp = nextwp;
			nextwp = wp->w_wndp;
	
			/* get rid of it if it is too low */
			if (wp->w_toprow >= n - 2) {

				if (--wp->w_bufp->b_nwnd == 0) {
					undispbuff(wp->w_bufp,wp);
				}
	
				/* update curwp and lastwp if needed */
				if (wp == curwp) {
					curwp = wheadp;
					make_current(curwp->w_bufp);
				}
				if (lastwp != NULL)
					lastwp->w_wndp = NULL;

				/* free the structure */
				free((char *)wp);
				wp = NULL;

			} else {
				/* need to change this window size? */
				if (mode_row(wp) >= n - 3) {
					wp->w_ntrows = n - wp->w_toprow - 2;
					wp->w_flag |= WFHARD|WFMODE;
				}
			}
	
			lastwp = wp;
		}
	}

	/* screen is garbage */
	term.t_nrow = n;
	sgarbf = TRUE;
	return(TRUE);
}

int
newwidth(int f, int n)	/* resize the screen, re-writing the screen */
{
	register WINDOW *wp;

	if (!f) {
		mlforce("[No width given]");
		return FALSE;
	}

	/* make sure it's in range */
	if (n < 10 || n > term.t_mcol) {
		mlforce("[Screen width out of range]");
		return(FALSE);
	}

	/* otherwise, just re-width it (no big deal) */
	term.t_ncol = n;
	term.t_margin = n / 10;
	term.t_scrsiz = n - (term.t_margin * 2);

	/* force all windows to redraw */
	for_each_window(wp)
		wp->w_flag |= WFHARD | WFMOVE | WFMODE;

	sgarbf = TRUE;

	return(TRUE);
}

#if OPT_EVAL
int
getwpos(void)	/* get screen offset of current line in current window */
{
	register int sline;	/* screen line from top of window */
	register LINE *lp;	/* scannile line pointer */

	/* search down the line we want */
	lp = curwp->w_line.l;
	sline = 1;
	while (lp != DOT.l) {
		sline += line_height(curwp,lp);
		lp = lforw(lp);
	}

	/* and return the value */
	return(sline);
}
#endif

/*
 * Initialize all of the windows.
 */
void
winit(int screen)
{
        register WINDOW *wp;

        wp = typecalloc(WINDOW);		/* First window         */
        if (wp==NULL)
		ExitProgram(BADEXIT);
        wheadp = wp;
        curwp  = wp;
        wp->w_wndp  = NULL;                     /* Initialize window    */
        wp->w_dot  = nullmark;
	wp->w_line = nullmark;
#if WINMARK
        wp->w_mark = nullmark;
#endif
        wp->w_lastdot = nullmark;
        wp->w_toprow = 0;
        wp->w_values = global_w_values;
        wp->w_ntrows = screen
			? term.t_nrow-2		/* "-1" for mode line.  */
			: 1;			/* command-line		*/
        wp->w_force = 0;
        wp->w_flag  = WFMODE|WFHARD;            /* Full.                */
	wp->w_bufp  = NULL;

	if (screen) {
		delink_bp(bminip);
	} else {
		/* create the command-buffer */
		wminip = wp;
		bminip = wp->w_bufp = bfind("", BFINVS);
	}
}

/* For memory-leak testing (only!), releases all display storage. */
#if NO_LEAKS
void	wp_leaks(void)
{
	register WINDOW *wp;

	while ((wp = wheadp) != 0) {
        	wp = wp->w_wndp;
		free((char *)wheadp);
		wheadp = wp;
	}
}
#endif
