/*	tcap:	Unix V5, V7 and BS4.2 Termcap video driver
 *		for MicroEMACS
 *
 * $Id: tcap.c,v 1.195 2025/01/26 17:07:24 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"

#if DISP_TERMCAP

#include	"tcap.h"

#undef WINDOW
#define WINDOW vile_WINDOW

#if USE_TERMINFO
#define VALID_TERM (cur_term != NULL)
#endif

#if USE_TERMCAP
#define VALID_TERM already_open
#  define TCAPSLEN 1024
static char tc_parsed[TCAPSLEN];
#endif

static int already_open = FALSE;

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
static int tcap_spal(const char *s);
#else
#define tcap_fcol nullterm_setfore
#define tcap_bcol nullterm_setback
#define tcap_spal nullterm_setpal
#endif

#if OPT_VIDEO_ATTRS
static void tcap_attr(UINT attr);
#else
static void tcap_rev(UINT state);
#endif

static int i_am_xterm;
static int i_was_closed;

#include "xtermkeys.h"

#ifdef HAVE_TPARM		/* usually terminfo */
#define CALL_TPARM(cap,code) tparm(cap, (long) code, 0L,0L,0L,0L,0L,0L,0L,0L)
#else
#define CALL_TPARM(cap,code) tgoto(cap, 0, code)
#endif

static ENC_CHOICES my_encoding = enc_DEFAULT;

static void
tcap_set_encoding(ENC_CHOICES code)
{
    my_encoding = code;
}

static ENC_CHOICES
tcap_get_encoding(void)
{
    return my_encoding;
}

