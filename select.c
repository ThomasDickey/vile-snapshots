/*
 * select.c		-- selection handling code for vile.
 *
 * Author: Kevin Buettner, Paul Fox
 * Creation: 2/26/94
 *
 * Description:  The following code is an attempt to improve the selection
 * mechanism for vile/xvile.  While my initial goal is to improve selection
 * handling for xvile, there is no reason that this code can not be used on
 * other platforms with some kind of pointing device.  In addition, the
 * mechanism is general enough to be used for other kinds of persisent
 * attributed text.
 *
 * For the purposes of this code, a selection is considered to be a region of
 * text which most applications highlight in some manner.  The user may
 * transfer selected text from one application to another (or even to the
 * same application) in some platform dependent way.  The mechanics of
 * transfering the selection are not dealt with in this file.  Procedures
 * for dealing with the representation are maintained in this file.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/select.c,v 1.77 1998/09/22 10:57:45 Kuntal.Daftary Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"
#include	"nefunc.h"

#define BTN_BEGIN   1
#define BTN_PASTE   2
#define BTN_EXTEND  3

#define SEL_BEGIN   10	/* button 1 up */
#define SEL_PASTE   11	/* button 2 up */
#define SEL_EXTEND  12	/* button 3 up */
#define SEL_RELEASE 13	/* click on modeline */
#define SEL_FINISH  14	/* finish a selection */

#if OPT_SELECTIONS

extern REGION *haveregion;

static	void	detach_attrib (BUFFER *bp, AREGION *arp);
static	int	attribute_cntl_a_sequences (void);

/*
 * startbufp and startregion are used to represent the start of a selection
 * prior to any highlighted text being displayed.  The start and end of
 * the region will both be where the selection is to start.  Although
 * startregion is attached to the buffer indicated by startbufp, nothing
 * will be displayed since the ar_vattr field is zero.  The reason for
 * attaching it to the buffer is to force the MARKs which represent the
 * start of the region to be updated.
 *
 * selbufp and selregion are used to represent a highlighted selection.
 *
 * When starbufp or selbufp are NULL, the corresponding AREGION (startregion or
 * selregion) is not attached to any buffer and is invalid.
 */

static MARK orig_region;
static MARK plus_region;
static BUFFER *	startbufp = NULL;
static AREGION	startregion;
static BUFFER *	selbufp = NULL;
static AREGION	selregion;
#if OPT_HYPERTEXT
static char *	hypercmd;
#endif

typedef enum { ORIG_FIXED, END_FIXED, UNFIXED } WHICHEND;

static WHICHEND whichend;

void
free_attribs(BUFFER *bp)
{
    AREGION *p, *q;
    p = bp->b_attribs;
    while (p != NULL) {
	q = p->ar_next;
#if OPT_HYPERTEXT
	if (p->ar_hypercmd)
	    free(p->ar_hypercmd);
	    p->ar_hypercmd = 0;
#endif
	if (p == &selregion)
	    selbufp = NULL;
	else if (p == &startregion)
	    startbufp = NULL;
	else
	    free((char *) p);
	p = q;
    }
    bp->b_attribs = NULL;
}

void
free_attrib(BUFFER *bp, AREGION *ap)
{
    detach_attrib(bp, ap);
#if OPT_HYPERTEXT
    if (ap->ar_hypercmd) {
	free(ap->ar_hypercmd);
	ap->ar_hypercmd = 0;
    }
#endif
    if (ap == &selregion)
	selbufp = NULL;
    else if (ap == &startregion)
	startbufp = NULL;
    else
	free((char *) ap);
}

static void
detach_attrib(BUFFER *bp, AREGION *arp)
{
    if (bp != NULL) {
	WINDOW *wp;
	AREGION **rpp;
	for_each_visible_window(wp) {
	    if (wp->w_bufp == bp)
		wp->w_flag |= WFHARD;
	}
	rpp = &bp->b_attribs;
	while (*rpp != NULL) {
	    if (*rpp == arp) {
		*rpp = (*rpp)->ar_next;
		arp->ar_region.r_attr_id = 0;
		break;
	    }
	    else
		rpp = &(*rpp)->ar_next;
	}
    }
}

void
find_release_attr(BUFFER *bp, REGION *rp)
{
    if (bp != NULL) {
	AREGION **rpp;
	rpp = &bp->b_attribs;
	while (*rpp != NULL) {
	    if ((*rpp)->ar_region.r_attr_id == rp->r_attr_id) {
		free_attrib(bp, *rpp);
		break;
	    }
	    else
		rpp = &(*rpp)->ar_next;
	}
    }
}

int
assign_attr_id(void)
{
    static int attr_id;
    return attr_id++;
}

static void
attach_attrib(BUFFER *bp, AREGION *arp)
{
    WINDOW *wp;
    arp->ar_next = bp->b_attribs;
    bp->b_attribs = arp;
    for_each_visible_window(wp)
	if (wp->w_bufp == bp)
	    wp->w_flag |= WFHARD;
    arp->ar_region.r_attr_id = (unsigned short) assign_attr_id();
}

/*
 * Adjusts dot to last char of last line if dot is past end of buffer.  This
 * can happen when selecting with the mouse.
 */

