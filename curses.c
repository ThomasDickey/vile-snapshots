/*
 * A terminal driver using the curses library
 *
 * $Header: /users/source/archives/vile.vcs/RCS/curses.c,v 1.20 2002/12/22 19:21:27 tom Exp $
 */

#include	"estruct.h"
#include	"edef.h"

#if DISP_CURSES

#define NPAUSE	10		/* # times thru update to pause */

#undef WINDOW

#include	"tcap.h"

#define is_default(color) (color < 0 || color == 255)

#if OPT_COLOR
	/* ANSI: black, red, green, yellow, blue, magenta, cyan, white   */
static const char ANSI_palette[] =
{"0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"};

#endif /* OPT_COLOR */

static const struct {
    char *capname;
    int code;
} keyseqs[] = {
    /* *INDENT-OFF* */
    /* Arrow keys */
    { CAPNAME("ku", "kcuu1"), KEY_Up },
    { CAPNAME("kd", "kcud1"), KEY_Down },
    { CAPNAME("kr", "kcuf1"), KEY_Right },
    { CAPNAME("kl", "kcub1"), KEY_Left },
    /* other cursor-movement */
    { CAPNAME("kh", "khome"), KEY_Home },
    { CAPNAME("kH", "kll"), KEY_End },
    { CAPNAME("@7", "kend"), KEY_End },
    /* page scroll */
    { CAPNAME("kN", "knp"), KEY_Next },
    { CAPNAME("kP", "kpp"), KEY_Prior },
    /* editing */
    { CAPNAME("kI", "kich1"), KEY_Insert },
    { CAPNAME("kD", "kdch1"), KEY_Delete },
    { CAPNAME("@0", "kfnd"), KEY_Find },
    { CAPNAME("*6", "kslt"), KEY_Select },
    /* command */
    { CAPNAME("%1", "khlp"), KEY_Help },
    /* function keys */
    { CAPNAME("k1", "kf1"), KEY_F1 },
    { CAPNAME("k2", "kf2"), KEY_F2 },
    { CAPNAME("k3", "kf3"), KEY_F3 },
    { CAPNAME("k4", "kf4"), KEY_F4 },
    { CAPNAME("k5", "kf5"), KEY_F5 },
    { CAPNAME("k6", "kf6"), KEY_F6 },
    { CAPNAME("k7", "kf7"), KEY_F7 },
    { CAPNAME("k8", "kf8"), KEY_F8 },
    { CAPNAME("k9", "kf9"), KEY_F9 },
    { CAPNAME("k;", "kf10"), KEY_F10 },
    { CAPNAME("F1", "kf11"), KEY_F11 },
    { CAPNAME("F2", "kf12"), KEY_F12 },
    { CAPNAME("F3", "kf13"), KEY_F13 },
    { CAPNAME("F4", "kf14"), KEY_F14 },
    { CAPNAME("F5", "kf15"), KEY_F15 },
    { CAPNAME("F6", "kf16"), KEY_F16 },
    { CAPNAME("F7", "kf17"), KEY_F17 },
    { CAPNAME("F8", "kf18"), KEY_F18 },
    { CAPNAME("F9", "kf19"), KEY_F19 },
    { CAPNAME("FA", "kf20"), KEY_F20 },
    { CAPNAME("FB", "kf21"), KEY_F21 },
    { CAPNAME("FC", "kf22"), KEY_F22 },
    { CAPNAME("FD", "kf23"), KEY_F23 },
    { CAPNAME("FE", "kf24"), KEY_F24 },
    { CAPNAME("FF", "kf25"), KEY_F25 },
    { CAPNAME("FG", "kf26"), KEY_F26 },
    { CAPNAME("FH", "kf27"), KEY_F27 },
    { CAPNAME("FI", "kf28"), KEY_F28 },
    { CAPNAME("FJ", "kf29"), KEY_F29 },
    { CAPNAME("FK", "kf30"), KEY_F30 },
    { CAPNAME("FL", "kf31"), KEY_F31 },
    { CAPNAME("FM", "kf32"), KEY_F32 },
    { CAPNAME("FN", "kf33"), KEY_F33 },
    { CAPNAME("FO", "kf34"), KEY_F34 },
    { CAPNAME("FP", "kf35"), KEY_F35 }
    /* *INDENT-ON* */

};

#if SYS_OS2_EMX
#include "os2keys.h"
#endif

static int in_screen = FALSE;
static int can_color = FALSE;

