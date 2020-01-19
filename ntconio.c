/*
 * Uses the Win32 console API.
 *
 * $Id: ntconio.c,v 1.106 2020/01/17 23:11:00 tom Exp $
 */

#include "estruct.h"
#include "edef.h"
#include "chgdfunc.h"

#include <process.h>

/*
 * Define this if you want to kernel fault win95 when ctrl brk is pressed.
 */
#undef DONT_USE_ON_WIN95

#define MAX_CURBLK_HEIGHT 100	/* max cursor block height (%)  */

#define NROW	128		/* Max Screen size.             */
#define NCOL    256		/* Edit if you want to.         */
#define NOKYMAP (-1)

#define ABS(x) (((x) < 0) ? -(x) : (x))

/* CHAR_INFO and KEY_EVENT_RECORD use the same struct member */
#ifdef UNICODE
#define WHICH_CHAR UnicodeChar
#else
#define WHICH_CHAR AsciiChar
#endif

#define DFT_BCOLOR  C_BLACK
#define DFT_FCOLOR  ((ncolors >= 8) ? 7 : (ncolors - 1))

#define ForeColor(f)	(ctrans[((f) < 0 ? DFT_FCOLOR : (f))] & (NCOLORS-1))
#define BackColor(b)	(ctrans[((b) < 0 ? DFT_BCOLOR : (b))] & (NCOLORS-1))
#define AttrColor(b,f)	((WORD)((BackColor(b) << 4) | ForeColor(f)))

static HANDLE hConsoleOutput;	/* handle to the console display */
static HANDLE hOldConsoleOutput;	/* handle to the old console display */
static HANDLE hConsoleInput;
static CONSOLE_SCREEN_BUFFER_INFO csbi;
static CONSOLE_CURSOR_INFO origcci;
static BOOL origcci_ok;
static WORD originalAttribute;
static WORD currentAttribute;
static int icursor;		/* T -> enable insertion cursor       */
static int icursor_cmdmode;	/* cmd mode cursor height             */
static int icursor_insmode;	/* insertion mode  cursor height      */
static int chgd_cursor;		/* must restore cursor height on exit */
static ENC_CHOICES my_encoding = enc_DEFAULT;

static int conemu = 0;		/* whether running under conemu */
static int cattr = 0;		/* current attributes */
static int cfcolor = -1;	/* current foreground color */
static int cbcolor = -1;	/* current background color */
static int nfcolor = -1;	/* normal foreground color */
static int nbcolor = -1;	/* normal background color */
static int rvcolor = 0;		/* nonzero if we reverse colors */
static int crow = -1;		/* current row */
static int ccol = -1;		/* current col */
static int keyboard_open = FALSE;	/* keyboard is open */
static int keyboard_was_closed = TRUE;

#ifdef VAL_AUTOCOLOR
static int ac_active = FALSE;	/* autocolor active */
#endif

/* ansi to ibm color translation table */
static const char *initpalettestr = "0 4 2 6 1 5 3 7 8 12 10 14 9 13 11 15";
/* black, red, green, yellow, blue, magenta, cyan, white   */

static W32_CHAR linebuf[NCOL];
static int bufpos = 0;

/* Add state variables for console vile's autoscroll feature.              */
static int buttondown = FALSE;	/* is the left mouse button currently down? */
static RECT client_rect;	/* writable portion of editor's window      */
static HANDLE hAsMutex;		/* autoscroll mutex handle (prevents worker
				 * thread and main thread from updating the
				 * display at the same time).
				 */
static WINDOW *mouse_wp;	/* vile window ptr during mouse operations. */
static int row_height;		/* pixels per row within client_rect        */
#define AS_TMOUT 2000		/* hAsMutex timeout period (2 sec) */

#ifdef GVAL_VIDEO
static WORD
AttrVideo(int b, int f)
{
    WORD result;
    if (conemu) {
	if (cattr & VABOLD) {
	    result = ((WORD) ((12 << 4) | ForeColor(f)));
	    TRACE2(("bold AttrVideo(%d,%d) = %04x\n", f, b, result));
	    return result;
	}
	if (cattr & VAITAL) {
	    result = ((WORD) ((13 << 4) | ForeColor(f)));
	    TRACE2(("ital AttrVideo(%d,%d) = %04x\n", f, b, result));
	    return result;
	}
    }
    if (rvcolor) {
	result = ((WORD) ((ForeColor(f) << 4) | BackColor(b)));
	TRACE2(("rev AttrVideo(%d,%d) = %04x\n", f, b, result));
    } else {
	result = ((WORD) ((BackColor(b) << 4) | ForeColor(f)));
	TRACE2(("AttrVideo(%d,%d) = %04x\n", f, b, result));
    }
    return result;
}
#undef AttrColor
#define AttrColor(b,f) AttrVideo(b,f)
#endif

static void
set_current_attr(void)
{
    currentAttribute = AttrColor(cbcolor, cfcolor);
    TRACE2(("set_current_attr %04x\n", currentAttribute));
}

static void
show_cursor(BOOL visible, int percent)
{
    CONSOLE_CURSOR_INFO cci;

    /*
     * if a 100% block height is fed back into the win32 console routines
     * on a win9x/winme host, the cursor is turned off.  win32 bug?
     */
    if (percent < 0)
	percent = 0;
    if (percent >= MAX_CURBLK_HEIGHT)
	percent = (MAX_CURBLK_HEIGHT - 1);

    cci.bVisible = visible;
    cci.dwSize = percent;
    SetConsoleCursorInfo(hConsoleOutput, &cci);
}

#if OPT_ICURSOR
static void
nticursor(int cmode)
{
    if (icursor) {
	show_cursor(TRUE, (cmode == 0) ? icursor_cmdmode : icursor_insmode);
    }
}
#endif

#if OPT_TITLE
static void
ntconio_title(const char *title)
{				/* set the current window title */
    if (title != 0)
	w32_set_console_title(title);
}
#endif

