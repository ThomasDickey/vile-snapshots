/*
 * 	X11 support, Dave Lemke, 11/91
 *	X Toolkit support, Kevin Buettner, 2/94
 *
 * $Header: /users/source/archives/vile.vcs/RCS/x11.c,v 1.154 1997/05/12 09:44:15 bod Exp $
 *
 */

/*
 * Widget set selection.
 *
 * You must have exactly one of the following defined
 *
 *    NO_WIDGETS	-- Use only Xlib and X toolkit
 *    ATHENA_WIDGETS	-- Use only Xlib X toolkit (Xt) and Xaw
 *    MOTIF_WIDGETS	-- Use Xlib, Xt, and Motif widget set
 *    OL_WIDGETS	-- Use Xlib, Xt, and Openlook widget set
 */

#define NEED_X_INCLUDES 1
#include	"estruct.h"
#include	"edef.h"
#include	"nefunc.h"

#include	<X11/cursorfont.h>

#if defined(lint) && HAVE_X11_INTRINSICI_H
#include	<X11/IntrinsicI.h>
#endif

#if XtSpecificationRelease < 4
#define XtPointer caddr_t
#endif

#if DISP_X11 && XTOOLKIT

#define MY_CLASS	"XVile"

#if SYS_VMS
#undef SYS_UNIX
#define coreWidgetClass widgetClass /* patch for VMS 5.4-3 (dickey) */
#endif

/* redefined in X11/Xos.h */
#undef strchr
#undef strrchr

#if OL_WIDGETS
#undef BANG
#include	<Xol/OpenLook.h>
#include	<Xol/Form.h>
#include	<Xol/BulletinBo.h>
#include	<Xol/Slider.h>
#include	<Xol/Scrollbar.h>
#endif /* OL_WIDGETS */

#include	<X11/Shell.h>
#include	<X11/keysym.h>
#include	<X11/Xatom.h>

#if MOTIF_WIDGETS
#include	<Xm/Form.h>
#include	<Xm/PanedW.h>
#include	<Xm/ScrollBar.h>
#include	<Xm/SashP.h>
#if defined(lint) && HAVE_XM_XMP_H
#include	<Xm/XmP.h>
#endif
#endif /* MOTIF_WIDGETS */

#define	XCalloc(type)	typecalloc(type)

#define	MARGIN	8
#define	SCRSIZ	64
#define	absol(x)	((x) > 0 ? (x) : -(x))
#define	CEIL(a,b)	((a + b - 1) / (b))

#define onMsgRow(tw)	(ttrow == (tw->rows - 1))

/* XXX -- use xcutsel instead */
#undef	SABER_HACK		/* hack to support Saber since it doesn't do
				 * selections right */

#undef btop		/* defined in SunOS includes */

#define PANE_WIDTH_MAX 200

#if NO_WIDGETS || ATHENA_WIDGETS
# define PANE_WIDTH_DEFAULT 15
# define PANE_WIDTH_MIN 7
#else
# if MOTIF_WIDGETS || OL_WIDGETS
#  define PANE_WIDTH_DEFAULT 20
#  define PANE_WIDTH_MIN 10
# endif
#endif

#ifndef XtNmenuHeight
#define XtNmenuHeight "menuHeight"
#endif

#ifndef XtCMenuHeight
#define XtCMenuHeight "MenuHeight"
#endif

#define MENU_HEIGHT_DEFAULT 20

#define MINCOLS	30
#define MINROWS MINWLNS
#define MAXSBS	((MAXROWS) / 2)

/* The following set of definitions comprise the interface to pscreen
 * in display.c.  They should perhaps be moved to a header file so
 * that other screen interfaces can make use of them.
 */
extern VIDEO **pscreen;
#define IS_DIRTY_LINE(r)	(pscreen[(r)]->v_flag & VFCHG)
#define IS_DIRTY(r,c)		(pscreen[(r)]->v_attrs[(c)] & VADIRTY)
#define IS_REVERSED(r,c)	((pscreen[(r)]->v_attrs[(c)] & (VAREV|VASEL)) != 0)
#define MARK_LINE_DIRTY(r)	(pscreen[(r)]->v_flag |= VFCHG)
#define MARK_CELL_DIRTY(r,c)	(pscreen[(r)]->v_attrs[(c)] |= VADIRTY)
#define CLEAR_LINE_DIRTY(r)	(pscreen[(r)]->v_flag &= ~VFCHG)
#define CLEAR_CELL_DIRTY(r,c)	(pscreen[(r)]->v_attrs[(c)] &= ~VADIRTY)
#define CELL_TEXT(r,c)		(pscreen[(r)]->v_text[(c)])
#define CELL_ATTR(r,c)		(pscreen[(r)]->v_attrs[(c)])

/* Blinking cursor toggle...defined this way to leave room for future
 * cursor flags.
 */
#define	BLINK_TOGGLE	0x1

/*
 * Fonts searched flags
 */

#define FSRCH_BOLD	0x1
#define FSRCH_ITAL	0x2
#define FSRCH_BOLDITAL	0x4

/* Keyboard queue size */
#define KQSIZE 64

static Display *dpy;

#if OPT_MENUS
#if ATHENA_WIDGETS
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Paned.h>
#endif
#if MOTIF_WIDGETS
#include <Xm/RowColumn.h>
#endif
    Widget	menub;
#endif

#if NO_WIDGETS || ATHENA_WIDGETS
typedef struct _scroll_info {
    int		top;		/* top of "thumb" */
    int		bot;		/* bottom of "thumb" */
    int		totlen;		/* total length of scrollbar */
    Boolean	exposed;	/* has scrollbar received expose event? */
} ScrollInfo;
#endif

typedef struct _text_win {
    /* X stuff */
    Window	win;		/* window corresponding to screen */
    XtAppContext
    		app_context;	/* application context */
    Widget	top_widget;	/* top level widget */
    Widget	screen;		/* screen widget */
#if ATHENA_WIDGETS
    Widget	pane_widget;	/* pane widget, actually a form */
    Widget	menu_widget;	/* menu-bar widget, actually a box */
#endif
    Widget	form_widget;	/* form enclosing text-display + scrollbars */
    Widget	pane;		/* panes in which scrollbars live */
    int		maxscrollbars;	/* how many scrollbars, sliders, etc. */
    Widget	*scrollbars;
    				/* the scrollbars */
    int		nscrollbars;	/* number of currently active scroll bars */
#if OL_WIDGETS
    Widget	*sliders;
#endif
#if NO_WIDGETS || ATHENA_WIDGETS
    Pixel	scrollbar_fg;
    Pixel	scrollbar_bg;
    Bool	slider_is_solid;
    Bool	slider_is_3D;
    GC		scrollbargc;	/* graphics context for scrollbar "thumb" */
    Pixmap	trough_pixmap;
    Pixmap	slider_pixmap;
    ScrollInfo	*scrollinfo;
    Widget	*grips; 	/* grips for resizing scrollbars */
    XtIntervalId scroll_repeat_id;
    ULONG	scroll_repeat_timeout;
#endif	/* NO_WIDGETS */
    ULONG	scroll_repeat_interval;
    XtIntervalId blink_id;
    int		blink_status;
    int		blink_interval;
    Bool	exposed;	/* Have we received any expose events? */
    int		visibility;	/* How visible is the window? */

    int		base_width;	/* width with screen widgets' width zero */
    int		base_height;
    UINT	pane_width;	/* full width of scrollbar pane */
    Dimension	menu_height;	/* height of menu-bar */
    Dimension	top_width;	/* width of top widget as of last resize */
    Dimension	top_height;	/* height of top widget as of last resize */

    int		fsrch_flags;	/* flags which indicate which fonts have
    				 * been searched for
				 */
    XFontStruct *pfont;		/* Normal font */
    XFontStruct *pfont_bold;
    XFontStruct *pfont_ital;
    XFontStruct *pfont_boldital;
    GC          textgc;
    GC          reversegc;
    GC		selgc;
    GC		revselgc;
    int		is_color_cursor;
    GC		cursgc;
    GC		revcursgc;
    GC		modeline_focus_gc;	/* GC for modeline w/ focus */
    GC		modeline_gc;		/* GC for other modelines  */
    GC		colors_fgc[16];
    GC		colors_bgc[16];
    Pixel	fg;
    Pixel	bg;
    Pixel	colors_fg[16];
    Pixel	colors_bg[16];
    Pixel	modeline_fg;
    Pixel	modeline_bg;
    Pixel	modeline_focus_fg;
    Pixel	modeline_focus_bg;
    Pixel	selection_fg;
    Pixel	selection_bg;
    int         char_width,
                char_ascent,
		char_descent,
                char_height;
    Bool	left_ink,	/* font has "ink" past bounding box on left */
    		right_ink;	/* font has "ink" past bounding box on right */
    char       *geometry;
    char       *starting_fontname;	/* name of font at startup */
    char       *fontname;		/* name of current font */
    Bool	focus_follows_mouse;
    Bool	fork_on_startup;
    Bool	scrollbar_on_left;
    Bool	update_window_name;
    Bool	update_icon_name;
    Bool	persistent_selections;
    Bool	selection_sets_DOT;

    /* text stuff */
    Bool        reverse;
    unsigned    rows,
                cols;
    Bool        show_cursor;

    /* cursor stuff */
    Pixel	cursor_fg;
    Pixel	cursor_bg;

    /* pointer stuff */
    Pixel	pointer_fg;
    Pixel	pointer_bg;
    Cursor	normal_pointer;
#if OPT_WORKING
    Cursor	watch_pointer;
#endif

    /* selection stuff */
    String	multi_click_char_class;	/* ?? */
    Time        lasttime;	/* for multi-click */
    Time        click_timeout;
    int         numclicks;
    Bool        have_selection;
    Bool	wipe_permitted;
    Bool	was_on_msgline;
    Bool	did_select;
    Bool	pasting;
    MARK	prevDOT;		/* DOT prior to selection */
    XtIntervalId sel_scroll_id;

    /* key press queue */
    int		kqhead;
    int		kqtail;
    int		kq[KQSIZE];

    /* Special translations */
    XtTranslations my_scrollbars_trans;
#if NO_WIDGETS || ATHENA_WIDGETS
    XtTranslations my_resizeGrip_trans;
#endif
}           TextWindowRec, *TextWindow;


static	TextWindowRec cur_win_rec;
static	TextWindow cur_win = &cur_win_rec;
static	TBUFF	*PasteBuf;

static	Atom	atom_WM_PROTOCOLS;
static	Atom	atom_WM_DELETE_WINDOW;
static	Atom	atom_FONT;
static	Atom	atom_FOUNDRY;
static	Atom	atom_WEIGHT_NAME;
static	Atom	atom_SLANT;
static	Atom	atom_SETWIDTH_NAME;
static	Atom	atom_PIXEL_SIZE;
static	Atom	atom_RESOLUTION_X;
static	Atom	atom_RESOLUTION_Y;
static	Atom	atom_SPACING;
static	Atom	atom_AVERAGE_WIDTH;
static	Atom	atom_CHARSET_REGISTRY;
static	Atom	atom_CHARSET_ENCODING;
static	Atom	atom_TARGETS;
static	Atom	atom_MULTIPLE;
static	Atom	atom_TIMESTAMP;
static	Atom	atom_TEXT;

#if NO_WIDGETS || ATHENA_WIDGETS
static	Cursor	curs_sb_v_double_arrow;
static	Cursor	curs_sb_up_arrow;
static	Cursor	curs_sb_down_arrow;
static	Cursor	curs_sb_left_arrow;
static	Cursor	curs_sb_right_arrow;
static	Cursor	curs_double_arrow;
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

static	int	x_getc   (void),
		x_cres   ( char *flag );

static	void	x_open   (void),
		x_close  (void),
		x_flush  (void),
		x_kopen  (void),
		x_kclose (void),
		x_beep   (void),
		x_rev    ( int state );

#if OPT_COLOR
static	void	x_fcol   ( int color ),
		x_bcol   ( int color );
#endif

static	void	x_scroll(int from, int to, int count);

static	SIGT	x_quit (int ACTUAL_SIG_ARGS);

static	int	set_character_class(char *s);
static	void	x_touch (TextWindow tw, int sc, int sr, UINT ec, UINT er);
static	void	x_paste_selection (void);
static	Boolean	x_get_selected_text(UCHAR **datp, SIZE_T *lenp);
static	Boolean	x_convert_selection (Widget w, Atom *selection, Atom *target,
			Atom *type, XtPointer *value, unsigned long *length,
			int *format);
static	void	extend_selection (TextWindow tw, int nr, int nc, Bool wipe);
static	void	x_process_event (Widget w, XtPointer unused, XEvent *ev,
			Boolean *continue_to_dispatch);
static	void	x_configure_window (Widget w, XtPointer unused, XEvent *ev,
			Boolean *continue_to_dispatch);
static	void	x_change_focus (Widget w, XtPointer unused, XEvent *ev,
			Boolean *continue_to_dispatch);
static	void	x_typahead_timeout (XtPointer flagp, XtIntervalId *id);
static	void	x_key_press (Widget w, XtPointer unused, XEvent *ev,
			Boolean *continue_to_dispatch);
static	void	x_wm_delwin (Widget w, XtPointer unused, XEvent *ev,
			Boolean *continue_to_dispatch);
#if NO_WIDGETS || ATHENA_WIDGETS
static	Boolean	too_light_or_too_dark (Pixel pixel);
static	Boolean	alloc_shadows (Pixel pixel, Pixel *light, Pixel *dark);
#endif
static	XFontStruct *query_font (TextWindow tw, const char *fname);
static	void	configure_bar (Widget w, XEvent *event, String *params,
			Cardinal *num_params);
static	int	check_scrollbar_allocs (void);
static	void	update_scrollbar_sizes (void);
static	void	kqinit(TextWindow tw);
static	int	kqempty(TextWindow tw);
static	int	kqfull(TextWindow tw);
static	void	kqadd (TextWindow tw, int c);
static	int	kqpop(TextWindow tw);
static	void	display_cursor (XtPointer client_data, XtIntervalId *idp);
#if 0
static	void	check_visuals (void);
#endif
#if MOTIF_WIDGETS
static	void	grip_moved (Widget w, XtPointer unused, XEvent *ev,
			Boolean *continue_to_dispatch);
static	void	pane_button (Widget w, XtPointer unused, XEvent *ev,
			Boolean *continue_to_dispatch);
#else
#if OL_WIDGETS
static	void	grip_moved (Widget slider, XtPointer closure, XtPointer call_data);
#else
#if NO_WIDGETS || ATHENA_WIDGETS
static	void	x_expose_scrollbar (Widget w, XtPointer unused, XEvent *ev,
			Boolean *continue_to_dispatch);
static	void	repeat_scroll (XtPointer count, XtIntervalId  *id);
#endif /* NO_WIDGETS */
#endif /* OL_WIDGETS */
#endif /* MOTIF_WIDGETS */
#if OPT_WORKING
static	void	x_set_watch_cursor(int onflag);
#endif /* OPT_WORKING */
static	int	evqempty (void);
static	void	evqadd(const XEvent *evp);

#define	FONTNAME	"7x13"

TERM        term = {
    0,				/* these four values are set dynamically at
				 * open time */
    0,
    0,
    0,
    MARGIN,
    SCRSIZ,
    0,
    x_open,
    x_close,
    x_kopen,
    x_kclose,
    x_getc,
    psc_putchar,
    tttypahead,
    psc_flush,
    psc_move,
    psc_eeol,
    psc_eeop,
    x_beep,
    x_rev,
    x_cres,
#if OPT_COLOR
    x_fcol,
    x_bcol,
#else
    null_t_setfor,
    null_t_setback,
#endif
    null_t_setpal,		/* no palette */
    x_scroll,
    x_flush,
    null_t_icursor,
    null_t_title,
};


#define	x_width(tw)		((tw)->cols * (tw)->char_width)
#define	x_height(tw)		((tw)->rows * (tw)->char_height)
#define	x_pos(tw, c)		((c) * (tw)->char_width)
#define	y_pos(tw, r)		((r) * (tw)->char_height)
#define	text_y_pos(tw, r)	(y_pos((tw), (r)) + (tw)->char_ascent)
#define min(a,b)		((a) < (b) ? (a) : (b))
#define max(a,b)		((a) > (b) ? (a) : (b))



#if NO_WIDGETS || ATHENA_WIDGETS
/* We define our own little bulletin board widget here...if this gets
 * too unwieldly, we should move it to another file.
 */

#include	<X11/IntrinsicP.h>

/* New fields for the Bb widget class record */
typedef struct {int empty;} BbClassPart;

/* Class record declaration */
typedef struct _BbClassRec {
    CoreClassPart	core_class;
    CompositeClassPart  composite_class;
    BbClassPart	bb_class;
} BbClassRec;

extern BbClassRec bbClassRec;

/* Instance declaration */
typedef struct _BbRec {
    CorePart	    core;
    CompositePart   composite;
} BbRec;

static XtGeometryResult bbGeometryManager (Widget w, XtWidgetGeometry *request, XtWidgetGeometry *reply);
static XtGeometryResult bbPreferredSize (Widget widget, XtWidgetGeometry *constraint, XtWidgetGeometry *preferred);

BbClassRec bbClassRec = {
  {
/* core_class fields      */
    /* superclass         */    (WidgetClass) &compositeClassRec,
    /* class_name         */    "Bb",
    /* widget_size        */    sizeof(BbRec),
    /* class_initialize   */    NULL,
    /* class_part_init    */	NULL,
    /* class_inited       */	FALSE,
    /* initialize         */    NULL,
    /* initialize_hook    */	NULL,
    /* realize            */    XtInheritRealize,
    /* actions            */    NULL,
    /* num_actions	  */	0,
    /* resources          */    NULL,
    /* num_resources      */    0,
    /* xrm_class          */    NULLQUARK,
    /* compress_motion	  */	TRUE,
    /* compress_exposure  */	TRUE,
    /* compress_enterleave*/	TRUE,
    /* visible_interest   */    FALSE,
    /* destroy            */    NULL,
    /* resize             */    XtInheritResize,
    /* expose             */    NULL,
    /* set_values         */    NULL,
    /* set_values_hook    */	NULL,
    /* set_values_almost  */    XtInheritSetValuesAlmost,
    /* get_values_hook    */	NULL,
    /* accept_focus       */    NULL,
    /* version            */	XtVersion,
    /* callback_private   */    NULL,
    /* tm_table           */    NULL,
    /* query_geometry     */	bbPreferredSize, /*XtInheritQueryGeometry,*/
    /* display_accelerator*/	XtInheritDisplayAccelerator,
    /* extension          */	NULL
  },{
/* composite_class fields */
    /* geometry_manager   */    bbGeometryManager,
    /* change_managed     */    XtInheritChangeManaged,
    /* insert_child	  */	XtInheritInsertChild,
    /* delete_child	  */	XtInheritDeleteChild,
    /* extension          */	NULL
  },{
/* Bb class fields */
    /* empty		  */	0,
  }
};

