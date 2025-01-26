/*
 * $Id: xterm.c,v 1.13 2025/01/26 20:54:00 tom Exp $
 *
 * xterm-specific code for vi-like-emacs.
 */

#include	"estruct.h"
#include	"edef.h"

#if DISP_TERMCAP || DISP_CURSES

#define putpad(s)	vl_fputs(s, stdout)
#define	XtermPos()	(keystroke() - 040)

#if OPT_XTERM >= 3
# define XTERM_ENABLE_TRACKING   "\033[?1001h"	/* mouse hilite tracking */
# define XTERM_DISABLE_TRACKING  "\033[?1001l"
#else
# if OPT_XTERM >= 2
#  define XTERM_ENABLE_TRACKING   "\033[?1000h"		/* normal tracking mode */
#  define XTERM_DISABLE_TRACKING  "\033[?1000l"
# else
#  define XTERM_ENABLE_TRACKING   "\033[?9h"	/* X10 compatibility mode */
#  define XTERM_DISABLE_TRACKING  "\033[?9l"
# endif
#endif

static int x_origin = 1, y_origin = 1;

void
xterm_open(TERM * tp)
{
    x_origin = 0;
    y_origin = 0;

    /* use xterm #251 feature to save title */
    putpad("\033[22t");

    if (tp != NULL) {
	tp->set_title = xterm_settitle;
	tp->mopen = xterm_mopen;
	tp->mclose = xterm_mclose;

#if OPT_XTERM
	addtosysmap("\033[M", 3, KEY_Mouse);
#if OPT_XTERM >= 3
	addtosysmap("\033[t", 3, KEY_text);
	addtosysmap("\033[T", 3, KEY_textInvalid);
#endif
#endif
    }
}

void
xterm_close(void)
{
    /* use xterm #251 feature to restore title */
    putpad("\033[23t");
}

void
xterm_mopen(void)
{
    TRACE((T_CALLED "xterm_mopen()\n"));
    if (global_g_val(GMDXTERM_MOUSE)) {
	putpad(XTERM_ENABLE_TRACKING);
	fflush(stdout);
    }
    returnVoid();
}

void
xterm_mclose(void)
{
    TRACE((T_CALLED "xterm_mclose()\n"));
    if (global_g_val(GMDXTERM_MOUSE)) {
	putpad(XTERM_DISABLE_TRACKING);
	fflush(stdout);
    }
    returnVoid();
}

/* Finish decoding a mouse-click in an xterm, after the ESC and '[' chars.
 *
 * There are 3 mutually-exclusive xterm mouse-modes (selected here by values of
 * OPT_XTERM):
 *	(1) X10-compatibility (not used here)
 *		Button-press events are received.
 *	(2) normal-tracking
 *		Button-press and button-release events are received.
 *		Button-events have modifiers (e.g., shift, control, meta).
 *	(3) hilite-tracking
 *		Button-press and text-location events are received.
 *		Button-events have modifiers (e.g., shift, control, meta).
 *		Dragging with the mouse produces highlighting.
 *		The text-locations are checked by xterm to ensure validity.
 *
 * NOTE:
 *	The hilite-tracking code is here for testing and (later) use.  Because
 *	we cannot guarantee that we always are decoding escape-sequences when
 *	reading from the terminal, there is the potential for the xterm hanging
 *	when a mouse-dragging operation is begun: it waits for us to specify
 *	the screen locations that limit the highlighting.
 *
 *	While highlighting, the xterm accepts other characters, but the display
 *	does not appear to be refreshed until highlighting is ended. So (even
 *	if we always capture the beginning of highlighting) we cannot simply
 *	loop here waiting for the end of highlighting.
 *
 *	1993/aug/6 dickey@software.org
 */

static int xterm_button(int c);

/*ARGSUSED*/
int
mouse_motion(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return xterm_button('M');
}

#if OPT_XTERM >= 3
/*ARGSUSED*/
int
xterm_mouse_t(int f, int n)
{
    return xterm_button('t');
}

/*ARGSUSED*/
int
xterm_mouse_T(int f, int n)
{
    return xterm_button('T');
}
#endif /* OPT_XTERM >= 3 */