static void
fix_dot(void)
{
    if (is_header_line(DOT, curwp->w_bufp)) {
	DOT.l = lback(DOT.l);
	DOT.o = llength(DOT.l);
    }
}

/*
 * Output positional information regarding the selection to the message line.
 */

static void
show_selection_position(int yanked)
{
#ifdef WMDTERSELECT
    if (!w_val(curwp, WMDTERSELECT)) {
	mlwrite("(%d,%d) thru (%d,%d) %s",
		line_no(selbufp, selregion.ar_region.r_orig.l),
		getcol(selregion.ar_region.r_orig, FALSE) + 1,
		line_no(selbufp, selregion.ar_region.r_end.l),
		getcol(selregion.ar_region.r_end, FALSE),
		yanked ? "yanked" : "selected");

    }
#endif /* WMDTERSELECT */
}

/* Start a selection at dot */
int
sel_begin(void)
{
    fix_dot();
    detach_attrib(startbufp, &startregion);
    plus_region =
    orig_region =
    startregion.ar_region.r_orig =
    startregion.ar_region.r_end  = DOT;
    plus_region.o += 1;
    startregion.ar_vattr = 0;
    startregion.ar_shape = EXACT;
#if OPT_HYPERTEXT
    startregion.ar_hypercmd = 0;
#endif
    startbufp = curwp->w_bufp;
    attach_attrib(startbufp, &startregion);
    whichend = UNFIXED;
    return TRUE;
}

static int
dot_vs_mark(void)
{
	int	cmp = line_no(curbp, DOT.l) - line_no(curbp, MK.l);
	if (cmp == 0)
		cmp = DOT.o - MK.o;
	return cmp;
}

/* Extend the current selection to dot */
int
sel_extend(int wiping, int include_dot)
{
	REGIONSHAPE save_shape = regionshape;
	REGION a,b;
	WINDOW *wp;
	MARK saved_dot;
	MARK working_dot;

	saved_dot = DOT;
	if (startbufp != NULL) {
		detach_attrib(selbufp, &selregion);
		selbufp = startbufp;
		selregion = startregion;
		attach_attrib(selbufp, &selregion);
		detach_attrib(startbufp, &startregion);
		startbufp = NULL;
	}

	if (curwp->w_bufp != selbufp)
		return FALSE;		/* handles NULL case also */

	fix_dot();
	regionshape = selregion.ar_shape;

	if (wiping && whichend == END_FIXED)
		MK = selregion.ar_region.r_end;
	else
		MK = selregion.ar_region.r_orig;

	/* FIXME: Make sure DOT and MK are in the same buffer */

	/*
	 * If we're extending in the positive direction, we want to include DOT
	 * in the selection.  To include DOT, we must advance it one char since
	 * a region runs from r_orig up to but not including r_end.
	 */
	working_dot = DOT;
	if (include_dot
	 && (selregion.ar_shape == EXACT)
	 && dot_vs_mark() >= 0) {
		if (samepoint(MK, orig_region)) {
			DOT.o += 1;
		} else if (samepoint(MK, plus_region)) {
			DOT.o += 1;
			MK = orig_region;
		}
	}
	if (getregion(&a) == FALSE) {
		return FALSE;
	}
	DOT = working_dot;

	/*
	 * Build a second region in the "opposite" direction.
	 */
	if (wiping && whichend == ORIG_FIXED)
		MK = selregion.ar_region.r_orig;
	else
		MK = selregion.ar_region.r_end;

	if (include_dot) {
		if (selregion.ar_shape == EXACT) {
			if (dot_vs_mark() <= 0) {
				if (samepoint(MK, orig_region))
					MK.o += 1;
			}
		} else if (selregion.ar_shape == RECTANGLE) {
			if (samepoint(MK, DOT)) { /* avoid making empty-region */
				MK =  orig_region;
				DOT = plus_region;
			}
		}
	}
	if (getregion(&b) == FALSE) {
		return FALSE;
	}

	/*
	 * The two regions, 'a' and 'b' are _usually_ identical, except for the
	 * special case where we've extended one to the right to include the
	 * right endpoint of the region.
	 *
	 * For EXACT selections, setting 'whichend' to ORIG_FIXED means that
	 * we're selecting from the anchor point right/down.  Conversely,
	 * setting it to END_FIXED means that we selecting left/up.
	 *
	 * Rectangles are specified by making MK the opposite corner from DOT.
	 * If DOT is below MK, we'll say that the selection region is
	 * ORIG_FIXED so that the next call on this function will build the
	 * regions a/b consistently.
	 *
	 * If the regions a/b are empty, we've made a mistake; this will cause
	 * the selection to be dropped in xvile.
	 */

	if (a.r_size > b.r_size) {
		whichend = ORIG_FIXED;
		selregion.ar_region = a;
	}
	else {
		if (selregion.ar_shape == RECTANGLE) {
			if (dot_vs_mark() < 0)
				whichend = END_FIXED;
			else
				whichend = ORIG_FIXED;
		} else {	/* exact or full-line */
			whichend = END_FIXED;
		}
		selregion.ar_region = b;
	}

	selregion.ar_vattr = VASEL | VOWN_SELECT;
	for_each_visible_window(wp) {
		if (wp->w_bufp == selbufp)
			wp->w_flag |= WFHARD;
	}

	show_selection_position(FALSE);

	regionshape = save_shape;
	DOT = saved_dot;
	OWN_SELECTION();
	return TRUE;
}