WidgetClass bbWidgetClass = (WidgetClass)&bbClassRec;

/*ARGSUSED*/
static XtGeometryResult
bbPreferredSize(
    Widget widget GCC_UNUSED,
    XtWidgetGeometry *constraint GCC_UNUSED,
    XtWidgetGeometry *preferred GCC_UNUSED)
{
    return XtGeometryYes;
}

/*ARGSUSED*/
static XtGeometryResult
bbGeometryManager(
    Widget		w,
    XtWidgetGeometry	*request,
    XtWidgetGeometry	*reply GCC_UNUSED)	/* RETURN */

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

#endif /* NO_WIDGETS */

static void set_pointer(Window win, Cursor cursor)
{
    XColor colordefs[2];		/* 0 is foreground, 1 is background */

    XDefineCursor(dpy, win, cursor);

    colordefs[0].pixel = cur_win->pointer_fg;
    colordefs[1].pixel = cur_win->pointer_bg;
    XQueryColors (dpy, DefaultColormap (dpy, DefaultScreen (dpy)),
		  colordefs, 2);
    XRecolorCursor (dpy, cursor, colordefs, colordefs+1);
}

#if !NO_WIDGETS && !ATHENA_WIDGETS
static int dont_update_sb = FALSE;

static void
set_scroll_window(long n)
{
    register WINDOW *wp;
    for_each_window(wp) {
	if (n-- == 0)
	    break;
    }
    if (n < 0)
	set_curwp(wp);
}
#endif /* !NO_WIDGETS */

#if OL_WIDGETS
static void
JumpProc(
    Widget scrollbar,
    XtPointer closure,
    XtPointer call_data)
{
    long value = (long)closure;
    OlScrollbarVerify *cbs = (OlScrollbarVerify *)call_data;

    if (value >= cur_win->nscrollbars)
	return;
    set_scroll_window(value);
    mvupwind(TRUE,
             line_no(curwp->w_bufp, curwp->w_line.l) - cbs->new_location);
    dont_update_sb = TRUE;
    (void)update(TRUE);
    dont_update_sb = FALSE;
}

static void
grip_moved(
    Widget slider,
    XtPointer closure,
    XtPointer call_data)
{
    register WINDOW *wp, *saved_curwp;
    int nlines;
    long i = (long) closure;
    OlScrollbarVerify *cbs = (OlScrollbarVerify *)call_data;

    for_each_window(wp) {
	if (i-- == 0)
	    break;
    }
    if (!wp)
	return;
    saved_curwp = curwp;
    nlines = - cbs->new_location;
    if (nlines == 0) {
	delwp(wp);
	if (saved_curwp == wp)
	    saved_curwp = wheadp;
    }
    else {
	curwp = wp;
	resize(TRUE, nlines);
    }
    set_curwp(saved_curwp);
    (void) update(TRUE);
}

static void
update_scrollbar_sizes(void)
{
    register WINDOW *wp;
    int i, newsbcnt;
    Dimension new_height;

    i=0;
    for_each_window(wp)
	i++;
    newsbcnt=i;

    /* Create any needed new scrollbars and sliders */
    for (i = cur_win->nscrollbars+1; i <= newsbcnt; i++)
	if (cur_win->scrollbars[i] == NULL) {
	    cur_win->scrollbars[i] = XtVaCreateWidget(
		    "scrollbar",
		    scrollbarWidgetClass,
		    cur_win->pane,
		    XtNtranslations, cur_win->my_scrollbars_trans,
		    NULL);
	    XtAddCallback(cur_win->scrollbars[i],
		    XtNsliderMoved, JumpProc, (XtPointer) i);
	    cur_win->sliders[i] = XtVaCreateWidget(
		    "slider",
		    sliderWidgetClass,
		    cur_win->pane,
		    NULL);
	    XtAddCallback(cur_win->sliders[i],
		    XtNsliderMoved, grip_moved, (XtPointer) i);
	}

    /* Unmanage current set of scrollbars */
    if (cur_win->nscrollbars > 0)
	XtUnmanageChildren(cur_win->scrollbars,
	                   (Cardinal) (cur_win->nscrollbars));
    if (cur_win->nscrollbars > 1)
	XtUnmanageChildren(cur_win->sliders,
	                   (Cardinal) (cur_win->nscrollbars - 1));

    /* Set sizes and positions on scrollbars and sliders */
    cur_win->nscrollbars = newsbcnt;
    i=0;
    for_each_window(wp) {
	new_height = wp->w_ntrows * cur_win->char_height;
	XtVaSetValues(cur_win->scrollbars[i],
	    XtNy,	wp->w_toprow * cur_win->char_height,
	    XtNheight,	new_height,
	    XtNsliderMin,	1,
	    XtNsliderMax,	200,
	    XtNproportionLength, 2,
	    XtNsliderValue,	3,
	    NULL);
	if (wp->w_wndp) {
	    XtVaSetValues(cur_win->sliders[i],
		XtNy,		wp->w_toprow * cur_win->char_height,
		XtNheight,	(wp->w_ntrows + wp->w_wndp->w_ntrows + 1)
					    * cur_win->char_height,
		XtNsliderMax,	0,
		XtNsliderMin,	-(wp->w_ntrows + wp->w_wndp->w_ntrows),
		XtNsliderValue, -wp->w_ntrows,
		XtNstopPosition, OL_GRANULARITY,
		XtNendBoxes,	FALSE,
		XtNdragCBType,	OL_RELEASE,
		XtNbackground,	cur_win->fg,
		NULL);
	}
	wp->w_flag &= ~WFSBAR;
	update_scrollbar(wp);
	i++;
    }

    /* Manage the sliders */
    if (cur_win->nscrollbars > 1)
	XtManageChildren(cur_win->sliders,
			    (Cardinal) (cur_win->nscrollbars - 1));

    /* Manage the current set of scrollbars */
    XtManageChildren(cur_win->scrollbars,
	                   (Cardinal) (cur_win->nscrollbars));


    for (i=0; i<cur_win->nscrollbars; i++)
	XRaiseWindow(dpy, XtWindow(cur_win->scrollbars[i]));
}

#else
#if MOTIF_WIDGETS

static void
JumpProc(
    Widget scrollbar,
    XtPointer closure,
    XtPointer call_data)
{
    long value = (long)closure;
    int lcur;

    lookfor_sb_resize = FALSE;
    if (value >= cur_win->nscrollbars)
	return;
    set_scroll_window(value);
    lcur = line_no(curwp->w_bufp, curwp->w_line.l);
    mvupwind(TRUE, lcur - ((XmScrollBarCallbackStruct *)call_data)->value);
    dont_update_sb = TRUE;
    (void)update(TRUE);
    dont_update_sb = FALSE;
}

