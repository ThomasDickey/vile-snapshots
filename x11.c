/*
 *	X11 support, Dave Lemke, 11/91
 *	X Toolkit support, Kevin Buettner, 2/94
 *
 * $Id: x11.c,v 1.436 2025/01/26 21:38:08 tom Exp $
 */

/*
 * Widget set selection.
 *
 * You must have exactly one of the following defined
 *
 *    NO_WIDGETS	-- Use only Xlib and X toolkit (Xt)
 *    ATHENA_WIDGETS	-- Use Xlib, Xt, and Xaw widget set
 *    MOTIF_WIDGETS	-- Use Xlib, Xt, and Motif widget set
 *
 * We derive/set from the configure script some flags that allow intermediate
 * configurations between NO_WIDGETS and ATHENA_WIDGETS
 *
 *    KEV_WIDGETS
 *    OPT_KEV_DRAGGING
 *    OPT_KEV_SCROLLBARS
 */

#include <x11vile.h>

#ifndef GCC_NORETURN
#ifdef _X_NORETURN
#define GCC_NORETURN		_X_NORETURN
#else
#define GCC_NORETURN		/* nothing */
#endif
#endif

#if DISP_X11 && XTOOLKIT

static Display *dpy;
static ENC_CHOICES term_encoding = enc_DEFAULT;

#if OPT_MULTIBYTE
static ENC_CHOICES font_encoding = enc_DEFAULT;
#endif

static TextWindowRec cur_win_rec;
static TextWindow cur_win = &cur_win_rec;
static TBUFF *PasteBuf;

#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
static Cursor curs_sb_v_double_arrow;
static Cursor curs_sb_up_arrow;
static Cursor curs_sb_down_arrow;
static Cursor curs_sb_left_arrow;
static Cursor curs_sb_right_arrow;
static Cursor curs_double_arrow;
#endif

#if MOTIF_WIDGETS
static Bool lookfor_sb_resize = FALSE;
#endif

struct eventqueue {
    XEvent event;
    struct eventqueue *next;
};

static struct eventqueue *evqhead = NULL;
static struct eventqueue *evqtail = NULL;

static void x_flush(void);
static void x_beep(void);

#if OPT_COLOR
static void x_set_cursor_color(int);
#else
#define	x_set_foreground	nullterm_setfore
#define	x_set_background	nullterm_setback
#define	x_set_cursor_color	nullterm_setccol
#endif

static int set_character_class(const char *s);
static void x_touch(TextWindow tw, int sc, int sr, UINT ec, UINT er);
static void x_own_selection(Atom selection);
static Boolean x_get_selected_text(UCHAR ** datp, size_t *lenp);
static void extend_selection(TextWindow tw, int nr, int nc, Bool wipe);
static void x_process_event(Widget w, XtPointer unused, XEvent *ev,
			    Boolean *continue_to_dispatch);
static void x_configure_window(Widget w, XtPointer unused, XEvent *ev,
			       Boolean *continue_to_dispatch);
static void x_change_focus(Widget w, XtPointer unused, XEvent *ev,
			   Boolean *continue_to_dispatch);
static void x_typahead_timeout(XtPointer flagp, XtIntervalId * id);
static void x_key_press(Widget w, XtPointer unused, XEvent *ev,
			Boolean *continue_to_dispatch);
static void x_wm_delwin(Widget w, XtPointer unused, XEvent *ev,
			Boolean *continue_to_dispatch);
#if OPT_COLOR&&!SMALLER
static void x_start_autocolor_timer(void);
static void x_autocolor_timeout(XtPointer flagp, XtIntervalId * id);
static void x_stop_autocolor_timer(void);
#else
#define x_start_autocolor_timer()	/* nothing */
#define x_stop_autocolor_timer()	/* nothing */
#endif
#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
static Boolean too_light_or_too_dark(Pixel pixel);
#endif
#if OPT_KEV_SCROLLBARS
static Boolean alloc_shadows(Pixel pixel, Pixel *light, Pixel *dark);
#endif
static void configure_bar(Widget w, XEvent *event, String *params,
			  Cardinal *num_params);
static int check_scrollbar_allocs(void);
static void kqinit(TextWindow tw);
static int kqempty(TextWindow tw);
static int kqfull(TextWindow tw);
static void kqadd(TextWindow tw, int c);
static int kqpop(TextWindow tw);
static void display_cursor(XtPointer client_data, XtIntervalId * idp);
#if MOTIF_WIDGETS
static void pane_button(Widget w, XtPointer unused, XEvent *ev,
			Boolean *continue_to_dispatch);
#endif /* MOTIF_WIDGETS */
#if OPT_KEV_SCROLLBARS
static void x_expose_scrollbar(Widget w, XtPointer unused, XEvent *ev,
			       Boolean *continue_to_dispatch);
#endif /* OPT_KEV_SCROLLBARS */
#if OPT_KEV_DRAGGING
static void repeat_scroll(XtPointer count, XtIntervalId * id);
#endif
#if OPT_INPUT_METHOD
static void init_xlocale(void);
#endif
#if OPT_WORKING
static void x_set_watch_cursor(int onflag);
static int x_has_events(void);
#else
#define x_has_events() (XtAppPending(cur_win->app_context) & XtIMXEvent)
#endif /* OPT_WORKING */
static void evqadd(const XEvent *evp);

#define	x_width(tw)		((tw)->cols * (UINT) (tw)->char_width)
#define	x_height(tw)		((tw)->rows * (UINT) (tw)->char_height)
#define	x_pos(tw, c)		((c) * (tw)->char_width)
#define	y_pos(tw, r)		((r) * (tw)->char_height)
#define	text_y_pos(tw, r)	(y_pos((tw), (r)) + (tw)->char_ascent)

#undef min
#define min(a,b)		((a) < (b) ? (a) : (b))

#undef max
#define max(a,b)		((a) > (b) ? (a) : (b))

#if KEV_WIDGETS
/* We define our own little bulletin board widget here...if this gets
 * too unwieldy, we should move it to another file.
 */

#include	<X11/IntrinsicP.h>

/* New fields for the Bb widget class record */
typedef struct {
    int empty;
} BbClassPart;

/* Class record declaration */
typedef struct _BbClassRec {
    CoreClassPart core_class;
    CompositeClassPart composite_class;
    BbClassPart bb_class;
} BbClassRec;

extern BbClassRec bbClassRec;

/* Instance declaration */
typedef struct _BbRec {
    CorePart core;
    CompositePart composite;
} BbRec;

static XtGeometryResult bbGeometryManager(Widget w, XtWidgetGeometry *
					  request, XtWidgetGeometry * reply);
static XtGeometryResult bbPreferredSize(Widget widget, XtWidgetGeometry *
					constraint, XtWidgetGeometry * preferred);

BbClassRec bbClassRec =
{
    {
/* core_class fields      */
    /* superclass         */ (WidgetClass) & compositeClassRec,
    /* class_name         */ "Bb",
    /* widget_size        */ sizeof(BbRec),
    /* class_initialize   */ NULL,
    /* class_part_init    */ NULL,
    /* class_inited       */ FALSE,
    /* initialize         */ NULL,
    /* initialize_hook    */ NULL,
    /* realize            */ XtInheritRealize,
    /* actions            */ NULL,
    /* num_actions        */ 0,
    /* resources          */ NULL,
    /* num_resources      */ 0,
    /* xrm_class          */ NULLQUARK,
    /* compress_motion    */ TRUE,
    /* compress_exposure  */ TRUE,
    /* compress_enterleave */ TRUE,
    /* visible_interest   */ FALSE,
    /* destroy            */ NULL,
    /* resize             */ XtInheritResize,
    /* expose             */ NULL,
    /* set_values         */ NULL,
    /* set_values_hook    */ NULL,
    /* set_values_almost  */ XtInheritSetValuesAlmost,
    /* get_values_hook    */ NULL,
    /* accept_focus       */ NULL,
    /* version            */ XtVersion,
    /* callback_private   */ NULL,
    /* tm_table           */ NULL,
							/* query_geometry     */ bbPreferredSize,
							/*XtInheritQueryGeometry, */
    /* display_accelerator */ XtInheritDisplayAccelerator,
    /* extension          */ NULL
    },
    {
/* composite_class fields */
    /* geometry_manager   */ bbGeometryManager,
    /* change_managed     */ XtInheritChangeManaged,
    /* insert_child       */ XtInheritInsertChild,
    /* delete_child       */ XtInheritDeleteChild,
    /* extension          */ NULL
    },
    {
/* Bb class fields */
    /* empty              */ 0,
    }
};

static WidgetClass bbWidgetClass = (WidgetClass) & bbClassRec;

/*ARGSUSED*/
static XtGeometryResult
bbPreferredSize(Widget widget GCC_UNUSED,
		XtWidgetGeometry * constraint GCC_UNUSED,
		XtWidgetGeometry * preferred GCC_UNUSED)
{
    return XtGeometryYes;
}

/*ARGSUSED*/
static XtGeometryResult
bbGeometryManager(Widget w,
		  XtWidgetGeometry * request,
		  XtWidgetGeometry * reply GCC_UNUSED)
{
    /* Allow any and all changes to the geometry */
    if (request->request_mode & CWWidth)
	w->core.width = request->width;
    if (request->request_mode & CWHeight)
	w->core.height = request->height;
    if (request->request_mode & CWBorderWidth)
	w->core.border_width = request->border_width;
    if (request->request_mode & CWX)
	w->core.x = request->x;
    if (request->request_mode & CWY)
	w->core.y = request->y;

    return XtGeometryYes;
}

#endif /* KEV_WIDGETS */

static void
set_pointer(Window win, Cursor cursor)
{
    XColor colordefs[2];	/* 0 is foreground, 1 is background */

    XDefineCursor(dpy, win, cursor);

    colordefs[0].pixel = cur_win->pointer_fg;
    colordefs[1].pixel = cur_win->pointer_bg;
    XQueryColors(dpy, DefaultColormap(dpy, DefaultScreen(dpy)),
		 colordefs, 2);
    XRecolorCursor(dpy, cursor, colordefs, colordefs + 1);
}

#if !OPT_KEV_SCROLLBARS && !OPT_XAW_SCROLLBARS
static int dont_update_sb = FALSE;
#endif

#if !OPT_KEV_DRAGGING
static void
set_scroll_window(long n)
{
    WINDOW *wp;
    for_each_visible_window(wp) {
	if (n-- == 0)
	    break;
    }
    if (n < 0)
	set_curwp(wp);
}
#endif /* !OPT_KEV_DRAGGING */

#if MOTIF_WIDGETS

static void
JumpProc(Widget scrollbar GCC_UNUSED,
	 XtPointer closure,
	 XtPointer call_data)
{
    long value = (long) closure;
    int lcur;

    lookfor_sb_resize = FALSE;
    if (value >= cur_win->nscrollbars)
	return;
    set_scroll_window(value);
    lcur = line_no(curwp->w_bufp, curwp->w_line.l);
    mvupwind(TRUE, lcur - ((XmScrollBarCallbackStruct *) call_data)->value);
    dont_update_sb = TRUE;
    (void) update(TRUE);
    dont_update_sb = FALSE;
}

static void
grip_moved(Widget w GCC_UNUSED,
	   XtPointer unused GCC_UNUSED,
	   XEvent *ev GCC_UNUSED,
	   Boolean *continue_to_dispatch GCC_UNUSED)
{
    int i;
    WINDOW *wp, *saved_curwp;
    Dimension height;
    int lines;

    if (!lookfor_sb_resize)
	return;
    lookfor_sb_resize = FALSE;
    saved_curwp = curwp;

    i = 0;
    for_each_visible_window(wp) {
	XtVaGetValues(cur_win->scrollbars[i],
		      XtNheight, &height,
		      NULL);
	lines = (height + (cur_win->char_height / 2)) / cur_win->char_height;
	if (lines <= 0) {
	    lines = 1;
	}
	curwp = wp;
	resize(TRUE, lines);
	i++;
    }
    set_curwp(saved_curwp);
    (void) update(TRUE);
}

static void
update_scrollbar_sizes(void)
{
    WINDOW *wp;
    int newsbcnt;
    Dimension new_height;
    Widget *children;
    int num_children;
    long i = 0;

    for_each_visible_window(wp)
	i++;
    newsbcnt = (int) i;

    /* Remove event handlers on sashes */
    XtVaGetValues(cur_win->pane,
		  XmNchildren, &children,
		  XmNnumChildren, &num_children,
		  NULL);
    while (num_children-- > 0)
	if (XmIsSash(children[num_children]))
	    XtRemoveEventHandler(children[num_children],
				 ButtonReleaseMask,
				 FALSE,
				 pane_button,
				 NULL);

    /* Create any needed new scrollbars */
    for (i = cur_win->nscrollbars + 1; i <= newsbcnt; i++)
	if (cur_win->scrollbars[i] == NULL) {
	    cur_win->scrollbars[i] =
		XtVaCreateWidget("scrollbar",
				 xmScrollBarWidgetClass,
				 cur_win->pane,
				 Nval(XmNsliderSize, 1),
				 Nval(XmNvalue, 1),
				 Nval(XmNminimum, 1),
				 Nval(XmNmaximum, 2),	/* so we don't get warning */
				 Nval(XmNorientation, XmVERTICAL),
				 Nval(XmNtranslations, cur_win->my_scrollbars_trans),
				 NULL);
	    XtAddCallback(cur_win->scrollbars[i],
			  XmNvalueChangedCallback, JumpProc, (XtPointer) i);
	    XtAddCallback(cur_win->scrollbars[i],
			  XmNdragCallback, JumpProc, (XtPointer) i);
	    XtAddEventHandler(cur_win->scrollbars[i],
			      StructureNotifyMask,
			      FALSE,
			      grip_moved,
			      (XtPointer) i);
	}

    /* Unmanage current set of scrollbars */
    if (cur_win->nscrollbars >= 0)
	XtUnmanageChildren(cur_win->scrollbars,
			   (Cardinal) (cur_win->nscrollbars + 1));

    /* Set sizes on scrollbars */
    cur_win->nscrollbars = newsbcnt;
    i = 0;
    for_each_visible_window(wp) {
	new_height = (Dimension) (wp->w_ntrows * cur_win->char_height);
	XtVaSetValues(cur_win->scrollbars[i],
		      Nval(XmNallowResize, TRUE),
		      Nval(XmNheight, new_height),
		      Nval(XmNpaneMinimum, 1),
		      Nval(XmNpaneMaximum, DisplayHeight(dpy, DefaultScreen(dpy))),
		      Nval(XmNshowArrows, wp->w_ntrows > 3 ? TRUE : FALSE),
		      NULL);
	wp->w_flag &= (USHORT) (~WFSBAR);
	gui_update_scrollbar(wp);
	i++;
    }
    XtVaSetValues(cur_win->scrollbars[i],
		  Nval(XmNheight, cur_win->char_height - 1),
		  Nval(XmNallowResize, FALSE),
		  Nval(XmNpaneMinimum, cur_win->char_height - 1),
		  Nval(XmNpaneMaximum, cur_win->char_height - 1),
		  Nval(XmNshowArrows, FALSE),
		  NULL);

    /* Manage the current set of scrollbars */
    XtManageChildren(cur_win->scrollbars,
		     (Cardinal) (cur_win->nscrollbars + 1));

    /* Add event handlers for sashes */
    XtVaGetValues(cur_win->pane,
		  XmNchildren, &children,
		  XmNnumChildren, &num_children,
		  NULL);
    while (num_children-- > 0)
	if (XmIsSash(children[num_children]))
	    XtAddEventHandler(children[num_children],
			      ButtonReleaseMask,
			      FALSE,
			      pane_button,
			      NULL);
}

#else
#if OPT_XAW_SCROLLBARS

#if !OPT_KEV_DRAGGING
static void
JumpProc(Widget scrollbar GCC_UNUSED,
	 XtPointer closure,
	 XtPointer call_data)
{
    L_NUM lcur, lmax;
    long value = (long) closure;
    float *percent = (float *) call_data;

    if (value < cur_win->nscrollbars) {
	set_scroll_window(value);
	lcur = line_no(curwp->w_bufp, curwp->w_line.l);
	lmax = vl_line_count(curwp->w_bufp);
	mvupwind(TRUE, (int) ((float) lcur - (float) lmax * (*percent)));
	(void) update(TRUE);
    }
}

static void
ScrollProc(Widget scrollbar GCC_UNUSED,
	   XtPointer closure,
	   XtPointer call_data)
{
    long value = (long) closure;
    long position = (long) call_data;

    if (value < cur_win->nscrollbars) {
	set_scroll_window(value);
	mvupwind(TRUE, -(position / cur_win->char_height));
	(void) update(TRUE);
    }
}
#endif

static void
update_scrollbar_sizes(void)
{
    WINDOW *wp;
    int i, newsbcnt;
    Dimension new_height;

    i = 0;
    for_each_visible_window(wp)
	i++;
    newsbcnt = i;

    /* Create any needed new scrollbars and grips */
    for (i = cur_win->nscrollbars + 1; i <= newsbcnt; i++) {
	if (cur_win->scrollbars[i] == NULL) {
	    cur_win->scrollbars[i] =
		XtVaCreateWidget("scrollbar",
				 scrollbarWidgetClass,
				 cur_win->pane,
				 Nval(XtNforeground, cur_win->scrollbar_fg),
				 Nval(XtNbackground, cur_win->scrollbar_bg),
				 Nval(XtNthumb, cur_win->thumb_bm),
				 Nval(XtNtranslations, cur_win->my_scrollbars_trans),
				 NULL);
#if !OPT_KEV_DRAGGING
	    XtAddCallback(cur_win->scrollbars[i],
			  XtNjumpProc, JumpProc, (XtPointer) (intptr_t) i);
	    XtAddCallback(cur_win->scrollbars[i],
			  XtNscrollProc, ScrollProc, (XtPointer) (intptr_t) i);
#endif
	}
    }
    for (i = newsbcnt - 2; i >= 0 && cur_win->grips[i] == NULL; i--) {
	cur_win->grips[i] =
	    XtVaCreateWidget("resizeGrip",
			     gripWidgetClass,
			     cur_win->pane,
			     Nval(XtNbackground, cur_win->modeline_bg),
			     Nval(XtNborderWidth, 0),
			     Nval(XtNheight, 1),
			     Nval(XtNwidth, 1),
			     Sval(XtNleft, ((cur_win->scrollbar_on_left)
					    ? XtChainLeft
					    : XtChainRight)),
			     Sval(XtNright, ((cur_win->scrollbar_on_left)
					     ? XtChainLeft
					     : XtChainRight)),
			     Nval(XtNtranslations, cur_win->my_resizeGrip_trans),
			     NULL);
    }

    /* Unmanage current set of scrollbars */
    if (cur_win->nscrollbars > 0)
	XtUnmanageChildren(cur_win->scrollbars,
			   (Cardinal) (cur_win->nscrollbars));

    /* Set sizes and positions on scrollbars and grips */
    cur_win->nscrollbars = newsbcnt;
    i = 0;
    for_each_visible_window(wp) {
	L_NUM total = vl_line_count(curwp->w_bufp);
	L_NUM thumb = line_no(curwp->w_bufp, curwp->w_line.l);

	new_height = (Dimension) (wp->w_ntrows * cur_win->char_height);
	cur_win->scrollinfo[i].totlen = new_height;
	XtVaSetValues(cur_win->scrollbars[i],
		      Nval(XtNy, wp->w_toprow * cur_win->char_height),
		      Nval(XtNheight, new_height),
		      Nval(XtNwidth, cur_win->pane_width - 1),
		      Nval(XtNorientation, XtorientVertical),
		      Nval(XtNvertDistance, wp->w_toprow * cur_win->char_height),
		      Nval(XtNhorizDistance, 1),
		      NULL);
	XawScrollbarSetThumb(cur_win->scrollbars[i],
			     ((float) (thumb - 1)) / (float) max(total, 1),
			     ((float) wp->w_ntrows) / (float) max(total, wp->w_ntrows));
	clr_typed_flags(wp->w_flag, USHORT, WFSBAR);
	gui_update_scrollbar(wp);
	if (wp->w_wndp) {
	    XtVaSetValues(cur_win->grips[i],
			  Nval(XtNx, 1),
			  Nval(XtNy, ((wp->w_wndp->w_toprow - 1)
				      * cur_win->char_height) + 2),
			  Nval(XtNheight, cur_win->char_height - 3),
			  Nval(XtNwidth, cur_win->pane_width - 1),
			  Nval(XtNvertDistance, ((wp->w_wndp->w_toprow - 1)
						 * cur_win->char_height) + 2),
			  Nval(XtNhorizDistance, 1),
			  NULL);
	}
	i++;
    }

    /* Manage the current set of scrollbars */
    XtManageChildren(cur_win->scrollbars,
		     (Cardinal) (cur_win->nscrollbars));

    XtManageChildren(cur_win->grips,
		     (Cardinal) (cur_win->nscrollbars - 1));

    for (i = 0; i < cur_win->nscrollbars; i++)
	XRaiseWindow(dpy, XtWindow(cur_win->scrollbars[i]));

    for (i = 1; i < cur_win->nscrollbars; i++)
	XRaiseWindow(dpy, XtWindow(cur_win->grips[i - 1]));
}

#else
#if OPT_KEV_SCROLLBARS
static void
update_scrollbar_sizes(void)
{
    WINDOW *wp;
    int i, newsbcnt;
    Dimension new_height;
    Cardinal nchildren;

    i = 0;
    for_each_visible_window(wp)
	i++;
    newsbcnt = i;

    /* Create any needed new scrollbars and grips */
    for (i = newsbcnt - 1; i >= 0 && cur_win->scrollbars[i] == NULL; i--) {
	if (cur_win->slider_is_3D) {
	    cur_win->scrollbars[i] =
		XtVaCreateWidget("scrollbar",
				 coreWidgetClass,
				 cur_win->pane,
				 Nval(XtNbackgroundPixmap, cur_win->trough_pixmap),
				 Nval(XtNborderWidth, 0),
				 Nval(XtNheight, 1),
				 Nval(XtNwidth, 1),
				 Nval(XtNtranslations, cur_win->my_scrollbars_trans),
				 NULL);
	} else {
	    cur_win->scrollbars[i] =
		XtVaCreateWidget("scrollbar",
				 coreWidgetClass,
				 cur_win->pane,
				 Nval(XtNbackground, cur_win->scrollbar_bg),
				 Nval(XtNborderWidth, 0),
				 Nval(XtNheight, 1),
				 Nval(XtNwidth, 1),
				 Nval(XtNtranslations, cur_win->my_scrollbars_trans),
				 NULL);
	}

	XtAddEventHandler(cur_win->scrollbars[i],
			  ExposureMask,
			  FALSE,
			  x_expose_scrollbar,
			  (XtPointer) 0);
	cur_win->scrollinfo[i].exposed = False;
    }
    for (i = newsbcnt - 2; i >= 0 && cur_win->grips[i] == NULL; i--)
	cur_win->grips[i] =
	    XtVaCreateWidget("resizeGrip",
			     coreWidgetClass,
			     cur_win->pane,
			     Nval(XtNbackground, cur_win->modeline_bg),
			     Nval(XtNborderWidth, 0),
			     Nval(XtNheight, 1),
			     Nval(XtNwidth, 1),
			     Nval(XtNtranslations, cur_win->my_resizeGrip_trans),
			     NULL);

    /* Set sizes and positions on scrollbars and grips */
    i = 0;
    for_each_visible_window(wp) {
	new_height = (Dimension) (wp->w_ntrows * cur_win->char_height);
	XtVaSetValues(cur_win->scrollbars[i],
		      Nval(XtNx, cur_win->slider_is_3D ? 0 : 1),
		      Nval(XtNy, wp->w_toprow * cur_win->char_height),
		      Nval(XtNheight, new_height),
		      Nval(XtNwidth, (int) cur_win->pane_width
			   + (cur_win->slider_is_3D ? 2 : -1)),
		      NULL);
	cur_win->scrollinfo[i].totlen = new_height;
	if (wp->w_wndp) {
	    XtVaSetValues(cur_win->grips[i],
			  Nval(XtNx, 1),
			  Nval(XtNy, ((wp->w_wndp->w_toprow - 1)
				      * cur_win->char_height) + 2),
			  Nval(XtNheight, cur_win->char_height - 3),
			  Nval(XtNwidth, cur_win->pane_width - 1),
			  NULL);
	}
	i++;
    }

    if (cur_win->nscrollbars > newsbcnt) {
	nchildren = (Cardinal) (cur_win->nscrollbars - newsbcnt);
	XtUnmanageChildren(cur_win->scrollbars + newsbcnt, nchildren);
	XtUnmanageChildren(cur_win->grips + newsbcnt - 1, nchildren);
	for (i = cur_win->nscrollbars; i > newsbcnt;) {
	    cur_win->scrollinfo[--i].exposed = False;
	}
    } else if (cur_win->nscrollbars < newsbcnt) {
	nchildren = (Cardinal) (newsbcnt - cur_win->nscrollbars);
	XtManageChildren(cur_win->scrollbars + cur_win->nscrollbars, nchildren);
	if (cur_win->nscrollbars > 0)
	    XtManageChildren(cur_win->grips + cur_win->nscrollbars - 1, nchildren);
	else if (cur_win->nscrollbars == 0 && nchildren > 1)
	    XtManageChildren(cur_win->grips, nchildren - 1);
    }
    cur_win->nscrollbars = newsbcnt;

    i = 0;
    for_each_visible_window(wp) {
	wp->w_flag = (USHORT) (wp->w_flag & ~WFSBAR);
	gui_update_scrollbar(wp);
	i++;
    }

    /*
     * Set the cursors... It would be nice if we could do this in the
     * initialization above, but the widget needs to be realized for this
     * to work
     */
    for (i = 0; i < cur_win->nscrollbars; i++) {
	if (XtIsRealized(cur_win->scrollbars[i]))
	    set_pointer(XtWindow(cur_win->scrollbars[i]),
			curs_sb_v_double_arrow);
	if (i < cur_win->nscrollbars - 1 && XtIsRealized(cur_win->grips[i]))
	    set_pointer(XtWindow(cur_win->grips[i]),
			curs_double_arrow);
    }
}

/*
 * The X11R5 Athena scrollbar code was used as a reference for writing
 * draw_thumb and parts of update_thumb.
 */

#define FILL_TOP 0x01
#define FILL_BOT 0x02

#define SP_HT 16		/* slider pixmap height */

static void
draw_thumb(Widget w, int top, int bot, int dofill)
{
    UINT length = (UINT) (bot - top);
    if (bot < 0 || top < 0 || bot <= top)
	return;

    if (!dofill)
	XClearArea(XtDisplay(w), XtWindow(w), cur_win->slider_is_3D ? 2 : 1,
		   top, cur_win->pane_width - 2, length, FALSE);
    else if (!cur_win->slider_is_3D)
	XFillRectangle(XtDisplay(w), XtWindow(w), cur_win->scrollbar_gc,
		       1, top, cur_win->pane_width - 2, length);
    else {
	if (dofill & FILL_TOP) {
	    int tbot;
	    tbot = bot - ((dofill & FILL_BOT) ? 2 : 0);
	    if (tbot <= top)
		tbot = top + 1;
	    if (top + (SP_HT - 2) < tbot)
		tbot = top + (SP_HT - 2);
	    XCopyArea(dpy, cur_win->slider_pixmap, XtWindow(w),
		      cur_win->scrollbar_gc,
		      0, 0, cur_win->pane_width - 2, (UINT) (tbot - top),
		      2, top);

	    top = tbot;
	}
	if (dofill & FILL_BOT) {
	    int btop = max(top, bot - (SP_HT - 2));
	    XCopyArea(dpy, cur_win->slider_pixmap, XtWindow(w),
		      cur_win->scrollbar_gc,
		      0, SP_HT - (bot - btop),
		      cur_win->pane_width - 2, (UINT) (bot - btop),
		      2, btop);
	    bot = btop;

	}
	if (top < bot) {
	    XFillRectangle(XtDisplay(w), XtWindow(w), cur_win->scrollbar_gc,
			   2, top,
			   cur_win->pane_width - 2, (UINT) (bot - top));
	}
    }
}

