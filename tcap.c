/*	tcap:	Unix V5, V7 and BS4.2 Termcap video driver
 *		for MicroEMACS
 *
 * $Header: /users/source/archives/vile.vcs/RCS/tcap.c,v 1.88 1997/03/30 21:03:38 tom Exp $
 *
 */

#define termdef 1			/* don't define "term" external */

#define WINDOW vile_WINDOW

#include	"estruct.h"
#include	"edef.h"

#if DISP_TERMCAP

#define MARGIN	8
#define SCRSIZ	64
#define NPAUSE	10			/* # times thru update to pause */

#if USE_TERMINFO
#  define TGETSTR(name, bufp) tigetstr(name)
#  define TGETNUM(name)       tigetnum(name) /* may be tigetint() */
#  define TGETFLAG(name)      tigetflag(name)
#  define CAPNAME(a,b)        b
#  define NO_CAP(s)           (s == 0 || s == (char *)-1)
#  undef TRUE
#  undef FALSE
#  undef WINDOW
#  include <curses.h>
#  undef WINDOW
#  define WINDOW vile_WINDOW
#  ifndef TRUE
#  define TRUE 1
#  endif
#  ifndef FALSE
#  define FALSE 0
#  endif
#  if HAVE_TERM_H
#    include <term.h>
#  endif
#  if !HAVE_TIGETNUM && HAVE_TIGETINT
#    define tigetnum tigetint
#  endif
#else /* USE_TERMCAP */
#  undef USE_TERMCAP
#  define USE_TERMCAP 1
#  define TGETSTR(name, bufp) tgetstr(name, bufp)
#  define TGETNUM(name)       tgetnum(name)
#  define TGETFLAG(name)      tgetflag(name)
#  define CAPNAME(a,b)        a
#  define NO_CAP(s)           (s == 0)
#  if HAVE_TERMCAP_H
#    include <termcap.h>
#  endif
#endif /* USE_TERMINFO */

#if USE_TERMCAP
#  define TCAPSLEN 768 
	static	char tcapbuf[TCAPSLEN];
#  if HAVE_EXTERN_TCAP_PC
	extern char PC;		/* used in 'tputs()' */
#  endif
#  if !HAVE_TERMCAP_H
	extern int tgetent (char *buffer, char *termtype);
	extern int tgetnum (char *name);
	extern int tgetflag (char *name);
	extern int tputs (char *string, int nlines, OUTC_DCL (*_f)(OUTC_ARGS) );
#    if HAVE_TPARAM
	extern char *tparam (char *cstring, char *buf, int size, ...);
#    endif
#  endif
#endif

static char *CM, *CE, *CL, *SO, *SE;
static char *TI, *TE, *KS, *KE;
static char *CS, *dl, *al, *DL, *AL, *SF, *SR;

#if OPT_VIDEO_ATTRS
static char *US;	/* underline-start */
static char *UE;	/* underline-end */
static char *ME;
static char *MD;
#endif

#if OPT_FLASH
static char *vb;	/* visible-bell */
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
 * FIXME: In this version, we don't support color pairs, since the only
 * platform on which it's been tested is Linux, with an IBM-PC compatible
 * display.  Also, the color names are hardcoded.  The termcap must have
 * the following capabilities set:
 *	Co (hardcoded to NCOLORS)
 *	AF (e.g., "\E[%a+c\036%dm")
 *	AB (e.g., "\E[%a+c\050%dm")
 *	oc (e.g., "\E[0m")
 *
 * Using termcap alone, we cannot get "yellow" on IBM-PC, since that's a
 * combination of bold+(fcolor=3).  We cannot make that automatically with a
 * termcap expression (without making a special case).  It's possible to do
 * this with terminfo, however (FIXME).
 */

#define NO_COLOR (-1)
#define	Num2Color(n) ((n >= 0) ? ctrans[(n) & (NCOLORS-1)] : NO_COLOR)

static	char	*Sf;
static	char	*Sb;
static	char	*OrigColors;
static	int	have_bce;

static	int	ctrans[NCOLORS];
	/* ansi to ibm color translation table */
