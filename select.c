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
 * mechanism is general enough to be used for other kinds of persistent
 * attributed text.
 *
 * For the purposes of this code, a selection is considered to be a region of
 * text which most applications highlight in some manner.  The user may
 * transfer selected text from one application to another (or even to the
 * same application) in some platform dependent way.  The mechanics of
 * transferring the selection are not dealt with in this file.  Procedures
 * for dealing with the representation are maintained in this file.
 *
 * $Id: select.c,v 1.196 2025/01/26 14:38:58 tom Exp $
 */

#include	"estruct.h"
#include	"edef.h"
#include	"nefunc.h"

#if OPT_FILTER
#include	<filters.h>
#endif

#define BTN_BEGIN   1
#define BTN_PASTE   2
#define BTN_EXTEND  3

#define SEL_BEGIN   10		/* button 1 up */
#define SEL_PASTE   11		/* button 2 up */
#define SEL_EXTEND  12		/* button 3 up */
#define SEL_RELEASE 13		/* click on modeline */
#define SEL_FINISH  14		/* finish a selection */

#if OPT_SELECTIONS

static void detach_attrib(BUFFER *bp, AREGION * arp);
static int attribute_cntl_a_sequences(void);
static void free_line_attribs(BUFFER *bp);
static int add_line_attrib(BUFFER *bp, REGION * rp, REGIONSHAPE rs,
			   unsigned vattr, TBUFF *hypercmd);
static void purge_line_attribs(BUFFER *bp, REGION * rp, REGIONSHAPE rs,
			       int owner);

#define mark_buffers_windows(bp) \
	{ \
	    WINDOW *wp; \
	    for_each_visible_window(wp) { \
		if (wp->w_bufp == bp) \
		    wp->w_flag |= WFHARD; \
	    } \
	}

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
 * When startbufp or selbufp are NULL, the corresponding AREGION (startregion or
 * selregion) is not attached to any buffer and is invalid.
 */

static MARK orig_region;
static MARK plus_region;
static BUFFER *startbufp = NULL;
static AREGION startregion;
static BUFFER *selbufp = NULL;
static AREGION selregion;
#if OPT_HYPERTEXT
static TBUFF *hypercmd;
#endif

typedef enum {
    ORIG_FIXED, END_FIXED, UNFIXED
} WHICHEND;

static WHICHEND whichend;

static AREGION *
alloc_AREGION(void)
{
    AREGION *arp;

    beginDisplay();
    if ((arp = typealloc(AREGION)) == NULL) {
	(void) no_memory("AREGION");
    }
    endofDisplay();
    return arp;
}