static void
scflush(void)
{
    if (bufpos) {
	COORD coordCursor;
	DWORD written;

	coordCursor.X = (SHORT) ccol;
	coordCursor.Y = (SHORT) crow;
	TRACE2(("scflush %04x [%d,%d]%.*s\n",
		currentAttribute, crow, ccol, bufpos, linebuf));
	WriteConsoleOutputCharacter(
				       hConsoleOutput, linebuf, bufpos,
				       coordCursor, &written
	    );
	FillConsoleOutputAttribute(
				      hConsoleOutput, currentAttribute,
				      bufpos, coordCursor, &written
	    );
	ccol += bufpos;
	bufpos = 0;
    }
}

#if OPT_COLOR
static void
ntconio_fcol(int color)
{				/* set the current output color */
    TRACE2(("ntconio_fcol(%d)\n", color));
    scflush();
    nfcolor = cfcolor = color;
    set_current_attr();
}

static void
ntconio_bcol(int color)
{				/* set the current background color */
    TRACE2(("ntconio_bcol(%d)\n", color));
    scflush();
    nbcolor = cbcolor = color;
    set_current_attr();
}
#endif

static void
ntconio_flush(void)
{
    COORD coordCursor;

    scflush();
    coordCursor.X = (SHORT) ccol;
    coordCursor.Y = (SHORT) crow;
    SetConsoleCursorPosition(hConsoleOutput, coordCursor);
}

static void
ntconio_move(int row, int col)
{
    scflush();
    crow = row;
    ccol = col;
}

static void
erase_at(COORD coordCursor, int length)
{
    W32_CHAR blank = ' ';
    DWORD written;

    FillConsoleOutputCharacter(
				  hConsoleOutput, blank, length,
				  coordCursor, &written
	);
    FillConsoleOutputAttribute(
				  hConsoleOutput, currentAttribute, length,
				  coordCursor, &written
	);
}

/* erase to the end of the line */
static void
ntconio_eeol(void)
{
    COORD coordCursor;
    int length;

    scflush();
    length = csbi.dwMaximumWindowSize.X - ccol;
    coordCursor.X = (SHORT) ccol;
    coordCursor.Y = (SHORT) crow;
    TRACE2(("ntconio_eeol [%d,%d] erase %d with %04x\n", crow, ccol, length, currentAttribute));
    erase_at(coordCursor, length);
}

/*
 * Move 'n' lines starting at 'from' to 'to'
 *
 * OPT_PRETTIER_SCROLL is prettier but slower -- it scrolls a line at a time
 *	instead of all at once.
 */

/* move howmany lines starting at from to to */
static void
ntconio_scroll(int from, int to, int n)
{
    SMALL_RECT sRect;
    COORD dest;
    CHAR_INFO fill;
    int scroll_pause;

    scflush();
    if (to == from)
	return;
#if OPT_PRETTIER_SCROLL
    if (ABS(from - to) > 1) {
	ntconio_scroll(from, (from < to) ? to - 1 : to + 1, n);
	if (from < to)
	    from = to - 1;
	else
	    from = to + 1;
    }
#endif
    fill.Char.WHICH_CHAR = ' ';
    fill.Attributes = currentAttribute;

    sRect.Left = 0;
    sRect.Top = (SHORT) from;
    sRect.Right = (SHORT) (csbi.dwMaximumWindowSize.X - 1);
    sRect.Bottom = (SHORT) (from + n - 1);

    dest.X = 0;
    dest.Y = (SHORT) to;

    ScrollConsoleScreenBuffer(hConsoleOutput, &sRect, NULL, dest, &fill);
    if ((scroll_pause = global_g_val(GVAL_SCROLLPAUSE)) > 0) {
	/*
	 * If the user has cheap video HW (1 MB or less) and
	 * there's a busy background app (say, dev studio), then
	 * the console version of vile can exhibit serious repaint
	 * problems when the display is rapidly scrolled.  By
	 * inserting a user-defined sleep after the scroll, the
	 * video HW has a chance to properly paint before the
	 * next scroll operation.
	 */

	Sleep(scroll_pause);
    }
#if !OPT_PRETTIER_SCROLL
    if (ABS(from - to) > n) {
	DWORD cnt;
	COORD coordCursor;

	coordCursor.X = 0;
	if (to > from) {
	    coordCursor.Y = (SHORT) (from + n);
	    cnt = to - from - n;
	} else {
	    coordCursor.Y = (SHORT) (to + n);
	    cnt = from - to - n;
	}
	cnt *= csbi.dwMaximumWindowSize.X;
	erase_at(coordCursor, cnt);
    }
#endif
}

#if OPT_FLASH
static void
flash_display(void)
{
    DWORD length = term.cols * term.rows;
    DWORD got;
    WORD *buf1;
    WORD *buf2;

    static COORD origin;

    if ((buf1 = typeallocn(WORD, length)) != 0
	&& (buf2 = typeallocn(WORD, length)) != 0) {
	ReadConsoleOutputAttribute(hConsoleOutput, buf1, length, origin, &got);
	ReadConsoleOutputAttribute(hConsoleOutput, buf2, length, origin, &got);
	for (got = 0; got < length; got++) {
	    buf2[got] ^= (FOREGROUND_INTENSITY | BACKGROUND_INTENSITY);
	}
	WriteConsoleOutputAttribute(hConsoleOutput, buf2, length, origin, &got);
	Sleep(200);
	WriteConsoleOutputAttribute(hConsoleOutput, buf1, length, origin, &got);
	free(buf1);
	free(buf2);
    }
}
#endif

static void
ntconio_beep(void)
{
#if	OPT_FLASH
    if (global_g_val(GMDFLASH)) {
	flash_display();
	return;
    }
#endif
    MessageBeep(0xffffffff);
}

/*
 * vile very rarely generates any of the ASCII printing control characters
 * except for a few hand coded routines but we have to support them anyway.
 */