/*
 * Detach current selection (if attached) and null the associated buffer
 * pointer.
 */
void
sel_release(void)
{
    detach_attrib(selbufp, &selregion);
    selbufp = NULL;
}

/*
 * Assert/reassert ownership of selection if appropriate.  This is necessary
 * in order to paste a selection after it's already been pasted once and then
 * modified.
 */

void
sel_reassert_ownership(BUFFER *bp)
{
    if (selbufp == bp) {
	OWN_SELECTION();
    }
}

#if OPT_SEL_YANK
/*
 * Yank the selection.  Return TRUE if selection could be yanked, FALSE
 * otherwise.  Note that this code will work even if the buffer being
 * yanked from is not attached to any window since it creates its own
 * fake window in order to perform the yanking.
 */

int
sel_yank(int reg)
{
    REGIONSHAPE save_shape;
    WINDOW *save_wp;

    if (selbufp == NULL)
	return FALSE;			/* No selection to yank */

    if ((save_wp = push_fake_win(selbufp)) == NULL)
	return FALSE;

    save_shape = regionshape;
    ukb = reg;
    kregflag = 0;
    haveregion = &selregion.ar_region;
    regionshape = selregion.ar_shape;
    yankregion();
    haveregion = NULL;
    regionshape = save_shape;
    pop_fake_win(save_wp);
    show_selection_position(TRUE);

    /* put cursor back on screen...is there a cheaper way to do this?  */
    (void)update(FALSE);
    return TRUE;
}

#if NEEDED
int
sel_attached(void)
{
    return startbufp == NULL;
}
#endif  /* NEEDED */

BUFFER *
sel_buffer(void)
{
    return (startbufp != NULL) ? startbufp : selbufp;
}
#endif  /* OPT_SEL_YANK */

int
sel_setshape(REGIONSHAPE shape)
{
    if (startbufp != NULL) {
	startregion.ar_shape = shape;
	return TRUE;
    }
    else if (selbufp != NULL) {
	selregion.ar_shape = shape;
	return TRUE;
    }
    else {
	return FALSE;
    }
}

/* return a region which goes from DOT to the far end of the current
	selection region.  shape is maintained.  returns pointer to static
	region struct.
*/
static REGION *
extended_region(void)
{
    REGION *rp = NULL;
    static REGION a, b;
    MARK savemark;

    savemark = MK;
    regionshape = selregion.ar_shape;
    MK = selregion.ar_region.r_orig;
    DOT.o += 1;
    if (getregion(&a) == TRUE) {
	DOT.o -= 1;
	MK = selregion.ar_region.r_end;
	if (regionshape == FULLLINE)
	    MK.l = lback(MK.l);
	/* region b is to the end of the selection */
	if (getregion(&b) == TRUE) {
	    /* if a is bigger, it's the one we want */
	    if (a.r_size > b.r_size)
		rp = &a;
	    else
		rp = &b;
	}
    } else {
	DOT.o -= 1;
    }
    MK = savemark;
    return rp;
}

static	int	doingopselect;

/* ARGSUSED */
int
sel_motion(int f GCC_UNUSED, int n GCC_UNUSED)
{
    if (selbufp == NULL) {
	mlwrite("[No selection exists.]");
	return FALSE;
    }

    if (selbufp != curbp) {
	/* FIXME -- sure would be nice if we could do non-destructive
		things to other buffers, mainly yank. */
	mlwrite("[Selection not in current buffer.]");
	return FALSE;
    }

    curwp->w_flag |= WFMOVE;

    /* if this is happening on behalf of an operator, we're pretending
     * that the motion took us from one end of the selection to the
     * other, unless we're trying to select to the selection, in which
     * case that would be self-defeating
     */
    if (doingopcmd && !doingopselect) {
	pre_op_dot = selregion.ar_region.r_orig;  /* move us there */
	haveregion = &selregion.ar_region;
	regionshape = selregion.ar_shape;
	return TRUE;
    }

    if (!doingopcmd) { /* it's a simple motion -- go to the top of selection */
	/* remember -- this can never be used with an operator, as in
	 * "delete to the selection", since that case is taken care
	 * of above, and is really the whole reason for this
	 * "motion" in the first place.  */
	DOT = selregion.ar_region.r_orig;  /* move us there */
	return TRUE;
    }

    /* we must be doing an extension */
    haveregion = extended_region();
    return haveregion ? TRUE:FALSE;

}

static int
selectregion(void)
{
	register int    status;
	REGION		region;
	MARK		savedot;
	MARK		savemark;
	int		hadregion = FALSE;

	savedot = DOT;
	savemark = MK;
	if (haveregion) {	/* getregion() will clear this, so
					we need to save it */
		region = *haveregion;
		hadregion = TRUE;
	}
	status = yankregion();
	DOT = savedot;
	MK = savemark;
	if (status != TRUE)
		return status;
	if (hadregion || ((status = getregion(&region)) == TRUE)) {
	    detach_attrib(startbufp, &startregion);
	    detach_attrib(selbufp, &selregion);
	    selbufp = curbp;
	    selregion.ar_region = region;
	    selregion.ar_vattr = VASEL | VOWN_SELECT;
	    selregion.ar_shape = regionshape;
#if OPT_HYPERTEXT
	    selregion.ar_hypercmd = 0;
#endif
	    attach_attrib(selbufp, &selregion);
	    OWN_SELECTION();
	}
	return status;
}

