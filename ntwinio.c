/*
 * Uses the Win32 screen API.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/ntwinio.c,v 1.8 1998/05/03 20:29:21 cmorgan Exp $
 *
 */

#include <windows.h>
#include <commdlg.h>

#define	termdef	1			/* don't define "term" external */

#include        "estruct.h"
#include        "edef.h"
#include        "pscreen.h"

#undef RECT	/* FIXME: symbol conflict */

#if OPT_TRACE
#define IGN_PROC(tag,name) \
 	case name: \
		TRACE((tag #name " (ignored)\n")); \
		break;

#define DEF_PROC(tag,name) \
 	case name: \
		TRACE((tag #name "\n")); \
		return (DefWindowProc(hWnd, message, wParam, lParam))
#else
#define IGN_PROC(tag,name) case name: break;
#define DEF_PROC(tag,name) /*nothing*/
#endif

#define MY_FONT  SYSTEM_FIXED_FONT	/* or ANSI_FIXED_FONT		*/
#define MY_CLASS "VileWClass"
#define MY_APPLE "Vile Application"

#define GetMyFont() vile_font

#define MM_FILE 1
#define MM_FONT 2

#define NROW	128			/* Max Screen size.		*/
#define NCOL    256			/* Edit if you want to.         */
#define	MARGIN	8			/* size of minimum margin and	*/
#define	SCRSIZ	64			/* scroll size for extended lines */
#define	NPAUSE	200			/* # times thru update to pause */
#define NOKYMAP (-1)

#define	AttrColor(b,f)	((WORD)(((ctrans[b] & 15) << 4) | (ctrans[f] & 15)))
#define Row(n) ((n) * nLineHeight)
#define Col(n) ((n) * nCharWidth)

#define RowToY(n) ((n) / nLineHeight)
#define ColToX(n) ((n) / nCharWidth)

static	int	ntgetch		(void);
static	void	ntmove		(int, int);
static	void	nteeol		(void);
static	void	nteeop		(void);
static	void	ntbeep		(void);
static	void	ntopen		(void);
static	void	ntrev		(UINT);
static	int	ntcres		(char *);
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
static	HWND	vile_hwnd;
static	HMENU	vile_menu;
static	HFONT	vile_font;
static	LOGFONT	vile_logfont;
static	HANDLE	hAccTable;   /* handle to accelerator table */
static	HCURSOR	hglass_cursor;
static	HCURSOR	arrow_cursor;
static	int	nLineHeight = 10;
static	int	nCharWidth = 8;
static	int	caret_visible = 0;
static	int	caret_exists = 0;
static	int	vile_in_getfkey = 0;
static	int	vile_resizing = FALSE;	/* rely on repaint_window if true */
static	DWORD	default_fcolor;
static	DWORD	default_bcolor;

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

static	void	repaint_window(HWND hWnd);
static  void	scflush  (void);

/*
 * Standard terminal interface dispatch table. None of the fields point into
 * "termio" code.
 */

TERM    term    = {
	NROW,
	NROW,
	NCOL,
	NCOL,
	MARGIN,
	SCRSIZ,
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
	null_t_setfor,
	null_t_setback,
	null_t_setpal,
#endif
	ntscroll,
	null_t_pflush,
#if OPT_ICURSOR
	nticursor,
#else
	null_t_icursor,
#endif
#if OPT_TITLE
	nttitle,
#else
	null_t_title,
#endif
	null_t_watchfd,
	null_t_unwatchfd,
};

static HBRUSH
Background(HDC hdc)
{
	TRACE(("Background\n"));
	return CreateSolidBrush(GetBkColor(hdc));
}

static void
AdjustWindowSize(int h, int w)
{
	RECT crect;
	RECT wrect;

	GetClientRect(vile_hwnd, &crect);
	GetWindowRect(vile_hwnd, &wrect);

	wrect.right  += (nCharWidth  * w) - crect.right;
	wrect.bottom += (nLineHeight * h) - crect.bottom;

	MoveWindow(vile_hwnd,
		wrect.left,
		wrect.top,
		wrect.right - wrect.left,
		wrect.bottom - wrect.top,
		TRUE);
}

static void
ResizeClient()
{
	int h, w;
	RECT crect;

	GetClientRect(vile_hwnd, &crect);

	h = (crect.bottom - crect.top) / nLineHeight;
	w = (crect.right - crect.left) / nCharWidth;

	if ((h > 1 && h != term.t_nrow) || (w > 1 && w != term.t_ncol)) {
		TRACE(("ResizeClient %dx%d\n", h, w));
		vile_resizing = TRUE;
		newscreensize(h, w);
		vile_resizing = FALSE;
		TRACE(("...ResizeClient %dx%d\n", h, w));
	}

	AdjustWindowSize(h, w);
}

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
		if (attr == VABOLD)
			*fcolor |= FOREGROUND_INTENSITY;
		else if (attr == VAITAL)
			*bcolor |= BACKGROUND_INTENSITY;
		else if (attr & VACOLOR)
			*fcolor = ((VCOLORNUM(attr)) & (NCOLORS - 1));
		else if (attr & VASPCOL)
			*fcolor = (attr & (NCOLORS - 1));
		else {  /* all other states == reverse video */
			*bcolor = nfcolor;
			*fcolor = nbcolor;
			ninvert = TRUE;
		}
	}
}

static void
set_colors (HDC hdc, VIDEO_ATTR attr)
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
	TRACE(("fhide_cursor\n"));
	if(caret_visible) {
		HideCaret(vile_hwnd);
		caret_visible = 0;
	}
	if(caret_exists) {
		DestroyCaret();
		caret_exists = 0;
	}
	return 0;
}

static int fshow_cursor(void)
{
	int x, y;
	fhide_cursor();

	TRACE(("fshow_cursor\n"));
	if (!caret_exists) {
		CreateCaret(vile_hwnd, (HBITMAP) 0, nCharWidth, nLineHeight);
		caret_exists = 1;
	}
	x = Col(ttcol) + 1;
	y = Row(ttrow) + 1;
	SetCaretPos(x, y);
	ShowCaret(vile_hwnd);
	caret_visible = 1;
	return 0;
}

/* Notes: lpntm is a pointer to a TEXTMETRIC struct if FontType does not
 * have TRUETYPE_FONTTYPE set, but we only need the tmPitchAndFamily member,
 * which has the same alignment as in NEWTEXTMETRIC.
 */
static int CALLBACK
enumerate_fonts(
	ENUMLOGFONT *lpelf,
	NEWTEXTMETRIC *lpntm,
	int FontType,
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

	hDC = GetDC(vile_hwnd);
	SelectObject(hDC, font);
	ok = GetTextMetrics(hDC, &metrics);
	ReleaseDC(vile_hwnd, hDC);

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
	HFONT *font = CreateFontIndirect(lf);

	if (font != 0) {
		if (is_fixed_pitch(font)) {
			DeleteObject(vile_font);
			vile_font = font;
			TRACE(("created new font\n"))
			return TRUE;
		}
	}
	return FALSE;
}

static void get_font(LOGFONT *lf)
{
	HDC             hDC;

	vile_font = GetStockObject(MY_FONT);
	hDC = GetDC(vile_hwnd);
	if (EnumFontFamilies(hDC, NULL, enumerate_fonts, (LPARAM) lf) <= 0)
	{
		TRACE(("Creating Pitch/Family: %#x\n", lf->lfPitchAndFamily))
		new_font(lf);
	}
	ReleaseDC(vile_hwnd, hDC);
}

static void use_font(HFONT my_font, BOOL resizable)
{
	HDC             hDC;
	TEXTMETRIC      textmetric;
	RECT		wrect, crect;

	hDC = GetDC(vile_hwnd);
	SelectObject(hDC, my_font);
	GetTextMetrics(hDC, &textmetric);
	ReleaseDC(vile_hwnd, hDC);

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

	if (resizable) {
		GetClientRect(vile_hwnd, &crect);
		GetWindowRect(vile_hwnd, &wrect);

		term.t_ncol = (crect.right - crect.left) / nCharWidth;
		term.t_nrow = (crect.bottom - crect.top) / nLineHeight;
	}

	AdjustWindowSize(term.t_nrow, term.t_ncol);
}

static void set_font(void)
{
	HDC		hDC;
	CHOOSEFONT	choose;

	memset(&choose, 0, sizeof(choose));
	choose.lStructSize = sizeof(choose);
	choose.hwndOwner   = vile_hwnd;
	choose.Flags       = CF_SCREENFONTS
			   | CF_FIXEDPITCHONLY
			   | CF_FORCEFONTEXIST
			   | CF_NOSCRIPTSEL
			   | CF_INITTOLOGFONTSTRUCT;
	choose.lpLogFont   = &vile_logfont;

	SelectObject(hDC, GetMyFont());
	GetTextFace(hDC, sizeof(vile_logfont.lfFaceName), vile_logfont.lfFaceName);
	ReleaseDC(vile_hwnd, hDC);

	vile_logfont.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
	vile_logfont.lfCharSet = ANSI_CHARSET;
	TRACE(("LOGFONT Facename:%s\n", vile_logfont.lfFaceName))

	if (ChooseFont(&choose)) {
		TRACE(("ChooseFont '%s'\n", vile_logfont.lfFaceName))
		if (new_font(&vile_logfont)) {
			mlwrite("[Set font to %s]", vile_logfont.lfFaceName);
			use_font(vile_font, FALSE);
			vile_refresh(FALSE,0);
		} else {
			mlforce("[Cannot create font]");
		}
	} else {
		mlforce("[No font selected]");
	}

	TRACE(("LOGFONT Facename:%s\n", vile_logfont.lfFaceName))
}

#if OPT_TITLE
static void
nttitle(char *title)		/* set the current window title */
{
	SetWindowText(vile_hwnd, title);
}
#endif

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
scflush(void)
{
	if (cur_pos && !vile_resizing) {
		HDC hdc;

		TRACE(("PUTC:flush %2d (%2d,%2d) (%.*s)\n", cur_pos, crow,ccol, cur_pos, &CELL_TEXT(crow,ccol)))

		hdc = GetDC(vile_hwnd);
		SelectObject(hdc, GetMyFont());
		set_colors(hdc, cur_atr);

		TextOut(hdc,
			Col(ccol),
			Row(crow),
			&CELL_TEXT(crow,ccol), cur_pos);

		ReleaseDC(vile_hwnd, hdc);
	}
	ccol = ccol + cur_pos;
	cur_pos = 0;
}

static void
ntflush(void)
{
	scflush();
	SetCaretPos(Col(ccol), Row(crow));
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

	TRACE(("NTEEOL %d,%d, atr %#x\n", crow, ccol, cur_atr));
	for (x = ccol; x < term.t_ncol; x++) {
		CELL_TEXT(crow,x) = ' ';
		CELL_ATTR(crow,x) = cur_atr;
	}

	GetClientRect(vile_hwnd, &rect);
	rect.left   = Col(ccol);
	rect.top    = Row(crow);
	rect.right  = Col(term.t_ncol);
	rect.bottom = Row(crow+1);

	hDC = GetDC(vile_hwnd);
	set_colors(hDC, cur_atr);
	brush = Background(hDC);
	FillRect(hDC, &rect, brush);
	DeleteObject(brush);
	ReleaseDC(vile_hwnd, hDC);
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
			if (crow < term.t_nrow - 1)
				crow++;
			else
				ntscroll(1, 0, term.t_nrow - 1);
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
	TRACE(("NTEEOP %d,%d, atr %#x\n", crow, ccol, cur_atr));
	for (y = crow; y < term.t_nrow; y++) {
		for (x = 0; x < term.t_ncol; x++) {
			CELL_TEXT(y,x) = ' ';
			CELL_ATTR(y,x) = cur_atr;
		}
		x0 = 0;
	}

	rect.left   = Col(ccol);
	rect.top    = Row(crow);
	rect.right  = Col(term.t_ncol);
	rect.bottom = Row(term.t_nrow);

	if (!vile_resizing) {
		hDC = GetDC(vile_hwnd);
		set_colors(hDC, cur_atr);
		brush = Background(hDC);
		FillRect(hDC, &rect, brush);
		DeleteObject(brush);
		ReleaseDC(vile_hwnd, hDC);
	}
}

static void
ntrev(UINT reverse)		/* change reverse video state */
{
	scflush();
	cur_atr = (VIDEO_ATTR) reverse;
}

static int
ntcres(char *res)	/* change screen resolution */
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

	GetClientRect(vile_hwnd, &rect);
	hDC = GetDC(vile_hwnd);
	InvertRect(hDC, &rect);
	Sleep(100);
	InvertRect(hDC, &rect);
	ReleaseDC(vile_hwnd, hDC);
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
	ntmove(term.t_nrow - 1, 0);
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

static const struct {
	int	windows;
	int	vile;
	int	shift;
} keyxlate[] = {
	{ VK_NEXT,	KEY_Next,	0 },
	{ VK_PRIOR,	KEY_Prior,	0 },
	{ VK_END,	KEY_End,	0 },
	{ VK_HOME,	KEY_Home,	0 },
	{ VK_LEFT,	KEY_Left,	0 },
	{ VK_RIGHT,	KEY_Right,	0 },
	{ VK_UP,	KEY_Up,		0 },
	{ VK_DOWN,	KEY_Down,	0 },
	{ VK_INSERT,	KEY_F33,   	LEFT_ALT_PRESSED|RIGHT_ALT_PRESSED },
	{ VK_INSERT,	KEY_F34,   	LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED },
	{ VK_INSERT,	KEY_F35,   	SHIFT_PRESSED },
	{ VK_INSERT,	KEY_Insert,	0 },
	{ VK_DELETE,	KEY_Delete,	0 },
	{ VK_HELP,	KEY_Help,	0 },
	{ VK_SELECT,	KEY_Select,	0 },
	{ VK_TAB,	'\t',		0 },
#if FIXME
	{ VK_TAB,	'\t',		SHIFT_PRESSED },
	/* Merely pressing the Alt key generates a VK_MENU key event. */
	{ VK_MENU,	KEY_Menu,	0 },
#endif
	{ VK_F1,	KEY_F1,		0 },
	{ VK_F2,	KEY_F2,		0 },
	{ VK_F3,	KEY_F3,		0 },
	{ VK_F4,	KEY_F4,		0 },
	{ VK_F5,	KEY_F5,		0 },
	{ VK_F6,	KEY_F6,		0 },
	{ VK_F7,	KEY_F7,		0 },
	{ VK_F8,	KEY_F8,		0 },
	{ VK_F9,	KEY_F9,		0 },
	{ VK_F10,	KEY_F10,	0 },
	{ VK_F11,	KEY_F11,	0 },
	{ VK_F12,	KEY_F12,	0 },
	{ VK_F13,	KEY_F13,	0 },
	{ VK_F14,	KEY_F14,	0 },
	{ VK_F15,	KEY_F15,	0 },
	{ VK_F16,	KEY_F16,	0 },
	{ VK_F17,	KEY_F17,	0 },
	{ VK_F18,	KEY_F18,	0 },
	{ VK_F19,	KEY_F19,	0 },
	{ VK_F20,	KEY_F20,	0 },
	/* Manually translate Ctrl-6 into Ctrl-^. */
	{ '6',		'^'-'@',	LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED },
};

static int savedChar;
static int saveCount = 0;

static int
decode_key_event(KEY_EVENT_RECORD *irp)
{
    int key;
    int i;

    key = (unsigned char) irp->uChar.AsciiChar;
    if (key != 0) {
        return key;
    }

    for (i = 0; i < TABLESIZE(keyxlate); i++)
    {
        if (keyxlate[i].windows == irp->wVirtualKeyCode)
        {
            if (keyxlate[i].shift != 0 &&
                    !(keyxlate[i].shift & irp->dwControlKeyState))
            {
                continue;
            }
            key = keyxlate[i].vile;
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
		if(GetFocus() == vile_hwnd) {
			SetCursor(arrow_cursor);
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
			TRACE(("GETC:no message\n"));
			quit(TRUE,1);
		}

		if (TranslateAccelerator(vile_hwnd, hAccTable, &msg))
			continue;

		TranslateMessage(&msg);

		switch(msg.message) {
		case WM_DESTROY:
			TRACE(("GETC:DESTROY\n"))
			PostQuitMessage(0);
			continue;

		case WM_COMMAND:
			TRACE(("GETC:COMMAND\n"))
			DispatchMessage(&msg);
			break;

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
			TRACE(("GETC:KEYDOWN:%#x ->%#x\n", msg.wParam, result))
			if (result == NOKYMAP) {
				DispatchMessage(&msg);
				result = 0;
			} else if (result == (int) msg.wParam) {
				result = 0; /* we'll get a WM_CHAR next */
			}
			break;

		case WM_LBUTTONDOWN:
			TRACE(("GETC:LBUTTONDOWN\n"))
			dword = GetMessagePos();
			points = MAKEPOINTS(dword);
			POINTSTOPOINT(first, points);
			ScreenToClient(vile_hwnd, &first);
			first.x /= nCharWidth;
			first.y /= nLineHeight;
			if (setcursor(first.y, first.x)) {
				(void)sel_begin();
				(void)update(TRUE);
				buttondown = TRUE;
			}
			break;

		case WM_RBUTTONDOWN:
			if (buttondown) {
				(void)sel_release();
				(void)update(TRUE);
			}
			break;

		case WM_MOUSEMOVE:
			if (buttondown) {
				int x = ColToX(LOWORD(msg.lParam));
				int y = RowToY(HIWORD(msg.lParam));

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
			TRACE(("GETC:LBUTTONUP\n"))
			if (buttondown) {
				sel_yank(0);
				buttondown = FALSE;
			}
			fshow_cursor();
			break;

		default:
			TRACE(("GETC:default(%#lx)\n", msg.message))
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
	if(GetFocus() == vile_hwnd) {
		SetCursor(hglass_cursor);
	}
	fhide_cursor();
	vile_in_getfkey = 0;

	TRACE(("...ntgetch %#x\n", result));
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
	region.right  = (SHORT) Col(term.t_ncol);

	if (from > to) {
		region.top    = (SHORT) Row(to);
		region.bottom = (SHORT) Row(from + n);
	} else {
		region.top    = (SHORT) Row(from);
		region.bottom = (SHORT) Row(to + n);
	}

	TRACE(("ScrollWindowEx from=%d, to=%d, n=%d  (%d,%d)/(%d,%d)\n",
		from, to, n,
		region.left, region.top,
		region.right, region.bottom));

	ScrollWindowEx(
		vile_hwnd,	/* handle of window to scroll */
		0,		/* amount of horizontal scrolling */
		Row(to-from),	/* amount of vertical scrolling */
		&region,	/* address of structure with scroll rectangle */
		&region,	/* address of structure with clip rectangle */
		(HRGN)0,	/* handle of update region */
		&tofill,	/* address of structure for update rectangle */
		SW_ERASE 	/* scrolling flags */
		);
	TRACE(("fill: (%d,%d)/(%d,%d)\n",
		tofill.left, tofill.top,
		tofill.right, tofill.bottom));

	hDC = GetDC(vile_hwnd);
	set_colors(hDC, cur_atr);
	brush = Background(hDC);
	FillRect(hDC, &tofill, brush);
	DeleteObject(brush);
	ReleaseDC(vile_hwnd, hDC);
}

static void repaint_window(HWND hWnd)
{
	PAINTSTRUCT ps;
	HBRUSH brush;
	int x0, y0, x1, y1;
	int row, col;

	BeginPaint(hWnd, &ps);
	TRACE(("repaint_window (erase:%d)\n", ps.fErase))
	SelectObject(ps.hdc, GetMyFont());
	set_colors(ps.hdc, cur_atr);
	brush = Background(ps.hdc);

	TRACE(("...painting (%d,%d) (%d,%d)\n",
		ps.rcPaint.top,
		ps.rcPaint.left,
		ps.rcPaint.bottom,
		ps.rcPaint.right));

	y0 = (ps.rcPaint.top) / nLineHeight;
	x0 = (ps.rcPaint.left) / nCharWidth;
	y1 = (ps.rcPaint.bottom + nLineHeight) / nLineHeight;
	x1 = (ps.rcPaint.right  + nCharWidth)  / nCharWidth;

	if (y0 < 0)
		y0 = 0;
	if (x0 < 0)
		x0 = 0;

	TRACE(("...erase %d\n", ps.fErase));
	TRACE(("...cells (%d,%d) - (%d,%d)\n", y0,x0, y1,x1));
	TRACE(("...top:    %d\n", Row(y0) - ps.rcPaint.top));
	TRACE(("...left:   %d\n", Col(x0) - ps.rcPaint.left));
	TRACE(("...bottom: %d\n", Row(y1) - ps.rcPaint.bottom));
	TRACE(("...right:  %d\n", Col(x1) - ps.rcPaint.right));

	for (row = y0; row < y1; row++) {
		if (pscreen != 0
		 && pscreen[row]->v_text != 0
		 && pscreen[row]->v_attrs != 0) {
			for (col = x0; col < x1; col++) {
				set_colors(ps.hdc, CELL_ATTR(row,col));
				TextOut(ps.hdc,
					Col(col),
					Row(row),
					&CELL_TEXT(row,col), 1);
			}
		}
	}
	DeleteObject(brush);

	TRACE(("...repaint_window\n"))
	EndPaint(hWnd, &ps);
}

static khit=0;

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

LONG FAR PASCAL MainWndProc(
			HWND hWnd,
			UINT message,
			WPARAM wParam,
			LONG lParam)
{
#if FIXME
	FARPROC lpProcAbout;
#endif

	switch (message) {
	case WM_MOUSEMOVE:
		TRACE(("MAIN:MOUSEMOVE\n"))
		if(GetFocus() == vile_hwnd) {
			if(vile_in_getfkey) {
				SetCursor(arrow_cursor);
			} else {
				SetCursor(hglass_cursor);
			}
		}
		break;
	case WM_COMMAND:
		TRACE(("MAIN:COMMAND\n"))
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
	case WM_SETFOCUS:
		fshow_cursor();
		break;

	case WM_SIZE:
		MoveWindow(vile_hwnd, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
		break;
*/
	case WM_KILLFOCUS:
		fhide_cursor();
		break;

	case WM_DESTROY:
		TRACE(("MAIN:DESTROY\n"))
		PostQuitMessage(0);
		break;

	case WM_PAINT:
		TRACE(("MAIN:PAINT %d/%d\n",
			GetUpdateRect(hWnd, (LPRECT)0, FALSE),
			(hWnd == vile_hwnd)));
		if (GetUpdateRect(hWnd, (LPRECT)0, FALSE)) {
			repaint_window(hWnd);
		} else {
			TRACE(("FIXME:WM_PAINT\n"));
			return (DefWindowProc(hWnd, message, wParam, lParam));
		}
		break;

	case WM_WINDOWPOSCHANGED:	/* FIXME:FALLTHRU for now */
		ResizeClient();
		return (DefWindowProc(hWnd, message, wParam, lParam));

	case WM_EXITSIZEMOVE:
		ResizeClient();
		return (DefWindowProc(hWnd, message, wParam, lParam));

	case WM_SYSCOMMAND:
		switch(LOWORD(wParam))
		{
		case MM_FONT:
			set_font();
			break;
		}
		return (DefWindowProc(hWnd, message, wParam, lParam));

	case WM_KEYDOWN:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_SYSKEYDOWN:
		khit = 1;
		/* FALLTHRU */
	default:
		TRACE(("MAIN:default %#x\n", message))
		return (DefWindowProc(hWnd, message, wParam, lParam));

	IGN_PROC("MAIN:", WM_ERASEBKGND);
	DEF_PROC("MAIN:", WM_KEYUP);
	DEF_PROC("MAIN:", WM_NCPAINT);
	DEF_PROC("MAIN:", WM_SETCURSOR);
	DEF_PROC("MAIN:", WM_NCCALCSIZE);
	DEF_PROC("MAIN:", WM_NCHITTEST);
	DEF_PROC("MAIN:", WM_NCMOUSEMOVE);
	DEF_PROC("MAIN:", WM_SIZING);
	DEF_PROC("MAIN:", WM_WINDOWPOSCHANGING);
	DEF_PROC("MAIN:", WM_GETMINMAXINFO);
	}
	return (0);
}

BOOL InitInstance(
	HANDLE          hInstance,
	int             nCmdShow)
{
	vile_hinstance = hInstance;

	hAccTable = LoadAccelerators(vile_hinstance, "VileAcc");

	vile_hwnd = CreateWindow(
		MY_CLASS,
		MY_APPLE,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		(HWND)0,
		(HMENU)0,
		hInstance,
		(LPVOID)0
	);

	/*
	 * Insert "File" and "Font" before "Close" in the system menu.
	 */
	vile_menu = GetSystemMenu(vile_hwnd, FALSE);
	AppendMenu(vile_menu, MF_SEPARATOR, 0, NULL);
#if 0	/* FIXME: later */
	AppendMenu(vile_menu, MF_STRING, MM_FILE, "File");
#endif
	AppendMenu(vile_menu, MF_STRING, MM_FONT, "&Font");

	if (!vile_hwnd)
		return (FALSE);

	SetCursor(hglass_cursor);

	term.t_ncol = 80;
	term.t_nrow = 24;

	get_font(&vile_logfont);
	use_font(vile_font, FALSE);

	ShowWindow(vile_hwnd, nCmdShow);
	UpdateWindow(vile_hwnd);
	return (TRUE);
}

BOOL InitApplication (HANDLE hInstance)
{
	WNDCLASS  wc;

	ZeroMemory(&wc, sizeof(&wc));
	hglass_cursor    = LoadCursor((HINSTANCE)0, IDC_WAIT);
	arrow_cursor     = LoadCursor((HINSTANCE)0, IDC_ARROW);
	wc.style         = CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc   = (WNDPROC) MainWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(hInstance, "VilewIcon");
	wc.hCursor       = (HCURSOR) 0;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName  = "VileMenu";
	wc.lpszClassName = MY_CLASS;

	default_bcolor = GetSysColor(COLOR_WINDOWTEXT+1);
	default_fcolor = GetSysColor(COLOR_WINDOW+1);

	if(! RegisterClass(&wc))
		return FALSE;

	return TRUE;
}


int PASCAL
WinMain(
	HANDLE hInstance,
	HANDLE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow)
{
#define MAXARGS 10
	int argc = 0;
	char *argv[MAXARGS];
	char *ptr;

	TRACE(("Starting ntvile\n"));

	if (!hPrevInstance) {
		if (!InitApplication(hInstance))
			return (FALSE);
	}

	if (!InitInstance(hInstance, nCmdShow))
		return (FALSE);

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
		TRACE(("argv%d:%s\n", argc, ptr));
		argv[argc++] = ptr;
		if (argc+1 >= MAXARGS) {
			break;
		}
		while (*ptr != delim && *ptr != '\0')
			ptr++;
		if (*ptr == delim)
			*ptr++ = '\0';
	}
	argv[argc] = 0;

	return MainProgram(argc, argv);
}