#define MIN_THUMB_SIZE 8

static void
update_thumb(int barnum, int newtop, int newlen)
{
    int oldtop, oldbot, newbot, totlen;
    int f = cur_win->slider_is_3D ? 2 : 0;
    Widget w = cur_win->scrollbars[barnum];

    oldtop = cur_win->scrollinfo[barnum].top;
    oldbot = cur_win->scrollinfo[barnum].bot;
    totlen = cur_win->scrollinfo[barnum].totlen;
    newtop = min(newtop, totlen - 3);
    newbot = newtop + max(newlen, MIN_THUMB_SIZE);
    newbot = min(newbot, totlen);
    cur_win->scrollinfo[barnum].top = newtop;
    cur_win->scrollinfo[barnum].bot = newbot;

    if (!cur_win->scrollinfo[barnum].exposed)
	return;

    if (XtIsRealized(w)) {
	if (newtop < oldtop) {
	    int tbot = min(newbot, oldtop + f);
	    draw_thumb(w, newtop, tbot,
		       FILL_TOP | ((tbot == newbot) ? FILL_BOT : 0));
	}
	if (newtop > oldtop) {
	    draw_thumb(w, oldtop, min(newtop, oldbot), 0);
	    if (cur_win->slider_is_3D && newtop < oldbot)
		draw_thumb(w, newtop, min(newtop + f, oldbot), FILL_TOP);
	}
	if (newbot < oldbot) {
	    draw_thumb(w, max(newbot, oldtop), oldbot, 0);
	    if (cur_win->slider_is_3D && oldtop < newbot)
		draw_thumb(w, max(newbot - f, oldtop), newbot, FILL_BOT);
	}
	if (newbot > oldbot) {
	    int btop = max(newtop, oldbot - f);
	    draw_thumb(w, btop, newbot,
		       FILL_BOT | ((btop == newtop) ? FILL_TOP : 0));
	}
    }
}

/*ARGSUSED*/
static void
x_expose_scrollbar(Widget w,
		   XtPointer unused GCC_UNUSED,
		   XEvent *ev,
		   Boolean *continue_to_dispatch GCC_UNUSED)
{
    int i;

    if (ev->type != Expose)
	return;

    for (i = 0; i < cur_win->nscrollbars; i++) {
	if (cur_win->scrollbars[i] == w) {
	    int top, bot;
	    top = cur_win->scrollinfo[i].top;
	    bot = cur_win->scrollinfo[i].bot;
	    cur_win->scrollinfo[i].top = -1;
	    cur_win->scrollinfo[i].bot = -1;
	    cur_win->scrollinfo[i].exposed = True;
	    update_thumb(i, top, bot - top);
	}
    }
}
#endif /* OPT_KEV_SCROLLBARS  */
#endif /* OPT_XAW_SCROLLBARS  */
#endif /* MOTIF_WIDGETS */

#if OPT_KEV_DRAGGING
static void
do_scroll(Widget w,
	  XEvent *event,
	  String *params,
	  Cardinal *num_params)
{
    static enum {
	none, forward, backward, drag
    } scrollmode = none;
    int pos;
    WINDOW *wp;
    int i;
    XEvent nev;
    int count;

    /*
     * Return immediately if behind in processing motion events.  Note:
     * If this is taken out, scrolling is actually smoother, but sometimes
     * takes a while to catch up.  I should think that performance would
     * be horrible on a slow server.
     */

    if (scrollmode == drag
	&& event->type == MotionNotify
	&& x_has_events()
	&& XtAppPeekEvent(cur_win->app_context, &nev)
	&& (nev.type == MotionNotify || nev.type == ButtonRelease))
	return;

    if (*num_params != 1)
	return;

    /* Determine vertical position */
    switch (event->type) {
    case MotionNotify:
	pos = event->xmotion.y;
	break;
    case ButtonPress:
    case ButtonRelease:
	pos = event->xbutton.y;
	break;
    default:
	return;
    }

    /* Determine scrollbar number and corresponding vile window */
    i = 0;
    for_each_visible_window(wp) {
	if (cur_win->scrollbars[i] == w)
	    break;
	i++;
    }

    if (!wp)
	return;

    if (pos < 0)
	pos = 0;
    if (pos > cur_win->scrollinfo[i].totlen)
	pos = cur_win->scrollinfo[i].totlen;

    switch (**params) {
    case 'E':			/* End */
	if (cur_win->scroll_repeat_id != (XtIntervalId) 0) {
	    XtRemoveTimeOut(cur_win->scroll_repeat_id);
	    cur_win->scroll_repeat_id = (XtIntervalId) 0;
	}
	set_pointer(XtWindow(w), curs_sb_v_double_arrow);
	scrollmode = none;
	break;
    case 'F':			/* Forward */
	if (scrollmode != none)
	    break;
	count = (pos / cur_win->char_height) + 1;
	scrollmode = forward;
	set_pointer(XtWindow(w), curs_sb_up_arrow);
	goto do_scroll_common;
    case 'B':			/* Backward */
	if (scrollmode != none)
	    break;
	count = -((pos / cur_win->char_height) + 1);
	scrollmode = backward;
	set_pointer(XtWindow(w), curs_sb_down_arrow);
      do_scroll_common:
	set_curwp(wp);
	mvdnwind(TRUE, count);
	cur_win->scroll_repeat_id =
	    XtAppAddTimeOut(cur_win->app_context,
			    (ULONG) cur_win->scroll_repeat_timeout,
			    repeat_scroll,
			    (XtPointer) (long) count);
	(void) update(TRUE);
	break;
    case 'S':			/* StartDrag */
	if (scrollmode == none) {
	    set_curwp(wp);
	    scrollmode = drag;
	    set_pointer(XtWindow(w), curs_sb_right_arrow);
	}
	/* FALLTHRU */
    case 'D':			/* Drag */
	if (scrollmode == drag) {
	    int lcur = line_no(curwp->w_bufp, curwp->w_line.l);
	    int ltarg = (vl_line_count(curwp->w_bufp) * pos
			 / cur_win->scrollinfo[i].totlen) + 1;
	    mvupwind(TRUE, lcur - ltarg);
	    (void) update(TRUE);
	}
	break;
    }
}

/*ARGSUSED*/
static void
repeat_scroll(XtPointer count,
	      XtIntervalId * id GCC_UNUSED)
{
    cur_win->scroll_repeat_id =
	XtAppAddTimeOut(cur_win->app_context,
			(ULONG) cur_win->scroll_repeat_interval,
			repeat_scroll,
			(XtPointer) count);
    mvdnwind(TRUE, (int) (long) count);
    (void) update(TRUE);
    XSync(dpy, False);
}
#endif /* OPT_KEV_DRAGGING */

#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
static void
resize_bar(Widget w,
	   XEvent *event,
	   String *params,
	   Cardinal *num_params)
{
    static int motion_permitted = False;
    static int root_y;
    int pos = 0;		/* stifle -Wall */
    WINDOW *wp;
    int i;
    XEvent nev;

    /* Return immediately if behind in processing motion events */
    if (motion_permitted
	&& event->type == MotionNotify
	&& x_has_events()
	&& XtAppPeekEvent(cur_win->app_context, &nev)
	&& (nev.type == MotionNotify || nev.type == ButtonRelease))
	return;

    if (*num_params != 1)
	return;

    switch (**params) {
    case 'S':			/* Start */
	motion_permitted = True;
	root_y = event->xbutton.y_root - event->xbutton.y;
	return;
    case 'D':			/* Drag */
	if (!motion_permitted)
	    return;
	/*
	 * We use kind of convoluted mechanism to determine the vertical
	 * position with respect to the widget which we are moving. The
	 * reason is that the x,y position from the event structure is
	 * unreliable since the widget may have moved in between the
	 * time when the event was first generated (at the display
	 * server) and the time at which the event is received (in
	 * this code here).  So we keep track of the vertical position
	 * of the widget with respect to the root window and use the
	 * corresponding field in the event structure to determine
	 * the vertical position of the pointer with respect to the
	 * widget of interest.  In the past, XQueryPointer() was
	 * used to determine the position of the pointer, but this is
	 * not really what we want since the position returned by
	 * XQueryPointer is the position of the pointer at a time
	 * after the event was both generated and received (and thus
	 * inaccurate).  This is why the resize grip would sometimes
	 * "follow" the mouse pointer after the button was released.
	 */
	pos = event->xmotion.y_root - root_y;
	break;
    case 'E':			/* End */
	if (!motion_permitted)
	    return;
	pos = event->xbutton.y_root - root_y;
	motion_permitted = False;
	break;
    }

    /* Determine grip number and corresponding vile window (above grip) */
    i = 0;
    for_each_visible_window(wp) {
	if (cur_win->grips[i] == w)
	    break;
	i++;
    }

    if (!wp)
	return;

    if (pos < 0)
	pos -= cur_win->char_height;
    pos = pos / cur_win->char_height;

    if (pos) {
	int nlines;
	if (pos >= wp->w_wndp->w_ntrows)
	    pos = wp->w_wndp->w_ntrows - 1;

	nlines = wp->w_ntrows + pos;
	if (nlines < 1)
	    nlines = 1;
	root_y += (nlines - wp->w_ntrows) * cur_win->char_height;
	set_curwp(wp);
	resize(TRUE, nlines);
	(void) update(TRUE);
    }
}
#endif /* OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS */

void
gui_update_scrollbar(WINDOW *uwp)
{
    WINDOW *wp;
    int i;
    int lnum, lcnt;

#if !OPT_KEV_SCROLLBARS && !OPT_XAW_SCROLLBARS
    if (dont_update_sb)
	return;
#endif /* !OPT_KEV_SCROLLBARS */

    i = 0;
    for_each_visible_window(wp) {
	if (wp == uwp)
	    break;
	i++;
    }
    if (wp == NULL)
	return;
    if (i >= cur_win->nscrollbars || (wp->w_flag & WFSBAR)) {
	/*
	 * update_scrollbar_sizes will recursively invoke gui_update_scrollbar,
	 * but with WFSBAR disabled.
	 */
	update_scrollbar_sizes();
	return;
    }

    lnum = line_no(wp->w_bufp, wp->w_line.l);
    lnum = (lnum > 0) ? lnum : 1;
    lcnt = vl_line_count(wp->w_bufp);
#if MOTIF_WIDGETS
    lcnt += 1;
    XtVaSetValues(cur_win->scrollbars[i],
		  Nval(XmNmaximum, lcnt + wp->w_ntrows),
		  Nval(XmNsliderSize, wp->w_ntrows),
		  Nval(XmNvalue, lnum),
		  Nval(XmNpageIncrement, ((wp->w_ntrows > 1)
					  ? wp->w_ntrows - 1
					  : 1)),
		  NULL);
#else
#if OPT_XAW_SCROLLBARS
    XawScrollbarSetThumb(cur_win->scrollbars[i],
			 ((float) (lnum - 1)) / (float) max(lcnt, wp->w_ntrows),
			 ((float) wp->w_ntrows) / (float) max(lcnt, wp->w_ntrows));
#else
#if OPT_KEV_SCROLLBARS
    {
	int top, len;
	lcnt = max(lcnt, 1);
	len = (min(lcnt, wp->w_ntrows) * cur_win->scrollinfo[i].totlen
	       / lcnt) + 1;
	top = ((lnum - 1) * cur_win->scrollinfo[i].totlen)
	    / lcnt;
	update_thumb(i, top, len);
    }
#endif /* OPT_KEV_SCROLLBARS */
#endif /* OPT_XAW_SCROLLBARS */
#endif /* MOTIF_WIDGETS */
}

#define XtNnormalShape		"normalShape"
#define XtCNormalShape		"NormalShape"
#define XtNwatchShape		"watchShape"
#define XtCWatchShape		"WatchShape"

#define XtNforkOnStartup	"forkOnStartup"
#define XtCForkOnStartup	"ForkOnStartup"

#define XtNscrollbarWidth	"scrollbarWidth"
#define XtCScrollbarWidth	"ScrollbarWidth"
#define XtNfocusFollowsMouse	"focusFollowsMouse"
#define XtCFocusFollowsMouse	"FocusFollowsMouse"
#define XtNscrollbarOnLeft	"scrollbarOnLeft"
#define XtCScrollbarOnLeft	"ScrollbarOnLeft"
#define XtNmultiClickTime	"multiClickTime"
#define XtCMultiClickTime	"MultiClickTime"
#define XtNcharClass		"charClass"
#define XtNwheelScrollAmount    "wheelScrollAmount"
#define XtCWheelScrollAmount    "WheelScrollAmount"
#define XtCCharClass		"CharClass"
#if OPT_KEV_DRAGGING
#define	XtNscrollRepeatTimeout	"scrollRepeatTimeout"
#define	XtCScrollRepeatTimeout	"ScrollRepeatTimeout"
#endif
#define	XtNscrollRepeatInterval	"scrollRepeatInterval"
#define	XtCScrollRepeatInterval	"ScrollRepeatInterval"
#define XtNpersistentSelections	"persistentSelections"
#define XtCPersistentSelections	"PersistentSelections"
#define XtNselectionSetsDOT	"selectionSetsDOT"
#define XtCSelectionSetsDOT	"SelectionSetsDOT"
#define XtNblinkInterval	"blinkInterval"
#define XtCBlinkInterval	"BlinkInterval"
#define XtNsliderIsSolid	"sliderIsSolid"
#define XtCSliderIsSolid	"SliderIsSolid"
#define	XtNfocusForeground	"focusForeground"
#define	XtNfocusBackground	"focusBackground"

#define XtNfcolor0		"fcolor0"
#define XtCFcolor0		"Fcolor0"
#define XtNbcolor0		"bcolor0"
#define XtCBcolor0		"Bcolor0"

#define XtNfcolor1		"fcolor1"
#define XtCFcolor1		"Fcolor1"
#define XtNbcolor1		"bcolor1"
#define XtCBcolor1		"Bcolor1"

#define XtNfcolor2		"fcolor2"
#define XtCFcolor2		"Fcolor2"
#define XtNbcolor2		"bcolor2"
#define XtCBcolor2		"Bcolor2"

#define XtNfcolor3		"fcolor3"
#define XtCFcolor3		"Fcolor3"
#define XtNbcolor3		"bcolor3"
#define XtCBcolor3		"Bcolor3"

#define XtNfcolor4		"fcolor4"
#define XtCFcolor4		"Fcolor4"
#define XtNbcolor4		"bcolor4"
#define XtCBcolor4		"Bcolor4"

#define XtNfcolor5		"fcolor5"
#define XtCFcolor5		"Fcolor5"
#define XtNbcolor5		"bcolor5"
#define XtCBcolor5		"Bcolor5"

#define XtNfcolor6		"fcolor6"
#define XtCFcolor6		"Fcolor6"
#define XtNbcolor6		"bcolor6"
#define XtCBcolor6		"Bcolor6"

#define XtNfcolor7		"fcolor7"
#define XtCFcolor7		"Fcolor7"
#define XtNbcolor7		"bcolor7"
#define XtCBcolor7		"Bcolor7"

#define XtNfcolor8		"fcolor8"
#define XtCFcolor8		"Fcolor8"
#define XtNbcolor8		"bcolor8"
#define XtCBcolor8		"Bcolor8"

#define XtNfcolor9		"fcolor9"
#define XtCFcolor9		"Fcolor9"
#define XtNbcolor9		"bcolor9"
#define XtCBcolor9		"Bcolor9"

#define XtNfcolorA		"fcolor10"
#define XtCFcolorA		"Fcolor10"
#define XtNbcolorA		"bcolor10"
#define XtCBcolorA		"Bcolor10"

#define XtNfcolorB		"fcolor11"
#define XtCFcolorB		"Fcolor11"
#define XtNbcolorB		"bcolor11"
#define XtCBcolorB		"Bcolor11"

#define XtNfcolorC		"fcolor12"
#define XtCFcolorC		"Fcolor12"
#define XtNbcolorC		"bcolor12"
#define XtCBcolorC		"Bcolor12"

#define XtNfcolorD		"fcolor13"
#define XtCFcolorD		"Fcolor13"
#define XtNbcolorD		"bcolor13"
#define XtCBcolorD		"Bcolor13"

#define XtNfcolorE		"fcolor14"
#define XtCFcolorE		"Fcolor14"
#define XtNbcolorE		"bcolor14"
#define XtCBcolorE		"Bcolor14"

#define XtNfcolorF		"fcolor15"
#define XtCFcolorF		"Fcolor15"
#define XtNbcolorF		"bcolor15"
#define XtCBcolorF		"Bcolor15"

/*
 * xvile has two categories of color resources: (a) those which depend only
 * on the X default colors, and (b) those which may depend on fixups to the
 * foreground/background colors within xvile.  In the latter case, we cannot
 * simply evaluate all of the resources at once; they are fetched via a
 * sub-resource call.
 */
#define XRES_TOP_COLOR(name, class, value, dft) \
    { \
	name, \
	class, \
	XtRPixel, \
	sizeof(Pixel), \
	XtOffset(TextWindow, value), \
	XtRString, \
	(XtPointer) dft \
    }

#define XRES_SUB_COLOR(name, class, value, dft) \
    { \
	name, \
	class, \
	XtRPixel, \
	sizeof(Pixel), \
	XtOffset(TextWindow, value), \
	XtRPixel, \
	(XtPointer) dft \
    }

#define XRES_FG(name, class, value) \
    XRES_TOP_COLOR(name, class, value, XtDefaultForeground)

#define XRES_BG(name, class, value) \
    XRES_TOP_COLOR(name, class, value, XtDefaultBackground)

/*
 * Other resources, for readability.
 */
#define XRES_BOOL(name, class, value, dft) \
    { \
	name, \
	class, \
	XtRBool, \
	sizeof(Bool), \
	XtOffset(TextWindow, value), \
	XtRImmediate, \
	(XtPointer) dft \
    }

#define XRES_DIMENSION(name, class, value, dft) \
    { \
	name, \
	class, \
	XtRDimension, \
	sizeof(cur_win->value), \
	XtOffset(TextWindow, value), \
	XtRImmediate, \
	(XtPointer) dft \
    }

#define XRES_INTEGER(name, class, value, dft) \
    { \
	name, \
	class, \
	XtRInt, \
	sizeof(cur_win->value), \
	XtOffset(TextWindow, value), \
	XtRImmediate, \
	(XtPointer) dft \
    }

#define XRES_STRING(name, class, value, dft) \
    { \
	name, \
	class, \
	XtRString, \
	sizeof(String *), \
	XtOffset(TextWindow, value), \
	XtRImmediate, \
	(XtPointer) dft \
    }

static XtResource app_resources[] =
{
#if OPT_KEV_DRAGGING
    XRES_INTEGER(XtNscrollRepeatTimeout, XtCScrollRepeatTimeout,
		 scroll_repeat_timeout, 500),	/* 1/2 second */
#endif				/* OPT_KEV_DRAGGING */
    XRES_INTEGER(XtNscrollRepeatInterval, XtCScrollRepeatInterval,
		 scroll_repeat_interval, 60),	/* 60 milliseconds */
    XRES_BOOL(XtNreverseVideo, XtCReverseVideo, reverse_video, False),
    XRES_STRING(XtNiconName, XtCIconName, iconname, ""),
    XRES_STRING(XtNgeometry, XtCGeometry, geometry, "80x36"),
#ifdef XRENDERFONT
    XRES_STRING(XtNfontFamily, XtCString, starting_fontname, "monospace"),
#else
    XRES_STRING(XtNfont, XtCFont, starting_fontname, "fixed"),
#endif
    XRES_FG(XtNforeground, XtCForeground, fg),
    XRES_BG(XtNbackground, XtCBackground, bg),
    XRES_BOOL(XtNforkOnStartup, XtCForkOnStartup, fork_on_startup, False),
    XRES_BOOL(XtNfocusFollowsMouse, XtCFocusFollowsMouse,
	      focus_follows_mouse, False),
    XRES_INTEGER(XtNmultiClickTime, XtCMultiClickTime, click_timeout, 600),
    XRES_STRING(XtNcharClass, XtCCharClass, multi_click_char_class, NULL),
    XRES_BOOL(XtNscrollbarOnLeft, XtCScrollbarOnLeft, scrollbar_on_left, False),
    XRES_INTEGER(XtNscrollbarWidth, XtCScrollbarWidth, pane_width, PANE_WIDTH_DEFAULT),
    XRES_INTEGER(XtNwheelScrollAmount, XtCWheelScrollAmount,
		 wheel_scroll_amount, 3),
#if OPT_MENUS
    XRES_DIMENSION(XtNmenuHeight, XtCMenuHeight, menu_height, MENU_HEIGHT_DEFAULT),
#endif
#if OPT_MENUS_COLORED
    XRES_FG(XtNmenuForeground, XtCMenuForeground, menubar_fg),
    XRES_BG(XtNmenuBackground, XtCMenuBackground, menubar_bg),
#endif
    XRES_BOOL(XtNpersistentSelections, XtCPersistentSelections,
	      persistent_selections, True),
    XRES_BOOL(XtNselectionSetsDOT, XtCSelectionSetsDOT, selection_sets_DOT, False),
    XRES_INTEGER(XtNblinkInterval, XtCBlinkInterval,
		 blink_interval, -666),		/* 2/3 second; only when highlighted */
#if OPT_INPUT_METHOD
    XRES_BOOL(XtNopenIm, XtCOpenIm, open_im, True),
    XRES_STRING(XtNinputMethod, XtCInputMethod, rs_inputMethod, ""),
    XRES_STRING(XtNpreeditType, XtCPreeditType,
		rs_preeditType, "OverTheSpot,Root"),
#ifndef DEFXIMFONT
#define DEFXIMFONT		"*"
#endif
#define XtNximFont		"ximFont"
#define XtCXimFont		"XimFont"
    XRES_STRING(XtNximFont, XtCXimFont, rs_imFont, DEFXIMFONT)
#endif				/* OPT_INPUT_METHOD */
};

/*
 * The reason for the sub-resource tables is to allow xvile to use the main
 * window's foreground/background color as the default value for other color
 * resources.  That makes setting up color resources simpler, but complicates
 * the program.
 *
 * The call that retrieves the subresources initializes _all_ of the resources
 * for the given name/class.  So there are additional resources in these tables
 * which cannot be moved to the app_resources table.
 */
static XtResource color_resources[] =
{
    XRES_SUB_COLOR(XtNbcolor0, XtCBcolor0, colors_bg[0], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbcolor1, XtCBcolor1, colors_bg[1], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbcolor2, XtCBcolor2, colors_bg[2], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbcolor3, XtCBcolor3, colors_bg[3], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbcolor4, XtCBcolor4, colors_bg[4], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbcolor5, XtCBcolor5, colors_bg[5], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbcolor6, XtCBcolor6, colors_bg[6], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbcolor7, XtCBcolor7, colors_bg[7], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbcolor8, XtCBcolor8, colors_bg[8], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbcolor9, XtCBcolor9, colors_bg[9], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbcolorA, XtCBcolorA, colors_bg[10], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbcolorB, XtCBcolorB, colors_bg[11], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbcolorC, XtCBcolorC, colors_bg[12], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbcolorD, XtCBcolorD, colors_bg[13], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbcolorE, XtCBcolorE, colors_bg[14], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbcolorF, XtCBcolorF, colors_bg[15], &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNfcolor0, XtCFcolor0, colors_fg[0], &cur_win_rec.fg),
    XRES_TOP_COLOR(XtNfcolor1, XtCFcolor1, colors_fg[1], "rgb:ff/0/0"),
    XRES_TOP_COLOR(XtNfcolor2, XtCFcolor2, colors_fg[2], "rgb:0/ff/0"),
    XRES_TOP_COLOR(XtNfcolor3, XtCFcolor3, colors_fg[3], "rgb:a5/2a/2a"),
    XRES_TOP_COLOR(XtNfcolor4, XtCFcolor4, colors_fg[4], "rgb:0/0/ff"),
    XRES_TOP_COLOR(XtNfcolor5, XtCFcolor5, colors_fg[5], "rgb:ff/0/ff"),
    XRES_TOP_COLOR(XtNfcolor6, XtCFcolor6, colors_fg[6], "rgb:0/ff/ff"),
    XRES_TOP_COLOR(XtNfcolor7, XtCFcolor7, colors_fg[7], "rgb:e6/e6/e6"),
    XRES_TOP_COLOR(XtNfcolor8, XtCFcolor8, colors_fg[8], "rgb:be/be/be"),
    XRES_TOP_COLOR(XtNfcolor9, XtCFcolor9, colors_fg[9], "rgb:ff/7f/7f"),
    XRES_TOP_COLOR(XtNfcolorA, XtCFcolorA, colors_fg[10], "rgb:7f/ff/7f"),
    XRES_TOP_COLOR(XtNfcolorB, XtCFcolorB, colors_fg[11], "rgb:ff/ff/0"),
    XRES_TOP_COLOR(XtNfcolorC, XtCFcolorC, colors_fg[12], "rgb:7f/7f/ff"),
    XRES_TOP_COLOR(XtNfcolorD, XtCFcolorD, colors_fg[13], "rgb:ff/7f/ff"),
    XRES_TOP_COLOR(XtNfcolorE, XtCFcolorE, colors_fg[14], "rgb:7f/ff/ff"),
    XRES_TOP_COLOR(XtNfcolorF, XtCFcolorF, colors_fg[15], "rgb:ff/ff/ff"),
};

#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
static XtResource scrollbar_resources[] =
{
    XRES_SUB_COLOR(XtNforeground, XtCForeground, scrollbar_fg, &cur_win_rec.fg),
    XRES_SUB_COLOR(XtNbackground, XtCBackground, scrollbar_bg, &cur_win_rec.bg),
    XRES_BOOL(XtNsliderIsSolid, XtCSliderIsSolid, slider_is_solid, False),
};
#endif

static XtResource modeline_resources[] =
{
    XRES_SUB_COLOR(XtNforeground, XtCForeground, modeline_fg, &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbackground, XtCBackground, modeline_bg, &cur_win_rec.fg),
    XRES_SUB_COLOR(XtNfocusForeground, XtCForeground, modeline_focus_fg, &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNfocusBackground, XtCBackground, modeline_focus_bg, &cur_win_rec.fg),
};

static XtResource selection_resources[] =
{
    XRES_SUB_COLOR(XtNforeground, XtCBackground, selection_fg, &cur_win_rec.bg),
    XRES_SUB_COLOR(XtNbackground, XtCForeground, selection_bg, &cur_win_rec.fg),
};

/*
 * We resort to a bit of trickery for the cursor resources.  Note that the
 * default foreground and background for the cursor is the same as that for
 * the rest of the window.  This would render the cursor invisible!  This
 * condition actually indicates that usual technique of inverting the
 * foreground and background colors should be used, the rationale being
 * that no (sane) user would want to set the cursor foreground and
 * background to be the same as the rest of the window.
 */

static XtResource cursor_resources[] =
{
    XRES_SUB_COLOR(XtNforeground, XtCForeground, cursor_fg, &cur_win_rec.fg),
    XRES_SUB_COLOR(XtNbackground, XtCBackground, cursor_bg, &cur_win_rec.bg),
};

