/*
 * Uses the Win32 console API.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/ntconio.c,v 1.58 2000/07/24 22:56:33 cmorgan Exp $
 *
 */

#include <windows.h>
#include <process.h>

#include        "estruct.h"
#include        "edef.h"

#undef RECT			/* FIXME: symbol conflict */

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

#ifdef VAL_AUTOCOLOR
static int ac_active = FALSE;	/* autocolor active */
#endif

/* ansi to ibm color translation table */
static const char *initpalettestr = "0 4 2 6 1 5 3 7 8 12 10 14 9 13 11 15";
/* black, red, green, yellow, blue, magenta, cyan, white   */

static char linebuf[NCOL];
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
    CONSOLE_CURSOR_INFO oldcci, newcci;
    BOOL oldcci_ok, newcci_ok;

    TRACE(("ntopen\n"));

    set_colors(NCOLORS);
    set_palette(initpalettestr);

    hOldConsoleOutput = 0;
    hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hConsoleOutput, &csbi);
    if (csbi.dwMaximumWindowSize.Y !=
	csbi.srWindow.Bottom - csbi.srWindow.Top + 1
	|| csbi.dwMaximumWindowSize.X !=
	csbi.srWindow.Right - csbi.srWindow.Left + 1) {
	hOldConsoleOutput = hConsoleOutput;
	oldcci_ok = GetConsoleCursorInfo(hConsoleOutput, &oldcci);
	hConsoleOutput = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
	    0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
	SetConsoleActiveScreenBuffer(hConsoleOutput);
	GetConsoleScreenBufferInfo(hConsoleOutput, &csbi);
	newcci_ok = GetConsoleCursorInfo(hConsoleOutput, &newcci);
	if (newcci_ok && oldcci_ok && newcci.dwSize != oldcci.dwSize) {
	    /*
	     * Ensure that user's cursor size prefs are carried forward
	     * in the newly created console.
	     */

	    newcci.dwSize = oldcci.dwSize;
	    (void) SetConsoleCursorInfo(hConsoleOutput, &newcci);
	}
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

    hwnd = GetForegroundWindow();
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
	setcursor(row, 0) && sel_extend(TRUE, TRUE);
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

    while (1) {
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
 *   current     - current cursor row/col coordiantes.
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
    MARK * lmbdn_mark,
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
	    (void) sel_setshape(RECTANGLE);
	if (sel_extend(TRUE, TRUE))
	    (void) update(TRUE);
	(void) ReleaseMutex(hAsMutex);
    }
    /* 
     * Else either the worker thread abandonded the mutex (not possible as
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
    int sel_pending, state;
    DWORD thisclick, status;
    UINT clicktime = GetDoubleClickTime();

    buttondown = FALSE;
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
			hwnd = GetForegroundWindow();
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
				    NULL) == -1) {
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
