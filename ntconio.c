/*
 * Uses the Win32 console API.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/ntconio.c,v 1.54 2000/01/07 01:55:47 cmorgan Exp $
 *
 */

#include <windows.h>

#include        "estruct.h"
#include        "edef.h"

/*
 * Define this if you want to kernel fault win95 when ctrl brk is pressed.
 */
#undef DONT_USE_ON_WIN95

#define NROW	128		/* Max Screen size.             */
#define NCOL    256		/* Edit if you want to.         */
#define	NPAUSE	200		/* # times thru update to pause */
#define NOKYMAP (-1)

#define	AttrColor(b,f)	((WORD)(((ctrans[b] & 15) << 4) | (ctrans[f] & 15)))

static HANDLE hConsoleOutput;	/* handle to the console display */
static HANDLE hOldConsoleOutput;	/* handle to the old console display */
static HANDLE hConsoleInput;
static CONSOLE_SCREEN_BUFFER_INFO csbi;
static WORD originalAttribute;

static int cfcolor = -1;	/* current forground color */
static int cbcolor = -1;	/* current background color */
static int nfcolor = -1;	/* normal foreground color */
static int nbcolor = -1;	/* normal background color */
static int crow = -1;		/* current row */
static int ccol = -1;		/* current col */
static int keyboard_open = FALSE;	/* keyboard is open */
static int keyboard_was_closed = TRUE;

/* ansi to ibm color translation table */
static const char *initpalettestr = "0 4 2 6 1 5 3 7 8 12 10 14 9 13 11 15";
/* black, red, green, yellow, blue, magenta, cyan, white   */

static char linebuf[NCOL];
static int bufpos = 0;

static void scflush(void);

#ifdef GVAL_VIDEO
static WORD
AttrVideo(int b, int f)
{
    WORD result;
    UINT mask = (global_g_val(GVAL_VIDEO) & VAREV);
    if (mask ^ VAREV) {
	result = AttrColor(b, f);
    } else {
	result = AttrColor(f, b);
    }
    return result;
}
#undef AttrColor
#define AttrColor(b,f) AttrVideo(b,f)
#endif

#if OPT_ICURSOR
static void
nticursor(int cmode)
{
    CONSOLE_CURSOR_INFO cci;

    switch (cmode) {
    case -1:
	cci.dwSize = 0;
	cci.bVisible = FALSE;
	break;
    case 0:
	cci.dwSize = 20;
	cci.bVisible = TRUE;
	break;
    case 1:
	cci.dwSize = 100;
	cci.bVisible = TRUE;
	break;
    }
    SetConsoleCursorInfo(hConsoleOutput, &cci);
}
#endif

#if OPT_TITLE
static void
nttitle(const char *title)
{				/* set the current window title */
    SetConsoleTitle(title);
}
#endif

#if OPT_COLOR
static void
ntfcol(int color)
{				/* set the current output color */
    scflush();
    nfcolor = cfcolor = color;
}

static void
ntbcol(int color)
{				/* set the current background color */
    scflush();
    nbcolor = cbcolor = color;
}
#endif

static void
scflush(void)
{
    if (bufpos) {
	COORD coordCursor;
	DWORD written;

	coordCursor.X = ccol;
	coordCursor.Y = crow;
	WriteConsoleOutputCharacter(
	    hConsoleOutput, linebuf, bufpos, coordCursor, &written
	    );
	FillConsoleOutputAttribute(
	    hConsoleOutput, AttrColor(cbcolor, cfcolor),
	    bufpos, coordCursor, &written
	    );
	ccol += bufpos;
	bufpos = 0;
    }
}

static void
ntflush(void)
{
    COORD coordCursor;

    scflush();
    coordCursor.X = ccol;
    coordCursor.Y = crow;
    SetConsoleCursorPosition(hConsoleOutput, coordCursor);
}

static void
ntmove(int row, int col)
{
    scflush();
    crow = row;
    ccol = col;
}

