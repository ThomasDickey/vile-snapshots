/*	tcap:	Unix V5, V7 and BS4.2 Termcap video driver
 *		for MicroEMACS
 *
 * $Header: /users/source/archives/vile.vcs/RCS/tcap.c,v 1.155 2004/10/21 23:10:28 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"

#if DISP_TERMCAP

#define NPAUSE	10		/* # times thru update to pause */

#include	"tcap.h"

#if OPT_ICONV_FUNCS
#include	<iconv.h>
#include	<locale.h>
#include	<langinfo.h>
#endif

#undef WINDOW
#define WINDOW vile_WINDOW

#if USE_TERMCAP
#  define TCAPSLEN 768
static char tcapbuf[TCAPSLEN];
#endif

static char *tc_CM, *tc_CE, *tc_CL, *tc_ME, *tc_MR, *tc_SO, *tc_SE;
static char *tc_TI, *tc_TE, *tc_KS, *tc_KE;
static char *tc_CS, *tc_dl, *tc_al, *tc_DL, *tc_AL, *tc_SF, *tc_SR;
static char *tc_VI, *tc_VE;

#if OPT_VIDEO_ATTRS
static char *tc_ZH;		/* italic-start */
static char *tc_ZR;		/* italic-end */
static char *tc_US;		/* underline-start */
static char *tc_UE;		/* underline-end */
static char *tc_MD;
static int tc_NC;		/* no_color_video */
#endif

#if OPT_FLASH
static char *vb;		/* visible-bell */
#endif

#if OPT_COLOR
/*
 * This implementation is based on the description of SysVr4 curses found in
 * ncurses 1.8.7, which lists the following terminal capabilities:
 *
 * Full name        Terminfo  Type   Termcap Description
 * ---------------- -------   ----   ----    -----------------------------
 * back_color_erase "bce"     bool   "ut"    screen erased with background color
 * max_colors       "colors"  num    "Co"    maximum numbers of colors on screen
 * max_pairs        "pairs"   num    "pa"    maximum number of color-pairs on the screen
 * no_color_video   "ncv"     num    "NC"    video attributes that can't be used with colors
 * orig_pair        "op"      str    "op"
 * orig_colors      "oc"      str    "oc"    set original colors
 * initialize_color "initc"   str    "Ic"
 * initialize_pair  "initp"   str    "Ip"
 * set_color_pair   "scp"     str    "sp"
 * set_a_foreground "setaf"   str    "AF"
 * set_a_background "setab"   str    "AB"
 * color_names      "colornm" str    "Yw"
 *
 * In this version, we don't support color pairs, since the only terminals on
 * which it's been tested are "ANSI".  The color names are hardcoded.  The
 * termcap must have the following capabilities set (or an equivalent
 * terminfo):
 *	Co (limited to 1 .. (NCOLORS-1)
 *	AF (e.g., "\E[%a+c\036%dm")
 *	AB (e.g., "\E[%a+c\050%dm")
 *	oc (e.g., "\E[0m")
 */

#define NO_COLOR (-1)
#define	Num2Color(n) ((n >= 0) ? ctrans[(n) & (NCOLORS-1)] : NO_COLOR)

static char *AF;
static char *AB;
static char *Sf;
static char *Sb;
static char *OrigColors;
static int have_bce;

	/* ANSI: black, red, green, yellow, blue, magenta, cyan, white   */
static const char ANSI_palette[] =
{"0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"};
static const char UNKN_palette[] =
{"0 4 2 6 1 5 3 7 8 12 10 14 9 13 11 15"};
/*
 * We don't really _know_ what the default colors are set to, so the initial
 * values of the current_[fb]color are set to an illegal value to force the
 * colors to be set.
 */
static int given_fcolor = NO_COLOR;
static int given_bcolor = NO_COLOR;

