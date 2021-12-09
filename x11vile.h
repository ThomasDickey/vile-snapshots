/*
 * Common definitions for xvile modules.
 *
 * $Id: x11vile.h,v 1.27 2021/12/07 23:26:52 tom Exp $
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

#ifndef _x11vile_h
#define _x11vile_h 1

#define NEED_X_INCLUDES 1
#include	"estruct.h"
#include	"edef.h"
#include	"nefunc.h"
#include	"pscreen.h"

#include	<X11/cursorfont.h>

#if defined(lint) && defined(HAVE_X11_INTRINSICI_H)
#include	<X11/IntrinsicI.h>
#endif

#define Nval(name,value) name, (XtArgVal)(value)
#define Sval(name,value) name, (value)

#if XtSpecificationRelease < 4
#define XtPointer caddr_t
#endif

/* sanity-check, so we know it's safe to not nest ifdef's */

#if NO_WIDGETS
# if ATHENA_WIDGETS || MOTIF_WIDGETS
> make an error <
# endif
#else
# if ATHENA_WIDGETS
#  if MOTIF_WIDGETS
> make an error <
#  endif
# else
#  if MOTIF_WIDGETS
#  else
> make an error <
#  endif
# endif
#endif

#ifndef OPT_KEV_SCROLLBARS
# if NO_WIDGETS
#  define OPT_KEV_SCROLLBARS 1
# else
#  define OPT_KEV_SCROLLBARS 0
# endif
#endif

#ifndef OPT_KEV_DRAGGING
# if NO_WIDGETS
#  define OPT_KEV_DRAGGING 1
# else
#  define OPT_KEV_DRAGGING 0
# endif
#endif

#ifndef KEV_WIDGETS
# if NO_WIDGETS || OPT_KEV_SCROLLBARS
#  define KEV_WIDGETS 1
# else
#  define KEV_WIDGETS 0
# endif
#endif

#ifndef OPT_XAW_SCROLLBARS
# if ATHENA_WIDGETS && !OPT_KEV_SCROLLBARS
#  define OPT_XAW_SCROLLBARS 1
# else
#  define OPT_XAW_SCROLLBARS 0
# endif
#endif

#define MY_CLASS	"XVile"

#if SYS_VMS
#undef SYS_UNIX
#define coreWidgetClass widgetClass	/* patch for VMS 5.4-3 (dickey) */
#endif

/* redefined in X11/Xos.h */
#undef strchr
#undef strrchr

#define OPT_INPUT_METHOD OPT_LOCALE

#if OPT_XAW_SCROLLBARS
#include	<X11/IntrinsicP.h>
#if defined(HAVE_LIB_XAW3DXFT)
#include	<X11/Xaw3dxft/Form.h>
#include	<X11/Xaw3dxft/Grip.h>
#include	<X11/Xaw3dxft/Scrollbar.h>
#include	<X11/Xaw3dxft/XawImP.h>
#elif defined(HAVE_LIB_XAW3D)
#include	<X11/Xaw3d/Form.h>
#include	<X11/Xaw3d/Grip.h>
#include	<X11/Xaw3d/Scrollbar.h>
#include	<X11/Xaw3d/XawImP.h>
#elif defined(HAVE_LIB_XAWPLUS)
#include	<X11/XawPlus/Form.h>
#include	<X11/XawPlus/Grip.h>
#include	<X11/XawPlus/Scrollbar.h>
#include	<X11/XawPlus/XawImP.h>
#elif defined(HAVE_LIB_NEXTAW)
#include	<X11/neXtaw/Form.h>
#include	<X11/neXtaw/Grip.h>
#include	<X11/neXtaw/Scrollbar.h>
#include	<X11/neXtaw/XawImP.h>
#elif defined(HAVE_LIB_XAW)
#include	<X11/Xaw/Form.h>
#include	<X11/Xaw/Grip.h>
#include	<X11/Xaw/Scrollbar.h>
#include	<X11/Xaw/XawImP.h>
#endif
#endif /* OPT_XAW_SCROLLBARS */

#include	<X11/Shell.h>
#include	<X11/keysym.h>
#include	<X11/Xatom.h>

#ifdef HAVE_X11_XMU_ATOMS_H
#include	<X11/Xmu/Atoms.h>
#endif

