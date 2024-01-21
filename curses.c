/*
 * A terminal driver using the curses library
 *
 * $Id: curses.c,v 1.60 2024/01/21 17:53:39 tom Exp $
 */

#include "estruct.h"
#include "edef.h"

#if OPT_LOCALE
#include <locale.h>
#endif

#if DISP_CURSES

#undef WINDOW

#include "tcap.h"

#if OPT_LOCALE && defined(NCURSES_VERSION) && defined(HAVE_ADDNWSTR)
#define USE_MULTIBYTE 1
#else
#define USE_MULTIBYTE 0
#endif

#define is_default(color) (color < 0 || color == 255)

#if defined(SIGWINCH) && defined(HAVE_RESIZETERM)
#define USE_WINCH_CODE 1
#else
#define USE_WINCH_CODE 0
#endif

#if USE_TERMCAP
#  define TCAPSLEN 1024
static char tc_parsed[TCAPSLEN];
#endif

#if OPT_COLOR
	/* ANSI: black, red, green, yellow, blue, magenta, cyan, white   */
static const char ANSI_palette[] =
{"0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"};

#ifdef HAVE_USE_DEFAULT_COLORS
#define DefaultColor(dft) -1
#define DEFAULT_FG        -1
#define DEFAULT_BG        -1
#else
#define DefaultColor(dft) dft
#define DEFAULT_FG        COLOR_WHITE
#define DEFAULT_BG        COLOR_BLACK
#endif

#define	Num2Color(n,dft)  ((n >= 0) ? (short)ctrans[(n) & (ncolors-1)] : DefaultColor(dft))
#define Num2Fore(n)       Num2Color(n, DEFAULT_FG)
#define Num2Back(n)       Num2Color(n, DEFAULT_BG)
#endif /* OPT_COLOR */

#if SYS_OS2_EMX
#include "os2keys.h"
#endif

static ENC_CHOICES my_encoding = enc_DEFAULT;
static int i_am_xterm = 1;
static int i_was_closed = FALSE;
static int have_data = FALSE;
static int in_screen = FALSE;
static int can_color = FALSE;

#include "xtermkeys.h"

#if OPT_COLOR

static int used_bcolor = -999;

/*
 * Compute a color pair number given foreground/background values.
 *
 * We use multiple foreground (text) colors on a given background color,
 * which should leave enough room in the color-pairs array to allow for
 * default colors as well.
 *
 * Color pair 0 in curses is special; it corresponds to default foreground
 * and background colors.
 */
static int
compute_pair(int fg, int bg, int updateit)
{
    short pair;
    short map_fg = DEFAULT_FG;
    short map_bg = DEFAULT_BG;

    if (is_default(fg) && is_default(bg)) {
	pair = 0;
	fg = -1;
	bg = -1;
    } else if (is_default(fg)) {
	pair = 1;
	fg = -1;
	map_bg = Num2Back(bg);
    } else {
	if (is_default(bg))
	    bg = -1;
	else
	    map_bg = Num2Back(bg);
	map_fg = Num2Fore(fg);
	pair = (short) (2 + fg);
    }

    if (updateit) {
	TRACE(("init_pair %2d fg %2d, bg %2d -> %2d, %2d\n",
	       pair, fg, bg, map_fg, map_bg));
	init_pair(pair, map_fg, map_bg);
    }
    return pair;
}

/*
 * Initialize all color pairs when the palette is (re)initialized.
 */
static void
reinitialize_colors(void)
{
    short fg;

    TRACE((T_CALLED "reinitialize_colors\n"));

    for (fg = -1; fg < ncolors; ++fg) {
	(void) compute_pair(fg, gbcolor, TRUE);
    }

    used_bcolor = gbcolor;
    returnVoid();
}
#endif /* OPT_COLOR */

#if USE_WINCH_CODE
static int catching_sigwinch;
static SIG_ATOMIC_T caught_sigwinch;

static SIGT
begin_sigwinch(int ACTUAL_SIG_ARGS GCC_UNUSED)
{
    caught_sigwinch = 1;
    SIGRET;
}

static void
finish_sigwinch(void)
{
    if (caught_sigwinch) {
	int w, h;
	caught_sigwinch = 0;
	getscreensize(&w, &h);
	resizeterm(h, w);
	newscreensize(h, w);
    }
}