static int shown_fcolor = NO_COLOR;
static int shown_bcolor = NO_COLOR;
#endif /* OPT_COLOR */
/* *INDENT-OFF* */
static const struct {
    char * capname;
    int    code;
} keyseqs[] = {
    /* Arrow keys */
    { CAPNAME("ku","kcuu1"),	KEY_Up },		/* up */
    { CAPNAME("kd","kcud1"),	KEY_Down },		/* down */
    { CAPNAME("kr","kcuf1"),	KEY_Right },		/* right */
    { CAPNAME("kl","kcub1"),	KEY_Left },		/* left */
    /* other cursor-movement */
    { CAPNAME("kh","khome"),	KEY_Home },		/* home */
    { CAPNAME("kH","kll"),	KEY_End },		/* end (variant) */
    { CAPNAME("@7","kend"),	KEY_End },		/* end */
    /* page scroll */
    { CAPNAME("kN","knp"),	KEY_Next },		/* next page */
    { CAPNAME("kP","kpp"),	KEY_Prior },		/* previous page */
    /* editing */
    { CAPNAME("kI","kich1"),	KEY_Insert },		/* Insert */
    { CAPNAME("kD","kdch1"),	KEY_Delete },		/* Delete */
    { CAPNAME("@0","kfnd"),	KEY_Find },		/* Find */
    { CAPNAME("*6","kslt"),	KEY_Select },		/* Select */
    /* command */
    { CAPNAME("%1","khlp"),	KEY_Help },		/* Help */
    /* function keys */
    { CAPNAME("k1","kf1"),	KEY_F1 },		/* F1 */
    { CAPNAME("k2","kf2"),	KEY_F2 },
    { CAPNAME("k3","kf3"),	KEY_F3 },
    { CAPNAME("k4","kf4"),	KEY_F4 },
    { CAPNAME("k5","kf5"),	KEY_F5 },
    { CAPNAME("k6","kf6"),	KEY_F6 },
    { CAPNAME("k7","kf7"),	KEY_F7 },
    { CAPNAME("k8","kf8"),	KEY_F8 },
    { CAPNAME("k9","kf9"),	KEY_F9 },
    { CAPNAME("k;","kf10"),	KEY_F10 },		/* F10 */
    { CAPNAME("F1","kf11"),	KEY_F11 },		/* F11 */
    { CAPNAME("F2","kf12"),	KEY_F12 },		/* F12 */
    { CAPNAME("F3","kf13"),	KEY_F13 },		/* F13 */
    { CAPNAME("F4","kf14"),	KEY_F14 },
    { CAPNAME("F5","kf15"),	KEY_F15 },
    { CAPNAME("F6","kf16"),	KEY_F16 },
    { CAPNAME("F7","kf17"),	KEY_F17 },
    { CAPNAME("F8","kf18"),	KEY_F18 },
    { CAPNAME("F9","kf19"),	KEY_F19 },		/* F19 */
    { CAPNAME("FA","kf20"),	KEY_F20 },		/* F20 */
    { CAPNAME("FB","kf21"),	KEY_F21 },
    { CAPNAME("FC","kf22"),	KEY_F22 },
    { CAPNAME("FD","kf23"),	KEY_F23 },
    { CAPNAME("FE","kf24"),	KEY_F24 },
    { CAPNAME("FF","kf25"),	KEY_F25 },
    { CAPNAME("FG","kf26"),	KEY_F26 },
    { CAPNAME("FH","kf27"),	KEY_F27 },
    { CAPNAME("FI","kf28"),	KEY_F28 },
    { CAPNAME("FJ","kf29"),	KEY_F29 },
    { CAPNAME("FK","kf30"),	KEY_F30 },
    { CAPNAME("FL","kf31"),	KEY_F31 },
    { CAPNAME("FM","kf32"),	KEY_F32 },
    { CAPNAME("FN","kf33"),	KEY_F33 },
    { CAPNAME("FO","kf34"),	KEY_F34 },
    { CAPNAME("FP","kf35"),	KEY_F35 }
};
/* *INDENT-ON* */

#if SYS_OS2_EMX
#include "os2keys.h"
#endif

static int colors_are_really_ANSI(void);
static void putpad(const char *str);
static void tcapscroll_delins(int from, int to, int n);
static void tcapscroll_reg(int from, int to, int n);
static void tcapscrollregion(int top, int bot);

#if OPT_COLOR
static void tcap_fcol(int color);
static void tcap_bcol(int color);
static void tcap_spal(const char *s);
#else
#define tcap_fcol nullterm_setfore
#define tcap_bcol nullterm_setback
#define tcap_spal nullterm_setpal
#endif

#if OPT_LOCALE
static OUTC_DCL mb_putch(OUTC_ARGS);
static int mb_getch(void);
#endif

#if OPT_VIDEO_ATTRS
static void tcap_attr(UINT attr);
#else
static void tcap_rev(UINT state);
#endif

#define	XtermPos()	((unsigned)(keystroke() - 040))

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

static int i_am_xterm;
static int x_origin = 1, y_origin = 1;

#ifdef HAVE_TPARM		/* usually terminfo */
#define CALL_TPARM(cap,code) tparm(cap, code,0,0,0,0,0,0,0,0)
#else
#define CALL_TPARM(cap,code) tgoto(cap, 0, code)
#endif