/* put a character at the current position in the current colors */
static void
ntconio_putc(int ch)
{
#define maybe_flush() \
    if (bufpos >= ((int) sizeof(linebuf) - 2)) \
	scflush()

    if (ch >= ' ') {

	/* This is an optimization for the most common case. */
	maybe_flush();
	linebuf[bufpos++] = (W32_CHAR) ch;

    } else {

	switch (ch) {

	case '\b':
	    scflush();
	    if (ccol)
		ccol--;
	    break;

	case '\a':
	    ntconio_beep();
	    break;

	case '\t':
	    do {
		maybe_flush();
		linebuf[bufpos++] = ' ';
	    } while ((ccol + bufpos) % 8 != 0);
	    break;

	case '\r':
	    scflush();
	    ccol = 0;
	    break;

	case '\n':
	    scflush();
	    if (crow < csbi.dwMaximumWindowSize.Y - 1) {
		crow++;
	    } else {
		ntconio_scroll(1, 0, csbi.dwMaximumWindowSize.Y - 1);
	    }
	    break;

	default:
	    linebuf[bufpos++] = (W32_CHAR) ch;
	    break;
	}
    }
    maybe_flush();
}

static void
ntconio_eeop(void)
{
    DWORD cnt;
    COORD coordCursor;

    scflush();
    coordCursor.X = (SHORT) ccol;
    coordCursor.Y = (SHORT) crow;
    cnt = csbi.dwMaximumWindowSize.X - ccol
	+ (csbi.dwMaximumWindowSize.Y - crow - 1)
	* csbi.dwMaximumWindowSize.X;
    TRACE2(("ntconio_eeop [%d,%d] erase %d with %04x\n", crow, ccol, cnt, currentAttribute));
    erase_at(coordCursor, cnt);
}

static void
ntconio_rev(UINT attr)
{				/* change video state */
    scflush();
    cattr = attr;
    cbcolor = nbcolor;
    cfcolor = nfcolor;
    rvcolor = (global_g_val(GVAL_VIDEO) & VAREV) ? 1 : 0;
    attr &= (VASPCOL | VACOLOR | VABOLD | VAITAL | VASEL | VAREV);

    TRACE2(("ntconio_rev(%04x) f=%d, b=%d\n", attr, cfcolor, cbcolor));

    if (attr) {
	if (attr & VASPCOL)
	    cfcolor = (VCOLORNUM(attr) & (NCOLORS - 1));
	else if (attr & VACOLOR)
	    cfcolor = ((VCOLORNUM(attr)) & (NCOLORS - 1));

	if (cfcolor == ENUM_UNKNOWN)
	    cfcolor = DFT_FCOLOR;
	if (cbcolor == ENUM_UNKNOWN)
	    cbcolor = DFT_BCOLOR;

	if (attr == VABOLD) {
	    cfcolor |= FOREGROUND_INTENSITY;
	}
	if (attr == VAITAL) {
	    cbcolor |= BACKGROUND_INTENSITY;
	}

	if (attr & (VASEL | VAREV)) {	/* reverse video? */
	    rvcolor ^= 1;
	}

	TRACE2(("...ntconio_rev(%04x) f=%d, b=%d\n", attr, cfcolor, cbcolor));
    }
    set_current_attr();
}

static int
ntconio_cres(const char *res)
{				/* change screen resolution */
    (void) res;
    scflush();
    return 0;
}

static BOOL WINAPI
nthandler(DWORD ctrl_type)
{
    switch (ctrl_type) {
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
	imdying(1);
	break;
    }
    return TRUE;
}

static void
ntconio_set_encoding(ENC_CHOICES code)
{
    my_encoding = code;
}

static ENC_CHOICES
ntconio_get_encoding(void)
{
    return my_encoding;
}

static void
ntconio_open(void)
{
    CONSOLE_CURSOR_INFO newcci;
    BOOL newcci_ok;

    TRACE((T_CALLED "ntconio_open\n"));

    conemu = getenv("ConEmuPID") != NULL;
    set_colors(NCOLORS);
    set_palette(initpalettestr);

    hOldConsoleOutput = 0;

    hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    TRACE(("hConsoleOutput %p\n", hConsoleOutput));

    origcci_ok = GetConsoleCursorInfo(hConsoleOutput, &origcci);

    GetConsoleScreenBufferInfo(hConsoleOutput, &csbi);
    TRACE(("GetConsoleScreenBufferInfo X:%d Y:%d Top:%d Bottom:%d Left:%d Right:%d\n",
	   csbi.dwMaximumWindowSize.X,
	   csbi.dwMaximumWindowSize.Y,
	   csbi.srWindow.Top,
	   csbi.srWindow.Bottom,
	   csbi.srWindow.Left,
	   csbi.srWindow.Right));

    TRACE(("...compare height %d vs %d\n",
	   csbi.dwMaximumWindowSize.Y,
	   csbi.srWindow.Bottom - csbi.srWindow.Top + 1));
    TRACE(("...compare width  %d vs %d\n",
	   csbi.dwMaximumWindowSize.X,
	   csbi.srWindow.Right - csbi.srWindow.Left + 1));

    if (csbi.dwMaximumWindowSize.Y !=
	csbi.srWindow.Bottom - csbi.srWindow.Top + 1
	|| csbi.dwMaximumWindowSize.X !=
	csbi.srWindow.Right - csbi.srWindow.Left + 1) {

	TRACE(("..creating alternate screen buffer\n"));
	hOldConsoleOutput = hConsoleOutput;
	hConsoleOutput = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
						   0, NULL,
						   CONSOLE_TEXTMODE_BUFFER, NULL);
	SetConsoleActiveScreenBuffer(hConsoleOutput);

	GetConsoleScreenBufferInfo(hConsoleOutput, &csbi);
	TRACE(("GetConsoleScreenBufferInfo X:%d Y:%d Top:%d Bottom:%d Left:%d Right:%d\n",
	       csbi.dwMaximumWindowSize.X,
	       csbi.dwMaximumWindowSize.Y,
	       csbi.srWindow.Top,
	       csbi.srWindow.Bottom,
	       csbi.srWindow.Left,
	       csbi.srWindow.Right));

	newcci_ok = GetConsoleCursorInfo(hConsoleOutput, &newcci);
	if (newcci_ok && origcci_ok && newcci.dwSize != origcci.dwSize) {
	    /*
	     * Ensure that user's cursor size prefs are carried forward
	     * in the newly created console.
	     */
	    show_cursor(TRUE, origcci.dwSize);
	}
    }

    originalAttribute = csbi.wAttributes;

    crow = csbi.dwCursorPosition.Y;
    ccol = csbi.dwCursorPosition.X;

    nfcolor = cfcolor = gfcolor;
    nbcolor = cbcolor = gbcolor;
    set_current_attr();

    newscreensize(csbi.dwMaximumWindowSize.Y, csbi.dwMaximumWindowSize.X);

    hConsoleInput = GetStdHandle(STD_INPUT_HANDLE);
    TRACE(("hConsoleInput %p\n", hConsoleInput));

    SetConsoleCtrlHandler(nthandler, TRUE);
    ntconio_set_encoding(enc_DEFAULT);
    returnVoid();
}