static	const char initpalettestr[] = { "0 1 2 3 4 5 6 7" };
	/* black, red, green, yellow, blue, magenta, cyan, white   */
/*
 * We don't really _know_ what the default colors are set to, so the initial
 * values of the current_[fb]color are set to an illegal value to force the
 * colors to be set.
 */
static	int	given_fcolor = NO_COLOR;
static	int	given_bcolor = NO_COLOR;

static	int	shown_fcolor = NO_COLOR;
static	int	shown_bcolor = NO_COLOR;
#endif /* OPT_COLOR */

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

static int  tcapcres ( char *cres );
static void putnpad(char *str, int n);
static void putpad(char *str);
static void tcapbeep (void);
static void tcapclose (void);
static void tcapeeol (void);
static void tcapeeop (void);
static void tcapkclose (void);
static void tcapkopen (void);
static void tcapmove(int row, int col);
static void tcapopen (void);
static void tcapscroll_delins(int from, int to, int n);
static void tcapscroll_reg(int from, int to, int n);
static void tcapscrollregion(int top, int bot);

#if OPT_COLOR
static void tcapfcol ( int color );
static void tcapbcol ( int  color);
static void tcapspal ( char *s );
#endif

#if OPT_VIDEO_ATTRS
static void tcapattr ( int attr );
#else
static void tcaprev  ( int state );
#endif

TERM term = {
	0,	/* these four values are set dynamically at open time */
	0,
	0,
	0,
	MARGIN,
	SCRSIZ,
	NPAUSE,
	tcapopen,
	tcapclose,
	tcapkopen,
	tcapkclose,
	ttgetc,
	ttputc,
	tttypahead,
	ttflush,
	tcapmove,
	tcapeeol,
	tcapeeop,
	tcapbeep,
#if OPT_VIDEO_ATTRS
	tcapattr,
#else
	tcaprev,
#endif
	tcapcres,
#if	OPT_COLOR
	tcapfcol,
	tcapbcol,
	tcapspal,
#else
	null_t_setfor,
	null_t_setback,
	null_t_setpal,
#endif
	null_t_scroll,		/* set dynamically at open time */
	null_t_pflush,
	null_t_icursor,
	null_t_title,
};

#define	XtermPos()	keystroke() - 040

#if OPT_XTERM >= 3
# define XTERM_ENABLE_TRACKING   "\033[?1001h"	/* mouse hilite tracking */
# define XTERM_DISABLE_TRACKING  "\033[?1001l"
#else
# if OPT_XTERM >= 2
#  define XTERM_ENABLE_TRACKING   "\033[?1000h"	/* normal tracking mode */
#  define XTERM_DISABLE_TRACKING  "\033[?1000l"
# else
#  define XTERM_ENABLE_TRACKING   "\033[?9h"	/* X10 compatibility mode */
#  define XTERM_DISABLE_TRACKING  "\033[?9l"
# endif
#endif

static	int	i_am_xterm;
static	int	x_origin = 1,
		y_origin = 1;