static void
tcap_open(void)
{
#if USE_TERMCAP
    char tcbuf[2048];
    char err_str[72];
    char *p;
#endif
    char *t;
    char *tv_stype;
    size_t i;
    int j;
    static int already_open = 0;
    /* *INDENT-OFF* */
    static const struct {
	    char *name;
	    char **data;
    } tc_strings[] = {
     { CAPNAME("AL","il"),    &tc_AL }	/* add p1 lines above cursor */
    ,{ CAPNAME("DL","dl"),    &tc_DL }	/* delete p1 lines, begin at cursor */
    ,{ CAPNAME("al","il1"),   &tc_al }	/* add line below cursor */
    ,{ CAPNAME("ce","el"),    &tc_CE }	/* clear to end of line */
    ,{ CAPNAME("cl","clear"), &tc_CL }	/* clear screen, cursor to home */
    ,{ CAPNAME("cm","cup"),   &tc_CM }	/* move cursor to row p1, col p2 */
    ,{ CAPNAME("cs","csr"),   &tc_CS }	/* set scrolling to rows p1 .. p2 */
    ,{ CAPNAME("dl","dl1"),   &tc_dl }	/* delete line */
    ,{ CAPNAME("ke","rmkx"),  &tc_KE }	/* end keypad-mode */
    ,{ CAPNAME("ks","smkx"),  &tc_KS }	/* start keypad-mode */
    ,{ CAPNAME("se","rmso"),  &tc_SE }	/* end standout-mode */
    ,{ CAPNAME("sf","ind"),   &tc_SF }	/* scroll forward 1 line */
    ,{ CAPNAME("so","smso"),  &tc_SO }	/* start standout-mode */
    ,{ CAPNAME("mr","rev"),   &tc_MR }	/* start reverse-mode */
    ,{ CAPNAME("sr","ri"),    &tc_SR }	/* scroll reverse 1 line */
    ,{ CAPNAME("te","rmcup"), &tc_TE }	/* end cursor-motion program */
    ,{ CAPNAME("ti","smcup"), &tc_TI }	/* initialize cursor-motion program */
#if OPT_COLOR
    ,{ CAPNAME("AF","setaf"), &AF }	/* set ANSI foreground-color */
    ,{ CAPNAME("AB","setab"), &AB }	/* set ANSI background-color */
    ,{ CAPNAME("Sf","setf"),  &Sf }	/* set foreground-color */
    ,{ CAPNAME("Sb","setb"),  &Sb }	/* set background-color */
    ,{ CAPNAME("op","op"), &OrigColors } /* set to original color pair */
    ,{ CAPNAME("oc","oc"), &OrigColors } /* set to original colors */
#endif
#if OPT_FLASH
    ,{ CAPNAME("vb","flash"), &vb }	/* visible bell */
#endif
#if OPT_VIDEO_ATTRS
    ,{ CAPNAME("me","sgr0"),  &tc_ME }	/* turn off all attributes */
    ,{ CAPNAME("md","bold"),  &tc_MD }	/* turn on bold attribute */
    ,{ CAPNAME("us","smul"),  &tc_US }	/* underline-start */
    ,{ CAPNAME("ue","rmul"),  &tc_UE }	/* underline-end */
    ,{ CAPNAME("ZH","sitm"),  &tc_ZH }	/* italic-start */
    ,{ CAPNAME("ZR","ritm"),  &tc_ZR }	/* italic-end */
#endif
    ,{ CAPNAME("ve","cnorm"), &tc_VE }	/* make cursor appear normal */
    ,{ CAPNAME("vi","civis"), &tc_VI }	/* make cursor invisible */
    /* FIXME: do xmc/ug */
    };

    static char * fallback_arrows[] = {
/*	 "\033",	** VT52 (type-ahead checks make this not useful) */
	"\033O",	/* SS3 */
	"\033[",	/* CSI */
	"\217",	/* SS3 */
	"\233",	/* CSI */
    };
    /* *INDENT-ON* */

    TRACE((T_CALLED "tcap_open()\n"));
    if (already_open)
	returnVoid();

    if ((tv_stype = getenv("TERM")) == NULL) {
	puts("Environment variable TERM not defined!");
	ExitProgram(BADEXIT);
    }
#if USE_TERMINFO
    setupterm(tv_stype, fileno(stdout), (int *) 0);
#else
    if ((tgetent(tcbuf, tv_stype)) != 1) {
	(void) lsprintf(err_str, "Unknown terminal type %s!", tv_stype);
	puts(err_str);
	ExitProgram(BADEXIT);
    }
#endif

#if OPT_LOCALE
    /* check if we're using UTF-8 locale */
    if (utf8_locale) {
	term.putch = mb_putch;
	term.getch = mb_getch;
    }
#endif

    /* Get screen size from system, or else from termcap.  */
    getscreensize(&term.cols, &term.rows);

    if ((term.rows <= 1)
	&& (term.rows = TGETNUM(CAPNAME("li", "lines"))) < 0) {
	term.rows = 24;
    }

    if ((term.cols <= 1)
	&& (term.cols = TGETNUM(CAPNAME("co", "cols"))) < 0) {
	term.cols = 80;
    }
#if OPT_COLOR
    if ((j = TGETNUM(CAPNAME("Co", "colors"))) > 0)
	set_colors(j);
    else
	set_colors(2);		/* white/black */
#endif

    /* are we probably an xterm?  */
    i_am_xterm = FALSE;
    if (!strncmp(tv_stype, "xterm", sizeof("xterm") - 1)
	|| !strncmp(tv_stype, "rxvt", sizeof("rxvt") - 1)) {
	i_am_xterm = TRUE;
    } else if ((t = TGETSTR(CAPNAME("Km", "kmous"), &p)) != 0
	       && (t != (char *) (-1))
	       && !strcmp(t, "\033[M")) {
	i_am_xterm = TRUE;
    } else if (TGETFLAG(CAPNAME("XT", "XT")) > 0) {	/* screen */
	i_am_xterm = TRUE;
    }
#if USE_TERMCAP
    else {
	p = tcbuf;
	while (*p && *p != ':') {
	    if (*p == 'x'
		&& strncmp(p, "xterm", sizeof("xterm") - 1) == 0) {
		i_am_xterm = TRUE;
		break;
	    }
	    p++;
	}
    }
#endif
    if (i_am_xterm) {
	x_origin = 0;
	y_origin = 0;
    }

    term.maxrows = term.rows;
    term.maxcols = term.cols;

#if USE_TERMCAP
    p = tcapbuf;
#endif
    for (i = 0; i < TABLESIZE(tc_strings); i++) {
	/* allow aliases */
	if (NO_CAP(*(tc_strings[i].data))) {
	    t = TGETSTR(tc_strings[i].name, &p);
	    TRACE(("TGETSTR(%s) = %s\n", tc_strings[i].name,
		   str_visible(t)));
	    /* simplify subsequent checks */
	    if (NO_CAP(t))
		t = 0;
	    *(tc_strings[i].data) = t;
	}
    }

#if USE_TERMCAP
#  ifdef HAVE_EXTERN_TCAP_PC
    t = TGETSTR("pc", &p);
    if (t)
	PC = *t;
#  endif
#endif

    /* if start/end sequences are not consistent, ignore them */
    if ((tc_SO != 0) ^ (tc_SE != 0))
	tc_SO = tc_SE = 0;
    if ((tc_MR != 0) ^ (tc_ME != 0))
	tc_MR = tc_ME = 0;
#if OPT_VIDEO_ATTRS
    if ((tc_MD != 0) ^ (tc_ME != 0))
	tc_MD = tc_ME = 0;
    if ((tc_US != 0) ^ (tc_UE != 0))
	tc_US = tc_UE = 0;
    if ((tc_ZH == 0) || (tc_ZR == 0)) {
	tc_ZH = tc_US;
	tc_ZR = tc_UE;
    }
#endif

    if ((tc_SO != 0 && tc_SE != 0) || (tc_MR != 0 && tc_ME != 0))
	revexist = TRUE;

    if (tc_CL == NULL || tc_CM == NULL) {
	puts("Incomplete termcap entry\n");
	ExitProgram(BADEXIT);
    }

    if (tc_CE == NULL)		/* will we be able to use clear to EOL? */
	eolexist = FALSE;

    if (!tc_CS || !tc_SR) {	/* some xterm's termcap entry is missing entries */
	if (i_am_xterm) {
	    if (!tc_CS)
		tc_CS = "\033[%i%d;%dr";
	    if (!tc_SR)
		tc_SR = "\033M";
	}
    }

    if (tc_CS && tc_SR) {
	if (tc_SF == NULL)	/* assume '\n' scrolls forward */
	    tc_SF = "\n";
	term.scroll = tcapscroll_reg;
    } else if ((tc_DL && tc_AL) || (tc_dl && tc_al)) {
	term.scroll = tcapscroll_delins;
    } else {
	term.scroll = nullterm_scroll;
    }
#if OPT_COLOR
    /*
     * If we've got one of the canonical strings for resetting to the
     * default colors, we don't have to assume the screen is black/white.
     */
    if (OrigColors != 0) {
	set_global_g_val(GVAL_FCOLOR, NO_COLOR);	/* foreground color */
	set_global_g_val(GVAL_BCOLOR, NO_COLOR);	/* background color */
    }

    /* clear with current bcolor */
    have_bce = TGETFLAG(CAPNAME("ut", "bce")) > 0;

#if OPT_VIDEO_ATTRS
    if (OrigColors == 0)
	OrigColors = tc_ME;
#endif
    if (ncolors >= 8 && AF != 0 && AB != 0) {
	Sf = AF;
	Sb = AB;
	set_palette(ANSI_palette);
    } else if (colors_are_really_ANSI()) {
	set_palette(ANSI_palette);
    } else
	set_palette(UNKN_palette);
#endif
#if OPT_VIDEO_ATTRS
    if ((tc_NC = TGETNUM(CAPNAME("NC", "ncv"))) < 0)
	tc_NC = 0;
    if (tc_US == 0 && tc_UE == 0) {	/* if we don't have underline, do bold */
	tc_US = tc_MD;
	tc_UE = tc_ME;
    }
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
#if OPT_XTERM
    addtosysmap("\033[M", 3, KEY_Mouse);
#if OPT_XTERM >= 3
    addtosysmap("\033[t", 3, KEY_text);
    addtosysmap("\033[T", 3, KEY_textInvalid);
#endif
#endif

#if USE_TERMCAP
    TRACE(("tcapbuf used %d of %d\n", p - tcapbuf, sizeof(tcapbuf)));
    if (p >= &tcapbuf[sizeof(tcapbuf)]) {
	puts("Terminal description too big!\n");
	ExitProgram(BADEXIT);
    }
#endif
    ttopen();
    already_open = TRUE;

    returnVoid();
}