static void
sweepmsg(const char *msg)
{
	char	temp[NLINE];
	(void)kcod2pstr(fnc2kcod(&f_multimotion), temp);
	mlforce("[%s (end with %*S)]", msg, *temp,temp+1);
}

static int
release_selection(int status)
{
	TRACE(("MOUSE release selection\n"))
	if (doingsweep) {
		doingsweep = FALSE;
		if (status != TRUE)
			mlforce("[Sweeping: Aborted]");
		else
			mlerase();
	}
	sel_release();
	return status;
}

#if OPT_MOUSE
static int
paste_selection(void)
{
	if (!doingsweep) {
		TRACE(("MOUSE paste selection\n"))
		mayneedundo();
		return putafter(FALSE, 1);
	}
	return SEL_PASTE;
}

/*
 * On button press, we get an explicit number (1,2,3), and on release we don't
 * really know which button, but assume it is the last-pressed button.
 */
int
on_mouse_click(int button, int y, int x)
{
	static int first_x, first_y, pending;
	WINDOW *this_wp, *that_wp;
	int status;

	if (button > 0) {
		if ((this_wp = row2window(y)) != 0
		 && (y != mode_row(this_wp))) {
			if (!doingsweep) {
				if (button == BTN_EXTEND) {
					first_x = offs2col(this_wp, this_wp->w_dot.l, this_wp->w_dot.o);
					first_y = line_no(this_wp->w_bufp, this_wp->w_dot.l)
						- line_no(this_wp->w_bufp, this_wp->w_line.l);
				} else {
					first_x = x;
					first_y = y;
				}
			}
			status = setcursor(y, x);
			/*
			 * Check for button1-down while we're in multimotion
			 * sweep, so we can suppress highlighting extension.
			 */
			if (button != BTN_EXTEND
			 && status == TRUE
			 && doingsweep) {
				status = SORTOFTRUE;
				if (button == BTN_BEGIN) {
					first_x = x;
					first_y = y;
				}
			}
		} else { /* pressed button on modeline */
			status = SORTOFTRUE;
			first_x = x;
			first_y = y;
		}
		pending = button;
	} else if (pending) {
		button  = pending;
		pending = FALSE;
		this_wp = row2window(y);
		that_wp = row2window(first_y);
		if (this_wp == 0
		 || that_wp == 0
		 || reading_msg_line) {
			TRACE(("MOUSE cannot move msg-line\n"))
			status = FALSE;
		} else if (insertmode
		 && (this_wp != curwp || that_wp != curwp)) {
			TRACE(("MOUSE cannot move from window while inserting\n"))
			kbd_alarm();
			status = ABORT;
		} else if (first_y == mode_row(that_wp)) { /* drag modeline? */
			if (first_y == y) {
				sel_release();
				status = SEL_RELEASE;
			} else {
				WINDOW *save_wp = curwp;
				TRACE(("MOUSE dragging modeline\n"))
				set_curwp(that_wp);
				status = shrinkwind(FALSE, first_y - y);
				set_curwp(save_wp);
			}
		} else if (y != first_y || x != first_x) { /* drag selection */
			if (button == BTN_PASTE) {
				(void) setcursor(y, x);
				status = paste_selection();
			} else if (doingsweep) {
				switch (button) {
				case BTN_BEGIN:
					(void) release_selection(TRUE);
					status = setcursor(first_y, first_x);
					if (status == TRUE) {
						MK = DOT;
						status = SEL_BEGIN;
						TRACE(("MOUSE setting SEL_BEGIN MK %d.%d\n",
							line_no(curbp, MK.l), MK.o))
					}
					break;
				case BTN_PASTE:
					(void) setcursor(y, x);
					status = paste_selection();
					break;
				default:
					(void) setcursor(y, x);
					status = SEL_EXTEND;
					TRACE(("MOUSE setting SEL_EXTEND DOT %d.%d MK %d.%d\n",
						line_no(curbp, MK.l), MK.o,
						line_no(curbp, DOT.l), DOT.o))
					break;
				}
			} else {
				TRACE(("MOUSE begin multimotion on button%d-up\n", button))
				if (button == BTN_EXTEND) {
					(void) setcursor(y, x);
					y = first_y;
					x = first_x;
				}
				doingsweep = SORTOFTRUE;
				(void)sel_begin();
				(void)sel_setshape(EXACT);
				status = setcursor(y, x);
				status = multimotion(TRUE,1);
				TRACE(("MOUSE end multimotion after button%d-up\n", button))
				if (status == SEL_PASTE)
					status = paste_selection();
			}
		} else { /* position the cursor */
			TRACE(("MOUSE button %d position cursor\n", button))
			(void) setcursor(y, x);
			switch (button) {
			case BTN_BEGIN:
				status = SEL_FINISH;
				break;
			case BTN_PASTE:
				status = paste_selection();
				break;
			default:
				status = release_selection(TRUE);
				break;
			}
		}
	} else {
		TRACE(("MOUSE ignored (illegal state)\n"))
		status = FALSE;
	}

	if (status == TRUE || status >= SORTOFTRUE)
		(void)update(TRUE);

	TRACE(("MOUSE status:%d\n", status))
	return status;
}
#endif