static void
tcapopen(void)
{
#if USE_TERMCAP
	char tcbuf[2048];
	char err_str[72];
	char *t, *p;
#endif
	char *tv_stype;
	SIZE_T i;
	int j;
	static int already_open = 0;

	static const struct {
		char *name;
		char **data;
	} tc_strings[] = {
	 { CAPNAME("AL","il"),    &AL }		/* add p1 lines above cursor */
	,{ CAPNAME("DL","dl"),    &DL }		/* delete p1 lines, begin at cursor */
	,{ CAPNAME("al","il1"),   &al }		/* add line below cursor */
	,{ CAPNAME("ce","el"),    &CE }		/* clear to end of line */
	,{ CAPNAME("cl","clear"), &CL }		/* clear screen, cursor to home */
	,{ CAPNAME("cm","cup"),   &CM }		/* move cursor to row p1, col p2 */
	,{ CAPNAME("cs","csr"),   &CS }		/* set scrolling to rows p1 .. p2 */
	,{ CAPNAME("dl","dl1"),   &dl }		/* delete line */
	,{ CAPNAME("ke","rmkx"),  &KE }		/* end keypad-mode */
	,{ CAPNAME("ks","smkx"),  &KS }		/* start keypad-mode */
	,{ CAPNAME("se","rmso"),  &SE }		/* end standout-mode */
	,{ CAPNAME("sf","ind"),   &SF }		/* scroll forward 1 line */
	,{ CAPNAME("so","smso"),  &SO }		/* start standout-mode */
	,{ CAPNAME("sr","ri"),    &SR }		/* scroll reverse 1 line */
	,{ CAPNAME("te","rmcup"), &TE }		/* end cursor-motion program */
	,{ CAPNAME("ti","smcup"), &TI }		/* initialize cursor-motion program */
#if	OPT_COLOR
	,{ CAPNAME("AF","setaf"), &Sf }		/* set ANSI foreground-color */
	,{ CAPNAME("AB","setab"), &Sb }		/* set ANSI background-color */
	,{ CAPNAME("Sf","setf"),  &Sf }		/* set foreground-color */
	,{ CAPNAME("Sb","setb"),  &Sb }		/* set background-color */
	,{ CAPNAME("op","op"), &OrigColors }	/* set to original color pair */
	,{ CAPNAME("oc","oc"), &OrigColors }	/* set to original colors */
#endif
#if	OPT_FLASH
	,{ CAPNAME("vb","flash"), &vb }		/* visible bell */
#endif
#if	OPT_VIDEO_ATTRS
	,{ CAPNAME("me","sgr0"),  &ME }		/* turn off all attributes */
	,{ CAPNAME("md","bold"),  &MD }		/* turn on bold attribute */
	,{ CAPNAME("us","smul"),  &US }		/* underline-start */
	,{ CAPNAME("ue","rmul"),  &UE }		/* underline-end */
#endif
	};

	if (already_open) 
		return;

	if ((tv_stype = getenv("TERM")) == NULL)
	{
		puts("Environment variable TERM not defined!");
		ExitProgram(BADEXIT);
	}

#if USE_TERMINFO
	setupterm(tv_stype, fileno(stdout), (int *)0);
#else
	if ((tgetent(tcbuf, tv_stype)) != 1)
	{
		(void)lsprintf(err_str, "Unknown terminal type %s!", tv_stype);
		puts(err_str);
		ExitProgram(BADEXIT);
	}
#endif

	/* Get screen size from system, or else from termcap.  */
	getscreensize(&term.t_ncol, &term.t_nrow);
 
	if ((term.t_nrow <= 1)
	 && (term.t_nrow = TGETNUM(CAPNAME("li","lines"))) < 0) {
		term.t_nrow = 24;
	}

	if ((term.t_ncol <= 1)
	 && (term.t_ncol = TGETNUM(CAPNAME("co","cols"))) < 0){
		term.t_ncol = 80;
	}

	/* are we probably an xterm?  */
	i_am_xterm = FALSE;
	if (!strncmp(tv_stype, "xterm", sizeof("xterm") - 1)
	 || !strcmp(tv_stype, "rxvt")) {
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

	term.t_mrow =  term.t_nrow;
	term.t_mcol =  term.t_ncol;

#if USE_TERMCAP
	p = tcapbuf;
#endif
	for (i = 0; i < TABLESIZE(tc_strings); i++) {
		/* allow aliases */
		if (NO_CAP(*(tc_strings[i].data)))
		    *(tc_strings[i].data) = TGETSTR(tc_strings[i].name, &p);
		/* simplify subsequence checks */
		if (NO_CAP(*(tc_strings[i].data)))
		    *(tc_strings[i].data) = 0;
	}

#if USE_TERMCAP
#  if HAVE_EXTERN_TCAP_PC
	t = TGETSTR("pc", &p);
	if(t)
		PC = *t;
#  endif
#endif

	if (SO != NULL)
		revexist = TRUE;

	if(CL == NULL || CM == NULL)
	{
		puts("Incomplete termcap entry\n");
		ExitProgram(BADEXIT);
	}

	if (CE == NULL) 	/* will we be able to use clear to EOL? */
		eolexist = FALSE;

	if (!CS || !SR) { /* some xterm's termcap entry is missing entries */
		if (i_am_xterm) {
			if (!CS) CS = "\033[%i%d;%dr";
			if (!SR) SR = "\033M";
		}
	}

	if (CS && SR) {
		if (SF == NULL) /* assume '\n' scrolls forward */
			SF = "\n";
		term.t_scroll = tcapscroll_reg;
	} else if ((DL && AL) || (dl && al)) {
		term.t_scroll = tcapscroll_delins;
	} else {
		term.t_scroll = null_t_scroll;
	}
#if	OPT_COLOR
	/*
	 * If we've got one of the canonical strings for resetting to the
	 * default colors, we don't have to assume the screen is black/white.
	 */
	if (OrigColors != 0) {
		set_global_g_val(GVAL_FCOLOR, NO_COLOR); /* foreground color */
		set_global_g_val(GVAL_BCOLOR, NO_COLOR); /* background color */
	}

	/* clear with current bcolor */
	have_bce = TGETFLAG(CAPNAME("ut","bce")) > 0;

	if (OrigColors == 0)
		OrigColors = ME;
	set_palette(initpalettestr);
#endif
#if OPT_VIDEO_ATTRS
	if (US == 0 && UE == 0) {	/* if we don't have underline, do bold */
		US = MD;
		UE = ME;
	}
#endif

	for (i = TABLESIZE(keyseqs); i--; ) {
	    char *seq = TGETSTR(keyseqs[i].capname, &p);
	    if (!NO_CAP(seq)) {
		int len;
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
		if (strcmp(seq,"\177") == 0)
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
		    (void)strcpy(temp, seq);
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
	if (p >= &tcapbuf[TCAPSLEN])
	{
		puts("Terminal description too big!\n");
		ExitProgram(BADEXIT);
	}
#endif
	ttopen();
	already_open = TRUE;
}

static void
tcapclose(void)
{
#if OPT_VIDEO_ATTRS
	if (ME)	/* end special attributes (including color) */
		putpad(ME);
#endif
	TTmove(term.t_nrow-1, 0);	/* cf: dumbterm.c */
	TTeeol();
#if OPT_COLOR
	shown_fcolor = shown_bcolor =
	given_fcolor = given_bcolor = NO_COLOR;
#endif
}

/*
 * We open or close the keyboard when either of the following are true:
 *	a) we're changing the xterm-mouse setting
 *	b) we're spawning a subprocess (e.g., shell or pipe command)
 */
static	int	keyboard_open = FALSE;

static void
tcapkopen(void)
{
#if OPT_XTERM
	if (i_am_xterm && global_g_val(GMDXTERM_MOUSE))
		putpad(XTERM_ENABLE_TRACKING);
#endif
	if (!keyboard_open) {
		keyboard_open = TRUE;
		if (TI)
			putnpad(TI, (int)strlen(TI));
		if (KS)
			putpad(KS);
	}
	(void)strcpy(sres, "NORMAL");
}

static void
tcapkclose(void)
{
#if OPT_XTERM
	if (i_am_xterm && global_g_val(GMDXTERM_MOUSE))
		putpad(XTERM_DISABLE_TRACKING);
#endif
	if (keyboard_open) {
		keyboard_open = FALSE;
		if (TE)
			putnpad(TE, (int)strlen(TE));
		if (KE)
			putpad(KE);
	}
	TTflush();
}

static void
tcapmove(register int row, register int col)
{
	putpad(tgoto(CM, col, row));
}

#if	OPT_COLOR
/*
 * Accommodate brain-damaged non-bce terminals by writing a blank to each
 * space that we'll color, return true if we moved the cursor.
 */
static int
clear_non_bce(int row, int col)
{
	int n;
	int last = (row >= term.t_nrow-1) ? (term.t_ncol - 1) : term.t_ncol;
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
		TTmove(row, col);
}

#define NEED_BCE_FIX (!have_bce && shown_bcolor != NO_COLOR)
#define FILL_BCOLOR(row,col) if(NEED_BCE_FIX) erase_non_bce(row, col)
#else
#define FILL_BCOLOR(row,col) /*nothing*/
#endif

static void
tcapeeol(void)
{
#if	OPT_COLOR
	if (NEED_BCE_FIX) {
		erase_non_bce(ttrow, ttcol);
	} else
#endif
	putpad(CE);
}

static void
tcapeeop(void)
{
#if	OPT_COLOR
	tcapfcol(gfcolor);
	tcapbcol(gbcolor);

	if (NEED_BCE_FIX) {
		int row = ttrow;
		if (row < term.t_nrow-1) {
			while (++row < term.t_nrow) {
				if (ttrow != row || ttcol != 0)
					TTmove(row, 0);
				(void) clear_non_bce(row, 0);
			}
			TTmove(ttrow, ttcol);
		}
		erase_non_bce(ttrow, ttcol);
	} else
#endif
	putpad(CL);
}

/*ARGSUSED*/
static int
tcapcres(char *res GCC_UNUSED)	/* change screen resolution */
{
	return(TRUE);
}


/* move howmany lines starting at from to to */
static void
tcapscroll_reg(int from, int to, int n)
{
	int i;
	if (to == from) return;
	if (to < from) {
		tcapscrollregion(to, from + n - 1);
		tcapmove(from + n - 1,0);
		for (i = from - to; i > 0; i--) {
			putpad(SF);
			FILL_BCOLOR(from + n - 1, 0);
		}
	} else { /* from < to */
		tcapscrollregion(from, to + n - 1);
		tcapmove(from,0);
		for (i = to - from; i > 0; i--) {
			putpad(SR);
			FILL_BCOLOR(from, 0);
		}
	}
	tcapscrollregion(0, term.t_nrow-1);
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
	if (to == from) return;
	if (DL && AL) {
		if (to < from) {
			tcapmove(to,0);
			putpad(tgoto(DL,0,from-to));
			tcapmove(to+n,0);
			putpad(tgoto(AL,0,from-to));
			FILL_BCOLOR(to+n, 0);
		} else {
			tcapmove(from+n,0);
			putpad(tgoto(DL,0,to-from));
			tcapmove(from,0);
			putpad(tgoto(AL,0,to-from));
			FILL_BCOLOR(from+n, 0);
		}
	} else { /* must be dl and al */
#if OPT_PRETTIER_SCROLL
		if (absol(from-to) > 1) {
			tcapscroll_delins(from, (from<to) ? to-1:to+1, n);
			if (from < to)
				from = to-1;
			else
				from = to+1;    
		}
#endif
		if (to < from) {
			tcapmove(to,0);
			for (i = from - to; i > 0; i--)
				putpad(dl);
			tcapmove(to+n,0);
			for (i = from - to; i > 0; i--) {
				putpad(al);
				FILL_BCOLOR(to + n, 0);
			}
		} else {
			tcapmove(from+n,0);
			for (i = to - from; i > 0; i--)
				putpad(dl);
			tcapmove(from,0);
			for (i = to - from; i > 0; i--) {
				putpad(al);
				FILL_BCOLOR(from, 0);
			}
		}
	}
}

/* cs is set up just like cm, so we use tgoto... */
static void
tcapscrollregion(int top,int bot)
{
	putpad(tgoto(CS, bot, top));
}

#if	OPT_COLOR
static void
show_ansi_colors (void)
{
#if HAVE_TPARM || HAVE_TPARAM
	char	*t;
#endif

	if (shown_fcolor == NO_COLOR
	 || shown_bcolor == NO_COLOR) {
		if (OrigColors)
			putpad(OrigColors);
	}

#if HAVE_TPARM
	if ((shown_fcolor != NO_COLOR)
	 && (t = tparm(Sf, shown_fcolor)) != 0)
		putpad(t);
	if ((shown_bcolor != NO_COLOR)
	 && (t = tparm(Sb, shown_bcolor)) != 0)
		putpad(t);
#else
#if HAVE_TPARAM
	if ((shown_fcolor != NO_COLOR)
	 && (t = tparam(Sf, (char *)0, 0, shown_fcolor)) != 0) {
		putpad(t);
		free(t);
	}
	if ((shown_bcolor != NO_COLOR)
	 && (t = tparam(Sb, (char *)0, 0, shown_bcolor)) != 0) {
		putpad(t);
		free(t);
	}
#endif
#endif

}

static void
reinitialize_colors (void)
{
	int	saved_fcolor = given_fcolor;
	int	saved_bcolor = given_bcolor;

	shown_fcolor = shown_bcolor =
	given_fcolor = given_bcolor = NO_COLOR;

	tcapfcol(saved_fcolor);
	tcapbcol(saved_bcolor);
}

static void
tcapfcol(int color)
{
	if (color != given_fcolor) {
		given_fcolor = color;
		shown_fcolor = (Sf != 0) ? Num2Color(color) : NO_COLOR;
		show_ansi_colors();
	}
}

static void
tcapbcol(int color)
{
	if (color != given_bcolor) {
		given_bcolor = color;
		shown_bcolor = (Sb != 0) ? Num2Color(color) : NO_COLOR;
		show_ansi_colors();
	}
}

static void
tcapspal(char *thePalette)	/* reset the palette registers */
{
    	/* this is pretty simplistic.  big deal. */
	(void)sscanf(thePalette,"%i %i %i %i %i %i %i %i",
	    	&ctrans[0], &ctrans[1], &ctrans[2], &ctrans[3],
	    	&ctrans[4], &ctrans[5], &ctrans[6], &ctrans[7] );
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
tcapattr(int attr)
{
	static	const	struct	{
		char	**start;
		char	**end;
		int	mask;
	} tbl[] = {
		{ &SO, &SE, VASEL|VAREV },
		{ &US, &UE, VAUL },
		{ &US, &UE, VAITAL },
		{ &MD, &ME, VABOLD },
	};
	static	int last;

	attr = VATTRIB(attr);
	attr &= ~(VAML|VAMLFOC);

	if (attr != last) {
		register SIZE_T n;
		register char *s;
		int	diff = attr ^ last;
		int	ends = FALSE;

		/* turn OFF old attributes */
		for (n = 0; n < TABLESIZE(tbl); n++) {
			if ((tbl[n].mask & diff) != 0
			 && (tbl[n].mask & attr) == 0
			 && (s = *(tbl[n].end))  != 0) {
				putpad(s);
#if OPT_COLOR
				reinitialize_colors();
#endif
				ends = TRUE;
				diff &= ~(tbl[n].mask);
			}
		}

		/* turn ON new attributes */
		for (n = 0; n < TABLESIZE(tbl); n++) {
			if ((tbl[n].mask & diff)  != 0
			 && (tbl[n].mask & attr)  != 0
			 && (s = *(tbl[n].start)) != 0) {
				putpad(s);
				diff &= ~(tbl[n].mask);
			}
		}

		if (SO != 0 && SE != 0) {
			if (ends && (attr & (VAREV|VASEL))) {
				putpad(SO);
			} else if (diff) {	/* we didn't find it */
				putpad(SE);
			}
		}
#if OPT_COLOR
		if (attr & VACOLOR)
			tcapfcol(VCOLORNUM(attr));
		else if (given_fcolor != gfcolor)
			tcapfcol(gfcolor);
#endif
		last = attr;
	}
}

#else	/* highlighting is a minimum attribute */

static void
tcaprev(		/* change reverse video status */
int state)		/* FALSE = normal video, TRUE = reverse video */
{
	static int revstate = -1;
	if (state == revstate)
		return;
	revstate = state;
	if (state) {
		if (SO != NULL)
			putpad(SO);
	} else {
		if (SE != NULL)
			putpad(SE);
	}
}

#endif	/* OPT_VIDEO_ATTRS */

static void
tcapbeep(void)
{
#if OPT_FLASH
	if (global_g_val(GMDFLASH)
	 && vb != NULL) {
		putpad(vb);
	} else
#endif
	ttputc(BEL);
}

static void
putpad(char *str)
{
	tputs(str, 1, ttputc);
}

static void
putnpad(char *str, int n)
{
	tputs(str, n, ttputc);
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
 * 	While highlighting, the xterm accepts other characters, but the display
 *	does not appear to be refreshed until highlighting is ended. So (even
 *	if we always capture the beginning of highlighting) we cannot simply
 *	loop here waiting for the end of highlighting.
 *
 *	1993/aug/6 dickey@software.org
 */

static	int	xterm_button (int c);

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
#endif	/* OPT_XTERM >= 3 */

static int
xterm_button(int c)
{
	WINDOW	*wp;
	int	event;
	int	button;
	int	x;
	int	y;
	int	status;
#if OPT_XTERM >= 3
	int	save_row = ttrow;
	int	save_col = ttcol;
	int	firstrow, lastrow;
	int	startx, endx, mousex;
	int	starty, endy, mousey;
	MARK	save_dot;
	char	temp[NSTRING];
	static	const	char	*fmt = "\033[%d;%d;%d;%d;%dT";
#endif	/* OPT_XTERM >= 3 */

	if ((status = (global_g_val(GMDXTERM_MOUSE))) != 0) {
		beginDisplay();
		switch(c) {
		case 'M':	/* button-event */
			event	= keystroke();
			x	= XtermPos() + x_origin;
			y	= XtermPos() + y_origin;

			button	= (event & 3) + 1;
			TRACE(("M-button event:%d x:%d y:%d\n", event, x, y))
			if (button > 3) {
				endofDisplay();
				return TRUE; /* button up */
			}
			wp = row2window(y-1);
			if (insertmode && wp != curwp) {
				kbd_alarm();
				return ABORT;
			}
#if OPT_XTERM >= 3
			/* Tell the xterm how to highlight the selection.
			 * It won't do anything else until we do this.
			 */
			if (wp != 0) {
				firstrow = wp->w_toprow + 1;
				lastrow  = mode_row(wp) + 1;
			} else {		/* from message-line */
				firstrow = term.t_nrow ;
				lastrow  = term.t_nrow + 1;
			}
			if (y >= lastrow)	/* don't select modeline */
				y = lastrow - 1;
			(void)lsprintf(temp, fmt, 1, x, y, firstrow, lastrow);
			putpad(temp);
			TTflush();
#endif	/* OPT_XTERM >= 3 */
			/* Set the dot-location if button 1 was pressed in a
			 * window.
			 */
			if (wp != 0
			 && button == 1
			 && !reading_msg_line
			 && setcursor(y-1, x-1)) {
				/*mlerase();*/
				(void)update(TRUE);
				status = TRUE;
			} else if (button <= 3) {
#if OPT_XTERM >= 3
				/* abort the selection */
				(void)lsprintf(temp, fmt, 0, x, y, firstrow, lastrow);
				putpad(temp);
				TTflush();
#endif	/* OPT_XTERM >= 3 */
				status = ABORT;
			} else {
				status = FALSE;
			}
			break;
#if OPT_XTERM >= 3
		case 't':	/* reports valid text-location */
			x = XtermPos();
			y = XtermPos();

			TRACE(("t: x:%d y:%d\n", x, y))
			setwmark(y-1, x-1);
			yankregion();

			movecursor(save_row, save_col);
			/*mlerase();*/
			(void)update(TRUE);
			break;
		case 'T':	/* reports invalid text-location */
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
			endx   = XtermPos();	/* ending-location */
			endy   = XtermPos();
			mousex = XtermPos();	/* location at mouse-up */
			mousey = XtermPos();

			TRACE(("T: start(%d,%d) end(%d,%d) mouse(%d,%d)\n",
				starty, startx,
				endy,   endx,
				mousey, mousex))
			setcursor(starty - 1, startx - 1);
			setwmark (endy   - 1, endx   - 1);
			if (MK.o != 0 && !is_at_end_of_line(MK))
				MK.o += 1;
			yankregion();

			DOT = save_dot;
			movecursor(save_row, save_col);
			/*mlerase();*/
			(void)update(TRUE);
			break;
#endif /* OPT_XTERM >= 3 */
		default:
			status = FALSE;
		}
		endofDisplay();
	}
	return status;
}
#endif	/* OPT_XTERM */

#endif	/* DISP_TERMCAP */