static void
tcap_close(void)
{
    TRACE((T_CALLED "tcap_close()\n"));
#if OPT_VIDEO_ATTRS
    if (tc_SE)
	putpad(tc_SE);
    if (tc_ME)			/* end special attributes (including color) */
	putpad(tc_ME);
#endif
    term.curmove(term.rows - 1, 0);	/* cf: dumbterm.c */
    term.eeol();
#if OPT_COLOR
    shown_fcolor = shown_bcolor =
	given_fcolor = given_bcolor = NO_COLOR;
#endif
    /* all of the ways one could find the original title and restore it
     * are too clumsy.  Setting it to $TERM is a nice way to appease about
     * 90% of the users.
     */
    term.set_title(getenv("TERM"));
    returnVoid();
}

/*
 * We open or close the keyboard when either of the following are true:
 *	a) we're changing the xterm-mouse setting
 *	b) we're spawning a subprocess (e.g., shell or pipe command)
 */
static int keyboard_open = FALSE;

static void
tcap_kopen(void)
{
    TRACE((T_CALLED "tcap_kopen()\n"));
#if OPT_XTERM
    if (i_am_xterm && global_g_val(GMDXTERM_MOUSE))
	putpad(XTERM_ENABLE_TRACKING);
#endif
    if (!keyboard_open) {
	keyboard_open = TRUE;
	if (tc_TI) {
	    putpad(tc_TI);
	    ttrow = ttcol = -1;	/* 'ti' may move the cursor */
	}
	if (tc_KS)
	    putpad(tc_KS);
    }
    (void) strcpy(screen_desc, "NORMAL");
    returnVoid();
}