static void
ntconio_close(void)
{
    TRACE((T_CALLED "ntconio_close\n"));
    if (chgd_cursor) {
	/* restore cursor */
	show_cursor(TRUE, origcci.dwSize);
    }
    scflush();
    ntconio_move(term.rows - 1, 0);
    currentAttribute = originalAttribute;
    ntconio_eeol();
    ntconio_flush();
    set_current_attr();

    SetConsoleTextAttribute(hConsoleOutput, originalAttribute);
    if (hOldConsoleOutput) {
	TRACE(("...restoring screen buffer\n"));
	SetConsoleActiveScreenBuffer(hOldConsoleOutput);
	CloseHandle(hConsoleOutput);
    }
    SetConsoleCtrlHandler(nthandler, FALSE);
    SetConsoleMode(hConsoleInput, ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    keyboard_open = FALSE;
    returnVoid();
}

static void
ntconio_kopen(void)
{				/* open the keyboard */
    TRACE((T_CALLED "ntconio_kopen (open:%d, was-closed:%d)\n",
	   keyboard_open, keyboard_was_closed));
    if (!keyboard_open) {
	if (hConsoleOutput) {
	    SetConsoleActiveScreenBuffer(hConsoleOutput);
	}
	keyboard_open = TRUE;
#ifdef DONT_USE_ON_WIN95
	SetConsoleCtrlHandler(NULL, TRUE);
#endif
	SetConsoleMode(hConsoleInput, ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT);
    }
    returnVoid();
}

static void
ntconio_kclose(void)
{				/* close the keyboard */
    TRACE((T_CALLED "ntconio_kclose\n"));
    if (keyboard_open) {
	keyboard_open = FALSE;
	keyboard_was_closed = TRUE;
	if (hOldConsoleOutput) {
	    SetConsoleActiveScreenBuffer(hOldConsoleOutput);
	}
#ifdef DONT_USE_ON_WIN95
	SetConsoleCtrlHandler(NULL, FALSE);
#endif
    }
    returnVoid();
}

#define isModified(state) (state & \
		(LEFT_CTRL_PRESSED \
		 | RIGHT_CTRL_PRESSED \
		 | LEFT_ALT_PRESSED \
		 | RIGHT_ALT_PRESSED \
		 | SHIFT_PRESSED))