static int
xterm_button(int c)
{
    int event;
    int button;
    int x;
    int y;
    int status;
#if OPT_XTERM >= 3
    WINDOW *wp;
    int save_row = ttrow;
    int save_col = ttcol;
    int firstrow, lastrow;
    int startx, endx, mousex;
    int starty, endy, mousey;
    MARK save_dot;
    char temp[NSTRING];
    static const char *fmt = "\033[%d;%d;%d;%d;%dT";
#endif /* OPT_XTERM >= 3 */

    if ((status = (global_g_val(GMDXTERM_MOUSE))) != 0) {
	beginDisplay();
	switch (c) {
#if OPT_XTERM < 3
	    /*
	     * If we get a click on a modeline, clear the current selection,
	     * if any.  Allow implied movement of the mouse (distance between
	     * pressing and releasing a mouse button) to drag the modeline.
	     *
	     * Likewise, if we get a click _not_ on a modeline, make that
	     * start/extend a selection.
	     */
	case 'M':		/* button-event */
	    event = keystroke();
	    x = XtermPos() + x_origin - 1;
	    y = XtermPos() + y_origin - 1;
	    button = (event & 3) + 1;

	    if (button > 3)
		button = 0;
	    TRACE(("MOUSE-button event %d -> btn %d loc %d.%d\n",
		   event, button, y, x));
	    status = on_mouse_click(button, y, x);
	    break;
#else /* OPT_XTERM >=3, highlighting mode */
	case 'M':		/* button-event */
	    event = keystroke();
	    x = XtermPos() + x_origin;
	    y = XtermPos() + y_origin;

	    button = (event & 3) + 1;
	    TRACE(("MOUSE-button event:%d x:%d y:%d\n", event, x, y));
	    if (button > 3) {
		endofDisplay();
		return TRUE;	/* button up */
	    }
	    wp = row2window(y - 1);
	    if (insertmode && wp != curwp) {
		kbd_alarm();
		return ABORT;
	    }
	    /* Tell the xterm how to highlight the selection.
	     * It won't do anything else until we do this.
	     */
	    if (wp != 0) {
		firstrow = wp->w_toprow + 1;
		lastrow = mode_row(wp) + 1;
	    } else {		/* from message-line */
		firstrow = term.rows;
		lastrow = term.rows + 1;
	    }
	    if (y >= lastrow)	/* don't select modeline */
		y = lastrow - 1;
	    (void) lsprintf(temp, fmt, 1, x, y, firstrow, lastrow);
	    putpad(temp);
	    term.flush();
	    /* Set the dot-location if button 1 was pressed in a
	     * window.
	     */
	    if (wp != 0
		&& button == 1
		&& !reading_msg_line
		&& setcursor(y - 1, x - 1)) {
		(void) update(TRUE);
		status = TRUE;
	    } else if (button <= 3) {
		/* abort the selection */
		(void) lsprintf(temp, fmt, 0, x, y, firstrow, lastrow);
		putpad(temp);
		term.flush();
		status = ABORT;
	    } else {
		status = FALSE;
	    }
	    break;
	case 't':		/* reports valid text-location */
	    x = XtermPos();
	    y = XtermPos();

	    TRACE(("MOUSE-valid: x:%d y:%d\n", x, y));
	    setwmark(y - 1, x - 1);
	    yankregion();

	    movecursor(save_row, save_col);
	    (void) update(TRUE);
	    break;
	case 'T':		/* reports invalid text-location */
	    /*
	     * The starting-location returned is not the location
	     * at which the mouse was pressed.  Instead, it is the
	     * top-most location of the selection.  In turn, the
	     * ending-location is the bottom-most location of the
	     * selection.  The mouse-up location is not necessarily
	     * a pointer to valid text.
	     *
	     * This case handles multi-clicking events as well as
	     * selections whose start or end location was not
	     * pointing to text.
	     */
	    save_dot = DOT;
	    startx = XtermPos();	/* starting-location */
	    starty = XtermPos();
	    endx = XtermPos();	/* ending-location */
	    endy = XtermPos();
	    mousex = XtermPos();	/* location at mouse-up */
	    mousey = XtermPos();

	    TRACE(("MOUSE-invalid: start(%d,%d) end(%d,%d) mouse(%d,%d)\n",
		   starty, startx,
		   endy, endx,
		   mousey, mousex));
	    setcursor(starty - 1, startx - 1);
	    setwmark(endy - 1, endx - 1);
	    if (MK.o != 0 && !is_at_end_of_line(MK)) {
		MK.o += BytesAt(MK.l, MK.o);
	    }
	    yankregion();

	    DOT = save_dot;
	    movecursor(save_row, save_col);
	    (void) update(TRUE);
	    break;
#endif /* OPT_XTERM < 3 */
	default:
	    status = FALSE;
	}
	endofDisplay();
    }
    return status;
}

void
xterm_settitle(const char *string)
{
    if (global_g_val(GMDXTERM_TITLE) && string != NULL) {
#if OPT_MULTIBYTE
	int check;
	UINT ch;
	UCHAR temp[MAX_UTF8];
	ENC_CHOICES want_encoding = title_encoding;
#endif
	TRACE(("xterm_settitle(%s)\n", string));
	putpad("\033]0;");
#if OPT_MULTIBYTE
	if (want_encoding == enc_LOCALE) {
	    if (vl_encoding >= enc_UTF8) {
		want_encoding = enc_UTF8;
	    } else {
		want_encoding = enc_8BIT;
	    }
	}
	if (curbp == NULL) {
	} else if (want_encoding == enc_UTF8 && !b_is_utfXX(curbp)) {
	    TRACE(("...converting to UTF-8\n"));
	    while (*string != EOS) {
		ch = CharOf(*string++);
		if (vl_8bit_to_ucs(&check, (int) ch))
		    ch = (UINT) check;
		else
		    ch = '?';
		check = vl_conv_to_utf8(temp, ch, sizeof(temp));
		temp[check] = EOS;
		putpad((char *) temp);
	    }
	} else if (want_encoding == enc_8BIT && b_is_utfXX(curbp)) {
	    TRACE(("...converting to 8-bit\n"));
	    while (*string != EOS) {
		check = vl_conv_to_utf32(&ch, string, strlen(string));
		if (check <= 0) {
		    string++;
		    continue;
		} else {
		    string += check;
		}
		if (isPrint(ch) && vl_ucs_to_8bit(&check, (int) ch)) {
		    ch = (UINT) (check < 256 ? check : '?');
		} else {
		    ch = '?';
		}
		temp[0] = (UCHAR) ch;
		temp[1] = EOS;
		putpad((char *) temp);
	    }
	} else
#endif
	    putpad(string);
	putpad("\007");
	term.flush();
    }
}
#endif /* DISP_TERMCAP || DISP_CURSES */