static void
tcap_kclose(void)
{
    TRACE((T_CALLED "tcap_kclose()\n"));
#if OPT_XTERM
    if (i_am_xterm && global_g_val(GMDXTERM_MOUSE))
	putpad(XTERM_DISABLE_TRACKING);
#endif
    if (keyboard_open) {
	keyboard_open = FALSE;
	if (tc_TE)
	    putpad(tc_TE);
	if (tc_KE)
	    putpad(tc_KE);
    }
    term.flush();
    returnVoid();
}

static void
tcap_move(register int row, register int col)
{
    putpad(tgoto(tc_CM, col, row));
}

static void
begin_reverse(void)
{
    if (tc_MR != 0)
	putpad(tc_MR);
    else if (tc_SO != 0)
	putpad(tc_SO);
}

static void
end_reverse(void)
{
    if (tc_MR != 0)
	putpad(tc_ME);
    else if (tc_SO != 0)
	putpad(tc_SE);
}

#ifdef GVAL_VIDEO
#define REVERSED (global_g_val(GVAL_VIDEO) & VAREV)
static void
set_reverse(void)
{
    if (REVERSED)
	begin_reverse();
}
#else
#define REVERSED 0
#define set_reverse()		/* nothing */
#endif

#if OPT_COLOR
/*
 * Accommodate brain-damaged non-bce terminals by writing a blank to each
 * space that we'll color, return true if we moved the cursor.
 */
static int
clear_non_bce(int row, int col)
{
    int n;
    int last = (row >= term.rows - 1) ? (term.cols - 1) : term.cols;
    if (col < last) {
	for (n = col; n < last; n++)
	    ttputc(' ');
	return TRUE;
    }
    return FALSE;
}

static void
erase_non_bce(int row, int col)
{
    if (clear_non_bce(row, col))
	term.curmove(row, col);
}

#define NEED_BCE_FIX ((!have_bce && shown_bcolor != NO_COLOR) || REVERSED)
#define FILL_BCOLOR(row,col) if(NEED_BCE_FIX) erase_non_bce(row, col)
#else
#define FILL_BCOLOR(row,col)	/*nothing */
#endif

static void
tcap_eeol(void)
{
    set_reverse();
#if OPT_COLOR
    if (NEED_BCE_FIX) {
	erase_non_bce(ttrow, ttcol);
    } else
#endif
	putpad(tc_CE);
}

static void
tcap_eeop(void)
{
    set_reverse();
#if OPT_COLOR
    tcap_fcol(gfcolor);
    tcap_bcol(gbcolor);

    if (NEED_BCE_FIX) {
	int row = ttrow;
	if (row < term.rows - 1) {
	    while (++row < term.rows) {
		if (ttrow != row || ttcol != 0)
		    term.curmove(row, 0);
		(void) clear_non_bce(row, 0);
	    }
	    term.curmove(ttrow, ttcol);
	}
	erase_non_bce(ttrow, ttcol);
    } else
#endif
	putpad(tc_CL);
}

/* move howmany lines starting at from to to */
static void
tcapscroll_reg(int from, int to, int n)
{
    int i;
    if (to == from)
	return;
    if (to < from) {
	tcapscrollregion(to, from + n - 1);
	tcap_move(from + n - 1, 0);
	for (i = from - to; i > 0; i--) {
	    putpad(tc_SF);
	    FILL_BCOLOR(from + n - 1, 0);
	}
    } else {			/* from < to */
	tcapscrollregion(from, to + n - 1);
	tcap_move(from, 0);
	for (i = to - from; i > 0; i--) {
	    putpad(tc_SR);
	    FILL_BCOLOR(from, 0);
	}
    }
    tcapscrollregion(0, term.rows - 1);
}

/*
OPT_PRETTIER_SCROLL is prettier but slower -- it scrolls
		a line at a time instead of all at once.
*/

/* move howmany lines starting at from to to */
static void
tcapscroll_delins(int from, int to, int n)
{
    int i;
    if (to == from)
	return;
    if (tc_DL && tc_AL) {
	if (to < from) {
	    tcap_move(to, 0);
	    putpad(tgoto(tc_DL, 0, from - to));
	    tcap_move(to + n, 0);
	    putpad(tgoto(tc_AL, 0, from - to));
	    FILL_BCOLOR(to + n, 0);
	} else {
	    tcap_move(from + n, 0);
	    putpad(tgoto(tc_DL, 0, to - from));
	    tcap_move(from, 0);
	    putpad(tgoto(tc_AL, 0, to - from));
	    FILL_BCOLOR(from + n, 0);
	}
    } else {			/* must be dl and al */
#if OPT_PRETTIER_SCROLL
	if (absol(from - to) > 1) {
	    tcapscroll_delins(from, (from < to) ? to - 1 : to + 1, n);
	    if (from < to)
		from = to - 1;
	    else
		from = to + 1;
	}
#endif
	if (to < from) {
	    tcap_move(to, 0);
	    for (i = from - to; i > 0; i--)
		putpad(tc_dl);
	    tcap_move(to + n, 0);
	    for (i = from - to; i > 0; i--) {
		putpad(tc_al);
		FILL_BCOLOR(to + n, 0);
	    }
	} else {
	    tcap_move(from + n, 0);
	    for (i = to - from; i > 0; i--)
		putpad(tc_dl);
	    tcap_move(from, 0);
	    for (i = to - from; i > 0; i--) {
		putpad(tc_al);
		FILL_BCOLOR(from, 0);
	    }
	}
    }
}