static void
update_scrollbar_sizes(void)
{
    register WINDOW *wp;
    int newsbcnt;
    Dimension new_height;
    Widget *children;
    int num_children;
    long i=0;

    for_each_window(wp)
	i++;
    newsbcnt=i;

    /* Remove event handlers on sashes */
    XtVaGetValues(cur_win->pane,
	XmNchildren, &children,
	XmNnumChildren, &num_children,
	NULL);
    while (num_children-- > 0)
	if (XmIsSash(children[num_children]))
	    XtRemoveEventHandler(
		children[num_children],
		ButtonReleaseMask,
		FALSE,
		pane_button,
		NULL);

    /* Create any needed new scrollbars */
    for (i = cur_win->nscrollbars+1; i <= newsbcnt; i++)
	if (cur_win->scrollbars[i] == NULL) {
	    cur_win->scrollbars[i] = XtVaCreateWidget(
		    "scrollbar",
		    xmScrollBarWidgetClass,
		    cur_win->pane,
		    XmNsliderSize,	1,
		    XmNvalue,		1,
		    XmNminimum,		1,
		    XmNmaximum,		2,	/* so we don't get warning */
		    XmNorientation,	XmVERTICAL,
		    XmNtranslations,	cur_win->my_scrollbars_trans,
		    NULL);
	    XtAddCallback(cur_win->scrollbars[i],
	            XmNvalueChangedCallback, JumpProc, (XtPointer) i);
	    XtAddCallback(cur_win->scrollbars[i],
	            XmNdragCallback, JumpProc, (XtPointer) i);
	    XtAddEventHandler(
		    cur_win->scrollbars[i],
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
    i=0;
    for_each_window(wp) {
	new_height = wp->w_ntrows * cur_win->char_height;
	XtVaSetValues(cur_win->scrollbars[i],
	    XmNallowResize,		TRUE,
	    XmNheight,			new_height,
	    XmNpaneMinimum,		1,
	    XmNpaneMaximum,		1000,
	    XmNshowArrows,		wp->w_ntrows > 3 ? TRUE : FALSE,
	    NULL);
	wp->w_flag &= ~WFSBAR;
	update_scrollbar(wp);
	i++;
    }
    XtVaSetValues(cur_win->scrollbars[i],
	    XmNheight,			cur_win->char_height-1,
	    XmNallowResize,		FALSE,
	    XmNpaneMinimum,		cur_win->char_height-1,
	    XmNpaneMaximum,		cur_win->char_height-1,
	    XmNshowArrows,		FALSE,
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
	    XtAddEventHandler(
		children[num_children],
		ButtonReleaseMask,
		FALSE,
		pane_button,
		NULL);
}

#else
#if NO_WIDGETS || ATHENA_WIDGETS
static void
update_scrollbar_sizes(void)
{
    register WINDOW *wp;
    int i, newsbcnt;
    Dimension new_height;
    Cardinal nchildren;

    i=0;
    for_each_window(wp)
	i++;
    newsbcnt=i;

    /* Create any needed new scrollbars and grips */
    for (i = newsbcnt-1; i >= 0 && cur_win->scrollbars[i] == NULL; i--) {
	if (cur_win->slider_is_3D) {
	    cur_win->scrollbars[i] = XtVaCreateWidget(
			"scrollbar",
			coreWidgetClass,
			cur_win->pane,
			XtNbackgroundPixmap,	cur_win->trough_pixmap,
			XtNborderWidth,		0,
			XtNheight,		1,
			XtNwidth,		1,
			XtNtranslations,	cur_win->my_scrollbars_trans,
			NULL);
	}
	else {
	    cur_win->scrollbars[i] = XtVaCreateWidget(
			"scrollbar",
			coreWidgetClass,
			cur_win->pane,
			XtNbackground,		cur_win->scrollbar_bg,
			XtNborderWidth,		0,
			XtNheight,		1,
			XtNwidth,		1,
			XtNtranslations,	cur_win->my_scrollbars_trans,
			NULL);
	}

	XtAddEventHandler(
		cur_win->scrollbars[i],
		ExposureMask,
		FALSE,
		x_expose_scrollbar,
		(XtPointer)0);
	cur_win->scrollinfo[i].exposed = False;
    }
    for (i = newsbcnt-2; i >= 0 && cur_win->grips[i] == NULL; i--)
	cur_win->grips[i] = XtVaCreateWidget(
		    "resizeGrip",
		    coreWidgetClass,
		    cur_win->pane,
		    XtNbackground,	cur_win->modeline_bg,
		    XtNborderWidth,	0,
		    XtNheight,		1,
		    XtNwidth,		1,
		    XtNtranslations,	cur_win->my_resizeGrip_trans,
		    NULL);

    /* Set sizes and positions on scrollbars and grips */
    i=0;
    for_each_window(wp) {
	new_height = wp->w_ntrows * cur_win->char_height;
	XtVaSetValues(cur_win->scrollbars[i],
	    XtNx,		cur_win->slider_is_3D ? 0 : 1,
	    XtNy,		wp->w_toprow * cur_win->char_height,
	    XtNheight,		new_height,
	    XtNwidth,		cur_win->pane_width
	                           + (cur_win->slider_is_3D ? 2 : 0),
	    NULL);
	cur_win->scrollinfo[i].totlen = new_height;
	if (wp->w_wndp) {
	    XtVaSetValues(cur_win->grips[i],
		XtNx,		1,
		XtNy,		(wp->w_wndp->w_toprow-1) * cur_win->char_height,
		XtNheight,	cur_win->char_height,
		XtNwidth,	cur_win->pane_width,
		NULL);
	}
	i++;
    }

    if (cur_win->nscrollbars > newsbcnt) {
	nchildren = cur_win->nscrollbars - newsbcnt;
	XtUnmanageChildren(cur_win->scrollbars+newsbcnt, nchildren);
	XtUnmanageChildren(cur_win->grips+newsbcnt-1,    nchildren);
	for (i = cur_win->nscrollbars; i > newsbcnt; )
	    cur_win->scrollinfo[--i].exposed = False;
    }
    else if (cur_win->nscrollbars < newsbcnt) {
	nchildren = newsbcnt - cur_win->nscrollbars;
	XtManageChildren(cur_win->scrollbars+cur_win->nscrollbars, nchildren);
	if (cur_win->nscrollbars > 0)
	    XtManageChildren(cur_win->grips+cur_win->nscrollbars-1, nchildren);
	else if (cur_win->nscrollbars == 0 && nchildren > 1)
	    XtManageChildren(cur_win->grips, nchildren-1);
    }
    cur_win->nscrollbars = newsbcnt;

    i = 0;
    for_each_window(wp) {
	wp->w_flag &= ~WFSBAR;
	update_scrollbar(wp);
	i++;
    }

    /*
     * Set the cursors... It would be nice if we could do this in the
     * initialization above, but the widget needs to be realized for this
     * to work
     */
    for (i=0; i<cur_win->nscrollbars; i++) {
	if (XtIsRealized(cur_win->scrollbars[i]))
	    set_pointer(
		    XtWindow(cur_win->scrollbars[i]),
		    curs_sb_v_double_arrow);
	if (i < cur_win->nscrollbars-1 && XtIsRealized(cur_win->grips[i]))
	    set_pointer(
		    XtWindow(cur_win->grips[i]),
		    curs_double_arrow);
    }
}


/*
 * The X11R5 Athena scrollbar code was used as a reference for writing
 * draw_thumb and parts of update_thumb.
 */

#define FILL_TOP 0x01
#define FILL_BOT 0x02

#define SP_HT 16				/* slider pixmap height */

static void
draw_thumb(
    Widget w,
    int top,
    int bot,
    int dofill)
{
    UINT length = bot - top;
    if (bot < 0 || top < 0 || bot <= top)
	return;

    if (!dofill)
	XClearArea(XtDisplay(w), XtWindow(w), cur_win->slider_is_3D ? 2 : 1,
	           top, cur_win->pane_width-2, length, FALSE);
    else if (!cur_win->slider_is_3D)
	XFillRectangle(XtDisplay(w), XtWindow(w), cur_win->scrollbargc,
		       1, top, cur_win->pane_width-2, length);
    else {
	if (dofill & FILL_TOP) {
	    int tbot;
	    tbot = bot - ((dofill & FILL_BOT) ? 2 : 0);
	    if (tbot <= top)
		tbot = top + 1;
	    if (top + (SP_HT-2) < tbot)
		tbot = top + (SP_HT-2);
	    XCopyArea(dpy, cur_win->slider_pixmap, XtWindow(w),
		      cur_win->scrollbargc,
		      0, 0, cur_win->pane_width-2, (unsigned int) (tbot - top),
		      2, top);

	    top = tbot;
	}
	if (dofill & FILL_BOT) {
	    int btop = max(top, bot-(SP_HT-2));
	    XCopyArea(dpy, cur_win->slider_pixmap, XtWindow(w),
	              cur_win->scrollbargc,
		      0, SP_HT - (bot-btop),
		      cur_win->pane_width-2, (unsigned int) (bot - btop),
		      2, btop);
	    bot = btop;

	}
	if (top < bot) {
	    XFillRectangle(XtDisplay(w), XtWindow(w), cur_win->scrollbargc,
		           2, top,
			   cur_win->pane_width-2, (unsigned int) (bot-top));
	}
    }
}


#define MIN_THUMB_SIZE 8

static void
update_thumb(
    int barnum,
    int newtop,
    int newlen)
{
    int oldtop, oldbot, newbot, totlen;
    int f = cur_win->slider_is_3D ? 2 : 0;
    Widget w = cur_win->scrollbars[barnum];

    oldtop = cur_win->scrollinfo[barnum].top;
    oldbot = cur_win->scrollinfo[barnum].bot;
    totlen = cur_win->scrollinfo[barnum].totlen;
    newtop = min(newtop, totlen-3);
    newbot = newtop + max(newlen, MIN_THUMB_SIZE);
    newbot = min(newbot, totlen);
    cur_win->scrollinfo[barnum].top = newtop;
    cur_win->scrollinfo[barnum].bot = newbot;

    if (!cur_win->scrollinfo[barnum].exposed)
	return;

    if (XtIsRealized(w)) {
	if (newtop < oldtop) {
	    int tbot = min(newbot, oldtop+f);
	    draw_thumb(w, newtop, tbot,
	               FILL_TOP | ((tbot == newbot) ? FILL_BOT : 0));
	}
	if (newtop > oldtop) {
	    draw_thumb(w, oldtop, min(newtop, oldbot), 0);
	    if (cur_win->slider_is_3D && newtop < oldbot)
		draw_thumb(w, newtop, min(newtop+f, oldbot), FILL_TOP);
	}
	if (newbot < oldbot) {
	    draw_thumb(w, max(newbot, oldtop), oldbot, 0);
	    if (cur_win->slider_is_3D && oldtop < newbot)
		draw_thumb(w, max(newbot-f, oldtop), newbot, FILL_BOT);
	}
	if (newbot > oldbot) {
	    int btop = max(newtop, oldbot-f);
	    draw_thumb(w, btop, newbot,
	               FILL_BOT | ((btop == newtop) ? FILL_TOP : 0));
	}
    }
}

/*ARGSUSED*/
static void
x_expose_scrollbar(
    Widget	w,
    XtPointer	unused GCC_UNUSED,
    XEvent     *ev,
    Boolean    *continue_to_dispatch GCC_UNUSED)
{
    int i;

    if (ev->type != Expose)
	return;

    for (i=0; i < cur_win->nscrollbars; i++) {
	if (cur_win->scrollbars[i] == w) {
	    int top, bot;
	    top = cur_win->scrollinfo[i].top;
	    bot = cur_win->scrollinfo[i].bot;
	    cur_win->scrollinfo[i].top = -1;
	    cur_win->scrollinfo[i].bot = -1;
	    cur_win->scrollinfo[i].exposed = True;
	    update_thumb(i, top, bot-top);
	}
    }
}


static void
do_scroll(
    Widget w,
    XEvent *event,
    String *params,
    Cardinal *num_params)
{
    static enum { none, forward, backward, drag } scrollmode = none;
    int pos;
    register WINDOW *wp;
    register int i;
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
     && (XtAppPending(cur_win->app_context) & XtIMXEvent)
     && XtAppPeekEvent(cur_win->app_context, &nev)
     && (nev.type == MotionNotify || nev.type == ButtonRelease))
	return;

    if (*num_params != 1)
	return;

    /* Determine vertical position */
    switch (event->type) {
	case MotionNotify :
	    pos = event->xmotion.y;
	    break;
	case ButtonPress :
	case ButtonRelease :
	    pos = event->xbutton.y;
	    break;
	default :
	    return;
    }

    /* Determine scrollbar number and corresponding vile window */
    i = 0;
    for_each_window (wp) {
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
	case 'E' :	/* End */
	    if (cur_win->scroll_repeat_id != (XtIntervalId) 0) {
		XtRemoveTimeOut(cur_win->scroll_repeat_id);
		cur_win->scroll_repeat_id = (XtIntervalId) 0;
	    }
	    set_pointer(XtWindow(w), curs_sb_v_double_arrow);
	    scrollmode = none;
	    break;
	case 'F' :	/* Forward */
	    if (scrollmode != none)
		break;
	    count = (pos / cur_win->char_height) + 1;
	    scrollmode = forward;
	    set_pointer(XtWindow(w), curs_sb_up_arrow);
	    goto do_scroll_common;
	case 'B' :	/* Backward */
	    if (scrollmode != none)
		break;
	    count = -((pos / cur_win->char_height) + 1);
	    scrollmode = backward;
	    set_pointer(XtWindow(w), curs_sb_down_arrow);
do_scroll_common:
	    set_curwp(wp);
	    mvdnwind(TRUE, count);
	    cur_win->scroll_repeat_id = XtAppAddTimeOut(
		    cur_win->app_context,
		    cur_win->scroll_repeat_timeout,
		    repeat_scroll,
		    (XtPointer) count);
	    (void)update(TRUE);
	    break;
	case 'S' :	/* StartDrag */
	    if (scrollmode == none) {
		set_curwp(wp);
		scrollmode = drag;
		set_pointer(XtWindow(w), curs_sb_right_arrow);
	    }
	    /* FALLTHRU */
	case 'D' :	/* Drag */
	    if (scrollmode == drag) {
		int lcur = line_no(curwp->w_bufp, curwp->w_line.l);
		int ltarg = (line_count(curwp->w_bufp) * pos
			    	/ cur_win->scrollinfo[i].totlen) + 1;
		mvupwind(TRUE, lcur-ltarg);
		(void)update(TRUE);
	    }
	    break;
    }
}

/*ARGSUSED*/
static void
repeat_scroll(
    XtPointer count,
    XtIntervalId  *id GCC_UNUSED)
{
    cur_win->scroll_repeat_id = XtAppAddTimeOut(
	    cur_win->app_context,
	    cur_win->scroll_repeat_interval,
	    repeat_scroll,
	    (XtPointer) count);
    mvdnwind(TRUE, (int)count);
    (void)update(TRUE);
    XSync(dpy, False);
}

static void
resize_bar(
    Widget w,
    XEvent *event,
    String *params,
    Cardinal *num_params)
{
    static int motion_permitted = False;
    static int root_y;
    int pos = 0;			/* stifle -Wall */
    register WINDOW *wp;
    register int i;
    XEvent nev;

    /* Return immediately if behind in processing motion events */
    if (motion_permitted
     && event->type == MotionNotify
     && (XtAppPending(cur_win->app_context) & XtIMXEvent)
     && XtAppPeekEvent(cur_win->app_context, &nev)
     && (nev.type == MotionNotify || nev.type == ButtonRelease))
	return;

    if (*num_params != 1)
	return;

    switch (**params) {
	case 'S' :	/* Start */
	    motion_permitted = True;
	    root_y = event->xbutton.y_root - event->xbutton.y;
	    return;
	case 'D' :	/* Drag */
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
	case 'E' :	/* End */
	    if (!motion_permitted)
		return;
	    pos = event->xbutton.y_root - root_y;
	    motion_permitted = False;
	    break;
    }

    /* Determine grip number and corresponding vile window (above grip) */
    i = 0;
    for_each_window (wp) {
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
	(void)update(TRUE);
    }
}
#endif /* NO_WIDGETS  */
#endif /* MOTIF_WIDGETS */
#endif /* OL_WIDGETS */

void
update_scrollbar(
    WINDOW *uwp)
{
    WINDOW *wp;
    int i;
    int lnum, lcnt;

#if !NO_WIDGETS && !ATHENA_WIDGETS
    if (dont_update_sb)
	return;
#endif /* !NO_WIDGETS */

    i = 0;
    for_each_window(wp) {
	if (wp == uwp)
	    break;
	i++;
    }
    if (i >= cur_win->nscrollbars || (wp->w_flag & WFSBAR)) {
	/*
	 * update_scrollbar_sizes will recursively invoke update_scrollbar,
	 * but with WFSBAR disabled.
	 */
	update_scrollbar_sizes();
	return;
    }

    lnum = line_no(wp->w_bufp, wp->w_line.l);
    lnum = (lnum > 0) ? lnum : 1;
    lcnt = line_count(wp->w_bufp);
#if MOTIF_WIDGETS
    lcnt += 1;
    XtVaSetValues(cur_win->scrollbars[i],
	    XmNmaximum,		lcnt + wp->w_ntrows,
	    XmNsliderSize,	wp->w_ntrows,
	    XmNvalue,		lnum,
	    XmNpageIncrement,	wp->w_ntrows > 1 ? wp->w_ntrows-1 : 1,
	    NULL);
#else
#if OL_WIDGETS
    lcnt += 1;
    XtVaSetValues(cur_win->scrollbars[i],
	    XtNsliderMin,	1,
	    XtNsliderMax,	lcnt + wp->w_ntrows,
	    XtNproportionLength, wp->w_ntrows,
	    XtNsliderValue,	lnum,
	    NULL);
#else
#if NO_WIDGETS || ATHENA_WIDGETS
    {
	int top, len;
	lcnt  = max(lcnt, 1);
	len   = (min(lcnt, wp->w_ntrows) * cur_win->scrollinfo[i].totlen
				/ lcnt) + 1;
	top   = ((lnum-1) * cur_win->scrollinfo[i].totlen)
		    / lcnt;
	update_thumb(i, top, len);
    }
#endif /* NO_WIDGETS */
#endif /* OL_WIDGETS */
#endif /* MOTIF_WIDGETS */
}

#define OLD_RESOURCES 1		/* New stuff not ready for prime time */

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
#define XtCCharClass		"CharClass"
#if NO_WIDGETS || ATHENA_WIDGETS
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

#define XtNfcolor0		XtNforeground
#define XtCFcolor0		XtCForeground
#define XtNbcolor0		XtNbackground
#define XtCBcolor0		XtCBackground

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

static XtResource resources[] = {
#if NO_WIDGETS || ATHENA_WIDGETS
    {
	XtNscrollRepeatTimeout,
	XtCScrollRepeatTimeout,
	XtRInt,
	sizeof(int),
	XtOffset(TextWindow, scroll_repeat_timeout),
	XtRImmediate,
	(XtPointer) 500			/* 1/2 second */
    },
#endif	/* NO_WIDGETS */
    {
	XtNscrollRepeatInterval,
	XtCScrollRepeatInterval,
	XtRInt,
	sizeof(int),
	XtOffset(TextWindow, scroll_repeat_interval),
	XtRImmediate,
	(XtPointer) 60			/* 60 milliseconds */
    },
    {
	XtNgeometry,
	XtCGeometry,
	XtRString,
	sizeof(String *),
	XtOffset(TextWindow, geometry),
	XtRImmediate,
	"80x36"
    },
    {
	XtNfont,
	XtCFont,
	XtRString,
	sizeof(String *),
	XtOffset(TextWindow, starting_fontname),
	XtRImmediate,
	XtDefaultFont 	/* used to be FONTNAME */
    },
    {
	XtNforeground,
	XtCForeground,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, fg),
	XtRString,
#if OLD_RESOURCES
        XtDefaultForeground
#else
	"#c71bc30bc71b"
#endif /* OLD_RESOURCES */
    },
    {
	XtNbackground,
	XtCBackground,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, bg),
	XtRString,
#if OLD_RESOURCES
	XtDefaultBackground
#else
	"#c71bc30bc71b"
#endif
    },
    {
	XtNforkOnStartup,
	XtCForkOnStartup,
	XtRBool,
	sizeof(Bool),
	XtOffset(TextWindow, fork_on_startup),
	XtRImmediate,
	(XtPointer) False
    },
    {
	XtNfocusFollowsMouse,
	XtCFocusFollowsMouse,
	XtRBool,
	sizeof(Bool),
	XtOffset(TextWindow, focus_follows_mouse),
	XtRImmediate,
	(XtPointer) False
    },
    {
	XtNmultiClickTime,
	XtCMultiClickTime,
	XtRInt,
	sizeof(Time),
	XtOffset(TextWindow, click_timeout),
	XtRImmediate,
	(XtPointer) 500
    },
    {
	XtNcharClass,
	XtCCharClass,
	XtRString,
	sizeof(String *),
	XtOffset(TextWindow, multi_click_char_class),
	XtRImmediate,
	NULL
    },
    {
	XtNscrollbarOnLeft,
	XtCScrollbarOnLeft,
	XtRBool,
	sizeof(Bool),
	XtOffset(TextWindow, scrollbar_on_left),
	XtRImmediate,
	(XtPointer) False
    },
    {
	XtNscrollbarWidth,
	XtCScrollbarWidth,
	XtRInt,
	sizeof(int),
	XtOffset(TextWindow, pane_width),
	XtRImmediate,
	(XtPointer) PANE_WIDTH_DEFAULT
    },
#if OPT_MENUS
    {
	XtNmenuHeight,
	XtCMenuHeight,
	XtRInt,
	sizeof(int),
	XtOffset(TextWindow, menu_height),
	XtRImmediate,
	(XtPointer) MENU_HEIGHT_DEFAULT
    },
#endif
    {
	XtNpersistentSelections,
	XtCPersistentSelections,
	XtRBool,
	sizeof(Bool),
	XtOffset(TextWindow, persistent_selections),
	XtRImmediate,
	(XtPointer) True
    },
    {
	XtNselectionSetsDOT,
	XtCSelectionSetsDOT,
	XtRBool,
	sizeof(Bool),
	XtOffset(TextWindow, selection_sets_DOT),
	XtRImmediate,
	(XtPointer) False
    },
    {
	XtNblinkInterval,
	XtCBlinkInterval,
	XtRInt,
	sizeof(int),
	XtOffset(TextWindow, blink_interval),
	XtRImmediate,
	(XtPointer) -666		/* 2/3 second; only when highlighted */
    },
};

static XtResource color_resources[] = {
    {
	XtNfcolor0,
	XtCFcolor0,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[0]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolor0,
	XtCBcolor0,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[0]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNfcolor1,
	XtCFcolor1,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[1]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolor1,
	XtCBcolor1,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[1]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNfcolor2,
	XtCFcolor2,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[2]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolor2,
	XtCBcolor2,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[2]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNfcolor3,
	XtCFcolor3,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[3]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolor3,
	XtCBcolor3,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[3]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNfcolor4,
	XtCFcolor4,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[4]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolor4,
	XtCBcolor4,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[4]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNfcolor5,
	XtCFcolor5,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[5]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolor5,
	XtCBcolor5,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[5]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNfcolor6,
	XtCFcolor6,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[6]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolor6,
	XtCBcolor6,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[6]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNfcolor7,
	XtCFcolor7,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[7]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolor7,
	XtCBcolor7,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[7]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNfcolor8,
	XtCFcolor8,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[8]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolor8,
	XtCBcolor8,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[8]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNfcolor9,
	XtCFcolor9,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[9]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolor9,
	XtCBcolor9,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[9]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNfcolorA,
	XtCFcolorA,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[10]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolorA,
	XtCBcolorA,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[10]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNfcolorB,
	XtCFcolorB,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[11]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolorB,
	XtCBcolorB,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[11]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNfcolorC,
	XtCFcolorC,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[12]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolorC,
	XtCBcolorC,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[12]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNfcolorD,
	XtCFcolorD,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[13]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolorD,
	XtCBcolorD,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[13]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNfcolorE,
	XtCFcolorE,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[14]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolorE,
	XtCBcolorE,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[14]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNfcolorF,
	XtCFcolorF,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_fg[15]),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbcolorF,
	XtCBcolorF,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, colors_bg[15]),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
};

#if NO_WIDGETS || ATHENA_WIDGETS
static XtResource scrollbar_resources[] = {
    {
	XtNforeground,
	XtCForeground,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, scrollbar_fg),
#if OLD_RESOURCES
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
#else
	XtRString,
	"#b6dab2cab6da"
#endif
    },
    {
	XtNbackground,
	XtCBackground,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, scrollbar_bg),
#if OLD_RESOURCES
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
#else
	XtRString,
	"#9e7996589e79"
#endif /* OLD_RESOURCES */
    },
    {
	XtNsliderIsSolid,
	XtCSliderIsSolid,
	XtRBool,
	sizeof(Bool),
	XtOffset(TextWindow, slider_is_solid),
#if OLD_RESOURCES
	XtRImmediate,
	(XtPointer) False
#else
	XtRBool,
	(XtPointer) &cur_win_rec.slider_is_solid
#endif /* OLD_RESOURCES */
    },
};
#endif

static XtResource modeline_resources[] = {
    {
	XtNforeground,
	XtCForeground,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, modeline_fg),
#if OLD_RESOURCES
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
#else
	XtRString,
	"#ffffffffffff"
#endif /* OLD_RESOURCES */
    },
    {
	XtNbackground,
	XtCBackground,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, modeline_bg),
#if OLD_RESOURCES
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
#else
	XtRString,
	"#70006ca37000"
#endif /* OLD_RESOURCES */
    },
    {
	XtNfocusForeground,
	XtCForeground,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, modeline_focus_fg),
#if OLD_RESOURCES
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
#else
	XtRString,
	"#ffffffffffff"
#endif /* OLD_RESOURCES */
    },
    {
	XtNfocusBackground,
	XtCBackground,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, modeline_focus_bg),
#if OLD_RESOURCES
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
#else
	XtRString,
	"#70006ca37000"
#endif /* OLD_RESOURCES */
    },
};

static XtResource selection_resources[] = {
    {
	XtNforeground,
	XtCBackground,				/* weird, huh? */
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, selection_fg),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
    {
	XtNbackground,
	XtCForeground,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, selection_bg),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
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

static XtResource cursor_resources[] = {
    {
	XtNforeground,
	XtCForeground,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, cursor_fg),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbackground,
	XtCBackground,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, cursor_bg),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
};

static XtResource pointer_resources[] = {
    {
	XtNforeground,
	XtCForeground,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, pointer_fg),
	XtRPixel,
	(XtPointer) &cur_win_rec.fg
    },
    {
	XtNbackground,
	XtCBackground,
	XtRPixel,
	sizeof(Pixel),
	XtOffset(TextWindow, pointer_bg),
	XtRPixel,
	(XtPointer) &cur_win_rec.bg
    },
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

static void my_error_handler(String message)
{
    fprintf(stderr, "%s: %s\n", prog_arg, message);
    print_usage();
}