static void
tcap_open(void)
{
#if USE_TERMCAP
    char tc_rawdata[4096];
    char *p = tc_parsed;
#endif
    char *t;
    char *tv_stype;
    size_t i;
    int j;
    /* *INDENT-OFF* */
    static const struct {
	    const char *name;
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
    /* *INDENT-ON* */

    TRACE((T_CALLED "tcap_open()\n"));

#if OPT_LOCALE
    vl_open_mbterm();
#endif

    if (already_open) {
	if (i_was_closed) {
	    i_was_closed = FALSE;
#if OPT_XTERM
	    if (i_am_xterm) {
		xterm_open(NULL);
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

    if ((tv_stype = getenv("TERM")) == NULL) {
	puts("Environment variable TERM not defined!");
	ExitProgram(BADEXIT);
    }
#if USE_TERMINFO
    setupterm(tv_stype, fileno(stdout), (int *) 0);
#else
    if ((tgetent(tc_rawdata, tv_stype)) != 1) {
	fprintf(stderr, "Unknown terminal type %s!\n", tv_stype);
	ExitProgram(BADEXIT);
    }
    TRACE(("tc_rawdata used %d of %d\n", strlen(tc_rawdata), sizeof(tc_rawdata)));
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
#if OPT_XTERM
    i_am_xterm = FALSE;
    I_AM_XTERM(tv_stype);
    if (i_am_xterm) {
	xterm_open(&term);
    }
#endif /* OPT_XTERM */

    term.maxrows = term.rows;
    term.maxcols = term.cols;

#if USE_TERMCAP
    p = tc_parsed;
#endif
    for (i = 0; i < TABLESIZE(tc_strings); i++) {
	/* allow aliases */
	if (NO_CAP(*(tc_strings[i].data))) {
	    t = TGETSTR(tc_strings[i].name, &p);
	    TRACE(("TGETSTR(%s) = %s\n", tc_strings[i].name, str_visible(t)));
	    /* simplify subsequent checks */
	    if (NO_CAP(t))
		t = NULL;
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
    if ((tc_SO != NULL) ^ (tc_SE != NULL))
	tc_SO = tc_SE = NULL;
    if ((tc_MR != NULL) ^ (tc_ME != NULL))
	tc_MR = tc_ME = NULL;
#if OPT_VIDEO_ATTRS
    if ((tc_MD != NULL) ^ (tc_ME != NULL))
	tc_MD = tc_ME = NULL;
    if ((tc_US != NULL) ^ (tc_UE != NULL))
	tc_US = tc_UE = NULL;
    if ((tc_ZH == NULL) || (tc_ZR == NULL)) {
	tc_ZH = tc_US;
	tc_ZR = tc_UE;
    }
#endif

    if ((tc_SO != NULL && tc_SE != NULL) || (tc_MR != NULL && tc_ME != NULL))
	revexist = TRUE;

    if (tc_CL == NULL || tc_CM == NULL) {
	puts("Incomplete termcap entry\n");
	ExitProgram(BADEXIT);
    }

    if (tc_CE == NULL)		/* will we be able to use clear to EOL? */
	eolexist = FALSE;

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
    if (OrigColors != NULL) {
	set_global_g_val(GVAL_FCOLOR, NO_COLOR);	/* foreground color */
	set_global_g_val(GVAL_BCOLOR, NO_COLOR);	/* background color */
    }

    /* clear with current bcolor */
    have_bce = TGETFLAG(CAPNAME("ut", "bce")) > 0;

#if OPT_VIDEO_ATTRS
    if (OrigColors == NULL)
	OrigColors = tc_ME;
#endif
    if (ncolors >= 8 && AF != NULL && AB != NULL) {
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
    if (tc_US == NULL && tc_UE == NULL) {	/* if we don't have underline, do bold */
	tc_US = tc_MD;
	tc_UE = tc_ME;
    }
#endif

    /*
     * Read the termcap data now so tcap_init_fkeys() does not depend on the
     * state of tgetstr() vs the buffer.
     */
    for (i = 0; i < TABLESIZE(keyseqs); ++i) {
	keyseqs[i].result = TGETSTR(keyseqs[i].capname, &p);
#if USE_TERMINFO
	if (NO_CAP(keyseqs[i].result))
	    keyseqs[i].result = NULL;
#endif
    }
    tcap_init_fkeys();

#if USE_TERMCAP
    TRACE(("tc_parsed used %d of %d\n", p - tc_parsed, sizeof(tc_parsed)));
    if (p >= &tc_parsed[sizeof(tc_parsed)]) {
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
    if (i_was_closed)
	returnVoid();
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
#if OPT_XTERM
    if (i_am_xterm) {
	xterm_close();
    }
#endif
#if OPT_LOCALE
    vl_close_mbterm();
#endif
#if NO_LEAKS && USE_TERMINFO
    del_curterm(cur_term);
    cur_term = NULL;
#endif
    i_was_closed = TRUE;
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
    term.mopen();
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
    term.mclose();
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
    if (VALID_TERM)
	putpad(tgoto(tc_CM, col, row));
}

static void
begin_reverse(void)
{
    if (tc_MR != NULL)
	putpad(tc_MR);
    else if (tc_SO != NULL)
	putpad(tc_SO);
}

static void
end_reverse(void)
{
    if (tc_MR != NULL) {
	putpad(tc_ME);
    } else if (tc_SO != NULL) {
	putpad(tc_SE);
    }
}

#ifdef GVAL_VIDEO
#define REVERSED ((UINT) global_g_val(GVAL_VIDEO) & VAREV)
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

#define NEED_BCE_FIX ((UINT) (!have_bce && shown_bcolor != NO_COLOR) || REVERSED)
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

    if (!VALID_TERM)
	return;
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

    if (!VALID_TERM)
	return;
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
    if (VALID_TERM)
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

    if (Sf != NULL && Sb != NULL && ncolors == 8) {
	for (n = 0; n < ncolors; n++) {
	    (void) lsprintf(cmp, "\033[4%dm", n);
	    if ((t = CALL_TPARM(Sb, n)) == NULL || strcmp(t, cmp))
		break;
	    (void) lsprintf(cmp, "\033[3%dm", n);
	    if ((t = CALL_TPARM(Sf, n)) == NULL || strcmp(t, cmp))
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

    if (VALID_TERM && Sf != NULL && Sb != NULL) {
	if (shown_fcolor == NO_COLOR
	    || shown_bcolor == NO_COLOR) {
	    if (OrigColors)
		putpad(OrigColors);
	}

	if ((shown_fcolor != NO_COLOR)
	    && (t = CALL_TPARM(Sf, shown_fcolor)) != NULL) {
	    putpad(t);
	}
	if ((shown_bcolor != NO_COLOR)
	    && (t = CALL_TPARM(Sb, shown_bcolor)) != NULL) {
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
	shown_fcolor = (Sf != NULL) ? Num2Color(color) : NO_COLOR;
	show_ansi_colors();
    }
}

static void
tcap_bcol(int color)
{
    if (color != given_bcolor) {
	given_bcolor = color;
	shown_bcolor = (Sb != NULL) ? Num2Color(color) : NO_COLOR;
	show_ansi_colors();
    }
}

static int
tcap_spal(const char *thePalette)	/* reset the palette registers */
{
    int rc = set_ctrans(thePalette);
    reinitialize_colors();
    return rc;
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
	    if (*tbl[n].end != NULL
		&& *tbl[m].end != NULL
		&& (!strcmp(*tbl[n].end, *tbl[m].end)
		    || strstr(*tbl[n].end, "\033[m") != NULL)) {
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
	attr &= (UINT) (~(VASPCOL | VACOLOR));
    }
    if (attr & VASPCOL) {
	attr = VCOLORATTR((VCOLORNUM(attr) & (ncolors - 1)));
    } else {
	if (attr & VACOLOR) {
	    attr &= (UINT) (VCOLORATTR(ncolors - 1) | ~VACOLOR);
	}
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
	    if ((tbl[n].NC_bit & (UINT) tc_NC) != 0
		&& (tbl[n].mask & attr) != 0) {
		if ((tbl[n].mask & (VASEL | VAREV)) != 0) {
		    attr &= (UINT) (~VACOLOR);
		} else {
		    attr &= ~tbl[n].mask;
		}
	    }
	}
    }
#endif

    if (attr != last) {
	register char *s;
	UINT diff;
	int ends = !colored;
#if OPT_COLOR
	int redo_color = FALSE;
#endif

	diff = last & ~attr;
	/* turn OFF old attributes */
	for (n = 0; n < TABLESIZE(tbl); n++) {
	    if ((tbl[n].mask & diff) != 0
		&& (tbl[n].mask & attr) == 0
		&& (s = *(tbl[n].end)) != NULL) {
		putpad(s);
#if OPT_COLOR
		/*
		 * Any of the resets can turn off color, but especially those
		 * for reverse/bold since they use the sgr0 value.
		 */
		if (colored)
		    redo_color = TRUE;
#endif
		ends = TRUE;
		diff &= ~(tbl[n].mask);
	    }
	}
#if OPT_COLOR
	if (redo_color)		/* do this once */
	    reinitialize_colors();
#endif

	if (all_sgr0)
	    diff = attr;
	else
	    diff = attr & ~last;

	/* turn ON new attributes */
	for (n = 0; n < TABLESIZE(tbl); n++) {
	    if ((tbl[n].mask & diff) != 0
		&& (tbl[n].mask & attr) != 0
		&& (s = *(tbl[n].start)) != NULL) {
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
    if (VALID_TERM
	&& tc_VI != NULL
	&& tc_VE != NULL) {
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
	static const char *seq[][2] =
	{
	    {NULL, NULL},	/* vtflash = off */
	    {VTFLASH_NORMSEQ, VTFLASH_REVSEQ},	/* reverse */
	    {VTFLASH_REVSEQ, VTFLASH_NORMSEQ},	/* normal  */
	};
	const char *str1, *str2;
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
    if (VALID_TERM)
	tputs(str, 1, ttputc);
}

TERM term =
{
    0,				/* the first four values are set dynamically */
    0,
    0,
    0,
    tcap_set_encoding,
    tcap_get_encoding,
    tcap_open,
    tcap_close,
    tcap_kopen,
    tcap_kclose,
    ttclean,
    ttunclean,
    nullterm_openup,
    ttgetc,
    vl_ttputc,
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
    nullterm_settitle,		/* filled in by xterm_open() */
    ttwatchfd,
    ttunwatchfd,
    tcap_cursor,
    nullterm_mopen,		/* filled in by xterm_open() */
    nullterm_mclose,		/* filled in by xterm_open() */
    nullterm_mevent,		/* filled in by xterm_open() */
};

#if NO_LEAKS
void
tcap_leaks(void)
{
#if defined(HAVE_EXIT_TERMINFO)
    exit_terminfo(0);
#elif defined(HAVE__NC_FREEALL)
    _nc_freeall();
#elif defined(HAVE__NC_FREE_TINFO)
    _nc_free_tinfo();
#endif
}
#endif

#endif /* DISP_TERMCAP */