/* cs is set up just like cm, so we use tgoto... */
static void
tcapscrollregion(int top, int bot)
{
    putpad(tgoto(tc_CS, bot, top));
}

#if OPT_COLOR
/*
 * This ugly hack is designed to work around an incompatibility built into
 * non BSD-derived systems that implemented color based on a SVr4 manpage.
 */
static int
colors_are_really_ANSI(void)
{
    int ok = FALSE;
    int n;
    char cmp[BUFSIZ], *t;

    if (Sf != 0 && Sb != 0 && ncolors == 8) {
	for (n = 0; n < ncolors; n++) {
	    (void) lsprintf(cmp, "\033[4%dm", n);
	    if ((t = CALL_TPARM(Sb, n)) == 0 || strcmp(t, cmp))
		break;
	    (void) lsprintf(cmp, "\033[3%dm", n);
	    if ((t = CALL_TPARM(Sf, n)) == 0 || strcmp(t, cmp))
		break;
	}
	if (n >= ncolors)	/* everything matched */
	    ok = TRUE;
    }
    return ok;
}

static void
show_ansi_colors(void)
{
    char *t;

    if (Sf != 0 || Sb != 0) {
	if (shown_fcolor == NO_COLOR
	    || shown_bcolor == NO_COLOR) {
	    if (OrigColors)
		putpad(OrigColors);
	}

	if ((shown_fcolor != NO_COLOR)
	    && (t = CALL_TPARM(Sf, shown_fcolor)) != 0) {
	    putpad(t);
	}
	if ((shown_bcolor != NO_COLOR)
	    && (t = CALL_TPARM(Sb, shown_bcolor)) != 0) {
	    putpad(t);
	}
    }
}

static void
reinitialize_colors(void)
{
    int saved_fcolor = given_fcolor;
    int saved_bcolor = given_bcolor;

    shown_fcolor = shown_bcolor =
	given_fcolor = given_bcolor = NO_COLOR;

    tcap_fcol(saved_fcolor);
    tcap_bcol(saved_bcolor);
}

static void
tcap_fcol(int color)
{
    if (color != given_fcolor) {
	given_fcolor = color;
	shown_fcolor = (Sf != 0) ? Num2Color(color) : NO_COLOR;
	show_ansi_colors();
    }
}

static void
tcap_bcol(int color)
{
    if (color != given_bcolor) {
	given_bcolor = color;
	shown_bcolor = (Sb != 0) ? Num2Color(color) : NO_COLOR;
	show_ansi_colors();
    }
}

static void
tcap_spal(const char *thePalette)	/* reset the palette registers */
{
    set_ctrans(thePalette);
    reinitialize_colors();
}
#endif /* OPT_COLOR */

#if OPT_VIDEO_ATTRS
/*
 * NOTE:
 * On Linux console, the 'me' termcap setting \E[m resets _all_ attributes,
 * including color.  However, if we use 'se' instead, it doesn't clear the
 * boldface.  To compensate, we reset the colors when we put out any "ending"
 * sequence, such as 'me'.
 *
 * In rxvt (2.12), setting _any_ attribute seems to clobber the color settings.
 */