/* erase to the end of the line */
static void
nteeol(void)
{
    DWORD written;
    COORD coordCursor;

    scflush();
    coordCursor.X = ccol;
    coordCursor.Y = crow;
    FillConsoleOutputCharacter(
	hConsoleOutput, ' ', csbi.dwMaximumWindowSize.X - ccol,
	coordCursor, &written);
    FillConsoleOutputAttribute(
	hConsoleOutput, AttrColor(cbcolor, cfcolor),
	csbi.dwMaximumWindowSize.X - ccol, coordCursor, &written);
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
    SMALL_RECT sRect;
    COORD dest;
    CHAR_INFO fill;
    int scroll_pause;

    scflush();
    if (to == from)
	return;
#if OPT_PRETTIER_SCROLL
    if (absol(from - to) > 1) {
	ntscroll(from, (from < to) ? to - 1 : to + 1, n);
	if (from < to)
	    from = to - 1;
	else
	    from = to + 1;
    }
#endif
    fill.Char.AsciiChar = ' ';
    fill.Attributes = AttrColor(cbcolor, cfcolor);

    sRect.Left = 0;
    sRect.Top = from;
    sRect.Right = csbi.dwMaximumWindowSize.X - 1;
    sRect.Bottom = from + n - 1;

    dest.X = 0;
    dest.Y = to;

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
    if (absol(from - to) > n) {
	DWORD cnt;
	DWORD written;
	COORD coordCursor;

	coordCursor.X = 0;
	if (to > from) {
	    coordCursor.Y = from + n;
	    cnt = to - from - n;
	} else {
	    coordCursor.Y = to + n;
	    cnt = from - to - n;
	}
	cnt *= csbi.dwMaximumWindowSize.X;
	FillConsoleOutputCharacter(
	    hConsoleOutput, ' ', cnt, coordCursor, &written);
	FillConsoleOutputAttribute(
	    hConsoleOutput, AttrColor(cbcolor, cfcolor), cnt,
	    coordCursor, &written);
    }
#endif
}

#if	OPT_FLASH
static void
flash_display(void)
{
    DWORD length = term.cols * term.rows;
    DWORD got;
    WORD *buf1 = malloc(sizeof(*buf1) * length);
    WORD *buf2 = malloc(sizeof(*buf2) * length);
    static COORD origin;
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
	linebuf[bufpos++] = ch;
	return;
    }

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
	do
	    linebuf[bufpos++] = ' ';
	while ((ccol + bufpos) % 8 != 0);
	break;

    case '\r':
	scflush();
	ccol = 0;
	break;

    case '\n':
	scflush();
	if (crow < csbi.dwMaximumWindowSize.Y - 1)
	    crow++;
	else
	    ntscroll(1, 0, csbi.dwMaximumWindowSize.Y - 1);
	break;

    default:
	linebuf[bufpos++] = ch;
	break;
    }
}

static void
nteeop(void)
{
    DWORD cnt;
    DWORD written;
    COORD coordCursor;

    scflush();
    coordCursor.X = ccol;
    coordCursor.Y = crow;
    cnt = csbi.dwMaximumWindowSize.X - ccol
	+ (csbi.dwMaximumWindowSize.Y - crow - 1)
	* csbi.dwMaximumWindowSize.X;
    FillConsoleOutputCharacter(
	hConsoleOutput, ' ', cnt, coordCursor, &written
	);
    FillConsoleOutputAttribute(
	hConsoleOutput, AttrColor(cbcolor, cfcolor), cnt,
	coordCursor, &written
	);
}