int
multimotion(int f, int n)
{
	const CMDFUNC	*cfp;
	int s, c, waserr;
	int pasting;
	REGIONSHAPE shape;
	MARK savedot;
	MARK savemark;
	MARK realdot;
	BUFFER *origbp = curbp;
	static int wassweephack = FALSE;

	/* Use the repeat-count as a shortcut to specify the type of selection.
	 * I'd use int-casts of the enum value, but declaring enums with
	 * specific values isn't 100% portable.
	 */
	if (!f || n <= 0)
		n = 1;
	if (n == 3)
		regionshape = RECTANGLE;
	else if (n == 2)
		regionshape = FULLLINE;
	else
		regionshape = EXACT;
	shape = regionshape;

	sweephack = FALSE;
	savedot = DOT;
	switch (doingsweep) {
	case TRUE:	/* the same command terminates as starts the sweep */
		doingsweep = FALSE;
		mlforce("[Sweeping: Completed]");
		regionshape = shape;
		/* since the terminating 'q' is executed as a motion, we have
		   now lost the value of sweephack we were interested in, the
		   one that tells us to include DOT.o in the selection.
		   so we preserved it in wassweephack, and restore it here.
		 */
		if (wassweephack)
			sweephack = wassweephack;
		return TRUE;
	case SORTOFTRUE:
		doingsweep = TRUE;
		sweepmsg("Begin cursor sweep...");
		sel_extend(TRUE,(regionshape != RECTANGLE && sweephack));
		savedot = MK;
		TRACE(("MOUSE BEGIN DOT: %d.%d MK %d.%d\n",
			line_no(curbp, DOT.l), DOT.o,
			line_no(curbp, MK.l), MK.o))
		break;
	case FALSE:
		doingsweep = TRUE;
		sweepmsg("Begin cursor sweep...");
		(void)sel_begin();
		(void)sel_setshape(shape);
		break;
	}

	waserr = TRUE; /* to force message "state-machine" */
	realdot = DOT;
	pasting = FALSE;

	while (doingsweep) {

		/* Fix up the screen	*/
		s = update(FALSE);

		/* get the next command from the keyboard */
		c = kbd_seq();

		if (ABORTED(c)
		 || curbp != origbp) {
			return release_selection(FALSE);
		}

		f = FALSE;
		n = 1;

		do_repeats(&c,&f,&n);

		/* and execute the command */
		cfp = kcod2fnc(c);
		if ( (cfp != NULL)
		 && ((cfp->c_flags & MOTION) != 0)) {
			MARK testdot;

			wassweephack = sweephack;
			sweephack = FALSE;
			TRACE(("MOUSE TEST DOT: %d.%d MK %d.%d\n",
				line_no(curbp, DOT.l), DOT.o,
				line_no(curbp, MK.l), MK.o))
			testdot = DOT;

			s = execute(cfp, f, n);
			switch (s) {
			case SEL_RELEASE:
				TRACE(("MOUSE SEL_RELEASE %d.%d\n",
					line_no(curbp, DOT.l), DOT.o))
				return release_selection(TRUE);

			case SEL_PASTE:
				pasting = TRUE;
				/* FALLTHRU */

			case SEL_FINISH:
				doingsweep = FALSE;
				break;

			case SORTOFTRUE:
				TRACE(("MOUSE selection pending %d.%d -> %d.%d\n",
					line_no(curbp, realdot.l), realdot.o,
					line_no(curbp, testdot.l), testdot.o))
				realdot = testdot;
				break;

			case SEL_BEGIN:
				savedot = MK;
				TRACE(("MOUSE SEL_BEGIN...\n"))
				/*FALLTHRU*/

			case SEL_EXTEND:
				TRACE(("MOUSE SEL_EXTEND from %d.%d to %d.%d\n",
					line_no(curbp, savedot.l), savedot.o,
					line_no(curbp, DOT.l), DOT.o))
				/*FALLTHRU*/

			case TRUE:
				if (waserr && doingsweep) {
					sweepmsg("Sweeping...");
					waserr = FALSE;
				}
				realdot = DOT;
				DOT = savedot;
				(void)sel_begin();
				DOT = realdot;
				TRACE(("MOUSE LOOP save: %d.%d real %d.%d, mark %d.%d\n",
					line_no(curbp, savedot.l), savedot.o,
					line_no(curbp, realdot.l), realdot.o,
					line_no(curbp, MK.l), MK.o))
				(void)sel_setshape(shape);
				/* we sometimes want to include DOT.o in the
				   selection (unless it's a rectangle, in
				   which case it's taken care of elsewhere)
				 */
				sel_extend(TRUE,(regionshape != RECTANGLE &&
					sweephack));
				break;

			default:
				sweepmsg("Sweeping: Motion failed.");
				waserr = TRUE;
				break;
			}
		 } else {
			sweepmsg("Sweeping: Only motions permitted");
			waserr = TRUE;
		 }

	}
	regionshape = shape;
	/* if sweephack is set here, it's because the last motion had
		it set */
	if (doingopcmd)
		pre_op_dot = savedot;

	savedot = DOT;
	savemark = MK;
	DOT = realdot;
	TRACE(("MOUSE SAVE DOT: %d.%d MK %d.%d\n",
		line_no(curbp, DOT.l), DOT.o,
		line_no(curbp, MK.l), MK.o))
	if ((regionshape != RECTANGLE) && sweephack) {
		if (dot_vs_mark() < 0)
			MK.o += 1;
		else
			DOT.o += 1;
	}
	s = yankregion();
	DOT = savedot;
	MK = savemark;

	sweephack = wassweephack = FALSE;

	if (s == TRUE && pasting)
		s = SEL_PASTE;
		
	return s;
}