static XtResource pointer_resources[] =
{
    XRES_SUB_COLOR(XtNforeground, XtCForeground, pointer_fg, &cur_win_rec.fg),
    XRES_SUB_COLOR(XtNbackground, XtCBackground, pointer_bg, &cur_win_rec.bg),
    {
	XtNnormalShape,
	XtCNormalShape,
	XtRCursor,
	sizeof(Cursor),
	XtOffset(TextWindow, normal_pointer),
	XtRString,
	(XtPointer) "xterm"
    },
#if OPT_WORKING
    {
	XtNwatchShape,
	XtCWatchShape,
	XtRCursor,
	sizeof(Cursor),
	XtOffset(TextWindow, watch_pointer),
	XtRString,
	(XtPointer) "watch"
    },
#endif
};

#define CHECK_MIN_MAX(v,min,max)	\
	do {				\
	    if ((v) > (max))		\
		(v)=(max);		\
	    else if ((v) < (min))	\
		(v) = (min);		\
	} one_time
static GCC_NORETURN void initial_error_handler(String message);
static void
initial_error_handler(String message)
{
    TRACE(("initial_error_handler:%s\n", NonNull(message)));
    fprintf(stderr, "%s: %s\n", prog_arg, NonNull(message));
    print_usage(BADEXIT);
}

static GCC_NORETURN void runtime_error_handler(String message);
static void
runtime_error_handler(String message)
{
    TRACE(("runtime_error_handler:%s\n", NonNull(message)));
    fprintf(stderr, "%s: %s\n", prog_arg, NonNull(message));
    imdying(SIGINT);		/* save files and exit, do not abort() */
}

/***====================================================================***/

#ifdef XRENDERFONT
static unsigned
maskToShift(unsigned long mask)
{
    unsigned result = 0;
    if (mask != 0) {
	while ((mask & 1) == 0) {
	    mask >>= 1;
	    ++result;
	}
    }
    return result;
}

static unsigned
maskToWidth(unsigned long mask)
{
    unsigned result = 0;
    while (mask != 0) {
	if ((mask & 1) != 0)
	    ++result;
	mask >>= 1;
    }
    return result;
}

static XVisualInfo *
getVisualInfo(TextWindow tw)
{
#define MYFMT "getVisualInfo \
depth %d, \
type %d (%s), \
size %d \
rgb masks (%04lx/%04lx/%04lx)\n"
#define MYARG \
       vi->depth,\
       vi->class,\
       ((vi->class & 1) ? "dynamic" : "static"),\
       vi->colormap_size,\
       vi->red_mask,\
       vi->green_mask,\
       vi->blue_mask

    XVisualInfo myTemplate;

    if (tw->visInfo == 0 && tw->numVisuals == 0) {
	myTemplate.visualid = XVisualIDFromVisual(DefaultVisual(dpy,
								XDefaultScreen(dpy)));
	tw->visInfo = XGetVisualInfo(dpy, (long) VisualIDMask,
				     &myTemplate, &tw->numVisuals);

	if ((tw->visInfo != 0) && (tw->numVisuals > 0)) {
	    XVisualInfo *vi = tw->visInfo;
	    tw->rgb_widths[0] = maskToWidth(vi->red_mask);
	    tw->rgb_widths[1] = maskToWidth(vi->green_mask);
	    tw->rgb_widths[2] = maskToWidth(vi->blue_mask);
	    tw->rgb_shifts[0] = maskToShift(vi->red_mask);
	    tw->rgb_shifts[1] = maskToShift(vi->green_mask);
	    tw->rgb_shifts[2] = maskToShift(vi->blue_mask);

	    tw->has_rgb = ((vi->red_mask != 0) &&
			   (vi->green_mask != 0) &&
			   (vi->blue_mask != 0) &&
			   ((vi->red_mask & vi->green_mask) == 0) &&
			   ((vi->green_mask & vi->blue_mask) == 0) &&
			   ((vi->blue_mask & vi->red_mask) == 0) &&
			   (vi->class == TrueColor
			    || vi->class == DirectColor));

	    TRACE((MYFMT, MYARG));
	    TRACE(("...shifts %u/%u/%u\n",
		   tw->rgb_shifts[0],
		   tw->rgb_shifts[1],
		   tw->rgb_shifts[2]));
	    TRACE(("...widths %u/%u/%u\n",
		   tw->rgb_widths[0],
		   tw->rgb_widths[1],
		   tw->rgb_widths[2]));
	}
    }
    return (tw->visInfo != 0) && (tw->numVisuals > 0) ? tw->visInfo : NULL;
#undef MYFMT
#undef MYARG
}
#endif /* XRENDERFONT */

/***====================================================================***/