/* ARGSUSED */
void
x_preparse_args(
    int        *pargc,
    char     ***pargv)
{
    XFontStruct *pfont;
    XGCValues   gcvals;
    ULONG	gcmask;
    int		geo_mask, startx, starty, screen_depth;
    int		i;
    Cardinal	start_cols, start_rows;
    static XrmOptionDescRec options[] = {
	{"-t",   	(char *)0,          XrmoptionSkipArg,	(caddr_t)0 },
	{"-fork",   	"*forkOnStartup",   XrmoptionNoArg,	"true" },
	{"-leftbar",	"*scrollbarOnLeft", XrmoptionNoArg,	"true" },
	{"-rightbar",	"*scrollbarOnLeft", XrmoptionNoArg,	"false" },
    };
#if MOTIF_WIDGETS || OL_WIDGETS
    static XtActionsRec new_actions[] = {
	{ "ConfigureBar", configure_bar }
    };
    static String scrollbars_translations =
	"#override \n\
		Ctrl<Btn1Down>:ConfigureBar(Split) \n\
		Ctrl<Btn2Down>:ConfigureBar(Kill) \n\
		Ctrl<Btn3Down>:ConfigureBar(Only)";
    static String fallback_resources[]= {
	"*scrollPane.background:grey80",
	"*scrollbar.background:grey60",
	NULL
    };
#else
#if NO_WIDGETS || ATHENA_WIDGETS
    static XtActionsRec new_actions[] = {
	{ "ConfigureBar", configure_bar },
	{ "DoScroll", do_scroll },
	{ "ResizeBar", resize_bar }
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
    static String fallback_resources[]= {
	NULL
    };
    static char stippled_pixmap_bits[] = { '\002', '\001' };
#endif /* NO_WIDGETS */
#endif /* MOTIF_WIDGETS || OL_WIDGETS */

#if OL_WIDGETS
    /* There is a cryptic statement in the poor documentation that I have
     * on OpenLook that OlToolkitInitialize is now preferred to the older
     * OlInitialize (which is used in the examples and is documented better).
     * The documentation I have says that OlToolkitInitialize returns a
     * widget.  I don't believe it, nor do I understand what kind of widget
     * it would be.  I get the impression that it takes one argument, but
     * I don't know what that argument is supposed to be.
     */
    (void) OlToolkitInitialize( NULL );
#endif /* OL_WIDGETS */

    XtSetErrorHandler(my_error_handler);
    cur_win->top_widget = XtVaAppInitialize(
	    &cur_win->app_context,
	    MY_CLASS,
	    options, XtNumber(options),
	    pargc, *pargv,
	    fallback_resources,
	    XtNgeometry,	NULL,
	    XtNinput,		TRUE,
	    NULL);
    XtSetErrorHandler((XtErrorHandler)0);
    dpy = XtDisplay(cur_win->top_widget);

#if 0
    check_visuals();
#endif

    XtVaGetValues(cur_win->top_widget,
		XtNdepth, &screen_depth,
		NULL);

#if !OLD_RESOURCES
    cur_win->slider_is_solid = (screen_depth >= 6);
#endif

    XtGetApplicationResources(
	    cur_win->top_widget,
	    (XtPointer)cur_win,
	    resources,
	    XtNumber(resources),
	    (ArgList)0,
	    0);

    if (cur_win->bg == cur_win->fg)
	cur_win->fg = BlackPixel(dpy,DefaultScreen(dpy));
    if (cur_win->bg == cur_win->fg)
	cur_win->bg = WhitePixel(dpy,DefaultScreen(dpy));


#if NO_WIDGETS || ATHENA_WIDGETS
    XtGetSubresources(
	    cur_win->top_widget,
	    (XtPointer)cur_win,
	    "scrollbar",
	    "Scrollbar",
	    scrollbar_resources,
	    XtNumber(scrollbar_resources),
	    (ArgList)0,
	    0);
#endif	/* NO_WIDGETS */

    XtGetSubresources(
	    cur_win->top_widget,
	    (XtPointer)cur_win,
	    "modeline",
	    "Modeline",
	    modeline_resources,
	    XtNumber(modeline_resources),
	    (ArgList)0,
	    0);

    XtGetSubresources(
	    cur_win->top_widget,
	    (XtPointer)cur_win,
	    "selection",
	    "Selection",
	    selection_resources,
	    XtNumber(selection_resources),
	    (ArgList)0,
	    0);

    XtGetSubresources(
	    cur_win->top_widget,
	    (XtPointer)cur_win,
	    "cursor",
	    "Cursor",
	    cursor_resources,
	    XtNumber(cursor_resources),
	    (ArgList)0,
	    0);

    XtGetSubresources(
	    cur_win->top_widget,
	    (XtPointer)cur_win,
	    "pointer",
	    "Pointer",
	    pointer_resources,
	    XtNumber(pointer_resources),
	    (ArgList)0,
	    0);

    XtGetSubresources(
	    cur_win->top_widget,
	    (XtPointer)cur_win,
	    "color",
	    "Color",
	    color_resources,
	    XtNumber(color_resources),
	    (ArgList)0,
	    0);

    /* Initialize atoms needed for getting a fully specified font name */
    atom_FONT 		= XInternAtom(dpy, "FONT", False);
    atom_FOUNDRY	= XInternAtom(dpy, "FOUNDRY", False);
    atom_WEIGHT_NAME	= XInternAtom(dpy, "WEIGHT_NAME", False);
    atom_SLANT		= XInternAtom(dpy, "SLANT", False);
    atom_SETWIDTH_NAME	= XInternAtom(dpy, "SETWIDTH_NAME", False);
    atom_PIXEL_SIZE	= XInternAtom(dpy, "PIXEL_SIZE", False);
    atom_RESOLUTION_X	= XInternAtom(dpy, "RESOLUTION_X", False);
    atom_RESOLUTION_Y	= XInternAtom(dpy, "RESOLUTION_Y", False);
    atom_SPACING	= XInternAtom(dpy, "SPACING", False);
    atom_AVERAGE_WIDTH	= XInternAtom(dpy, "AVERAGE_WIDTH", False);
    atom_CHARSET_REGISTRY = XInternAtom(dpy, "CHARSET_REGISTRY", False);
    atom_CHARSET_ENCODING = XInternAtom(dpy, "CHARSET_ENCODING", False);

    pfont = query_font(cur_win, cur_win->starting_fontname);
    if (!pfont) {
	pfont = query_font(cur_win, FONTNAME);
	if (!pfont) {
	    (void)fprintf(stderr, "couldn't get font \"%s\" or \"%s\", exiting\n",
		    cur_win->starting_fontname, FONTNAME);
	    ExitProgram(BADEXIT);
	}
    }
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
	    gp++;			/* skip over width and height */
	if (*gp)
	    XtVaSetValues(cur_win->top_widget,
			XtNgeometry,	gp,
			NULL);
    }

    /* Sanity check values obtained from XtGetApplicationResources */
    CHECK_MIN_MAX(cur_win->pane_width, PANE_WIDTH_MIN, PANE_WIDTH_MAX);


#if MOTIF_WIDGETS
    cur_win->form_widget = XtVaCreateManagedWidget(
	    "form",
	    xmFormWidgetClass,
	    cur_win->top_widget,
	    NULL);
#else
#if OL_WIDGETS
    cur_win->form_widget = XtVaCreateManagedWidget(
	    "form",
	    formWidgetClass,
	    cur_win->top_widget,
	    NULL);
#else
#if ATHENA_WIDGETS && OPT_MENUS
    cur_win->pane_widget = XtVaCreateManagedWidget(
	    "pane",
	    panedWidgetClass,
	    cur_win->top_widget,
	    NULL);
    cur_win->menu_widget = XtVaCreateManagedWidget(
	    "menubar",
	    boxWidgetClass,
	    cur_win->pane_widget,
	    XtNshowGrip,		False,
	    NULL);
    cur_win->form_widget = XtVaCreateManagedWidget(
	    "form",
	    bbWidgetClass,
	    cur_win->pane_widget,
	    XtNwidth,			x_width(cur_win)
	    					+ cur_win->pane_width + 2,
	    XtNheight,			x_height(cur_win),
	    XtNbackground,		cur_win->bg,
	    XtNbottom,			XtChainBottom,
	    XtNleft,			XtChainLeft,
	    XtNright,			XtChainRight,
	    XtNfromVert,		cur_win->menu_widget,
	    XtNvertDistance,		0,
	    NULL);
#else
#if NO_WIDGETS || ATHENA_WIDGETS
    cur_win->form_widget = XtVaCreateManagedWidget(
	    "form",
	    bbWidgetClass,
	    cur_win->top_widget,
	    XtNwidth,			x_width(cur_win)
	    					+ cur_win->pane_width + 2,
	    XtNheight,			x_height(cur_win),
	    XtNbackground,		cur_win->bg,
	    NULL);
#endif /* NO_WIDGETS */
#endif /* ATHENA_WIDGETS */
#endif /* OL_WIDGETS */
#endif /* MOTIF_WIDGETS */

#if OPT_MENUS
#if ATHENA_WIDGETS
	do_menu ( cur_win->menu_widget );
#endif
#if MOTIF_WIDGETS
	menub = XmCreateMenuBar (cur_win->form_widget, "menub", NULL, 0);

	XtVaSetValues (menub,
			XmNtopAttachment,	XmATTACH_FORM,
			XmNleftAttachment,	XmATTACH_FORM,
			XmNbottomAttachment,	XmATTACH_NONE,
			XmNrightAttachment,	XmATTACH_FORM,
			NULL);
	XtManageChild (menub);
	do_menu ( menub );
#endif
#endif

    cur_win->screen = XtVaCreateManagedWidget(
	    "screen",
#if MOTIF_WIDGETS
	    xmPrimitiveWidgetClass,
#else
	    coreWidgetClass,
#endif
	    cur_win->form_widget,
	    XtNwidth,			x_width(cur_win),
	    XtNheight,			x_height(cur_win),
	    XtNborderWidth, 		0,
	    XtNbackground,		cur_win->bg,
#if MOTIF_WIDGETS
	    XmNresizable,		TRUE,
	    XmNbottomAttachment,	XmATTACH_FORM,
#if OPT_MENUS
	    XmNtopAttachment,		XmATTACH_WIDGET,
	    XmNtopWidget,		menub,
	    XmNtopOffset,		2,
#else
	    XmNtopAttachment,		XmATTACH_FORM,
#endif
	    XmNleftAttachment,		XmATTACH_FORM,
	    XmNrightAttachment,		XmATTACH_NONE,
#else
#if OL_WIDGETS
	    XtNyAttachBottom,		TRUE,
	    XtNyVaryOffset,		FALSE,
	    XtNxAddWidth,		TRUE,
	    XtNyAddHeight,		TRUE,
#else
#if NO_WIDGETS || ATHENA_WIDGETS
	    XtNx,			cur_win->scrollbar_on_left
					    ? cur_win->pane_width+2
					    : 0,
	    XtNy,			0,
#endif	/* NO_WIDGETS */
#endif	/* OL_WIDGETS */
#endif	/* MOTIF_WIDGETS */
	    NULL);

    /* Initialize graphics context for display of normal and reverse text */
    gcmask = GCForeground | GCBackground | GCFont | GCGraphicsExposures;
    gcvals.foreground = cur_win->fg;
    gcvals.background = cur_win->bg;
    gcvals.font = cur_win->pfont->fid;
    gcvals.graphics_exposures = False;
    cur_win->textgc = XCreateGC(dpy,
            DefaultRootWindow(dpy),
	    gcmask, &gcvals);
    cur_win->exposed    = FALSE;
    cur_win->visibility = VisibilityUnobscured;

    gcvals.foreground = cur_win->bg;
    gcvals.background = cur_win->fg;
    gcvals.font = cur_win->pfont->fid;
    cur_win->reversegc = XCreateGC(dpy,
            DefaultRootWindow(dpy),
	    gcmask, &gcvals);

    for (i = 0; i < NCOLORS; i++) {
	if ( screen_depth == 1
	  || cur_win->colors_fg[i] == cur_win->colors_bg[i]
	  || ( cur_win->colors_fg[i] == cur_win->fg
	    && cur_win->colors_bg[i] == cur_win->bg )) {
	    /* Reuse the standard GCs if possible */
	    cur_win->colors_fgc[i] = cur_win->textgc;
	    cur_win->colors_bgc[i] = cur_win->reversegc;
	} else {
	    gcvals.foreground = cur_win->colors_fg[i];
	    gcvals.background = cur_win->colors_bg[i];
	    cur_win->colors_fgc[i] = XCreateGC(dpy,
		                               DefaultRootWindow(dpy),
		                               gcmask, &gcvals);
	    gcvals.foreground = cur_win->colors_bg[i];
	    gcvals.background = cur_win->colors_fg[i];
	    cur_win->colors_bgc[i] = XCreateGC(dpy,
		                               DefaultRootWindow(dpy),
		                               gcmask, &gcvals);
	}
    }

    /* Initialize graphics context for display of selections */
    if (screen_depth == 1
     || cur_win->selection_bg == cur_win->selection_fg
     ||  (cur_win->fg == cur_win->selection_fg
       && cur_win->bg == cur_win->selection_bg)
     ||  (cur_win->fg == cur_win->selection_bg
       && cur_win->bg == cur_win->selection_fg)) {
	cur_win->selgc = cur_win->reversegc;
	cur_win->revselgc = cur_win->textgc;
    }
    else {
	gcvals.foreground = cur_win->selection_fg;
	gcvals.background = cur_win->selection_bg;
	cur_win->selgc = XCreateGC(dpy,
		DefaultRootWindow(dpy),
		gcmask, &gcvals);
	gcvals.foreground = cur_win->selection_bg;
	gcvals.background = cur_win->selection_fg;
	cur_win->revselgc = XCreateGC(dpy,
		DefaultRootWindow(dpy),
		gcmask, &gcvals);
    }

    /*
     * Initialize graphics context for display of normal modelines.
     * Portions of the modeline are never displayed in reverse video (wrt
     * the modeline) so there is no corresponding reverse video gc.
     */
    if (screen_depth == 1
     || cur_win->modeline_bg == cur_win->modeline_fg
     ||  (cur_win->fg == cur_win->modeline_fg
       && cur_win->bg == cur_win->modeline_bg)
     ||  (cur_win->fg == cur_win->modeline_bg
       && cur_win->bg == cur_win->modeline_fg)) {
	cur_win->modeline_gc = cur_win->reversegc;
    }
    else {
	gcvals.foreground = cur_win->modeline_fg;
	gcvals.background = cur_win->modeline_bg;
	cur_win->modeline_gc = XCreateGC(dpy,
		                         DefaultRootWindow(dpy),
		                         gcmask, &gcvals);
    }

    /*
     * Initialize graphics context for display of modelines which indicate
     * that the corresponding window has focus.
     */
    if (screen_depth == 1
     || cur_win->modeline_focus_bg == cur_win->modeline_focus_fg
     ||  (cur_win->fg == cur_win->modeline_focus_fg
       && cur_win->bg == cur_win->modeline_focus_bg)
     ||  (cur_win->fg == cur_win->modeline_focus_bg
       && cur_win->bg == cur_win->modeline_focus_fg)) {
	cur_win->modeline_focus_gc = cur_win->reversegc;
    }
    else {
	gcvals.foreground = cur_win->modeline_focus_fg;
	gcvals.background = cur_win->modeline_focus_bg;
	cur_win->modeline_focus_gc = XCreateGC(dpy,
		                               DefaultRootWindow(dpy),
		                               gcmask, &gcvals);
    }

    /* Initialize cursor graphics context and flag which indicates how to
     * display cursor.
     */
    if (screen_depth == 1
     || cur_win->cursor_bg == cur_win->cursor_fg
     ||  (cur_win->fg == cur_win->cursor_fg
       && cur_win->bg == cur_win->cursor_bg)
     ||  (cur_win->fg == cur_win->cursor_bg
       && cur_win->bg == cur_win->cursor_fg)) {
	cur_win->is_color_cursor = FALSE;
	cur_win->cursor_fg = cur_win->bg;		/* undo our trickery */
	cur_win->cursor_bg = cur_win->fg;
	cur_win->cursgc = cur_win->reversegc;
	cur_win->revcursgc = cur_win->textgc;
    }
    else {
	cur_win->is_color_cursor = TRUE;
	gcvals.foreground = cur_win->cursor_fg;
	gcvals.background = cur_win->cursor_bg;
	cur_win->cursgc = XCreateGC(dpy,
		DefaultRootWindow(dpy),
		gcmask, &gcvals);
	gcvals.foreground = cur_win->cursor_bg;
	gcvals.background = cur_win->cursor_fg;
	cur_win->revcursgc = XCreateGC(dpy,
		DefaultRootWindow(dpy),
		gcmask, &gcvals);
    }

#if NO_WIDGETS || ATHENA_WIDGETS

    if (cur_win->scrollbar_bg == cur_win->scrollbar_fg) {
	cur_win->scrollbar_bg = cur_win->bg;
	cur_win->scrollbar_fg = cur_win->fg;
    }
    if (screen_depth == 1 || too_light_or_too_dark(cur_win->scrollbar_fg))
	cur_win->slider_is_solid = False;
    gcvals.background = cur_win->scrollbar_bg;
    if (!cur_win->slider_is_solid) {
	gcmask = GCFillStyle | GCStipple | GCForeground | GCBackground;
	gcvals.foreground = cur_win->scrollbar_fg;
	gcvals.fill_style = FillOpaqueStippled;
	gcvals.stipple = XCreatePixmapFromBitmapData(dpy,
		DefaultRootWindow(dpy),
		stippled_pixmap_bits,
		2, 2,
		1L, 0L,
		1);
    }
    else {
	gcmask = GCForeground | GCBackground;
	gcvals.foreground = cur_win->scrollbar_fg;
    }
    gcmask |= GCGraphicsExposures;
    gcvals.graphics_exposures = False;
    cur_win->scrollbargc = XCreateGC(dpy,
	    DefaultRootWindow(dpy),
	    gcmask, &gcvals);

    if (screen_depth >= 6 && cur_win->slider_is_solid) {
	Pixel fg_light, fg_dark, bg_light, bg_dark;
	if ( alloc_shadows(cur_win->scrollbar_fg, &fg_light, &fg_dark)
	  && alloc_shadows(cur_win->scrollbar_bg, &bg_light, &bg_dark)) {
	    GC gc;
	    Pixmap slider_pixmap;
	    cur_win->slider_is_3D = True;

	    cur_win->trough_pixmap = XCreatePixmap(dpy, DefaultRootWindow(dpy),
		    cur_win->pane_width+2, 16, (unsigned int)screen_depth);

#define TROUGH_HT 16
	    gcvals.foreground = cur_win->scrollbar_bg;
	    gc = XCreateGC(dpy, DefaultRootWindow(dpy), gcmask, &gcvals);
	    XFillRectangle(dpy, cur_win->trough_pixmap, gc, 0,0,
		    cur_win->pane_width+2, TROUGH_HT);
	    XSetForeground(dpy, gc, bg_dark);
	    XFillRectangle(dpy, cur_win->trough_pixmap, gc, 0,0, 2, TROUGH_HT);
	    XSetForeground(dpy, gc, bg_light);
	    XFillRectangle(dpy, cur_win->trough_pixmap, gc,
		    (int) cur_win->pane_width, 0, 2, TROUGH_HT);

	    slider_pixmap = XCreatePixmap(dpy, DefaultRootWindow(dpy),
		    cur_win->pane_width-2, SP_HT, (unsigned int)screen_depth);

	    XSetForeground(dpy, gc, cur_win->scrollbar_fg);
	    XFillRectangle(dpy, slider_pixmap, gc, 0,0,
		    cur_win->pane_width-2, SP_HT);
	    XSetForeground(dpy, gc, fg_light);
	    XFillRectangle(dpy, slider_pixmap, gc, 0,0, 2, SP_HT);
	    XSetForeground(dpy, gc, fg_dark);
	    XFillRectangle(dpy, slider_pixmap, gc,
		    (int) cur_win->pane_width-4, 0, 2, SP_HT);

	    XSetTile(dpy, cur_win->scrollbargc, slider_pixmap);
	    XSetFillStyle(dpy, cur_win->scrollbargc, FillTiled);
	    XSetTSOrigin(dpy, cur_win->scrollbargc, 2, 0);

	    cur_win->slider_pixmap = XCreatePixmap(dpy, DefaultRootWindow(dpy),
		    cur_win->pane_width-2, SP_HT, (unsigned int)screen_depth);
	    XCopyArea(dpy, slider_pixmap, cur_win->slider_pixmap, gc,
	              0, 0, cur_win->pane_width-2, SP_HT, 0, 0);

	    /* Draw top bevel */
	    XSetForeground(dpy, gc, fg_light);
	    XDrawLine(dpy, cur_win->slider_pixmap, gc,
	              0, 0, (int)cur_win->pane_width-3, 0);
	    XDrawLine(dpy, cur_win->slider_pixmap, gc,
	              0, 1, (int)cur_win->pane_width-4, 1);

	    /* Draw bottom bevel */
	    XSetForeground(dpy, gc, fg_dark);
	    XDrawLine(dpy, cur_win->slider_pixmap, gc,
	              2, SP_HT-2, (int)cur_win->pane_width-3, SP_HT-2);
	    XDrawLine(dpy, cur_win->slider_pixmap, gc,
	              1, SP_HT-1, (int)cur_win->pane_width-3, SP_HT-1);

	    XFreeGC(dpy, gc);
	}
    }
#endif /* NO_WIDGETS */

    XtAppAddActions(cur_win->app_context, new_actions, XtNumber(new_actions));
    cur_win->my_scrollbars_trans = XtParseTranslationTable(scrollbars_translations);

#if MOTIF_WIDGETS
    cur_win->pane = XtVaCreateManagedWidget(
	    "scrollPane",
	    xmPanedWindowWidgetClass,
	    cur_win->form_widget,
	    XtNwidth,			cur_win->pane_width,
	    XmNbottomAttachment,	XmATTACH_FORM,
#if OPT_MENUS
	    XmNtopAttachment,		XmATTACH_WIDGET,
	    XmNtopWidget,		menub,
	    XmNtopOffset,		3,
#else
	    XmNtopAttachment,		XmATTACH_FORM,
#endif
	    XmNleftAttachment,		XmATTACH_WIDGET,
	    XmNleftWidget,		cur_win->screen,
	    XmNrightAttachment,		XmATTACH_FORM,
	    XmNspacing,			cur_win->char_height,
	    XmNsashIndent,		2,
	    XmNsashWidth,		cur_win->pane_width - 4,
	    XmNmarginHeight,		0,
	    XmNmarginWidth,		0,
	    XmNseparatorOn,		FALSE,
	    NULL);
#else
#if OL_WIDGETS
    cur_win->pane = XtVaCreateManagedWidget(
	    "scrollPane",
	    bulletinBoardWidgetClass,
	    cur_win->form_widget,
	    XtNwidth,			cur_win->pane_width,
	    XtNheight,			x_height(cur_win),
	    XtNxRefWidget,		cur_win->screen,
	    XtNyAttachBottom,		TRUE,
	    XtNyVaryOffset,		FALSE,
	    XtNxAddWidth,		TRUE,
	    XtNyAddHeight,		TRUE,
	    XtNlayout,			OL_IGNORE,
	    NULL);
#else
#if NO_WIDGETS || ATHENA_WIDGETS
    cur_win->my_resizeGrip_trans = XtParseTranslationTable(resizeGrip_translations);
    cur_win->pane = XtVaCreateManagedWidget(
	    "scrollPane",
	    bbWidgetClass,
	    cur_win->form_widget,
	    XtNwidth,			cur_win->pane_width + 2,
	    XtNheight,			x_height(cur_win)
	    					- cur_win->char_height,
	    XtNx,			cur_win->scrollbar_on_left
					    ? 0
					    : x_width(cur_win),
	    XtNy,			0,
	    XtNborderWidth,		0,
	    XtNbackground,		cur_win->modeline_bg,
	    NULL);
    curs_sb_v_double_arrow = XCreateFontCursor(dpy, XC_sb_v_double_arrow);
    curs_sb_up_arrow       = XCreateFontCursor(dpy, XC_sb_up_arrow);
    curs_sb_down_arrow     = XCreateFontCursor(dpy, XC_sb_down_arrow);
    curs_sb_left_arrow     = XCreateFontCursor(dpy, XC_sb_left_arrow);
    curs_sb_right_arrow    = XCreateFontCursor(dpy, XC_sb_right_arrow);
    curs_double_arrow      = XCreateFontCursor(dpy, XC_double_arrow);
#endif	/* NO_WIDGETS */
#endif	/* OL_WIDGETS */
#endif	/* MOTIF_WIDGETS */

#if NO_WIDGETS || ATHENA_WIDGETS
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
	    XmNleftAttachment,	XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_WIDGET,
	    XmNrightWidget,	cur_win->screen,
	    NULL);
	XtVaSetValues(cur_win->screen,
	    XmNleftAttachment,	XmATTACH_NONE,
	    XmNrightAttachment,	XmATTACH_FORM,
	    NULL);