static void
initialize(void)
{
    size_t i;
    int j;
    static int already_open = 0;

    static char *fallback_arrows[] =
    {
	"\033O",		/* SS3 */
	"\033[",		/* CSI */
	"\217",			/* SS3 */
	"\233",			/* CSI */
    };

    if (already_open)
	return;

    initscr();
    raw();
    noecho();
    nonl();
    nodelay(stdscr, TRUE);
    idlok(stdscr, TRUE);

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
     * Provide fallback definitions for all ANSI/ISO/DEC cursor keys.
     */
    for (i = 0; i < TABLESIZE(fallback_arrows); i++) {
	for (j = 'A'; j <= 'D'; j++) {
	    char temp[80];
	    lsprintf(temp, "%s%c", fallback_arrows[i], j);
	    addtosysmap(temp, strlen(temp), SPEC | j);
	}
    }

#if SYS_OS2_EMX
    for (i = TABLESIZE(VIO_KeyMap); i--;) {
	addtosysmap(VIO_KeyMap[i].seq, 2, VIO_KeyMap[i].code);
    }
#endif
    for (i = TABLESIZE(keyseqs); i--;) {
	char *seq = TGETSTR(keyseqs[i].capname, &p);
	if (!NO_CAP(seq)) {
	    int len;
	    TRACE(("TGETSTR(%s) = %s\n", keyseqs[i].capname, str_visible(seq)));
#define DONT_MAP_DEL 1
#if DONT_MAP_DEL
	    /* NetBSD, FreeBSD, etc. have the kD (delete) function key
	       defined as the DEL char.  i don't like this hack, but
	       until we (and we may never) have separate system "map"
	       and "map!" maps, we can't allow this -- DEL has different
	       semantics in insert and command mode, whereas KEY_Delete
	       has the same semantics (whatever they may be) in both.
	       KEY_Delete is the only non-motion system map, by the
	       way -- so the rest are benign in insert or command
	       mode.  */
	    if (strcmp(seq, "\177") == 0)
		continue;
#endif
	    addtosysmap(seq, len = strlen(seq), keyseqs[i].code);
	    /*
	     * Termcap represents nulls as octal 200, which is ambiguous
	     * (ugh).  To avoid losing escape sequences that may contain
	     * nulls, check here, and add a mapping for the strings with
	     * explicit nulls.
	     */
#define TCAP_NULL '\200'
	    if (strchr(seq, TCAP_NULL) != 0) {
		char temp[BUFSIZ];
		(void) strcpy(temp, seq);
		for (j = 0; j < len; j++)
		    if (char2int(temp[j]) == TCAP_NULL)
			temp[j] = '\0';
		addtosysmap(temp, len, keyseqs[i].code);
	    }
	}
    }

    ttopen();
    already_open = TRUE;
}

static void
curs_open(void)
{
    TRACE(("curs_open\n"));
    vl_save_tty();
}

static void
curs_close(void)
{
    TRACE(("curs_close\n"));
    vl_restore_tty();
}

static int last_key = -1;

static int
curs_typahead(void)
{
    if (last_key == -1) {
	nodelay(stdscr, TRUE);
	last_key = getch();
    }
    return (last_key >= 0);
}

static int
curs_getc(void)
{
    int result = last_key;

    if (!in_screen) {
	fflush(stdout);
	result = getchar();
    } else if (result == -1) {
	nodelay(stdscr, FALSE);
	result = getch();
    }
    last_key = -1;
    TRACE(("curs_getc:%d%s\n", result, in_screen ? "" : "*"));
    return result;
}

static OUTC_DCL
curs_putc(OUTC_ARGS)
{
    c &= (N_chars - 1);
    if (in_screen) {
	OUTC_RET addch((UINT) (c));
    } else {
	OUTC_RET putchar(c);
    }
}

static void
curs_flush(void)
{
    if (in_screen) {
	refresh();
	TRACE(("curs_flush\n"));
    }
}

static void
curs_kopen(void)
{
    static int initialized = FALSE;
    if (!initialized) {
	initialized = TRUE;
	initialize();
    }
    refresh();
    in_screen = TRUE;
    TRACE(("curs_kopen\n"));
}

static void
curs_kclose(void)
{
    endwin();
    in_screen = FALSE;
    TRACE(("curs_kclose\n"));
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
    if (sgarbf) {
	erase();
	wrefresh(curscr);
    } else {
	clrtobot();
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
    NPAUSE,
    curs_open,
    curs_close,
    curs_kopen,
    curs_kclose,
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
};

#endif /* DISP_CURSES */