static int
modified_key(int key, DWORD state)
{
    key |= mod_KEY;
    if (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
	key |= mod_CTRL;
    if (state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
	key |= mod_ALT;
    if (state & SHIFT_PRESSED)
	key |= mod_SHIFT;

    return key;
}

static struct {
    int windows;
    int vile;
} keyxlate[] = {
    /* *INDENT-OFF* */
    { VK_NEXT,		KEY_Next },
    { VK_PRIOR,		KEY_Prior },
    { VK_END,		KEY_End },
    { VK_HOME,		KEY_Home },
    { VK_LEFT,		KEY_Left },
    { VK_RIGHT,		KEY_Right },
    { VK_UP,		KEY_Up },
    { VK_DOWN,		KEY_Down },
    { VK_INSERT,	KEY_Insert },
    { VK_DELETE,	KEY_Delete },
    { VK_HELP,		KEY_Help },
    { VK_SELECT,	KEY_Select },
#if 0
    /* Merely pressing the Alt key generates a VK_MENU key event. */
    { VK_MENU,		KEY_Menu },
#endif
    { VK_F1,		KEY_F1 },
    { VK_F2,		KEY_F2 },
    { VK_F3,		KEY_F3 },
    { VK_F4,		KEY_F4 },
    { VK_F5,		KEY_F5 },
    { VK_F6,		KEY_F6 },
    { VK_F7,		KEY_F7 },
    { VK_F8,		KEY_F8 },
    { VK_F9,		KEY_F9 },
    { VK_F10,		KEY_F10 },
    { VK_F11,		KEY_F11 },
    { VK_F12,		KEY_F12 },
    { VK_F13,		KEY_F13 },
    { VK_F14,		KEY_F14 },
    { VK_F15,		KEY_F15 },
    { VK_F16,		KEY_F16 },
    { VK_F17,		KEY_F17 },
    { VK_F18,		KEY_F18 },
    { VK_F19,		KEY_F19 },
    { VK_F20,		KEY_F20 },
    { VK_F21,		KEY_F21 },
    { VK_F22,		KEY_F22 },
    { VK_F23,		KEY_F23 },
    { VK_F24,		KEY_F24 },
    /* winuser.h stops with VK_F24 */
    /* Allow ^-6 to invoke the alternate-buffer command, a la Unix.  */
    { '6',		'6' },
    /* Support recognition of ^@ */
    { '2',		'2' },
    /* *INDENT-ON* */

};

static int savedChar;
static int saveCount = 0;

static int
decode_key_event(INPUT_RECORD * irp)
{
    DWORD state = irp->Event.KeyEvent.dwControlKeyState;
    int key;
    int i;

    if (!irp->Event.KeyEvent.bKeyDown)
	return (NOKYMAP);

    TRACE(("decode_key_event(%c=%02x, Virtual=%#x,%#x, State=%#lx)\n",
	   irp->Event.KeyEvent.uChar.WHICH_CHAR,
	   irp->Event.KeyEvent.uChar.WHICH_CHAR,
	   irp->Event.KeyEvent.wVirtualKeyCode,
	   irp->Event.KeyEvent.wVirtualScanCode,
	   irp->Event.KeyEvent.dwControlKeyState));

    if ((key = irp->Event.KeyEvent.uChar.WHICH_CHAR) != 0) {
	if (isCntrl(key)) {
	    DWORD cstate = state & ~(LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED);
	    if (isModified(cstate))
		key = modified_key(key, cstate);
	}
	return key;
    }

    key = irp->Event.KeyEvent.wVirtualKeyCode;
    if ((state & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) != 0
	&& (state & (RIGHT_CTRL_PRESSED
		     | LEFT_CTRL_PRESSED
		     | SHIFT_PRESSED)) == state) {
	/*
	 * Control-shift-6 is control/^, control/~ or control/`.
	 */
	if (key == '6'
	    || key == '^'
	    || key == '\036') {
	    TRACE(("...decode_key_event ^^\n"));
	    return '\036';
	}
    }

    key = NOKYMAP;
    for (i = 0; i < (int) TABLESIZE(keyxlate); i++) {
	if (keyxlate[i].windows == irp->Event.KeyEvent.wVirtualKeyCode) {

	    /*
	     * Add the modifiers that we recognize.  Specifically, we don't
	     * care about ENHANCED_KEY, since we already have portable
	     * pageup/pagedown and arrow key bindings that would be lost if we
	     * used the Win32-only definition.
	     */
	    if (isModified(state)) {
		key = modified_key(keyxlate[i].vile, state);
		if (keyxlate[i].vile == '2') {
		    if ((key & mod_CTRL) && ((key & mod_ALT) == 0)) {
			/* either ^2 or ^@, => nul char */

			key = 0;
		    } else if ((key & mod_SHIFT) &&
			       ((key & (mod_ALT | mod_CTRL)) == 0)) {
			/*
			 * this will be mapped to '@' if we let Windows do
			 * the translation
			 */

			key = NOKYMAP;
		    }
		}
	    } else
		key = keyxlate[i].vile;
	    TRACE(("... %#x -> %#x\n", irp->Event.KeyEvent.wVirtualKeyCode, key));
	    break;
	}
    }

    return key;
}

static int
MouseClickSetPos(COORD * result, int *onmode)
{
    WINDOW *wp;

    TRACE(("GETC:setcursor(%d, %d)\n", result->Y, result->X));

    /*
     * If we're getting a button-down in a window, allow it to begin a
     * selection.  A button-down on its modeline will allow resizing the
     * window.
     */
    *onmode = FALSE;
    if ((wp = row2window(result->Y)) != 0) {
	if (result->Y == mode_row(wp)) {
	    *onmode = TRUE;
	    return TRUE;
	}
	return setcursor(result->Y, result->X);
    }
    return FALSE;
}

/*
 * Shrink a window by dragging the modeline
 */
static int
adjust_window(WINDOW *wp, COORD * current, COORD * latest)
{
    if (latest->Y == mode_row(wp)) {
	if (current->Y != latest->Y) {
	    WINDOW *save_wp = curwp;
	    set_curwp(wp);
	    shrinkwind(FALSE, latest->Y - current->Y);
	    set_curwp(save_wp);
	    update(TRUE);
	}
	*latest = *current;
	return TRUE;
    }
    return FALSE;
}

/*
 * Return the current mouse position scaled in character cells, with the
 * understanding that the cursor might not be in the client rect (due to a
 * captured mouse).  The right way to do this is to call GetCursorPos() and
 * convert that info to a character cell location by performing some simple
 * computations based on the current font geometry, as is done in ntwinio.c .
 * However, I've not found a way to obtain the font used in a command
 * prompt's (aka DOS box's) client area (yes, I did try GetTextMetrics()).
 * Without font info, other mechanisms are required.
 *
 * Note: GetMousePos() only returns accurate Y coordinate info, because
 * that's all the info AutoScroll needs to make its decision.
 */
static void
GetMousePos(POINT * result)
{
    HWND hwnd;

    hwnd = GetVileWindow();
    GetCursorPos(result);
    ScreenToClient(hwnd, result);
    if (result->y < client_rect.top) {
	/*
	 * mouse is above the editor's client area, return a suitable
	 * character cell location that properly triggers autoscroll
	 */

	result->y = -1;
    } else if (result->y > client_rect.bottom) {
	/*
	 * mouse is below the editor's client area, return a suitable
	 * character cell location that properly triggers autoscroll
	 */

	result->y = term.rows;
    } else if (result->x < client_rect.left || result->x > client_rect.right) {
	/*
	 * dragged mouse out of client rectangle on either the left or
	 * right side.  don't know where the cursor really is, but it's
	 * easy to forestall autoscroll by returning a legit mouse
	 * coordinate within the current editor window.
	 */

	result->y = mouse_wp->w_toprow;
    } else {
	/* cursor within client area. */

	result->y /= row_height;
    }
}

// Get current mouse position.  If the mouse is above/below the current
// window then scroll the window down/up proportionally to the time the LMB
// is held down.  This function is called from autoscroll_thread() when the
// mouse is captured and a wipe selection is active.
static void
AutoScroll(WINDOW *wp)
{
#define DVSR    10
#define INCR    6
#define TRIGGER (DVSR + INCR)

    POINT current;
    int Scroll = 0;
    static int ScrollCount = 0, Throttle = INCR;

    GetMousePos(&current);

    if (wp == 0)
	return;

    // Determine if we are above or below the window,
    // and if so, how far...
    if (current.y < wp->w_toprow) {
	// Above the window
	// Scroll = wp->w_toprow - current.y;
	Scroll = 1;
    }
    if (current.y > mode_row(wp)) {
	// Below
	// Scroll = current.y - mode_row(wp);
	// Scroll *= -1;
	Scroll = -1;
    }
    if (Scroll) {
	int row;
	if (Scroll > 0) {
	    row = wp->w_toprow;
	} else {
	    row = mode_row(wp) - 1;
	}

	// Scroll the pre-determined amount, ensuring at least one line of
	// window movement per timer tick.  Note also that ScrollScale is
	// signed, so it will be negative if we want to scroll down.
	mvupwind(TRUE, Scroll * max(ScrollCount, TRIGGER) / (Throttle + DVSR));

	// Set the cursor. Column doesn't really matter, it will
	// get updated as soon as we get back into the window...
	if (setcursor(row, 0)) {
	    sel_extend(TRUE, TRUE);
	}
	(void) update(TRUE);
	ScrollCount++;
	if (ScrollCount > TRIGGER && Throttle > 0 && ScrollCount % INCR == 0)
	    Throttle--;
    } else {
	// Reset counters
	Throttle = INCR;
	ScrollCount = 0;
    }
#undef DVSR
#undef INCR
#undef TRIGGER
}

static void
halt_autoscroll_thread(void)
{
    buttondown = FALSE;
    (void) ReleaseCapture();	/* Release captured mouse */

    /* Wait for autoscroll thread to exit screen processing */
    (void) WaitForSingleObject(hAsMutex, AS_TMOUT);
    (void) CloseHandle(hAsMutex);
}

static void
autoscroll_thread(void *unused)
{
    DWORD status;

    (void) unused;
    for_ever {
	status = WaitForSingleObject(hAsMutex, AS_TMOUT);
	if (!buttondown) {
	    (void) ReleaseMutex(hAsMutex);
	    break;		/* button no longer held down, die */
	}
	if (status == WAIT_ABANDONED) {
	    /* main thread closed thread handle or ??? */

	    (void) ReleaseMutex(hAsMutex);
	    break;
	}
	if (status == WAIT_OBJECT_0) {
	    /* thread got mutex and "owns" the display. */

	    AutoScroll(mouse_wp);
	}
	(void) ReleaseMutex(hAsMutex);
	Sleep(25);		/* Don't hog the processor */
    }
}

/*
 * FUNCTION
 *   mousemove(int    *sel_pending,
 *             POINT  *first,
 *             POINT  *current,
 *             MARK   *lmbdn_mark,
 *             int    rect_rgn)
 *
 *   sel_pending - Boolean, T -> client has recorded a left mouse button (LMB)
 *                 click, and so, a selection is pending.
 *
 *   first       - editor row/col coordinates where LMB was initially recorded.
 *
 *   latest      - during the mouse move, assuming the LMB is still down,
 *                 "latest" tracks the cursor position wrt window resizing
 *                 operations (via a modeline drag).
 *
 *   current     - current cursor row/col coordinates.
 *
 *   lmbdn_mark  - editor MARK when "LMB down" was initially recorded.
 *
 *   rect_rgn    - Boolean, T -> user wants rectangular region selection.
 *
 * DESCRIPTION
 *   Using several state variables, this function handles all the semantics
 *   of a left mouse button "MOVE" event.  The semantics are as follows:
 *
 *   1) This function will not be called unless the LMB is down and the
 *      cursor is not being used to drage the mode line (enforced by caller).
 *   2) a LMB move within the current editor window selects a region of text.
 *      Later, when the user releases the LMB, that text is yanked to the
 *      unnamed register (the yank code is not handled in this function).
 *
 * RETURNS
 *   None
 */
static void
mousemove(int *sel_pending,
	  COORD * first,
	  COORD * current,
	  MARK *lmbdn_mark,
	  int rect_rgn)
{
    int dummy;

    if (WaitForSingleObject(hAsMutex, AS_TMOUT) == WAIT_OBJECT_0) {
	if (*sel_pending) {
	    /*
	     * Selection pending.  If the mouse has moved at least one char,
	     * start a selection.
	     */

	    if (MouseClickSetPos(current, &dummy)) {
		/* ignore mouse jitter */

		if (current->X != first->X || current->Y != first->Y) {
		    *sel_pending = FALSE;
		    DOT = *lmbdn_mark;
		    (void) sel_begin();
		    (void) update(TRUE);
		} else {
		    (void) ReleaseMutex(hAsMutex);
		    return;
		}
	    }
	}
	if (mouse_wp != row2window(current->Y)) {
	    /*
	     * mouse moved into a different editor window or row2window()
	     * returned a NULL ptr.
	     */

	    (void) ReleaseMutex(hAsMutex);
	    return;
	}
	if (!setcursor(current->Y, current->X)) {
	    (void) ReleaseMutex(hAsMutex);
	    return;
	}
	if (rect_rgn)
	    (void) sel_setshape(rgn_RECTANGLE);
	if (sel_extend(TRUE, TRUE))
	    (void) update(TRUE);
	(void) ReleaseMutex(hAsMutex);
    }
    /*
     * Else either the worker thread abandoned the mutex (not possible as
     * currently coded) or timed out.  If the latter, something is
     * hung--don't do anything.
     */
}

static void
handle_mouse_event(MOUSE_EVENT_RECORD mer)
{
    static DWORD lastclick = 0;
    static int clicks = 0;

    int onmode = FALSE;
    COORD current, first, latest;
    MARK lmbdn_mark;		/* left mouse button down here */
    int sel_pending = 0, state;
    DWORD thisclick;
    UINT clicktime = GetDoubleClickTime();

    memset(&first, 0, sizeof(first));
    memset(&latest, 0, sizeof(latest));
    memset(&current, 0, sizeof(current));
    memset(&lmbdn_mark, 0, sizeof(lmbdn_mark));
    buttondown = FALSE;
    for_ever {
	current = mer.dwMousePosition;
	switch (mer.dwEventFlags) {
	case 0:
	    state = mer.dwButtonState;
	    if (state == 0) {	/* button released */
		thisclick = GetTickCount();
		TRACE(("CLICK %ld/%ld\n", lastclick, thisclick));
		if (thisclick - lastclick < clicktime) {
		    clicks++;
		    TRACE(("MOUSE CLICKS %d\n", clicks));
		} else {
		    clicks = 0;
		}
		lastclick = thisclick;

		switch (clicks) {
		case 1:
		    on_double_click();
		    break;
		case 2:
		    on_triple_click();
		    break;
		}

		if (buttondown) {
		    int dummy;

		    halt_autoscroll_thread();

		    /* Finalize cursor position. */
		    (void) MouseClickSetPos(&current, &dummy);
		    if (!(onmode || sel_pending))
			sel_yank(0);
		}
		return;
	    }
	    if (state & FROM_LEFT_1ST_BUTTON_PRESSED) {
		if (MouseClickSetPos(&current, &onmode)) {
		    first = latest = current;
		    lmbdn_mark = DOT;
		    sel_pending = FALSE;
		    mouse_wp = row2window(latest.Y);
		    if (onmode) {
			buttondown = TRUE;
			sel_release();
			update(TRUE);
		    } else {
			HWND hwnd;

			(void) update(TRUE);	/* possible wdw change */
			buttondown = FALSE;	/* until all inits are successful */

			/* Capture mouse to console vile's window handle. */
			hwnd = GetVileWindow();
			(void) SetCapture(hwnd);

			/* Compute pixel height of each row on screen. */
			(void) GetClientRect(hwnd, &client_rect);
			row_height = client_rect.bottom / term.rows;

			/*
			 * Create mutex to ensure that main thread and worker
			 * thread don't update display at the same time.
			 */
			if ((hAsMutex = CreateMutex(0, FALSE, 0)) == NULL)
			    mlforce("[Can't create autoscroll mutex]");
			else {
			    /*
			     * Setup a worker thread to act as a pseudo
			     * timer that kicks off autoscroll when
			     * necessary.
			     */

			    if (_beginthread(autoscroll_thread,
					     0,
					     NULL) == (unsigned long) -1) {
				(void) CloseHandle(hAsMutex);
				mlforce("[Can't create autoscroll thread]");
			    } else
				sel_pending = buttondown = TRUE;
			}
			if (!buttondown)
			    (void) ReleaseCapture();
		    }
		}
	    } else if (state & FROM_LEFT_2ND_BUTTON_PRESSED) {
		if (MouseClickSetPos(&current, &onmode)
		    && !onmode) {
		    sel_yank(0);
		    sel_release();
		    paste_selection();
		    (void) update(TRUE);
		}
		return;
	    } else {
		if (MouseClickSetPos(&current, &onmode)
		    && onmode) {
		    sel_release();
		    update(TRUE);
		} else {
		    kbd_alarm();
		}
	    }
	    break;

	case MOUSE_MOVED:
	    if (!buttondown)
		return;
	    if (onmode) {
		/* on mode line, resize window (if possible). */

		if (!adjust_window(mouse_wp, &current, &latest)) {
		    /*
		     * left mouse button still down, but cursor moved off mode
		     * line.  Update latest to keep track of cursor in case
		     * it wanders back on the mode line.
		     */

		    latest = current;
		}
	    } else {
		mousemove(&sel_pending,
			  &first,
			  &current,
			  &lmbdn_mark,
			  (mer.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
		    );
	    }
	    break;

#ifdef MOUSE_WHEELED
	case MOUSE_WHEELED:
	    /*
	     * Trial and error experimentation shows that dwButtonState
	     * has its high bit set when the wheel moves back and not
	     * set otherwise.
	     */
	    mvupwind(TRUE, ((long) mer.dwButtonState < 0) ? -3 : 3);
	    update(TRUE);
	    return;
#endif /* MOUSE_WHEELED */
	}

	for_ever {
	    INPUT_RECORD ir;
	    DWORD nr;
	    int key;

	    if (!ReadConsoleInput(hConsoleInput, &ir, 1, &nr))
		imdying(0);
	    switch (ir.EventType) {
	    case KEY_EVENT:
		key = decode_key_event(&ir);
		if (key == ESC) {
		    if (buttondown)
			halt_autoscroll_thread();
		    sel_release();
		    (void) update(TRUE);
		    return;
		}
		continue;

	    case MOUSE_EVENT:
		mer = ir.Event.MouseEvent;
		break;
	    }
	    break;
	}
    }
}

static int
ntconio_getch(void)
{
    INPUT_RECORD ir;
    DWORD nr;
    int key;
#ifdef VAL_AUTOCOLOR
    int milli_ac, orig_milli_ac;
#endif

    TRACE((T_CALLED "ntconio_getch saveCount:%d\n", saveCount));

    if (saveCount > 0) {
	saveCount--;
	returnCode(savedChar);
    }
#ifdef VAL_AUTOCOLOR
    orig_milli_ac = global_b_val(VAL_AUTOCOLOR);
#endif
    for_ever {
#ifdef VAL_AUTOCOLOR
	milli_ac = orig_milli_ac;
	while (milli_ac > 0) {
	    if (PeekConsoleInput(hConsoleInput, &ir, 1, &nr) == 0) {
		TRACE(("PeekConsoleInput failed\n"));
		break;		/* ?? system call failed ?? */
	    }
	    TRACE(("PeekConsoleInput nr %ld\n", nr));
	    if (nr > 0)
		break;		/* something in the queue */
	    Sleep(20);		/* sleep a bit, but be responsive to keybd input */
	    milli_ac -= 20;
	}
	if (orig_milli_ac && milli_ac <= 0) {
	    ac_active = TRUE;
	    autocolor();
	    ac_active = FALSE;
	}
#endif
	if (!ReadConsoleInput(hConsoleInput, &ir, 1, &nr))
	    imdying(0);
	switch (ir.EventType) {

	case KEY_EVENT:
	    key = decode_key_event(&ir);
	    if (key == NOKYMAP)
		continue;
	    if (ir.Event.KeyEvent.wRepeatCount > 1) {
		saveCount = ir.Event.KeyEvent.wRepeatCount - 1;
		savedChar = key;
	    }
	    returnCode(key);

	case WINDOW_BUFFER_SIZE_EVENT:
	    GetConsoleScreenBufferInfo(hConsoleOutput, &csbi);
	    newscreensize(
			     ir.Event.WindowBufferSizeEvent.dwSize.Y,
			     ir.Event.WindowBufferSizeEvent.dwSize.X
		);
	    continue;

	case MOUSE_EVENT:
	    handle_mouse_event(ir.Event.MouseEvent);
	    continue;

	}
    }
}

/*
 * The function `kbhit' returns true if there are *any* input records
 * available.  We need to define our own type ahead routine because
 * otherwise events which we will discard (like pressing or releasing
 * the Shift key) can block screen updates because `ntconio_getch' won't
 * return until a ordinary key event occurs.
 */

static int
ntconio_typahead(void)
{
    INPUT_RECORD ir;
    DWORD nr;
    int key;

    if (!keyboard_open)
	return 0;
#ifdef VAL_AUTOCOLOR
    if (ac_active) {
	/*
	 * Came here during an autocolor operation.  Do nothing, in an
	 * attempt to avoid a keyboard lockup (editor loop) that occurs on
	 * rare occasions (not reproducible).
	 */

	return (0);
    }
#endif
    if (saveCount > 0)
	return 1;

    for_ever {
	if (!PeekConsoleInput(hConsoleInput, &ir, 1, &nr))
	    return 0;

	if (nr == 0)
	    break;

	switch (ir.EventType) {

	case KEY_EVENT:
	    key = decode_key_event(&ir);
	    if (key < 0) {
		ReadConsoleInput(hConsoleInput, &ir, 1, &nr);
		continue;
	    }
	    return 1;

	default:
	    /* Ignore type-ahead for non-keyboard events. */
	    return 0;
	}
    }

    return 0;
}

void
ntcons_reopen(void)
{
    /* If we are coming back from a shell command, pick up the current window
     * size, since that may have changed due to a 'mode con' command.  Run
     * this after the 'pressreturn()' call, since otherwise that gets lost
     * by side-effects of this code.
     */
    if (keyboard_was_closed) {
	CONSOLE_SCREEN_BUFFER_INFO temp;
	keyboard_was_closed = FALSE;
	GetConsoleScreenBufferInfo(hConsoleOutput, &temp);
	newscreensize(temp.dwMaximumWindowSize.Y, temp.dwMaximumWindowSize.X);
    }
}

#if OPT_ICURSOR

/* supported syntax is described in chgd_icursor() */
static int
parse_icursor_string(char *str, int *revert_cursor)
{
    int failed, rc = TRUE, tmp = 0;
    char *pinsmode, *pcmdmode;

    *revert_cursor = FALSE;
    pinsmode = str;
    if ((pcmdmode = strchr(pinsmode, ',')) != NULL) {
	/* vl_atoul won't like the delimiter... */

	tmp = *pcmdmode;
	*pcmdmode = '\0';
    }
    icursor_insmode = (int) vl_atoul(pinsmode, 10, &failed);
    if (pcmdmode)
	*pcmdmode = (char) tmp;	/* delimiter restored */
    if (failed)
	return (FALSE);
    if (pcmdmode) {
	icursor_cmdmode = (int) vl_atoul(pcmdmode + 1, 10, &failed);
	if (failed)
	    return (FALSE);
    } else {
	/* block mode syntax */

	icursor_cmdmode = icursor_insmode;
    }

    /* semantic chks */
    if (icursor_insmode == 0) {
	if (!pcmdmode)
	    *revert_cursor = TRUE;
	else
	    rc = FALSE;		/* 0% insmode cursor block heights not valid */
	return (rc);
    }
    if (icursor_cmdmode == 0)
	rc = FALSE;		/* 0% cmdmode cursor block heights not valid */
    else {
	if (icursor_cmdmode > MAX_CURBLK_HEIGHT ||
	    icursor_insmode > MAX_CURBLK_HEIGHT) {
	    rc = FALSE;
	}
    }
    return (rc);
}

/*
 * user changed icursor mode
 *
 * Insertion cursor mode is a string that may be used to either set a fixed
 * block cursor height or set the block cursor heights in insertion and
 * command mode.  Supported syntax:
 *
 *     "<fixed_block_height>"
 *
 *              or
 *
 *     "<insmode_height>,<cmdmode_height>"
 *
 * The valid range of <fixed_block_cursor_height> is 0-100.  Specifying 0
 * forces the editor to revert to the cursor height in effect when the
 * editor was invoked.
 *
 * The valid range of <insmode_height> and <cmdmode_height> is 1-100.
 */
int
chgd_icursor(BUFFER *bp, VALARGS * args, int glob_vals, int testing)
{
    (void) bp;
    (void) glob_vals;

    if (!testing) {
	int revert_cursor;
	char *val = args->global->vp->p;

	if (!parse_icursor_string(val, &revert_cursor)) {
	    mlforce("[invalid icursor syntax]");
	    return (FALSE);
	}
	if (!revert_cursor) {
	    chgd_cursor = TRUE;
	    icursor = (icursor_insmode != icursor_cmdmode);
	    if (icursor)
		term.icursor(insertmode);
	    else {
		/* just set a block cursor */
		show_cursor(TRUE, icursor_cmdmode);
	    }
	} else {
	    /*
	     * user wants to disable previous changes made to cursor,
	     * thereby reverting to cursor height in effect when the editor
	     * was invoked.
	     */

	    if (chgd_cursor) {
		/*
		 * NB -- don't reset "chgd_cursor" here.  special cleanup
		 * is required in ntconio_close().
		 */

		if (origcci_ok)
		    show_cursor(TRUE, origcci.dwSize);
	    }
	}
    }
    return (TRUE);
}
#endif

/*
 * Standard terminal interface dispatch table. None of the fields point into
 * "termio" code.
 */

TERM term =
{
    NROW,
    NROW,
    NCOL,
    NCOL,
    ntconio_set_encoding,
    ntconio_get_encoding,
    ntconio_open,
    ntconio_close,
    ntconio_kopen,
    ntconio_kclose,
    nullterm_clean,
    nullterm_unclean,
    nullterm_openup,
    ntconio_getch,
    ntconio_putc,
    ntconio_typahead,
    ntconio_flush,
    ntconio_move,
    ntconio_eeol,
    ntconio_eeop,
    ntconio_beep,
    ntconio_rev,
    ntconio_cres,
#if OPT_COLOR
    ntconio_fcol,
    ntconio_bcol,
    set_ctrans,
#else
    nullterm_setfore,
    nullterm_setback,
    nullterm_setpal,
#endif
    nullterm_setccol,
    ntconio_scroll,
    nullterm_pflush,
#if OPT_ICURSOR
    nticursor,
#else
    nullterm_icursor,
#endif
#if OPT_TITLE
    ntconio_title,
#else
    nullterm_settitle,
#endif
    nullterm_watchfd,
    nullterm_unwatchfd,
    nullterm_cursorvis,
    nullterm_mopen,
    nullterm_mclose,
    nullterm_mevent,
};