/*ARGSUSED*/
int
multimotionfullline(int f GCC_UNUSED, int n GCC_UNUSED)
{
	return multimotion(TRUE,2);
}

/*ARGSUSED*/
int
multimotionrectangle(int f GCC_UNUSED, int n GCC_UNUSED)
{
	return multimotion(TRUE,3);
}

#if OPT_PERL || OPT_TCL
BUFFER *
get_selection_buffer_and_region(AREGION *arp)
{
    if (selbufp) {
	*arp = selregion;
    }
    return selbufp;
}
#endif

int
attributeregion(void)
{
	register int    status;
	REGION		region;
	AREGION *	arp;

	if ((status = getregion(&region)) == TRUE) {
	    if (VATTRIB(videoattribute) != 0
#if OPT_HYPERTEXT
		|| hypercmd != 0)
#endif
	    {
		/* add new attribute-region */
		if ((arp = typealloc(AREGION)) == NULL) {
		    (void)no_memory("AREGION");
		    return FALSE;
		}
		arp->ar_region = region;
		arp->ar_vattr = videoattribute; /* include ownership */
		arp->ar_shape = regionshape;
#if OPT_HYPERTEXT
		arp->ar_hypercmd = hypercmd;	/* already malloc'd for us */
		hypercmd = 0;			/* reset it for future calls */
#endif
		attach_attrib(curbp, arp);
	    } else { /* purge attributes in this region */
		L_NUM rls = line_no(curbp, region.r_orig.l);
		L_NUM rle = line_no(curbp, region.r_end.l);
		C_NUM ros = region.r_orig.o;
		C_NUM roe = region.r_end.o;
		AREGION *p, *q, *n;
		int owner;

		owner = VOWNER(videoattribute);

		for (p = curbp->b_attribs, q = 0; p != 0; p = q) {
		    L_NUM pls, ple;
		    C_NUM pos, poe;

		    q = p->ar_next;

		    if (owner != 0 && owner != VOWNER(p->ar_vattr))
			continue;

		    pls = line_no(curbp, p->ar_region.r_orig.l);
		    ple = line_no(curbp, p->ar_region.r_end.l);
		    pos = p->ar_region.r_orig.o;
		    poe = p->ar_region.r_end.o;

		    /* Earlier the overlapping region check was made based only
		     * on line numbers and so was right only for FULLINES shape
		     * changed it to be correct for EXACT and RECTANLGE also
		     * -kuntal 9/13/98
		     */
		    /*
		     * check for overlap:
		     * for any shape of region 'p' things are fine as long as
		     * 'region' is above or below it
		     */
		    if (ple < rls || pls > rle)
			continue;
		    /*
		     * for EXACT 'p' region
		     */
		    if ( p->ar_shape == EXACT ) {
			if ( ple == rls && poe-1 < ros )
				continue;
			if ( pls == rle && pos > roe )
				continue;
		    }
		    /*
		     * for RECTANGLE 'p' region
		     */
		    if ( p->ar_shape == RECTANGLE )
			if (poe < ros || pos > roe)
			    continue;

		    /*
		     * FIXME: this removes the whole of an overlapping region;
		     * we really only want to remove the overlapping portion...
		     */

		    /*
		     * we take care of this fix easily as long as neither of
		     * 'p' or 'region' are RECTANGLE. we will need to create
		     * at the most one new region in case 'region' is
		     * completely contained within 'p'
		     */
		    if (p->ar_shape != RECTANGLE && regionshape != RECTANGLE) {
			if ((rls > pls) || (rls == pls && ros > pos)) {
			    p->ar_shape = EXACT;
			    if ((rle < ple) || (rle == ple && roe < poe)) {
				/* open a new region */
				if ((n = typealloc(AREGION)) == NULL) {
				    (void)no_memory("AREGION");
				    return FALSE;
				}
				n->ar_region = p->ar_region;
				n->ar_vattr  = p->ar_vattr;
				n->ar_shape  = p->ar_shape;
#if OPT_HYPERTEXT
				n->ar_hypercmd = p->ar_hypercmd;
#endif
				n->ar_region.r_orig.l=(region.r_end.l);
				n->ar_region.r_orig.o=(region.r_end.o);
				attach_attrib(curbp, n);
			    }
			    p->ar_region.r_end.l = (region.r_orig.l);
			    p->ar_region.r_end.o = (region.r_orig.o);
			    /*
			    if (p->ar_region.r_orig.o == 0 &&
				p->ar_region.r_end.o
				  == w_left_margin(curwp) ) {
				p->ar_shape == FULLLINE;
			    }
			    if (n->ar_region.r_orig.o == 0 &&
				n->ar_region.r_end.o
				  == w_left_margin(curwp) ) {
				n->ar_shape == FULLLINE;
			    }
			    */
			    curwp->w_flag |= WFHARD;
			    continue;
			} else if ((rle < ple) || (rle == ple && roe < poe)) {
			    p->ar_region.r_orig.l = (region.r_end.l);
			    p->ar_region.r_orig.o = (region.r_end.o);
			    curwp->w_flag |= WFHARD;
			    continue;
			}
		    }

		    detach_attrib(curbp, p);
		}
	    }
	}
	return status;
}