static void
ntrev(UINT attr)
{				/* change video state */
    scflush();
    cbcolor = nbcolor;
    cfcolor = nfcolor;
    if (attr) {
	if (attr & VASPCOL)
	    cfcolor = (attr & (NCOLORS - 1));
	else if (attr == VABOLD)
	    cfcolor |= FOREGROUND_INTENSITY;
	else if (attr == VAITAL)
	    cbcolor |= BACKGROUND_INTENSITY;
	else if (attr & VACOLOR)
	    cfcolor = ((VCOLORNUM(attr)) & (NCOLORS - 1));

	if (attr & (VASEL | VAREV)) {	/* reverse video? */
	    int temp = cfcolor;
	    cfcolor = cbcolor;
	    cbcolor = temp;
	}
    }
}

static int
ntcres(const char *res)
{				/* change screen resolution */
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
ntopen(void)
{
    TRACE(("ntopen\n"));
    set_palette(initpalettestr);
    hOldConsoleOutput = 0;
    hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hConsoleOutput, &csbi);
    if (csbi.dwMaximumWindowSize.Y !=
	csbi.srWindow.Bottom - csbi.srWindow.Top + 1
	|| csbi.dwMaximumWindowSize.X !=
	csbi.srWindow.Right - csbi.srWindow.Left + 1) {
	hOldConsoleOutput = hConsoleOutput;
	hConsoleOutput = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
	    0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
	SetConsoleActiveScreenBuffer(hConsoleOutput);
	GetConsoleScreenBufferInfo(hConsoleOutput, &csbi);
    }
    originalAttribute = csbi.wAttributes;
    crow = csbi.dwCursorPosition.Y;
    ccol = csbi.dwCursorPosition.X;
    nfcolor = cfcolor = gfcolor;
    nbcolor = cbcolor = gbcolor;
    newscreensize(csbi.dwMaximumWindowSize.Y, csbi.dwMaximumWindowSize.X);
    hConsoleInput = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleCtrlHandler(nthandler, TRUE);
}

static void
ntclose(void)
{
    TRACE(("ntclose\n"));
    scflush();
    ntmove(term.rows - 1, 0);
    nteeol();
    ntflush();
    SetConsoleTextAttribute(hConsoleOutput, originalAttribute);
    if (hOldConsoleOutput) {
	SetConsoleActiveScreenBuffer(hOldConsoleOutput);
	CloseHandle(hConsoleOutput);
    }
    SetConsoleCtrlHandler(nthandler, FALSE);
    SetConsoleMode(hConsoleInput, ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    keyboard_open = FALSE;
}

static void
ntkopen(void)
{				/* open the keyboard */
    TRACE(("ntkopen (open:%d, was-closed:%d)\n", keyboard_open, keyboard_was_closed));
    if (keyboard_open)
	return;
    if (hConsoleOutput)
	SetConsoleActiveScreenBuffer(hConsoleOutput);
    keyboard_open = TRUE;
#ifdef DONT_USE_ON_WIN95
    SetConsoleCtrlHandler(NULL, TRUE);
#endif
    SetConsoleMode(hConsoleInput, ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT);
}

static void
ntkclose(void)
{				/* close the keyboard */
    TRACE(("ntkclose\n"));
    if (!keyboard_open)
	return;
    keyboard_open = FALSE;
    keyboard_was_closed = TRUE;
    if (hOldConsoleOutput)
	SetConsoleActiveScreenBuffer(hOldConsoleOutput);
#ifdef DONT_USE_ON_WIN95
    SetConsoleCtrlHandler(NULL, FALSE);
#endif
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
    /* Allow ^-6 to invoke the alternate-buffer command, a la Unix.  */
    { '6',		'6' },
    /* *INDENT-ON* */

};

static int savedChar;
static int saveCount = 0;

static int
decode_key_event(INPUT_RECORD * irp)
{
    int key;
    int i;

    if (!irp->Event.KeyEvent.bKeyDown)
	return (NOKYMAP);

    if ((key = (unsigned char) irp->Event.KeyEvent.uChar.AsciiChar) != 0)
	return key;

    for (i = 0; i < TABLESIZE(keyxlate); i++) {
	if (keyxlate[i].windows == irp->Event.KeyEvent.wVirtualKeyCode) {
	    DWORD state = irp->Event.KeyEvent.dwControlKeyState;

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
		    | SHIFT_PRESSED)) {
		key = W32_KEY | keyxlate[i].windows;
		if (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
		    key |= W32_CTRL;
		if (state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
		    key |= W32_ALT;
		if (state & SHIFT_PRESSED)
		    key |= W32_SHIFT;
	    } else
		key = keyxlate[i].vile;
	    break;
	}
    }
    if (key == 0)
	return (NOKYMAP);

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
	    sel_release();
	    return TRUE;
	}
	return setcursor(result->Y, result->X);
    }
    sel_release();
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

