/*
 * A terminal driver using the curses library
 *
 * $Header: /users/source/archives/vile.vcs/RCS/curses.c,v 1.35 2008/03/25 18:58:57 tom Exp $
 */

#include "estruct.h"
#include "edef.h"

#if OPT_LOCALE
#include <locale.h>
#endif

#if DISP_CURSES

#undef WINDOW

#include "tcap.h"

#define is_default(color) (color < 0 || color == 255)

#if USE_TERMCAP
#  define TCAPSLEN 1024
static char tc_parsed[TCAPSLEN];
#endif

#if OPT_COLOR
	/* ANSI: black, red, green, yellow, blue, magenta, cyan, white   */
static const char ANSI_palette[] =
{"0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"};

#endif /* OPT_COLOR */

#if SYS_OS2_EMX
#include "os2keys.h"
#endif

static int i_am_xterm = 1;
static int have_data = FALSE;
static int in_screen = FALSE;
static int can_color = FALSE;

#include "xtermkeys.h"

static void
curs_initialize(void)
{
    static int already_open = 0;

#if USE_TERMCAP
    char tc_rawdata[4096];
    char *p = tc_parsed;
#endif
    unsigned i;
    int j;

    if (already_open)
	return;

    TRACE((T_CALLED "curs_initialize\n"));
#if OPT_LOCALE
    TRACE(("setting locale to %s\n", utf8_locale));
    setlocale(LC_CTYPE, utf8_locale);
#endif
    initscr();
    raw();
    noecho();
    nonl();
    nodelay(stdscr, TRUE);
    idlok(stdscr, TRUE);

    /*
     * Note: we do not set the locale to vl_locale since that would confuse
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
	for (j = 1; j < COLOR_PAIRS; j++) {
	    init_pair(j, j / COLORS, j % COLORS);
	}
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
#if OPT_LOCALE
    vl_close_mbterm();
#endif
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
#ifdef VAL_AUTOCOLOR
	int acmilli = global_b_val(VAL_AUTOCOLOR);

	if (acmilli != 0) {
	    timeout(acmilli);
	    for_ever {
		result = getch();
		if (result < 0) {
		    autocolor();
		} else {
		    break;
		}
	    }
	} else {
	    nodelay(stdscr, FALSE);
	    result = getch();
	}
#else
	nodelay(stdscr, FALSE);
	result = getch();
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
    int pair;

#ifdef HAVE_USE_DEFAULT_COLORS
    /*
     * ncurses supports the use of default colors, which we can specify
     * in the init_pair() function at the expense of specifying all of the
     * allowable explicit color combinations.
     */
    if (is_default(fg) && is_default(bg)) {
	pair = 0;
	fg = -1;
	bg = -1;
    } else {
	if (is_default(fg)) {
	    pair = bg;
	    fg = -1;
	} else if (is_default(bg)) {
	    pair = fg * COLORS;
	    bg = -1;
	} else {
	    pair = fg * COLORS + bg;
	}
    }
    if (init_pair(pair, fg, bg) == ERR)
	return;
#else
    pair = fg * COLORS + bg;
#endif
    bkgdset(COLOR_PAIR(pair) | ' ');
}

static void
curs_fcol(int color)
{
    short fg, bg;
    int pair = PAIR_NUMBER(getattrs(stdscr));

    pair_content(pair, &fg, &bg);
    set_bkgd_colors(color, bg);
}

static void
curs_bcol(int color)
{
    short fg, bg;
    int pair = PAIR_NUMBER(getattrs(stdscr));

    pair_content(pair, &fg, &bg);
    set_bkgd_colors(fg, color);
}

static void
curs_spal(const char *thePalette)
{				/* reset the palette registers */
    set_ctrans(thePalette);
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
#if OPT_COLOR
    if (can_color) {
	int pair = PAIR_NUMBER(getattrs(stdscr));
	short fg, bg;
	if (attr & VACOLOR) {
	    fg = VCOLORNUM(attr);
	    bg = gbcolor;
#ifdef HAVE_USE_DEFAULT_COLORS
	    if (is_default(bg)) {
		pair = fg * COLORS;
		init_pair(pair, fg, -1);
	    } else {
		pair = fg * COLORS + bg;
	    }
#else
	    pair = fg * COLORS + bg;
#endif
	    result |= COLOR_PAIR(pair);
	}
    }
#endif
    attrset(result);
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
    enc_DEFAULT,
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

#endif /* DISP_CURSES */
