/*
 * Uses the Win32 screen API.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/ntwinio.c,v 1.44 1999/05/19 01:28:44 tom Exp $
 * Written by T.E.Dickey for vile (october 1997).
 * -- improvements by Clark Morgan (see w32cbrd.c, w32pipe.c).
 */

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <stdlib.h>
#include <math.h>

#define	termdef	1			/* don't define "term" external */

#include        "estruct.h"
#include        "edef.h"
#include        "pscreen.h"
#include        "winvile.h"

#undef RECT	/* FIXME: symbol conflict */

#define MIN_ROWS MINWLNS
#define MIN_COLS 15

#define FIXME_POSCHANGING 1		/* this doesn't seem to help */
#define FIXME_RECUR_SB 0		/* I'm not sure this is needed */

#if OPT_TRACE
#define IGN_PROC(tag,name) \
	case name: \
		TRACE((tag #name " (ignored)\n")) \
		break;

#define DEF_PROC(tag,name) \
	case name: \
		TRACE((tag #name " (%s)\n", which_window(hWnd))) \
		return (DefWindowProc(hWnd, message, wParam, lParam))
#else
#define IGN_PROC(tag,name) case name: break;
#define DEF_PROC(tag,name) /*nothing*/
#endif

#define MAIN_CLASS "VileMain"
#define TEXT_CLASS "VileText"
#define SCRL_CLASS "SCROLLBAR"
#define GRIP_CLASS "VileResize"

#define MY_APPLE "Vile Application"

#define MY_FONT  SYSTEM_FIXED_FONT	/* or ANSI_FIXED_FONT		*/
#define GetMyFont() vile_font

#define MM_FILE 1
#define MM_FONT 2

#define NROW	128			/* Max Screen size.		*/
#define NCOL    256			/* Edit if you want to.         */
#define	NPAUSE	200			/* # times thru update to pause */
#define NOKYMAP (-1)
#define KYREDIR (-2)			/* sent keystroke elsewhere.	*/

#define SetCols(value) term.cols = cur_win->cols = value
#define SetRows(value) term.rows = cur_win->rows = value

#define	AttrColor(b,f)	((WORD)(((ctrans[b] & 15) << 4) | (ctrans[f] & 15)))

#define RowToPixel(n) ((n) * nLineHeight)
#define ColToPixel(n) ((n) * nCharWidth)

#define PixelToRow(n) ((n) / nLineHeight)
#define PixelToCol(n) ((n) / nCharWidth)

#define RectToCols(rect) (PixelToCol(rect.right - rect.left - SbWidth))
#define RectToRows(rect) (PixelToRow(rect.bottom - rect.top))

#if OPT_SCROLLBARS
#define SbWidth   nLineHeight
#else
#define SbWidth   0
#endif

static	int	ntgetch		(void);
static	void	ntmove		(int, int);
static	void	nteeol		(void);
static	void	nteeop		(void);
static	void	ntbeep		(void);
static	void	ntopen		(void);
static	void	ntrev		(UINT);
static	int	ntcres		(const char *);
static	void	ntclose		(void);
static	void	ntputc		(int);
static	int	nttypahead	(void);
static	void	ntkopen		(void);
static	void	ntkclose	(void);
#if OPT_COLOR
static	void	ntfcol		(int);
static	void	ntbcol		(int);
#endif
static	void	ntflush		(void);
static	void	ntscroll	(int, int, int);
#if OPT_ICURSOR
static	void	nticursor	(int);
#endif
#if OPT_TITLE
static	void	nttitle		(char *);
#endif

static	HANDLE	vile_hinstance;
static	HMENU	vile_menu;
static	HFONT	vile_font;
static	LOGFONT	vile_logfont;
static	HANDLE	hAccTable;   /* handle to accelerator table */
static	HCURSOR	hglass_cursor;
static	HCURSOR	arrow_cursor;
static	int	nLineHeight = 10;
static	int	nCharWidth = 8;
static	int	caret_disabled = FALSE;
static	int	caret_visible = 0;
static	int	caret_exists = 0;
static	int	vile_in_getfkey = 0;
static	int	vile_resizing = FALSE;	/* rely on repaint_window if true */
static	int	dont_update_sb = FALSE;
static	DWORD	default_fcolor;
static	DWORD	default_bcolor;

#ifdef VILE_OLE
static	OLEAUTO_OPTIONS oa_opts;
static	int             redirect_keys;
#endif

static int	nfcolor = -1;		/* normal foreground color */
static int	nbcolor = -1;		/* normal background color */
static int	ninvert = FALSE;	/* normal colors inverted? */
static int	crow = -1;		/* current row */
static int	ccol = -1;		/* current col */

/* ansi to ibm color translation table */
static	const char *initpalettestr = "0 4 2 6 1 5 3 7 8 12 10 14 9 13 11 15";
/* black, red, green, yellow, blue, magenta, cyan, white   */

static	int	cur_pos = 0;
static	VIDEO_ATTR cur_atr = 0;

typedef	struct {
	HWND	w;
	RECT	r;
	int	shown;
} SBDATA;

struct	gui_info {
	HWND	main_hwnd;		/* the top-level window */
	HWND	text_hwnd;		/* the enclosed text area */
	int	closed;
	int	cols;
	int	rows;
	RECT	xy_limit;
	int	x_border;
	int	y_border;
	int	y_titles;		/* height of caption/titles */
	int	nscrollbars;
	int	maxscrollbars;
	SBDATA	*scrollbars;
	SBDATA	size_box;
	SBDATA	size_grip;
} only_window, *cur_win = &only_window;

#if OPT_SCROLLBARS
static	int	check_scrollbar_allocs(void);
static	void	update_scrollbar_sizes(void);
#endif

/*
 * Standard terminal interface dispatch table. None of the fields point into
 * "termio" code.
 */

TERM    term    = {
	NROW,
	NROW,
	NCOL,
	NCOL,
	NPAUSE,
	ntopen,
	ntclose,
	ntkopen,
	ntkclose,
	ntgetch,
	ntputc,
	nttypahead,
	ntflush,
	ntmove,
	nteeol,
	nteeop,
	ntbeep,
	ntrev,
	ntcres,
#if OPT_COLOR
	ntfcol,
	ntbcol,
	set_ctrans,
#else
	nullterm_setfore,
	nullterm_setback,
	nullterm_setpal,
#endif
	ntscroll,
	nullterm_pflush,
#if OPT_ICURSOR
	nticursor,
#else
	nullterm_icursor,
#endif
#if OPT_TITLE
	nttitle,
#else
	nullterm_settitle,
#endif
	nullterm_watchfd,
	nullterm_unwatchfd,
	nullterm_cursorvis,
};

#if OPT_TRACE
static char *
which_window(HWND hwnd)
{
	if (hwnd == 0) {
		return "NULL";
	} if (hwnd == cur_win->main_hwnd) {
		return "main";
	} else if (hwnd == cur_win->text_hwnd) {
		return "text";
	} else {
		int n;
		static char temp[20];
		sprintf(temp, "%#lx", hwnd);
		for (n = 0; n < cur_win->nscrollbars; n++) {
			if (hwnd == cur_win->scrollbars[n].w) {
				sprintf(temp, "sb%d", n);
				break;
			}
		}
		return temp;
	}
}

static char *
message2s(unsigned code)
{
	static struct {
		WORD code;
		char *name;
	} table[] = {
		{ WM_ACTIVATE,		"WM_ACTIVATE" },
		{ WM_ACTIVATEAPP,	"WM_ACTIVATEAPP" },
		{ WM_CANCELMODE,	"WM_CANCELMODE" },
		{ WM_CAPTURECHANGED,	"WM_CAPTURECHANGED" },
		{ WM_CLOSE,		"WM_CLOSE" },
		{ WM_CREATE,		"WM_CREATE" },
		{ WM_CTLCOLORSCROLLBAR,	"WM_CTLCOLORSCROLLBAR" },
		{ WM_ENABLE,		"WM_ENABLE" },
		{ WM_ENTERIDLE,		"WM_ENTERIDLE" },
		{ WM_ENTERMENULOOP,	"WM_ENTERMENULOOP" },
		{ WM_ENTERSIZEMOVE,	"WM_ENTERSIZEMOVE" },
		{ WM_ERASEBKGND,	"WM_ERASEBKGND" },
		{ WM_EXITMENULOOP,	"WM_EXITMENULOOP" },
		{ WM_EXITSIZEMOVE,	"WM_EXITSIZEMOVE" },
		{ WM_GETMINMAXINFO,	"WM_GETMINMAXINFO" },
		{ WM_GETTEXT,		"WM_GETTEXT" },
		{ WM_KILLFOCUS,		"WM_KILLFOCUS" },
		{ WM_INITMENU,		"WM_INITMENU" },
		{ WM_INITMENUPOPUP,	"WM_INITMENUPOPUP" },
		{ WM_KEYDOWN,		"WM_KEYDOWN" },
		{ WM_KEYUP,		"WM_KEYUP" },
		{ WM_MENUSELECT,	"WM_MENUSELECT" },
		{ WM_MOUSEACTIVATE,	"WM_MOUSEACTIVATE" },
		{ WM_MOVE,		"WM_MOVE" },
		{ WM_MOVING,		"WM_MOVING" },
		{ WM_NCACTIVATE,	"WM_NCACTIVATE" },
		{ WM_NCCALCSIZE,	"WM_NCCALCSIZE" },
		{ WM_NCCREATE,		"WM_NCCREATE" },
		{ WM_NCHITTEST,		"WM_NCHITTEST" },
		{ WM_NCLBUTTONDOWN,	"WM_NCLBUTTONDOWN" },
		{ WM_NCMOUSEMOVE,	"WM_NCMOUSEMOVE" },
		{ WM_NCPAINT,		"WM_NCPAINT" },
		{ WM_PAINT,		"WM_PAINT" },
		{ WM_PARENTNOTIFY,	"WM_PARENTNOTIFY" },
		{ WM_QUERYNEWPALETTE,	"WM_QUERYNEWPALETTE" },
		{ WM_SETCURSOR,		"WM_SETCURSOR" },
		{ WM_SETFOCUS,		"WM_SETFOCUS" },
		{ WM_SETTEXT,		"WM_SETTEXT" },
		{ WM_SHOWWINDOW,	"WM_SHOWWINDOW" },
		{ WM_SIZE,		"WM_SIZE" },
		{ WM_SIZING,		"WM_SIZING" },
		{ WM_SYSCOMMAND,	"WM_SYSCOMMAND" },
		{ WM_WINDOWPOSCHANGED,	"WM_WINDOWPOSCHANGED" },
		{ WM_WINDOWPOSCHANGING,	"WM_WINDOWPOSCHANGING" },
	};
	size_t n;
	static char temp[20];

	for (n = 0; n < TABLESIZE(table); n++) {
		if (table[n].code == code) {
			return table[n].name;
		}
	}
	sprintf(temp, "%#x", code);
	return temp;
}

static char *
syscommand2s(unsigned code)
{
	static struct {
		WORD code;
		char *name;
	} table[] = {
		{ SC_SIZE,		"SC_SIZE" },
		{ SC_MOVE,		"SC_MOVE" },
		{ SC_MINIMIZE,		"SC_MINIMIZE" },
		{ SC_MAXIMIZE,		"SC_MAXIMIZE" },
		{ SC_NEXTWINDOW,	"SC_NEXTWINDOW" },
		{ SC_PREVWINDOW,	"SC_PREVWINDOW" },
		{ SC_CLOSE,		"SC_CLOSE" },
		{ SC_VSCROLL,		"SC_VSCROLL" },
		{ SC_HSCROLL,		"SC_HSCROLL" },
		{ SC_MOUSEMENU,		"SC_MOUSEMENU" },
		{ SC_KEYMENU,		"SC_KEYMENU" },
		{ SC_ARRANGE,		"SC_ARRANGE" },
		{ SC_RESTORE,		"SC_RESTORE" },
		{ SC_TASKLIST,		"SC_TASKLIST" },
		{ SC_SCREENSAVE,	"SC_SCREENSAVE" },
		{ SC_HOTKEY,		"SC_HOTKEY" },
	};
	size_t n;
	static char temp[80];
	unsigned remainder = code & 0xf;

	for (n = 0; n < TABLESIZE(table); n++) {
		if (table[n].code == (code - remainder)) {
			sprintf(temp, "%s+%X", table[n].name, remainder);
			return temp;
		}
	}
	sprintf(temp, "%#x", code);
	return temp;
}

static void
TraceWindowRect(HWND hwnd)
{
	RECT wrect;
	int rows;
	int cols;
	GetWindowRect(hwnd, &wrect);
	cols = RectToCols(wrect);
	rows = RectToRows(wrect);
	TRACE(("... (%3d,%3d) (%3d,%3d), window is %dx%d cells (%dx%d pixels)%s\n",
		wrect.top, wrect.left,
		wrect.bottom, wrect.right,
		rows,
		cols,
		RowToPixel(rows),
		ColToPixel(cols),
		IsZoomed(hwnd) ? " zoomed" : ""
		))
}

static void
TraceClientRect(HWND hwnd)
{
	RECT crect;
	int rows;
	int cols;
	GetClientRect(hwnd, &crect);
	cols = RectToCols(crect);
	rows = RectToRows(crect);
	TRACE(("... (%3d,%3d) (%3d,%3d), client is %dx%d cells (%dx%d pixels)\n",
		crect.top, crect.left,
		crect.bottom, crect.right,
		rows,
		cols,
		RowToPixel(rows),
		ColToPixel(cols)
		))
}
#else
#define TraceWindowRect(hwnd) /* nothing */
#define TraceClientRect(hwnd) /* nothing */
#endif

static HBRUSH
Background(HDC hdc)
{
	TRACE(("Background %#06x\n", GetBkColor(hdc)))
	return CreateSolidBrush(GetBkColor(hdc));
}

void
gui_resize(int cols, int rows)
{
	static int level;
	RECT crect;
	RECT wrect;
	int main_wide;
	int main_high;
	int text_wide;
	int text_high;

	level++;
	TRACE(("gui_resize(%d x %d) begin %d\n", rows, cols, level))
	TraceWindowRect(cur_win->main_hwnd);
	TraceClientRect(cur_win->main_hwnd);

	GetClientRect(cur_win->main_hwnd, &crect);
	GetWindowRect(cur_win->main_hwnd, &wrect);

	text_wide = ColToPixel(cols);
	text_high = RowToPixel(rows);
	wrect.right  += text_wide - crect.right;
	wrect.bottom += text_high - crect.bottom;
#if FIXME_POSCHANGING
	main_wide = text_wide + (2 * cur_win->x_border) + SbWidth;
	main_high = text_high + (2 * cur_win->y_border) + cur_win->y_titles;
#else
	main_wide = wrect.right - wrect.left + SbWidth;
	main_high = wrect.bottom - wrect.top;
#endif

	TRACE(("... gui_resize -> (%d,%d) (%d,%d) main %dx%d, text %dx%d\n",
		wrect.top,
		wrect.left,
		wrect.bottom,
		wrect.right,
		main_high,
		main_wide,
		text_high,
		text_wide))

	MoveWindow(cur_win->main_hwnd,
		wrect.left,
		wrect.top,
		main_wide,
		main_high,
		TRUE);
	MoveWindow(cur_win->text_hwnd,
		0,
		0,
		text_wide,
		text_high,
		TRUE);

	TraceWindowRect(cur_win->main_hwnd);
	TraceClientRect(cur_win->main_hwnd);
#if OPT_SCROLLBARS
	update_scrollbar_sizes();
#endif
	TRACE(("... gui_resize finish %d\n", level))
	level--;
}

static int
AdjustedHeight(int high)
{
	int	extra = cur_win->y_titles + (2 * cur_win->y_border);
	int	rows;
	if (high > cur_win->xy_limit.bottom)
		high = cur_win->xy_limit.bottom;
	rows = (high - extra) / nLineHeight;
	if (rows < MIN_ROWS)
		rows = MIN_ROWS;
	return (rows * nLineHeight) + extra;
}

static int
AdjustedWidth(int wide)
{
	int	extra = SbWidth + (2 * cur_win->x_border);
	int	cols;
	if (wide > cur_win->xy_limit.right)
		wide = cur_win->xy_limit.right;
	cols = (wide - extra) / nCharWidth;
	if (cols < MIN_COLS)
		cols = MIN_COLS;
	return (cols * nCharWidth) + extra;
}

#if FIXME_POSCHANGING
static int
AdjustPosChanging(HWND hwnd, WINDOWPOS *pos)
{
	if (!(pos->flags & SWP_NOSIZE)) {
		int wide = AdjustedWidth(pos->cx);
		int high = AdjustedHeight(pos->cy);

		TRACE(("...%s position %d,%d, resize from %d,%d to %d,%d\n",
			IsZoomed(hwnd) ? " zoomed" : "",
			pos->y,  pos->x,
			pos->cy, pos->cx, high, wide))
		pos->cx = wide;
		pos->cy = high;
	}
	return 0;
}
#endif

/*
 * Handle WM_SIZING, forcing the screen size to stay in multiples of a
 * character cell.
 */
static int
AdjustResizing(HWND hwnd, WPARAM fwSide, RECT *rect)
{
	int	wide = rect->right - rect->left;
	int	high = rect->bottom - rect->top;
	int	adjX = wide - AdjustedWidth(wide);
	int	adjY = high - AdjustedHeight(high);

	TRACE(("AdjustResizing now (%d,%d) (%d,%d) (%dx%d pixels)\n",
		rect->top, rect->left, rect->bottom, rect->right,
		rect->bottom - rect->top,
		rect->right - rect->left))

	TraceWindowRect(hwnd);
	TraceClientRect(hwnd);

	if (fwSide == WMSZ_LEFT
	 || fwSide == WMSZ_TOPLEFT
	 || fwSide == WMSZ_BOTTOMLEFT)
		rect->left += adjX;
	else
	if (fwSide == WMSZ_RIGHT
	 || fwSide == WMSZ_TOPRIGHT
	 || fwSide == WMSZ_BOTTOMRIGHT)
		rect->right -= adjX;

	if (fwSide == WMSZ_TOP
	 || fwSide == WMSZ_TOPLEFT
	 || fwSide == WMSZ_TOPRIGHT)
		rect->top += adjY;
	else
	if (fwSide == WMSZ_BOTTOM
	 || fwSide == WMSZ_BOTTOMLEFT
	 || fwSide == WMSZ_BOTTOMRIGHT)
		rect->bottom -= adjY;

	TRACE(("... AdjustResizing (%d,%d) (%d,%d) adjY:%d, adjX:%d\n",
		rect->top, rect->left, rect->bottom, rect->right,
		adjY, adjX))

	return TRUE;
}

static void
ResizeClient()
{
	static int level;
	int h, w;
	RECT crect;

	if (cur_win->closed) {
		TRACE(("ResizeClient ignored (closed)\n"))
		return;
	}

	level++;
	TRACE(("ResizeClient begin %d, currently %dx%d\n", level, term.rows, term.cols))
	TraceWindowRect(cur_win->main_hwnd);
	TraceClientRect(cur_win->main_hwnd);
	GetClientRect(cur_win->main_hwnd, &crect);

	h = RectToRows(crect);
	w = RectToCols(crect);

	/*
	 * The WM_WINDOWPOSCHANGING message is supposed to allow modification
	 * to keep a window in bounds.  But it doesn't work.  This does (by
	 * forcing the calls on MoveWindow to have a "good" value).
	 */
	if (h < MIN_ROWS)
		h = MIN_ROWS;

	if (w < MIN_COLS)
		w = MIN_COLS;

	if ((h > 1 && h != term.rows) || (w > 1 && w != term.cols)) {
		TRACE(("...ResizeClient %dx%d\n", h, w))
		vile_resizing = TRUE;
		newscreensize(h, w);
		SetRows(h);
		SetCols(w);
#if OPT_SCROLLBARS
		if (check_scrollbar_allocs() == TRUE) /* no allocation failure */
			update_scrollbar_sizes();
#endif
		vile_resizing = FALSE;
		TRACE(("...ResizeClient %dx%d\n", h, w))
	} else {
		TRACE(("ResizeClient ignored (h=%d, w=%d, vs %d x %d)\n",
			h, w, term.rows, term.cols));
	}

	gui_resize(w, h);
	TRACE(("...ResizeClient finish %d\n", level))
	level--;
}

#define LTGRAY_COLOR 140
#define NORMAL_COLOR 180
#define BRIGHT_COLOR 255

static COLORREF
color_of (int code)
{
	int red = 0, green = 0, blue = 0;
	COLORREF result = 0;
	if (code & 1)
		red = NORMAL_COLOR;
	if (code & 2)
		green = NORMAL_COLOR;
	if (code & 4)
		blue = NORMAL_COLOR;
	if (code & 8) {
		if (red)   red   = BRIGHT_COLOR;
		if (green) green = BRIGHT_COLOR;
		if (blue)  blue  = BRIGHT_COLOR;
		if (code == 8) {
			red   = LTGRAY_COLOR;
			green = LTGRAY_COLOR;
			blue  = LTGRAY_COLOR;
		}
	}
	return PALETTERGB(red, green, blue);
}

static void
attr_to_colors(VIDEO_ATTR attr, int *fcolor, int *bcolor)
{
	*fcolor = nfcolor;
	*bcolor = nbcolor;
	ninvert = FALSE;

	if (attr) {
		if (attr & VASPCOL)
			*fcolor = (attr & (NCOLORS - 1));
		else if (attr == VABOLD)
			*fcolor |= FOREGROUND_INTENSITY;
		else if (attr == VAITAL)
			*bcolor |= BACKGROUND_INTENSITY;
		else if (attr & VACOLOR)
			*fcolor = ((VCOLORNUM(attr)) & (NCOLORS - 1));

		if (attr & (VASEL|VAREV)) {  /* reverse video? */
			int temp = *bcolor;
			*bcolor = *fcolor;
			*fcolor = temp;
			ninvert = TRUE;
		}
	}
}

static void
nt_set_colors (HDC hdc, VIDEO_ATTR attr)
{
	int fcolor;
	int bcolor;

	attr_to_colors(attr, &fcolor, &bcolor);

	if (fcolor < 0)
		fcolor = ninvert
			? default_bcolor
			: default_fcolor;
	else
		fcolor = color_of(fcolor);

	if (bcolor < 0)
		bcolor = ninvert
			? default_fcolor
			: default_bcolor;
	else
		bcolor = color_of(bcolor);

	SetTextColor(hdc, fcolor);
	SetBkColor(hdc,   bcolor);
}

#if OPT_ICURSOR
static void nticursor(int cmode)
{
}
#endif

static int fhide_cursor(void)
{
	TRACE(("fhide_cursor pos %#lx,%#lx (visible:%d, exists:%d)\n", ttrow, ttcol, caret_visible, caret_exists))
	if(caret_visible) {
		HideCaret(cur_win->text_hwnd);
		caret_visible = 0;
	}
	if(caret_exists) {
		DestroyCaret();
		caret_exists = 0;
	}
	return 0;
}

static void fshow_cursor(void)
{
	int x, y;
	POINT z;

	if (caret_disabled		/* reject display during font-dialog */
	 || ttrow > term.rows		/* reject bogus position in init */
	 || ttcol > term.cols)
		return;

	TRACE(("fshow_cursor pos %#lx,%#lx (visible:%d, exists:%d)\n", ttrow, ttcol, caret_visible, caret_exists))
	x = ColToPixel(ttcol) + 1;
	y = RowToPixel(ttrow) + 1;
	if (caret_exists) {
		GetCaretPos(&z);
		if (x != z.x
		 || y != z.y)
			fhide_cursor();
	}

	if (!caret_exists) {
		TRACE(("...CreateCaret(%d,%d)\n", ttrow, ttcol))
		CreateCaret(cur_win->text_hwnd, (HBITMAP) 0, nCharWidth, nLineHeight);
		caret_exists = 1;
		SetCaretPos(x, y);
		ShowCaret(cur_win->text_hwnd);
		caret_visible = 1;
	}
}

static void
get_borders(void)
{
	SystemParametersInfo(SPI_GETWORKAREA, 0, &cur_win->xy_limit, 0);
	TRACE(("WORKAREA: %d,%d %d,%d\n",
		cur_win->xy_limit.top,
		cur_win->xy_limit.left,
		cur_win->xy_limit.right,
		cur_win->xy_limit.bottom))
	cur_win->x_border = GetSystemMetrics(SM_CXSIZEFRAME);
	cur_win->y_border = GetSystemMetrics(SM_CYSIZEFRAME);
	cur_win->y_titles = GetSystemMetrics(SM_CYCAPTION);
	TRACE(("X border: %d, Y border: %d\n", cur_win->x_border, cur_win->y_border))
	TRACE(("CYFRAME:   %d\n", GetSystemMetrics(SM_CYFRAME)))
	TRACE(("CYCAPTION: %d\n", cur_win->y_titles))
}

/* Notes: lpntm is a pointer to a TEXTMETRIC struct if FontType does not
 * have TRUETYPE_FONTTYPE set, but we only need the tmPitchAndFamily member,
 * which has the same alignment as in NEWTEXTMETRIC.
 */
static int CALLBACK
enumerate_fonts(
	ENUMLOGFONT *lpelf,
	NEWTEXTMETRIC *lpntm,
	DWORD FontType,
	LPARAM lParam)
{
	int code = 2;
	LOGFONT *src = &(lpelf->elfLogFont);
	LOGFONT *dst = ((LOGFONT *)lParam);

	if ((src->lfPitchAndFamily & 3) != FIXED_PITCH) {
		code = 1;
	} else {
		*dst = *src;
		if (src->lfCharSet == ANSI_CHARSET) {
			code = 0;
			TRACE(("Found good font:%s\n", lpelf->elfFullName))
		}
		TRACE(("Found ok font:%s\n", lpelf->elfFullName))
		TRACE(("Pitch/Family:   %#x\n", dst->lfPitchAndFamily))
	}

	return code;
}

static int is_fixed_pitch(HFONT font)
{
	BOOL		ok;
	HDC             hDC;
	TEXTMETRIC      metrics;

	hDC = GetDC(cur_win->text_hwnd);
	SelectObject(hDC, font);
	ok = GetTextMetrics(hDC, &metrics);
	ReleaseDC(cur_win->text_hwnd, hDC);

	if (ok)
		ok = ((metrics.tmPitchAndFamily & TMPF_FIXED_PITCH) == 0);

	TRACE(("is_fixed_pitch: %d\n",  ok))
	TRACE(("Ave Text width: %d\n",  metrics.tmAveCharWidth))
	TRACE(("Max Text width: %d\n",  metrics.tmMaxCharWidth))
	TRACE(("Pitch/Family:   %#x\n", metrics.tmPitchAndFamily))

	return (ok);
}

static int new_font(LOGFONT *lf)
{
	HFONT font = CreateFontIndirect(lf);

	if (font != 0) {
		if (is_fixed_pitch(font)) {
			DeleteObject(vile_font);
			vile_font = font;
			TRACE(("created new font\n"))
			return TRUE;
		}
		else
		    DeleteObject(font);
	}
	return FALSE;
}

static void get_font(LOGFONT *lf)
{
	HDC             hDC;

	vile_font = GetStockObject(MY_FONT);
	hDC = GetDC(cur_win->text_hwnd);
	if (EnumFontFamilies(hDC, NULL, enumerate_fonts, (LPARAM) lf) <= 0)
	{
		TRACE(("Creating Pitch/Family: %#x\n", lf->lfPitchAndFamily))
		new_font(lf);
	}
	ReleaseDC(cur_win->text_hwnd, hDC);
}

static void use_font(HFONT my_font, BOOL resizable)
{
	HDC             hDC;
	TEXTMETRIC      textmetric;
	RECT		crect;

	hDC = GetDC(cur_win->text_hwnd);
	SelectObject(hDC, my_font);
	GetTextMetrics(hDC, &textmetric);
	ReleaseDC(cur_win->text_hwnd, hDC);

	TRACE(("Text height:    %d\n",  textmetric.tmHeight))
	TRACE(("Ave Text width: %d\n",  textmetric.tmAveCharWidth))
	TRACE(("Max Text width: %d\n",  textmetric.tmMaxCharWidth))
	TRACE(("Pitch/Family:   %#x\n", textmetric.tmPitchAndFamily))

	/*
	 * We'll use the average text-width, since some fonts (e.g., Courier
	 * New) have a bogus max text-width.
	 */
	nLineHeight = textmetric.tmExternalLeading + textmetric.tmHeight;
	nCharWidth  = textmetric.tmAveCharWidth;
	get_borders();

	if (resizable) {
		GetClientRect(cur_win->main_hwnd, &crect);

		SetCols(RectToCols(crect));
		SetRows(RectToRows(crect));
	}

	gui_resize(term.cols, term.rows);
}

static void set_font(void)
{
	HDC		hDC;
	CHOOSEFONT	choose;

	TRACE(("set_font\n"))
	fhide_cursor();
	caret_disabled = TRUE;
	memset(&choose, 0, sizeof(choose));
	choose.lStructSize = sizeof(choose);
	choose.hwndOwner   = cur_win->text_hwnd;
	choose.Flags       = CF_SCREENFONTS
			   | CF_FIXEDPITCHONLY
			   | CF_FORCEFONTEXIST
			   | CF_NOSCRIPTSEL
			   | CF_INITTOLOGFONTSTRUCT;
	choose.lpLogFont   = &vile_logfont;

	hDC = GetDC(cur_win->text_hwnd);
	SelectObject(hDC, GetMyFont());
	GetTextFace(hDC, sizeof(vile_logfont.lfFaceName), vile_logfont.lfFaceName);
	ReleaseDC(cur_win->text_hwnd, hDC);

	vile_logfont.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
	vile_logfont.lfCharSet = ANSI_CHARSET;
	TRACE(("LOGFONT Facename:%s\n", vile_logfont.lfFaceName))

	if (ChooseFont(&choose)) {
		TRACE(("ChooseFont '%s'\n", vile_logfont.lfFaceName))
		if (new_font(&vile_logfont)) {
			int saverow = ttrow;
			int savecol = ttcol;
			mlwrite("[Set font to %s]", vile_logfont.lfFaceName);
			movecursor(saverow, savecol);
			use_font(vile_font, FALSE);
			vile_refresh(FALSE,0);
		} else {
			mlforce("[Cannot create font]");
		}
	} else {
		mlforce("[No font selected]");
	}

	caret_disabled = FALSE;
	fshow_cursor();
	TRACE(("...set_font, LOGFONT Facename:%s\n", vile_logfont.lfFaceName))
}

static int
last_w32_error(int use_msg_box)
{
    if (use_msg_box)
        disp_win32_error(W32_SYS_ERROR, winvile_hwnd());
    else
    {
        char *msg = NULL;

        fmt_win32_error(W32_SYS_ERROR, &msg, 0);
        mlforce(msg);
        LocalFree(msg);
    }
    return (FALSE);
}

/*
 * Set font from string specification.  See the function parse_font_str()
 * in file w32misc.c for acceptable font string syntax.
 *
 * Prerequistes before calling this function:
 *
 *    winvile's windows and default font (vile_font) created.
 *
 * Returns: T -> all is well, F -> failure.
 */
int
ntwinio_font_frm_str(
    const char *fontstr,
    int        use_mb    /* Boolean, T -> errors reported via MessageBox.
                          *          F -> errors reported via message line.
                          */
                     )
{
    int             face_specified;
    HDC             hdc;
    HFONT           hfont;
    HWND            hwnd;
    LOGFONT         logfont;
    char            *msg, font_mapper_face[LF_FACESIZE + 1];
    TEXTMETRIC      metrics;
    FONTSTR_OPTIONS str_rslts;

    if (! parse_font_str(fontstr, &str_rslts))
    {
        msg = "Font syntax invalid";
        if (use_mb)
            MessageBox(winvile_hwnd(), msg, prognam, MB_ICONSTOP|MB_OK);
        else
            mlforce(msg);
        return (FALSE);
    }
    hwnd           = cur_win->text_hwnd;
    face_specified = (str_rslts.face[0] != '\0');
    if (! ((hdc = GetDC(hwnd)) && SelectObject(hdc, vile_font)))
    {
        if (hdc)
            ReleaseDC(hwnd, hdc);
        return (last_w32_error(use_mb));
    }
    if (! face_specified)
    {
        /* user didn't specify a face name, get current name. */

        if ((GetTextFace(hdc, sizeof(str_rslts.face), str_rslts.face)) == 0)
        {
            ReleaseDC(hwnd, hdc);
            return (last_w32_error(use_mb));
        }
    }

    /* Build up LOGFONT data structure. */
    memset(&logfont, 0, sizeof(logfont));
    logfont.lfWeight = (str_rslts.bold) ? FW_BOLD : FW_NORMAL;
    logfont.lfHeight = -MulDiv(str_rslts.size,
                               GetDeviceCaps(hdc, LOGPIXELSY),
                               72);
    if (str_rslts.italic)
        logfont.lfItalic = TRUE;
    logfont.lfCharSet        = DEFAULT_CHARSET;
    logfont.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    strncpy(logfont.lfFaceName, str_rslts.face, LF_FACESIZE - 1);
    logfont.lfFaceName[LF_FACESIZE - 1] = '\0';
    if (! ((hfont = CreateFontIndirect(&logfont)) && SelectObject(hdc, hfont)))
    {
        if (hfont)
            DeleteObject(hfont);
        ReleaseDC(hwnd, hdc);
        return (last_w32_error(use_mb));
    }

    /*
     * The font mapper will substitute some other font for a font request
     * that cannot exactly be met.  Ck first to see if the face name
     * matches the user chosen face (if applicatble).
     */
    if (face_specified)
    {
        if ((GetTextFace(hdc, sizeof(font_mapper_face), font_mapper_face)) == 0)
        {
            ReleaseDC(hwnd, hdc);
            DeleteObject(hfont);
            return (last_w32_error(use_mb));
        }
        if (stricmp(font_mapper_face, str_rslts.face) != 0)
        {
            ReleaseDC(hwnd, hdc);
            DeleteObject(hfont);
            msg = "Font face unknown or size/style unsupported";
            if (use_mb)
                MessageBox(winvile_hwnd(), msg, prognam, MB_ICONSTOP|MB_OK);
            else
                mlforce(msg);
            return (FALSE);
        }
    }

    /* Next, font must be fixed pitch. */
    if (! GetTextMetrics(hdc, &metrics))
    {
        ReleaseDC(hwnd, hdc);
        DeleteObject(hfont);
        return (last_w32_error(use_mb));
    }
    if ((metrics.tmPitchAndFamily & TMPF_FIXED_PITCH) != 0)
    {
        /* Misnamed constant! */

        ReleaseDC(hwnd, hdc);
        DeleteObject(hfont);
        msg = "Font not fixed pitch";
        if (use_mb)
            MessageBox(winvile_hwnd(), msg, prognam, MB_ICONSTOP|MB_OK);
        else
            mlforce(msg);
        return (FALSE);
    }
    DeleteObject(vile_font);      /* Nuke original font     */
    ReleaseDC(hwnd, hdc);         /* finally done with this */
    vile_font = hfont;
    memcpy(&vile_logfont, &logfont, sizeof(vile_logfont));
    use_font(vile_font, FALSE);
    vile_refresh(FALSE, 0);
    return (TRUE);
}

char *
ntwinio_current_font(void)
{
    static char   *buf;
    HDC           hdc;
    HWND          hwnd;
    LONG          size;
    char          *style;

    if (! buf)
    {
        buf = castalloc(char,
                        sizeof("bold-italic") +
                        LF_FACESIZE +
                        16); /* space for delimiters and point size */
        if (! buf)
            return ("out of memory");
    }
    hwnd = cur_win->text_hwnd;
    if (! ((hdc = GetDC(hwnd)) && SelectObject(hdc, vile_font)))
    {
        char *msg = NULL;

        if (hdc)
            ReleaseDC(hwnd, hdc);

        fmt_win32_error(W32_SYS_ERROR, &msg, 0);
        return (msg);
        /* "msg" leaks here, but this code path should never be taken. */
    }
    if (vile_logfont.lfWeight == FW_BOLD && vile_logfont.lfItalic)
        style = "bold-italic";
    else if (vile_logfont.lfWeight == FW_BOLD)
        style = "bold";
    else if (vile_logfont.lfItalic)
        style = "italic";
    else
        style = NULL;
    size = MulDiv(labs(vile_logfont.lfHeight),
                  72,
                  GetDeviceCaps(hdc, LOGPIXELSY));
    sprintf(buf,
            "%s,%ld%s%s",
            vile_logfont.lfFaceName,
            size,
            (style) ? "," : "",
            (style) ? style : "");
    ReleaseDC(hwnd, hdc);
    return (buf);
}

#if OPT_TITLE
static void
nttitle(char *title)		/* set the current window title */
{
	SetWindowText(winvile_hwnd(), title);
}
#endif

static void
scflush(void)
{
	if (cur_pos && !vile_resizing) {
		HDC hdc;

		TRACE(("PUTC:flush %2d (%2d,%2d) (%.*s)\n", cur_pos, crow,ccol, cur_pos, &CELL_TEXT(crow,ccol)))

		hdc = GetDC(cur_win->text_hwnd);
		SelectObject(hdc, GetMyFont());
		nt_set_colors(hdc, cur_atr);

		TextOut(hdc,
			ColToPixel(ccol),
			RowToPixel(crow),
			&CELL_TEXT(crow,ccol), cur_pos);

		ReleaseDC(cur_win->text_hwnd, hdc);
	}
	ccol = ccol + cur_pos;
	cur_pos = 0;
}

#if OPT_COLOR
static void
ntfcol(int color)		/* set the current output color */
{
	scflush();
	nfcolor = color;
}

static void
ntbcol(int color)		/* set the current background color */
{
	scflush();
	nbcolor = color;
}
#endif

static void
ntflush(void)
{
	scflush();
	SetCaretPos(ColToPixel(ccol), RowToPixel(crow));
}

static void
ntmove(int row, int col)
{
	scflush();
	crow = (short) row;
	ccol = (short) col;
}

/* erase to the end of the line */
static void
nteeol(void)
{
	HDC	hDC;
	HBRUSH	brush;
	RECT	rect;
	int	x;

	scflush();

	TRACE(("NTEEOL %d,%d, atr %#x\n", crow, ccol, cur_atr))
	for (x = ccol; x < term.cols; x++) {
		CELL_TEXT(crow,x) = ' ';
		CELL_ATTR(crow,x) = cur_atr;
	}

	GetClientRect(cur_win->text_hwnd, &rect);
	rect.left   = ColToPixel(ccol);
	rect.top    = RowToPixel(crow);
	rect.right  = ColToPixel(term.cols);
	rect.bottom = RowToPixel(crow+1);

	hDC = GetDC(cur_win->text_hwnd);
	nt_set_colors(hDC, cur_atr);
	brush = Background(hDC);
	FillRect(hDC, &rect, brush);
	DeleteObject(brush);
	ReleaseDC(cur_win->text_hwnd, hDC);
}

/*
 * vile very rarely generates any of the ASCII printing control characters
 * except for a few hand coded routines but we have to support them anyway.
 */

/* put a character at the current position in the current colors */
static void
ntputc(int ch)
{
	/* This is an optimization for the most common case. */
	if (ch >= ' ') {
		CELL_TEXT(crow,ccol+cur_pos) = (char) ch;
		CELL_ATTR(crow,ccol+cur_pos) = cur_atr;
		cur_pos++;
	} else {

		switch (ch) {

		case '\b':
			scflush();
			if (ccol)
				ccol--;
			break;

		case '\a':
			ntbeep();
			break;

		case '\t':
			scflush();
			do {
				CELL_TEXT(crow,ccol+cur_pos) = ' ';
				CELL_ATTR(crow,ccol+cur_pos) = cur_atr;
				cur_pos++;
			} while ((ccol + cur_pos) % 8 != 0);
			break;

		case '\r':
			scflush();
			ccol = 0;
			break;

		case '\n':
			scflush();
			if (crow < term.rows - 1)
				crow++;
			else
				ntscroll(1, 0, term.rows - 1);
			break;

		default:
			CELL_TEXT(crow,ccol+cur_pos) = (char) ch;
			CELL_ATTR(crow,ccol+cur_pos) = cur_atr;
			cur_pos++;
			break;
		}
	}
}

static void
nteeop(void)
{
	HDC	hDC;
	HBRUSH	brush;
	RECT	rect;
	int	y, x, x0;

	scflush();

	x0 = ccol;
	TRACE(("NTEEOP %d,%d, atr %#x\n", crow, ccol, cur_atr))
	for (y = crow; y < term.rows; y++) {
		for (x = 0; x < term.cols; x++) {
			CELL_TEXT(y,x) = ' ';
			CELL_ATTR(y,x) = cur_atr;
		}
		x0 = 0;
	}

	rect.left   = ColToPixel(ccol);
	rect.top    = RowToPixel(crow);
	rect.right  = ColToPixel(term.cols);
	rect.bottom = RowToPixel(term.rows);

	if (!vile_resizing) {
		hDC = GetDC(cur_win->text_hwnd);
		nt_set_colors(hDC, cur_atr);
		brush = Background(hDC);
		FillRect(hDC, &rect, brush);
		DeleteObject(brush);
		ReleaseDC(cur_win->text_hwnd, hDC);
	}
}

static void
ntrev(UINT reverse)		/* change reverse video state */
{
	scflush();
	cur_atr = (VIDEO_ATTR) reverse;
}

static int
ntcres(const char *res)		/* change screen resolution */
{
	scflush();
	return 0;
}

#if	OPT_FLASH
static void
flash_display()
{
	RECT	rect;
	HDC	hDC;

	GetClientRect(cur_win->text_hwnd, &rect);
	hDC = GetDC(cur_win->text_hwnd);
	InvertRect(hDC, &rect);
	Sleep(100);
	InvertRect(hDC, &rect);
	ReleaseDC(cur_win->text_hwnd, hDC);
}
#endif

static void
ntbeep(void)
{
#if	OPT_FLASH
	if (global_g_val(GMDFLASH)) {
		flash_display();
		return;
	}
#endif
	MessageBeep(0xffffffff);
}

static void
ntopen(void)
{
	TRACE(("ntopen\n"))

	set_colors(NCOLORS);
	set_palette(initpalettestr);
}

static int old_title_set = 0;
static char old_title[256];
static int orig_title_set = 0;
static char orig_title[256];

static void
ntclose(void)
{
	TRACE(("ntclose\n"))

	scflush();
	ntmove(term.rows - 1, 0);
	nteeol();
	ntflush();
}

static void
ntkopen(void)	/* open the keyboard */
{
}

static void
ntkclose(void)	/* close the keyboard */
{
}

static struct keyxlate_struct {
	int	windows;
	int	vile;
} keyxlate[] = {
	{ VK_NEXT,	KEY_Next },
	{ VK_PRIOR,	KEY_Prior },
	{ VK_END,	KEY_End },
	{ VK_HOME,	KEY_Home },
	{ VK_LEFT,	KEY_Left },
	{ VK_RIGHT,	KEY_Right },
	{ VK_UP,	KEY_Up },
	{ VK_DOWN,	KEY_Down },
	{ VK_INSERT,	KEY_Insert },
	{ VK_DELETE,	KEY_Delete },
	{ VK_HELP,	KEY_Help },
	{ VK_SELECT,	KEY_Select },
#if 0
	/* Merely pressing the Alt key generates a VK_MENU key event. */
	{ VK_MENU,	KEY_Menu },
#endif
	{ VK_F1,	KEY_F1 },
	{ VK_F2,	KEY_F2 },
	{ VK_F3,	KEY_F3 },
	{ VK_F4,	KEY_F4 },
	{ VK_F5,	KEY_F5 },
	{ VK_F6,	KEY_F6 },
	{ VK_F7,	KEY_F7 },
	{ VK_F8,	KEY_F8 },
	{ VK_F9,	KEY_F9 },
	{ VK_F10,	KEY_F10 },
	{ VK_F11,	KEY_F11 },
	{ VK_F12,	KEY_F12 },
	{ VK_F13,	KEY_F13 },
	{ VK_F14,	KEY_F14 },
	{ VK_F15,	KEY_F15 },
	{ VK_F16,	KEY_F16 },
	{ VK_F17,	KEY_F17 },
	{ VK_F18,	KEY_F18 },
	{ VK_F19,	KEY_F19 },
	{ VK_F20,	KEY_F20 },
	/* Allow ^-6 to invoke the alternate-buffer command, a la Unix.  */
	{ '6',		'6' },
};

static int savedChar;
static int saveCount = 0;

static int
decode_key_event(KEY_EVENT_RECORD *irp)
{
    int i, key;
    struct keyxlate_struct *keyp;

    if ((key = (unsigned char) irp->uChar.AsciiChar) != 0)
        return key;

#if VILE_OLE
    if (redirect_keys &&
           oleauto_redirected_key(irp->wVirtualKeyCode, irp->dwControlKeyState))
    {
        return (KYREDIR);  /* Key sent to another window. */
    }
#endif

    for (i = 0, keyp = keyxlate; i < TABLESIZE(keyxlate); i++, keyp++)
    {
        if (keyp->windows == irp->wVirtualKeyCode)
        {
	    DWORD state = irp->dwControlKeyState;

	    /*
	     * There are two special keys that we must deal with here on an
	     * ad hoc basis:  ALT+F4 and SHIFT+6 .  The former should _never_
	     * be remapped by any user (nor messed with by vile) and the
	     * latter is actually '^' -- leave it alone as well.
	     */
	    if ((keyp->windows == VK_F4 &&
			(state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)))
			        ||
		(keyp->windows == '6' && (state & SHIFT_PRESSED)))
	    {
		break;
	    }

	    /*
	     * If this key is modified in some way, we'll prefer to use the
	     * Win32 definition.  But only for the modifiers that we
	     * recognize.  Specifically, we don't care about ENHANCED_KEY,
	     * since we already have portable pageup/pagedown and arrow key
	     * bindings that would be lost if we used the Win32-only
	     * definition.
	     */
	    if (state &
		(LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED
		| LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED
		| SHIFT_PRESSED))
            {
                key = W32_KEY | keyp->windows;
                if (state & (LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED))
                    key |= W32_CTRL;
                if (state & (LEFT_ALT_PRESSED|RIGHT_ALT_PRESSED))
                    key |= W32_ALT;
                if (state & SHIFT_PRESSED)
                    key |= W32_SHIFT;
            }
            else
                key = keyp->vile;
            TRACE(("... %#x -> %#x\n", irp->wVirtualKeyCode, key))
            break;
        }
    }
    if (key == 0)
        return (NOKYMAP);

    return key;
}

static int
get_keyboard_state(void)
{
	int result = 0;

	if (GetKeyState(VK_CONTROL) < 0)
		result |= (LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED);
	if (GetKeyState(VK_MENU) < 0)
		result |= (LEFT_ALT_PRESSED|RIGHT_ALT_PRESSED);
	if (GetKeyState(VK_SHIFT) < 0)
		result |= SHIFT_PRESSED;
	return result;
}

static int
ntgetch(void)
{
	int buttondown = FALSE;
	MSG msg;
	POINTS points;
	POINT first;
	int have_focus = 0;
	int result = 0;
	DWORD dword;
	KEY_EVENT_RECORD ker;

	if (saveCount > 0) {
		saveCount--;
		return savedChar;
	}

	vile_in_getfkey = 1;
	while(! result) {
		if(GetFocus() == cur_win->main_hwnd) {
			if(! have_focus) {
				have_focus = 1;
				fshow_cursor();
			}
		} else {
			if(have_focus) {
				have_focus = 0;
				fhide_cursor();
			}
		}
		if(GetMessage(&msg, (HWND)0, 0, 0) != TRUE) {
			PostQuitMessage(1);
			TRACE(("GETC:no message\n"))
			quit(TRUE,1);
		}

		if (TranslateAccelerator(cur_win->main_hwnd, hAccTable, &msg)) {
			TRACE(("GETC:no accelerator\n"))
			continue;
		}

		TranslateMessage(&msg);

		switch(msg.message) {
		case WM_DESTROY:
			TRACE(("GETC:DESTROY\n"))
			PostQuitMessage(0);
			continue;

		case WM_CHAR:
			TRACE(("GETC:CHAR:%#x\n", msg.wParam))
			result = msg.wParam;
			if (result == ESC) {
				sel_release();
				(void)update(TRUE);
			}
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			ker.uChar.AsciiChar = 0;
			ker.wVirtualKeyCode = (SHORT) msg.wParam;
			ker.dwControlKeyState = get_keyboard_state();
			result = decode_key_event(&ker);
			TRACE(("GETC:%sKEYDOWN:%#x ->%#x\n",
				(msg.message == WM_SYSKEYDOWN) ? "SYS" : "",
				msg.wParam, result))
			if (result == NOKYMAP) {
				DispatchMessage(&msg);
				result = 0;
#ifdef VILE_OLE
			} else if (result == KYREDIR) {
				result = 0; /* keystroke sent elsewhere */
#endif
			} else if (result == (int) msg.wParam) {
				result = 0; /* we'll get a WM_CHAR next */
			}
			break;

		case WM_LBUTTONDOWN:
			TRACE(("GETC:LBUTTONDOWN %s\n", which_window(msg.hwnd)))
			if (msg.hwnd == cur_win->text_hwnd) {
				dword = GetMessagePos();
				points = MAKEPOINTS(dword);
				POINTSTOPOINT(first, points);
				ScreenToClient(cur_win->main_hwnd, &first);
				first.x /= nCharWidth;
				first.y /= nLineHeight;
				TRACE(("GETC:setcursor(%d, %d)\n", first.y, first.x))
				if (setcursor(first.y, first.x)) {
					fhide_cursor();
					(void)sel_begin();
					(void)update(TRUE);
					buttondown = TRUE;
				}
			} else {
				DispatchMessage(&msg);
			}
			break;

		case WM_RBUTTONDOWN:
			TRACE(("GETC:RBUTTONDOWN %s\n", which_window(msg.hwnd)))
			if (msg.hwnd == cur_win->text_hwnd) {
				static HMENU hmenu;
				POINT	     point;

				if (hmenu == NULL) {
					hmenu = LoadMenu(vile_hinstance,
							 "WinvilePopMenu");
					hmenu = GetSubMenu(hmenu, 0);
				}
				point.x = LOWORD(msg.lParam);
				point.y = HIWORD(msg.lParam);
				ClientToScreen(msg.hwnd, &point);
				TrackPopupMenu(hmenu,
					       0,
					       point.x,
					       point.y,
					       0,
					       msg.hwnd,
					       NULL) ;
			} else {
				DispatchMessage(&msg);
			}
			break;

		case WM_COMMAND:
		    if (LOWORD(msg.wParam) == IDM_OPEN) {
			winopen(0, 0);

			/*
			 * workaround repaint bug--sometimes cursor hangs in
			 * mini-buffer and screen is not updated.
			 */
			update(FALSE);
		    }
		    else
			DispatchMessage(&msg);
		    break;

		case WM_MOUSEMOVE:
			if (buttondown) {
				int x = PixelToCol(LOWORD(msg.lParam));
				int y = PixelToRow(HIWORD(msg.lParam));

				TRACE(("GETC:MOUSEMOVE (%d,%d)%s\n",
					x, y, buttondown ? " selecting" : ""));

				fhide_cursor();
				if (!setcursor(y, x))
					break;
				if (get_keyboard_state()
				 & (LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED))
					(void)sel_setshape(RECTANGLE);
				if (!sel_extend(TRUE, TRUE))
					break;
				(void)update(TRUE);
			}
			break;

		case WM_LBUTTONUP:
			TRACE(("GETC:LBUTTONUP %s\n", which_window(msg.hwnd)))
			if (msg.hwnd == cur_win->text_hwnd) {
				fhide_cursor();
				if (buttondown) {
					sel_yank(0);
					buttondown = FALSE;
				}
				(void)update(TRUE);
				fshow_cursor();
			} else {
				DispatchMessage(&msg);
			}
			break;

		default:
			TRACE(("GETC:default(%s)\n", message2s(msg.message)))
		case WM_KEYUP:
		case WM_NCHITTEST:
		case WM_NCMOUSEMOVE:
		case WM_NCLBUTTONDOWN:
		case WM_PAINT:
		case WM_NCACTIVATE:
		case WM_SETCURSOR:
		case 0x118:
			DispatchMessage(&msg);
			break;
		}
	}
	fhide_cursor();
	vile_in_getfkey = 0;

	TRACE(("...ntgetch %#x\n", result))
	return result;
}

/*
 * The function `kbhit' returns true if there are *any* input records
 * available.  We need to define our own type ahead routine because
 * otherwise events which we will discard (like pressing or releasing
 * the Shift key) can block screen updates because `ntgetch' won't
 * return until a ordinary key event occurs.
 */

static int
nttypahead()
{
#if FIXME
#endif
	return 0;
}

/*
 * Move 'n' lines starting at 'from' to 'to'
 *
 * OPT_PRETTIER_SCROLL is prettier but slower -- it scrolls a line at a time
 *	instead of all at once.
 */

/* move howmany lines starting at from to to */
static void
ntscroll(int from, int to, int n)
{
	HDC     hDC;
	HBRUSH	brush;
	RECT region;
	RECT tofill;

	scflush();
	if (to == from)
		return;
#if OPT_PRETTIER_SCROLL
	if (absol(from-to) > 1) {
		ntscroll(from, (from<to) ? to-1:to+1, n);
		if (from < to)
			from = to-1;
		else
			from = to+1;
	}
#endif

	region.left   = 0;
	region.right  = (SHORT) ColToPixel(term.cols);

	if (from > to) {
		region.top    = (SHORT) RowToPixel(to);
		region.bottom = (SHORT) RowToPixel(from + n);
	} else {
		region.top    = (SHORT) RowToPixel(from);
		region.bottom = (SHORT) RowToPixel(to + n);
	}

	TRACE(("ScrollWindowEx from=%d, to=%d, n=%d  (%d,%d)/(%d,%d)\n",
		from, to, n,
		region.left, region.top,
		region.right, region.bottom));

	ScrollWindowEx(
		cur_win->text_hwnd,	/* handle of window to scroll */
		0,		/* amount of horizontal scrolling */
		RowToPixel(to-from),	/* amount of vertical scrolling */
		&region,	/* address of structure with scroll rectangle */
		&region,	/* address of structure with clip rectangle */
		(HRGN)0,	/* handle of update region */
		&tofill,	/* address of structure for update rectangle */
		SW_ERASE	/* scrolling flags */
		);
	TRACE(("fill: (%d,%d)/(%d,%d)\n",
		tofill.left, tofill.top,
		tofill.right, tofill.bottom));

	hDC = GetDC(cur_win->text_hwnd);
	nt_set_colors(hDC, cur_atr);
	brush = Background(hDC);
	FillRect(hDC, &tofill, brush);
	DeleteObject(brush);
	ReleaseDC(cur_win->text_hwnd, hDC);
}


#if OPT_SCROLLBARS
static
int check_scrollbar_allocs(void)
{
	int newmax = cur_win->rows/2;
	int oldmax = cur_win->maxscrollbars;

	TRACE(("check_scrollbar_allocs %d > %d ?\n", oldmax, newmax))
	if (newmax > oldmax) {
		GROW(cur_win->scrollbars, SBDATA, oldmax, newmax);
		cur_win->maxscrollbars = newmax;
		TRACE(("GROW scrollbars=%p, oldmax=%d, newmax=%d\n",
			cur_win->scrollbars, oldmax, newmax))
	}
	return TRUE;
}

static SBDATA
new_scrollbar(void)
{
	SBDATA result;

	result.shown    = FALSE;
	result.r.top    = -1;
	result.r.left   = -1;
	result.r.bottom = -1;
	result.r.right  = -1;

	result.w = CreateWindow(
		SCRL_CLASS,
		"scrollbar",
		WS_CHILD | SBS_VERT | WS_CLIPSIBLINGS,
		0,		/* x */
		0,		/* y */
		SbWidth,	/* width */
		1,		/* height */
		cur_win->main_hwnd,
		(HMENU)0,
		vile_hinstance,
		(LPVOID)0
		);
	return result;
}

static int
show_scrollbar(int n, int flag)
{
	if (cur_win->scrollbars[n].shown != flag) {
		ShowScrollBar(cur_win->scrollbars[n].w, SB_CTL, flag);
		cur_win->scrollbars[n].shown = flag;
		return TRUE;
	}
	return FALSE;
}

static void
set_scrollbar_range(int n, WINDOW *wp)
{
	SCROLLINFO info;
	int lnum, lcnt;

	lnum = line_no(wp->w_bufp, wp->w_dot.l);
	lnum = max(lnum, 1);

	lcnt = line_count(wp->w_bufp);
	lcnt = max(lcnt, 1) + wp->w_ntrows - 1;

	TRACE(("set_scrollbar_range(%d, %s) %d:%d\n",
		n, wp->w_bufp->b_bname, lnum, lcnt))

	info.cbSize = sizeof(info);
	info.fMask  = SIF_POS | SIF_RANGE | SIF_PAGE;
	info.nPos   = lnum - 1;
	info.nMin   = 0;
	info.nMax   = lcnt - 1;
	info.nPage  = wp->w_ntrows;
	SetScrollInfo(cur_win->scrollbars[n].w, SB_CTL, &info, TRUE);
}

/*
 * All we want to do here is to paint the gap between the real resize-grip and
 * the borders of the dummy window we're surrounding it with.  That's because
 * the resize-grip itself isn't resizable.
 */
LONG FAR PASCAL GripWndProc(
			HWND hWnd,
			UINT message,
			WPARAM wParam,
			LONG lParam)
{
	PAINTSTRUCT ps;
	HBRUSH brush;

	TRACE(("GRIP:%s\n", message2s(message)))

	switch (message) {
	case WM_PAINT:
		BeginPaint(hWnd, &ps);
		TRACE(("...painting (%d,%d) (%d,%d)\n",
			ps.rcPaint.top,
			ps.rcPaint.left,
			ps.rcPaint.bottom,
			ps.rcPaint.right))
		brush = CreateSolidBrush(GetSysColor(COLOR_SCROLLBAR));
		SelectObject(ps.hdc, brush);
		Rectangle(ps.hdc,
			ps.rcPaint.left,
			ps.rcPaint.top,
			ps.rcPaint.right,
			ps.rcPaint.bottom);
		DeleteObject(brush);
		EndPaint(hWnd, &ps);
		break;
	}
	return (DefWindowProc(hWnd, message, wParam, lParam));
}

static void
update_scrollbar_sizes(void)
{
	RECT crect;
	register WINDOW *wp;
	int i, top, left;
	int newsbcnt;
	int oldsbcnt = cur_win->nscrollbars;

	TRACE(("update_scrollbar_sizes\n"))

	i = 0;
	for_each_visible_window(wp)
		i++;
	newsbcnt = i;

	for (i = cur_win->nscrollbars+1; i <= newsbcnt; i++) {
		if (cur_win->scrollbars[i].w == NULL) {
			cur_win->scrollbars[i] = new_scrollbar();
			TRACE(("... created sb%d=%#x\n", i, cur_win->scrollbars[i].w))
		}
	}
	cur_win->nscrollbars = newsbcnt;

	/* Set sizes and positions on scrollbars and sliders */
	i = 0;
	GetClientRect(cur_win->main_hwnd, &crect);
	top = crect.top;
	left = crect.right - SbWidth;
	for_each_visible_window(wp) {
		int high = RowToPixel(wp->w_ntrows + 1);
		int wide = SbWidth;
		if (show_scrollbar(i, TRUE)
		 || cur_win->scrollbars[i].r.top    != top
		 || cur_win->scrollbars[i].r.left   != left
		 || cur_win->scrollbars[i].r.bottom != high
		 || cur_win->scrollbars[i].r.right  != wide) {
			MoveWindow(cur_win->scrollbars[i].w,
				cur_win->scrollbars[i].r.left   = left,
				cur_win->scrollbars[i].r.top    = top,
				cur_win->scrollbars[i].r.right  = wide,
				cur_win->scrollbars[i].r.bottom = high,
				TRUE);
			TRACE(("... adjusted %s to (%d,%d) (%d,%d)\n",
				which_window(cur_win->scrollbars[i].w),
				left,
				top,
				SbWidth,
				high));
		 }
		if (cur_win->nscrollbars == i+1)
			set_scrollbar_range(i, wp);
		i++;
		top += high;
	}

	while (i < oldsbcnt) {
		(void) show_scrollbar(i, FALSE);
		i++;
	}
#if FIXME_RECUR_SB
	for_each_visible_window(wp) {
		wp->w_flag &= ~WFSBAR;
		gui_update_scrollbar(wp);
	}
#endif

	if (cur_win->size_box.w == 0) {
		cur_win->size_box.w = CreateWindow(
			GRIP_CLASS,
			"sizebox",
			WS_CHILD
			| WS_VISIBLE
			| WS_CLIPSIBLINGS,
			cur_win->size_box.r.left   = left,
			cur_win->size_box.r.top    = top,
			cur_win->size_box.r.right  = SbWidth + 1,
			cur_win->size_box.r.bottom = nLineHeight,
			cur_win->main_hwnd,
			(HMENU)0,
			vile_hinstance,
			(LPVOID)0
			);
		cur_win->size_grip.w = CreateWindow(
			"SCROLLBAR",
			"sizebox",
			WS_CHILD
			| WS_VISIBLE
			| SB_CTL
			| SBS_SIZEGRIP
			| WS_CLIPSIBLINGS
			| SBS_SIZEBOXBOTTOMRIGHTALIGN,
			cur_win->size_box.r.left   = 0,
			cur_win->size_box.r.top    = 0,
			cur_win->size_box.r.right  = SbWidth,
			cur_win->size_box.r.bottom = nLineHeight,
			cur_win->size_box.w,
			(HMENU)0,
			vile_hinstance,
			(LPVOID)0
			);
		TRACE(("... made SIZEGRIP %x at %d,%d\n", cur_win->size_box.w, left, top))
	} else {
		int ok;

		if (cur_win->size_box.r.left   != left
		 || cur_win->size_box.r.top    != top
		 || cur_win->size_box.r.right  != SbWidth
		 || cur_win->size_box.r.bottom != nLineHeight) {
			ok = MoveWindow(cur_win->size_box.w,
				cur_win->size_box.r.left   = left,
				cur_win->size_box.r.top    = top,
				cur_win->size_box.r.right  = SbWidth,
				cur_win->size_box.r.bottom = nLineHeight,
				TRUE);
			TRACE(("... move SIZE_BOX %d:%x to %d,%d\n",
				ok, cur_win->size_box.w, left, top))
		}

		left = 0;
		top  = 0;
		if (cur_win->size_grip.r.left   != left
		 || cur_win->size_grip.r.top    != top
		 || cur_win->size_grip.r.right  != SbWidth
		 || cur_win->size_grip.r.bottom != nLineHeight) {
			ok = MoveWindow(cur_win->size_grip.w,
				cur_win->size_grip.r.left   = left,
				cur_win->size_grip.r.top    = top,
				cur_win->size_grip.r.right  = SbWidth,
				cur_win->size_grip.r.bottom = nLineHeight,
				TRUE);
			TRACE(("... move SIZEGRIP %d:%x to %d,%d\n",
				ok, cur_win->size_grip.w, left, top))
		}
	}
}

void
gui_update_scrollbar(WINDOW *uwp)
{
	WINDOW *wp;
	int i;

	TRACE(("gui_update_scrollbar uwp=%p %s\n", uwp, uwp->w_bufp->b_bname))
	if (dont_update_sb)
		return;

	i = 0;
	for_each_visible_window(wp) {
		TRACE(("wp=%p name='%s'\n", wp, wp->w_bufp->b_bname))
		if (wp == uwp)
			break;
		i++;
	}

	TRACE(("i=%d, nscrollbars=%d\n", i, cur_win->nscrollbars))
	if (i >= cur_win->nscrollbars || (wp->w_flag & WFSBAR)) {
		/*
		 * update_scrollbar_sizes will recursively invoke gui_update_scrollbar,
		 * but with WFSBAR disabled.
		 */
		update_scrollbar_sizes();
		return;
	}

	set_scrollbar_range(i, wp);
}

static int find_scrollbar (HWND hWnd)
{
	WINDOW *wp;
	int i = 0;

	for_each_visible_window(wp) {
		if (cur_win->scrollbars[i].w == hWnd) {
			set_curwp (wp);
			if (wp->w_bufp != curbp) {
				swbuffer(wp->w_bufp);
			}
			return i;
		}
		i++;
	}
	return -1;
}

static void handle_scrollbar (HWND hWnd, int msg, int nPos)
{
	int snum = find_scrollbar(hWnd);

	TRACE(("handle_scrollbar msg=%d, nPos=%d\n", msg, nPos))

	if (snum < 0) {
		TRACE(("...could not find window for %s\n", which_window(hWnd)))
		return;
	}

	fhide_cursor();
	switch (msg)
	{
	case SB_BOTTOM:
		TRACE(("-> SB_BOTTOM\n"))
		gotoline(FALSE, 1);
		break;
	case SB_ENDSCROLL:
		TRACE(("-> SB_ENDSCROLL\n"))
		break;
	case SB_LINEDOWN:
		TRACE(("-> SB_LINEDOWN\n"))
		mvdnwind(FALSE,1);
		break;
	case SB_LINEUP:
		TRACE(("-> SB_LINEUP\n"))
		mvupwind(FALSE,1);
		break;
	case SB_PAGEDOWN:
		TRACE(("-> SB_PAGEDOWN\n"))
		forwpage(FALSE,1);
		break;
	case SB_PAGEUP:
		TRACE(("-> SB_PAGEUP\n"))
		backpage(FALSE,1);
		break;
	case SB_THUMBPOSITION:
		TRACE(("-> SB_THUMBPOSITION: %d\n", nPos))
		gotoline(TRUE, nPos + 1);
		break;
	case SB_THUMBTRACK:
		TRACE(("-> SB_THUMBTRACK: %d\n", nPos))
		mvupwind(TRUE, line_no(curwp->w_bufp, curwp->w_line.l) - nPos);
		break;
	case SB_TOP:
		TRACE(("-> SB_TOP\n"))
		gotoline(TRUE, 1);
		break;
	}
	(void)update(TRUE);
	set_scrollbar_range(snum, curwp);
	fshow_cursor();
}
#endif /* OPT_SCROLLBARS */

static void repaint_window(HWND hWnd)
{
	PAINTSTRUCT ps;
	HBRUSH brush;
	int x0, y0, x1, y1;
	int row, col;

	BeginPaint(hWnd, &ps);
	TRACE(("repaint_window (erase:%d)\n", ps.fErase))
	SelectObject(ps.hdc, GetMyFont());
	nt_set_colors(ps.hdc, cur_atr);
	brush = Background(ps.hdc);

	TRACE(("...painting (%d,%d) (%d,%d)\n",
		ps.rcPaint.top,
		ps.rcPaint.left,
		ps.rcPaint.bottom,
		ps.rcPaint.right))

	y0 = (ps.rcPaint.top) / nLineHeight;
	x0 = (ps.rcPaint.left) / nCharWidth;
	y1 = (ps.rcPaint.bottom + nLineHeight) / nLineHeight;
	x1 = (ps.rcPaint.right  + nCharWidth)  / nCharWidth;

	if (y0 < 0)
		y0 = 0;
	if (x0 < 0)
		x0 = 0;

	TRACE(("...erase %d\n", ps.fErase))
	TRACE(("...cells (%d,%d) - (%d,%d)\n", y0,x0, y1,x1))
	TRACE(("...top:    %d\n", RowToPixel(y0) - ps.rcPaint.top))
	TRACE(("...left:   %d\n", ColToPixel(x0) - ps.rcPaint.left))
	TRACE(("...bottom: %d\n", RowToPixel(y1) - ps.rcPaint.bottom))
	TRACE(("...right:  %d\n", ColToPixel(x1) - ps.rcPaint.right))

	for (row = y0; row < y1; row++) {
		if (pscreen != 0
		 && pscreen[row]->v_text != 0
		 && pscreen[row]->v_attrs != 0) {
			for (col = x0; col < x1; col++) {
				nt_set_colors(ps.hdc, CELL_ATTR(row,col));
				TextOut(ps.hdc,
					ColToPixel(col),
					RowToPixel(row),
					&CELL_TEXT(row,col), 1);
			}
		}
	}
	DeleteObject(brush);

	TRACE(("...repaint_window\n"))
	EndPaint(hWnd, &ps);
}

static int khit = 0;

int kbhit(void)
{
	MSG msg;
	int hit;

	if(PeekMessage(&msg, (HWND)0, (UINT)0, (UINT)0, PM_REMOVE)){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	hit = khit;
	khit = 0;
	return hit;
}

static void HandleClose(HWND hWnd)
{

	quit(FALSE,1);
}

LONG FAR PASCAL TextWndProc(
			HWND hWnd,
			UINT message,
			WPARAM wParam,
			LONG lParam)
{
	TRACE(("TEXT:%s, %s\n", message2s(message), which_window(hWnd)))

	switch (message) {
	case WM_PAINT:
		if (GetUpdateRect(hWnd, (LPRECT)0, FALSE)) {
			repaint_window(hWnd);
		} else {
			TRACE(("FIXME:WM_PAINT\n"))
			return (DefWindowProc(hWnd, message, wParam, lParam));
		}
		break;

	case WM_KEYDOWN:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_SYSKEYDOWN:
		khit = 1;
		/* FALLTHRU */
	default:
		return (DefWindowProc(hWnd, message, wParam, lParam));

	IGN_PROC("TEXT:", WM_ERASEBKGND);
	}
	return (0);
}

LONG FAR PASCAL MainWndProc(
			HWND hWnd,
			UINT message,
			WPARAM wParam,
			LONG lParam)
{
#if FIXME
	FARPROC lpProcAbout;
#endif

	TRACE(("MAIN:%s, %s\n", message2s(message), which_window(hWnd)))

	switch (message) {
	HANDLE_MSG(hWnd, WM_CLOSE,	HandleClose);
	case WM_COMMAND:
#if FIXME
		switch (wParam) {
		case IDC_button:
			khit = 1;
			wParam = IDC_button_x;
			PostMessage(hWnd, message, wParam, lParam);
			break;
		case IDM_ABOUT:
			lpProcAbout = MakeProcInstance(About, vile_hinstance);
			DialogBox(vile_hinstance, "AboutBox", hWnd, lpProcAbout);
			FreeProcInstance(lpProcAbout);
			break;

		/* file menu commands */

		case IDM_PRINT:
			MessageBox (
				GetFocus(),
				"Command not implemented",
				MY_APPLE,
				MB_ICONASTERISK | MB_OK);
			break;

		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		}
#endif
		break;

/*
	case WM_SIZE:
		MoveWindow(cur_win->main_hwnd, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
		break;
*/
	case WM_SETFOCUS:
		fshow_cursor();
		break;

	case WM_KILLFOCUS:
		fhide_cursor();
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_WINDOWPOSCHANGED:
		ResizeClient();
		return (DefWindowProc(hWnd, message, wParam, lParam));

	case WM_WINDOWPOSCHANGING:
#if FIXME_POSCHANGING
		if (wheadp != 0)
			return AdjustPosChanging(hWnd, (LPWINDOWPOS)lParam);
#endif
		return (DefWindowProc(hWnd, message, wParam, lParam));

	case WM_SIZING:
		return AdjustResizing(hWnd, wParam, (LPRECT)lParam);

	case WM_EXITSIZEMOVE:
		ResizeClient();
		return (DefWindowProc(hWnd, message, wParam, lParam));

	case WM_SYSCOMMAND:
		TRACE(("MAIN:WM_SYSCOMMAND %s at %d,%d\n",
			syscommand2s(LOWORD(wParam)),
			HIWORD(lParam),
			LOWORD(lParam)))
		switch(LOWORD(wParam))
		{
		case MM_FONT:
			set_font();
			break;
		}
		return (DefWindowProc(hWnd, message, wParam, lParam));

#if OPT_SCROLLBARS
	case WM_VSCROLL:
		handle_scrollbar((HWND)lParam, LOWORD(wParam), HIWORD(wParam));
		return (DefWindowProc(hWnd, message, wParam, lParam));
#endif

	case WM_KEYDOWN:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_SYSKEYDOWN:
		khit = 1;
		/* FALLTHRU */
	default:
		return (TextWndProc(hWnd, message, wParam, lParam));

	IGN_PROC("MAIN:", WM_ERASEBKGND);
	}
	return (1);
}

static BOOL
InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	WNDCLASS  wc;

	hglass_cursor    = LoadCursor((HINSTANCE)0, IDC_WAIT);
	arrow_cursor     = LoadCursor((HINSTANCE)0, IDC_ARROW);

	default_bcolor   = GetSysColor(COLOR_WINDOWTEXT+1);
	default_fcolor   = GetSysColor(COLOR_WINDOW+1);

	ZeroMemory(&wc, sizeof(&wc));
	wc.style         = CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc   = (WNDPROC) MainWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(hInstance, "VilewIcon");
	wc.hCursor       = arrow_cursor;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName  = "VileMenu";
	wc.lpszClassName = MAIN_CLASS;

	if(! RegisterClass(&wc))
		return FALSE;

	TRACE(("Registered(%s)\n", MAIN_CLASS))

	vile_hinstance = hInstance;
	hAccTable = LoadAccelerators(vile_hinstance, "VileAcc");

	cur_win->main_hwnd = CreateWindow(
		MAIN_CLASS,
		MY_APPLE,
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		1,
		1,
		(HWND)0,
		(HMENU)0,
		hInstance,
		(LPVOID)0
	);
	TRACE(("CreateWindow(main) -> %#lx\n", cur_win->main_hwnd))
	if (!cur_win->main_hwnd)
		return (FALSE);

	ZeroMemory(&wc, sizeof(&wc));
	wc.style         = CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc   = (WNDPROC) TextWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = 0;
	wc.hCursor       = arrow_cursor;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = TEXT_CLASS;

	if(! RegisterClass(&wc))
		return FALSE;

	TRACE(("Registered(%s)\n", TEXT_CLASS))

	cur_win->text_hwnd = CreateWindow(
		TEXT_CLASS,
		"text",
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		1,
		1,
		cur_win->main_hwnd,
		(HMENU)0,
		hInstance,
		(LPVOID)0
	);
	TRACE(("CreateWindow(text) -> %#lx\n", cur_win->text_hwnd))
	if (!cur_win->text_hwnd)
		return (FALSE);

	/*
	 * Register the GRIP_CLASS now also, otherwise it won't succeed when
	 * we create the first scrollbars, until we resize the window.
	 */
#if OPT_SCROLLBARS
	ZeroMemory(&wc, sizeof(&wc));
	wc.style         = CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc   = (WNDPROC) GripWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = vile_hinstance;
	wc.hCursor       = arrow_cursor;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = GRIP_CLASS;

	if(! RegisterClass(&wc)) {
		TRACE(("could not register class %s:%#x\n", GRIP_CLASS, GetLastError()))
		return (FALSE);
	}
	TRACE(("Registered(%s)\n", GRIP_CLASS))
#endif

	cur_win->nscrollbars = -1;

	/*
	 * Insert "File" and "Font" before "Close" in the system menu.
	 */
	vile_menu = GetSystemMenu(cur_win->main_hwnd, FALSE);
	AppendMenu(vile_menu, MF_SEPARATOR, 0, NULL);
#if 0	/* FIXME: later */
	AppendMenu(vile_menu, MF_STRING, MM_FILE, "File");
#endif
	AppendMenu(vile_menu, MF_STRING, MM_FONT, "&Font");

#if OPT_SCROLLBARS
	if (check_scrollbar_allocs() != TRUE)
		return (FALSE);
#endif

	get_font(&vile_logfont);
	use_font(vile_font, FALSE);

	ShowWindow(cur_win->main_hwnd, nCmdShow);
	UpdateWindow(cur_win->main_hwnd);
	return (TRUE);
}

int WINAPI
WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow)
{
	int argc = 0;
	int n;
	int maxargs = (strlen(lpCmdLine) + 1) / 2;
	char **argv = typeallocn(char *, maxargs);
	char *ptr, *fontstr;
#ifdef VILE_OLE
	int oa_invoke, oa_reg;

	memset(&oa_opts, 0, sizeof(oa_opts));
	oa_invoke = oa_reg = FALSE;
#endif

	TRACE(("Starting ntvile, CmdLine:%s\n", lpCmdLine))

	argv[argc++] = "VILE";

	for(ptr = lpCmdLine; *ptr != '\0';) {
		char delim = ' ';

		while (*ptr == ' ')
			ptr++;

		if (*ptr == '\''
		 || *ptr == '"'
		 || *ptr == ' ') {
			delim = *ptr++;
		}
		TRACE(("argv%d:%s\n", argc, ptr))
		argv[argc++] = ptr;
		if (argc+1 >= maxargs) {
			break;
		}
		while (*ptr != delim && *ptr != '\0')
			ptr++;
		if (*ptr == delim)
			*ptr++ = '\0';
	}
	fontstr = argv[argc] = 0;

	SetCols(80);
	SetRows(24);

	/*
	 * Get screen size and OLE options, if any.  Parsing logic is
	 * messy, but must remain that way to handle the various command
	 * line options available with and without OLE automation.
	 */
	for (n = 1; n < argc; n++) {
		int m = n, eat = 0;
		if (n + 1 < argc) {
			if (strcmp(argv[n], "-geometry") == 0) {
				char *src = argv[n+1];
				char *dst = 0;
				int value = strtol(src, &dst, 0);
				if (dst != src) {
					if (value > 2)
						SetCols(value);
					if (*dst++ == 'x') {
						src = dst;
						value = strtol(src, &dst, 0);
						if (value > 2) {
							SetRows(value);
#ifdef VILE_OLE
							oa_opts.cols = term.cols;
							oa_opts.rows = term.rows;
#endif
						}
					}
					eat = 2;
				}
			}
			else if (strcmp(argv[n], "-font") == 0 ||
						strcmp(argv[n], "-fn") == 0)
			{
				fontstr = argv[n + 1];
				eat     = 2;
			}
		}
#ifdef VILE_OLE
		if (eat == 0)
		{
			/* No valid options seen yet. */

			if (argv[n][0] == '-' && argv[n][1] == 'O')
			{
				int which = argv[n][2];

				if (which == 'r')
				{
					/*
					 * Flag OLE registration request,
					 * but don't eat argument.  Instead,
					 * registration will be carried out
					 * in main.c, so that the regular
					 * cmdline parser has an opportunity
					 * to flag misspelled OLE options.
					 *
					 * Ex:  winvile -Or -mutiple
					 */

					oa_reg = TRUE;
				}
				else if (which == 'u')
					ExitProgram(oleauto_unregister());
				else if (which == 'a')
				{
					oa_invoke = TRUE;
					eat       = 1;
				}
			}
			else if (strcmp(argv[n], "-invisible") == 0)
			{
				oa_opts.invisible = TRUE;
				eat               = 1;
			}
			else if (strcmp(argv[n], "-multiple") == 0)
			{
				oa_opts.multiple = TRUE;
				eat            = 1;
			}
		}
#endif
		if (eat) {
			while (m+eat <= argc) {
				argv[m] = argv[m+eat];
				m++;
			}
			n--;
			argc -= eat;
		}
	}

#ifdef VILE_OLE
	if (oa_reg && oa_invoke)
	{
		/* tsk tsk */

		MessageBox(cur_win->main_hwnd,
			   "-Oa and -Or are mutually exclusive",
			   prognam,
			   MB_OK|MB_ICONSTOP);
		ExitProgram(1);
	}
	if (oa_reg)
	{
		/*
		 * The main program's cmd line parser will eventually cause
		 * OLE autoamation registration to occur, at which point
		 * winvile exits.  So don't show a window.
		 */

		nCmdShow = SW_HIDE;
	}
	if (oa_opts.invisible)
		nCmdShow = SW_HIDE;
#endif

	if (!InitInstance(hInstance, nCmdShow))
		return (FALSE);

	/*
	 * Vile window created and default font set.  It's now kosher to set
	 * the font from a cmdline switch.
	 */
	if (fontstr)
    {
        int success = ntwinio_font_frm_str(fontstr, TRUE);

#ifdef VILE_OLE
        if (oa_reg)
        {
            if (! success)
            {
                /*
                 * That's it, game over -- crummy font spec detected during
                 * OLE registration.
                 */

                ExitProgram(1);
            }
            else
                oa_opts.fontstr = fontstr;
        }
#endif
        /* Regardless of success or failure, continue with new/default font. */
    }

#ifdef VILE_OLE
	if (oa_invoke)
	{
		/* Intialize OLE Automation */

		if (! oleauto_init(&oa_opts))
			ExitProgram(1);
	}
#endif

	return MainProgram(argc, argv);
}

void *
winvile_hwnd(void)
{
	return (cur_win->main_hwnd);
}

#ifdef VILE_OLE
void
ntwinio_oleauto_reg(void)
{
	/* Pound a bunch of OLE registration data into the registry & exit. */

	ExitProgram(oleauto_register(&oa_opts));
}

void
ntwinio_redirect_hwnd(int redirect)
{
	redirect_keys = redirect;
}
#endif

/*
 * Split the version-message to allow us to format with tabs, so the
 * proportional font doesn't look ugly.
 */
static size_t
option_size(const char *option)
{
	if (*option == '-') {
		const char *next = skip_ctext(option);
		if (next[0] == ' '
		 && next[1] != ' '
		 && next[1] != 0) {
			next = skip_ctext(next + 1);
			return next - option;
		}
		return 14;	/* use embedded blanks to fix the tabs ... */
	}
	return 0;
}

void
gui_version(char *program)
{
	ShowWindow(cur_win->main_hwnd, SW_HIDE);
	MessageBox(cur_win->main_hwnd, getversion(), prognam, MB_OK|MB_ICONSTOP);
}

void
gui_usage(char *program, const char *const *options, size_t length)
{
	char *buf, *s;
	size_t need, n;
	const char *fmt1 = "%s\n\nOptions:\n";
	const char *fmt2 = "    %s\t%s\n";
	const char *fmt3 = "%s\n";

	/*
	 * Hide the (partly-constructed) main window.  It'll flash (FIXME).
	 */
	ShowWindow(cur_win->main_hwnd, SW_HIDE);

	need = strlen(fmt1) + strlen(prognam);
	for (n = 0; n < length; n++) {
		if (option_size(options[n]))
			need += strlen(fmt2) + strlen(options[n]);
		else
			need += strlen(fmt3) + strlen(options[n]);
	}
	buf = malloc(need);

	s = lsprintf(buf, fmt1, prognam);
	for (n = 0; n < length; n++) {
		char	temp[80];
		if ((need = option_size(options[n])) != 0) {
			strncpy(temp, options[n], need);
			temp[need] = EOS;
			s = lsprintf(s, fmt2, temp, skip_cblanks(options[n] + need));
		} else {
			s = lsprintf(s, fmt3, options[n]);
		}
	}

	MessageBox(cur_win->main_hwnd, buf, prognam, MB_OK|MB_ICONSTOP);
}