static void
tcap_attr(UINT attr)
{
	/* *INDENT-OFF* */
#define VA_SGR (VASEL|VAREV|VAUL|VAITAL|VABOLD)
	static	const	struct	{
		char	**start;
		char	**end;
		UINT	NC_bit;
		UINT	mask;
	} tbl[] = {
		{ &tc_MR, &tc_ME,  4, VASEL|VAREV }, /* more reliable than standout */
		{ &tc_SO, &tc_SE,  1, VASEL|VAREV },
		{ &tc_US, &tc_UE,  2, VAUL },
		{ &tc_ZH, &tc_ZR,  2, VAITAL },
		{ &tc_MD, &tc_ME, 32, VABOLD },
	};
	/* *INDENT-ON* */

    static UINT last;
    static int all_sgr0 = -1;
    unsigned n;
    int colored = (ncolors > 2);

    /*
     * If we have one or more video attributes ending with the same
     * pattern, it's likely that they all do (like a vt100).  In that
     * case, set a flag indicating that we'll have to assume that turning
     * any one off turns all off.
     *
     * As a special case, look for the \E[m string, since that is often
     * mixed with other pieces in sgr0, such as ^O or time delays.
     */
    if (all_sgr0 < 0) {
	all_sgr0 = 0;
	for (n = 0; n < TABLESIZE(tbl); n++) {
	    unsigned m = (n + 1) % TABLESIZE(tbl);
	    if (*tbl[n].end != 0
		&& *tbl[m].end != 0
		&& (!strcmp(*tbl[n].end, *tbl[m].end)
		    || strstr(*tbl[n].end, "\033[m") != 0)) {
		all_sgr0 = 1;
		break;
	    }
	}
    }

    attr = VATTRIB(attr);
#ifdef GVAL_VIDEO
    if (REVERSED) {
	if (attr & VASEL)
	    attr ^= VASEL;
	else
	    attr ^= VAREV;
    }
#endif
    if (!colored
	&& (attr & (VASPCOL | VACOLOR)) != 0) {
	attr &= ~(VASPCOL | VACOLOR);
    }
    if (attr & VASPCOL) {
	attr = VCOLORATTR((VCOLORNUM(attr) & (NCOLORS - 1)));
    } else {
	attr &= ~(VAML | VAMLFOC);
    }

#if OPT_COLOR
    /*
     * If we have a choice between color and some other attribute, choose
     * color, since the other attributes may not be real anyway.  But
     * treat VASEL specially, otherwise we won't be able to see selections,
     * as well as VAREV, in case it is used for visual-matches.
     */
    if (tc_NC != 0
	&& (attr & VACOLOR) != 0) {
	for (n = 0; n < TABLESIZE(tbl); n++) {
	    if ((tbl[n].NC_bit & tc_NC) != 0
		&& (tbl[n].mask & attr) != 0) {
		if ((tbl[n].mask & (VASEL | VAREV)) != 0)
		    attr &= ~VACOLOR;
		else
		    attr &= ~tbl[n].mask;
	    }
	}
    }
#endif

    if (attr != last) {
	register char *s;
	UINT diff;
	int ends = !colored;

	diff = last & ~attr;
	/* turn OFF old attributes */
	for (n = 0; n < TABLESIZE(tbl); n++) {
	    if ((tbl[n].mask & diff) != 0
		&& (tbl[n].mask & attr) == 0
		&& (s = *(tbl[n].end)) != 0) {
		putpad(s);
#if OPT_COLOR
		if (!ends)	/* do this once */
		    reinitialize_colors();
#endif
		ends = TRUE;
		diff &= ~(tbl[n].mask);
	    }
	}

	if (all_sgr0)
	    diff = attr;
	else
	    diff = attr & ~last;

	/* turn ON new attributes */
	for (n = 0; n < TABLESIZE(tbl); n++) {
	    if ((tbl[n].mask & diff) != 0
		&& (tbl[n].mask & attr) != 0
		&& (s = *(tbl[n].start)) != 0) {
		putpad(s);
		diff &= ~(tbl[n].mask);
	    }
	}

	if (ends && (attr & (VAREV | VASEL))) {
	    begin_reverse();
	} else if (diff & VA_SGR) {	/* we didn't find it */
	    end_reverse();
	}
#if OPT_COLOR
	if (colored) {
	    if (attr & VACOLOR) {
		tcap_fcol(VCOLORNUM(attr));
	    } else if (given_fcolor != gfcolor) {
		tcap_fcol(gfcolor);
	    }
	}
#endif
	last = attr;
    }
}

#else /* highlighting is a minimum attribute */

/*
 * change reverse video status
 */
static void
tcap_rev(UINT state)		/* FALSE = normal video, TRUE = reverse video */
{
    static UINT revstate = SORTOFTRUE;
    if (state == revstate)
	return;
    revstate = state;
    if (state) {
	begin_reverse();
    } else {
	end_reverse();
    }
}

#endif /* OPT_VIDEO_ATTRS */

/*
 * Hide/show cursor.  We do this in levels, so we can do the "right" thing with
 * multimotion.
 */
static void
tcap_cursor(int flag)
{
    static int level;
    if (tc_VI != 0
	&& tc_VE != 0) {
	if (flag) {
	    if (!++level) {
		TRACE(("CURSOR ON\n"));
		putpad(tc_VE);
	    }
	} else {
	    if (!level--) {
		TRACE(("CURSOR OFF\n"));
		putpad(tc_VI);
	    }
	}
    }
}

static void
tcap_beep(void)
{
#if OPT_FLASH
    int hit = 0;

    if (global_g_val(GMDFLASH)) {
	if (vb) {
	    putpad(vb);
	    hit = 1;
	}
    }
    if (!hit) {
	static char *seq[][2] =
	{
	    {NULL, NULL},	/* vtflash = off */
	    {VTFLASH_NORMSEQ, VTFLASH_REVSEQ},	/* reverse */
	    {VTFLASH_REVSEQ, VTFLASH_NORMSEQ},	/* normal  */
	};
	char *str1, *str2;
	int val;

	val = global_g_val(GVAL_VTFLASH);
	str1 = seq[val][0];
	if (str1) {
	    str2 = seq[val][1];
	    putpad(str1);
	    term.flush();
	    catnap(150, FALSE);
	    putpad(str2);
	    hit = 1;
	}
    }
    if (!hit)
#endif
	ttputc(BEL);
}

static void
putpad(const char *str)
{
    tputs(str, 1, ttputc);
}

#if OPT_LOCALE
#if OPT_ICONV_FUNCS
static iconv_t mb_desc;
static char *mb_table[256];

static char *
get_encoding(char *locale)
{
    char *encoding;

    setlocale(LC_ALL, locale);
    if ((encoding = nl_langinfo(CODESET)) == 0 || *encoding == '\0') {
	fprintf(stderr, "Cannot find encoding for %s\n", locale);
	tidy_exit(BADEXIT);
    }
    return strmalloc(encoding);
}