#if MOTIF_WIDGETS
#include	<Xm/Form.h>
#include	<Xm/PanedW.h>
#include	<Xm/ScrollBar.h>
#include	<Xm/SashP.h>
#if defined(lint) && defined(HAVE_XM_XMP_H)
#include	<Xm/XmP.h>
#endif
#endif /* MOTIF_WIDGETS */

#ifndef XtNinputMethod
#define XtNinputMethod		"inputMethod"
#define XtCInputMethod		"InputMethod"
#endif

#ifndef XtNpreeditType
#define XtNpreeditType		"preeditType"
#define XtCPreeditType		"PreeditType"
#endif

#ifndef XtNopenIm
#define XtNopenIm		"openIm"
#define XtCOpenIm		"OpenIm"
#endif

#ifdef HAVE_LIBXPM
#include <X11/xpm.h>
#endif

#if OPT_INPUT_METHOD
#include <locale.h>
#endif

#ifndef CI_NONEXISTCHAR
/* from X11/Xlibint.h - not all vendors install this file */
#define CI_NONEXISTCHAR(cs) (((cs)->width == 0) && \
			     (((cs)->rbearing|(cs)->lbearing| \
			       (cs)->ascent|(cs)->descent) == 0))

#define CI_GET_CHAR_INFO_1D(fs,col,def,cs) \
{ \
    cs = def; \
    if (col >= fs->min_char_or_byte2 && col <= fs->max_char_or_byte2) { \
	if (fs->per_char == NULL) { \
	    cs = &fs->min_bounds; \
	} else { \
	    cs = &fs->per_char[(col - fs->min_char_or_byte2)]; \
	    if (CI_NONEXISTCHAR(cs)) cs = def; \
	} \
    } \
}

#define CI_GET_CHAR_INFO_2D(fs,row,col,def,cs) \
{ \
    cs = def; \
    if (row >= fs->min_byte1 && row <= fs->max_byte1 && \
	col >= fs->min_char_or_byte2 && col <= fs->max_char_or_byte2) { \
	if (fs->per_char == NULL) { \
	    cs = &fs->min_bounds; \
	} else { \
	    cs = &fs->per_char[((row - fs->min_byte1) * \
				(fs->max_char_or_byte2 - \
				 fs->min_char_or_byte2 + 1)) + \
			       (col - fs->min_char_or_byte2)]; \
	    if (CI_NONEXISTCHAR(cs)) cs = def; \
	} \
    } \
}
#endif /* CI_NONEXISTCHAR */

#define TRACE_RES_I(res,val) TRACE(("\t%s: %d\n", res, cur_win->val))
#define TRACE_RES_B(res,val) TRACE(("\t%s: %s\n", res, cur_win->val ? "true" : "false"))
#define TRACE_RES_P(res,val) TRACE(("\t%s: %06lx\n", res, cur_win->val))
#define TRACE_RES_S(res,val) TRACE(("\t%s: %s\n", res, NonNull(cur_win->val)))

#define IsPrimary(s)    ((s) == XA_PRIMARY)

#define	absol(x)	((x) > 0 ? (x) : -(x))
#define	CEIL(a,b)	((a + b - 1) / (b))

#define onMsgRow(tw)	(ttrow == (int)(tw->rows - 1))

/* XXX -- use xcutsel instead */
#undef	SABER_HACK		/* hack to support Saber since it doesn't do
				 * selections right */

#undef btop			/* defined in SunOS includes */

#define PANE_WIDTH_MAX 200

#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
# define PANE_WIDTH_DEFAULT 15
# define PANE_WIDTH_MIN 7
#else
# if MOTIF_WIDGETS
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

#if OPT_MENUS_COLORED
#define XtNmenuBackground "menuBackground"
#define XtNmenuForeground "menuForeground"
#define XtCMenuBackground "MenuBackground"
#define XtCMenuForeground "MenuForeground"
#endif

#define MENU_HEIGHT_DEFAULT 20

#define MINCOLS	30
#define MINROWS MINWLNS
#define MAXSBS	((MAXROWS) / 2)

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