#else
#define finish_sigwinch()	/* nothing */
#endif /* USE_WINCH_CODE */

static void
curs_set_encoding(ENC_CHOICES code)
{
    /*
     * Check if we're able to display UTF-8, and if not, decline attempts to
     * use that in the display.
     *
     * ncurses differs slightly from X/Open (following the SVr4 pattern which
     * appears to have been lost in committee) by allowing addch() to
     * accumulate multibyte characters and display those.  At the moment, this
     * driver only uses addch(), rather than add_wch() or addnwstr(), which
     * both would require us to accumulate and convert.  So the ifdef checks
     * for wide-character ncurses rather than just wide-character curses.
     */
#if !USE_MULTIBYTE
    if (code > enc_LOCALE) {
	code = enc_LOCALE;
    }
#endif
    my_encoding = code;
}

static ENC_CHOICES
curs_get_encoding(void)
{
    return my_encoding;
}

static void
curs_initialize(void)
{
    static int already_open = 0;

#if USE_TERMCAP
    char tc_rawdata[4096];
    char *p = tc_parsed;
#endif
    unsigned i;

    TRACE((T_CALLED "curs_initialize()\n"));

    if (already_open) {
	if (i_was_closed) {
	    i_was_closed = FALSE;
#if OPT_XTERM
	    if (i_am_xterm) {
		xterm_open(0);
	    }
#endif /* OPT_XTERM */
#if OPT_TITLE
	    if (auto_set_title) {
		term.set_title(tb_values(current_title));
	    }
#endif
	}
	returnVoid();
    }

    TRACE((T_CALLED "curs_initialize - not opened\n"));
#if OPT_LOCALE
    if (okCTYPE2(vl_wide_enc)) {
	TRACE(("setting locale to %s\n", vl_wide_enc.locale));
	setlocale(LC_CTYPE, vl_wide_enc.locale);
    } else {
	TRACE(("setting locale to %s\n", vl_real_enc.locale));
	setlocale(LC_CTYPE, vl_real_enc.locale);
    }
#endif
    initscr();
    raw();
    noecho();
    nonl();
    nodelay(stdscr, TRUE);
    idlok(stdscr, TRUE);

#if USE_WINCH_CODE
    /*
     * ncurses should set up a handler for SIGWINCH, but some developers chose
     * to differ.  Check for that problem, and work around by starting out
     */
    {
	SIGNAL_HANDLER old_handler = original_sighandler(SIGWINCH);

	if (old_handler == SIG_ERR
	    || old_handler == SIG_DFL
	    || old_handler == SIG_IGN) {
	    catching_sigwinch = TRUE;
	    old_handler = setup_handler(SIGWINCH, begin_sigwinch);
	}
    }
#endif /* USE_WINCH_CODE */

    /*
     * Note: we do not set the locale to vl_real_enc since that would confuse
     * curses when it adds multibyte characters to the display.
     */

#if USE_TERMCAP
    if ((tgetent(tc_rawdata, getenv("TERM"))) != 1) {
	fprintf(stderr, "Unknown terminal type %s!\n", getenv("TERM"));
	ExitProgram(BADEXIT);
    }
    TRACE(("tc_rawdata used %d of %d\n", strlen(tc_rawdata), sizeof(tc_rawdata)));
#endif

#if OPT_XTERM
    {
	char *t = getenv("TERM");
	I_AM_XTERM(t);
	if (i_am_xterm) {
	    xterm_open(&term);
	}
    }
#endif

    term.maxrows = term.rows = LINES;
    term.maxcols = term.cols = COLS;

#if OPT_COLOR
    if (has_colors()) {
	can_color = TRUE;
	start_color();
#ifdef HAVE_USE_DEFAULT_COLORS
	use_default_colors();
#endif
	set_colors(COLORS);
    } else {
	set_colors(2);		/* white/black */
    }
#endif

#if OPT_COLOR
    set_palette(ANSI_palette);
#endif

    /*
     * Read the termcap data now so tcap_init_fkeys() does not depend on the
     * state of tgetstr() vs the buffer.
     */
    for (i = 0; i < TABLESIZE(keyseqs); ++i) {
	keyseqs[i].result = TGETSTR(keyseqs[i].capname, &p);
#if USE_TERMINFO
	if (NO_CAP(keyseqs[i].result))
	    keyseqs[i].result = 0;
#endif
    }
    tcap_init_fkeys();

    ttopen();
    already_open = TRUE;

    returnVoid();
}