#else	/* !MOTIF_WIDGETS */
# if OL_WIDGETS
	XtVaSetValues(cur_win->pane,
	    XtNxRefWidget,	cur_win->form_widget,
	    NULL);
	XtVaSetValues(cur_win->screen,
	    XtNxRefWidget,	cur_win->pane,
	    NULL);
# else
	/* EMPTY */;
# endif	/* OL_WIDGETS */
#endif	/* !MOTIF_WIDGETS */
    }

    XtAddEventHandler(
	    cur_win->screen,
	    KeyPressMask,
	    FALSE,
	    x_key_press,
	    (XtPointer)0);

    XtAddEventHandler(
	    cur_win->screen,
	    (EventMask) (ButtonPressMask | ButtonReleaseMask
	    	       | (cur_win->focus_follows_mouse ? PointerMotionMask
		                                       : ButtonMotionMask)
	    	       | ExposureMask | VisibilityChangeMask),
	    TRUE,
	    x_process_event,
	    (XtPointer)0);

    XtAddEventHandler(
	    cur_win->top_widget,
	    StructureNotifyMask,
	    FALSE,
	    x_configure_window,
	    (XtPointer)0);

    XtAddEventHandler(
	    cur_win->top_widget,
	    EnterWindowMask | LeaveWindowMask | FocusChangeMask,
	    FALSE,
	    x_change_focus,
	    (XtPointer)0);

    cur_win->base_width = -1;	/* force base width to be set when configured */
    XtRealizeWidget(cur_win->top_widget);

    /* We can't test this until after the widget's realized */
    if (cur_win->fork_on_startup)
	(void) newprocessgroup(TRUE,1);

    cur_win->win = XtWindow(cur_win->screen);

    /* We wish to participate in the "delete window" protocol */
    atom_WM_PROTOCOLS = XInternAtom(dpy, "WM_PROTOCOLS", False);
    atom_WM_DELETE_WINDOW = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    {
	Atom atoms[2];
	i = 0;
	atoms[i++] = atom_WM_DELETE_WINDOW;
	XSetWMProtocols(dpy,
		XtWindow(cur_win->top_widget),
		atoms,
		i);
    }
    XtAddEventHandler(
	    cur_win->top_widget,
	    NoEventMask,
	    TRUE,
	    x_wm_delwin,
	    (XtPointer)0);

    /* Atoms needed for selections */
    atom_TARGETS	= XInternAtom(dpy, "TARGETS",   False);
    atom_MULTIPLE	= XInternAtom(dpy, "MULTIPLE",  False);
    atom_TIMESTAMP	= XInternAtom(dpy, "TIMESTAMP", False);
    atom_TEXT		= XInternAtom(dpy, "TEXT",      False);

    set_pointer(XtWindow(cur_win->screen), cur_win->normal_pointer);
}

#if 0
static void
check_visuals(void)
{
    static char *classes[] = { "StaticGray", "GrayScale", "StaticColor", "PseudoColor", "TrueColor", "DirectColor" };
    XVisualInfo *visuals, visual_template;
    int nvisuals;
    visuals = XGetVisualInfo(dpy, VisualNoMask, &visual_template, &nvisuals);
    if (visuals != NULL) {
	int i;
	for (i=0; i<nvisuals; i++) {
	    printf("Class: %s, Depth: %d\n",
	           classes[visuals[i].class], visuals[i].depth);
	}
	XFree(visuals);
    }
}
#endif


#if NO_WIDGETS || ATHENA_WIDGETS
static Boolean
too_light_or_too_dark(
    Pixel pixel)
{
    XColor color;
    Colormap colormap;

    XtVaGetValues(cur_win->screen,
    	XtNcolormap,	&colormap,
	NULL);

    color.pixel = pixel;
    XQueryColor(dpy, colormap, &color);

    return (color.red > 0xfff0 && color.green > 0xfff0 && color.blue > 0xfff0)
        || (color.red < 0x0020 && color.green < 0x0020 && color.blue < 0x0020);
}

static Boolean
alloc_shadows(
    Pixel pixel,
    Pixel *light,
    Pixel *dark)
{
    XColor color;
    Colormap colormap;
    unsigned long lred, lgreen, lblue, dred, dgreen, dblue;

    XtVaGetValues(cur_win->screen,
    	XtNcolormap,	&colormap,
	NULL);

    color.pixel = pixel;
    XQueryColor(dpy, colormap, &color);

    if ( (color.red > 0xfff0 && color.green > 0xfff0 && color.blue > 0xfff0)
      || (color.red < 0x0020 && color.green < 0x0020 && color.blue < 0x0020))
	return False;			/* It'll look awful! */

#define MAXINTENS ((unsigned long)65535L)
#define PlusFortyPercent(v) ((7 * (long) (v)) / 5)
    lred   = PlusFortyPercent(color.red);
    lred   = min(lred, MAXINTENS);
    lred   = max(lred, (color.red + MAXINTENS)/2);

    lgreen = PlusFortyPercent(color.green);
    lgreen = min(lgreen, MAXINTENS);
    lgreen = max(lgreen, (color.green + MAXINTENS)/2);

    lblue  = PlusFortyPercent(color.blue);
    lblue  = min(lblue, MAXINTENS);
    lblue  = max(lblue, (color.blue + MAXINTENS)/2);

#define MinusFortyPercent(v) ((3 * (long) (v)) / 5)

    dred   = MinusFortyPercent(color.red);
    dgreen = MinusFortyPercent(color.green);
    dblue  = MinusFortyPercent(color.blue);

    color.red = (unsigned short) lred;
    color.green = (unsigned short) lgreen;
    color.blue = (unsigned short) lblue;

    if (!XAllocColor(dpy, colormap, &color))
	return False;

    *light = color.pixel;

    color.red = (unsigned short) dred;
    color.green = (unsigned short) dgreen;
    color.blue = (unsigned short) dblue;

    if (!XAllocColor(dpy, colormap, &color))
	return False;

    *dark = color.pixel;

    return True;
}
#endif

char *
x_current_fontname(void)
{
    return cur_win->fontname;
}

static char *
x_get_font_atom_property(
    XFontStruct *pf,
    Atom atom)
{
    XFontProp *pp;
    int i;
    char *retval = NULL;

    for (i=0, pp = pf->properties; i < pf->n_properties; i++, pp++)
	if (pp->name == atom) {
	    retval = XGetAtomName(dpy, pp->card32);
	    break;
	}
    return retval;
}

static XFontStruct *
query_font(
	TextWindow tw,
	const char *fname)
{
    XFontStruct *pf;

    if ((pf = XLoadQueryFont(dpy, fname)) != 0) {
	char *fullname = NULL;

	if (pf->max_bounds.width != pf->min_bounds.width) {
	    (void)fprintf(stderr,
			  "proportional font, things will be miserable\n");
	}

	/*
	 * Free resources assoicated with any presently loaded fonts.
	 */
	if (tw->pfont)
	    XFreeFont(dpy, tw->pfont);
	if (tw->pfont_bold) {
	    XFreeFont(dpy, tw->pfont_bold);
	    tw->pfont_bold = NULL;
	}
	if (tw->pfont_ital) {
	    XFreeFont(dpy, tw->pfont_ital);
	    tw->pfont_ital = NULL;
	}
	if (tw->pfont_boldital) {
	    XFreeFont(dpy, tw->pfont_boldital);
	    tw->pfont_boldital = NULL;
	}
	tw->fsrch_flags = 0;

	tw->pfont = pf;
	tw->char_width  = pf->max_bounds.width;
	tw->char_height = pf->ascent + pf->descent;
	tw->char_ascent = pf->ascent;
	tw->char_descent = pf->descent;
	tw->left_ink	= (pf->min_bounds.lbearing < 0);
	tw->right_ink	= (pf->max_bounds.rbearing > tw->char_width);

	FreeIfNeeded(cur_win->fontname);
	if ((fullname = x_get_font_atom_property(pf, atom_FONT)) != NULL
	 && fullname[0] == '-') {
	    /*
	     * Good. Not much work to do; the name was available via the FONT
	     * property.
	     */
	    tw->fontname = strmalloc(fullname);
	    XFree(fullname);
	}
	else {
	    /*
	     * Woops, fully qualified name not available from the FONT property.
	     * Attempt to get the full name piece by piece.  Ugh!
	     */
	    char str[1024], *s;
	    if (fullname != NULL)
		XFree(fullname);

	    s = str;
	    *s++ = '-';

#define GET_ATOM_OR_STAR(atom)					\
    do {							\
	char *as;						\
	if ((as = x_get_font_atom_property(pf, (atom))) != NULL) { \
	    char *asp = as;					\
	    while ((*s++ = *asp++))				\
		;						\
	    *(s-1) = '-';					\
	    XFree(as);						\
	}							\
	else {							\
	    *s++ = '*';						\
	    *s++ = '-';						\
	}							\
    } one_time
#define GET_ATOM_OR_GIVEUP(atom)				\
    do {							\
	char *as;						\
	if ((as = x_get_font_atom_property(pf, (atom))) != NULL) { \
	    char *asp = as;					\
	    while ((*s++ = *asp++))				\
		;						\
	    *(s-1) = '-';					\
	    XFree(as);						\
	}							\
	else							\
	    goto piecemeal_done;				\
    } one_time
#define GET_LONG_OR_GIVEUP(atom)				\
    do {							\
	unsigned long val;					\
	if (XGetFontProperty(pf, (atom), &val)) {		\
	    sprintf(s,"%ld",val);				\
	    while (*s++ != '\0')				\
		;						\
	    *(s-1) = '-';					\
	}							\
	else							\
	    goto piecemeal_done;				\
    } one_time

	    GET_ATOM_OR_STAR(atom_FOUNDRY);
	    GET_ATOM_OR_GIVEUP(XA_FAMILY_NAME);
	    GET_ATOM_OR_GIVEUP(atom_WEIGHT_NAME);
	    GET_ATOM_OR_GIVEUP(atom_SLANT);
	    GET_ATOM_OR_GIVEUP(atom_SETWIDTH_NAME);
	    *s++ = '*';				/* ADD_STYLE_NAME */
	    *s++ = '-';
	    GET_LONG_OR_GIVEUP(atom_PIXEL_SIZE);
	    GET_LONG_OR_GIVEUP(XA_POINT_SIZE);
	    GET_LONG_OR_GIVEUP(atom_RESOLUTION_X);
	    GET_LONG_OR_GIVEUP(atom_RESOLUTION_Y);
	    GET_ATOM_OR_GIVEUP(atom_SPACING);
	    GET_LONG_OR_GIVEUP(atom_AVERAGE_WIDTH);
	    GET_ATOM_OR_STAR(atom_CHARSET_REGISTRY);
	    GET_ATOM_OR_STAR(atom_CHARSET_ENCODING);
	    *(s-1) = '\0';

#undef GET_ATOM_OR_STAR
#undef GET_ATOM_OR_GIVEUP
#undef GET_LONG_OR_GIVEUP

	    fname = str;
piecemeal_done:
	    /*
	     * We will either use the name which was built up piecemeal or
	     * the name which was originally passed to us to assign to
	     * the fontname field.  We prefer the fully qualified name
	     * so that we can later search for bold and italic fonts.
	     */
	    tw->fontname = strmalloc(fname);
	}
    }
    return pf;
}

static XFontStruct *
alternate_font(
    char *weight,
    char *slant)
{
    char *newname, *np, *op;
    int cnt;
    XFontStruct *fsp = NULL;
    if (cur_win->fontname == NULL
     || cur_win->fontname[0] != '-'
     || (newname = castalloc(char, (SIZE_T)strlen(cur_win->fontname)+32)) == NULL)
	return NULL;

    /* copy initial two fields */
    for (cnt=3, np=newname, op=cur_win->fontname; *op && cnt > 0; ) {
	if (*op == '-')
	    cnt--;
	*np++ = *op++;
    }
    if (!*op)
	goto done;

    /* substitute new weight and slant as appropriate */
#define SUBST_FIELD(field) 				\
    do {						\
	if ((field) != NULL) {				\
	    char *fp = (field);				\
	    if (nocase_eq(*fp, *op))			\
		goto done;				\
	    while ((*np++ = *fp++))			\
		;					\
	    *(np-1) = '-';				\
	    while (*op && *op++ != '-')			\
		;					\
	}						\
	else {						\
	    while (*op && (*np++ = *op++) != '-')	\
		;					\
	}						\
	if (!*op)					\
	    goto done;					\
    } one_time

    SUBST_FIELD(weight);
    SUBST_FIELD(slant);
#undef SUBST_FIELD

    /* copy rest of name */
    while ((*np++ = *op++))
	;
    if ((fsp = XLoadQueryFont(dpy, newname)) != NULL) {
	cur_win->left_ink = cur_win->left_ink || (fsp->min_bounds.lbearing < 0);
	cur_win->right_ink = cur_win->right_ink
		    || (fsp->max_bounds.rbearing > cur_win->char_width);
    }

done:
    free(newname);
    return fsp;

}

#if OPT_MENUS
int
x_menu_height(void)
{
	return cur_win->menu_height;
}
#endif