#if OPT_MENUS
#if ATHENA_WIDGETS
#if defined(HAVE_LIB_XAW3DXFT)
#include	<X11/Xaw3dxft/SimpleMenu.h>
#include	<X11/Xaw3dxft/Box.h>
#include	<X11/Xaw3dxft/Form.h>
#include	<X11/Xaw3dxft/Paned.h>
#elif defined(HAVE_LIB_XAW3D)
#include	<X11/Xaw3d/SimpleMenu.h>
#include	<X11/Xaw3d/Box.h>
#include	<X11/Xaw3d/Form.h>
#include	<X11/Xaw3d/Paned.h>
#elif defined(HAVE_LIB_XAWPLUS)
#include	<X11/XawPlus/SimpleMenu.h>
#include	<X11/XawPlus/Box.h>
#include	<X11/XawPlus/Form.h>
#include	<X11/XawPlus/Paned.h>
#elif defined(HAVE_LIB_NEXTAW)
#include	<X11/neXtaw/SimpleMenu.h>
#include	<X11/neXtaw/Box.h>
#include	<X11/neXtaw/Form.h>
#include	<X11/neXtaw/Paned.h>
#elif defined(HAVE_LIB_XAW)
#include	<X11/Xaw/SimpleMenu.h>
#include	<X11/Xaw/Box.h>
#include	<X11/Xaw/Form.h>
#include	<X11/Xaw/Paned.h>
#endif
#elif MOTIF_WIDGETS
#include	<Xm/RowColumn.h>
#endif
#endif /* OPT_MENUS */

#ifdef XRENDERFONT
#include	<X11/Xft/Xft.h>
#endif

#ifdef XRENDERFONT
typedef XftFont XVileFont;
#else
typedef XFontStruct XVileFont;
#endif

typedef struct {
    XVileFont *norm;		/* normal */
    XVileFont *bold;		/* bold */
    XVileFont *ital;		/* italic */
    XVileFont *btal;		/* bold/italic */
} XVileFonts;

#if OPT_XAW_SCROLLBARS
typedef struct _scroll_info {
    int totlen;			/* total length of scrollbar */
} ScrollInfo;

#else
#if OPT_KEV_SCROLLBARS
typedef struct _scroll_info {
    int top;			/* top of "thumb" */
    int bot;			/* bottom of "thumb" */
    int totlen;			/* total length of scrollbar */
    Boolean exposed;		/* has scrollbar received expose event? */
} ScrollInfo;
#endif
#endif

typedef enum {
    sgNONE = 0
    ,sgREDO
    ,sgINIT
    ,sgMADE
} XVileStateGC;

typedef struct _text_gc {
    GC gc;
    XGCValues gcvals;
    ULONG gcmask;
    XVileStateGC state;
#ifdef XRENDERFONT
    XftColor xft;
#endif
} ColorGC;