int
attributeregion_in_region(REGION *rp,
			  REGIONSHAPE shape,
			  VIDEO_ATTR vattr,
			  char *hc)
{
    haveregion = rp;
    DOT =  rp->r_orig;
    MK = rp->r_end;
    if (shape == FULLLINE)
	MK.l = lback(MK.l);
    regionshape = shape;	/* Not that the following actually cares */
    videoattribute = vattr;
#if OPT_HYPERTEXT
    hypercmd = hc;
#endif /* OPT_HYPERTEXT */
    return attributeregion();
}

int
operselect(int f, int n)
{
	int s;
	opcmd = OPOTHER;
	doingopselect = TRUE;
	s = vile_op(f,n,selectregion,"Select");
	doingopselect = FALSE;
	return s;
}

int
operattrbold(int f, int n)
{
      opcmd = OPOTHER;
      videoattribute = VABOLD | VOWN_OPERS;
      return vile_op(f,n,attributeregion,"Set bold attribute");
}

int
operattrital(int f, int n)
{
      opcmd = OPOTHER;
      videoattribute = VAITAL | VOWN_OPERS;
      return vile_op(f,n,attributeregion,"Set italic attribute");
}

int
operattrno(int f, int n)
{
      opcmd = OPOTHER;
      videoattribute = 0;	/* clears no matter who "owns" */
      return vile_op(f,n,attributeregion,"Set normal attribute");
}

int
operattrrev(int f, int n)
{
      opcmd = OPOTHER;
      videoattribute = VAREV | VOWN_OPERS;
      return vile_op(f,n,attributeregion,"Set reverse attribute");
}

int
operattrul(int f, int n)
{
      opcmd = OPOTHER;
      videoattribute = VAUL | VOWN_OPERS;
      return vile_op(f,n,attributeregion,"Set underline attribute");
}

#if OPT_HYPERTEXT
static int
attributehyperregion(void)
{
    char line[NLINE];
    int  status;

    line[0] = 0;
    status = mlreply_no_opts("Hypertext Command: ", line, NLINE);

    if (status != TRUE)
	return status;

    hypercmd = strmalloc(line);
    return attributeregion();
}

int
operattrhc(int f, int n)
{
    opcmd = OPOTHER;
    videoattribute = 0;
    return vile_op(f,n,attributehyperregion,"Set hypertext command");
}

static int
hyperspray(int (*f)(char *))
{
    L_NUM    dlno;
    int      doff;
    AREGION *p;
    int count = 0;

    (void) bsizes(curbp);		/* attach line numbers to each line */

    dlno = DOT.l->l_number;
    doff = DOT.o;

    for (p = curbp->b_attribs; p != 0; p = p->ar_next) {
	if (p->ar_hypercmd) {
	    int slno, elno, soff, eoff;

	    slno = p->ar_region.r_orig.l->l_number;
	    elno = p->ar_region.r_end.l->l_number;
	    soff = p->ar_region.r_orig.o;
	    eoff = p->ar_region.r_end.o;

	    if (   ((slno == dlno && doff >= soff) || dlno > slno)
		&& ((elno == dlno && doff <  eoff) || dlno < elno) )
	    {
		f(p->ar_hypercmd);
		count++;
		/* As originally written, there was no break below.
		        This means that we'd loop over all of the
			attributes in case there were multiple
			overlapping attributes with attached hypertext
			commands.  As cool as this may sound, it is
			actually of very dubious utility, and an
			action which removes some or all of the
			attributes very quickly gets this loop into
			trouble.  So, if this functionality is really
			desired, we'll have to either make a copy of
			the attributes or somehow otherwise determine
			that they were modified or deleted.  */
		break;
	    }
	}
    }
    return count;
}

static int
doexechypercmd(char *cmd)
{
    return docmd(cmd,TRUE,FALSE,1);
}

int
exechypercmd(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int count;
    count = hyperspray(doexechypercmd);
    return count != 0;
}

static int
doshowhypercmd(char *cmd)
{
    mlforce("%s",cmd);
    return 1;
}

int
showhypercmd(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int count;
    count = hyperspray(doshowhypercmd);
    return count != 0;
}
#endif