static void
curs_open(void)
{
    TRACE(("curs_open\n"));
    vl_save_tty();
#if OPT_LOCALE
    vl_open_mbterm();
#endif
}

static void
curs_close(void)
{
    TRACE(("curs_close\n"));

    /* see tcap.c */
    term.curmove(term.rows - 1, 0);	/* cf: dumbterm.c */
    term.eeol();
    term.set_title(getenv("TERM"));

    vl_restore_tty();
#if OPT_XTERM
    if (i_am_xterm) {
	xterm_close();
    }
#endif
#if OPT_LOCALE
    vl_close_mbterm();
#endif
    i_was_closed = TRUE;
}

static int last_key = -1;

static int
curs_typahead(void)
{
    int result = FALSE;
    if (in_screen) {
	if (last_key == -1) {
	    nodelay(stdscr, TRUE);
	    last_key = getch();
	}
	result = (last_key >= 0);
    }
    return result;
}

static int
curs_getc(void)
{
    int result = last_key;

    if (!in_screen) {
	fflush(stdout);
	result = getchar();
    } else if (result == -1) {
      resized:
	{
#ifdef VAL_AUTOCOLOR
	    int acmilli = global_b_val(VAL_AUTOCOLOR);

	    if (acmilli != 0) {
		timeout(acmilli);
		for_ever {
		    result = getch();
		    if (result < 0) {
			finish_sigwinch();
			autocolor();
		    } else {
			break;
		    }
		}
	    } else
#endif
#if USE_WINCH_CODE
	    if (catching_sigwinch) {
		timeout(100);
		for_ever {
		    result = getch();
		    if (result < 0) {
			finish_sigwinch();
		    } else {
			break;
		    }
		}
	    } else
#endif /* USE_WINCH_CODE */
	    {
		nodelay(stdscr, FALSE);
		result = getch();
	    }
	}
#ifdef KEY_RESIZE
	/*
	 * If ncurses returns KEY_RESIZE, it has updated the LINES/COLS
	 * values and we need only update our copy of those and repaint
	 * the screen.
	 */
	if (result == KEY_RESIZE) {
	    if ((LINES > 1 && LINES != term.rows)
		|| (COLS > 1 && COLS != term.cols))
		newscreensize(LINES, COLS);
	    goto resized;
	}
#endif
    }
    last_key = -1;
    TRACE(("curs_getc:%d%s\n", result, in_screen ? "" : "*"));
    return result;
}

static OUTC_DCL
curs_putc(int c)
{
    c &= (N_chars - 1);
    if (in_screen) {
	have_data = TRUE;
	OUTC_RET addch((UINT) (c));
    } else {
	OUTC_RET putchar(c);
    }
}

static void
curs_flush(void)
{
    if (in_screen && have_data) {
	refresh();
	TRACE(("curs_flush\n"));
	have_data = FALSE;
    }
}

static void
curs_kopen(void)
{
    static int initialized = FALSE;

    TRACE((T_CALLED "curs_kopen\n"));
    term.mopen();
    if (!initialized) {
	initialized = TRUE;
	curs_initialize();
    }
    in_screen = TRUE;
    curs_flush();
    returnVoid();
}

static void
curs_kclose(void)
{
    TRACE((T_CALLED "curs_kclose\n"));
    endwin();
    term.mclose();
    in_screen = FALSE;
    have_data = FALSE;
    returnVoid();
}

static void
curs_move(register int row, register int col)
{
    move(row, col);
}

static void
curs_eeol(void)
{
    clrtoeol();
}

static void
curs_eeop(void)
{
    if (in_screen) {
	if (sgarbf) {
	    erase();
	    wrefresh(curscr);
	} else {
	    clrtobot();
	}
    }
}

/* move howmany lines starting at 'from' to 'to' */
static void
curs_scroll(int from, int to, int howmany)
{
    scrollok(stdscr, TRUE);
    if (from > to) {
	setscrreg(to, from + howmany - 1);
    } else if (from < to) {
	setscrreg(from, to + howmany - 1);
    }
    scrl(from - to);
    scrollok(stdscr, FALSE);
}