void
free_attribs(BUFFER *bp)
{
    AREGION *p, *q;

    beginDisplay();
    p = bp->b_attribs;
    while (p != NULL) {
	q = p->ar_next;
#if OPT_HYPERTEXT
	FreeAndNull(p->ar_hypercmd);
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

    free_line_attribs(bp);
    endofDisplay();
}

void
free_attrib2(BUFFER *bp, AREGION ** rpp)
{
    AREGION *ap = *rpp;
    AREGION *next = ap->ar_next;

    /* The caller already has found the right value for 'rpp',
     * so there is no need to call detach_attrib() to find it.
     */
    mark_buffers_windows(bp);
    ap->ar_region.r_attr_id = 0;

    beginDisplay();
#if OPT_HYPERTEXT
    FreeAndNull(ap->ar_hypercmd);
#endif
    if (ap == &selregion)
	selbufp = NULL;
    else if (ap == &startregion)
	startbufp = NULL;
    else
	free((char *) ap);
    endofDisplay();

    /* finally, delink the region */
    *rpp = next;
}

static void
detach_attrib(BUFFER *bp, AREGION * arp)
{
    if (find_bp(bp) != NULL) {
	if (valid_buffer(bp)) {
	    AREGION **rpp;
	    mark_buffers_windows(bp);
	    rpp = &bp->b_attribs;
	    while (*rpp != NULL) {
		if (*rpp == arp) {
		    *rpp = (*rpp)->ar_next;
		    arp->ar_region.r_attr_id = 0;
		    break;
		} else
		    rpp = &(*rpp)->ar_next;
	    }
	}
    }
}

void
find_release_attr(BUFFER *bp, REGION * rp)
{
    if (valid_buffer(bp)) {
	AREGION **rpp = &bp->b_attribs;
	AREGION *ap;

	while ((ap = *rpp) != NULL) {
	    if (ap->ar_region.r_attr_id == rp->r_attr_id) {
		free_attrib2(bp, rpp);
		break;
	    } else
		rpp = &ap->ar_next;
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
attach_attrib(BUFFER *bp, AREGION * arp)
{
    if (valid_buffer(bp)) {
	arp->ar_next = bp->b_attribs;
	bp->b_attribs = arp;
	mark_buffers_windows(bp);
	arp->ar_region.r_attr_id = (USHORT) assign_attr_id();
    }
}

/*
 * Adjusts dot to last char of last line if dot is past end of buffer.  This
 * can happen when selecting with the mouse.
 */

static void
fix_dot(void)
{
    if (curwp != NULL && is_header_line(DOT, curwp->w_bufp)) {
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
    show_mark_is_set('<');
    show_mark_is_set('>');
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
	startregion.ar_region.r_end = DOT;
    plus_region.o += BytesAt(plus_region.l, plus_region.o);
    startregion.ar_vattr = 0;
    startregion.ar_shape = rgn_EXACT;
#if OPT_HYPERTEXT
    startregion.ar_hypercmd = NULL;
#endif
    startbufp = curwp->w_bufp;
    attach_attrib(startbufp, &startregion);
    whichend = UNFIXED;
    return TRUE;
}

static int
dot_vs_mark(void)
{
    int cmp = line_no(curbp, DOT.l) - line_no(curbp, MK.l);
    if (cmp == 0)
	cmp = DOT.o - MK.o;
    return cmp;
}

/* Extend the current selection to dot */
int
sel_extend(int wiping, int include_dot)
{
    BUFFER *bp = curbp;
    REGIONSHAPE save_shape = regionshape;
    REGION a, b;
    MARK saved_dot;
    MARK working_dot;

    saved_dot = DOT;
    if (valid_buffer(startbufp)) {
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
	&& (selregion.ar_shape == rgn_EXACT)
	&& dot_vs_mark() >= 0) {
	if (samepoint(MK, orig_region)) {
	    DOT.o += BytesAt(DOT.l, DOT.o);
	} else if (samepoint(MK, plus_region)) {
	    DOT.o += BytesAt(DOT.l, DOT.o);
	    MK = orig_region;
	}
    }
    if (getregion(bp, &a) == FALSE) {
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
	if (selregion.ar_shape == rgn_EXACT) {
	    if (dot_vs_mark() <= 0) {
		if (samepoint(MK, orig_region))
		    MK.o += BytesAt(MK.l, MK.o);
	    }
	} else if (selregion.ar_shape == rgn_RECTANGLE) {
	    if (samepoint(MK, DOT)) {	/* avoid making empty-region */
		MK = orig_region;
		DOT = plus_region;
	    }
	}
    }
    if (getregion(bp, &b) == FALSE) {
	return FALSE;
    }

    /*
     * The two regions, 'a' and 'b' are _usually_ identical, except for the
     * special case where we've extended one to the right to include the
     * right endpoint of the region.
     *
     * For rgn_EXACT selections, setting 'whichend' to ORIG_FIXED means that
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
    } else {
	if (selregion.ar_shape == rgn_RECTANGLE) {
	    if (dot_vs_mark() < 0)
		whichend = END_FIXED;
	    else
		whichend = ORIG_FIXED;
	} else {		/* exact or full-line */
	    whichend = END_FIXED;
	}
	selregion.ar_region = b;
    }

    selregion.ar_vattr = VASEL | VOWN_SELECT;
    mark_buffers_windows(selbufp);

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
    BUFFER *save_bp = curbp;
    int code = FALSE;

    TRACE((T_CALLED "sel_yank(%d)\n", reg));
    if (valid_window(save_wp = push_fake_win(selbufp))) {
	/*
	 * We're not guaranteed that curbp and selbufp are the same.
	 */
	save_shape = regionshape;

	curbp = selbufp;

	ukb = (short) reg;
	kregflag = 0;
	haveregion = &selregion.ar_region;
	regionshape = selregion.ar_shape;
	yankregion();
	haveregion = NULL;

#ifdef GMDCBRD_ECHO
	if (global_g_val(GMDCBRD_ECHO))
	    cbrdcpy_unnamed(FALSE, FALSE);
#endif

	pop_fake_win(save_wp, save_bp);

	regionshape = save_shape;

	show_selection_position(TRUE);

	/* put cursor back on screen...is there a cheaper way to do this?  */
	(void) update(FALSE);
	code = TRUE;
    }
    returnCode(code);
}

/* select all text in curbp and yank to unnamed register */
int
sel_all(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int rc;
    MARK savedot;

    savedot = DOT;
    gotobob(0, 0);
    sel_begin();
    gotoeob(0, 0);
    gotoeol(0, 0);
    (void) sel_setshape(rgn_EXACT);
    rc = sel_extend(TRUE, TRUE);
    DOT = savedot;
    if (rc)
	sel_yank(0);
    return (rc);
}
#endif /* OPT_SEL_YANK */

BUFFER *
sel_buffer(void)
{
    return valid_buffer(startbufp) ? startbufp : selbufp;
}

static int
get_selregion(REGION * result)
{
    if (valid_buffer(startbufp)) {
	*result = startregion.ar_region;
	return TRUE;
    } else if (valid_buffer(selbufp)) {
	*result = selregion.ar_region;
	return TRUE;
    } else {
	return FALSE;
    }
}

int
sel_get_leftmark(MARK *result)
{
    REGION my_region;
    if (get_selregion(&my_region)) {
	*result = my_region.r_orig;
	return TRUE;
    }
    return FALSE;
}

int
sel_get_rightmark(MARK *result)
{
    REGION my_region;
    if (get_selregion(&my_region)) {
	*result = my_region.r_end;
	return TRUE;
    }
    return FALSE;
}

int
sel_setshape(REGIONSHAPE shape)
{
    if (valid_buffer(startbufp)) {
	startregion.ar_shape = shape;
	return TRUE;
    } else if (valid_buffer(selbufp)) {
	selregion.ar_shape = shape;
	return TRUE;
    } else {
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
    BUFFER *bp = curbp;
    REGION *rp = NULL;
    static REGION a, b;
    MARK savemark;

    savemark = MK;
    regionshape = selregion.ar_shape;
    MK = selregion.ar_region.r_orig;
    DOT.o += BytesAt(DOT.l, DOT.o);
    if (getregion(bp, &a) == TRUE) {
	DOT.o -= BytesBefore(DOT.l, DOT.o);
	MK = selregion.ar_region.r_end;
	if (regionshape == rgn_FULLLINE)
	    MK.l = lback(MK.l);
	/* region b is to the end of the selection */
	if (getregion(bp, &b) == TRUE) {
	    /* if a is bigger, it's the one we want */
	    if (a.r_size > b.r_size)
		rp = &a;
	    else
		rp = &b;
	}
    } else {
	DOT.o -= BytesBefore(DOT.l, DOT.o);
    }
    MK = savemark;
    return rp;
}

static int doingopselect;

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
	pre_op_dot = selregion.ar_region.r_orig;	/* move us there */
	haveregion = &selregion.ar_region;
	regionshape = selregion.ar_shape;
	return TRUE;
    }

    if (!doingopcmd) {		/* it's a simple motion -- go to the top of selection */
	/* remember -- this can never be used with an operator, as in
	 * "delete to the selection", since that case is taken care
	 * of above, and is really the whole reason for this
	 * "motion" in the first place.  */
	DOT = selregion.ar_region.r_orig;	/* move us there */
	return TRUE;
    }

    /* we must be doing an extension */
    haveregion = extended_region();
    return haveregion ? TRUE : FALSE;

}

static int
selectregion(void)
{
    BUFFER *bp = curbp;
    int status;
    REGION region;
    MARK savedot;
    MARK savemark;
    int hadregion = FALSE;

    savedot = DOT;
    savemark = MK;
    memset(&region, 0, sizeof(region));
    if (haveregion) {		/* getregion() will clear this, so
				   we need to save it */
	region = *haveregion;
	hadregion = TRUE;
    }
    status = yankregion();
    DOT = savedot;
    MK = savemark;
    if (status != TRUE)
	return status;
    if (hadregion || ((status = getregion(bp, &region)) == TRUE)) {
	detach_attrib(startbufp, &startregion);
	detach_attrib(selbufp, &selregion);
	selbufp = curbp;
	selregion.ar_region = region;
	selregion.ar_vattr = VASEL | VOWN_SELECT;
	selregion.ar_shape = regionshape;
#if OPT_HYPERTEXT
	selregion.ar_hypercmd = NULL;
#endif
	attach_attrib(selbufp, &selregion);
	OWN_SELECTION();
    }
    return status;
}

void
do_sweep(int flag)
{
    if (doingsweep != flag) {
	if ((flag && !doingsweep)
	    || (!flag && doingsweep))
	    term.cursorvis(!flag);
	doingsweep = flag;
    }
}

static void
sweepmsg(const char *msg)
{
    char temp[NLINE];
    (void) kcod2pstr(fnc2kcod(&f_multimotion), temp, (int) sizeof(temp));
    mlforce("[%s (end with %.*s)]", msg, CharOf(*temp), temp + 1);
}

static int
release_selection(int status)
{
    TRACE(("MOUSE release selection\n"));
    if (doingsweep) {
	do_sweep(FALSE);
	if (status != TRUE)
	    mlforce("[Sweeping: Aborted]");
	else
	    mlerase();
    }
    sel_release();
    return status;
}

#if OPT_MOUSE
int
paste_selection(void)
{
    if (!doingsweep) {
	TRACE(("MOUSE paste selection\n"));
	if (b_val(curbp, MDVIEW)) {
	    kbd_alarm();
	    return FALSE;
	}
	mayneedundo();
	return putafter(FALSE, 1);
    }
    return SEL_PASTE;
}

void
on_double_click(void)
{
    MARK save;

    save = DOT;
    TRACE(("MOUSE double-click DOT %d.%d\n", line_no(curbp, DOT.l), DOT.o));
    sel_release();
    if (!is_at_end_of_line(DOT)
	&& !isSpace(CharAtDot())) {
	while (DOT.o > 0) {
	    DOT.o -= BytesBefore(DOT.l, DOT.o);
	    if (isSpace(CharAtDot())) {
		DOT.o += BytesAt(DOT.l, DOT.o);
		break;
	    }
	}
	sel_begin();
	MK = DOT;
	while (!is_at_end_of_line(DOT)) {
	    DOT.o += BytesAt(DOT.l, DOT.o);
	    if (is_at_end_of_line(DOT)
		|| isSpace(CharAtDot())) {
		DOT.o -= BytesBefore(DOT.l, DOT.o);
		break;
	    }
	}
	sel_extend(FALSE, TRUE);
    }
    DOT = save;
    update(TRUE);
}

void
on_triple_click(void)
{
    MARK save;

    save = DOT;
    TRACE(("MOUSE triple-click DOT %d.%d\n", line_no(curbp, DOT.l), DOT.o));
    sel_release();
    gotobol(FALSE, 1);
    sel_begin();
    MK = DOT;
    gotoeol(FALSE, 1);
    sel_extend(FALSE, TRUE);
    DOT = save;
    update(TRUE);
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
	if (valid_window(this_wp = row2window(y))
	    && (y != mode_row(this_wp))) {
	    /*
	     * If we get a click on the "<" marking the left side
	     * of a shifted window, force the screen right. This
	     * makes it more consistent if there's a tab.
	     */
	    if (w_val(this_wp, WVAL_SIDEWAYS)
		&& x == 0) {
		mvleftwind(FALSE, 1);
	    }
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
	} else {		/* pressed button on modeline */
	    status = SORTOFTRUE;
	    first_x = x;
	    first_y = y;
	}
	pending = button;
    } else if (pending) {
	button = pending;
	pending = FALSE;
	this_wp = row2window(y);
	that_wp = row2window(first_y);
	if (this_wp == NULL
	    || that_wp == NULL
	    || reading_msg_line) {
	    TRACE(("MOUSE cannot move msg-line\n"));
	    status = FALSE;
	} else if (insertmode
		   && (this_wp != curwp || that_wp != curwp)) {
	    TRACE(("MOUSE cannot move from window while inserting\n"));
	    kbd_alarm();
	    status = ABORT;
	} else if (first_y == mode_row(that_wp)) {	/* drag modeline? */
	    if (first_y == y) {
		sel_release();
		status = SEL_RELEASE;
	    } else {
		WINDOW *save_wp = curwp;
		TRACE(("MOUSE dragging modeline\n"));
		set_curwp(that_wp);
		status = shrinkwind(FALSE, first_y - y);
		set_curwp(save_wp);
	    }
	} else if (y != first_y || x != first_x) {	/* drag selection */
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
			       line_no(curbp, MK.l), MK.o));
		    }
		    break;
		default:
		    (void) setcursor(y, x);
		    status = SEL_EXTEND;
		    TRACE(("MOUSE setting SEL_EXTEND DOT %d.%d MK %d.%d\n",
			   line_no(curbp, MK.l), MK.o,
			   line_no(curbp, DOT.l), DOT.o));
		    break;
		}
	    } else {
		TRACE(("MOUSE begin multimotion on button%d-up\n", button));
		if (button == BTN_EXTEND) {
		    (void) setcursor(y, x);
		    y = first_y;
		    x = first_x;
		}
		do_sweep(SORTOFTRUE);
		(void) sel_begin();
		(void) sel_setshape(rgn_EXACT);
		(void) setcursor(y, x);
		status = multimotion(TRUE, 1);
		TRACE(("MOUSE end multimotion after button%d-up\n", button));
		if (status == SEL_PASTE)
		    status = paste_selection();
	    }
	} else {		/* position the cursor */
	    TRACE(("MOUSE button %d position cursor\n", button));
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
	TRACE(("MOUSE ignored (illegal state)\n"));
	status = FALSE;
    }

    if (status == TRUE || status >= SORTOFTRUE)
	(void) update(TRUE);

    TRACE(("MOUSE status:%d\n", status));
    return status;
}
#endif