typedef struct _text_win {
    /* X stuff */
    Window win;			/* window corresponding to screen */
    XtAppContext app_context;	/* application context */
    Widget top_widget;		/* top level widget */
    Widget screen;		/* screen widget */
    UINT screen_depth;
#if OPT_MENUS
#if ATHENA_WIDGETS
    Widget pane_widget;		/* pane widget, actually a form */
#endif
    Widget menu_widget;		/* menu-bar widget, actually a box */
#endif
    Widget form_widget;		/* form enclosing text-display + scrollbars */
    Widget pane;		/* panes in which scrollbars live */

    int maxscrollbars;		/* how many scrollbars, sliders, etc. */
    Widget *scrollbars;		/* the scrollbars */
    int nscrollbars;		/* number of currently active scroll bars */

#if OPT_MENUS_COLORED
    Pixel menubar_fg;		/* XRES: color of the menubar */
    Pixel menubar_bg;		/* XRES */
#endif
#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
    Pixel scrollbar_fg;		/* XRES */
    Pixel scrollbar_bg;		/* XRES */
    Bool slider_is_solid;	/* XRES */
    Bool slider_is_3D;
    GC scrollbar_gc;		/* graphics context for scrollbar "thumb" */
    Pixmap trough_pixmap;
    Pixmap slider_pixmap;
    ScrollInfo *scrollinfo;
    Widget *grips;		/* grips for resizing scrollbars */
    XtIntervalId scroll_repeat_id;
    int scroll_repeat_timeout;	/* XRES */
#endif				/* OPT_KEV_SCROLLBARS */
#if OPT_XAW_SCROLLBARS
    Pixmap thumb_bm;		/* bitmap for scrollbar thumb */
#endif
    int scroll_repeat_interval;	/* XRES */
    Bool reverse_video;		/* XRES */
    XtIntervalId blink_id;
    int blink_status;
    int blink_interval;		/* XRES */
    Bool exposed;		/* Have we received any expose events? */
    int visibility;		/* How visible is the window? */

    int base_width;		/* width with screen widgets' width zero */
    int base_height;
    UINT pane_width;		/* XRES: full width of scrollbar pane */
    Dimension menu_height;	/* XRES: height of menu-bar */
    Dimension top_width;	/* width of top widget as of last resize */
    Dimension top_height;	/* height of top widget as of last resize */

    int fsrch_flags;		/* flags which indicate which fonts have
				 * been searched for
				 */
    XVileFonts fonts;
    XVileFont *curfont;		/* Current font */
    ColorGC tt_info;		/* normal/uncolored text GC, related state */
    ColorGC rt_info;		/* reverse/uncolored text GC, related state */
    ColorGC ss_info;		/* normal/selected text GC, related state */
    ColorGC rs_info;		/* reverse/selected text GC, related state */
    Boolean is_color_cursor;
    ColorGC mm_info;		/* modeline w/ focus */
    ColorGC rm_info;		/* reverse/modeline w/ focus  */
    ColorGC oo_info;		/* other modelines  */
    ColorGC ro_info;		/* reverse/other modelines  */
    ColorGC cc_info;		/* cursor GC and related state */
    ColorGC rc_info;		/* reverse cursor GC and related state */
    ColorGC fg_info[NCOLORS];	/* foreground GC and related state */
    ColorGC bg_info[NCOLORS];	/* background GC and related state */
    Boolean bg_follows_fg;
    Pixel fg;			/* XRES: window's foreground */
    Pixel bg;			/* XRES: window's background */
    Pixel default_fg;		/* actual foreground, accounting for reverse */
    Pixel default_bg;		/* actual background, accounting for reverse */
    Pixel colors_fg[NCOLORS];	/* XRES: text foreground colors */
    Pixel colors_bg[NCOLORS];	/* XRES: text background colors */
    Pixel modeline_fg;		/* XRES: modeline foreground color */
    Pixel modeline_bg;		/* XRES: modeline background color */
    Pixel modeline_focus_fg;	/* XRES: modeline focus-foreground color */
    Pixel modeline_focus_bg;	/* XRES: modeline focus-background color */
    Pixel selection_fg;		/* XRES: selection foreground color */
    Pixel selection_bg;		/* XRES: selection background color */
    int char_width;
    int char_ascent;
    int char_descent;
    int char_height;
    Bool left_ink;		/* font has "ink" past bounding box on left */
    Bool right_ink;		/* font has "ink" past bounding box on right */
    int wheel_scroll_amount;	/* XRES */
    char *iconname;		/* XRES */
    char *geometry;		/* XRES */
    char *starting_fontname;	/* XRES name of font at startup */
    char *fontname;		/* name of current font */
    Bool focus_follows_mouse;	/* XRES */
    Bool fork_on_startup;	/* XRES */
    Bool scrollbar_on_left;	/* XRES */
    Bool persistent_selections;	/* XRES */
    Bool selection_sets_DOT;	/* XRES */

    /* text stuff */
    Bool reverse;		/* true if window is currently reversed */
    UINT rows, cols;
    Bool show_cursor;		/* true if cursor is currently in-focus/shown */

    /* cursor stuff */
    Pixel cursor_fg;		/* XRES */
    Pixel cursor_bg;		/* XRES */

    /* pointer stuff */
    Pixel pointer_fg;		/* XRES */
    Pixel pointer_bg;		/* XRES */
    Cursor normal_pointer;	/* XRES */
#if OPT_WORKING
    Cursor watch_pointer;	/* XRES */
    Bool want_to_work;		/* true if "working..." */
#endif

    /* color conversions */
    Colormap colormap;
    XVisualInfo *visInfo;
    int numVisuals;
    Bool has_rgb;
    unsigned rgb_shifts[3];
    unsigned rgb_widths[3];

    /* selection stuff */
    String multi_click_char_class;	/* XRES */
    unsigned click_timeout;	/* XRES */
    Time lasttime;		/* for multi-click */
    int numclicks;
    int last_getc;
    Bool have_selection;
    Bool wipe_permitted;
    Bool was_on_msgline;
    Bool did_select;
    Bool pasting;
    MARK prevDOT;		/* DOT prior to selection */
    XtIntervalId sel_scroll_id;

    /* key press queue */
    int kqhead;
    int kqtail;
    int kq[KQSIZE];

    /* Special translations */
    XtTranslations my_scrollbars_trans;
#if OPT_KEV_SCROLLBARS || OPT_XAW_SCROLLBARS
    XtTranslations my_resizeGrip_trans;
#endif
#if OPT_INPUT_METHOD
    Bool open_im;		/* XRES: XtNopenIm */
    Bool cannot_im;
    char *rs_imFont;		/* XRES: XtNximFont */
    char *rs_inputMethod;	/* XRES: XtNinputMethod */
    char *rs_preeditType;	/* XRES: XtNpreeditType */
    XFontSet imFontSet;
    int imFontHeight;
    XIC imInputContext;		/* input context */
    XIM xim;
#endif

} TextWindowRec, *TextWindow;