#if OPT_COLOR
static void
set_bkgd_colors(int fg, int bg)
{
    if (used_bcolor != gbcolor) {
	TRACE(("...bcolor changed from %d to %d\n", used_bcolor, gbcolor));
	reinitialize_colors();
    }
    bkgdset((chtype) (COLOR_PAIR(compute_pair(fg, bg, TRUE)) | ' '));
}

static void
curs_fcol(int color)
{
    short fg, bg;
    short pair = (short) PAIR_NUMBER(getattrs(stdscr));

    pair_content(pair, &fg, &bg);
    set_bkgd_colors(color, bg);
}

static void
curs_bcol(int color)
{
    short fg, bg;
    short pair = (short) PAIR_NUMBER(getattrs(stdscr));

    pair_content(pair, &fg, &bg);
    set_bkgd_colors(fg, color);
}

static int
curs_spal(const char *thePalette)
{				/* reset the palette registers */
    int rc;
    if ((rc = set_ctrans(thePalette))) {
	TRACE(("palette changed\n"));
	reinitialize_colors();
    }
    return rc;
}
#endif /* OPT_COLOR */

#if OPT_VIDEO_ATTRS
static void
curs_attr(UINT attr)
{
    chtype result = A_NORMAL;

    if (attr & VABOLD)
	result |= A_BOLD;
    if (attr & VAUL)
	result |= A_UNDERLINE;
    if (attr & (VAREV | VASEL))
	result |= A_REVERSE;
#ifdef A_ITALIC
    if (attr & VAITAL)
	result |= A_ITALIC;
#endif
#if OPT_COLOR
    if (can_color) {
	int fg;

	if (attr & VACOLOR) {
	    fg = (VCOLORNUM(attr) % ncolors);
	    result |= COLOR_PAIR(compute_pair(fg, gbcolor, FALSE));
	}
    }
#endif
    attrset((int) result);
}

#else /* highlighting is a minimum attribute */

static void
curs_rev(			/* change reverse video status */
	    UINT state)
{				/* FALSE = normal video, TRUE = reverse video */
    if (state) {
	standout();
    } else {
	standend();
    }
}

#endif /* OPT_VIDEO_ATTRS */

/*
 * Hide/show cursor.  We do this in levels, so we can do the "right" thing with
 * multimotion.
 */
static void
curs_cursor(int flag)
{
    static int level;
    if (flag) {
	if (!++level) {
	    TRACE(("CURSOR ON\n"));
	    curs_set(1);
	}
    } else {
	if (!level--) {
	    TRACE(("CURSOR OFF\n"));
	    curs_set(0);
	}
    }
}

static void
curs_beep(void)
{
#if OPT_FLASH
    if (global_g_val(GMDFLASH))
	flash();
    else
#endif
	beep();
}

TERM term =
{
    0,				/* the first four values are set dynamically */
    0,
    0,
    0,
    curs_set_encoding,
    curs_get_encoding,
    curs_open,
    curs_close,
    curs_kopen,
    curs_kclose,
    ttclean,
    ttunclean,
    nullterm_openup,
    curs_getc,
    curs_putc,
    curs_typahead,
    curs_flush,
    curs_move,
    curs_eeol,
    curs_eeop,
    curs_beep,
#if OPT_VIDEO_ATTRS
    curs_attr,
#else
    curs_rev,
#endif
    nullterm_setdescrip,
#if OPT_COLOR
    curs_fcol,
    curs_bcol,
    curs_spal,
#else
    nullterm_setfore,
    nullterm_setback,
    nullterm_setpal,
#endif
    nullterm_setccol,
    curs_scroll,
    nullterm_pflush,
    nullterm_icursor,
    nullterm_settitle,
    ttwatchfd,
    ttunwatchfd,
    curs_cursor,
    nullterm_mopen,
    nullterm_mclose,
    nullterm_mevent,
};

#if NO_LEAKS
void
curses_leaks(void)
{
#if defined(HAVE_EXIT_CURSES)
    exit_curses(0);
#elif defined(HAVE__NC_FREEALL)
    _nc_freeall();
#endif
}
#endif

#endif /* DISP_CURSES */