int
x_setfont(
    const char  *fname)
{
    XFontStruct *pfont;
    int         oldw,
                oldh;

    if (cur_win) {
	oldw = x_width(cur_win);
	oldh = x_height(cur_win);
	if ((pfont = query_font(cur_win, fname)) != 0) {
	    int i;

	    XSetFont(dpy, cur_win->textgc, pfont->fid);
	    XSetFont(dpy, cur_win->reversegc, pfont->fid);
	    XSetFont(dpy, cur_win->selgc, pfont->fid);
	    XSetFont(dpy, cur_win->revselgc, pfont->fid);
	    XSetFont(dpy, cur_win->cursgc, pfont->fid);
	    XSetFont(dpy, cur_win->revcursgc, pfont->fid);
	    XSetFont(dpy, cur_win->modeline_focus_gc, pfont->fid);
	    XSetFont(dpy, cur_win->modeline_gc, pfont->fid);
	    if (cur_win->textgc != cur_win->revselgc) {
		XSetFont(dpy, cur_win->selgc, pfont->fid);
		XSetFont(dpy, cur_win->revselgc, pfont->fid);
	    }
	    for (i = 0; i < NCOLORS; i++) {
		if (cur_win->colors_fgc[i] != cur_win->textgc) {
		    XSetFont(dpy, cur_win->colors_fgc[i], pfont->fid);
		    XSetFont(dpy, cur_win->colors_bgc[i], pfont->fid);
		}
	    }

	    /* if size changed, resize it, otherwise refresh */
	    if (oldw != x_width(cur_win) || oldh != x_height(cur_win)) {
		XtVaSetValues(cur_win->top_widget,
			XtNminHeight,	cur_win->base_height
					    + MINROWS*cur_win->char_height,
			XtNminWidth,	cur_win->base_width
					    + MINCOLS*cur_win->char_width,
			XtNheightInc,	cur_win->char_height,
			XtNwidthInc,	cur_win->char_width,
			NULL);
		update_scrollbar_sizes();
		XClearWindow(dpy, cur_win->win);
		x_touch(cur_win, 0, 0, cur_win->cols, cur_win->rows);
		XResizeWindow(dpy, XtWindow(cur_win->top_widget),
			      x_width(cur_win) + cur_win->base_width,
			      x_height(cur_win) + cur_win->base_height);

	    } else {
		XClearWindow(dpy, cur_win->win);
		x_touch(cur_win, 0, 0, cur_win->cols, cur_win->rows);
		x_flush();
	    }

	    return 1;
	}
	return 0;
    }
    return 1;
}

static
/* ARGSUSED */
SIGT x_quit (int ACTUAL_SIG_ARGS GCC_UNUSED)
{
    x_close();
    ExitProgram(GOODEXIT);
    /* NOTREACHED */
    SIGRET;
}

static void
x_open(void)
{
    kqinit(cur_win);
    cur_win->scrollbars = NULL;
#if NO_WIDGETS || ATHENA_WIDGETS
    cur_win->scrollinfo = NULL;
    cur_win->grips = NULL;
#endif
#if OL_WIDGETS
    cur_win->sliders = NULL;
#endif

    setup_handler(SIGHUP, x_quit);
    setup_handler(SIGINT, catchintr);
    setup_handler(SIGTERM, x_quit);

    /* main code assumes that it can access a cell at nrow x ncol */
    term.t_mcol = term.t_ncol = cur_win->cols;
    term.t_mrow = term.t_nrow = cur_win->rows;

    if (check_scrollbar_allocs() != TRUE)
	ExitProgram(BADEXIT);
}

static void
x_close(void)
{
    /* FIXME: Free pixmaps and GCs !!! */

    XtDestroyWidget(cur_win->top_widget);
}

static void
x_kopen(void)
{
}

static void
x_kclose(void)
{
}

static void
x_touch(
    TextWindow tw,
    int	 sc,
    int	 sr,
    UINT ec,
    UINT er)
{
    register UINT r;
    register UINT c;

    if (er > tw->rows)
	er = tw->rows;
    if (ec > tw->cols)
	ec = tw->cols;

    for (r = sr; r < er; r++) {
	MARK_LINE_DIRTY(r);
	for (c = sc; c < ec; c++)
	    if (CELL_TEXT(r,c) != ' ' || CELL_ATTR(r,c))
		MARK_CELL_DIRTY(r,c);
    }
}


static void
wait_for_scroll(
    TextWindow  tw)
{
    XEvent      ev;
    int         sc,
                sr;
    unsigned    ec,
                er;
    XGraphicsExposeEvent *gev;

    for_ever {		/* loop looking for a gfx expose or no expose */
	if (XCheckTypedEvent(dpy, NoExpose, &ev))
	    return;
	if (XCheckTypedEvent(dpy, GraphicsExpose, &ev)) {
	    gev = (XGraphicsExposeEvent *) & ev;
	    sc = gev->x / tw->char_width;
	    sr = gev->y / tw->char_height;
	    ec = CEIL(gev->x + gev->width,  tw->char_width);
	    er = CEIL(gev->y + gev->height, tw->char_height);
	    x_touch(tw, sc, sr, ec, er);
	    if (gev->count == 0)
		return;
	}
	XSync(dpy, False);
    }
}


static void
x_scroll(
    int from,
    int to,
    int count)
{
    if (cur_win->visibility == VisibilityFullyObscured)
	return;			/* Why bother? */

    if (from == to)
	return;			/* shouldn't happen */

    XCopyArea(dpy, cur_win->win, cur_win->win, cur_win->textgc,
	      x_pos(cur_win, 0), y_pos(cur_win, from),
	      x_width(cur_win), (unsigned)(count * cur_win->char_height),
	      x_pos(cur_win, 0), y_pos(cur_win, to));
    if (from < to)
	XClearArea(dpy, cur_win->win,
        	x_pos(cur_win, 0), y_pos(cur_win,from),
		x_width(cur_win), (unsigned)((to-from) * cur_win->char_height),
		FALSE);
    else
	XClearArea(dpy, cur_win->win,
        	x_pos(cur_win, 0), y_pos(cur_win,to+count),
		x_width(cur_win), (unsigned)((from-to) * cur_win->char_height),
		FALSE);
    if (cur_win->visibility == VisibilityPartiallyObscured) {
	XFlush(dpy);
	wait_for_scroll(cur_win);
    }
}

/*
 * The X protocol request for clearing a rectangle (PolyFillRectangle) takes
 * 20 bytes.  It will therefore be more expensive to switch from drawing text
 * to filling a rectangle unless the area to be cleared is bigger than 20
 * spaces.  Actually it is worse than this if we are going to switch
 * immediately to drawing text again since we incur a certain overhead
 * (16 bytes) for each string to be displayed.  This is how the value of
 * CLEAR_THRESH was computed (36 = 20+16).
 *
 * Kev's opinion:  If XDrawImageString is to be called, it is hardly ever
 * worth it to call XFillRectangle.  The only time where it will be a big
 * win is when the entire area to update is all spaces (in which case
 * XDrawImageString will not be called).  The following code would be much
 * cleaner, simpler, and easier to maintain if we were to just call
 * XDrawImageString where there are non-spaces to be written and
 * XFillRectangle when the entire region is to be cleared.
 */
#define	CLEAR_THRESH	36