int
multimotion(int f, int n)
{
    const CMDFUNC *cfp;
    int status, c, waserr;
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
    n = need_at_least(f, n, 1);

    if (n == 3)
	regionshape = rgn_RECTANGLE;
    else if (n == 2)
	regionshape = rgn_FULLLINE;
    else
	regionshape = rgn_EXACT;
    shape = regionshape;

    sweephack = FALSE;
    savedot = DOT;
    switch (doingsweep) {
    case TRUE:			/* the same command terminates as starts the sweep */
	if (doingsweep) {
	    do_sweep(FALSE);
	}
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
	if (doingsweep != TRUE) {
	    do_sweep(TRUE);
	}
	sweepmsg("Begin cursor sweep...");
	sel_extend(TRUE, (regionshape != rgn_RECTANGLE && sweephack));
	savedot = MK;
	TRACE(("MOUSE BEGIN DOT: %d.%d MK %d.%d\n",
	       line_no(curbp, DOT.l), DOT.o,
	       line_no(curbp, MK.l), MK.o));
	break;
    case FALSE:
	if (doingsweep != TRUE) {
	    do_sweep(TRUE);
	}
	sweepmsg("Begin cursor sweep...");
	(void) sel_begin();
	(void) sel_setshape(shape);
	break;
    }

    waserr = TRUE;		/* to force message "state-machine" */
    realdot = DOT;
    pasting = FALSE;

    while (doingsweep) {

	/* Fix up the screen    */
	(void) update(FALSE);

	/* get the next command from the keyboard */
	c = kbd_seq();

	if (ABORTED(c)
	    || curbp != origbp) {
	    return release_selection(FALSE);
	}

	f = FALSE;
	n = 1;

	do_repeats(&c, &f, &n);

	/* and execute the command */
	cfp = SelectKeyBinding(c);
	if ((cfp != NULL)
	    && ((cfp->c_flags & (GOAL | MOTION)) != 0)) {
	    MARK testdot;

	    wassweephack = sweephack;
	    sweephack = FALSE;
	    TRACE(("MOUSE TEST DOT: %d.%d MK %d.%d\n",
		   line_no(curbp, DOT.l), DOT.o,
		   line_no(curbp, MK.l), MK.o));
	    testdot = DOT;

	    status = execute(cfp, f, n);
	    switch (status) {
	    case SEL_RELEASE:
		TRACE(("MOUSE SEL_RELEASE %d.%d\n",
		       line_no(curbp, DOT.l), DOT.o));
		return release_selection(TRUE);

	    case SEL_PASTE:
		pasting = TRUE;
		/* FALLTHRU */

	    case SEL_FINISH:
		do_sweep(FALSE);
		break;

	    case SORTOFTRUE:
		TRACE(("MOUSE selection pending %d.%d -> %d.%d\n",
		       line_no(curbp, realdot.l), realdot.o,
		       line_no(curbp, testdot.l), testdot.o));
		realdot = testdot;
		break;

	    case SEL_BEGIN:
		savedot = MK;
		TRACE(("MOUSE SEL_BEGIN...\n"));
		/*FALLTHRU */

	    case SEL_EXTEND:
		TRACE(("MOUSE SEL_EXTEND from %d.%d to %d.%d\n",
		       line_no(curbp, savedot.l), savedot.o,
		       line_no(curbp, DOT.l), DOT.o));
		/*FALLTHRU */

	    case TRUE:
		if (waserr && doingsweep) {
		    sweepmsg("Sweeping...");
		    waserr = FALSE;
		}
		realdot = DOT;
		DOT = savedot;
		(void) sel_begin();
		DOT = realdot;
		TRACE(("MOUSE LOOP save: %d.%d real %d.%d, mark %d.%d\n",
		       line_no(curbp, savedot.l), savedot.o,
		       line_no(curbp, realdot.l), realdot.o,
		       line_no(curbp, MK.l), MK.o));
		(void) sel_setshape(shape);
		/* we sometimes want to include DOT.o in the
		   selection (unless it's a rectangle, in
		   which case it's taken care of elsewhere)
		 */
		sel_extend(TRUE, (regionshape != rgn_RECTANGLE &&
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
	   line_no(curbp, MK.l), MK.o));
    if ((regionshape != rgn_RECTANGLE) && sweephack) {
	if (dot_vs_mark() < 0)
	    MK.o += BytesAt(MK.l, MK.o);
	else
	    DOT.o += BytesAt(DOT.l, DOT.o);
    }
    status = yankregion();
    DOT = savedot;
    MK = savemark;

    sweephack = wassweephack = FALSE;

    if (status == TRUE && pasting)
	status = SEL_PASTE;

    return status;
}

/*ARGSUSED*/
int
multimotionfullline(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return multimotion(TRUE, 2);
}

/*ARGSUSED*/
int
multimotionrectangle(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return multimotion(TRUE, 3);
}

#if OPT_PERL || OPT_TCL || SYS_WINNT
BUFFER *
get_selection_buffer_and_region(AREGION * arp)
{
    if (selbufp) {
	*arp = selregion;
    }
    return selbufp;
}
#endif

int
apply_attribute(void)
{
    return (VATTRIB(videoattribute) != 0
#if OPT_HYPERTEXT
	    || tb_length(hypercmd) != 0
#endif
	);
}

int
attributeregion(void)
{
    BUFFER *bp = curbp;
    int status;
    REGION region;
    AREGION *arp;

    if ((status = getregion(bp, &region)) == TRUE) {
	if (apply_attribute()) {
	    if (add_line_attrib(bp, &region, regionshape, videoattribute,
#if OPT_HYPERTEXT
				hypercmd
#else
				0
#endif
		)) {
		return TRUE;
	    }

	    /* add new attribute-region */
	    if ((arp = alloc_AREGION()) == NULL) {
		return FALSE;
	    }
	    arp->ar_region = region;
	    arp->ar_vattr = videoattribute;	/* include ownership */
	    arp->ar_shape = regionshape;
#if OPT_HYPERTEXT
	    arp->ar_hypercmd = NULL;
	    if (tb_length(hypercmd) && *tb_values(hypercmd)) {
#if OPT_EXTRA_COLOR
		if (tb_length(hypercmd) > 4
		    && !memcmp(tb_values(hypercmd), "view ", (size_t) 4)) {
		    int *newVideo = lookup_extra_color(XCOLOR_HYPERTEXT);
		    if (!isEmpty(newVideo)) {
			arp->ar_vattr = (VIDEO_ATTR) * newVideo;
		    }
		}
#endif
		arp->ar_hypercmd = strmalloc(tb_values(hypercmd));
		tb_init(&hypercmd, 0);
	    }
#endif
	    attach_attrib(bp, arp);
	} else {		/* purge attributes in this region */
	    L_NUM rls = line_no(bp, region.r_orig.l);
	    L_NUM rle = line_no(bp, region.r_end.l);
	    C_NUM ros = region.r_orig.o;
	    C_NUM roe = region.r_end.o;
	    AREGION **pp;
	    AREGION **qq;
	    AREGION *p, *n;
	    int owner;

	    owner = VOWNER(videoattribute);

	    purge_line_attribs(bp, &region, regionshape, owner);

	    pp = &(bp->b_attribs);

	    for (p = *pp; p != NULL; pp = qq, p = *pp) {
		L_NUM pls, ple;
		C_NUM pos, poe;

		if (interrupted())
		    return FALSE;

		qq = &(p->ar_next);

		if (owner != 0 && owner != VOWNER(p->ar_vattr))
		    continue;

		pls = line_no(bp, p->ar_region.r_orig.l);
		ple = line_no(bp, p->ar_region.r_end.l);
		pos = p->ar_region.r_orig.o;
		poe = p->ar_region.r_end.o;

		/* Earlier the overlapping region check was made based only
		 * on line numbers and so was right only for FULLINES shape
		 * changed it to be correct for rgn_EXACT and rgn_RECTANGLE also
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
		 * for rgn_EXACT 'p' region
		 */
		if (p->ar_shape == rgn_EXACT) {
		    if (ple == rls && poe - 1 < ros)
			continue;
		    if (pls == rle && pos > roe)
			continue;
		}
		/*
		 * for rgn_RECTANGLE 'p' region
		 */
		if (p->ar_shape == rgn_RECTANGLE)
		    if (poe < ros || pos > roe)
			continue;

		/*
		 * FIXME: this removes the whole of an overlapping region;
		 * we really only want to remove the overlapping portion...
		 */

		/*
		 * we take care of this fix easily as long as neither of
		 * 'p' or 'region' are rgn_RECTANGLE. we will need to create
		 * at the most one new region in case 'region' is
		 * completely contained within 'p'
		 */
		if (p->ar_shape != rgn_RECTANGLE && regionshape != rgn_RECTANGLE) {
		    if ((rls > pls) || (rls == pls && ros > pos)) {
			p->ar_shape = rgn_EXACT;
			if ((rle < ple) || (rle == ple && roe < poe)) {
			    /* open a new region */
			    if ((n = alloc_AREGION()) == NULL) {
				return FALSE;
			    }
			    n->ar_region = p->ar_region;
			    n->ar_vattr = p->ar_vattr;
			    n->ar_shape = p->ar_shape;
#if OPT_HYPERTEXT
			    n->ar_hypercmd = p->ar_hypercmd;
#endif
			    n->ar_region.r_orig.l = (region.r_end.l);
			    n->ar_region.r_orig.o = (region.r_end.o);
			    attach_attrib(bp, n);
			}
			p->ar_region.r_end.l = (region.r_orig.l);
			p->ar_region.r_end.o = (region.r_orig.o);
			curwp->w_flag |= WFHARD;
			continue;
		    } else if ((rle < ple) || (rle == ple && roe < poe)) {
			p->ar_region.r_orig.l = (region.r_end.l);
			p->ar_region.r_orig.o = (region.r_end.o);
			curwp->w_flag |= WFHARD;
			continue;
		    }
		}

		free_attrib2(bp, pp);
		qq = pp;
	    }
	}
    }
    return status;
}

int
attributeregion_in_region(REGION * rp,
			  REGIONSHAPE shape,
			  unsigned vattr,
			  char *hc)
{
    haveregion = rp;
    DOT = rp->r_orig;
    MK = rp->r_end;
    if (shape == rgn_FULLLINE)
	MK.l = lback(MK.l);
    regionshape = shape;	/* Not that the following actually cares */
    videoattribute = (VIDEO_ATTR) vattr;
#if OPT_HYPERTEXT
    tb_scopy(&hypercmd, hc);
#endif /* OPT_HYPERTEXT */
    return attributeregion();
}

int
operselect(int f, int n)
{
    int status;
    opcmd = OPOTHER;
    doingopselect = TRUE;
    status = vile_op(f, n, selectregion, "Select");
    doingopselect = FALSE;
    return status;
}

int
operattrbold(int f, int n)
{
    opcmd = OPOTHER;
    videoattribute = VABOLD | VOWN_OPERS;
    return vile_op(f, n, attributeregion, "Set bold attribute");
}

int
operattrital(int f, int n)
{
    opcmd = OPOTHER;
    videoattribute = VAITAL | VOWN_OPERS;
    return vile_op(f, n, attributeregion, "Set italic attribute");
}

int
operattrno(int f, int n)
{
    opcmd = OPOTHER;
    videoattribute = 0;		/* clears no matter who "owns" */
    return vile_op(f, n, attributeregion, "Set normal attribute");
}

int
operattrrev(int f, int n)
{
    opcmd = OPOTHER;
    videoattribute = VAREV | VOWN_OPERS;
    return vile_op(f, n, attributeregion, "Set reverse attribute");
}

int
operattrul(int f, int n)
{
    opcmd = OPOTHER;
    videoattribute = VAUL | VOWN_OPERS;
    return vile_op(f, n, attributeregion, "Set underline attribute");
}

#if OPT_HYPERTEXT
static int
attributehyperregion(void)
{
    char line[NLINE];
    int status;

    line[0] = 0;
    status = mlreply_no_opts("Hypertext Command: ", line, NLINE);

    if (status != TRUE)
	return status;

    tb_scopy(&hypercmd, line);
    return attributeregion();
}

int
operattrhc(int f, int n)
{
    opcmd = OPOTHER;
    videoattribute = 0;
    return vile_op(f, n, attributehyperregion, "Set hypertext command");
}

static int
hyperspray(int (*f) (char *))
{
    L_NUM dlno;
    int doff;
    AREGION *p;
    int count = 0;

    (void) bsizes(curbp);	/* attach line numbers to each line */

    dlno = DOT.l->l_number;
    doff = DOT.o;

    for (p = curbp->b_attribs; p != NULL; p = p->ar_next) {
	if (p->ar_hypercmd) {
	    int slno, elno, soff, eoff;

	    slno = p->ar_region.r_orig.l->l_number;
	    elno = p->ar_region.r_end.l->l_number;
	    soff = p->ar_region.r_orig.o;
	    eoff = p->ar_region.r_end.o;

	    if (((slno == dlno && doff >= soff) || dlno > slno)
		&& ((elno == dlno && doff < eoff) || dlno < elno)) {
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
    return docmd(cmd, TRUE, FALSE, 1);
}

/*ARGSUSED*/
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
    mlforce("%s", cmd);
    return 1;
}

/*ARGSUSED*/
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
    videoattribute = 0;
    return vile_op(f, n, attribute_cntl_a_sequences,
		   "Attribute ^A sequences");
}

int
attribute_cntl_a_seqs_in_region(REGION * rp, REGIONSHAPE shape)
{
    haveregion = rp;
    DOT = rp->r_orig;
    MK = rp->r_end;
    if (shape == rgn_FULLLINE)
	MK.l = lback(MK.l);
    regionshape = shape;	/* Not that the following actually cares */
    return attribute_cntl_a_sequences();
}

/*
 * Setup for iteration over region to attribute, ensure that DOT < MK.
 */
LINE *
setup_region(void)
{
    BUFFER *bp = curbp;
    LINE *pastline;		/* pointer to line just past EOP */

    if (!sameline(MK, DOT)) {
	REGION region;
	if (getregion(bp, &region) != TRUE)
	    return NULL;
	if (sameline(region.r_orig, MK))
	    swapmark();
    }
    pastline = MK.l;
    if (pastline != win_head(curwp))
	pastline = lforw(pastline);
    DOT.o = b_left_margin(curbp);
    regionshape = rgn_EXACT;

    return pastline;
}

/*
 * Parse a cntl_a sequence, returning the number of characters processed.
 * Set videoattribute and hypercmd as side-effects.
 */
int
decode_attribute(char *text, size_t length, size_t offset, int *countp)
{
    int c;			/* current char during scan */
    int count = 0;
    int found;
#if OPT_HYPERTEXT
    size_t save_offset;
#endif

    while (text[offset] == CONTROL_A) {
	found = FALSE;
	count = 0;
	offset++;
	while (offset < length) {
	    c = text[offset];
	    if (isDigit(c)) {
		count = count * 10 + c - '0';
		offset++;
	    } else
		break;
	}
	if (count == 0)
	    count = 1;
	videoattribute = VOWN_CTLA;
	while (offset < length
	       && (c = text[offset]) != CONTROL_A
	       && !found) {
	    switch (c) {
	    case 'C':
		/* We have color. Get color value */
		offset++;
		c = text[offset];
		if (isDigit(c))
		    videoattribute |= VCOLORATTR(c - '0');
		else if ('A' <= c && c <= 'F')
		    videoattribute |= VCOLORATTR(c - 'A' + 10);
		else if ('a' <= c && c <= 'f')
		    videoattribute |= VCOLORATTR(c - 'a' + 10);
		else
		    offset--;	/* Invalid attribute */
		break;

	    case 'U':
		videoattribute |= VAUL;
		break;
	    case 'B':
		videoattribute |= VABOLD;
		break;
	    case 'R':
		videoattribute |= VAREV;
		break;
	    case 'I':
		videoattribute |= VAITAL;
		break;
#if OPT_HYPERTEXT
	    case 'H':
		save_offset = offset;
		offset++;
		while (offset < length
		       && text[offset] != EOS)
		    offset++;

		if (offset < length) {
		    tb_init(&hypercmd, EOS);
		    tb_bappend(&hypercmd,
			       &text[save_offset + 1],
			       offset - save_offset);
		    tb_append(&hypercmd, EOS);
		} else {	/* skip bad hypertext string */
		    offset = save_offset;
		}
		break;
#endif
	    case 'M':
		if (((offset + 2) == length) && (text[offset + 1] == ':')) {
		    /*
		     * Work around special case in builtflt.c's flt_puts(),
		     * which is sending only the markup code without data.
		     */
		    count = -1;
		    found = TRUE;
		} else {
		    save_offset = offset;
		    offset++;
		    while (offset < length
			   && text[offset] != EOS)
			offset++;

		    if (offset >= length) {
			offset = save_offset;
		    } else {
			TRACE(("flt_meta:%s\n", text + save_offset));
		    }
		}
		break;
	    case ':':
		found = TRUE;
		break;

	    default:
		offset--;
		found = TRUE;
		break;
	    }
	    offset++;
	}
	if (videoattribute != VOWN_CTLA && count != 0)
	    break;
    }
    *countp = count;
    return (int) offset;
}

/*
 * DOT points to the beginning of a region, we're given the count of characters
 * to put into the region.  Set MK at the end.  This handles counts that extend
 * beyond the current line, but makes assumptions about the record separator.
 * Data from an external filter always uses newline for a separator, otherwise
 * we will run into problems with lex/flex.  Internally computed regions are
 * not the same problem.
 */
static void
set_mark_after(int count, int rslen)
{
    int offset = DOT.o;

    MK = DOT;
    while (count > 0) {
	MK.o += count;		/* FIXME? */
	if (is_last_line(MK, curbp)) {
	    count = 0;
	} else if (MK.o > llength(MK.l)) {
	    if (MK.o <= (llength(MK.l) + rslen)) {
		MK.o = llength(MK.l);
		break;
	    }
	    count -= (llength(MK.l) + rslen - offset);
	    MK.l = lforw(MK.l);
	    MK.o = b_left_margin(curbp);
	} else {
	    break;
	}
	offset = 0;
    }
}

/*
 * attribute_cntl_a_sequences can take quite a while when processing a region
 * with a large number of attributes.  The reason for this is that the number
 * of marks to check for fixing (from ldel_bytes) increases with each attribute
 * that is added.  It is not really necessary to check the attributes that
 * we are adding in attribute_cntl_a_sequences due to the order in which
 * they are added (i.e, none of them ever need to be fixed up when ldel_bytes
 * is called from within attribute_cntl_a_sequences).
 *
 * It is still necessary to update those attributes which existed (if any)
 * prior to calling attribute_cntl_a_sequences.
 *
 * We define EFFICIENCY_HACK to be 1 if we want to enable the code which
 * will prevent ldel_bytes from doing unnecessary work.  Note that we are
 * depending on the fact that attach_attrib() adds new attributes to the
 * beginning of the list.  It is for this reason that I consider this
 * code to be somewhat hacky.
 */
#define EFFICIENCY_HACK 1

static int
attribute_cntl_a_sequences(void)
{
    BUFFER *bp = curbp;
    LINE *pastline;
    C_NUM offset;		/* offset in cur line of place to attribute */
    int count;

#if EFFICIENCY_HACK
    AREGION *orig_attribs = bp->b_attribs;
    AREGION *new_attribs;
#endif

    if ((pastline = setup_region()) == NULL)
	return FALSE;

    while (DOT.l != pastline) {
	if (interrupted())
	    return FALSE;
	while (DOT.o < llength(DOT.l)) {
	    if (CharAtDot() == CONTROL_A) {
		offset = decode_attribute(lvalue(DOT.l),
					  (size_t) llength(DOT.l),
					  (size_t) DOT.o, &count);
		if (offset > DOT.o) {
#if EFFICIENCY_HACK
		    new_attribs = bp->b_attribs;
		    bp->b_attribs = orig_attribs;
		    ldel_bytes((B_COUNT) (offset - DOT.o), FALSE);
		    bp->b_attribs = new_attribs;
#else
		    ldel_bytes((B_COUNT) (offset - DOT.o), FALSE);
#endif
		}
		set_mark_after(count, len_record_sep(bp));
		if (apply_attribute())
		    (void) attributeregion();
	    } else {
		DOT.o += BytesAt(DOT.l, DOT.o);
	    }
	}
	dot_next_bol();
    }
    return TRUE;
}

#if OPT_SHELL || OPT_FILTER
static void
discard_syntax_highlighting(void)
{
    detach_attrib(selbufp, &selregion);
    detach_attrib(startbufp, &startregion);
    free_attribs(curbp);
}
#endif

/*
 * Apply attributes from a filtering command on the current buffer.  The
 * buffer is not modified.
 */
#if OPT_SHELL
static int
attribute_from_filter(void)
{
    BUFFER *bp = curbp;
    LINE *pastline;
    int skip;
    size_t nbytes;
    size_t n;
    int done;
    int result = TRUE;
    int drained = FALSE;

    TRACE((T_CALLED "attribute_from_filter\n"));
    if ((pastline = setup_region()) == NULL) {
	result = FALSE;

#ifdef MDHILITE
    } else if (!b_val(bp, MDHILITE)) {
	discard_syntax_highlighting();
#endif

    } else if (open_region_filter() == TRUE) {

	discard_syntax_highlighting();
	while (DOT.l != pastline) {

	    if (interrupted()) {
		result = FALSE;
		break;
	    }

	    if (ffgetline(&nbytes) > FIOSUC) {
		drained = TRUE;
		break;
	    }

	    DOT.o = b_left_margin(curbp);
	    for (n = 0; n < nbytes; n++) {
		if (fflinebuf[n] == CONTROL_A) {
		    done = decode_attribute(fflinebuf, nbytes, n, &skip);
		    if (done) {
			n = ((size_t) done - 1);
			set_mark_after(skip, 1);
			if (apply_attribute())
			    (void) attributeregion();
		    }
		} else {
		    DOT.o += BytesAt(DOT.l, DOT.o);
		}
	    }
	    dot_next_bol();
	}

	/* some pipes will hang if they're not drained */
	if (!drained) {
	    while (ffgetline(&nbytes) <= FIOSUC) {
		;
	    }
	}

	(void) ffclose();	/* Ignore errors.       */
	attach_attrib(selbufp, &selregion);
	attach_attrib(startbufp, &startregion);
#if OPT_HILITEMATCH
	if (bp->b_highlight & HILITE_ON) {
	    bp->b_highlight |= HILITE_DIRTY;
	    attrib_matches();
	}
#endif
    }
    returnCode(result);
}

int
operattrfilter(int f, int n)
{
    opcmd = OPOTHER;
    videoattribute = 0;
    return vile_op(f, n, attribute_from_filter,
		   "Attribute ^A sequences from filter");
}
#endif /*  OPT_SHELL */

#if OPT_FILTER
static int
attribute_directly(void)
{
    BUFFER *bp = curbp;
    int code = FALSE;

#if OPT_MAJORMODE
    if (valid_buffer(bp)) {
#if OPT_AUTOCOLOR
	VL_ELAPSED begin_time;
	(void) vl_elapsed(&begin_time, TRUE);
#endif
	discard_syntax_highlighting();
	if (b_val(bp, MDHILITE)) {
	    char *filtername = NULL;
	    TBUFF *token = NULL;

	    if (clexec || isnamedcmd)
		filtername = mac_unquotedarg(&token);

	    if (filtername == NULL
		&& bp->majr != NULL)
		filtername = bp->majr->shortname;

	    if (filtername != NULL
		&& flt_start(filtername)) {
		TRACE(("attribute_directly(%s) using %s\n",
		       bp->b_bname,
		       filtername));
		flt_finish();
		code = TRUE;
	    }
	    tb_free(&token);
	}
	attach_attrib(selbufp, &selregion);
	attach_attrib(startbufp, &startregion);
#if OPT_HILITEMATCH
	if (bp->b_highlight & HILITE_ON) {
	    bp->b_highlight |= HILITE_DIRTY;
	    attrib_matches();
	}
#endif
#if OPT_AUTOCOLOR
	bp->last_autocolor_time = vl_elapsed(&begin_time, FALSE);
	bp->next_autocolor_time = 0;
#endif
    }
#endif
    return code;
}

int
operattrdirect(int f, int n)
{
    opcmd = OPOTHER;
    videoattribute = 0;
    return vile_op(f, n, attribute_directly,
		   "Attribute directly with internal filter");
}
#endif

#if OPT_LINE_ATTRS

#define INIT_LINE_ATTR_TBL() \
    if (!line_attr_tbl[0].in_use) init_line_attr_tbl()

static void
init_line_attr_tbl(void)
{
    /* Slot 0 indicates no more line attributes */
    line_attr_tbl[0].in_use = TRUE;
    line_attr_tbl[0].vattr = 0;
    /* Slot 1 indicates a normal attribute */
    line_attr_tbl[1].in_use = TRUE;
    line_attr_tbl[1].vattr = 0;
}

/* Find an index in line_attr_tbl[] containing the specified attribute.
   Add the attribute to the table if not found.  Return -1 if table is
   full. (Kevin's note: I don't think the table full condition will
   be a real problem.  But if it is, it should be possible to garbage
   collect the table.) */
static int
find_line_attr_idx(unsigned vattr)
{
    int hash = 0;
    int start;
    UINT i;
    VIDEO_ATTR v;

    INIT_LINE_ATTR_TBL();

    if (vattr == 0)
	return 1;		/* Normal attributes get mapped to index 1 */

    v = (VIDEO_ATTR) vattr;
    for (i = 0; i < sizeof(VIDEO_ATTR); i++) {
	hash ^= v & 0xff;
	v >>= 8;
    }

    start = hash;
    while (line_attr_tbl[hash].in_use) {

	if (line_attr_tbl[hash].vattr == vattr)
	    return hash;

	hash++;
	if (hash == start)
	    return -1;

	if (hash >= N_chars)
	    hash = 2;		/* No point starting at 0, since we know
				   that 0 and 1 must be in use. */
    }

    line_attr_tbl[hash].vattr = (VIDEO_ATTR) vattr;
    line_attr_tbl[hash].in_use = TRUE;

    return hash;
}

/* Attempt to shift a portion of a line either left or right for
 * inserts or deletes.  The idea is to preserve the line attributes
 * as much as possible until autocolor gets around to recoloring the
 * line.
 *
 * Returns false if we ran out of memory.
 */
/* ARGSUSED */
int
lattr_shift(BUFFER *bp GCC_UNUSED, LINE *lp, int doto, int shift)
{
    int status = TRUE;
    UCHAR *lap;

    if (lp->l_attrs != NULL) {
	lap = lp->l_attrs;
	if (shift > 0) {
	    int f, t, len;
	    len = (int) strlen((char *) lap);
	    t = len - 1;
	    if (t > 0) {
		for (f = t; f >= doto && f > t - shift; f--) {
		    if (lap[f] != 1) {
			int newlen;

			beginDisplay();
			newlen = len + shift - (t - f);
			safe_castrealloc(UCHAR, lap, (size_t) newlen + 1);
			if (lap != NULL) {
			    lp->l_attrs = lap;
			    lap[newlen] = 0;
			    t = newlen - 1;
			    f = t - shift;
			}
			endofDisplay();
			break;
		    }
		}
		if (lap != NULL) {
		    while (f > doto) {
			lap[t--] = lap[f--];
		    }
		} else {
		    no_memory("lattr_shift");
		    status = FALSE;
		}
	    }
	} else if (shift < 0) {
	    int f, t;
	    int saw_attr = 0;
	    shift = -shift;
	    /* Move t to doto, but don't run off end */
	    for (t = 0; t < doto && lap[t]; t++)
		saw_attr |= (lap[t] != 1);
	    if (lap[t] != 0) {
		/* Position f, but don't run off end */
		for (f = t; f < doto + shift && lap[f]; f++) ;
		/* Shift via copying, but observe what it is we shift */
		while (lap[f]) {
		    saw_attr |= (lap[f] != 1);
		    lap[t++] = lap[f++];
		}
		/* Try to get rid of the line attributes entirely */
		if (!saw_attr) {
		    beginDisplay();
		    FreeAndNull(lp->l_attrs);
		    endofDisplay();
		} else {
		    /* Normal out the stuff at the end. */
		    while (t < f)
			lap[t++] = 1;
		}
	    }
	}
    }
    return status;
}

#endif /* OPT_LINE_ATTRS */

static void
free_line_attribs(BUFFER *bp)
{
#if OPT_LINE_ATTRS
    LINE *lp;
    int do_update = 0;
    for_each_line(lp, bp) {
	do_update |= (lp->l_attrs != NULL);
	FreeAndNull(lp->l_attrs);
    }
    if (do_update) {
	mark_buffers_windows(bp);
    }
#endif /* OPT_LINE_ATTRS */
}

static int
add_line_attrib(BUFFER *bp, REGION * rp, REGIONSHAPE rs, unsigned vattr,
		TBUFF *hypercmdp)
{
#if OPT_LINE_ATTRS
    LINE *lp;
    int vidx;
    int i;
    int overlap = FALSE;
    int last;

    if (rp->r_orig.l != rp->r_end.l	/* must be confined to one line */
	|| rs != rgn_EXACT	/* must be an exact region */
	|| (hypercmdp && tb_length(hypercmdp) != 0)
    /* can't be a hypertext command */
	|| vattr == 0		/* can't be normal */
	|| ((UCHAR) vattr) != vattr
	|| (vattr & VASEL) != 0) {	/* can't be a selection */
	return FALSE;
    }

    beginDisplay();

    lp = rp->r_orig.l;
    last = rp->r_end.o;

    if (lp->l_attrs != NULL) {
	int len = (int) strlen((char *) (lp->l_attrs));
	/* Make sure the line attribute is long enough */
	if (len < rp->r_end.o) {
	    last = rp->r_end.o;
	    safe_castrealloc(UCHAR, lp->l_attrs, (size_t) last + 1);
	    if (lp->l_attrs != NULL) {
		for (i = len; i < rp->r_end.o; i++)
		    lp->l_attrs[i] = 1;
		lp->l_attrs[i] = 0;
	    }
	}
	/* See if attributed region we're about to add overlaps an existing
	   line based one */
	if (lp->l_attrs != NULL) {
	    for (i = rp->r_orig.o; i < last; i++) {
		if (lp->l_attrs[i] != 1) {
		    overlap = TRUE;
		    break;
		}
	    }
	}
    } else {
	/* Must allocate and initialize memory for the line attributes */
	beginDisplay();
	lp->l_attrs = castalloc(UCHAR, (size_t) llength(lp) + 1);
	if (lp->l_attrs != NULL) {
	    lp->l_attrs[llength(lp)] = 0;
	    for (i = llength(lp) - 1; i >= 0; i--)
		lp->l_attrs[i] = 1;
	    if (last > llength(lp))
		last = llength(lp);
	}
	endofDisplay();
    }
    endofDisplay();

    if (lp->l_attrs != NULL && !overlap) {
	if ((vidx = find_line_attr_idx(vattr)) >= 0) {
	    for (i = rp->r_orig.o; i < last; i++)
		lp->l_attrs[i] = (UCHAR) vidx;

	    mark_buffers_windows(bp);
	    return TRUE;
	}
    }
#endif /* OPT_LINE_ATTRS */
    return FALSE;
}

static void
purge_line_attribs(BUFFER *bp, REGION * rp, REGIONSHAPE rs, int owner)
{
#if OPT_LINE_ATTRS
    LINE *ls = rp->r_orig.l;
    LINE *le = rp->r_end.l;
    int os = rp->r_orig.o;
    int oe = rp->r_end.o;
    LINE *lp;
    int i;
    int do_update = 0;

    for (lp = ls; lp != buf_head(curbp); lp = lforw(lp)) {
	if (lp->l_attrs != NULL) {
	    for (i = 0; i < llength(lp); i++) {
		if (lp->l_attrs[i] == 0)
		    break;	/* at end of attrs */
		if (lp->l_attrs[i] == 1)
		    continue;	/* normal, so proceed to next one */
		if (rs != rgn_FULLLINE) {
		    if ((rs == rgn_RECTANGLE || lp == ls) && i < os)
			continue;
		    if ((rs == rgn_RECTANGLE || lp == le) && i >= oe)
			break;
		}
		if (owner == 0
		    || owner == VOWNER(line_attr_tbl[lp->l_attrs[i]].vattr)) {
		    /* If we get here, set it back to normal */
		    lp->l_attrs[i] = 1;
		    do_update = 1;
		}
	    }
	}
	if (lp == le)
	    break;
    }
    if (do_update) {
	mark_buffers_windows(bp);
    }
#endif /* OPT_LINE_ATTRS */
}

/* release selection, if any */
int
sel_clear(int f GCC_UNUSED, int n GCC_UNUSED)
{
    sel_release();
    return (TRUE);
}

#endif /* OPT_SELECTIONS */