#ifdef XRENDERFONT
static void
pixelToXftColor(TextWindow tw, XftColor *target, Pixel source)
{
#define UnMaskIt(name,nn) \
	((unsigned short)((def.pixel & tw->visInfo->name ##_mask) >> tw->rgb_shifts[nn]))
#define UnMaskIt2(name,nn) \
	(unsigned short)((((UnMaskIt(name,nn) << 8) \
			   |UnMaskIt(name,nn))) << (8 - tw->rgb_widths[nn]))
    XColor def;
    Boolean result = True;

    memset(&def, 0, sizeof(def));
    def.pixel = source;

    if ((tw->visInfo = getVisualInfo(tw)) != NULL && tw->has_rgb) {
	/* *INDENT-EQLS* */
	def.red   = UnMaskIt2(red, 0);
	def.green = UnMaskIt2(green, 1);
	def.blue  = UnMaskIt2(blue, 2);
    } else if (!XQueryColor(dpy, tw->colormap, &def)) {
	result    = False;
    }

    if (result) {
	/* *INDENT-EQLS* */
	target->pixel       = source;
	target->color.red   = def.red;
	target->color.green = def.green;
	target->color.blue  = def.blue;
	target->color.alpha = 0xffff;
    } else {
	memset(target, 0, sizeof(*target));
    }

    TRACE2((".. pixelToXftColor %06lx -> %04x/%04x/%04x\n",
	    source,
	    target->color.red,
	    target->color.green,
	    target->color.blue));
}
#endif

#define MIN_UDIFF  0x1000
#define UDIFF(a,b) ((a)>(b)?(a)-(b):(b)-(a))

/*
 * Returns true if the RGB components are close enough to make distinguishing
 * two colors difficult.
 */
static Boolean
SamePixel(Pixel a, Pixel b)
{
    Boolean result = True;

    if (a != b) {
	result = False;
	if (cur_win->top_widget != NULL) {
#ifdef XRENDERFONT
	    XftColor a_color;
	    XftColor b_color;

	    pixelToXftColor(cur_win, &a_color, a);
	    pixelToXftColor(cur_win, &b_color, b);

	    if (UDIFF(a_color.color.red, b_color.color.red) < MIN_UDIFF
		&& UDIFF(a_color.color.green, b_color.color.green) < MIN_UDIFF
		&& UDIFF(a_color.color.blue, b_color.color.blue) < MIN_UDIFF)
		result = True;
#else
	    XColor a_color;
	    XColor b_color;

	    a_color.pixel = a;
	    b_color.pixel = b;

	    if (XQueryColor(dpy, cur_win->colormap, &a_color)
		&& XQueryColor(dpy, cur_win->colormap, &b_color)) {
		if (UDIFF(a_color.red, b_color.red) < MIN_UDIFF
		    && UDIFF(a_color.green, b_color.green) < MIN_UDIFF
		    && UDIFF(a_color.blue, b_color.blue) < MIN_UDIFF)
		    result = True;
	    } else {
		TRACE(("FIXME: SamePixel failed\n"));
	    }
#endif
	} else {
	    TRACE(("FIXME: SamePixel cannot compute (too soon)\n"));
	}
    }
    return result;
}

/***====================================================================***/

#if OPT_TRACE
static Boolean
retraceBoolean(Boolean code)
{
    Trace(T_RETURN "%s\n", code ? "True" : "False");
    return code;
}

static const char *
traceColorGC(TextWindow win, ColorGC * source)
{
    static char temp[20];
    const char *result = "?";

#define if_CASE(name) \
    	if (source == &win->name) \
		result = #name

#define if_MANY(name) \
	if (source >= &win->name[0] && \
	    source < &win->name[NCOLORS]) { \
	    sprintf(temp, "%s[%d]", #name, (int)(source - win->name)); \
	    result = temp; \
	}
    /* *INDENT-OFF* */
    if_CASE(tt_info);
    else if_CASE(rt_info);
    else if_CASE(ss_info);
    else if_CASE(rs_info);
    else if_CASE(mm_info);
    else if_CASE(rm_info);
    else if_CASE(oo_info);
    else if_CASE(ro_info);
    else if_CASE(cc_info);
    else if_CASE(rc_info);
    else if_MANY(fg_info)
    else if_MANY(bg_info)
    /* *INDENT-ON* */
    return result;
}
#define returnBoolean(code) return retraceBoolean(code)
#else
#define returnBoolean(code) return(code)
#endif

/*
 * Set parameters in a ColorGC so that a subsequent makeColorGC can evaluate
 * the parameters to allocate a GC.
 */
static void
initColorGC(TextWindow win, ColorGC * data, Pixel new_fg, Pixel new_bg)
{
    (void) win;

    data->gcmask = GCForeground | GCBackground | GCGraphicsExposures;
#ifndef XRENDERFONT
    data->gcmask |= GCFont;
    data->gcvals.font = win->fonts.norm->fid;
#endif
    data->gcvals.graphics_exposures = False;

    data->gcvals.foreground = new_fg;
    data->gcvals.background = new_bg;
    data->state = sgINIT;
}

static void
freeColorGC(ColorGC * target)
{
    if (target->gc != NULL) {
	XFreeGC(dpy, target->gc);
	memset(target, 0, sizeof(*target));
    }
}

static void
copyColorGC(TextWindow win, ColorGC * target, ColorGC * source)
{
    if (target != source) {
	(void) win;
	TRACE(("copyColorGC %s", traceColorGC(win, source)));
	TRACE((" -> %s\n", traceColorGC(win, target)));
	*target = *source;
	target->gc = NULL;
	if (target->state >= sgINIT)
	    target->state = sgREDO;
    }
}

/*
 * Given the parameters in gcmask/gcvals, allocate a GC if none exists, and
 * return pointer to the target.
 */
ColorGC *
makeColorGC(TextWindow win, ColorGC * target)
{
    int changed = 0;

    (void) win;

    if (target->gc == NULL) {
	target->gc = XCreateGC(dpy, DefaultRootWindow(dpy), target->gcmask, &target->gcvals);
	changed = 1;
    } else if (target->state < sgMADE) {
	XChangeGC(dpy, target->gc, target->gcmask, &target->gcvals);
	changed = 2;
    }

    if (changed) {
	(void) changed;

	TRACE(("makeColorGC(%s) %06lx/%06lx -> %p (%s)\n",
	       traceColorGC(win, target),
	       (long) target->gcvals.foreground,
	       (long) target->gcvals.background,
	       (void *) target->gc,
	       (changed == 1) ? "created" : "changed"));
    }
#ifdef XRENDERFONT
    if (changed) {
	pixelToXftColor(win, &target->xft, target->gcvals.foreground);
    }
#endif
    target->state = sgMADE;
    return target;
}

static void
monochrome_cursor(TextWindow win)
{
    TRACE((T_CALLED "monochrome_cursor\n"));
    win->is_color_cursor = False;
    win->cursor_fg = cur_win->bg;	/* undo our trickery */
    win->cursor_bg = cur_win->fg;
    copyColorGC(win, &win->cc_info, &win->rt_info);
    copyColorGC(win, &win->rc_info, &win->tt_info);
    returnVoid();
}

static int
color_cursor(TextWindow win)
{
    TRACE((T_CALLED "color_cursor\n"));
    initColorGC(win, &cur_win->cc_info, cur_win->fg, cur_win->bg);
    initColorGC(win, &cur_win->rc_info, cur_win->fg, cur_win->bg);

    if (win->cc_info.gc == NULL
	|| win->rc_info.gc == NULL) {
	freeColorGC(&win->cc_info);
	freeColorGC(&win->rc_info);
	monochrome_cursor(win);
    }

    win->is_color_cursor = True;
    returnCode(TRUE);
}

static void
reset_color_gcs(void)
{
    int n;

    for (n = 0; n < NCOLORS; n++) {
	cur_win->fg_info[n].state = sgREDO;
	cur_win->bg_info[n].state = sgREDO;
    }
}

ColorGC *
x_get_color_gc(TextWindow win, int n, Bool normal)
{
    ColorGC *data;

    assert(n >= 0 && n < NCOLORS);

    if (n < 0 || n >= NCOLORS)
	n = 0;			/* shouldn't happen */
    data = (normal
	    ? &(win->fg_info[n])
	    : &(win->bg_info[n]));
    if (win->screen_depth == 1) {
	copyColorGC(win, data, (normal ? &win->tt_info : &win->rt_info));
    } else if (data->state == sgREDO) {
	Pixel new_fg = data->gcvals.foreground;
	Pixel new_bg = data->gcvals.background;

	if (win->bg_follows_fg) {
	    new_fg = (normal
		      ? win->colors_fg[n]
		      : win->colors_bg[n]);
	    new_bg = (normal
		      ? win->colors_bg[n]
		      : win->colors_fg[n]);
	} else {
	    if (normal) {
		new_fg = win->colors_fg[n];
		new_bg = win->bg;
	    } else {
		new_fg = win->bg;
		new_fg = win->colors_fg[n];
	    }
	}
	if (new_fg == new_bg) {
	    new_fg = normal ? win->fg : win->bg;
	    new_bg = normal ? win->bg : win->fg;
	}
	initColorGC(win, data, new_fg, new_bg);
    }
    return data;
}

#if OPT_TRACE
static char *
ColorsOf(Pixel pixel)
{
    static char result[80];

    XColor color;

    color.pixel = pixel;
    XQueryColor(dpy, cur_win->colormap, &color);
    sprintf(result, "%4X/%4X/%4X", color.red, color.green, color.blue);
    return result;
}

static const char *
visibleAtoms(Atom value)
{
    const char *result = 0;
    if (value == None)
	result = "None";
    else if (value == XA_ATOM)
	result = "XA_ATOM";
    else if (value == GetAtom(CLIPBOARD))
	result = "XA_CLIPBOARD";
    else if (value == GetAtom(COMPOUND_TEXT))
	result = "XA_COMPOUND_TEXT";
    else if (value == GetAtom(MULTIPLE))
	result = "MULTIPLE";
    else if (value == XA_PRIMARY)
	result = "XA_PRIMARY";
    else if (value == XA_STRING)
	result = "XA_STRING";
    else if (value == GetAtom(TARGETS))
	result = "XA_TARGETS";
    else if (value == GetAtom(TEXT))
	result = "XA_TEXT";
    else if (value == GetAtom(TIMESTAMP))
	result = "XA_TIMESTAMP";
    else if (value == GetAtom(UTF8_STRING))
	result = "XA_UTF8_STRING";
    else
	result = "unknown";
    return result;
}
#endif

#ifndef PIXMAP_ROOTDIR
#define PIXMAP_ROOTDIR "/usr/share/pixmaps/"
#endif

#ifdef HAVE_LIBXPM
static int
getVisualDepth(void)
{
    XVisualInfo myTemplate, *visInfoPtr;
    int numFound;
    int result = 0;

    myTemplate.visualid = XVisualIDFromVisual(DefaultVisual(dpy,
							    XDefaultScreen(dpy)));
    visInfoPtr = XGetVisualInfo(dpy, (long) VisualIDMask,
				&myTemplate, &numFound);
    if (visInfoPtr != NULL) {
	if (numFound != 0) {
	    result = visInfoPtr->depth;
	}
	XFree(visInfoPtr);
    }
    return result;
}
#endif /* HAVE_LIBXPM */

static char *
x_find_icon(char **work, int *state, const char *suffix)
{
    const char *filename = cur_win->iconname;
    const char *prefix = "";
    char *result = NULL;
    size_t length;

    switch (*state) {
    case 0:
	suffix = "";
	break;
    case 1:
	break;
    case 2:
	if (!strncmp(filename, "/", 1) ||
	    !strncmp(filename, "./", 2) ||
	    !strncmp(filename, "../", 3))
	    goto giveup;
	prefix = PIXMAP_ROOTDIR;
	suffix = "";
	break;
    case 3:
	prefix = PIXMAP_ROOTDIR;
	break;
      giveup:
    default:
	*state = -1;
	break;
    }
    if (*state >= 0) {
	if (*work) {
	    free(*work);
	    *work = NULL;
	}
	length = 3 + strlen(prefix) + strlen(filename) + strlen(suffix);
	if ((result = malloc(length)) != NULL) {
	    sprintf(result, "%s%s%s", prefix, filename, suffix);
	    *work = result;
	}
	*state += 1;
	TRACE(("x_find_icon %d:%s\n", *state, result));
    }
    return result;
}

static void
x_load_icon(void)
{
    Pixmap myIcon = 0;
    Pixmap myMask = 0;
    char *workname = NULL;

    TRACE((T_CALLED "x_load_icon\n"));
    /*
     * Use the compiled-in icon as a resource default.
     */
#if OPT_X11_ICON
    {
#ifdef VILE_ICON
# include VILE_ICON
#else
# ifdef HAVE_LIBXPM
#  include <icons/vile.xpm>
# else
#  if SYS_VMS
#   include "icons/vile.xbm"
#  else
#   include <icons/vile.xbm>
#  endif
# endif
#endif
#ifdef HAVE_LIBXPM
	if (XpmCreatePixmapFromData(dpy,
				    DefaultRootWindow(dpy),
				    vile, &myIcon, &myMask, NULL) != 0) {
	    myIcon = 0;
	    myMask = 0;
	}
#else
	myIcon = XCreateBitmapFromData(dpy,
				       DefaultRootWindow(dpy),
				       (char *) vile_bits,
				       vile_width,
				       vile_height);
#endif
    }
#endif /* OPT_X11_ICON */

    if (!isEmpty(cur_win->iconname)) {
	int state = 0;
#ifdef HAVE_LIBXPM
	while (x_find_icon(&workname, &state, ".xpm") != NULL) {
	    Pixmap resIcon = 0;
	    Pixmap shapemask = 0;
	    XpmAttributes attributes;

	    attributes.depth = (unsigned) getVisualDepth();
	    attributes.valuemask = XpmDepth;

	    if (XpmReadFileToPixmap(dpy,
				    DefaultRootWindow(dpy),
				    workname,
				    &resIcon,
				    &shapemask,
				    &attributes) == XpmSuccess) {
		myIcon = resIcon;
		myMask = shapemask;
		TRACE(("...success\n"));
		break;
	    }
	}
#else
	while (x_find_icon(&workname, &state, ".xbm") != 0) {
	    Pixmap resIcon = 0;
	    unsigned width;
	    unsigned height;
	    int x_hot;
	    int y_hot;

	    if (XReadBitmapFile(dpy,
				DefaultRootWindow(dpy),
				workname,
				&width, &height,
				&resIcon,
				&x_hot, &y_hot) == BitmapSuccess) {
		myIcon = resIcon;
		TRACE(("...success\n"));
		break;
	    }
	}
#endif
    }

    if (myIcon != 0) {
	XWMHints *hints = XGetWMHints(dpy, XtWindow(cur_win->top_widget));
	if (!hints)
	    hints = XAllocWMHints();

	if (hints) {
	    hints->flags |= IconPixmapHint;
	    hints->icon_pixmap = myIcon;
	    if (myMask) {
		hints->flags |= IconMaskHint;
		hints->icon_mask = myMask;
	    }

	    XSetWMHints(dpy, XtWindow(cur_win->top_widget), hints);
	    XFree(hints);
	}
    }

    if (workname != NULL)
	free(workname);

    returnVoid();
}

/* ARGSUSED */
int
x_preparse_args(int *pargc, char ***pargv)
{
    XVileFont *pfont;
    XGCValues gcvals;
    ULONG gcmask;
    int geo_mask, startx, starty;
    int i;
    int status = TRUE;
    Cardinal start_cols, start_rows;
    const char *xvile_class = MY_CLASS;
    static XrmOptionDescRec options[] =
    {
	{"-t", (char *) 0, XrmoptionSkipArg, (caddr_t) 0},
	{"-fork", "*forkOnStartup", XrmoptionNoArg, "true"},
	{"+fork", "*forkOnStartup", XrmoptionNoArg, "false"},
	{"-leftbar", "*scrollbarOnLeft", XrmoptionNoArg, "true"},
	{"-rightbar", "*scrollbarOnLeft", XrmoptionNoArg, "false"},
	{"-class", NULL, XrmoptionSkipArg, (caddr_t) 0},
    };
#if MOTIF_WIDGETS
    static XtActionsRec new_actions[] =
    {
	{"ConfigureBar", configure_bar}
    };
    static String scrollbars_translations =
    "#override \n\
		Ctrl<Btn1Down>:ConfigureBar(Split) \n\
		Ctrl<Btn2Down>:ConfigureBar(Kill) \n\
		Ctrl<Btn3Down>:ConfigureBar(Only)";
    static String fallback_resources[] =
    {
	"*scrollPane.background:grey80",
	"*scrollbar.background:grey60",
	NULL
    };
#else
#if OPT_KEV_DRAGGING
    static XtActionsRec new_actions[] =
    {
	{"ConfigureBar", configure_bar},
	{"DoScroll", do_scroll},
	{"ResizeBar", resize_bar}
    };
    static String scrollbars_translations =
    "#override \n\
		Ctrl<Btn1Down>:ConfigureBar(Split) \n\
		Ctrl<Btn2Down>:ConfigureBar(Kill) \n\
		Ctrl<Btn3Down>:ConfigureBar(Only) \n\
		<Btn1Down>:DoScroll(Forward) \n\
		<Btn2Down>:DoScroll(StartDrag) \n\
		<Btn3Down>:DoScroll(Backward) \n\
		<Btn2Motion>:DoScroll(Drag) \n\
		<BtnUp>:DoScroll(End)";
    static String resizeGrip_translations =
    "#override \n\
		<BtnDown>:ResizeBar(Start) \n\
		<BtnMotion>:ResizeBar(Drag) \n\
		<BtnUp>:ResizeBar(End)";
    static String fallback_resources[] =
    {
	NULL
    };
#else
    static XtActionsRec new_actions[] =
    {
	{"ConfigureBar", configure_bar},
	{"ResizeBar", resize_bar}
    };
    static String scrollbars_translations =
    "#override \n\
		Ctrl<Btn1Down>:ConfigureBar(Split) \n\
		Ctrl<Btn2Down>:ConfigureBar(Kill) \n\
		Ctrl<Btn3Down>:ConfigureBar(Only)";
    static String resizeGrip_translations =
    "#override \n\
		<BtnDown>:ResizeBar(Start) \n\
		<BtnMotion>:ResizeBar(Drag) \n\
		<BtnUp>:ResizeBar(End)";
    static String fallback_resources[] =
    {
	NULL
    };
#endif /* OPT_KEV_DRAGGING */
#if OPT_XAW_SCROLLBARS
    static char solid_pixmap_bits[] =
    {'\003', '\003'};
#endif
    static char stippled_pixmap_bits[] =
    {'\002', '\001'};
#endif /* MOTIF_WIDGETS */

    TRACE((T_CALLED "x_preparse_args\n"));

    for (i = 1; i < (*pargc) - 1; i++) {
	char *param = (*pargv)[i];
	size_t len = strlen(param);
	if (len > 1) {
#if OPT_TITLE
	    /*
	     * If user sets the title explicitly, he probably will not like
	     * allowing xvile to set it automatically when visiting a new
	     * buffer.
	     */
	    if (!strncmp(param, "-title", len))
		auto_set_title = FALSE;
#endif
	    if (!strncmp(param, "-class", len))
		xvile_class = (*pargv)[++i];
	}
    }
    if (isEmpty(xvile_class))
	xvile_class = MY_CLASS;

    XtSetErrorHandler(initial_error_handler);
    memset(cur_win, 0, sizeof(*cur_win));
    cur_win->top_widget = XtVaAppInitialize(&cur_win->app_context,
					    xvile_class,
					    options, XtNumber(options),
					    pargc, *pargv,
					    fallback_resources,
					    Nval(XtNgeometry, NULL),
					    Nval(XtNinput, TRUE),
					    NULL);
    XtSetErrorHandler(runtime_error_handler);
    dpy = XtDisplay(cur_win->top_widget);

    XtVaGetValues(cur_win->top_widget,
		  XtNcolormap, &cur_win->colormap,
		  NULL);

    XtVaGetValues(cur_win->top_widget,
		  XtNdepth, &cur_win->screen_depth,
		  NULL);

    XtGetApplicationResources(cur_win->top_widget,
			      (XtPointer) cur_win,
			      app_resources,
			      XtNumber(app_resources),
			      (ArgList) 0,
			      0);

    TRACE(("** ApplicationResources:\n"));

#if OPT_KEV_DRAGGING
    TRACE_RES_I(XtNscrollRepeatTimeout, scroll_repeat_timeout);
#endif
    TRACE_RES_I(XtNscrollRepeatInterval, scroll_repeat_interval);
    TRACE_RES_B(XtNreverseVideo, reverse_video);
#ifdef XRENDERFONT
    TRACE_RES_S(XtNfontFamily, starting_fontname);
#else
    TRACE_RES_S(XtNfont, starting_fontname);
#endif
    TRACE_RES_S(XtNgeometry, geometry);
    TRACE_RES_P(XtNforeground, fg);
    TRACE_RES_P(XtNbackground, bg);
    TRACE_RES_B(XtNforkOnStartup, fork_on_startup);
    TRACE_RES_B(XtNfocusFollowsMouse, focus_follows_mouse);
    TRACE_RES_I(XtNmultiClickTime, click_timeout);
    TRACE_RES_S(XtNcharClass, multi_click_char_class);
    TRACE_RES_B(XtNscrollbarOnLeft, scrollbar_on_left);
    TRACE_RES_I(XtNscrollbarWidth, pane_width);
    TRACE_RES_I(XtNwheelScrollAmount, wheel_scroll_amount);
#if OPT_MENUS
    TRACE_RES_I(XtNmenuHeight, menu_height);
#endif
#if OPT_MENUS_COLORED
    TRACE_RES_P(XtNmenuForeground, menubar_fg);
    TRACE_RES_P(XtNmenuBackground, menubar_bg);
#endif
    TRACE_RES_B(XtNpersistentSelections, persistent_selections);
    TRACE_RES_B(XtNselectionSetsDOT, selection_sets_DOT);
    TRACE_RES_I(XtNblinkInterval, blink_interval);
#if OPT_INPUT_METHOD
    TRACE_RES_B(XtNopenIm, open_im);
    TRACE_RES_B(XtNinputMethod, rs_inputMethod);
    TRACE_RES_B(XtNpreeditType, rs_preeditType);
    TRACE_RES_B(XtNximFont, rs_imFont);
#endif

    if (cur_win->fork_on_startup)
	(void) newprocessgroup(TRUE, 1);

    if (SamePixel(cur_win->bg, cur_win->fg))
	cur_win->fg = BlackPixel(dpy, DefaultScreen(dpy));
    if (SamePixel(cur_win->bg, cur_win->fg))
	cur_win->bg = WhitePixel(dpy, DefaultScreen(dpy));

    if (cur_win->reverse_video) {
	Pixel temp = cur_win->fg;
	cur_win->fg = cur_win->bg;
	cur_win->bg = temp;
    }
    cur_win->default_fg = cur_win->fg;
    cur_win->default_bg = cur_win->bg;

#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
    XtGetSubresources(cur_win->top_widget,
		      (XtPointer) cur_win,
		      "scrollbar",
		      "Scrollbar",
		      scrollbar_resources,
		      XtNumber(scrollbar_resources),
		      (ArgList) 0,
		      0);
    TRACE(("SubResources:scrollbar\n"));
    TRACE_RES_P(XtNforeground, scrollbar_fg);
    TRACE_RES_P(XtNbackground, scrollbar_bg);
    TRACE_RES_B(XtNsliderIsSolid, slider_is_solid);
#endif /* OPT_KEV_SCROLLBARS */

    XtGetSubresources(cur_win->top_widget,
		      (XtPointer) cur_win,
		      "modeline",
		      "Modeline",
		      modeline_resources,
		      XtNumber(modeline_resources),
		      (ArgList) 0,
		      0);
    TRACE(("SubResources:modeline\n"));
    TRACE_RES_P(XtNforeground, modeline_fg);
    TRACE_RES_P(XtNbackground, modeline_bg);
    TRACE_RES_P(XtNfocusForeground, modeline_focus_fg);
    TRACE_RES_P(XtNfocusBackground, modeline_focus_bg);

    XtGetSubresources(cur_win->top_widget,
		      (XtPointer) cur_win,
		      "selection",
		      "Selection",
		      selection_resources,
		      XtNumber(selection_resources),
		      (ArgList) 0,
		      0);
    TRACE(("SubResources:selection\n"));
    TRACE_RES_P(XtNforeground, selection_fg);
    TRACE_RES_P(XtNbackground, selection_bg);

    XtGetSubresources(cur_win->top_widget,
		      (XtPointer) cur_win,
		      "cursor",
		      "Cursor",
		      cursor_resources,
		      XtNumber(cursor_resources),
		      (ArgList) 0,
		      0);
    TRACE(("SubResources:cursor\n"));
    TRACE_RES_P(XtNforeground, cursor_fg);
    TRACE_RES_P(XtNbackground, cursor_bg);

    XtGetSubresources(cur_win->top_widget,
		      (XtPointer) cur_win,
		      "pointer",
		      "Pointer",
		      pointer_resources,
		      XtNumber(pointer_resources),
		      (ArgList) 0,
		      0);
    TRACE(("SubResources:pointer\n"));
    TRACE_RES_P(XtNforeground, pointer_fg);
    TRACE_RES_P(XtNbackground, pointer_bg);
    TRACE_RES_P(XtNnormalShape, normal_pointer);
#if OPT_WORKING
    TRACE_RES_P(XtNwatchShape, watch_pointer);
#endif

    /*
     * Try to keep the default for fcolor0 looking like something other than
     * white.
     */
    if (SamePixel(cur_win_rec.fg, WhitePixel(dpy, DefaultScreen(dpy)))) {
	static Pixel black;
	TRACE(("force default value of fcolor0 to Black\n"));
	black = BlackPixel(dpy, DefaultScreen(dpy));
	color_resources[0].default_addr = (XtPointer) &black;
    }

    XtGetSubresources(cur_win->top_widget,
		      (XtPointer) cur_win,
		      "color",
		      "Color",
		      color_resources,
		      XtNumber(color_resources),
		      (ArgList) 0,
		      0);
    TRACE(("SubResources:color\n"));
    TRACE_RES_P(XtNfcolor0, colors_fg[0]);
    TRACE_RES_P(XtNbcolor0, colors_bg[0]);
    TRACE_RES_P(XtNfcolor1, colors_fg[1]);
    TRACE_RES_P(XtNbcolor1, colors_bg[1]);
    TRACE_RES_P(XtNfcolor2, colors_fg[2]);
    TRACE_RES_P(XtNbcolor2, colors_bg[2]);
    TRACE_RES_P(XtNfcolor3, colors_fg[3]);
    TRACE_RES_P(XtNbcolor3, colors_bg[3]);
    TRACE_RES_P(XtNfcolor4, colors_fg[4]);
    TRACE_RES_P(XtNbcolor4, colors_bg[4]);
    TRACE_RES_P(XtNfcolor5, colors_fg[5]);
    TRACE_RES_P(XtNbcolor5, colors_bg[5]);
    TRACE_RES_P(XtNfcolor6, colors_fg[6]);
    TRACE_RES_P(XtNbcolor6, colors_bg[6]);
    TRACE_RES_P(XtNfcolor7, colors_fg[7]);
    TRACE_RES_P(XtNbcolor7, colors_bg[7]);
    TRACE_RES_P(XtNfcolor8, colors_fg[8]);
    TRACE_RES_P(XtNbcolor8, colors_bg[8]);
    TRACE_RES_P(XtNfcolor9, colors_fg[9]);
    TRACE_RES_P(XtNbcolor9, colors_bg[9]);
    TRACE_RES_P(XtNfcolorA, colors_fg[10]);
    TRACE_RES_P(XtNbcolorA, colors_bg[10]);
    TRACE_RES_P(XtNfcolorB, colors_fg[11]);
    TRACE_RES_P(XtNbcolorB, colors_bg[11]);
    TRACE_RES_P(XtNfcolorC, colors_fg[12]);
    TRACE_RES_P(XtNbcolorC, colors_bg[12]);
    TRACE_RES_P(XtNfcolorD, colors_fg[13]);
    TRACE_RES_P(XtNbcolorD, colors_bg[13]);
    TRACE_RES_P(XtNfcolorE, colors_fg[14]);
    TRACE_RES_P(XtNbcolorE, colors_bg[14]);
    TRACE_RES_P(XtNfcolorF, colors_fg[15]);
    TRACE_RES_P(XtNbcolorF, colors_bg[15]);

    pfont = xvileQueryFont(dpy, cur_win, cur_win->starting_fontname);
    if (!pfont) {
	pfont = xvileQueryFont(dpy, cur_win, FONTNAME);
	if (!pfont) {
	    if (strcmp(cur_win->starting_fontname, FONTNAME)) {
		(void) fprintf(stderr,
			       "couldn't get font \"%s\" or \"%s\", exiting\n",
			       cur_win->starting_fontname, FONTNAME);
	    } else {
		(void) fprintf(stderr,
			       "couldn't get font \"%s\", exiting\n",
			       FONTNAME);
	    }
	    ExitProgram(BADEXIT);
	}
    }
    default_font = strmalloc(cur_win->starting_fontname);
    (void) set_character_class(cur_win->multi_click_char_class);

    /*
     * Look at our copy of the geometry resource to obtain the dimensions of
     * the window in characters.  We've provided a default value of 80x36
     * so there'll always be something to parse.  We still need to check
     * the return mask since the user may specify a position, but no size.
     */
    geo_mask = XParseGeometry(cur_win->geometry,
			      &startx, &starty,
			      &start_cols, &start_rows);

    cur_win->rows = (geo_mask & HeightValue) ? start_rows : 36;
    cur_win->cols = (geo_mask & WidthValue) ? start_cols : 80;

    /*
     * Fix up the geometry resource of top level shell providing initial
     * position if so requested by user.
     */

    if (geo_mask & (XValue | YValue)) {
	char *gp = cur_win->geometry;
	while (*gp && *gp != '+' && *gp != '-')
	    gp++;		/* skip over width and height */
	if (*gp)
	    XtVaSetValues(cur_win->top_widget,
			  Nval(XtNgeometry, gp),
			  NULL);
    }

    /* Sanity check values obtained from XtGetApplicationResources */
    CHECK_MIN_MAX(cur_win->pane_width, PANE_WIDTH_MIN, PANE_WIDTH_MAX);

#if MOTIF_WIDGETS
    cur_win->form_widget =
	XtVaCreateManagedWidget("form",
				xmFormWidgetClass,
				cur_win->top_widget,
				NULL);
#else
#if ATHENA_WIDGETS && OPT_MENUS
    cur_win->pane_widget =
	XtVaCreateManagedWidget("pane",
				panedWidgetClass,
				cur_win->top_widget,
				NULL);
    cur_win->menu_widget =
	XtVaCreateManagedWidget("menubar",
				boxWidgetClass,
				cur_win->pane_widget,
				Nval(XtNshowGrip, False),
				NULL);
    cur_win->form_widget =
	XtVaCreateManagedWidget("form",
#if KEV_WIDGETS			/* FIXME */
				bbWidgetClass,
#else
				formWidgetClass,
#endif
				cur_win->pane_widget,
				Nval(XtNwidth, (x_width(cur_win)
						+ cur_win->pane_width
						+ 2)),
				Nval(XtNheight, x_height(cur_win)),
				Nval(XtNbackground, cur_win->bg),
				Sval(XtNbottom, XtChainBottom),
				Sval(XtNleft, XtChainLeft),
				Sval(XtNright, XtChainRight),
				Nval(XtNfromVert, cur_win->menu_widget),
				Nval(XtNvertDistance, 0),
				NULL);
#else /* !(ATHENA_WIDGETS && OPT_MENUS) */
#if ATHENA_WIDGETS
    cur_win->form_widget =
	XtVaCreateManagedWidget("form",
#if KEV_WIDGETS			/* FIXME */
				bbWidgetClass,
#else
				formWidgetClass,
#endif
				cur_win->top_widget,
				XtNwidth, (x_width(cur_win)
					   + cur_win->pane_width
					   + 2),
				XtNheight, x_height(cur_win),
				XtNbackground, cur_win->bg,
				NULL);
#else
#if NO_WIDGETS
    cur_win->form_widget =
	XtVaCreateManagedWidget("form",
				bbWidgetClass,
				cur_win->top_widget,
				Nval(XtNwidth, (x_width(cur_win)
						+ cur_win->pane_width
						+ 2)),
				Nval(XtNheight, x_height(cur_win)),
				Nval(XtNbackground, cur_win->bg),
				NULL);
#endif /* NO_WIDGETS */
#endif /* ATHENA_WIDGETS */
#endif /* ATHENA_WIDGETS && OPT_MENUS */
#endif /* MOTIF_WIDGETS */

#if OPT_MENUS
#if MOTIF_WIDGETS
    cur_win->menu_widget = XmCreateMenuBar(cur_win->form_widget, "menub",
					   NULL, 0);

    XtVaSetValues(cur_win->menu_widget,
		  Nval(XmNtopAttachment, XmATTACH_FORM),
		  Nval(XmNleftAttachment, XmATTACH_FORM),
		  Nval(XmNbottomAttachment, XmATTACH_NONE),
		  Nval(XmNrightAttachment, XmATTACH_FORM),
		  NULL);
    XtManageChild(cur_win->menu_widget);
#endif
    status = do_menu(cur_win->menu_widget);
#endif

    cur_win->screen =
	XtVaCreateManagedWidget("screen",
#if MOTIF_WIDGETS
				xmPrimitiveWidgetClass,
#else
				coreWidgetClass,
#endif
				cur_win->form_widget,
				Nval(XtNwidth, x_width(cur_win)),
				Nval(XtNheight, x_height(cur_win)),
				Nval(XtNborderWidth, 0),
				Nval(XtNbackground, cur_win->bg),
#if MOTIF_WIDGETS
				Nval(XmNhighlightThickness, 0),
				Nval(XmNresizable, TRUE),
				Nval(XmNbottomAttachment, XmATTACH_FORM),
#if OPT_MENUS
				Nval(XmNtopAttachment, XmATTACH_WIDGET),
				Nval(XmNtopWidget, cur_win->menu_widget),
				Nval(XmNtopOffset, 2),
#else
				Nval(XmNtopAttachment, XmATTACH_FORM),
#endif
				Nval(XmNleftAttachment, XmATTACH_FORM),
				Nval(XmNrightAttachment, XmATTACH_NONE),
#else
#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
#if !OPT_KEV_SCROLLBARS
				Sval(XtNleft, XtChainLeft),
				Sval(XtNright, XtChainRight),
#endif
				Nval(XtNx, (cur_win->scrollbar_on_left
					    ? cur_win->pane_width + 2
					    : 0)),
				Nval(XtNy, 0),
#endif /* OPT_KEV_SCROLLBARS */
#endif /* MOTIF_WIDGETS */
				NULL);

#if defined(LESSTIF_VERSION)
    /*
     * Lesstif (0.81) seems to install translations that cause certain
     * keys (like TAB) to manipulate the focus in addition to their
     * functions within xvile.  This leads to frustration when you are
     * trying to insert text, and find the focus shifting to a scroll
     * bar or whatever.  To fix this problem, we remove those nasty
     * translations here.
     *
     * Aside from this little nit, "lesstif" seems to work admirably
     * with xvile.  Just build with screen=motif.  You can find
     * lesstif at:  ftp://ftp.lesstif.org/pub/hungry/lesstif
     */

    XtUninstallTranslations(cur_win->screen);
#endif /* LESSTIF_VERSION */

    /* Initialize graphics context for display of normal and reverse text */
    initColorGC(cur_win, &cur_win->tt_info, cur_win->fg, cur_win->bg);

    cur_win->exposed = FALSE;
    cur_win->visibility = VisibilityUnobscured;

    initColorGC(cur_win, &cur_win->rt_info, cur_win->bg, cur_win->fg);

    cur_win->bg_follows_fg = (Boolean) (gbcolor == ENUM_FCOLOR);

#define COLOR_FMT " %06lx %s"
#define COLOR_ARG(name) name, ColorsOf(name)
#define COLOR_ONE(name) "%-20s " COLOR_FMT "\n", #name, COLOR_ARG(cur_win->name)

    TRACE(("colors_fg/colors_bg pixel values:\n"));
    for (i = 0; i < NCOLORS; i++) {
	ctrans[i] = i;
	TRACE(("   [%2d] " COLOR_FMT " " COLOR_FMT "\n", i,
	       COLOR_ARG(cur_win->colors_fg[i]),
	       COLOR_ARG(cur_win->colors_bg[i])));
    }
    reset_color_gcs();

    TRACE((COLOR_ONE(fg)));
    TRACE((COLOR_ONE(bg)));

    TRACE((COLOR_ONE(selection_fg)));
    TRACE((COLOR_ONE(selection_bg)));

    /* Initialize graphics context for display of selections */
    if (
#ifdef XRENDERFONT
	   0 &&			/* FIXME - difference should not exist */
#endif
	   (cur_win->screen_depth == 1
	    || SamePixel(cur_win->selection_bg, cur_win->selection_fg)
	    || (SamePixel(cur_win->fg, cur_win->selection_fg)
		&& SamePixel(cur_win->bg, cur_win->selection_bg))
	    || (SamePixel(cur_win->fg, cur_win->selection_bg)
		&& SamePixel(cur_win->bg, cur_win->selection_fg)))) {
	TRACE(("...Forcing selection GC to reverse\n"));
	copyColorGC(cur_win, &cur_win->ss_info, &cur_win->rt_info);
	copyColorGC(cur_win, &cur_win->rs_info, &cur_win->tt_info);
    } else {
	initColorGC(cur_win, &cur_win->ss_info,
		    cur_win->selection_fg, cur_win->selection_bg);
	initColorGC(cur_win, &cur_win->rs_info,
		    cur_win->selection_bg, cur_win->selection_fg);
	TRACE(("...Created selection GC\n"));
    }

    TRACE((COLOR_ONE(modeline_fg)));
    TRACE((COLOR_ONE(modeline_bg)));

    /*
     * Initialize graphics context for display of normal modelines.
     * Portions of the modeline are never displayed in reverse video (wrt
     * the modeline) so there is no corresponding reverse video gc.
     */
    if (cur_win->screen_depth == 1
	|| SamePixel(cur_win->modeline_bg, cur_win->modeline_fg)
	|| (SamePixel(cur_win->fg, cur_win->modeline_fg)
	    && SamePixel(cur_win->bg, cur_win->modeline_bg))
	|| (SamePixel(cur_win->fg, cur_win->modeline_bg)
	    && SamePixel(cur_win->bg, cur_win->modeline_fg))) {
	copyColorGC(cur_win, &cur_win->oo_info, &cur_win->rt_info);
	copyColorGC(cur_win, &cur_win->ro_info, &cur_win->tt_info);
	TRACE(("...Forcing modeline GC to reverse\n"));
    } else {
	initColorGC(cur_win, &cur_win->oo_info,
		    cur_win->modeline_fg, cur_win->modeline_bg);
	initColorGC(cur_win, &cur_win->ro_info,
		    cur_win->modeline_bg, cur_win->modeline_fg);
	TRACE(("...Created modeline GC\n"));
    }

    TRACE((COLOR_ONE(modeline_focus_fg)));
    TRACE((COLOR_ONE(modeline_focus_bg)));

    /*
     * Initialize graphics context for display of modelines which indicate
     * that the corresponding window has focus.
     */
    if (cur_win->screen_depth == 1
	|| SamePixel(cur_win->modeline_focus_bg, cur_win->modeline_focus_fg)
	|| (SamePixel(cur_win->fg, cur_win->modeline_focus_fg)
	    && SamePixel(cur_win->bg, cur_win->modeline_focus_bg))
	|| (SamePixel(cur_win->fg, cur_win->modeline_focus_bg)
	    && SamePixel(cur_win->bg, cur_win->modeline_focus_fg))) {
	copyColorGC(cur_win, &cur_win->mm_info, &cur_win->rt_info);
	copyColorGC(cur_win, &cur_win->rm_info, &cur_win->tt_info);
	TRACE(("...Forcing modeline focus GC to reverse\n"));
    } else {
	initColorGC(cur_win, &cur_win->mm_info,
		    cur_win->modeline_focus_fg, cur_win->modeline_focus_bg);
	initColorGC(cur_win, &cur_win->rm_info,
		    cur_win->modeline_focus_bg, cur_win->modeline_focus_fg);
	TRACE(("...Created modeline focus GC\n"));
    }

    TRACE((COLOR_ONE(cursor_fg)));
    TRACE((COLOR_ONE(cursor_bg)));

    /* Initialize cursor graphics context and flag which indicates how to
     * display cursor.
     */
    if (cur_win->screen_depth == 1
	|| SamePixel(cur_win->cursor_bg, cur_win->cursor_fg)
	|| (SamePixel(cur_win->fg, cur_win->cursor_fg)
	    && SamePixel(cur_win->bg, cur_win->cursor_bg))
	|| (SamePixel(cur_win->fg, cur_win->cursor_bg)
	    && SamePixel(cur_win->bg, cur_win->cursor_fg))) {
	monochrome_cursor(cur_win);
	TRACE(("...Forcing monochrome cursor\n"));
    } else if (color_cursor(cur_win)) {
	TRACE(("...Created color cursor\n"));
	x_set_cursor_color(-1);
    }
#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
    if (SamePixel(cur_win->scrollbar_bg, cur_win->scrollbar_fg)) {
	cur_win->scrollbar_bg = cur_win->bg;
	cur_win->scrollbar_fg = cur_win->fg;
    }
    if (cur_win->screen_depth == 1
	|| too_light_or_too_dark(cur_win->scrollbar_fg))
	cur_win->slider_is_solid = False;
#endif /* OPT_KEV_SCROLLBARS */

#if OPT_XAW_SCROLLBARS
    cur_win->thumb_bm =
	XCreateBitmapFromData(dpy, DefaultRootWindow(dpy),
			      cur_win->slider_is_solid
			      ? solid_pixmap_bits
			      : stippled_pixmap_bits,
			      2, 2);
#endif /* OPT_XAW_SCROLLBARS */

#if OPT_KEV_SCROLLBARS
    gcvals.background = cur_win->scrollbar_bg;
    if (!cur_win->slider_is_solid) {
	gcmask = GCFillStyle | GCStipple | GCForeground | GCBackground;
	gcvals.foreground = cur_win->scrollbar_fg;
	gcvals.fill_style = FillOpaqueStippled;
	gcvals.stipple =
	    XCreatePixmapFromBitmapData(dpy,
					DefaultRootWindow(dpy),
					stippled_pixmap_bits,
					2, 2,
					1L, 0L,
					1);
    } else {
	gcmask = GCForeground | GCBackground;
	gcvals.foreground = cur_win->scrollbar_fg;
    }
    gcmask |= GCGraphicsExposures;
    gcvals.graphics_exposures = False;
    cur_win->scrollbar_gc = XCreateGC(dpy,
				      DefaultRootWindow(dpy),
				      gcmask, &gcvals);

    if (cur_win->screen_depth >= 6 && cur_win->slider_is_solid) {
	Pixel fg_light, fg_dark, bg_light, bg_dark;
	if (alloc_shadows(cur_win->scrollbar_fg, &fg_light, &fg_dark)
	    && alloc_shadows(cur_win->scrollbar_bg, &bg_light, &bg_dark)) {
	    GC gc;
	    Pixmap my_slider_pixmap;
	    cur_win->slider_is_3D = True;

	    cur_win->trough_pixmap =
		XCreatePixmap(dpy, DefaultRootWindow(dpy),
			      cur_win->pane_width + 2,
			      16, cur_win->screen_depth);

#define TROUGH_HT 16
	    gcvals.foreground = cur_win->scrollbar_bg;
	    gc = XCreateGC(dpy, DefaultRootWindow(dpy), gcmask, &gcvals);
	    XFillRectangle(dpy, cur_win->trough_pixmap, gc, 0, 0,
			   cur_win->pane_width + 2, TROUGH_HT);
	    XSetForeground(dpy, gc, bg_dark);
	    XFillRectangle(dpy, cur_win->trough_pixmap, gc, 0, 0, 2, TROUGH_HT);
	    XSetForeground(dpy, gc, bg_light);
	    XFillRectangle(dpy, cur_win->trough_pixmap, gc,
			   (int) cur_win->pane_width, 0, 2, TROUGH_HT);

	    my_slider_pixmap = XCreatePixmap(dpy, DefaultRootWindow(dpy),
					     cur_win->pane_width - 2,
					     SP_HT,
					     cur_win->screen_depth);

	    XSetForeground(dpy, gc, cur_win->scrollbar_fg);
	    XFillRectangle(dpy, my_slider_pixmap, gc, 0, 0,
			   cur_win->pane_width - 2, SP_HT);
	    XSetForeground(dpy, gc, fg_light);
	    XFillRectangle(dpy, my_slider_pixmap, gc, 0, 0, 2, SP_HT);
	    XSetForeground(dpy, gc, fg_dark);
	    XFillRectangle(dpy, my_slider_pixmap, gc,
			   (int) cur_win->pane_width - 4, 0, 2, SP_HT);

	    XSetTile(dpy, cur_win->scrollbar_gc, my_slider_pixmap);
	    XSetFillStyle(dpy, cur_win->scrollbar_gc, FillTiled);
	    XSetTSOrigin(dpy, cur_win->scrollbar_gc, 2, 0);

	    cur_win->slider_pixmap =
		XCreatePixmap(dpy, DefaultRootWindow(dpy),
			      cur_win->pane_width - 2,
			      SP_HT, cur_win->screen_depth);
	    XCopyArea(dpy, my_slider_pixmap, cur_win->slider_pixmap, gc,
		      0, 0, cur_win->pane_width - 2, SP_HT, 0, 0);

	    /* Draw top bevel */
	    XSetForeground(dpy, gc, fg_light);
	    XDrawLine(dpy, cur_win->slider_pixmap, gc,
		      0, 0, (int) cur_win->pane_width - 3, 0);
	    XDrawLine(dpy, cur_win->slider_pixmap, gc,
		      0, 1, (int) cur_win->pane_width - 4, 1);

	    /* Draw bottom bevel */
	    XSetForeground(dpy, gc, fg_dark);
	    XDrawLine(dpy, cur_win->slider_pixmap, gc,
		      2, SP_HT - 2, (int) cur_win->pane_width - 3, SP_HT - 2);
	    XDrawLine(dpy, cur_win->slider_pixmap, gc,
		      1, SP_HT - 1, (int) cur_win->pane_width - 3, SP_HT - 1);

	    XFreeGC(dpy, gc);
	}
    }
#endif /* OPT_KEV_SCROLLBARS */

    XtAppAddActions(cur_win->app_context, new_actions, XtNumber(new_actions));
    cur_win->my_scrollbars_trans = XtParseTranslationTable(scrollbars_translations);

#if MOTIF_WIDGETS
    cur_win->pane =
	XtVaCreateManagedWidget("scrollPane",
				xmPanedWindowWidgetClass,
				cur_win->form_widget,
				Nval(XtNwidth, cur_win->pane_width),
				Nval(XmNbottomAttachment, XmATTACH_FORM),
				Nval(XmNtraversalOn, False),	/* Added by gdr */
#if OPT_MENUS
				Nval(XmNtopAttachment, XmATTACH_WIDGET),
				Nval(XmNtopWidget, cur_win->menu_widget),
#else
				Nval(XmNtopAttachment, XmATTACH_FORM),
#endif
				Nval(XmNleftAttachment, XmATTACH_WIDGET),
				Nval(XmNleftWidget, cur_win->screen),
				Nval(XmNrightAttachment, XmATTACH_FORM),
				Nval(XmNspacing, cur_win->char_height),
				Nval(XmNsashIndent, 2),
				Nval(XmNsashWidth, cur_win->pane_width - 4),
				Nval(XmNmarginHeight, 0),
				Nval(XmNmarginWidth, 0),
				Nval(XmNseparatorOn, FALSE),
				NULL);
#else
#if OPT_XAW_SCROLLBARS
    cur_win->my_resizeGrip_trans = XtParseTranslationTable(resizeGrip_translations);
    cur_win->pane =
	XtVaCreateManagedWidget("scrollPane",
				formWidgetClass,
				cur_win->form_widget,
				Nval(XtNwidth, cur_win->pane_width + 2),
				Nval(XtNheight, ((int) x_height(cur_win)
						 - cur_win->char_height)),
				Nval(XtNx, (cur_win->scrollbar_on_left
					    ? 0
					    : x_width(cur_win))),
				Nval(XtNy, 0),
				Nval(XtNborderWidth, 0),
				Nval(XtNbackground, cur_win->modeline_bg),
				Nval(XtNfromHoriz, (cur_win->scrollbar_on_left
						    ? NULL
						    : cur_win->screen)),
				Nval(XtNhorizDistance, 0),
				Sval(XtNleft, ((cur_win->scrollbar_on_left)
					       ? XtChainLeft
					       : XtChainRight)),
				Sval(XtNright, ((cur_win->scrollbar_on_left)
						? XtChainLeft
						: XtChainRight)),
				NULL);
    if (cur_win->scrollbar_on_left)
	XtVaSetValues(cur_win->screen,
		      Nval(XtNfromHoriz, cur_win->pane),
		      NULL);
#else
#if OPT_KEV_SCROLLBARS
    cur_win->my_resizeGrip_trans = XtParseTranslationTable(resizeGrip_translations);
    cur_win->pane =
	XtVaCreateManagedWidget("scrollPane",
				bbWidgetClass,
				cur_win->form_widget,
				Nval(XtNwidth, cur_win->pane_width + 2),
				Nval(XtNheight, ((int) x_height(cur_win)
						 - cur_win->char_height)),
				Nval(XtNx, (cur_win->scrollbar_on_left
					    ? 0
					    : x_width(cur_win))),
				Nval(XtNy, 0),
				Nval(XtNborderWidth, 0),
				Nval(XtNbackground, cur_win->modeline_bg),
				NULL);
#endif /* OPT_KEV_SCROLLBARS */
#endif /* OPT_XAW_SCROLLBARS */
#endif /* MOTIF_WIDGETS */

#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
    curs_sb_v_double_arrow = XCreateFontCursor(dpy, XC_sb_v_double_arrow);
    curs_sb_up_arrow = XCreateFontCursor(dpy, XC_sb_up_arrow);
    curs_sb_down_arrow = XCreateFontCursor(dpy, XC_sb_down_arrow);
    curs_sb_left_arrow = XCreateFontCursor(dpy, XC_sb_left_arrow);
    curs_sb_right_arrow = XCreateFontCursor(dpy, XC_sb_right_arrow);
    curs_double_arrow = XCreateFontCursor(dpy, XC_double_arrow);
#endif

#if OPT_KEV_SCROLLBARS
    cur_win->nscrollbars = 0;
#else
    cur_win->nscrollbars = -1;
#endif

    /*
     * Move scrollbar to the left if requested via the resources.
     * Note that this is handled elsewhere for NO_WIDGETS.
     */
    if (cur_win->scrollbar_on_left) {
#if MOTIF_WIDGETS
	XtVaSetValues(cur_win->pane,
		      Nval(XmNleftAttachment, XmATTACH_FORM),
		      Nval(XmNrightAttachment, XmATTACH_WIDGET),
		      Nval(XmNrightWidget, cur_win->screen),
		      NULL);
	XtVaSetValues(cur_win->screen,
		      Nval(XmNleftAttachment, XmATTACH_NONE),
		      Nval(XmNrightAttachment, XmATTACH_FORM),
		      NULL);
#endif /* !MOTIF_WIDGETS */
    }

    XtAddEventHandler(cur_win->screen,
		      KeyPressMask,
		      FALSE,
		      x_key_press,
		      (XtPointer) 0);

    XtAddEventHandler(cur_win->screen,
		      (EventMask) (ButtonPressMask | ButtonReleaseMask
				   | (cur_win->focus_follows_mouse ? PointerMotionMask
				      : (Button1MotionMask | Button3MotionMask))
				   | ExposureMask | VisibilityChangeMask),
		      TRUE,
		      x_process_event,
		      (XtPointer) 0);

    XtAddEventHandler(cur_win->top_widget,
		      StructureNotifyMask,
		      FALSE,
		      x_configure_window,
		      (XtPointer) 0);

    XtAddEventHandler(cur_win->top_widget,
		      EnterWindowMask | LeaveWindowMask | FocusChangeMask,
		      FALSE,
		      x_change_focus,
		      (XtPointer) 0);

    /* Realize the widget, but don't do the mapping (yet...) */
    XtSetMappedWhenManaged(cur_win->top_widget, False);
    XtRealizeWidget(cur_win->top_widget);

    /* Now that the widget hierarchy is realized, we can fetch
       some crucial dimensions and set the window manager hints
       dealing with resize appropriately. */
    {
	Dimension new_width, new_height;
	XtVaGetValues(cur_win->top_widget,
		      XtNheight, &cur_win->top_height,
		      XtNwidth, &cur_win->top_width,
		      NULL);
#if ATHENA_WIDGETS && OPT_MENUS
	XtVaGetValues(cur_win->menu_widget,
		      XtNheight, &cur_win->menu_height,
		      XtNwidth, &new_width,
		      NULL);
#endif
	XtVaGetValues(cur_win->screen,
		      XtNheight, &new_height,
		      XtNwidth, &new_width,
		      NULL);

	cur_win->base_width = cur_win->top_width - new_width;
	cur_win->base_height = cur_win->top_height - new_height;

	/* Ugly hack:  If the window manager chooses not to respect
	   the min_height hint, it may instead choose to use base_height
	   as min_height instead.  I believe that the reason for this
	   is because O'Reilly's Xlib Programming Manual, Vol 1
	   has lead some window manager implementors astray.  It says:

	   In R4, the base_width and base_height fields have been
	   added to the XSizeHints structure.  They are used with
	   the width_inc and height_inc fields to indicate to the
	   window manager that it should resize the window in
	   steps -- in units of a certain number of pixels
	   instead of single pixels.  The window manager resizes
	   the window to any multiple of width_inc in width and
	   height_inc in height, but no smaller than min_width
	   and min_height and no bigger than max_width and
	   max_height.  If you think about it, min_width and
	   min_height and base_width and base_height have
	   basically the same purpose.  Therefore, base_width and
	   base_height take priority over min_width and
	   min_height, so only one of these pairs should be set.

	   We are indeed lucky that most window managers have chosen to
	   ignore the last two sentences in the above paragraph.  These
	   two pairs of values *do not* serve the same purpose.  I can
	   see where they're coming from...  if you have a minimum
	   height that you want the application to be, you could simply
	   set base_height to be the this minimum height which would be
	   the real_base_height + N*unit_height where N is a
	   non-negative integer.  But what they're forgetting in all
	   this is that the window manager reports the size in units
	   to the user and the size it reports will likely be off by N.

	   Unfortunately, enlightenment 0.16.3 (and probably other
	   versions too) do seem to only use base_height and
	   base_width to determine the smallest window size and this
	   is causing some problems for xvile with no menu bars.
	   Specifically, I've seen BadValue errors from the guts of Xt
	   when cur_win->screen gets resized down to zero.  So we make
	   sure that base_height is non-zero and hope the user doesn't
	   notice that extra pixel of height.  */
	if (cur_win->base_height == 0)
	    cur_win->base_height = 1;

	XtVaSetValues(cur_win->top_widget,
#if XtSpecificationRelease >= 4
		      Nval(XtNbaseHeight, cur_win->base_height),
		      Nval(XtNbaseWidth, cur_win->base_width),
#endif
		      Nval(XtNminHeight, (cur_win->base_height
					  + MINROWS * cur_win->char_height)),
		      Nval(XtNminWidth, (cur_win->base_width
					 + MINCOLS * cur_win->char_width)),
		      Nval(XtNheightInc, cur_win->char_height),
		      Nval(XtNwidthInc, cur_win->char_width),
		      NULL);
    }
    /* According to the docs, this should map the widget too... */
    XtSetMappedWhenManaged(cur_win->top_widget, True);
    /* ... but an explicit map calls seems to be necessary anyway */
    XtMapWidget(cur_win->top_widget);

    cur_win->win = XtWindow(cur_win->screen);

#if OPT_INPUT_METHOD
    init_xlocale();
    if (okCTYPE2(vl_wide_enc)) {
	term.set_enc(enc_UTF8);
    }
#endif

    x_load_icon();

    /* We wish to participate in the "delete window" protocol */
    {
	Atom atoms[2];
	i = 0;
	atoms[i++] = GetAtom(WM_DELETE_WINDOW);
	XSetWMProtocols(dpy,
			XtWindow(cur_win->top_widget),
			atoms,
			i);
    }
    XtAddEventHandler(cur_win->top_widget,
		      NoEventMask,
		      TRUE,
		      x_wm_delwin,
		      (XtPointer) 0);

    set_pointer(XtWindow(cur_win->screen), cur_win->normal_pointer);

    returnCode(status);
}

#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
static Boolean
#define TooLight(color) ((color) > 0xfff0)
#define TooDark(color)  ((color) < 0x0020)
too_light_or_too_dark(Pixel pixel)
{
    XColor color;

    color.pixel = pixel;
    XQueryColor(dpy, cur_win->colormap, &color);

    return (Boolean) ((TooLight(color.red) &&
		       TooLight(color.green) &&
		       TooLight(color.blue)) ||
		      (TooDark(color.red) &&
		       TooDark(color.green) &&
		       TooDark(color.blue)));
}
#endif

#if OPT_KEV_SCROLLBARS
static Boolean
alloc_shadows(Pixel pixel, Pixel *light, Pixel *dark)
{
    XColor color;
    ULONG lred, lgreen, lblue, dred, dgreen, dblue;

    color.pixel = pixel;
    XQueryColor(dpy, cur_win->colormap, &color);

    if ((color.red > 0xfff0 && color.green > 0xfff0 && color.blue > 0xfff0)
	|| (color.red < 0x0020 && color.green < 0x0020 && color.blue < 0x0020))
	return False;		/* It'll look awful! */

#define MAXINTENS ((ULONG)65535L)
#define PlusFortyPercent(v) (ULONG) ((7 * (long) (v)) / 5)
    lred = PlusFortyPercent(color.red);
    lred = min(lred, MAXINTENS);
    lred = max(lred, (color.red + MAXINTENS) / 2);

    lgreen = PlusFortyPercent(color.green);
    lgreen = min(lgreen, MAXINTENS);
    lgreen = max(lgreen, (color.green + MAXINTENS) / 2);

    lblue = PlusFortyPercent(color.blue);
    lblue = min(lblue, MAXINTENS);
    lblue = max(lblue, (color.blue + MAXINTENS) / 2);

#define MinusFortyPercent(v) (ULONG) ((3 * (long) (v)) / 5)

    dred = MinusFortyPercent(color.red);
    dgreen = MinusFortyPercent(color.green);
    dblue = MinusFortyPercent(color.blue);

    color.red = (USHORT) lred;
    color.green = (USHORT) lgreen;
    color.blue = (USHORT) lblue;

    if (!XAllocColor(dpy, cur_win->colormap, &color))
	return False;

    *light = color.pixel;

    color.red = (USHORT) dred;
    color.green = (USHORT) dgreen;
    color.blue = (USHORT) dblue;

    if (!XAllocColor(dpy, cur_win->colormap, &color))
	return False;

    *dark = color.pixel;

    return True;
}
#endif

#if OPT_MULTIBYTE
void
x_set_font_encoding(ENC_CHOICES new_encoding)
{
    ENC_CHOICES old_encoding = font_encoding;

    if (old_encoding != new_encoding) {
	TRACE(("set $font-encoding to %s\n", encoding2s(new_encoding)));
	font_encoding = new_encoding;
	set_winflags(TRUE, WFHARD);
    }
}
#endif

void
x_set_fontname(TextWindow tw, const char *fname)
{
    char *newfont;

    if (fname != NULL
	&& (newfont = strmalloc(fname)) != NULL) {
	FreeIfNeeded(tw->fontname);
	tw->fontname = newfont;
    }
}

char *
x_current_fontname(void)
{
    return cur_win->fontname;
}

#if OPT_MENUS
Widget
x_menu_widget(void)
{
    return cur_win->menu_widget;
}

int
x_menu_height(void)
{
    return cur_win->menu_height;
}
#endif /* OPT_MENUS */

#if OPT_MENUS_COLORED
int
x_menu_foreground(void)
{
    return (int) cur_win->menubar_fg;
}

int
x_menu_background(void)
{
    return (int) cur_win->menubar_bg;
}
#endif /* OPT_MENUS_COLORED */

int
x_setfont(const char *fname)
{
    XVileFont *pfont;
    Dimension oldw;
    Dimension oldh;
    int code = 1;

    TRACE((T_CALLED "x_setfont(%s)\n", fname));
    if (cur_win) {
	XVileFonts oldf = cur_win->fonts;
	oldw = (Dimension) x_width(cur_win);
	oldh = (Dimension) x_height(cur_win);
	code = 0;
	if ((pfont = xvileQueryFont(dpy, cur_win, fname)) != NULL) {
#ifndef XRENDERFONT
	    XSetFont(dpy, GetColorGC(cur_win, tt_info), pfont->fid);
	    XSetFont(dpy, GetColorGC(cur_win, rt_info), pfont->fid);
	    XSetFont(dpy, GetColorGC(cur_win, ss_info), pfont->fid);
	    XSetFont(dpy, GetColorGC(cur_win, rs_info), pfont->fid);
	    XSetFont(dpy, GetColorGC(cur_win, cc_info), pfont->fid);
	    XSetFont(dpy, GetColorGC(cur_win, rc_info), pfont->fid);
	    XSetFont(dpy, GetColorGC(cur_win, mm_info), pfont->fid);
	    XSetFont(dpy, GetColorGC(cur_win, rm_info), pfont->fid);
	    XSetFont(dpy, GetColorGC(cur_win, oo_info), pfont->fid);
	    XSetFont(dpy, GetColorGC(cur_win, ro_info), pfont->fid);
	    if (GetColorGC(cur_win, tt_info) != GetColorGC(cur_win, rs_info)) {
		XSetFont(dpy, GetColorGC(cur_win, ss_info), pfont->fid);
		XSetFont(dpy, GetColorGC(cur_win, rs_info), pfont->fid);
	    }
#endif
	    reset_color_gcs();
	    xvileCloseFonts(dpy, &oldf);

	    /* if size changed, resize it, otherwise refresh */
	    if (oldw != x_width(cur_win) || oldh != x_height(cur_win)) {
		XtVaSetValues(cur_win->top_widget,
			      Nval(XtNminHeight, (cur_win->base_height
						  + MINROWS * cur_win->char_height)),
			      Nval(XtNminWidth, (cur_win->base_width
						 + MINCOLS * cur_win->char_width)),
			      Nval(XtNheightInc, cur_win->char_height),
			      Nval(XtNwidthInc, cur_win->char_width),
			      NULL);
		update_scrollbar_sizes();
		XClearWindow(dpy, cur_win->win);
		x_touch(cur_win, 0, 0, cur_win->cols, cur_win->rows);
		XResizeWindow(dpy, XtWindow(cur_win->top_widget),
			      x_width(cur_win) + (unsigned) cur_win->base_width,
			      x_height(cur_win) + (unsigned) cur_win->base_height);

	    } else {
		XClearWindow(dpy, cur_win->win);
		x_touch(cur_win, 0, 0, cur_win->cols, cur_win->rows);
		x_flush();
	    }

	    code = 1;
	}
    }
    returnCode(code);
}

static void
x_set_encoding(ENC_CHOICES code)
{
    term_encoding = code;
    TRACE(("x11:set_encoding %s\n", encoding2s(code)));
}

static ENC_CHOICES
x_get_encoding(void)
{
    return term_encoding;
}

static void
x_open(void)
{
    static int already_open;

#if OPT_COLOR
    static const char *initpalettestr = "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15";
#endif

    TRACE((T_CALLED "x_open\n"));

    if (!already_open) {
#if OPT_COLOR
	set_colors(NCOLORS);
	set_ctrans(initpalettestr);	/* should be set_palette() */
#endif
	kqinit(cur_win);
	cur_win->scrollbars = NULL;
	cur_win->maxscrollbars = 0;
#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
	cur_win->scrollinfo = NULL;
	cur_win->grips = NULL;
#endif

	/* main code assumes that it can access a cell at nrow x ncol */
	term.maxcols = term.cols = (int) cur_win->cols;
	term.maxrows = term.rows = (int) cur_win->rows;

	if (check_scrollbar_allocs() != TRUE) {
	    fprintf(stderr, "Cannot allocate scrollbars\n");
	    ExitProgram(BADEXIT);
	}
	already_open = TRUE;
    }
    returnVoid();
}

#if OPT_INPUT_METHOD
static void
CloseInputMethod(void)
{
    if (cur_win->xim) {
	XCloseIM(cur_win->xim);
	cur_win->xim = NULL;
	TRACE(("freed cur_win->xim\n"));
    }
}
#else
#define CloseInputMethod()	/* nothing */
#endif

static void
x_close(void)
{
    /* FIXME: Free pixmaps and GCs !!! */

    if (cur_win->top_widget) {
#if NO_LEAKS
	XtDestroyWidget(cur_win->top_widget);
#endif
	cur_win->top_widget = NULL;
	CloseInputMethod();
	XtCloseDisplay(dpy);	/* need this if $xshell left subprocesses */
    }
}

static void
x_touch(TextWindow tw, int sc, int sr, UINT ec, UINT er)
{
    UINT r;
    UINT c;

    if (er > tw->rows)
	er = tw->rows;
    if (ec > tw->cols)
	ec = tw->cols;

    for (r = (UINT) sr; r < er; r++) {
	MARK_LINE_DIRTY(r);
	for (c = (UINT) sc; c < ec; c++) {
	    if (CELL_TEXT(r, c) != ' ' || CELL_ATTR(r, c)) {
		MARK_CELL_DIRTY(r, c);
	    }
	}
    }
}

static void
wait_for_scroll(TextWindow tw)
{
    XEvent ev;
    int sc, sr;
    UINT ec, er;
    XGraphicsExposeEvent *gev;

    for_ever {			/* loop looking for a gfx expose or no expose */
	if (XCheckTypedEvent(dpy, NoExpose, &ev))
	    return;
	if (XCheckTypedEvent(dpy, GraphicsExpose, &ev)) {
	    gev = (XGraphicsExposeEvent *) & ev;
	    sc = gev->x / tw->char_width;
	    sr = gev->y / tw->char_height;
	    ec = (UINT) CEIL(gev->x + gev->width, tw->char_width);
	    er = (UINT) CEIL(gev->y + gev->height, tw->char_height);
	    x_touch(tw, sc, sr, ec, er);
	    if (gev->count == 0)
		return;
	}
	XSync(dpy, False);
    }
}

static int
x_set_palette(const char *thePalette)
{
    int rc;

    TRACE(("x_set_palette(%s)\n", thePalette));
    rc = set_ctrans(thePalette);
    x_touch(cur_win, 0, 0, cur_win->cols, cur_win->rows);
    x_flush();

    return rc;
}

static void
x_scroll(int from, int to, int count)
{
    if (cur_win->visibility == VisibilityFullyObscured)
	return;			/* Why bother? */

    if (from == to)
	return;			/* shouldn't happen */

    XCopyArea(dpy,
	      cur_win->win,
	      cur_win->win,
	      GetColorGC(cur_win, tt_info),
	      x_pos(cur_win, 0), y_pos(cur_win, from),
	      x_width(cur_win), (UINT) (count * cur_win->char_height),
	      x_pos(cur_win, 0), y_pos(cur_win, to));
    if (from < to)
	XClearArea(dpy, cur_win->win,
		   x_pos(cur_win, 0), y_pos(cur_win, from),
		   x_width(cur_win), (UINT) ((to - from) * cur_win->char_height),
		   FALSE);
    else
	XClearArea(dpy, cur_win->win,
		   x_pos(cur_win, 0), y_pos(cur_win, to + count),
		   x_width(cur_win), (UINT) ((from - to) * cur_win->char_height),
		   FALSE);
    if (cur_win->visibility == VisibilityPartiallyObscured) {
	XFlush(dpy);
	wait_for_scroll(cur_win);
    }
}

/* See above comment regarding CLEAR_THRESH */
#define NONDIRTY_THRESH 16

/* make sure the screen looks like we want it to */
static void
x_flush(void)
{
    int r, c, sc, ec, cleanlen;
    VIDEO_ATTR attr;

    if (cur_win->visibility == VisibilityFullyObscured || !cur_win->exposed)
	return;			/* Why bother? */

    /*
     * Write out cursor _before_ rest of the screen in order to avoid
     * flickering / winking effect noticeable on some display servers.  This
     * means that the old cursor position (if different from the current
     * one) will be cleared after the new cursor is displayed.
     */

    if (ttrow >= 0 && ttrow < term.rows && ttcol >= 0 && ttcol < term.cols
	&& !cur_win->wipe_permitted) {
	CLEAR_CELL_DIRTY(ttrow, ttcol);
	display_cursor((XtPointer) 0, (XtIntervalId *) 0);
    }

    /* sometimes we're the last to know about resizing... */
    if ((int) cur_win->rows > term.maxrows)
	cur_win->rows = (UINT) term.maxrows;

    for (r = 0; r < (int) cur_win->rows; r++) {
	if (!IS_DIRTY_LINE(r))
	    continue;
	if (r != ttrow)
	    CLEAR_LINE_DIRTY(r);

	/*
	 * The following code will cause monospaced fonts with ink outside
	 * the bounding box to be cleaned up.
	 */
	if (cur_win->left_ink || cur_win->right_ink)
	    for (c = 0; c < term.cols;) {
		while (c < term.cols && !IS_DIRTY(r, c))
		    c++;
		if (c >= term.cols)
		    break;
		if (cur_win->left_ink && c > 0)
		    MARK_CELL_DIRTY(r, c - 1);
		while (c < term.cols && IS_DIRTY(r, c))
		    c++;
		if (cur_win->right_ink && c < term.cols) {
		    MARK_CELL_DIRTY(r, c);
		    c++;
		}
	    }

	c = 0;
	while (c < term.cols) {
	    /* Find the beginning of the next dirty sequence */
	    while (c < term.cols && !IS_DIRTY(r, c))
		c++;
	    if (c >= term.cols)
		break;
	    if (r == ttrow && c == ttcol && !cur_win->wipe_permitted) {
		c++;
		continue;
	    }
	    CLEAR_CELL_DIRTY(r, c);
	    sc = ec = c;
	    attr = VATTRIB(CELL_ATTR(r, c));
	    cleanlen = NONDIRTY_THRESH;
	    c++;
	    /*
	     * Scan until we find the end of line, a cell with a different
	     * attribute, a sequence of NONDIRTY_THRESH non-dirty chars, or
	     * the cursor position.
	     */
	    while (c < term.cols) {
		if (attr != VATTRIB(CELL_ATTR(r, c)))
		    break;
		else if (r == ttrow && c == ttcol && !cur_win->wipe_permitted) {
		    c++;
		    break;
		} else if (IS_DIRTY(r, c)) {
		    ec = c;
		    cleanlen = NONDIRTY_THRESH;
		    CLEAR_CELL_DIRTY(r, c);
		} else if (--cleanlen <= 0)
		    break;
		c++;
	    }
	    /* write out the portion from sc thru ec */
	    xvileDraw(dpy, cur_win, &CELL_TEXT(r, sc), ec - sc + 1,
		      (UINT) VATTRIB(CELL_ATTR(r, sc)), r, sc);
	}
    }
    XFlush(dpy);
}

/* selection processing stuff */

/* multi-click code stolen from xterm */
/*
 * double click table for cut and paste in 8 bits
 *
 * This table is divided in four parts :
 *
 *	- control characters	[0,0x1f] U [0x80,0x9f]
 *	- separators		[0x20,0x3f] U [0xa0,0xb9]
 *	- binding characters	[0x40,0x7f] U [0xc0,0xff]
 *	- exceptions
 */
static int charClass[256] =
{
/* NUL  SOH  STX  ETX  EOT  ENQ  ACK  BEL */
    32, 1, 1, 1, 1, 1, 1, 1,
/*  BS   HT   NL   VT   NP   CR   SO   SI */
    1, 32, 1, 1, 1, 1, 1, 1,
/* DLE  DC1  DC2  DC3  DC4  NAK  SYN  ETB */
    1, 1, 1, 1, 1, 1, 1, 1,
/* CAN   EM  SUB  ESC   FS   GS   RS   US */
    1, 1, 1, 1, 1, 1, 1, 1,
/*  SP    !    "    #    $    %    &    ' */
    32, 33, 34, 35, 36, 37, 38, 39,
/*   (    )    *    +    ,    -    .    / */
    40, 41, 42, 43, 44, 45, 46, 47,
/*   0    1    2    3    4    5    6    7 */
    48, 48, 48, 48, 48, 48, 48, 48,
/*   8    9    :    ;    <    =    >    ? */
    48, 48, 58, 59, 60, 61, 62, 63,
/*   @    A    B    C    D    E    F    G */
    64, 48, 48, 48, 48, 48, 48, 48,
/*   H    I    J    K    L    M    N    O */
    48, 48, 48, 48, 48, 48, 48, 48,
/*   P    Q    R    S    T    U    V    W */
    48, 48, 48, 48, 48, 48, 48, 48,
/*   X    Y    Z    [    \    ]    ^    _ */
    48, 48, 48, 91, 92, 93, 94, 48,
/*   `    a    b    c    d    e    f    g */
    96, 48, 48, 48, 48, 48, 48, 48,
/*   h    i    j    k    l    m    n    o */
    48, 48, 48, 48, 48, 48, 48, 48,
/*   p    q    r    s    t    u    v    w */
    48, 48, 48, 48, 48, 48, 48, 48,
/*   x    y    z    {    |    }    ~  DEL */
    48, 48, 48, 123, 124, 125, 126, 1,
/* x80  x81  x82  x83  IND  NEL  SSA  ESA */
    1, 1, 1, 1, 1, 1, 1, 1,
/* HTS  HTJ  VTS  PLD  PLU   RI  SS2  SS3 */
    1, 1, 1, 1, 1, 1, 1, 1,
/* DCS  PU1  PU2  STS  CCH   MW  SPA  EPA */
    1, 1, 1, 1, 1, 1, 1, 1,
/* x98  x99  x9A  CSI   ST  OSC   PM  APC */
    1, 1, 1, 1, 1, 1, 1, 1,
/*   -    i   c/    L   ox   Y-    |   So */
    160, 161, 162, 163, 164, 165, 166, 167,
/*  ..   c0   ip   <<    _        R0    - */
    168, 169, 170, 171, 172, 173, 174, 175,
/*   o   +-    2    3    '    u   q|    . */
    176, 177, 178, 179, 180, 181, 182, 183,
/*   ,    1    2   >>  1/4  1/2  3/4    ? */
    184, 185, 186, 187, 188, 189, 190, 191,
/*  A`   A'   A^   A~   A:   Ao   AE   C, */
    48, 48, 48, 48, 48, 48, 48, 48,
/*  E`   E'   E^   E:   I`   I'   I^   I: */
    48, 48, 48, 48, 48, 48, 48, 48,
/*  D-   N~   O`   O'   O^   O~   O:    X */
    48, 48, 48, 48, 48, 48, 48, 216,
/*  O/   U`   U'   U^   U:   Y'    P    B */
    48, 48, 48, 48, 48, 48, 48, 48,
/*  a`   a'   a^   a~   a:   ao   ae   c, */
    48, 48, 48, 48, 48, 48, 48, 48,
/*  e`   e'   e^   e:    i`  i'   i^   i: */
    48, 48, 48, 48, 48, 48, 48, 48,
/*   d   n~   o`   o'   o^   o~   o:   -: */
    48, 48, 48, 48, 48, 48, 48, 248,
/*  o/   u`   u'   u^   u:   y'    P   y: */
    48, 48, 48, 48, 48, 48, 48, 48};

/* low, high are in the range 0..255 */
static int
set_character_class_range(int low, int high, int value)
{

    if (low < 0 || high > 255 || high < low)
	return (-1);

    for (; low <= high; low++)
	charClass[low] = value;

    return (0);
}

/*
 * set_character_class - takes a string of the form
 *
 *                 low[-high]:val[,low[-high]:val[...]]
 *
 * and sets the indicated ranges to the indicated values.
 */

static int
set_character_class(const char *s)
{
    int i;			/* iterator, index into s */
    int len;			/* length of s */
    int acc;			/* accumulator */
    int low, high;		/* bounds of range [0..127] */
    int base;			/* 8, 10, 16 (octal, decimal, hex) */
    int numbers;		/* count of numbers per range */
    int digits;			/* count of digits in a number */
    static const char *errfmt = "xvile:  %s in range string \"%s\" (position %d)\n";

    if (!s || !s[0])
	return -1;

    base = 10;			/* in case we ever add octal, hex */
    low = high = -1;		/* out of range */

    for (i = 0, len = (int) strlen(s), acc = 0, numbers = digits = 0;
	 i < len; i++) {
	int c = s[i];

	if (isSpace(c)) {
	    continue;
	} else if (isDigit(c)) {
	    acc = acc * base + (c - '0');
	    digits++;
	    continue;
	} else if (c == '-') {
	    low = acc;
	    acc = 0;
	    if (digits == 0) {
		(void) fprintf(stderr, errfmt, "missing number", s, i);
		return (-1);
	    }
	    digits = 0;
	    numbers++;
	    continue;
	} else if (c == ':') {
	    if (numbers == 0)
		low = acc;
	    else if (numbers == 1)
		high = acc;
	    else {
		(void) fprintf(stderr, errfmt, "too many numbers",
			       s, i);
		return (-1);
	    }
	    digits = 0;
	    numbers++;
	    acc = 0;
	    continue;
	} else if (c == ',') {
	    /*
	     * now, process it
	     */

	    if (high < 0) {
		high = low;
		numbers++;
	    }
	    if (numbers != 2) {
		(void) fprintf(stderr, errfmt, "bad value number",
			       s, i);
	    } else if (set_character_class_range(low, high, acc) != 0) {
		(void) fprintf(stderr, errfmt, "bad range", s, i);
	    }
	    low = high = -1;
	    acc = 0;
	    digits = 0;
	    numbers = 0;
	    continue;
	} else {
	    (void) fprintf(stderr, errfmt, "bad character", s, i);
	    return (-1);
	}			/* end if else if ... else */

    }

    if (low < 0 && high < 0)
	return (0);

    /*
     * now, process it
     */

    if (high < 0)
	high = low;
    if (numbers < 1 || numbers > 2) {
	(void) fprintf(stderr, errfmt, "bad value number", s, i);
    } else if (set_character_class_range(low, high, acc) != 0) {
	(void) fprintf(stderr, errfmt, "bad range", s, i);
    }
    return (0);
}

#if OPT_MULTIBYTE
static int
pasting_utf8(TBUFF *p, int c)
{
    int result = FALSE;
    if (b_is_utfXX(curbp)) {
	char temp[2];
	int n, limit, len;

	temp[0] = (char) c;
	if (vl_conv_to_utf32((UINT *) 0, temp, (B_COUNT) 6) > 0) {
	    result = TRUE;
	} else if ((limit = (int) tb_length(p)) > 0) {
	    for (n = limit - 1; n > 0; --n) {
		len = vl_conv_to_utf32((UINT *) 0,
				       tb_values(p) + n,
				       (B_COUNT) 6);
		if (len > 0) {
		    result = TRUE;
		    break;
		}
	    }
	}
    }
    return result;
}
#define PastingUTF8(p,c) pasting_utf8(p,c)
#else
#define PastingUTF8(p,c) FALSE
#endif

/*
 * Copy a single character into the paste-buffer, quoting it if necessary
 */
static int
add2paste(TBUFF **p, int c)
{
    int result;

    TRACE2(("add2paste '%04o'\n", c));
    if (c == '\n' || isBlank(c)) {
	/*EMPTY */ ;
    } else if (isSpecial(c) ||
	       isreturn(c) ||
	       !(isPrint(c) || PastingUTF8(*p, c))) {
	(void) tb_append(p, quotec);
    }
    result = (tb_append(p, c) != NULL);
    TRACE2(("...added %d:%s\n", tb_length(*p), tb_visible(*p)));
    return result;
}

/*
 * Copy the selection into the PasteBuf buffer.  If we are pasting into a
 * window, check to see if:
 *
 *	+ the window's buffer is modifiable (if not, don't waste time copying
 *	  text!)
 *	+ the buffer uses 'autoindent' mode (if so, do some heuristics
 *	  for placement of the pasted text -- we may put it on lines by
 *	  itself, above or below the current line)
 */
#define OLD_PASTE 0

static int
copy_paste(TBUFF **p, char *value, size_t length)
{
    WINDOW *wp = row2window(ttrow);
    BUFFER *bp = valid_window(wp) ? wp->w_bufp : NULL;
    int status;

    if (valid_buffer(bp) && b_val(bp, MDVIEW))
	return FALSE;

    status = TRUE;

    if (valid_buffer(bp) && (b_val(bp, MDCINDENT) || b_val(bp, MDAIND))) {

#if OLD_PASTE
	/*
	 * If the cursor points before the first nonwhite on
	 * the line, convert the insert into an 'O' command.
	 * If it points to the end of the line, convert it into
	 * an 'o' command.  Otherwise (if it is within the
	 * nonwhite portion of the line), assume the user knows
	 * what (s)he is doing.
	 */
#endif
	if (setwmark(ttrow, ttcol)) {	/* MK gets cursor */
#if OLD_PASTE
	    LINE *lp = MK.l;
	    int first = firstchar(lp);
	    int last = lastchar(lp);
	    CMDFUNC *f = 0;

	    /* If the line contains only a single nonwhite,
	     * we will insert before it.
	     */
	    if (first >= MK.o)
		f = &f_openup_no_aindent;
	    else if (last <= MK.o)
		f = &f_opendown_no_aindent;
	    if (insertmode) {
		if ((*value != '\n') && MK.o == 0)
		    (void) tb_append(p, '\n');
	    } else if (f) {
		char *pstr;
		/* we're _replacing_ the default
		   insertion command, so reinit */
		tb_init(p, esc_c);
		pstr = fnc2pstr(f);
		tb_bappend(p, pstr + 1, (size_t) *pstr);
	    }
#endif
	}
    }

    while (length-- > 0) {
	if (!add2paste(p, CharOf(*value++))) {
	    status = FALSE;
	    break;
	}
    }

    return status;
}

static Atom *
GetSelectionTargets(void)
{
    static Atom result[10];
    if (result[0] == 0) {
	Atom *tp = result;
#if OPT_MULTIBYTE
	*tp++ = GetAtom(UTF8_STRING);
	*tp++ = GetAtom(COMPOUND_TEXT);
#endif
	*tp++ = GetAtom(TEXT);
	*tp++ = XA_STRING;
	*tp = None;
#if OPT_TRACE
	TRACE(("SelectionTargets:\n"));
	for (tp = result; *tp != None; ++tp) {
	    TRACE((">%s\n", visibleAtoms(*tp)));
	}
#endif
    }
    return result;
}

static void
insert_selection(Atom *selection, char *value, size_t length)
{
    int do_ins;
    char *s = NULL;		/* stifle warning */

    /* should be impossible to hit this with existing paste */
    /* XXX massive hack -- leave out 'i' if in prompt line */
    do_ins = !insertmode
	&& (!onMsgRow(cur_win) || *selection == GetAtom(CLIPBOARD))
	&& ((s = fnc2pstr(&f_insert_no_aindent)) != NULL);

    if (tb_init(&PasteBuf, esc_c)) {
	if ((do_ins && !tb_bappend(&PasteBuf, s + 1, (size_t) CharOf(*s)))
	    || !copy_paste(&PasteBuf, value, length)
	    || (do_ins && !tb_append(&PasteBuf, esc_c)))
	    tb_free(&PasteBuf);
    }
}

/* ARGSUSED */
static void
x_get_selection(Widget w GCC_UNUSED,
		XtPointer cldat,
		Atom *selection,
		Atom *target,
		XtPointer value,
		ULONG * length,
		int *format)
{
    SelectionList *list = (SelectionList *) cldat;

    TRACE(("x_get_selection(selection %s, target %s, format %d, length %lu)\n",
	   visibleAtoms(*selection),
	   visibleAtoms(*target),
	   *format,
	   length ? *length : 0));

    if (length != NULL && value != NULL) {
	if (*format != 8) {
	    kbd_alarm();	/* can't handle incoming data */
	} else if (*target == XA_STRING || *target == GetAtom(TEXT)) {
	    insert_selection(selection, (char *) value, (size_t) *length);
	    XtFree((char *) value);
	} else
#if OPT_MULTIBYTE
	    if ((*target == GetAtom(COMPOUND_TEXT))
		|| (*target == GetAtom(UTF8_STRING))) {
	    XTextProperty text_prop;
	    char **text_list = NULL;
	    int text_list_count, n;

	    text_prop.value = (unsigned char *) value;
	    text_prop.encoding = *target;
	    text_prop.format = *format;
	    text_prop.nitems = *length;

#ifdef HAVE_XUTF8TEXTPROPERTYTOTEXTLIST
	    if (Xutf8TextPropertyToTextList(dpy, &text_prop,
					    &text_list,
					    &text_list_count) < 0) {
		TRACE(("Conversion failed\n"));
		text_list = NULL;
	    }
#elif defined(HAVE_XMBTEXTPROPERTYTOTEXTLIST)
	    if (XmbTextPropertyToTextList(dpy, &text_prop,
					  &text_list,
					  &text_list_count) < 0) {
		TRACE(("Conversion failed\n"));
		text_list = NULL;
	    }
#endif
	    if (text_list != NULL) {
		for (n = 0; n < text_list_count; ++n) {
		    TRACE(("Inserting:%s\n", text_list[n]));
		    insert_selection(selection,
				     text_list[n],
				     strlen(text_list[n]));
		}
		XFreeStringList(text_list);
		XtFree((char *) value);
	    } else {
		kbd_alarm();	/* can't handle incoming data */
	    }
	} else
#endif
	{
	    kbd_alarm();	/* can't handle incoming data */
	}
	XtFree((char *) list);
    } else if (cldat != NULL) {
	if (list->targets[0] != None) {
	    Atom newTarget = list->targets[0];

	    list->targets++;
	    XtGetSelectionValue(cur_win->top_widget,
				*selection,
				newTarget,
				x_get_selection,
				(XtPointer) (list),	/* client data */
				list->time);
	} else {
	    XtFree((char *) list);
	}
    }
}

static void
x_paste_selection(Atom selection)
{
    if (cur_win->have_selection && IsPrimary(selection)) {
	/* local transfer */
	UCHAR *data = NULL;
	size_t len_st = 0;
	ULONG len_ul;

	Atom target = XA_STRING;
	int format = 8;

	if (!x_get_selected_text(&data, &len_st)) {
	    kbd_alarm();
	    return;
	}
	len_ul = (ULONG) len_st;	/* Ugh. */
	x_get_selection(cur_win->top_widget, NULL, &selection, &target,
			(XtPointer) data, &len_ul, &format);
    } else {
	Atom *targets = GetSelectionTargets();
	SelectionList *list = (void *) XtNew(SelectionList);
	Time ev_time = XtLastTimestampProcessed(dpy);

	list->targets = targets + 1;
	list->time = ev_time;
	XtGetSelectionValue(cur_win->top_widget,
			    selection,
			    targets[0],
			    x_get_selection,
			    (XtPointer) (list),		/* client data */
			    ev_time);
    }
}

static Boolean
x_get_selected_text(UCHAR ** datp, size_t *lenp)
{
    UCHAR *data = NULL;
    UCHAR *dp = NULL;
    size_t length;
    KILL *kp;			/* pointer into kill register */

    /* FIXME: Can't select message line */

    if (!cur_win->have_selection)
	return False;

    sel_yank(SEL_KREG);
    for (length = 0, kp = kbs[SEL_KREG].kbufh; kp; kp = kp->d_next)
	length += KbSize(SEL_KREG, kp);
    if (length == 0
	|| (dp = data = (UCHAR *) XtMalloc((Cardinal) (length
						       * sizeof(UCHAR)))) == NULL
	|| (kp = kbs[SEL_KREG].kbufh) == NULL)
	return False;

    while (kp != NULL) {
	size_t len = KbSize(SEL_KREG, kp);
	(void) memcpy((char *) dp, (char *) kp->d_chunk, len);
	kp = kp->d_next;
	dp += len;
    }

    *lenp = length;
    *datp = data;
    return True;
}

static Boolean
x_get_clipboard_text(UCHAR ** datp, size_t *lenp)
{
    UCHAR *data = NULL;
    UCHAR *dp = NULL;
    size_t length;
    KILL *kp;			/* pointer into kill register */

    for (length = 0, kp = kbs[CLIP_KREG].kbufh; kp; kp = kp->d_next)
	length += KbSize(CLIP_KREG, kp);
    if (length == 0
	|| (dp = data = (UCHAR *) XtMalloc((Cardinal) (length
						       * sizeof(UCHAR)))) == NULL
	|| (kp = kbs[CLIP_KREG].kbufh) == NULL)
	return False;

    while (kp != NULL) {
	size_t len = KbSize(CLIP_KREG, kp);
	(void) memcpy((char *) dp, (char *) kp->d_chunk, len);
	kp = kp->d_next;
	dp += len;
    }

    *lenp = length;
    *datp = data;
    return True;
}

/* ARGSUSED */
static Boolean
x_convert_selection(Widget w GCC_UNUSED,
		    Atom *selection,
		    Atom *target,
		    Atom *type,
		    XtPointer *value,
		    ULONG * length,
		    int *format)
{
    Boolean result = False;

    TRACE((T_CALLED
	   "x_convert_selection(selection %s, target %s)\n",
	   visibleAtoms(*selection),
	   visibleAtoms(*target)));

    if (!cur_win->have_selection && IsPrimary(*selection))
	returnBoolean(False);

    /*
     * The ICCCM requires us to handle the following targets:  TARGETS,
     * MULTIPLE, and TIMESTAMP.  MULTIPLE and TIMESTAMP are handled by the Xt
     * intrinsics.  Below, we handle TARGETS, STRING, and TEXT.
     *
     * The STRING and TEXT targets are what xvile uses to transfer selected
     * text to another client.
     *
     * TARGETS is simply a list of the targets we support (including the ones
     * handled by the Xt intrinsics).
     */

    if (*target == GetAtom(TARGETS)) {
	Atom *tp;
	Atom *sp;

#define NTARGS 10

	tp = (void *) XtMalloc((Cardinal) (NTARGS * sizeof(Atom)));
	*(Atom **) value = tp;

	if (tp != NULL) {
	    *tp++ = GetAtom(TARGETS);
	    *tp++ = GetAtom(MULTIPLE);
	    *tp++ = GetAtom(TIMESTAMP);
	    for (sp = GetSelectionTargets(); *sp != None; ++sp)
		*tp++ = *sp;

	    *type = XA_ATOM;
	    *length = (ULONG) (tp - *(Atom **) value);
	    *format = 32;	/* width of the data being transferred */
	    result = True;
	}
    } else if (*target == XA_STRING || *target == GetAtom(TEXT)) {
	*type = XA_STRING;
	*format = 8;
	if (IsPrimary(*selection))
	    result = x_get_selected_text((UCHAR **) value, (size_t *) length);
	else			/* CLIPBOARD */
	    result = x_get_clipboard_text((UCHAR **) value, (size_t *) length);
    }
#if OPT_MULTIBYTE
    else if (*target == GetAtom(UTF8_STRING)) {
	*type = *target;
	*format = 8;
	if (IsPrimary(*selection))
	    result = x_get_selected_text((UCHAR **) value, (size_t *) length);
	else			/* CLIPBOARD */
	    result = x_get_clipboard_text((UCHAR **) value, (size_t *) length);
    } else if (*target == GetAtom(COMPOUND_TEXT)) {
	*type = *target;
	*format = 8;
	if (IsPrimary(*selection))
	    result = x_get_selected_text((UCHAR **) value, (size_t *) length);
	else			/* CLIPBOARD */
	    result = x_get_clipboard_text((UCHAR **) value, (size_t *) length);
    }
#endif /* OPT_MULTIBYTE */

    returnBoolean(result);
}

/* ARGSUSED */
static void
x_lose_selection(Widget w GCC_UNUSED,
		 Atom *selection)
{
    if (IsPrimary(*selection)) {
	cur_win->have_selection = False;
	cur_win->was_on_msgline = False;
	sel_release();
	(void) update(TRUE);
    } else {
	/* Free up the data in the kill buffer (how do we do this?) */
    }
}

void
own_selection(void)
{
    x_own_selection(XA_PRIMARY);
}

static void
x_own_selection(Atom selection)
{
    /*
     * Note:  we've been told that the Hummingbird X Server (which runs on a
     * PC) updates the contents of the clipboard only if we remove the next
     * line, causing this program to assert the selection on each call.  We
     * don't do that, however, since it would violate the sense of the ICCCM,
     * which is minimizing network traffic.
     *
     * Kev's note on the above comment (which I assume was written by Tom):
     * I've added some new code for dealing with clipboards in now.  It
     * may well be that the clipboard will work properly now.  Of course,
     * you'll need to run the copy-to-clipboard command from vile.  If
     * you're on a Sun keyboard, you might want to bind this to the Copy
     * key (F16).  I may also think about doing a sort of timer mechanism
     * which asserts ownership of the clipboard if a certain amount of
     * time has gone by with no activity.
     */
    if (!cur_win->have_selection || !IsPrimary(selection)) {
	TRACE(("x_own_selection\n"));

	cur_win->have_selection =
	    XtOwnSelection(cur_win->top_widget,
			   selection,
			   XtLastTimestampProcessed(dpy),
			   x_convert_selection,
			   x_lose_selection,
			   (XtSelectionDoneProc) 0);
    }
}

static void
scroll_selection(XtPointer rowcol,
		 XtIntervalId * idp)
{
    int row, col;
    if (*idp == cur_win->sel_scroll_id)
	XtRemoveTimeOut(cur_win->sel_scroll_id);	/* shouldn't happen */
    cur_win->sel_scroll_id = (XtIntervalId) 0;

    row = (((long) rowcol) >> 16) & 0xffff;
    col = ((long) rowcol) & 0xffff;
#define sign_extend16(n) \
    	if ((n) & 0x8000) \
		n = n | (int)((unsigned)(~0) << 16)
    sign_extend16(row);
    sign_extend16(col);
    extend_selection(cur_win, row, col, TRUE);
}

static int
line_count_and_interval(long scroll_count, ULONG * ip)
{
    scroll_count = scroll_count / 4 - 2;
    if (scroll_count <= 0) {
	*ip = (ULONG) ((1 - scroll_count) * cur_win->scroll_repeat_interval);
	return 1;
    } else {
	/*
	 * FIXME: figure out a cleaner way to do this or something like it...
	 */
	if (scroll_count > 450)
	    scroll_count *= 1024;
	else if (scroll_count > 350)
	    scroll_count *= 128;
	else if (scroll_count > 275)
	    scroll_count *= 64;
	else if (scroll_count > 200)
	    scroll_count *= 16;
	else if (scroll_count > 150)
	    scroll_count *= 8;
	else if (scroll_count > 100)
	    scroll_count *= 4;
	else if (scroll_count > 75)
	    scroll_count *= 3;
	else if (scroll_count > 50)
	    scroll_count *= 2;
	*ip = (ULONG) cur_win->scroll_repeat_interval;
	return (int) scroll_count;
    }
}

static void
extend_selection(TextWindow tw GCC_UNUSED,
		 int nr,
		 int nc,
		 Bool wipe)
{
    static long scroll_count = 0;
    long rowcol = 0;
    ULONG interval = 0;

    if (cur_win->sel_scroll_id != (XtIntervalId) 0) {
	if (nr < curwp->w_toprow || nr >= mode_row(curwp))
	    return;		/* Only let timer extend selection */
	XtRemoveTimeOut(cur_win->sel_scroll_id);
	cur_win->sel_scroll_id = (XtIntervalId) 0;
    }

    if (nr < curwp->w_toprow) {
	if (wipe) {
	    mvupwind(TRUE, line_count_and_interval(scroll_count++, &interval));
	    rowcol = (nr << 16) | (nc & 0xffff);
	} else {
	    scroll_count = 0;
	}
	nr = curwp->w_toprow;
    } else if (nr >= mode_row(curwp)) {
	if (wipe) {
	    mvdnwind(TRUE, line_count_and_interval(scroll_count++, &interval));
	    rowcol = (nr << 16) | (nc & 0xffff);
	} else {
	    scroll_count = 0;
	}
	nr = mode_row(curwp) - 1;
    } else {
	scroll_count = 0;
    }
    if (setcursor(nr, nc) && sel_extend(wipe, TRUE)) {
	cur_win->did_select = True;
	(void) update(TRUE);
	if (scroll_count > 0) {
	    x_flush();
	    cur_win->sel_scroll_id =
		XtAppAddTimeOut(cur_win->app_context,
				interval,
				scroll_selection,
				(XtPointer) rowcol);
	}
    } else {
	kbd_alarm();
    }
}

static void
multi_click(TextWindow tw, int nr, int nc)
{
    VIDEO_TEXT *p;
    int cclass;
    int sc = nc;
    int oc = nc;
    WINDOW *wp;

    tw->numclicks++;

    if ((wp = row2window(nr)) != NULL && nr == mode_row(wp)) {
	set_curwp(wp);
	sel_release();
	(void) update(TRUE);
    } else {
	switch (tw->numclicks) {
	case 0:
	case 1:		/* shouldn't happen */
	    mlforce("BUG: 0 or 1 multiclick value.");
	    return;
	case 2:		/* word */
#if OPT_HYPERTEXT
	    if (setcursor(nr, nc) && exechypercmd(0, 0)) {
		(void) update(TRUE);
		return;
	    }
#endif
	    /* find word start */
	    p = &CELL_TEXT(nr, sc);
	    cclass = charClass[CharOf(*p)];
	    do {
		--sc;
		--p;
	    } while (sc >= 0 && charClass[*p] == cclass);
	    sc++;
	    /* and end */
	    p = &CELL_TEXT(nr, nc);
	    cclass = charClass[CharOf(*p)];
	    do {
		++nc;
		++p;
	    } while (nc < (int) tw->cols && charClass[*p] == cclass);
	    --nc;

	    if (setcursor(nr, sc)) {
		(void) sel_begin();
		extend_selection(tw, nr, nc, FALSE);
		(void) setcursor(nr, oc);
		/* FIXME: Too many updates */
		(void) update(TRUE);
	    }
	    return;
	case 3:		/* line (doesn't include trailing newline) */
	    if (setcursor(nr, sc)) {
		MARK saveDOT;
		saveDOT = DOT;
		(void) gotobol(0, 0);
		(void) sel_begin();
		(void) gotoeol(FALSE, 0);
		(void) sel_extend(FALSE, TRUE);
		DOT = saveDOT;
		cur_win->did_select = True;
		(void) update(TRUE);
	    }
	    return;
	case 4:		/* document (doesn't include trailing newline) */
	    if (setcursor(nr, sc)) {
		MARK saveDOT;
		saveDOT = DOT;
		(void) gotobob(0, 0);
		(void) sel_begin();
		(void) gotoeob(FALSE, 0);
		(void) gotoeol(FALSE, 0);
		(void) sel_extend(FALSE, TRUE);
		DOT = saveDOT;
		cur_win->did_select = True;
		(void) update(TRUE);
	    }
	    return;
	default:
	    /*
	     * This provides a mechanism for getting rid of the
	     * selection.
	     */
	    sel_release();
	    (void) update(TRUE);
	    return;
	}
    }
}

static void
start_selection(TextWindow tw, XButtonPressedEvent * ev, int nr, int nc)
{
    tw->wipe_permitted = FALSE;
    if ((tw->lasttime != 0)
	&& (absol(ev->time - tw->lasttime) < tw->click_timeout)) {
	/* FIXME: This code used to ignore multiple clicks which
	 *      spanned rows.  Do we still want this behavior?
	 *        If so, we'll have to (re)implement it.
	 */
	multi_click(tw, nr, nc);
    } else {
	WINDOW *wp;

	beginDisplay();

	tw->lasttime = ev->time;
	tw->numclicks = 1;
	tw->was_on_msgline = onMsgRow(tw);

	if ((wp = row2window(nr)) != NULL) {
	    set_curwp(wp);
	}
	tw->prevDOT = DOT;

	/*
	 * If we're on the message line, do nothing.
	 *
	 * If we're on a mode line, make the window whose mode line we're
	 * on the current window.
	 *
	 * Otherwise update the cursor position in whatever window we're
	 * in and set things up so that the current position can be the
	 * possible start of a selection.
	 */
	if (reading_msg_line) {
	    /* EMPTY */ ;
	} else if (wp != NULL && nr == mode_row(wp)) {
	    (void) update(TRUE);
	} else if (setcursor(nr, nc)) {
	    if (!cur_win->persistent_selections) {
		sel_yank(SEL_KREG);
		sel_release();
	    }
	    (void) sel_begin();
	    (void) update(TRUE);
	    tw->wipe_permitted = TRUE;
	    /* force the editor to notice the changed DOT, if it cares */
	    kqadd(cur_win, KEY_Mouse);
	}
	endofDisplay();
    }
}

/* this doesn't need to do anything.  it's invoked when we do
	shove KEY_Mouse back on the input stream, to force the
	main editor code to notice that DOT has moved. */
/*ARGSUSED*/
int
mouse_motion(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return TRUE;
}

/*ARGSUSED*/
int
copy_to_clipboard(int f GCC_UNUSED, int n GCC_UNUSED)
{
    if (!cur_win->have_selection) {
	kbd_alarm();
	return FALSE;
    }

    sel_yank(CLIP_KREG);
    x_own_selection(GetAtom(CLIPBOARD));

    return TRUE;
}

/*ARGSUSED*/
int
paste_from_clipboard(int f GCC_UNUSED, int n GCC_UNUSED)
{
    x_paste_selection(GetAtom(CLIPBOARD));
    return TRUE;
}

/*ARGSUSED*/
int
paste_from_primary(int f GCC_UNUSED, int n GCC_UNUSED)
{
    x_paste_selection(XA_PRIMARY);
    return TRUE;
}

static XMotionEvent *
compress_motion(XMotionEvent * ev)
{
    XEvent nev;

    while (XPending(ev->display)) {
	XPeekEvent(ev->display, &nev);
	if (nev.type == MotionNotify &&
	    nev.xmotion.window == ev->window &&
	    nev.xmotion.subwindow == ev->subwindow) {
	    XNextEvent(ev->display, (XEvent *) ev);
	} else
	    break;
    }
    return ev;
}

/*
 * handle non keyboard events associated with vile screen
 */
/*ARGSUSED*/
static void
x_process_event(Widget w GCC_UNUSED,
		XtPointer unused GCC_UNUSED,
		XEvent *ev,
		Boolean *continue_to_dispatch GCC_UNUSED)
{
    int sc, sr;
    UINT ec, er;

    int nr, nc;
    static int onr = -1, onc = -1;

    XMotionEvent *mev;
    XExposeEvent *gev;
    Bool do_sel;
    WINDOW *wp;
    Bool ignore = vile_is_busy;

    switch (ev->type) {
    case Expose:
	gev = (XExposeEvent *) ev;
	sc = gev->x / cur_win->char_width;
	sr = gev->y / cur_win->char_height;
	ec = (UINT) CEIL(gev->x + gev->width, cur_win->char_width);
	er = (UINT) CEIL(gev->y + gev->height, cur_win->char_height);
	x_touch(cur_win, sc, sr, ec, er);
	cur_win->exposed = TRUE;
	if (ev->xexpose.count == 0)
	    x_flush();
	break;

    case VisibilityNotify:
	cur_win->visibility = ev->xvisibility.state;
	XSetGraphicsExposures(dpy,
			      GetColorGC(cur_win, tt_info),
			      cur_win->visibility != VisibilityUnobscured);
	break;

    case MotionNotify:
	if (ignore)
	    break;
	do_sel = cur_win->wipe_permitted;
	if (!(ev->xmotion.state & (Button1Mask | Button3Mask))) {
	    if (!cur_win->focus_follows_mouse)
		return;
	    else
		do_sel = FALSE;
	}
	mev = compress_motion((XMotionEvent *) ev);
	nc = mev->x / cur_win->char_width;
	nr = mev->y / cur_win->char_height;

	if (nr < 0)
	    nr = -1;		/* want to be out of bounds to force scrolling */
	else if (nr > (int) cur_win->rows)
	    nr = (int) cur_win->rows;

	if (nc < 0)
	    nc = 0;
	else if (nc >= (int) cur_win->cols)
	    nc = (int) (cur_win->cols - 1);

	/* ignore any spurious motion during a multi-cick */
	if (cur_win->numclicks > 1
	    && cur_win->lasttime != 0
	    && (absol(ev->xmotion.time - cur_win->lasttime) < cur_win->click_timeout))
	    return;
	if (do_sel) {
	    if (ev->xbutton.state & ControlMask) {
		(void) sel_setshape(rgn_RECTANGLE);
	    }
	    if (nr != onr || nc != onc)
		extend_selection(cur_win, nr, nc, True);
	    onr = nr;
	    onc = nc;
	} else {
	    if (!reading_msg_line && (wp = row2window(nr)) && wp != curwp) {
		(void) set_curwp(wp);
		(void) update(TRUE);
	    }
	}
	break;
    case ButtonPress:
	if (ignore)
	    break;
	nc = ev->xbutton.x / cur_win->char_width;
	nr = ev->xbutton.y / cur_win->char_height;
	TRACE(("ButtonPress #%d (%d,%d)\n", ev->xbutton.button, nr, nc));
	switch (ev->xbutton.button) {
	case Button1:		/* move button and set selection point */
	    start_selection(cur_win, (XButtonPressedEvent *) ev, nr, nc);
	    onr = nr;
	    onc = nc;
	    break;
	case Button2:		/* paste selection */
	    /*
	     * If shifted, paste at mouse.  Otherwise, paste at the last
	     * position marked before beginning a selection.
	     */
	    if (ev->xbutton.state & ShiftMask) {
		if (!setcursor(nr, nc)) {
		    kbd_alarm();	/* don't know how to paste here */
		    break;
		}
	    }
	    x_paste_selection(XA_PRIMARY);
	    break;
	case Button3:		/* end/extend selection */
	    if (((wp = row2window(nr)) != NULL) && sel_buffer() == wp->w_bufp)
		(void) set_curwp(wp);
	    if (ev->xbutton.state & ControlMask)
		(void) sel_setshape(rgn_RECTANGLE);
	    cur_win->wipe_permitted = True;
	    cur_win->prevDOT = DOT;
	    extend_selection(cur_win, nr, nc, False);
	    break;
	case Button4:
	    if (cur_win->wheel_scroll_amount < 0)
		backpage(FALSE, 1);
	    else
		mvupwind(TRUE, cur_win->wheel_scroll_amount);
	    (void) update(TRUE);
	    break;
	case Button5:
	    if (cur_win->wheel_scroll_amount < 0)
		forwpage(FALSE, 1);
	    else
		mvdnwind(TRUE, cur_win->wheel_scroll_amount);
	    (void) update(TRUE);
	    break;
	}
	break;
    case ButtonRelease:
	if (ignore)
	    break;
	TRACE(("ButtonRelease #%d (%d,%d)%s\n",
	       ev->xbutton.button,
	       ev->xbutton.y / cur_win->char_height,
	       ev->xbutton.x / cur_win->char_width,
	       cur_win->did_select ? ": did_select" : ""));
	switch (ev->xbutton.button) {
	case Button1:
	    if (cur_win->persistent_selections)
		sel_yank(SEL_KREG);

	    /* FALLTHRU */
	case Button3:
	    if (cur_win->sel_scroll_id != ((XtIntervalId) 0)) {
		XtRemoveTimeOut(cur_win->sel_scroll_id);
		cur_win->sel_scroll_id = (XtIntervalId) 0;
	    }
	    if (cur_win->did_select && !cur_win->selection_sets_DOT) {
		restore_dot(cur_win->prevDOT);
		(void) update(TRUE);
	    }
	    cur_win->did_select = False;
	    cur_win->wipe_permitted = False;
	    display_cursor((XtPointer) 0, (XtIntervalId *) 0);
	    break;
	}
	break;
    }
}

/*ARGSUSED*/
static void
x_configure_window(Widget w GCC_UNUSED,
		   XtPointer unused GCC_UNUSED,
		   XEvent *ev,
		   Boolean *continue_to_dispatch GCC_UNUSED)
{
    int nr, nc;
    Dimension new_width, new_height;

    if (ev->type != ConfigureNotify)
	return;

    if (ev->xconfigure.height == cur_win->top_height
	&& ev->xconfigure.width == cur_win->top_width)
	return;

    XtVaGetValues(cur_win->top_widget,
		  XtNheight, &new_height,
		  XtNwidth, &new_width,
		  NULL);
    new_height = (Dimension) (((int) (new_height - cur_win->base_height)
			       / cur_win->char_height)
			      * cur_win->char_height);
    new_width = (Dimension) (((int) (new_width - cur_win->base_width)
			      / cur_win->char_width)
			     * cur_win->char_width);

    /* Check to make sure the dimensions are sane both here and below
       to avoid BadMatch errors */
    nr = ((int) new_height / cur_win->char_height);
    nc = ((int) new_width / cur_win->char_width);

    if (nr < MINROWS || nc < MINCOLS) {
	gui_resize(nc, nr);
	/* Calling XResizeWindow will cause another ConfigureNotify
	 * event, so we should return early and let this event occur.
	 */
	return;
    }
#if MOTIF_WIDGETS
    XtVaSetValues(cur_win->form_widget,
		  Nval(XmNresizePolicy, XmRESIZE_NONE),
		  NULL);
    {
	WidgetList children;
	Cardinal nchildren;
	XtVaGetValues(cur_win->form_widget,
		      XmNchildren, &children,
		      XmNnumChildren, &nchildren,
		      NULL);
	XtUnmanageChildren(children, nchildren);
    }
#else
#if NO_WIDGETS || ATHENA_WIDGETS
    XtVaSetValues(cur_win->form_widget,
		  Nval(XtNwidth, new_width + cur_win->pane_width + 2),
		  Nval(XtNheight, new_height),
#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
		  Nval(XtNx, (cur_win->scrollbar_on_left
			      ? (cur_win->pane_width + 2)
			      : 0)),
#endif
		  NULL);
#endif /* NO_WIDGETS */
#endif /* MOTIF_WIDGETS */
    XtVaSetValues(cur_win->screen,
		  Nval(XtNheight, new_height),
		  Nval(XtNwidth, new_width),
#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
		  Nval(XtNx, (cur_win->scrollbar_on_left
			      ? cur_win->pane_width + 2
			      : 0)),
#endif
		  NULL);
    XtVaSetValues(cur_win->pane,
#if !OPT_KEV_SCROLLBARS && !OPT_XAW_SCROLLBARS
		  Nval(XtNwidth, cur_win->pane_width),
#else /* OPT_KEV_SCROLLBARS */
		  Nval(XtNx, (cur_win->scrollbar_on_left
			      ? 0
			      : new_width)),
		  Nval(XtNwidth, cur_win->pane_width + 2),
		  Nval(XtNheight, new_height - cur_win->char_height),
#endif /* OPT_KEV_SCROLLBARS */
		  NULL);
#if MOTIF_WIDGETS
    {
	WidgetList children;
	Cardinal nchildren;
	XtVaGetValues(cur_win->form_widget,
		      XmNchildren, &children,
		      XmNnumChildren, &nchildren,
		      NULL);
	XtManageChildren(children, nchildren);
    }
    XtVaSetValues(cur_win->form_widget,
		  Nval(XmNresizePolicy, XmRESIZE_ANY),
		  NULL);
#endif /* MOTIF_WIDGETS */

    XtVaGetValues(cur_win->top_widget,
		  XtNheight, &cur_win->top_height,
		  XtNwidth, &cur_win->top_width,
		  NULL);
    XtVaGetValues(cur_win->screen,
		  XtNheight, &new_height,
		  XtNwidth, &new_width,
		  NULL);

    nr = (int) (new_height / cur_win->char_height);
    nc = (int) (new_width / cur_win->char_width);

    if (nr < MINROWS || nc < MINCOLS) {
	gui_resize(nc, nr);
	/* Calling XResizeWindow will cause another ConfigureNotify
	 * event, so we should return early and let this event occur.
	 */
	return;
    }

    if (nc != (int) cur_win->cols
	|| nr != (int) cur_win->rows) {
	newscreensize(nr, nc);
	cur_win->rows = (UINT) nr;
	cur_win->cols = (UINT) nc;
	if (check_scrollbar_allocs() == TRUE)	/* no allocation failure */
	    update_scrollbar_sizes();
    }
#if MOTIF_WIDGETS
    lookfor_sb_resize = FALSE;
#endif
}

void
gui_resize(int cols, int rows)
{
    if (cols < MINCOLS)
	cols = MINCOLS;
    if (rows < MINROWS)
	rows = MINROWS;

    XResizeWindow(dpy, XtWindow(cur_win->top_widget),
		  (UINT) (cols * cur_win->char_width + cur_win->base_width),
		  (UINT) (rows * cur_win->char_height + cur_win->base_height));
    /* This should cause a ConfigureNotify event */
}

static int
check_scrollbar_allocs(void)
{
    int newmax = (int) (cur_win->rows / 2);
    int oldmax = cur_win->maxscrollbars;

    if (newmax > oldmax) {

	GROW(cur_win->scrollbars, Widget, oldmax, newmax);
#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
	GROW(cur_win->scrollinfo, ScrollInfo, oldmax, newmax);
	GROW(cur_win->grips, Widget, oldmax, newmax);
#endif

	cur_win->maxscrollbars = newmax;
    }
    return TRUE;
}

static void
configure_bar(Widget w,
	      XEvent *event,
	      String *params,
	      Cardinal *num_params)
{
    WINDOW *wp;
    int i;

    if (*num_params != 1
	|| (event->type != ButtonPress && event->type != ButtonRelease))
	return;

    i = 0;
    for_each_visible_window(wp) {
	if (cur_win->scrollbars[i] == w) {
	    if (strcmp(params[0], "Only") == 0) {
		set_curwp(wp);
		onlywind(TRUE, 0);
	    } else if (strcmp(params[0], "Kill") == 0) {
		set_curwp(wp);
		delwind(TRUE, 0);
	    } else if (strcmp(params[0], "Split") == 0) {
		if (wp->w_ntrows < 3) {
		    kbd_alarm();
		    break;
		} else {
		    int newsize;
		    set_curwp(wp);
		    newsize = CEIL(event->xbutton.y, cur_win->char_height) - 1;
		    if (newsize > wp->w_ntrows - 2)
			newsize = wp->w_ntrows - 2;
		    else if (newsize < 1)
			newsize = 1;
		    splitwind(TRUE, 1);
		    resize(TRUE, newsize);
		}
	    }
	    (void) update(TRUE);
	    break;
	}
	i++;
    }
}

#if MOTIF_WIDGETS
static void
pane_button(Widget w GCC_UNUSED,
	    XtPointer unused GCC_UNUSED,
	    XEvent *ev GCC_UNUSED,
	    Boolean *continue_to_dispatch GCC_UNUSED)
{
    lookfor_sb_resize = TRUE;
}
#endif /* MOTIF_WIDGETS */

/*ARGSUSED*/
static void
x_change_focus(Widget w GCC_UNUSED,
	       XtPointer unused GCC_UNUSED,
	       XEvent *ev,
	       Boolean *continue_to_dispatch GCC_UNUSED)
{
    static int got_focus_event = FALSE;

    TRACE(("x11:x_change_focus(type=%d)\n", ev->type));
    switch (ev->type) {
    case EnterNotify:
	TRACE(("... EnterNotify\n"));
	if (!ev->xcrossing.focus || got_focus_event)
	    return;
	goto focus_in;
    case FocusIn:
	TRACE(("... FocusIn\n"));
	got_focus_event = TRUE;
      focus_in:
	cur_win->show_cursor = True;
#if MOTIF_WIDGETS
	XmProcessTraversal(cur_win->screen, XmTRAVERSE_CURRENT);
#else /* NO_WIDGETS */
	XtSetKeyboardFocus(w, cur_win->screen);
#endif
	x_flush();
	break;
    case LeaveNotify:
	TRACE(("... LeaveNotify\n"));
	if (!ev->xcrossing.focus
	    || got_focus_event
	    || ev->xcrossing.detail == NotifyInferior)
	    return;
	goto focus_out;
    case FocusOut:
	TRACE(("... FocusOut\n"));
	got_focus_event = TRUE;
      focus_out:
	cur_win->show_cursor = False;
	x_flush();
	break;
    }
}

/*ARGSUSED*/
static void
x_wm_delwin(Widget w GCC_UNUSED,
	    XtPointer unused GCC_UNUSED,
	    XEvent *ev,
	    Boolean *continue_to_dispatch GCC_UNUSED)
{
    if (ev->type == ClientMessage
	&& ev->xclient.message_type == GetAtom(WM_PROTOCOLS)
	&& (Atom) ev->xclient.data.l[0] == GetAtom(WM_DELETE_WINDOW)) {
	quit(FALSE, 0);		/* quit might not return */
	(void) update(TRUE);
    }
}

/*
 * Return true if we want to disable reports of the cursor position because the
 * cursor really should be on the message-line.
 */
#if VILE_NEVER
int
x_on_msgline(void)
{
    return reading_msg_line || cur_win->was_on_msgline;
}
#endif

/*
 * Because we poll our input-characters in 'x_getc()', it is possible to have
 * exposure-events pending while doing lengthy processes (e.g., reading from a
 * pipe).  This procedure is invoked from a timer-handler and is designed to
 * handle the exposure-events, and to get keypress-events (i.e., for stopping a
 * lengthy process).
 */
void
x_move_events(void)
{
    XEvent ev;

    while (x_has_events()
	   && !kqfull(cur_win)) {

	/* Get and dispatch next event */
	XtAppNextEvent(cur_win->app_context, &ev);

	/*
	 * Ignore or save certain events which could get us into trouble with
	 * reentrancy.
	 */
	switch (ev.type) {
	case ButtonPress:
	case ButtonRelease:
	case MotionNotify:
	    /* Ignore the event */
	    continue;

	case ClientMessage:
	case SelectionClear:
	case SelectionNotify:
	case SelectionRequest:
	case ConfigureNotify:
	case ConfigureRequest:
	case PropertyNotify:
	case ReparentNotify:
	case ResizeRequest:
	    /* Queue for later processing.  */
	    evqadd(&ev);
	    continue;

	default:
	    /* do nothing here...we'll dispatch the event below */
	    break;
	}

	XtDispatchEvent(&ev);

	/*
	 * If the event was a keypress, check it to see if it was an
	 * interrupt character.  We check here to make sure that the
	 * queue was non-empty, because not all keypresses put
	 * characters into the queue.  We assume that intrc will not
	 * appear in any multi-character sequence generated by a key
	 * press, or that if it does, it will be the last character in
	 * the sequence.  If this is a bad assumption, we will have to
	 * keep track of what the state of the queue was prior to the
	 * keypress and scan the characters added to the queue as a
	 * result of the keypress.
	 */

	if (!kqempty(cur_win) && ev.type == KeyPress) {
	    int c = kqpop(cur_win);
	    if (c == intrc) {
		kqadd(cur_win, esc_c);
#if SYS_VMS
		kbd_alarm();	/* signals? */
#else
		(void) signal_pg(SIGINT);
#endif
	    } else
		kqadd(cur_win, c);
	}
    }
}

#if OPT_WORKING
void
x_working(void)
{
    cur_win->want_to_work = TRUE;
}

static int
x_has_events(void)
{
    if (cur_win->want_to_work == TRUE) {
	x_set_watch_cursor(TRUE);
	cur_win->want_to_work = FALSE;
    }
    return (int) (XtAppPending(cur_win->app_context) & XtIMXEvent);
}

static void
x_set_watch_cursor(int onflag)
{
    static int watch_is_on = FALSE;
#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
    int i;
#endif

    if (onflag == watch_is_on)
	return;

    watch_is_on = onflag;

    if (onflag) {
	set_pointer(XtWindow(cur_win->screen), cur_win->watch_pointer);
#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
	for (i = 0; i < cur_win->nscrollbars; i++) {
	    set_pointer(XtWindow(cur_win->scrollbars[i]), cur_win->watch_pointer);
	    if (i < cur_win->nscrollbars - 1)
		set_pointer(XtWindow(cur_win->grips[i]), cur_win->watch_pointer);
	}
#endif /* OPT_KEV_SCROLLBARS */
    } else {
	set_pointer(XtWindow(cur_win->screen), cur_win->normal_pointer);
#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
	for (i = 0; i < cur_win->nscrollbars; i++) {
	    set_pointer(XtWindow(cur_win->scrollbars[i]),
			curs_sb_v_double_arrow);
	    if (i < cur_win->nscrollbars - 1)
		set_pointer(XtWindow(cur_win->grips[i]),
			    curs_double_arrow);
	}
#endif /* OPT_KEV_SCROLLBARS */
    }
}
#endif /* OPT_WORKING */

static int
evqempty(void)
{
    return evqhead == NULL;
}

static void
evqadd(const XEvent *evp)
{
    struct eventqueue *newentry;
    newentry = typealloc(struct eventqueue);
    if (newentry == NULL)
	return;			/* FIXME: Need method for indicating error */
    newentry->next = NULL;
    newentry->event = *evp;
    if (evqhead == NULL)
	evqhead = evqtail = newentry;
    else {
	evqtail->next = newentry;
	evqtail = newentry;
    }
}

static void
evqdel(XEvent *evp)
{
    struct eventqueue *delentry = evqhead;
    if (delentry == NULL)
	return;			/* should not happen */
    *evp = delentry->event;
    evqhead = delentry->next;
    if (evqhead == NULL)
	evqtail = NULL;
    free((char *) delentry);
}

static void
kqinit(TextWindow tw)
{
    tw->kqhead = 0;
    tw->kqtail = 0;
}

static int
kqempty(TextWindow tw)
{
    return tw->kqhead == tw->kqtail;
}

static int
kqfull(TextWindow tw)
{
    return tw->kqhead == (tw->kqtail + 1) % KQSIZE;
}

static int
kqdel(TextWindow tw)
{
    int c;
    c = tw->kq[tw->kqhead];
    tw->kqhead = (tw->kqhead + 1) % KQSIZE;
    return c;
}

static void
kqadd(TextWindow tw, int c)
{
    tw->kq[tw->kqtail] = c;
    tw->kqtail = (tw->kqtail + 1) % KQSIZE;
}

static int
kqpop(TextWindow tw)
{
    if (--(tw->kqtail) < 0)
	tw->kqtail = KQSIZE - 1;
    return (tw->kq[tw->kqtail]);
}

/*ARGSUSED*/
static void
display_cursor(XtPointer client_data GCC_UNUSED, XtIntervalId * idp)
{
    static Bool am_blinking = FALSE;
    int the_col = (ttcol >= term.cols) ? term.cols - 1 : ttcol;

    /*
     * Return immediately if we are either in the process of making a
     * selection (by wiping with the mouse) or if the cursor is already
     * displayed and display_cursor() is being called explicitly from the
     * event loop in x_getc.
     */
    if (cur_win->wipe_permitted) {
	am_blinking = FALSE;
	if (cur_win->blink_id != (XtIntervalId) 0) {
	    XtRemoveTimeOut(cur_win->blink_id);
	    cur_win->blink_id = (XtIntervalId) 0;
	}
	return;
    }

    if (IS_DIRTY(ttrow, the_col) && idp == (XtIntervalId *) 0) {
	return;
    }

    if (cur_win->show_cursor) {
	if (cur_win->blink_interval > 0
	    || (cur_win->blink_interval < 0 && IS_REVERSED(ttrow, the_col))) {
	    if (idp != (XtIntervalId *) 0 || !am_blinking) {
		/* Set timer to get blinking */
		cur_win->blink_id =
		    XtAppAddTimeOut(cur_win->app_context,
				    (ULONG) max(cur_win->blink_interval,
						-cur_win->blink_interval),
				    display_cursor,
				    (XtPointer) 0);
		cur_win->blink_status ^= BLINK_TOGGLE;
		am_blinking = TRUE;
	    } else {
		cur_win->blink_status &= ~BLINK_TOGGLE;
	    }
	} else {
	    am_blinking = FALSE;
	    cur_win->blink_status &= ~BLINK_TOGGLE;
	    if (cur_win->blink_id != (XtIntervalId) 0) {
		XtRemoveTimeOut(cur_win->blink_id);
		cur_win->blink_id = (XtIntervalId) 0;
	    }
	}

	MARK_CELL_DIRTY(ttrow, the_col);
	MARK_LINE_DIRTY(ttrow);
	xvileDraw(dpy, cur_win, &CELL_TEXT(ttrow, the_col), 1,
		  (UINT) (VATTRIB(CELL_ATTR(ttrow, the_col))
			  ^ ((cur_win->blink_status & BLINK_TOGGLE)
			     ? 0 : VACURS)),
		  ttrow, the_col);
    } else {
	/* This code will get called when the window no longer has the focus. */
	if (cur_win->blink_id != (XtIntervalId) 0) {
	    XtRemoveTimeOut(cur_win->blink_id);
	    cur_win->blink_id = (XtIntervalId) 0;
	}
	am_blinking = FALSE;
	MARK_CELL_DIRTY(ttrow, the_col);
	MARK_LINE_DIRTY(ttrow);
	xvileDraw(dpy, cur_win, &CELL_TEXT(ttrow, the_col), 1,
		  (UINT) VATTRIB(CELL_ATTR(ttrow, the_col)), ttrow, the_col);
	XDrawRectangle(dpy, cur_win->win,
		       (IS_REVERSED(ttrow, the_col)
			? GetColorGC(cur_win, cc_info)
			: GetColorGC(cur_win, rc_info)),
		       x_pos(cur_win, ttcol), y_pos(cur_win, ttrow),
		       (UINT) (cur_win->char_width - 1),
		       (UINT) (cur_win->char_height - 1));
    }
}

/*
 * main event loop.  this means we'll be stuck if an event that needs
 * instant processing comes in while its off doing other work, but
 * there's no (easy) way around that.
 */
static int
x_getc(void)
{
    int c;

    while (!evqempty()) {
	XEvent ev;
	evqdel(&ev);
	XtDispatchEvent(&ev);
    }
#if OPT_WORKING
    x_set_watch_cursor(FALSE);
#endif
    x_start_autocolor_timer();
    for_ever {

	if (tb_more(PasteBuf)) {	/* handle any queued pasted text */
#if OPT_MULTIBYTE
	    UINT result;
	    int check;
	    int limit = (int) tb_length(PasteBuf);
	    int offset = (int) PasteBuf->tb_last;
	    char *data = tb_values(PasteBuf) + offset;

	    check = vl_conv_to_utf32(&result, data, (B_COUNT) (limit - offset));
	    if (check > 0) {
		c = (int) result;
		PasteBuf->tb_last += (size_t) check;
	    } else
#endif
		c = tb_next(PasteBuf);
	    c = (c | (int) NOREMAP);	/* pasted chars are not subject to mapping */
	    cur_win->pasting = True;
	    break;
	} else if (cur_win->pasting) {
	    /*
	     * Set the default position for new pasting to just past the newly
	     * inserted text.
	     *
	     * Except - when the insert ended with a newline and is at the
	     * beginning of a line.  That makes whole-line select/paste
	     * behave "normally".
	     */
	    if (DOT.o < llength(DOT.l)
		&& !insertmode
		&& !(DOT.o == 0
		     && cur_win->last_getc == (NOREMAP | '\n'))) {
		DOT.o += BytesAt(DOT.l, DOT.o);
		/* Advance DOT so that consecutive pastes come out right */
	    }
	    cur_win->pasting = False;
	    update(TRUE);	/* make sure ttrow & ttcol are valid */
	}

	if (!kqempty(cur_win)) {
	    c = kqdel(cur_win);
	    break;
	}

	/*
	 * Get and dispatch as many X events as possible.  This permits
	 * the editor to catch up if it gets behind in processing keyboard
	 * events since the keyboard queue will likely have something in it.
	 * update() will check for typeahead and will defer its processing
	 * until there is nothing more in the keyboard queue.
	 */

	do {
	    XEvent ev;
	    XtAppNextEvent(cur_win->app_context, &ev);
	    XtDispatchEvent(&ev);
	} while (x_has_events()
		 && !kqfull(cur_win));
    }

    x_stop_autocolor_timer();

    if (c != ((int) NOREMAP | esc_c))
	cur_win->last_getc = c;
    return c;
}

/*
 * Another event loop used for determining type-ahead.
 *
 * milli - milliseconds to wait for type-ahead
 */
int
x_milli_sleep(int milli)
{
    int status;
    XtIntervalId timeoutid = 0;
    int timedout;
    int olddkr;

    if (!cur_win->exposed)
	return FALSE;

    olddkr = im_waiting(TRUE);

    status = !kqempty(cur_win) || tb_more(PasteBuf);

    if (!status) {

	if (milli) {
	    timedout = 0;
	    timeoutid = XtAppAddTimeOut(cur_win->app_context,
					(ULONG) milli,
					x_typahead_timeout,
					(XtPointer) &timedout);
	} else
	    timedout = 1;

	while (kqempty(cur_win) && !evqempty()) {
	    XEvent ev;
	    evqdel(&ev);
	    XtDispatchEvent(&ev);
	}
#if OPT_WORKING
	x_set_watch_cursor(FALSE);
#endif

	/*
	 * Process pending events until we get some keyboard input.
	 * Note that we do not block here.
	 */
	while (kqempty(cur_win) &&
	       x_has_events()) {
	    XEvent ev;
	    XtAppNextEvent(cur_win->app_context, &ev);
	    XtDispatchEvent(&ev);
	}

	/* Now wait for timer and process events as necessary. */
	while (!timedout && kqempty(cur_win)) {
	    XtAppProcessEvent(cur_win->app_context, (XtInputMask) XtIMAll);
	}

	if (!timedout)
	    XtRemoveTimeOut(timeoutid);

	status = !kqempty(cur_win);
    }

    (void) im_waiting(olddkr);

    return status;
}

static int
x_typeahead(void)
{
    return x_milli_sleep(0);
}

/*ARGSUSED*/
static void
x_typahead_timeout(XtPointer flagp, XtIntervalId * id GCC_UNUSED)
{
    *(int *) flagp = 1;
}

/*ARGSUSED*/
static void
x_key_press(Widget w GCC_UNUSED,
	    XtPointer unused GCC_UNUSED,
	    XEvent *ev,
	    Boolean *continue_to_dispatch GCC_UNUSED)
{
    char buffer[128];
    KeySym keysym;
    int num;

#if OPT_INPUT_METHOD
    UINT uch;
#endif

    int i;
    size_t n;
    /* *INDENT-OFF* */
    static const struct {
	KeySym key;
	int code;
    } escapes[] = {
	/* Arrow keys */
	{ XK_Up,	KEY_Up },
	{ XK_Down,	KEY_Down },
	{ XK_Right,	KEY_Right },
	{ XK_Left,	KEY_Left },
	/* page scroll */
	{ XK_Next,	KEY_Next },
	{ XK_Prior,	KEY_Prior },
	{ XK_Home,	KEY_Home },
	{ XK_End,	KEY_End },
	/* editing */
	{ XK_Insert,	KEY_Insert },
	{ XK_Delete,	KEY_Delete },
	{ XK_Find,	KEY_Find },
	{ XK_Select,	KEY_Select },
	/* command keys */
	{ XK_Menu,	KEY_Menu },
	{ XK_Help,	KEY_Help },
	/* function keys */
	{ XK_F1,	KEY_F1 },
	{ XK_F2,	KEY_F2 },
	{ XK_F3,	KEY_F3 },
	{ XK_F4,	KEY_F4 },
	{ XK_F5,	KEY_F5 },
	{ XK_F6,	KEY_F6 },
	{ XK_F7,	KEY_F7 },
	{ XK_F8,	KEY_F8 },
	{ XK_F9,	KEY_F9 },
	{ XK_F10,	KEY_F10 },
	{ XK_F11,	KEY_F11 },
	{ XK_F12,	KEY_F12 },
	{ XK_F13,	KEY_F13 },
	{ XK_F14,	KEY_F14 },
	{ XK_F15,	KEY_F15 },
	{ XK_F16,	KEY_F16 },
	{ XK_F17,	KEY_F17 },
	{ XK_F18,	KEY_F18 },
	{ XK_F19,	KEY_F19 },
	{ XK_F20,	KEY_F20 },
#if defined(XK_F21) && defined(KEY_F21)
	{ XK_F21,	KEY_F21 },
	{ XK_F22,	KEY_F22 },
	{ XK_F23,	KEY_F23 },
	{ XK_F24,	KEY_F24 },
	{ XK_F25,	KEY_F25 },
	{ XK_F26,	KEY_F26 },
	{ XK_F27,	KEY_F27 },
	{ XK_F28,	KEY_F28 },
	{ XK_F29,	KEY_F29 },
	{ XK_F30,	KEY_F30 },
	{ XK_F31,	KEY_F31 },
	{ XK_F32,	KEY_F32 },
	{ XK_F33,	KEY_F33 },
	{ XK_F34,	KEY_F34 },
	{ XK_F35,	KEY_F35 },
#endif
	/* keypad function keys */
	{ XK_KP_F1,	KEY_KP_F1 },
	{ XK_KP_F2,	KEY_KP_F2 },
	{ XK_KP_F3,	KEY_KP_F3 },
	{ XK_KP_F4,	KEY_KP_F4 },
#if defined(XK_KP_Up)
	{ XK_KP_Up,	KEY_Up },
	{ XK_KP_Down,	KEY_Down },
	{ XK_KP_Right,	KEY_Right },
	{ XK_KP_Left,	KEY_Left },
	{ XK_KP_Next,	KEY_Next },
	{ XK_KP_Prior,	KEY_Prior },
	{ XK_KP_Home,	KEY_Home },
	{ XK_KP_End,	KEY_End },
	{ XK_KP_Insert, KEY_Insert },
	{ XK_KP_Delete, KEY_Delete },
#endif
#ifdef  XK_ISO_Left_Tab
	{ XK_ISO_Left_Tab, KEY_Tab },	/* with shift, becomes back-tab */
#endif
    };
    /* *INDENT-ON* */

    if (ev->type != KeyPress)
	return;

    x_start_autocolor_timer();

#if OPT_INPUT_METHOD
    if (!XFilterEvent(ev, *(&ev->xkey.window))) {
	if (cur_win->imInputContext != NULL) {
	    Status status_return;
#ifdef HAVE_XUTF8LOOKUPSTRING
	    if (b_is_utfXX(curbp)) {
		num = Xutf8LookupString(cur_win->imInputContext,
					(XKeyPressedEvent *) ev, buffer,
					(int) sizeof(buffer), &keysym,
					&status_return);
	    } else
#endif
	    {
		num = XmbLookupString(cur_win->imInputContext,
				      (XKeyPressedEvent *) ev, buffer,
				      (int) sizeof(buffer), &keysym,
				      &status_return);
	    }
	} else {
	    num = XLookupString((XKeyPressedEvent *) ev, buffer,
				(int) sizeof(buffer), &keysym,
				(XComposeStatus *) 0);
	}
    } else {
	return;
    }
#else
    num = XLookupString((XKeyPressedEvent *) ev, buffer, sizeof(buffer),
			&keysym, (XComposeStatus *) 0);
#endif

    TRACE((T_CALLED "x_key_press(0x%4X) = %.*s (%s%s%s%s%s%s%s%s)\n",
	   (int) keysym,
	   ((num > 0 && keysym < 256) ? num : 0),
	   buffer,
	   (ev->xkey.state & ShiftMask) ? "Shift" : "",
	   (ev->xkey.state & LockMask) ? "Lock" : "",
	   (ev->xkey.state & ControlMask) ? "Ctrl" : "",
	   (ev->xkey.state & Mod1Mask) ? "Mod1" : "",
	   (ev->xkey.state & Mod2Mask) ? "Mod2" : "",
	   (ev->xkey.state & Mod3Mask) ? "Mod3" : "",
	   (ev->xkey.state & Mod4Mask) ? "Mod4" : "",
	   (ev->xkey.state & Mod5Mask) ? "Mod5" : ""));

    if (num <= 0) {
	unsigned modifier = 0;

	if (ev->xkey.state & ShiftMask)
	    modifier |= mod_SHIFT;
	if (ev->xkey.state & ControlMask)
	    modifier |= mod_CTRL;
	if (ev->xkey.state & Mod1Mask)
	    modifier |= mod_ALT;
	if (modifier != 0)
	    modifier |= mod_KEY;

	for (n = 0; n < TABLESIZE(escapes); n++) {
	    if (keysym == escapes[n].key) {
		TRACE(("ADD-FKEY %#x\n", escapes[n].code));
		kqadd(cur_win, (int) modifier | escapes[n].code);
		break;
	    }
	}
    } else {
	unsigned modifier = 0;

	if (ev->xkey.state & ShiftMask)
	    modifier |= mod_SHIFT;
	if (modifier != 0)
	    modifier |= mod_KEY;
	TRACE(("modifier %#x\n", modifier));

	if (num == 1 && (ev->xkey.state & Mod1Mask)) {
	    TRACE(("ADD-META %#x\n", CharOf(buffer[0])));
	    buffer[0] |= (char) HIGHBIT;
	}

	/* FIXME: Should do something about queue full conditions */
	for (i = 0; i < num && !kqfull(cur_win); i++) {
	    int ch = CharOf(buffer[i]);
	    if (isCntrl(ch)) {
		TRACE(("ADD-CTRL %#x\n", ch));
		kqadd(cur_win, (int) modifier | ch);
	    }
#if OPT_INPUT_METHOD
	    else if (i == 0
		     && num > 1
		     && cur_win->imInputContext != NULL
		     && b_is_utfXX(curbp)
		     && vl_conv_to_utf32(&uch, buffer, (B_COUNT) num) == num) {
		TRACE(("ADD-CHAR %#x\n", uch));
		kqadd(cur_win, (int) uch);
		break;
	    }
#endif
	    else {
		TRACE(("ADD-CHAR %#x\n", ch));
		kqadd(cur_win, ch);
	    }
	}
    }
    returnVoid();
}

/*
 * change reverse video status
 */
static void
x_rev(UINT state)
{
    cur_win->reverse = (int) state;
}

#if OPT_COLOR
static void
x_set_foreground(int color)
{
    TRACE(("x_set_foreground(%d), cur_win->fg was %#lx\n", color, cur_win->fg));
    cur_win->fg = (color >= 0 && color < NCOLORS)
	? cur_win->colors_fg[color]
	: cur_win->default_fg;
    TRACE(("...cur_win->fg = %#lx%s\n", cur_win->fg, cur_win->fg ==
	   cur_win->default_fg ? " (default)" : ""));

    XSetForeground(dpy, GetColorGC(cur_win, tt_info), cur_win->fg);
    XSetBackground(dpy, GetColorGC(cur_win, rt_info), cur_win->fg);

    x_touch(cur_win, 0, 0, cur_win->cols, cur_win->rows);
    x_flush();
}

static void
x_set_background(int color)
{
    TRACE(("x_set_background(%d), cur_win->bg was %#lx\n", color, cur_win->bg));
    cur_win->bg = (color >= 0 && color < NCOLORS)
	? (SamePixel(cur_win->colors_bg[color], cur_win->default_bg)
	   ? cur_win->colors_fg[color]
	   : cur_win->colors_bg[color])
	: cur_win->default_bg;
    TRACE(("...cur_win->bg = %#lx%s\n",
	   cur_win->bg,
	   cur_win->bg == cur_win->default_bg ? " (default)" : ""));

    if (color == ENUM_FCOLOR) {
	XSetBackground(dpy, GetColorGC(cur_win, tt_info), cur_win->default_bg);
	XSetForeground(dpy, GetColorGC(cur_win, rt_info), cur_win->default_bg);
    } else {
	XSetBackground(dpy, GetColorGC(cur_win, tt_info), cur_win->bg);
	XSetForeground(dpy, GetColorGC(cur_win, rt_info), cur_win->bg);
    }
    cur_win->bg_follows_fg = (Boolean) (color == ENUM_FCOLOR);
    TRACE(("...cur_win->bg_follows_fg = %#x\n", cur_win->bg_follows_fg));

    reset_color_gcs();

    XtVaSetValues(cur_win->screen,
		  Nval(XtNbackground, cur_win->bg),
		  NULL);

    x_touch(cur_win, 0, 0, cur_win->cols, cur_win->rows);
    x_flush();
}

static void
x_set_cursor_color(int color)
{
    Pixel fg, bg;
    ColorGC *data;

    TRACE((T_CALLED "x_set_cursor_color(%d)\n", color));
    if (cur_win->is_color_cursor == False) {
	if (!color_cursor(cur_win)) {
	    gccolor = -1;
	    return;
	}
    }

    fg = (color >= 0 && color < NCOLORS)
	? (SamePixel(cur_win->colors_bg[color], cur_win->default_bg)
	   ? cur_win->colors_bg[color]
	   : cur_win->colors_fg[color])
	: cur_win->cursor_fg;

    bg = (color >= 0 && color < NCOLORS)
	? (SamePixel(cur_win->colors_bg[color], cur_win->default_bg)
	   ? cur_win->colors_fg[color]
	   : cur_win->colors_bg[color])
	: cur_win->cursor_bg;

    data = &cur_win->cc_info;
    data->state = sgINIT;
    data->gcmask = GCForeground | GCBackground;
    data->gcvals.background = bg;
    data->gcvals.foreground = fg;
    makeColorGC(cur_win, data);

    data = &cur_win->rc_info;
    data->state = sgINIT;
    data->gcmask = GCForeground | GCBackground;
    data->gcvals.foreground = bg;
    data->gcvals.background = fg;
    makeColorGC(cur_win, data);

    returnVoid();
}

#endif

/* beep */
static void
x_beep(void)
{
#if OPT_FLASH
    if (global_g_val(GMDFLASH)) {
	beginDisplay();
	XGrabServer(dpy);
	XSetFunction(dpy, GetColorGC(cur_win, tt_info), GXxor);
	XSetBackground(dpy, GetColorGC(cur_win, tt_info), 0L);
	XSetForeground(dpy,
		       GetColorGC(cur_win, tt_info),
		       cur_win->fg ^ cur_win->bg);
	XFillRectangle(dpy,
		       cur_win->win,
		       GetColorGC(cur_win, tt_info),
		       0, 0,
		       x_width(cur_win),
		       x_height(cur_win));
	XFlush(dpy);
	catnap(90, FALSE);
	XFillRectangle(dpy,
		       cur_win->win,
		       GetColorGC(cur_win, tt_info),
		       0, 0,
		       x_width(cur_win),
		       x_height(cur_win));
	XFlush(dpy);
	XSetFunction(dpy, GetColorGC(cur_win, tt_info), GXcopy);
	XSetBackground(dpy, GetColorGC(cur_win, tt_info), cur_win->bg);
	XSetForeground(dpy, GetColorGC(cur_win, tt_info), cur_win->fg);
	XUngrabServer(dpy);
	endofDisplay();
    } else
#endif
	XBell(dpy, 0);
}

#if NO_LEAKS
void
x11_leaks(void)
{
    if (cur_win != 0) {
	FreeIfNeeded(cur_win->fontname);
    }
#if OPT_MENUS
    init_menus();
#endif
}
#endif /* NO_LEAKS */

#ifdef USE_SET_WM_NAME
static char x_window_name[NFILEN];
#endif

static char x_icon_name[NFILEN];

void
x_set_icon_name(const char *name)
{
    XTextProperty Prop;

    (void) strncpy0(x_icon_name, name, (size_t) NFILEN);

    Prop.value = (UCHAR *) x_icon_name;
    Prop.encoding = XA_STRING;
    Prop.format = 8;
    Prop.nitems = strlen(x_icon_name);

    XSetWMIconName(dpy, XtWindow(cur_win->top_widget), &Prop);
    TRACE(("x_set_icon_name(%s)\n", name));
}

char *
x_get_icon_name(void)
{
    return x_icon_name;
}

void
x_set_window_name(const char *name)
{
    if (name != NULL && strcmp(name, x_get_window_name())) {
#ifdef USE_SET_WM_NAME
	XTextProperty Prop;

	(void) strncpy0(x_window_name, name, NFILEN);

	Prop.value = (UCHAR *) x_window_name;
	Prop.encoding = XA_STRING;
	Prop.format = 8;
	Prop.nitems = strlen(x_window_name);

	XSetWMName(dpy, XtWindow(cur_win->top_widget), &Prop);
	TRACE(("x_set_window_name(%s)\n", name));
#else
	XtVaSetValues(cur_win->top_widget,
		      Nval(XtNtitle, name),
		      NULL);
#endif
    }
}

const char *
x_get_window_name(void)
{
    const char *result;
#ifdef USE_SET_WM_NAME
    result = x_window_name;
#else
    result = "";
    if (cur_win->top_widget != NULL) {
	XtVaGetValues(cur_win->top_widget, XtNtitle, &result, NULL);
    }
#endif
    TRACE(("x_get_window_name(%s)\n", result));
    return result;
}

char *
x_get_display_name(void)
{
    return XDisplayString(dpy);
}

static void
watched_input_callback(XtPointer fd,
		       int *src GCC_UNUSED,
		       XtInputId * id GCC_UNUSED)
{
    dowatchcallback((int) (long) fd);
}

static int
x_watchfd(int fd, WATCHTYPE type, long *idp)
{
    *idp = (long) XtAppAddInput(cur_win->app_context,
				fd,
				(XtPointer) ((type & WATCHREAD)
					     ? XtInputReadMask
					     : ((type & WATCHWRITE)
						? XtInputWriteMask
						: XtInputExceptMask)),
				watched_input_callback,
				(XtPointer) (long) fd);
    return TRUE;
}

static void
x_unwatchfd(int fd GCC_UNUSED, long id)
{
    XtRemoveInput((XtInputId) id);
}

/* Autocolor functions
 *
 * Note that these are self contained and could be moved to another
 * file if desired.
 */

#if OPT_COLOR&&!SMALLER
static XtIntervalId x_autocolor_timeout_id;

static void
x_start_autocolor_timer(void)
{
    int millisecs = global_b_val(VAL_AUTOCOLOR);
    x_stop_autocolor_timer();
    if (millisecs > 0)
	x_autocolor_timeout_id = XtAppAddTimeOut(cur_win->app_context,
						 (ULONG) millisecs,
						 x_autocolor_timeout,
						 (XtPointer) 0);
}

static void
x_stop_autocolor_timer(void)
{
    if (x_autocolor_timeout_id != 0)
	XtRemoveTimeOut(x_autocolor_timeout_id);
    x_autocolor_timeout_id = 0;
}

static void
x_autocolor_timeout(XtPointer data GCC_UNUSED, XtIntervalId * id GCC_UNUSED)
{
    if (kqempty(cur_win)) {
	XClientMessageEvent ev;

	autocolor();
	XSync(dpy, False);

	/* Send a null message to ourselves to prevent stalling in
	   the event loop. */
	ev.type = ClientMessage;
	ev.serial = 0;
	ev.send_event = True;
	ev.display = dpy;
	ev.window = cur_win->win;
	ev.message_type = None;
	ev.format = 8;
	XSendEvent(dpy, cur_win->win, False, (long) 0, (XEvent *) &ev);
    }
}
#endif /* OPT_COLOR&&!SMALLER */

/*
 * Return true if the given character would be printable.  Not all characters
 * are printable.
 */
int
gui_isprint(int ch)
{
    XVileFont *pf = cur_win->fonts.norm;
    int result = TRUE;

    if (ch >= 0 && pf != NULL) {
#ifdef XRENDERFONT
	if (!XftGlyphExists(dpy, pf, (FcChar32) ch)) {
	    result = FALSE;
	}
#else
	static XCharStruct dft, *tmp = &dft;
	XCharStruct *pc = NULL;

	if (pf->per_char != NULL
	    && !pf->all_chars_exist) {

	    if (pf->max_byte1 == 0) {
		if (ch > 255) {
		    result = FALSE;
		} else {
		    CI_GET_CHAR_INFO_1D(pf, (unsigned) ch, tmp, pc);
		    if (pc == NULL || CI_NONEXISTCHAR(pc)) {
			result = FALSE;
		    }
		}
	    } else {
		CI_GET_CHAR_INFO_2D(pf, CharOf((ch >> 8)), CharOf(ch), tmp, pc);
		if (pc == NULL || CI_NONEXISTCHAR(pc)) {
		    result = FALSE;
		}
	    }
	}
#endif
    }
    return result;
}

#if OPT_INPUT_METHOD
/*
 * This is more or less stolen straight from XFree86 xterm.
 * This should support all European type languages.
 */

/* adapted from IntrinsicI.h */
#define MyStackAlloc(size, stack_cache_array)     \
    ((size) <= sizeof(stack_cache_array)	  \
    ?  (XtPointer)(stack_cache_array)		  \
    :  (XtPointer)malloc((size_t)(size)))

#define MyStackFree(pointer, stack_cache_array) \
    if ((pointer) != ((char *)(stack_cache_array))) free(pointer)
/*
 *  For OverTheSpot, client has to inform the position for XIM preedit.
 */
static void
PreeditPosition(void)
{
    XPoint spot;
    XVaNestedList list;

    if (cur_win->imInputContext) {
	spot.x = (short) x_pos(cur_win, ttcol);
	spot.y = (short) y_pos(cur_win, ttrow);
	list = XVaCreateNestedList(0,
				   XNSpotLocation, &spot,
				   XNForeground, cur_win->fg,
				   XNBackground, cur_win->bg,
				   NULL);
	XSetICValues(cur_win->imInputContext, XNPreeditAttributes, list, NULL);
	XFree(list);
    }
}

/* limit this feature to recent XFree86 since X11R6.x core dumps */
#if defined(XtSpecificationRelease) && XtSpecificationRelease >= 6 && defined(X_HAVE_UTF8_STRING)
#define USE_XIM_INSTANTIATE_CB

static void
xim_instantiate_cb(Display *display,
		   XPointer client_data GCC_UNUSED,
		   XPointer call_data GCC_UNUSED)
{
    TRACE(("xim_instantiate_cb\n"));
    if (display == XtDisplay(cur_win->screen)) {
	init_xlocale();
    }
}

static void
xim_destroy_cb(XIM im GCC_UNUSED,
	       XPointer client_data GCC_UNUSED,
	       XPointer call_data GCC_UNUSED)
{
    TRACE(("xim_destroy_cb\n"));
    cur_win->xim = NULL;

    XRegisterIMInstantiateCallback(XtDisplay(cur_win->screen),
				   NULL, NULL, NULL,
				   xim_instantiate_cb, NULL);
}
#endif /* X11R6+ */

static void
xim_real_init(void)
{
    unsigned i, j;
    char *p, *s, *t, *ns, *end, buf[32];
    char *save_ctype = NULL;
    XIMStyle input_style = 0;
    XIMStyles *xim_styles = NULL;
    Bool found;
    static struct {
	const char *name;
	unsigned long code;
    } known_style[] = {
	{
	    "OverTheSpot", (XIMPreeditPosition | XIMStatusNothing)
	},
	{
	    "OffTheSpot", (XIMPreeditArea | XIMStatusArea)
	},
	{
	    "Root", (XIMPreeditNothing | XIMStatusNothing)
	},
    };

    cur_win->imInputContext = NULL;

    if (cur_win->cannot_im) {
	return;
    }

    if (okCTYPE2(vl_wide_enc))
	save_ctype = setlocale(LC_CTYPE, vl_wide_enc.locale);

    TRACE((T_CALLED "init_xlocale:\n  inputMethod:%s\n  preeditType:%s\n",
	   NonNull(cur_win->rs_inputMethod),
	   NonNull(cur_win->rs_preeditType)));

    if (isEmpty(cur_win->rs_inputMethod)) {
	if ((p = XSetLocaleModifiers("@im=none")) != NULL && *p)
	    cur_win->xim = XOpenIM(dpy, NULL, NULL, NULL);
    } else {
	s = cur_win->rs_inputMethod;
	i = (unsigned) (5 + strlen(s));
	t = (char *) MyStackAlloc(i, buf);
	if (t == NULL) {
	    fprintf(stderr, "Cannot allocate buffer for input-method\n");
	    ExitProgram(BADEXIT);
	} else {
	    for (ns = s; ns && *s;) {
		while (*s && isSpace(CharOf(*s)))
		    s++;
		if (!*s)
		    break;
		if ((ns = end = strchr(s, ',')) == NULL)
		    end = s + strlen(s);
		while ((end != s) && isSpace(CharOf(end[-1])))
		    end--;

		if (end != s) {
		    strcpy(t, "@im=");
		    strncat(t, s, (size_t) (end - s));

		    if ((p = XSetLocaleModifiers(t)) != NULL && *p
			&& (cur_win->xim = XOpenIM(XtDisplay(cur_win->screen),
						   NULL,
						   NULL,
						   NULL)) != NULL)
			break;

		}
		s = ns + 1;
	    }
	    MyStackFree(t, buf);
	}
    }

    if (cur_win->xim == NULL
	&& (p = XSetLocaleModifiers("")) != NULL
	&& *p) {
	cur_win->xim = XOpenIM(dpy, NULL, NULL, NULL);
    }

    if (cur_win->xim == NULL) {
	fprintf(stderr, "Failed to open input method\n");
	cur_win->cannot_im = True;
	returnVoid();
    }

    if (XGetIMValues(cur_win->xim, XNQueryInputStyle, &xim_styles, NULL)
	|| !xim_styles
	|| !xim_styles->count_styles) {
	fprintf(stderr, "input method doesn't support any style\n");
	CloseInputMethod();
	cur_win->cannot_im = True;
	returnVoid();
    }

    found = False;
    for (s = cur_win->rs_preeditType; s && !found;) {
	while (*s && isSpace(CharOf(*s)))
	    s++;
	if (!*s)
	    break;
	if ((ns = end = strchr(s, ',')) != NULL)
	    ns++;
	else
	    end = s + strlen(s);
	while ((end != s) && isSpace(CharOf(end[-1])))
	    end--;

	if (end != s) {		/* just in case we have a spurious comma */
	    TRACE(("looking for style '%.*s'\n", (int) (end - s), s));
	    for (i = 0; i < XtNumber(known_style); i++) {
		if ((int) strlen(known_style[i].name) == (end - s)
		    && !strncmp(s, known_style[i].name, (size_t) (end - s))) {
		    input_style = known_style[i].code;
		    for (j = 0; j < xim_styles->count_styles; j++) {
			if (input_style == xim_styles->supported_styles[j]) {
			    found = True;
			    break;
			}
		    }
		    if (found)
			break;
		}
	    }
	}

	s = ns;
    }
    XFree(xim_styles);

    if (!found) {
	fprintf(stderr,
		"input method doesn't support my preedit type (%s)\n",
		cur_win->rs_preeditType);
	CloseInputMethod();
	cur_win->cannot_im = True;
	returnVoid();
    }

    /*
     * Check for styles we do not yet support.
     */
    TRACE(("input_style %#lx\n", input_style));
    if (input_style == (XIMPreeditArea | XIMStatusArea)) {
	fprintf(stderr,
		"This program doesn't support the 'OffTheSpot' preedit type\n");
	CloseInputMethod();
	cur_win->cannot_im = True;
	returnVoid();
    }

    /*
     * For XIMPreeditPosition (or OverTheSpot), XIM client has to
     * prepare a font.
     * The font has to be locale-dependent XFontSet, whereas
     * XTerm use Unicode font.  This leads a problem that the
     * same font cannot be used for XIM preedit.
     */
    if (input_style != (XIMPreeditNothing | XIMStatusNothing)) {
	char **missing_charset_list;
	int missing_charset_count;
	char *def_string;
	XVaNestedList p_list;
	XPoint spot =
	{0, 0};
	XFontStruct **fonts;
	char **font_name_list;

	cur_win->imFontSet = XCreateFontSet(XtDisplay(cur_win->screen),
					    cur_win->rs_imFont,
					    &missing_charset_list,
					    &missing_charset_count,
					    &def_string);
	if (cur_win->imFontSet == NULL) {
	    fprintf(stderr, "Preparation of font set "
		    "\"%s\" for XIM failed.\n", cur_win->rs_imFont);
	    cur_win->imFontSet = XCreateFontSet(XtDisplay(cur_win->screen),
						DEFXIMFONT,
						&missing_charset_list,
						&missing_charset_count,
						&def_string);
	}
	if (cur_win->imFontSet == NULL) {
	    fprintf(stderr, "Preparation of default font set "
		    "\"%s\" for XIM failed.\n", DEFXIMFONT);
	    CloseInputMethod();
	    cur_win->cannot_im = True;
	    returnVoid();
	}
	(void) XExtentsOfFontSet(cur_win->imFontSet);
	j = (unsigned) XFontsOfFontSet(cur_win->imFontSet, &fonts, &font_name_list);
	for (i = 0, cur_win->imFontHeight = 0; i < j; i++) {
	    if (cur_win->imFontHeight < (*fonts)->ascent)
		cur_win->imFontHeight = (*fonts)->ascent;
	}
	p_list = XVaCreateNestedList(0,
				     XNSpotLocation, &spot,
				     XNFontSet, cur_win->imFontSet,
				     NULL);
	cur_win->imInputContext = XCreateIC(cur_win->xim,
					    XNInputStyle, input_style,
					    XNClientWindow, cur_win->win,
					    XNFocusWindow, cur_win->win,
					    XNPreeditAttributes, p_list,
					    NULL);
    } else {
	cur_win->imInputContext = XCreateIC(cur_win->xim, XNInputStyle, input_style,
					    XNClientWindow, cur_win->win,
					    XNFocusWindow, cur_win->win,
					    NULL);
    }

    if (!cur_win->imInputContext) {
	fprintf(stderr, "Failed to create input context\n");
	CloseInputMethod();
    }
#if defined(USE_XIM_INSTANTIATE_CB)
    else {
	XIMCallback destroy_cb;

	destroy_cb.callback = xim_destroy_cb;
	destroy_cb.client_data = NULL;
	if (XSetIMValues(cur_win->xim, XNDestroyCallback, &destroy_cb, NULL))
	    fprintf(stderr, "Could not set destroy callback to IM\n");
    }
#endif

    if (save_ctype != NULL)
	setlocale(LC_CTYPE, save_ctype);

    returnVoid();
}

static void
init_xlocale(void)
{
    if (cur_win->open_im) {
	xim_real_init();

#if defined(USE_XIM_INSTANTIATE_CB)
	if (cur_win->imInputContext == NULL && !cur_win->cannot_im) {
	    sleep(3);
	    XRegisterIMInstantiateCallback(XtDisplay(cur_win->screen),
					   NULL, NULL, NULL,
					   xim_instantiate_cb, NULL);
	}
#endif
    }
}
#else
#define PreeditPosition()
#endif /* OPT_INPUT_METHOD */

static void
x_move(int row, int col)
{
    psc_move(row, col);
    PreeditPosition();
}

static const char *
ae_names(XVileAtom n)
{
    const char *result = NULL;

#define DATA(name) case ae ## name: result = #name; break
    switch (n) {
    case aeMAX:
	break;
	DATA(AVERAGE_WIDTH);
	DATA(CHARSET_ENCODING);
	DATA(CHARSET_REGISTRY);
	DATA(CLIPBOARD);
	DATA(COMPOUND_TEXT);
	DATA(FONT);
	DATA(FOUNDRY);
	DATA(MULTIPLE);
	DATA(NONE);
	DATA(PIXEL_SIZE);
	DATA(RESOLUTION_X);
	DATA(RESOLUTION_Y);
	DATA(SETWIDTH_NAME);
	DATA(SLANT);
	DATA(SPACING);
	DATA(TARGETS);
	DATA(TEXT);
	DATA(TIMESTAMP);
	DATA(UTF8_STRING);
	DATA(WEIGHT_NAME);
	DATA(WM_DELETE_WINDOW);
	DATA(WM_PROTOCOLS);
    }
#undef DATA
    return result;
}

Atom
xvileAtom(XVileAtom n)
{
    static Atom xvile_atoms[aeMAX];
    Atom result = None;

    if (n < aeMAX) {
	if (xvile_atoms[n] == None) {
	    xvile_atoms[n] = XInternAtom(dpy, ae_names(n), False);
	}
	result = xvile_atoms[n];
    }
    return result;
}

TERM term =
{
    0,				/* these four values are set dynamically at
				 * open time */
    0,
    0,
    0,
    x_set_encoding,
    x_get_encoding,
    x_open,
    x_close,
    nullterm_kopen,
    nullterm_kclose,
    nullterm_clean,
    nullterm_unclean,
    nullterm_openup,
    x_getc,
    psc_putchar,
    x_typeahead,
    psc_flush,
    x_move,
    psc_eeol,
    psc_eeop,
    x_beep,
    x_rev,
    nullterm_setdescrip,
    x_set_foreground,
    x_set_background,
    x_set_palette,
    x_set_cursor_color,
    x_scroll,
    x_flush,
    nullterm_icursor,
#if OPT_TITLE
    x_set_window_name,
#else
    nullterm_settitle,
#endif
    x_watchfd,
    x_unwatchfd,
    nullterm_cursorvis,
    nullterm_mopen,
    nullterm_mclose,
    nullterm_mevent,
};

#endif /* DISP_X11 && XTOOLKIT */