static void
handle_mouse_event(MOUSE_EVENT_RECORD mer)
{
    static DWORD lastclick = 0;
    static int clicks = 0;

    int buttondown = FALSE;
    int onmode = FALSE;
    COORD current, latest;
    int state;
    DWORD thisclick;
    WINDOW *that_wp;
    UINT clicktime = GetDoubleClickTime();

    for_ever {
	current = mer.dwMousePosition;
	switch (mer.dwEventFlags) {
	case 0:
	    state = mer.dwButtonState;
	    if (state == 0) {	/* button released */
		thisclick = GetTickCount();
		TRACE(("CLICK %d/%d\n", lastclick, thisclick));
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
		    if (MouseClickSetPos(&current, &onmode))
			sel_yank(0);
		    buttondown = FALSE;
		    onmode = FALSE;
		}
		return;
	    }
	    if (state & FROM_LEFT_1ST_BUTTON_PRESSED) {
		if (MouseClickSetPos(&current, &onmode)) {
		    buttondown = TRUE;
		    latest = current;
		    that_wp = row2window(latest.Y);
		    (void) sel_begin();
		    (void) update(TRUE);
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

	    if (onmode
		&& adjust_window(that_wp, &current, &latest))
		break;

	    if (that_wp != row2window(current.Y))
		break;
	    if (!setcursor(current.Y, current.X))
		break;

	    if (mer.dwControlKeyState &
		(LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
		(void) sel_setshape(RECTANGLE);
	    }
	    if (!sel_extend(TRUE, TRUE))
		break;
	    (void) update(TRUE);
	    break;
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
ntgetch(void)
{
    INPUT_RECORD ir;
    DWORD nr;
    int key;
#ifdef VAL_AUTOCOLOR
    int milli_ac, orig_milli_ac;
#endif

    if (saveCount > 0) {
	saveCount--;
	return savedChar;
    }
#ifdef VAL_AUTOCOLOR
    orig_milli_ac = global_b_val(VAL_AUTOCOLOR);
#endif
    for_ever {
#ifdef VAL_AUTOCOLOR
	milli_ac = orig_milli_ac;
	while (milli_ac > 0) {
	    if (PeekConsoleInput(hConsoleInput, &ir, 1, &nr) == 0)
		break;		/* ?? system call failed ?? */
	    if (nr > 0)
		break;		/* something in the queue */
	    Sleep(20);		/* sleep a bit, but be responsive to keybd input */
	    milli_ac -= 20;
	}
	if (orig_milli_ac && milli_ac <= 0)
	    autocolor();
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
	    return key;

	case WINDOW_BUFFER_SIZE_EVENT:
	    newscreensize(
		ir.Event.WindowBufferSizeEvent.dwSize.Y,
		ir.Event.WindowBufferSizeEvent.dwSize.X
		);
	    GetConsoleScreenBufferInfo(hConsoleOutput, &csbi);
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
 * the Shift key) can block screen updates because `ntgetch' won't
 * return until a ordinary key event occurs.
 */

static int
nttypahead(void)
{
    INPUT_RECORD ir;
    DWORD nr;
    int key;

    if (!keyboard_open)
	return 0;

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
    nullterm_setccol,
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