static void
open_encoding(char *from, char *to)
{
    mb_desc = iconv_open(to, from);
    if (mb_desc == (iconv_t) (-1)) {
	fprintf(stderr, "cannot setup translation from %s to %s\n", from, to);
	tidy_exit(BADEXIT);
    }
}

void
tcap_setup_locale(char *wide, char *narrow)
{
    char *wide_enc;
    char *narrow_enc;
    int n;

    TRACE(("setup_locale(%s, %s)\n", wide, narrow));
    wide_enc = get_encoding(wide);
    narrow_enc = get_encoding(narrow);
    TRACE(("...setup_locale(%s, %s)\n", wide_enc, narrow_enc));

    open_encoding(narrow_enc, wide_enc);

    for (n = 0; n < 256; ++n) {
	size_t converted;
	char input[80], *ip = input;
	char output[80], *op = output;
	size_t in_bytes = 1;
	size_t out_bytes = sizeof(output);
	input[0] = n;
	input[1] = 0;
	converted = iconv(mb_desc, &ip, &in_bytes, &op, &out_bytes);
	if (converted == (size_t) (-1)) {
	    TRACE(("err:%d\n", errno));
	    TRACE(("convert(%d) %d %d/%d\n", n, converted, in_bytes, out_bytes));
	} else {
	    output[sizeof(output) - out_bytes] = 0;
	    mb_table[n] = strmalloc(output);
	}
    }
    iconv_close(mb_desc);
    /*
     * If we were able to convert in one direction, the other should
     * succeed.
     */
    open_encoding(wide_enc, narrow_enc);
}
#endif /* OPT_ICONV_FUNCS */

/*
 * Write 8-bit (e.g., Latin-1) characters to a multibyte (e.g., UTF-8) display.
 */
static OUTC_DCL
mb_putch(OUTC_ARGS)
{
    unsigned ch = (c & 0xff);
#if OPT_ICONV_FUNCS
    char *s = mb_table[ch];
    if (s != 0) {
	while (*s != 0) {
	    ttputc(*s++);
	}
    }
    OUTC_RET 0;
#else
    if (ch < 128) {
	OUTC_RET ttputc(ch);
    } else {
	(void) ttputc(0xC0 | (0x03 & (ch >> 6)));
	OUTC_RET ttputc(0x80 | (0x3F & ch));
    }
#endif
}
#endif /* OPT_LOCALE */

/*
 * Decode multibyte (e.g., UTF-8) to 8-bit (e.g., Latin-1) characters.
 */
static int
mb_getch(void)
{
    int ch, c2;
#if OPT_ICONV_FUNCS
    char input[80], *ip;
    char output[80], *op;
    int used = 0;
    size_t converted, in_bytes, out_bytes;
    for (;;) {
	c2 = ttgetc();
	input[used++] = c2;
	input[used] = 0;
	ip = input;
	in_bytes = used;
	op = output;
	out_bytes = sizeof(output);
	*output = 0;
	converted = iconv(mb_desc, &ip, &in_bytes, &op, &out_bytes);
	TRACE(("converted %d '%s' -> %d:%#x\n",
	       converted, input, out_bytes, *output));
	if (converted == (size_t) (-1)) {
	    if (errno == EILSEQ) {
		ch = -1;
		break;
	    }
	} else {
	    /* assume it is 8-bits */
	    ch = CharOf(output[0]);
	    break;
	}
    }
#else
    ch = ttgetc();
    if (ch >= 0x80) {
	if (ch <= 0xc3) {
	    c2 = ttgetc();
	    ch &= 0x3;
	    ch <<= 6;
	    ch |= c2 & 0x3f;
	} else {
	    do {
		ch = ttgetc();
	    } while (ch < 0 || ch >= 0x80);
	    ch = -1;
	}
    }
#endif
    TRACE(("mb_getch:%#x\n", ch));
    return ch;
}

#if OPT_XTERM
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
	    if (MK.o != 0 && !is_at_end_of_line(MK))
		MK.o += 1;
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

static void
tcap_settitle(const char *string)
{
    if (i_am_xterm && global_g_val(GMDXTERM_TITLE) && string != 0) {
	putpad("\033]0;");
	putpad(string);
	putpad("\007");
	term.flush();
    }
}
#else
#define tcap_settitle nullterm_settitle
#endif /* OPT_XTERM */

TERM term =
{
    0,				/* the first four values are set dynamically */
    0,
    0,
    0,
    NPAUSE,
    tcap_open,
    tcap_close,
    tcap_kopen,
    tcap_kclose,
    ttgetc,
    ttputc,
    tttypahead,
    ttflush,
    tcap_move,
    tcap_eeol,
    tcap_eeop,
    tcap_beep,
#if OPT_VIDEO_ATTRS
    tcap_attr,
#else
    tcap_rev,
#endif
    nullterm_setdescrip,
    tcap_fcol,
    tcap_bcol,
    tcap_spal,
    nullterm_setccol,
    nullterm_scroll,		/* set dynamically at open time */
    nullterm_pflush,
    nullterm_icursor,
    tcap_settitle,
    ttwatchfd,
    ttunwatchfd,
    tcap_cursor,
};

#endif /* DISP_TERMCAP */