typedef struct {
    Atom *targets;
    Time time;
} SelectionList;

#ifdef XRENDERFONT
#define	FONTNAME	"monospace"
#else
#define	FONTNAME	"7x13"
#endif

#define	x_width(tw)		((tw)->cols * (UINT) (tw)->char_width)
#define	x_height(tw)		((tw)->rows * (UINT) (tw)->char_height)
#define	x_pos(tw, c)		((c) * (tw)->char_width)
#define	y_pos(tw, r)		((r) * (tw)->char_height)
#define	text_y_pos(tw, r)	(y_pos((tw), (r)) + (tw)->char_ascent)

#undef min
#define min(a,b)		((a) < (b) ? (a) : (b))

#undef max
#define max(a,b)		((a) > (b) ? (a) : (b))

#define XtNfontFamily		"fontFamily"
#define XtCFontFamily		"FontFamily"

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

#define XtNwheelScrollAmount    "wheelScrollAmount"
#define XtCWheelScrollAmount    "WheelScrollAmount"

#define	XtNscrollRepeatTimeout	"scrollRepeatTimeout"
#define	XtCScrollRepeatTimeout	"ScrollRepeatTimeout"

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

typedef enum {
    aeAVERAGE_WIDTH
    ,aeCHARSET_ENCODING
    ,aeCHARSET_REGISTRY
    ,aeCLIPBOARD
    ,aeCOMPOUND_TEXT
    ,aeFONT
    ,aeFOUNDRY
    ,aeMULTIPLE
    ,aeNONE
    ,aePIXEL_SIZE
    ,aeRESOLUTION_X
    ,aeRESOLUTION_Y
    ,aeSETWIDTH_NAME
    ,aeSLANT
    ,aeSPACING
    ,aeTARGETS
    ,aeTEXT
    ,aeTIMESTAMP
    ,aeUTF8_STRING
    ,aeWEIGHT_NAME
    ,aeWM_DELETE_WINDOW
    ,aeWM_PROTOCOLS
    ,aeMAX
} XVileAtom;

#define GetAtom(name) xvileAtom(ae ## name)
#define GetColorGC(win, name) makeColorGC(win, &win->name)->gc

extern Atom xvileAtom(XVileAtom);
extern ColorGC *x_get_color_gc(TextWindow, int, Bool);
extern ColorGC *makeColorGC(TextWindow, ColorGC *);
extern XVileFont *xvileQueryFont(Display *, TextWindow, const char *);
extern void x_set_font_encoding(ENC_CHOICES);
extern void x_set_fontname(TextWindow, const char *);
extern void xvileCloseFonts(Display *, XVileFonts *);
extern void xvileDraw(Display *, TextWindow, VIDEO_TEXT *, int, UINT, int, int);

#define NotImpl() fprintf(stderr, "%s:%d: not implemented\n", __FILE__, __LINE__)

#endif /* _x11vile_h */