int
operattrcaseq(int f, int n)
{
      opcmd = OPOTHER;
      videoattribute = VAUL | VOWN_CTLA;
      return vile_op(f,n,attribute_cntl_a_sequences,
		      "Attribute ^A sequences");
}

int
attribute_cntl_a_seqs_in_region(REGION *rp, REGIONSHAPE shape)
{
    haveregion = rp;
    DOT =  rp->r_orig;
    MK = rp->r_end;
    if (shape == FULLLINE)
	MK.l = lback(MK.l);
    regionshape = shape;	/* Not that the following actually cares */
    return attribute_cntl_a_sequences();
}

/*
 * attribute_cntl_a_sequences can take quite a while when processing a region
 * with a large number of attributes.  The reason for this is that the number
 * of marks to check for fixing (from ldelete) increases with each attribute
 * that is added.  It is not really necessary to check the attributes that
 * we are adding in attribute_cntl_a_sequences due to the order in which
 * they are added (i.e, none of them ever need to be fixed up when ldelete
 * is called from within attribute_cntl_a_sequences).
 *
 * It is still necessary to update those attributes which existed (if any)
 * prior to calling attribute_cntl_a_sequences.
 *
 * We define EFFICIENCY_HACK to be 1 if we want to enable the code which
 * will prevent ldelete from doing unnecessary work.  Note that we are
 * depending on the fact that attach_attrib() adds new attributes to the
 * beginning of the list.  It is for this reason that I consider this
 * code to be somewhat hacky.
 */
#define EFFICIENCY_HACK 1

static int
attribute_cntl_a_sequences(void)
{
    register int c;		/* current char during scan */
    register LINEPTR pastline;	/* pointer to line just past EOP */
    C_NUM offset;		/* offset in cur line of place to attribute */

#if EFFICIENCY_HACK
    AREGION *orig_attribs;
    AREGION *new_attribs;
    orig_attribs = new_attribs = curbp->b_attribs;
#endif

    if (!sameline(MK, DOT)) {
	REGION region;
	if (getregion(&region) != TRUE)
	    return FALSE;
	if (sameline(region.r_orig, MK))
	    swapmark();
    }
    pastline = MK.l;
    if (pastline != win_head(curwp))
	    pastline = lforw(pastline);
    DOT.o = 0;
    regionshape = EXACT;
    while (DOT.l != pastline) {
	while (DOT.o < llength(DOT.l)) {
	    if (char_at(DOT) == CONTROL_A) {
		int count = 0;
		offset = DOT.o+1;
start_scan:
		while (offset < llength(DOT.l)) {
		    c = lgetc(DOT.l, offset);
		    if (isDigit(c)) {
			count = count * 10 + c - '0';
			offset++;
		    }
		    else
			break;
		}
		if (count == 0)
		    count = 1;
		videoattribute = VOWN_CTLA;
		while (offset < llength(DOT.l)) {
		    c = lgetc(DOT.l, offset);
		    switch (c) {
			case 'C' :
			    /* We have color. Get color value */
			    offset++;
			    c = lgetc(DOT.l, offset);
			    if (isDigit(c))
				videoattribute |= VCOLORATTR(c - '0');
			    else if ('A' <= c && c <= 'F')
				videoattribute |= VCOLORATTR(c - 'A' + 10);
			    else if ('a' <= c && c <= 'f')
				videoattribute |= VCOLORATTR(c - 'a' + 10);
			    else
				offset--; /* Invalid attribute */
			    break;
			case 'U' : videoattribute |= VAUL;   break;
			case 'B' : videoattribute |= VABOLD; break;
			case 'R' : videoattribute |= VAREV;  break;
			case 'I' : videoattribute |= VAITAL; break;
#if OPT_HYPERTEXT
			case 'H' : {
			    int save_offset = offset;
			    offset++;
			    while (offset < llength(DOT.l)
				&& lgetc(DOT.l,offset) != 0)
				offset++;
			    if (offset < llength(DOT.l)) {
				hypercmd = strmalloc(&DOT.l->l_text[save_offset+1]);
			    }
			    else {
				/* Bad hypertext string... skip it */
				offset = save_offset;
			    }

			    break;
			}
#endif
			case ':' : offset++;
			    if (offset < llength(DOT.l)
			     && lgetc(DOT.l, offset) == CONTROL_A) {
				count = 0;
				offset++;
				goto start_scan; /* recover from filter-err */
			    }
			    /* FALLTHROUGH */
			default  : goto attribute_found;
		    }
		    offset++;
		}
attribute_found:
#if EFFICIENCY_HACK
		new_attribs = curbp->b_attribs;
		curbp->b_attribs = orig_attribs;
		ldelete((B_COUNT)(offset - DOT.o), FALSE);
		curbp->b_attribs = new_attribs;
#else
		ldelete((B_COUNT)(offset - DOT.o), FALSE);
#endif
		MK = DOT;
		MK.o += count;
		if (MK.o > llength(DOT.l))
		    MK.o = llength(DOT.l);
		if (VATTRIB(videoattribute) || hypercmd != 0)
		    (void) attributeregion();
	    }
	    DOT.o++;
	}
	DOT.l = lforw(DOT.l);
	DOT.o = 0;
    }
    return TRUE;
}
#endif /* OPT_SELECTIONS */