static void
flush_line(
    char  *text,
    int	   len,
    unsigned int attr,
    int	   sr,
    int	   sc)
{
    GC	fore_gc;
    GC	back_gc;
    int	fore_yy = text_y_pos(cur_win, sr);
    int	back_yy = y_pos(cur_win, sr);
    char *p;
    int   cc, tlen, i, startcol;
    int   fontchanged = FALSE;

    if (attr == 0) {	/* This is the most common case, so we list it first */
	fore_gc = cur_win->textgc;
	back_gc = cur_win->reversegc;
    }
    else if ((attr & VACURS) && cur_win->is_color_cursor) {
	fore_gc = cur_win->cursgc;
	back_gc = cur_win->revcursgc;
	attr &= ~VACURS;
    }
    else if (attr & VASEL) {
	fore_gc = cur_win->selgc;
	back_gc = cur_win->revselgc;
    }
    else if (attr & VAMLFOC)
	fore_gc = back_gc = cur_win->modeline_focus_gc;
    else if (attr & VAML)
	fore_gc = back_gc = cur_win->modeline_gc;
    else if (attr & (VACOLOR)) {
	fore_gc = cur_win->colors_fgc[VCOLORNUM(attr)];
	back_gc = cur_win->colors_bgc[VCOLORNUM(attr)];
    }
    else {
	fore_gc = cur_win->textgc;
	back_gc = cur_win->reversegc;
    }

    if (attr & (VAREV | VACURS)) {
	GC tmp_gc = fore_gc;
	fore_gc = back_gc;
	back_gc = tmp_gc;
    }

    if (attr & (VABOLD | VAITAL)) {
	XFontStruct *fsp = NULL;
	if ((attr & (VABOLD | VAITAL)) == (VABOLD | VAITAL)) {
	    if (!(cur_win->fsrch_flags & FSRCH_BOLDITAL)) {
		if ((fsp = alternate_font("bold","i")) != NULL
	         || (fsp = alternate_font("bold","o")) != NULL)
		    cur_win->pfont_boldital = fsp;
		cur_win->fsrch_flags |= FSRCH_BOLDITAL;
	    }
	    if (cur_win->pfont_boldital != NULL) {
		XSetFont(dpy, fore_gc, cur_win->pfont_boldital->fid);
		fontchanged = TRUE;
		attr &= ~(VABOLD | VAITAL);	/* don't use fallback */
	    }
	    else
		goto tryital;
	}
	else if (attr & VAITAL) {
tryital:
	    if (!(cur_win->fsrch_flags & FSRCH_ITAL)) {
		if ((fsp = alternate_font((char *)0,"i")) != NULL
	         || (fsp = alternate_font((char *)0,"o")) != NULL)
		    cur_win->pfont_ital = fsp;
		cur_win->fsrch_flags |= FSRCH_ITAL;
	    }
	    if (cur_win->pfont_ital != NULL) {
		XSetFont(dpy, fore_gc, cur_win->pfont_ital->fid);
		fontchanged = TRUE;
		attr &= ~VAITAL;		/* don't use fallback */
	    }
	    else if (attr & VABOLD)
		goto trybold;
	}
	else if (attr & VABOLD) {
trybold:
	    if (!(cur_win->fsrch_flags & FSRCH_BOLD)) {
		cur_win->pfont_bold = alternate_font("bold",NULL);
		cur_win->fsrch_flags |= FSRCH_BOLD;
	    }
	    if (cur_win->pfont_bold != NULL) {
		XSetFont(dpy, fore_gc, cur_win->pfont_bold->fid);
		fontchanged = TRUE;
		attr &= ~VABOLD;		/* don't use fallback */
	    }
	}
    }

    /* break line into TextStrings and FillRects */
    p = (char *)text;
    cc = 0;
    tlen = 0;
    startcol = sc;
    for (i = 0; i < len; i++) {
	if (text[i] == ' ') {
	    cc++;
	    tlen++;
	} else {
	    if (cc >= CLEAR_THRESH) {
		tlen -= cc;
		XDrawImageString(dpy, cur_win->win, fore_gc,
				 (int)x_pos(cur_win, sc), fore_yy,
				 p, tlen);
		if (attr & VABOLD)
		    XDrawString(dpy, cur_win->win, fore_gc,
				(int)x_pos(cur_win, sc)+1, fore_yy,
				 p, tlen);
		p += tlen + cc;
		sc += tlen;
		XFillRectangle(dpy, cur_win->win, back_gc,
			       x_pos(cur_win, sc), back_yy,
			       (unsigned)(cc * cur_win->char_width),
			       (unsigned)(cur_win->char_height));
		sc += cc;
		tlen = 1;	/* starting new run */
	    } else
		tlen++;
	    cc = 0;
	}
    }
    if (cc >= CLEAR_THRESH) {
	tlen -= cc;
	XDrawImageString(dpy, cur_win->win, fore_gc,
			 x_pos(cur_win, sc), fore_yy,
			 p, tlen);
	if (attr & VABOLD)
	    XDrawString(dpy, cur_win->win, fore_gc,
			(int)x_pos(cur_win, sc)+1, fore_yy,
			 p, tlen);
	sc += tlen;
	XFillRectangle(dpy, cur_win->win, back_gc,
		       x_pos(cur_win, sc), back_yy,
		       (unsigned)(cc * cur_win->char_width),
		       (unsigned)(cur_win->char_height));
    } else if (tlen > 0) {
	XDrawImageString(dpy, cur_win->win, fore_gc,
			 x_pos(cur_win, sc), fore_yy,
			 p, tlen);
	if (attr & VABOLD)
	    XDrawString(dpy, cur_win->win, fore_gc,
			(int)x_pos(cur_win, sc)+1, fore_yy,
			 p, tlen);
    }
    if (attr & (VAUL | VAITAL)) {
	fore_yy += cur_win->char_descent - 1;
	XDrawLine(dpy, cur_win->win, fore_gc,
	          x_pos(cur_win, startcol), fore_yy,
		  x_pos(cur_win, startcol + len) - 1, fore_yy);
    }

    if (fontchanged)
	XSetFont(dpy, fore_gc, cur_win->pfont->fid);
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
	return;		/* Why bother? */

    /*
     * Write out cursor _before_ rest of the screen in order to avoid
     * flickering / winking effect noticable on some display servers.  This
     * means that the old cursor position (if different from the current
     * one) will be cleared after the new cursor is displayed.
     */

    if (ttrow >=0 && ttrow < term.t_nrow && ttcol >= 0 && ttcol < term.t_ncol
     && !cur_win->wipe_permitted) {
	CLEAR_CELL_DIRTY(ttrow, ttcol);
	display_cursor((XtPointer) 0, (XtIntervalId *) 0);
    }

    /* sometimes we're the last to know about resizing...*/
    if (cur_win->rows > term.t_mrow)
    	cur_win->rows = term.t_mrow;

    for (r = 0; r < cur_win->rows; r++) {
	if (!IS_DIRTY_LINE(r))
	    continue;
	if (r !=  ttrow)
	    CLEAR_LINE_DIRTY(r);

	/*
	 * The following code will cause monospaced fonts with ink outside
	 * the bounding box to be cleaned up.
	 */
	if (cur_win->left_ink || cur_win->right_ink)
	    for (c=0; c < term.t_ncol; ) {
		while (c < term.t_ncol && !IS_DIRTY(r,c))
		    c++;
		if (c >= term.t_ncol)
		    break;
		if (cur_win->left_ink && c > 0)
		    MARK_CELL_DIRTY(r,c-1);
		while (c < term.t_ncol && IS_DIRTY(r,c))
		    c++;
		if (cur_win->right_ink && c < term.t_ncol) {
		    MARK_CELL_DIRTY(r,c);
		    c++;
		}
	    }

	c = 0;
	while (c < term.t_ncol) {
	    /* Find the beginning of the next dirty sequence */
	    while (c < term.t_ncol && !IS_DIRTY(r,c))
		c++;
	    if (c >= term.t_ncol)
		break;
	    if (r == ttrow && c == ttcol && !cur_win->wipe_permitted) {
		c++;
		continue;
	    }
	    CLEAR_CELL_DIRTY(r,c);
	    sc = ec = c;
	    attr = VATTRIB(CELL_ATTR(r,c));
	    cleanlen = NONDIRTY_THRESH;
	    c++;
	    /*
	     * Scan until we find the end of line, a cell with a different
	     * attribute, a sequence of NONDIRTY_THRESH non-dirty chars, or
	     * the cursor position.
	     */
	    while (c < term.t_ncol) {
		if (attr != VATTRIB(CELL_ATTR(r,c)))
		    break;
		else if (r == ttrow && c == ttcol && !cur_win->wipe_permitted) {
		    c++;
		    break;
		}
		else if (IS_DIRTY(r,c)) {
		    ec = c;
		    cleanlen = NONDIRTY_THRESH;
		    CLEAR_CELL_DIRTY(r,c);
		}
		else if (--cleanlen <= 0)
		    break;
		c++;
	    }
	    /* write out the portion from sc thru ec */
	    flush_line(&CELL_TEXT(r,sc), ec-sc+1,
	               (unsigned int) VATTRIB(CELL_ATTR(r,sc)), r, sc);
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
 *  	- exceptions
 */
static int  charClass[256] = {
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

static int
set_character_class_range(
    register int low,
    register int high,		/* in range of [0..255] */
    register int value)		/* arbitrary */
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
set_character_class(register char *s)
{
    register int i;		/* iterator, index into s */
    int         len;		/* length of s */
    int         acc;		/* accumulator */
    int         low,
                high;		/* bounds of range [0..127] */
    int         base;		/* 8, 10, 16 (octal, decimal, hex) */
    int         numbers;	/* count of numbers per range */
    int         digits;		/* count of digits in a number */
    static char *errfmt = "xvile:  %s in range string \"%s\" (position %d)\n";

    if (!s || !s[0])
	return -1;

    base = 10;			/* in case we ever add octal, hex */
    low = high = -1;		/* out of range */

    for (i = 0, len = strlen(s), acc = 0, numbers = digits = 0;
	    i < len; i++) {
	int        c = s[i];

	if (isspace(c)) {
	    continue;
	} else if (isdigit(c)) {
	    acc = acc * base + (c - '0');
	    digits++;
	    continue;
	} else if (c == '-') {
	    low = acc;
	    acc = 0;
	    if (digits == 0) {
		(void)fprintf(stderr, errfmt, "missing number", s, i);
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
		(void)fprintf(stderr, errfmt, "too many numbers",
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
		(void)fprintf(stderr, errfmt, "bad value number",
			s, i);
	    } else if (set_character_class_range(low, high, acc) != 0) {
		(void)fprintf(stderr, errfmt, "bad range", s, i);
	    }
	    low = high = -1;
	    acc = 0;
	    digits = 0;
	    numbers = 0;
	    continue;
	} else {
	    (void)fprintf(stderr, errfmt, "bad character", s, i);
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
	(void)fprintf(stderr, errfmt, "bad value number", s, i);
    } else if (set_character_class_range(low, high, acc) != 0) {
	(void)fprintf(stderr, errfmt, "bad range", s, i);
    }
    return (0);
}

/*
 * Copy a single character into the paste-buffer, quoting it if necessary
 */
static int
add2paste(
TBUFF	**p,
int	c)
{
	if (c == '\n' || isblank(c))
		/*EMPTY*/;
	else if (isspecial(c) || (c == '\r') || !isprint(c))
	 	(void)tb_append(p, quotec);
	return (tb_append(p, c) != 0);
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
copy_paste(
TBUFF	**p,
char	*value,
SIZE_T	length)
{
	WINDOW	*wp = row2window(ttrow);
	BUFFER	*bp = (wp != NULL) ? wp->w_bufp : NULL;
	int	status;

	if (bp != NULL && b_val(bp,MDVIEW))
		return FALSE;

	status = TRUE;

	if (bp != NULL && (b_val(bp,MDCMOD) || b_val(bp,MDAIND))) {

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
			LINE	*lp	= MK.l;
			int	first   = firstchar(lp);
			int	last    = lastchar(lp);
			CMDFUNC	*f = NULL;

			/* If the line contains only a single nonwhite,
			 * we will insert before it.
			 */
			if (first >= MK.o)
				f = &f_openup_no_aindent;
			else if (last <= MK.o)
				f = &f_opendown_no_aindent;
			if (insertmode) {
				if ((*value != '\n') && MK.o == 0)
					(void)tb_append(p, '\n');
			}
			else if (f) {
			    	char *pstr;
				/* we're _replacing_ the default
					insertion command, so reinit */
				tb_init(p, abortc);
				pstr = fnc2pstr(f);
				tb_bappend(p, pstr + 1, (ALLOC_T) *pstr);
			}
#endif
		}
	}

	while (length-- > 0) {
		if (!add2paste(p, *value++)) {
			status = FALSE;
			break;
		}
	}

	return status;
}

/* ARGSUSED */
static void
x_get_selection(
    Widget         w GCC_UNUSED,
    XtPointer      cldat GCC_UNUSED,
    Atom          *selection GCC_UNUSED,
    Atom          *type,
    XtPointer      value,
    unsigned long *length,
    int           *format)
{
    int	do_ins;

    if (*format != 8 || *type != XA_STRING) {
	x_beep();			/* can't handle incoming data */
	return;
    }

    if (length != 0 && value != NULL) {
	char *s = NULL;			/* stifle warning */
	/* should be impossible to hit this with existing paste */
	/* XXX massive hack -- leave out 'i' if in prompt line */
	do_ins = !insertmode
		&& !onMsgRow(cur_win)
		&& ((s = fnc2pstr(&f_insert_no_aindent)) != NULL);

	if (tb_init(&PasteBuf, abortc)) {
		if ((do_ins && !tb_bappend(&PasteBuf, s+1, (ALLOC_T)*s))
		 || !copy_paste(&PasteBuf, (char *)value, (SIZE_T) *length)
		 || (do_ins && !tb_append(&PasteBuf, abortc)))
			tb_free(&PasteBuf);
	}
	XtFree(value);
    }
}

static void
x_paste_selection(void)
{
    if (cur_win->have_selection) {
	/* local transfer */
	UCHAR  *data;
	SIZE_T len_st;
	unsigned long len_ul;

	Atom	selection = XA_PRIMARY;
	Atom	type      = XA_STRING;
	int	format    = 8;

	if (!x_get_selected_text(&data, &len_st)) {
	    x_beep();
	    return;
	}
	len_ul = (unsigned long) len_st;	/* Ugh. */
	x_get_selection(cur_win->top_widget, NULL, &selection, &type,
	                (XtPointer) data, &len_ul, &format);
    }
    else {
	XtGetSelectionValue(
	    cur_win->top_widget,
	    XA_PRIMARY,
	    XA_STRING,
	    x_get_selection,
	    (XtPointer)0,		/* client data */
	    XtLastTimestampProcessed(dpy));
    }
}

static Boolean
x_get_selected_text(
    UCHAR **datp,
    SIZE_T *lenp)
{
    UCHAR	*data;
    UCHAR	*dp;
    SIZE_T	length;
    KILL	*kp;		/* pointer into kill register */

    /* FIXME: Can't select message line */

    if (!cur_win->have_selection)
	return False;

    sel_yank(SEL_KREG);
    for (length = 0, kp = kbs[SEL_KREG].kbufh; kp; kp = kp->d_next)
	length += KbSize(SEL_KREG, kp);
    if ( length == 0
      || (dp = data = (UCHAR *) XtMalloc(length * sizeof(UCHAR))) == 0
      || (kp = kbs[SEL_KREG].kbufh) == 0)
	return False;

    while (kp != NULL) {
	SIZE_T len = KbSize(SEL_KREG,kp);
	(void)memcpy((char *)dp, (char *)kp->d_chunk, len);
	kp = kp->d_next;
	dp += len;
    }

    *lenp = length;
    *datp = data;
    return True;
}

/* ARGSUSED */
static Boolean
x_convert_selection(
    Widget	   w GCC_UNUSED,
    Atom          *selection GCC_UNUSED,
    Atom          *target,
    Atom          *type,
    XtPointer     *value,
    unsigned long *length,
    int           *format)
{
    if (!cur_win->have_selection)
	return False;

    /*
     * The ICCCM requires us to handle the following targets: TARGETS,
     * MULTIPLE, and TIMESTAMP.  MULTIPLE and TIMESTAMP are handled by
     * the Xt intrinsics.  Below, we handle TARGETS, STRING, and TEXT.
     * The STRING and TEXT targets are what xvile uses to transfer
     * selected text to another client.  TARGETS is simply a list of
     * the targets we support (including the ones handled by the Xt
     * intrinsics).
     */

    if (*target == atom_TARGETS) {
	Atom *tp;

#define NTARGS 5

	*(Atom **)value = tp = (Atom *)XtMalloc(NTARGS * sizeof(Atom));

	if (tp == NULL)
	    return False;	/* should not happen (even if out of memory) */

	*tp++ = atom_TARGETS;
	*tp++ = atom_MULTIPLE;
	*tp++ = atom_TIMESTAMP;
	*tp++ = XA_STRING;
	*tp++ = atom_TEXT;

	*type   = XA_ATOM;
	*length = tp - *(Atom **)value;
	*format = 32;		/* width of the data being transfered */
	return True;
    }
    else if (*target == XA_STRING || *target == atom_TEXT) {
	*type   = XA_STRING;
	*format = 8;
	return x_get_selected_text((UCHAR **)value, (SIZE_T *)length);
    }

    return False;
}

/* ARGSUSED */
static void
x_lose_selection(
    Widget w GCC_UNUSED,
    Atom  *selection GCC_UNUSED)
{
    cur_win->have_selection = False;
    cur_win->was_on_msgline = False;
    sel_release();
    (void) update(TRUE);
}

void
own_selection(void)
{
    if (!cur_win->have_selection)
	XtOwnSelection(
	    cur_win->top_widget,
	    XA_PRIMARY,
	    XtLastTimestampProcessed(dpy),
	    x_convert_selection,
	    x_lose_selection,
	    (XtSelectionDoneProc)0);
    cur_win->have_selection = True;
}

static void
scroll_selection(
    XtPointer rowcol,
    XtIntervalId *idp)
{
    int row, col;
    if (*idp == cur_win->sel_scroll_id)
	XtRemoveTimeOut(cur_win->sel_scroll_id);	/* shouldn't happen */
    cur_win->sel_scroll_id = (XtIntervalId) 0;

    row = (((long) rowcol) >> 16) & 0xffff;
    col = ((long) rowcol) & 0xffff;
    if (row & 0x8000)
	row |= -1 << 16;
    if (col & 0x8000)
	col |= -1 << 16;
    extend_selection(cur_win, row, col, TRUE);
}

static int
line_count_and_interval(
    long scroll_count,
    unsigned long *ip)
{
    scroll_count = scroll_count / 4 - 2;
    if (scroll_count <= 0) {
	*ip = (1 - scroll_count) * cur_win->scroll_repeat_interval;
	return 1;
    }
    else {
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
	*ip = cur_win->scroll_repeat_interval;
	return scroll_count;
    }
}

static void
extend_selection(
    TextWindow tw GCC_UNUSED,
    int	nr,
    int	nc,
    Bool wipe)
{
    static long scroll_count = 0;
    long rowcol = 0;
    unsigned long interval = 0;

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
	}
	else {
	    scroll_count = 0;
	}
	nr = curwp->w_toprow;
    }
    else if (nr >= mode_row(curwp)) {
	if (wipe) {
	    mvdnwind(TRUE, line_count_and_interval(scroll_count++, &interval));
	    rowcol = (nr << 16) | (nc & 0xffff);
	}
	else {
	    scroll_count = 0;
	}
	nr = mode_row(curwp) - 1;
    }
    else {
	scroll_count = 0;
    }
    if (setcursor(nr,nc) && sel_extend(wipe,TRUE)) {
	cur_win->did_select = True;
	(void)update(TRUE);
	if (scroll_count > 0) {
	    x_flush();
	    cur_win->sel_scroll_id = XtAppAddTimeOut(cur_win->app_context,
	                                             interval,
						     scroll_selection,
						     (XtPointer) rowcol);
	}
    }
    else
	x_beep();
}

static void
multi_click(
	TextWindow  tw,
	int         nr,
	int         nc)
{
    UCHAR	*p;
    int	cclass;
    int	sc = nc;
    int oc = nc;
    WINDOW	*wp;

    tw->numclicks++;

    if ((wp = row2window(nr)) != 0 && nr == mode_row(wp)) {
	set_curwp(wp);
	sel_release();
	(void)update(TRUE);
    }
    else {
	switch (tw->numclicks) {
	case 0:
	case 1:			/* shouldn't happen */
		mlwrite("BUG: 0 or 1 multiclick value.");
		return;
	case 2:			/* word */
		/* find word start */
		p = (UCHAR *)(&CELL_TEXT(nr,sc));
		cclass = charClass[*p];
		do {
			--sc;
			--p;
		} while (sc >= 0 && charClass[*p] == cclass);
		sc++;
		/* and end */
		p = (UCHAR *)(&CELL_TEXT(nr,nc));
		cclass = charClass[*p];
		do {
			++nc;
			++p;
		} while (nc < tw->cols && charClass[*p] == cclass);
		--nc;
		break;
	case 3:			/* line (doesn't include trailing newline) */
		sc = 0;
		/* nc = tw->cols; */
		nc = HUGE;
		break;
	case 4:			/* screen */
		/* XXX blow off till we can figure out where screen starts
		 * and ends */
	default:
		/*
		 * This provides a mechanism for getting rid of the
		 * selection.
		 */
		sel_release();
		(void)update(TRUE);
		return;
	}
	if (setcursor(nr,sc)) {
	    (void)sel_begin();
	    extend_selection(tw, nr, nc, FALSE);
	    (void) setcursor(nr,oc);
	    /* FIXME: Too many updates */
	    (void) update(TRUE);
	}
    }
}

static void
start_selection(
    TextWindow  tw,
    XButtonPressedEvent *ev,
    int	nr,
    int	nc)
{
    tw->wipe_permitted = FALSE;
    if ((tw->lasttime != 0)
     && (absol(ev->time - tw->lasttime) < tw->click_timeout)) {
	/* FIXME: This code used to ignore multiple clicks which
	 *	  spanned rows.  Do we still want this behavior?
	 *	  If so, we'll have to (re)implement it.
	 */
	multi_click(tw, nr, nc);
    }
    else {
	WINDOW *wp;

	beginDisplay();

	tw->lasttime = ev->time;
	tw->numclicks = 1;
	tw->was_on_msgline = onMsgRow(tw);

	if ((wp = row2window(nr)) != 0) {
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
	    /*EMPTY*/;	/* ignore */
	}
	else if (wp != 0 && nr == mode_row(wp)) {
	    (void)update(TRUE);
	}
	else if (setcursor(nr, nc)) {
	    if (!cur_win->persistent_selections) {
		sel_yank(SEL_KREG);
		sel_release();
	    }
	    (void) sel_begin();
	    (void)update(TRUE);
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

static XMotionEvent *
compress_motion(
    XMotionEvent *ev)
{
    XEvent      nev;

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
x_process_event(
    Widget	w GCC_UNUSED,
    XtPointer	unused GCC_UNUSED,
    XEvent     *ev,
    Boolean    *continue_to_dispatch GCC_UNUSED)
{
    int         sc,
                sr;
    unsigned    ec,
                er;

    int         nr,
                nc;
    static int onr = -1, onc = -1;

    XMotionEvent *mev;
    XExposeEvent *gev;
    Bool	do_sel;
    WINDOW	*wp;

    switch (ev->type) {
    case Expose:
	gev = (XExposeEvent *)ev;
	sc = gev->x / cur_win->char_width;
	sr = gev->y / cur_win->char_height;
	ec = CEIL(gev->x + gev->width,  cur_win->char_width);
	er = CEIL(gev->y + gev->height, cur_win->char_height);
	x_touch(cur_win, sc, sr, ec, er);
	cur_win->exposed = TRUE;
	if (ev->xexpose.count == 0)
		x_flush();
	break;

    case VisibilityNotify:
	cur_win->visibility = ev->xvisibility.state;
	XSetGraphicsExposures(dpy, cur_win->textgc,
			      cur_win->visibility != VisibilityUnobscured);
	break;

    case MotionNotify:
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
	    nr = -1;	/* want to be out of bounds to force scrolling */
	else if (nr > cur_win->rows)
	    nr = cur_win->rows;

	if (nc < 0)
	    nc = 0;
	else if (nc >= cur_win->cols)
	    nc = cur_win->cols-1;

	/* ignore any spurious motion during a multi-cick */
	if (cur_win->numclicks > 1
	 && cur_win->lasttime != 0
         && (absol(ev->xmotion.time - cur_win->lasttime) < cur_win->click_timeout))
	    return;
	if (do_sel) {
	    if (ev->xbutton.state & ControlMask) {
		(void)sel_setshape(RECTANGLE);
	    }
	    if (nr != onr || nc != onc)
		extend_selection(cur_win, nr, nc, True);
	    onr = nr;
	    onc = nc;
	}
	else {
	    if (!reading_msg_line && (wp = row2window(nr)) && wp != curwp) {
		(void) set_curwp(wp);
		(void) update(TRUE);
	    }
	}
	break;
    case ButtonPress:
	nc = ev->xbutton.x / cur_win->char_width;
	nr = ev->xbutton.y / cur_win->char_height;
	TRACE(("ButtonPress #%d (%d,%d)\n", ev->xbutton.button, nr, nc))
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
	    x_paste_selection();
	    break;
	case Button3:		/* end/extend selection */
	    if (((wp = row2window(nr)) != 0) && sel_buffer() == wp->w_bufp)
		(void) set_curwp(wp);
	    if (ev->xbutton.state & ControlMask)
		(void)sel_setshape(RECTANGLE);
	    cur_win->wipe_permitted = True;
	    cur_win->prevDOT = DOT;
	    extend_selection(cur_win, nr, nc, False);
	    break;
	}
	break;
    case ButtonRelease:
	TRACE(("ButtonRelease #%d (%d,%d)%s\n",
		ev->xbutton.button,
		ev->xbutton.y / cur_win->char_height,
		ev->xbutton.x / cur_win->char_width,
		cur_win->did_select ? ": did_select" : ""))
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
		DOT = cur_win->prevDOT;
		(void)update(TRUE);
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
x_configure_window(
    Widget	w GCC_UNUSED,
    XtPointer	unused GCC_UNUSED,
    XEvent     *ev,
    Boolean    *continue_to_dispatch GCC_UNUSED)
{
    int nr, nc;
    Dimension new_width, new_height;

    if (ev->type != ConfigureNotify)
	return;

    if (cur_win->base_width < 0) {
	/* First time through...figure out the base width and height */
	XtVaGetValues(cur_win->top_widget,
		XtNheight,	&cur_win->top_height,
		XtNwidth,	&cur_win->top_width,
		NULL);
#if ATHENA_WIDGETS
	XtVaGetValues(cur_win->menu_widget,
		XtNheight,	&cur_win->menu_height,
		XtNwidth,	&new_width,
		NULL);
#endif
	XtVaGetValues(cur_win->screen,
		XtNheight,	&new_height,
		XtNwidth,	&new_width,
		NULL);
	cur_win->base_width = cur_win->top_width - new_width;
	cur_win->base_height = cur_win->menu_height;

	XtVaSetValues(cur_win->top_widget,
#if XtSpecificationRelease >= 4
		XtNbaseHeight,	cur_win->base_height,
		XtNbaseWidth,	cur_win->base_width,
#endif
		XtNminHeight,	cur_win->base_height
				    + MINROWS*cur_win->char_height,
		XtNminWidth,	cur_win->base_width
				    + MINCOLS*cur_win->char_width,
		XtNheightInc,	cur_win->char_height,
		XtNwidthInc,	cur_win->char_width,
		NULL);
    }

    if (ev->xconfigure.height == cur_win->top_height
     && ev->xconfigure.width == cur_win->top_width)
 	return;

    XtVaGetValues(cur_win->top_widget,
	    XtNheight,	&new_height,
	    XtNwidth,	&new_width,
	    NULL);
    new_height = ((new_height - cur_win->base_height) / cur_win->char_height)
			     * cur_win->char_height;
    new_width = ((new_width - cur_win->base_width) /
		    cur_win->char_width) * cur_win->char_width;
#if MOTIF_WIDGETS
    XtVaSetValues(cur_win->form_widget,
	    XmNresizePolicy,	XmRESIZE_NONE,
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
	    XtNwidth,		new_width + cur_win->pane_width + 2,
	    XtNheight,		new_height,
	    NULL);
#endif /* NO_WIDGETS */
#endif /* MOTIF_WIDGETS */
    XtVaSetValues(cur_win->screen,
	    XtNheight,	new_height,
	    XtNwidth,	new_width,
#if NO_WIDGETS || ATHENA_WIDGETS
	    XtNx,	cur_win->scrollbar_on_left ? cur_win->pane_width+2 : 0,
#endif
	    NULL);
    XtVaSetValues(cur_win->pane,
#if !NO_WIDGETS && !ATHENA_WIDGETS
	    XtNwidth,	cur_win->pane_width,
#if OL_WIDGETS
	    XtNheight,	new_height,
#endif /* OL_WIDGETS */
#else	/* NO_WIDGETS */
	    XtNx,	cur_win->scrollbar_on_left ? 0 : new_width,
	    XtNwidth,	cur_win->pane_width+2,
	    XtNheight,	new_height - cur_win->char_height,
#endif /* NO_WIDGETS */
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
	    XmNresizePolicy,	XmRESIZE_ANY,
	    NULL);
#endif /* MOTIF_WIDGETS */

    XtVaGetValues(cur_win->top_widget,
	    XtNheight,	&cur_win->top_height,
	    XtNwidth,	&cur_win->top_width,
	    NULL);
    XtVaGetValues(cur_win->screen,
	    XtNheight,	&new_height,
	    XtNwidth,	&new_width,
	    NULL);

    nr = (int)(new_height / cur_win->char_height);
    nc = (int)(new_width  / cur_win->char_width);

    if (nr < MINROWS || nc < MINCOLS) {
	x_resize(nc, nr);
	/* Calling XResizeWindow will cause another ConfigureNotify
	 * event, so we should return early and let this event occur.
	 */
	return;
    }

    if (nc != cur_win->cols || nr != cur_win->rows) {
	newscreensize(nr,nc);
	cur_win->rows = nr;
	cur_win->cols = nc;
	if (check_scrollbar_allocs() == TRUE) /* no allocation failure */
		update_scrollbar_sizes();
    }
#if MOTIF_WIDGETS
    lookfor_sb_resize = FALSE;
#endif
}

void
x_resize(
    int cols,
    int rows)
{
    if (cols < MINCOLS)
	cols = MINCOLS;
    if (rows < MINROWS)
	rows = MINROWS;

    XResizeWindow(dpy, XtWindow(cur_win->top_widget),
		  (unsigned)cols * cur_win->char_width + cur_win->base_width,
		  (unsigned)rows * cur_win->char_height + cur_win->base_height);
    /* This should cause a ConfigureNotify event */
}

static
int check_scrollbar_allocs(void)
{
	int newmax = cur_win->rows/2;
	int oldmax = cur_win->maxscrollbars;

	if (newmax > oldmax) {

		GROW(cur_win->scrollbars, Widget, oldmax, newmax);
#if OL_WIDGETS
		GROW(cur_win->sliders, Widget, oldmax, newmax);
#endif
#if NO_WIDGETS || ATHENA_WIDGETS
		GROW(cur_win->scrollinfo, ScrollInfo, oldmax, newmax);
		GROW(cur_win->grips, Widget, oldmax, newmax);
#endif

		cur_win->maxscrollbars = newmax;
	}
	return TRUE;
}

#if MOTIF_WIDGETS
static void
grip_moved(
    Widget	w,
    XtPointer	unused,
    XEvent     *ev,
    Boolean    *continue_to_dispatch)
{
    int i;
    register WINDOW *wp, *saved_curwp;
    Dimension height;
    int	lines;

    if (!lookfor_sb_resize)
	return;
    lookfor_sb_resize = FALSE;
    saved_curwp = curwp;

    i = 0;
    for_each_window(wp) {
	XtVaGetValues(cur_win->scrollbars[i],
		XtNheight, &height,
		NULL);
	lines = (height+(cur_win->char_height/2)) / cur_win->char_height;
	if (lines == 0) {
	    /* Delete the window */
	    delwp(wp);
	    if (saved_curwp == wp)
		saved_curwp = wheadp;
	}
	else {
	    curwp = wp;
	    resize(TRUE, lines);
	}
	i++;
    }
    set_curwp(saved_curwp);
    (void) update(TRUE);
}
#endif

static void
configure_bar(
    Widget w,
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
    for_each_window(wp) {
	if (cur_win->scrollbars[i] == w) {
	    if (strcmp(params[0], "Only") == 0) {
		set_curwp(wp);
		onlywind(TRUE,0);
	    }
	    else if (strcmp(params[0], "Kill") == 0) {
		set_curwp(wp);
		delwind(TRUE,0);
	    }
	    else if (strcmp(params[0], "Split") == 0) {
		if (wp->w_ntrows < 3) {
		    x_beep();
		    break;
		}
		else {
		    int newsize;
		    set_curwp(wp);
		    newsize = CEIL(event->xbutton.y, cur_win->char_height)-1;
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
pane_button(
    Widget	w,
    XtPointer	unused,
    XEvent     *ev,
    Boolean    *continue_to_dispatch)
{
    lookfor_sb_resize = TRUE;
}
#endif /* MOTIF_WIDGETS */

/*ARGSUSED*/
static void
x_change_focus(
    Widget	w,
    XtPointer	unused GCC_UNUSED,
    XEvent     *ev,
    Boolean    *continue_to_dispatch GCC_UNUSED)
{
    static int got_focus_event = FALSE;

    switch (ev->type) {
	case EnterNotify:
	    if (!ev->xcrossing.focus || got_focus_event)
		return;
	    goto focus_in;
	case FocusIn:
	    got_focus_event = TRUE;
focus_in:
	    cur_win->show_cursor = True;
#if MOTIF_WIDGETS
	    XmProcessTraversal(cur_win->screen, XmTRAVERSE_CURRENT);
#else /* OL_WIDGETS || NO_WIDGETS */
	    XtSetKeyboardFocus(w, cur_win->screen);
#endif
	    x_flush();
	    break;
	case LeaveNotify:
	    if ( !ev->xcrossing.focus
	      || got_focus_event
	      || ev->xcrossing.detail == NotifyInferior)
		return;
	    goto focus_out;
	case FocusOut:
	    got_focus_event = TRUE;
focus_out:
	    cur_win->show_cursor = False;
	    x_flush();
	    break;
    }
}

/*ARGSUSED*/
static void
x_wm_delwin(
    Widget	w GCC_UNUSED,
    XtPointer	unused GCC_UNUSED,
    XEvent     *ev,
    Boolean    *continue_to_dispatch GCC_UNUSED)
{
    if ( ev->type == ClientMessage
      && ev->xclient.message_type == atom_WM_PROTOCOLS
      && (Atom) ev->xclient.data.l[0] == atom_WM_DELETE_WINDOW) {
	quit(FALSE, 0);		/* quit might not return */
	(void) update(TRUE);
    }
}

/*
 * Return true if we want to disable reports of the cursor position because the
 * cursor really should be on the message-line.
 */
#if UNUSED
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
    while ((XtAppPending(cur_win->app_context) & XtIMXEvent)
        && !kqfull(cur_win)) {

	/* Get and dispatch next event */
	XtAppNextEvent(cur_win->app_context, &ev);

	/*
	 * Ignore or save certain events which could get us into trouble with
	 * reentrancy.
	 */
	switch (ev.type) {
	    case ButtonPress :
	    case ButtonRelease :
	    case MotionNotify :
		/* Ignore the event */
		continue;

	    case ClientMessage :
	    case SelectionClear :
	    case SelectionNotify :
	    case SelectionRequest :
	    case ConfigureNotify :
	    case ConfigureRequest :
	    case PropertyNotify :
	    case ReparentNotify :
	    case ResizeRequest :
		/* Queue for later processing.  */
		evqadd(&ev);
		continue;

	    default :
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
		kqadd(cur_win, abortc);
#if SYS_VMS
		kbd_alarm(); /* signals? */
#else
		(void)signal_pg(SIGINT);
#endif
	    }
	    else
		kqadd(cur_win, c);
	}
    }
}

#if OPT_WORKING
void
x_working(void)
{
    x_set_watch_cursor(TRUE);
    x_move_events();
}

static void
x_set_watch_cursor(int onflag)
{
    static int watch_is_on = FALSE;
#if NO_WIDGETS || ATHENA_WIDGETS
    int i;
#endif

    if (onflag == watch_is_on)
	return;

    watch_is_on = onflag;

    if (onflag) {
	set_pointer(XtWindow(cur_win->screen), cur_win->watch_pointer);
#if NO_WIDGETS || ATHENA_WIDGETS
	for (i=0; i<cur_win->nscrollbars; i++) {
	    set_pointer(
		    XtWindow(cur_win->scrollbars[i]), cur_win->watch_pointer);
	    if (i < cur_win->nscrollbars-1)
		set_pointer(
			XtWindow(cur_win->grips[i]), cur_win->watch_pointer);
	}
#endif /* NO_WIDGETS */
    }
    else {
	set_pointer(XtWindow(cur_win->screen), cur_win->normal_pointer);
#if NO_WIDGETS || ATHENA_WIDGETS
	for (i=0; i<cur_win->nscrollbars; i++) {
	    set_pointer(
		    XtWindow(cur_win->scrollbars[i]),
		    curs_sb_v_double_arrow);
	    if (i < cur_win->nscrollbars-1)
		set_pointer(
			XtWindow(cur_win->grips[i]),
			curs_double_arrow);
	}
#endif /* NO_WIDGETS */
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
    struct eventqueue * newentry;
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
    free((char *)delentry);
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
kqadd(
    TextWindow tw,
    int c)
{
    tw->kq[tw->kqtail] = c;
    tw->kqtail = (tw->kqtail + 1) % KQSIZE;
}

static int
kqpop(TextWindow tw)
{
    if (--(tw->kqtail) < 0)
	tw->kqtail = KQSIZE-1;
    return (tw->kq[tw->kqtail]);
}

/*ARGSUSED*/
static void
display_cursor(
    XtPointer client_data GCC_UNUSED,
    XtIntervalId *idp)
{
    static Bool am_blinking = FALSE;

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

    if (IS_DIRTY(ttrow,ttcol) && idp == (XtIntervalId *) 0)
	return;

    if (cur_win->show_cursor) {
	if ( cur_win->blink_interval > 0
	  || ( cur_win->blink_interval < 0 && IS_REVERSED(ttrow, ttcol) )) {
	    if (idp != (XtIntervalId *) 0 || !am_blinking) {
		/* Set timer to get blinking */
		cur_win->blink_id = XtAppAddTimeOut(
			cur_win->app_context,
			(unsigned long) max(cur_win->blink_interval,
			                    -cur_win->blink_interval),
			display_cursor,
			(XtPointer) 0);
		cur_win->blink_status ^= BLINK_TOGGLE;
		am_blinking = TRUE;
	    }
	    else
		cur_win->blink_status &= ~BLINK_TOGGLE;
	}
	else {
	    am_blinking = FALSE;
	    cur_win->blink_status &= ~BLINK_TOGGLE;
	    if (cur_win->blink_id != (XtIntervalId) 0) {
		XtRemoveTimeOut(cur_win->blink_id);
		cur_win->blink_id = (XtIntervalId) 0;
	    }
	}

	MARK_CELL_DIRTY(ttrow,ttcol);
	MARK_LINE_DIRTY(ttrow);
	flush_line(&CELL_TEXT(ttrow,ttcol), 1,
	           (unsigned int) (VATTRIB(CELL_ATTR(ttrow,ttcol))
		                ^ ((cur_win->blink_status & BLINK_TOGGLE)
				  ? 0 : VACURS)),
		   ttrow, ttcol);
    }
    else {
	/* This code will get called when the window no longer has the focus. */
	if (cur_win->blink_id != (XtIntervalId) 0) {
	    XtRemoveTimeOut(cur_win->blink_id);
	    cur_win->blink_id = (XtIntervalId) 0;
	}
	am_blinking = FALSE;
	MARK_CELL_DIRTY(ttrow,ttcol);
	MARK_LINE_DIRTY(ttrow);
	flush_line(&CELL_TEXT(ttrow,ttcol), 1,
	    (unsigned int) VATTRIB(CELL_ATTR(ttrow,ttcol)), ttrow, ttcol);
	XDrawRectangle(dpy, cur_win->win,
	               IS_REVERSED(ttrow,ttcol) ? cur_win->cursgc
		                                : cur_win->revcursgc,
	               x_pos(cur_win, ttcol), y_pos(cur_win, ttrow),
		       (unsigned)(cur_win->char_width - 1),
		       (unsigned)(cur_win->char_height - 1));
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
    for_ever {

	if (tb_more(PasteBuf)) {	/* handle any queued pasted text */
	    c = tb_next(PasteBuf);
	    c |= NOREMAP;	/* pasted chars are not subject to mapping */
	    cur_win->pasting = True;
	    break;
	} else if (cur_win->pasting) {
	    /*
	     * Set the default position for new pasting to just past the newly
	     * inserted text.
	     */
	    if (DOT.o < llength(DOT.l) && !insertmode)
		DOT.o++;		/* Advance DOT so that consecutive
					   pastes come out right */
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
	} while ((XtAppPending(cur_win->app_context) & XtIMXEvent)
	    && !kqfull(cur_win));
    }

    return c;
}

/*
 * Another event loop used for determining type-ahead.
 */
int
x_typahead(
    int milli)		/* milliseconds to wait for type-ahead */
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
	    timeoutid = XtAppAddTimeOut(
			    cur_win->app_context,
			    (ULONG) milli,
			    x_typahead_timeout,
			    (XtPointer) &timedout);
	}
	else
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
		(XtAppPending(cur_win->app_context) & XtIMXEvent)) {
	    XEvent ev;
	    XtAppNextEvent(cur_win->app_context, &ev);
	    XtDispatchEvent(&ev);
	}

	/* Now wait for timer and process events as necessary. */
	while (!timedout && kqempty(cur_win)) {
	    XtAppProcessEvent(cur_win->app_context, (XtInputMask)XtIMAll);
	}

	if (!timedout)
	    XtRemoveTimeOut(timeoutid);

	status = !kqempty(cur_win);
    }

    (void) im_waiting(olddkr);

    return status;
}

/*ARGSUSED*/
static void
x_typahead_timeout(
    XtPointer flagp,
    XtIntervalId *id GCC_UNUSED)
{
    * (int *) flagp = 1;
}

/*ARGSUSED*/
static void
x_key_press(
    Widget	w GCC_UNUSED,
    XtPointer	unused GCC_UNUSED,
    XEvent     *ev,
    Boolean    *continue_to_dispatch GCC_UNUSED)
{
    char	buffer[128];
    KeySym	keysym;
    int		num;

    register int i;
    register SIZE_T n;

    static const struct {
	KeySym  key;
	int     code;
    } escapes[] = {
	/* Arrow keys */
	{XK_Up,      KEY_Up},
	{XK_Down,    KEY_Down},
	{XK_Right,   KEY_Right},
	{XK_Left,    KEY_Left},
	/* page scroll */
	{XK_Next,    KEY_Next},
	{XK_Prior,   KEY_Prior},
	{XK_Home,    KEY_Home},
	{XK_End,     KEY_End},
	/* editing */
	{XK_Insert,  KEY_Insert},
	{XK_Find,    KEY_Find},
	{XK_Select,  KEY_Select},
	/* command keys */
	{XK_Menu,    KEY_Menu},
	{XK_Help,    KEY_Help},
	/* function keys */
	{XK_F1,      KEY_F1},
	{XK_F2,      KEY_F2},
	{XK_F3,      KEY_F3},
	{XK_F4,      KEY_F4},
	{XK_F5,      KEY_F5},
	{XK_F6,      KEY_F6},
	{XK_F7,      KEY_F7},
	{XK_F8,      KEY_F8},
	{XK_F9,      KEY_F9},
	{XK_F10,     KEY_F10},
	{XK_F11,     KEY_F11},
	{XK_F12,     KEY_F12},
	{XK_F13,     KEY_F13},
	{XK_F14,     KEY_F14},
	{XK_F15,     KEY_F15},
	{XK_F16,     KEY_F16},
	{XK_F17,     KEY_F17},
	{XK_F18,     KEY_F18},
	{XK_F19,     KEY_F19},
	{XK_F20,     KEY_F20},
#if defined(XK_F21) && defined(KEY_F21)
	{XK_F21,     KEY_F21},
	{XK_F22,     KEY_F22},
	{XK_F23,     KEY_F23},
	{XK_F24,     KEY_F24},
	{XK_F25,     KEY_F25},
	{XK_F26,     KEY_F26},
	{XK_F27,     KEY_F27},
	{XK_F28,     KEY_F28},
	{XK_F29,     KEY_F29},
	{XK_F30,     KEY_F30},
	{XK_F31,     KEY_F31},
	{XK_F32,     KEY_F32},
	{XK_F33,     KEY_F33},
	{XK_F34,     KEY_F34},
	{XK_F35,     KEY_F35},
#endif
	/* keypad function keys */
	{XK_KP_F1,   KEY_KP_F1},
	{XK_KP_F2,   KEY_KP_F2},
	{XK_KP_F3,   KEY_KP_F3},
	{XK_KP_F4,   KEY_KP_F4}
    };

    if (ev->type != KeyPress)
	return;

    num = XLookupString((XKeyPressedEvent *) ev, buffer, sizeof(buffer),
		&keysym, (XComposeStatus *) 0);

    if (num <= 0) {
	for (n = 0; n < TABLESIZE(escapes); n++) {
	    if (keysym == escapes[n].key) {
		kqadd(cur_win, escapes[n].code);
		return;
	    }
	}
    }
    else if (num == 1 && (ev->xkey.state & Mod1Mask))
	buffer[0] |= HIGHBIT;

    /* FIXME: Should do something about queue full conditions */
    if (num > 0) {
	for (i=0; i<num && !kqfull(cur_win); i++)
	    kqadd(cur_win, char2int(buffer[i]));
    }
}

/*
 * change reverse video status
 */
static void
x_rev(int state)
{
    cur_win->reverse = state;
}

/* change screen resolution */
/*ARGSUSED*/
static int
x_cres(char *flag GCC_UNUSED)
{
    return TRUE;
}

#if OPT_COLOR
static void
x_fcol(int color)
{
}

static void
x_bcol(int color)
{
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
	XSetFunction(dpy, cur_win->textgc, GXxor);
	XSetBackground(dpy, cur_win->textgc, 0L);
	XSetForeground(dpy, cur_win->textgc, cur_win->fg ^ cur_win->bg);
	XFillRectangle(dpy, cur_win->win, cur_win->textgc,
		       0, 0, x_width(cur_win), x_height(cur_win));
	XFlush(dpy);
	catnap(90, FALSE);
	XFillRectangle(dpy, cur_win->win, cur_win->textgc,
		       0, 0, x_width(cur_win), x_height(cur_win));
	XFlush(dpy);
	XSetFunction(dpy, cur_win->textgc, GXcopy);
	XSetBackground(dpy, cur_win->textgc, cur_win->bg);
	XSetForeground(dpy, cur_win->textgc, cur_win->fg);
	XUngrabServer(dpy);
	endofDisplay();
    }
    else
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
}
#endif	/* NO_LEAKS */

char x_window_name[NFILEN];
char x_icon_name[NFILEN];

void
x_set_icon_name(char *name)
{
	XTextProperty Prop;

	(void)strncpy0(x_icon_name, name, NFILEN);

	Prop.value = (unsigned char *)name;
	Prop.encoding = XA_STRING;
	Prop.format = 8;
	Prop.nitems = strlen(name);

	XSetWMIconName(dpy,XtWindow(cur_win->top_widget),&Prop);
}

char *
x_get_icon_name(void)
{
	return x_icon_name;
}

void
x_set_window_name(char *name)
{
	XTextProperty Prop;

	(void)strncpy0(x_window_name, name, NFILEN);

	Prop.value = (unsigned char *)name;
	Prop.encoding = XA_STRING;
	Prop.format = 8;
	Prop.nitems = strlen(name);

	XSetWMName(dpy,XtWindow(cur_win->top_widget),&Prop);
}

char *
x_get_window_name(void)
{
    	return x_window_name;
}

#endif	/* DISP_X11 && XTOOLKIT */
