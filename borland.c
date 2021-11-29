/*
 * Uses Borland console i/o routines.
 * (Created for OS/2 by Charles Moschel 29-MAR-94.)
 * (modified to be more generic, not os/2 specific, pgf, april '94)
 *
 * Supported monitor cards include
 *	CGA, MONO, EGA, VGA.
 *
 * Note: Visual flashes are not yet supported.
 *
 *
 * $Id: borland.c,v 1.43 2020/01/17 23:12:22 tom Exp $
 *
 */

#include        "estruct.h"
#include        "edef.h"

#if SYS_OS2
#define INCL_VIO
#define INCL_NOPMAPI
#include <os2.h>
#undef OPT_COLOR
#endif /* SYS_OS2 */

#if !DISP_BORLAND
#error misconfigured:  DISP_BORLAND should be defined if using borland.c
#endif

#define NROW	50		/* Max Screen size.             */
#define NCOL    80		/* Edit if you want to.         */
#define	SPACE	32		/* space character              */

/* We assume that most users have a color display.  */

#include <conio.h>

#if 0				//def GVAL_VIDEO
#define REVERSED (global_g_val(GVAL_VIDEO) & VAREV)
static void
set_reverse(void)
{
    //if (REVERSED)
    //begin_reverse();
}

#else
#define REVERSED 0
#define set_reverse()		/* nothing */
#endif

static void borflush(void);

static int cfcolor = -1;	/* current foreground color */
static int cbcolor = -1;	/* current background color */
/* ansi to ibm color translation table */
static const char *initpalettestr = "0 4 2 6 1 5 3 7 8 12 10 14 9 13 11 15";
/* black, red, green, yellow, blue, magenta, cyan, white   */

static int scinit(int);

static char linebuf[128];
static int bufpos = 0;
/* *INDENT-OFF* */
static struct {
    char  *seq;
    int   code;
} keyseqs[] = {
    /* Arrow keys */
    {"\0\110",     KEY_Up},
    {"\0\120",     KEY_Down},
    {"\0\115",     KEY_Right},
    {"\0\113",     KEY_Left},
#if 0
    {"\0\110",     KEY_S_Up},	/* shift (unchanged) */
    {"\0\120",     KEY_S_Down},
    {"\0\115",     KEY_S_Right},
    {"\0\113",     KEY_S_Left},
    {"\0\215",     KEY_C_Up},	/* control */
    {"\0\221",     KEY_C_Down},
    {"\0\164",     KEY_C_Right},
    {"\0\163",     KEY_C_Left},
    {"\0\230",     KEY_A_Up},	/* left-alt */
    {"\0\240",     KEY_A_Down},
    {"\0\235",     KEY_A_Right},
    {"\0\233",     KEY_A_Left},
#endif
    /* page scroll */
    {"\0\121",     KEY_Next},
    {"\0\111",     KEY_Prior},
    {"\0\107",     KEY_Home},
    {"\0\117",     KEY_End},
#if 0
    {"\0\121",     KEY_S_Next},	/* shift (unchanged) */
    {"\0\111",     KEY_S_Prior},
    {"\0\107",     KEY_S_Home},
    {"\0\117",     KEY_S_End},
    {"\0\166",     KEY_C_Next},	/* control */
    {"\0\204",     KEY_C_Prior},
    {"\0\167",     KEY_C_Home},
    {"\0\165",     KEY_C_End},
    {"\0\241",     KEY_A_Next},	/* left-alt */
    {"\0\231",     KEY_A_Prior},
    {"\0\227",     KEY_A_Home},
    {"\0\237",     KEY_A_End},
#endif
    /* editing */
    {"\0\122",     KEY_Insert},
    {"\0\123",     KEY_Delete},
#if 0
    {"\0\122",     KEY_S_Insert},	/* shift (unchanged) */
    {"\0\123",     KEY_S_Delete},
    {"\0\222",     KEY_C_Insert},	/* control */
    {"\0\223",     KEY_C_Delete},
    {"\0\242",     KEY_A_Insert},	/* left-alt */
    {"\0\243",     KEY_A_Delete},
#endif
    /* function keys */
    {"\0;",        KEY_F1},
    {"\0<",        KEY_F2},
    {"\0=",        KEY_F3},
    {"\0>",        KEY_F4},
    {"\0?",        KEY_F5},
    {"\0@",        KEY_F6},
    {"\0A",        KEY_F7},
    {"\0B",        KEY_F8},
    {"\0C",        KEY_F9},
    {"\0D",        KEY_F10},
    {"\0\205",     KEY_F11},
    {"\0\206",     KEY_F12},

    /* shift + F1-F12 */
    {"\0T",        KEY_F13},
    {"\0U",        KEY_F14},
    {"\0V",        KEY_F15},
    {"\0W",        KEY_F16},
    {"\0X",        KEY_F17},
    {"\0Y",        KEY_F18},
    {"\0Z",        KEY_F19},
    {"\0[",        KEY_F20},
    {"\0\\",       KEY_F21},
    {"\0]",        KEY_F22},
    {"\0\207",     KEY_F23},
    {"\0\210",     KEY_F24},

    /* control + F1-F12 */
    {"\0^",        KEY_F25},
    {"\0_",        KEY_F26},
    {"\0`",        KEY_F27},
    {"\0a",        KEY_F28},
    {"\0b",        KEY_F29},
    {"\0c",        KEY_F30},
    {"\0d",        KEY_F31},
    {"\0e",        KEY_F32},
    {"\0f",        KEY_F33},
    {"\0g",        KEY_F34},
    {"\0\211",     KEY_F35},
    {"\0\212",     KEY_F36},

    /* left-alt + F1-F12 */
    {"\0h",        KEY_F37},
    {"\0i",        KEY_F38},
    {"\0j",        KEY_F39},
    {"\0k",        KEY_F40},
    {"\0l",        KEY_F41},
    {"\0m",        KEY_F42},
    {"\0n",        KEY_F43},
    {"\0o",        KEY_F44},
    {"\0p",        KEY_F45},
    {"\0q",        KEY_F46},
    {"\0\213",     KEY_F47},
    {"\0\214",     KEY_F48},

    {"\0\017",     KEY_BackTab },
};
/* *INDENT-ON* */

static int ibmtype;

static int borlastchar = -1;
static int bortttypahead(void);
static int borttgetc(void);

#if OPT_ICURSOR
static void
boricursor(int cmode)
{
    switch (cmode) {
    case -1:
	_setcursortype(_NOCURSOR);
	break;
    case 0:
	_setcursortype(_NORMALCURSOR);
	break;
    case 1:
	_setcursortype(_SOLIDCURSOR);
	break;
    }
}
#endif

#if OPT_COLOR
static void
borfcol(int color)		/* set the current output color */
{
    if (color < 0)
	color = C_WHITE;
    cfcolor = ctrans[color];
    textcolor(cfcolor & 15);
}

static void
borbcol(int color)		/* set the current background color */
{
    if (color < 0)
	color = C_BLACK;
    cbcolor = ctrans[color];
    textbackground(cbcolor & 7);
}

static void
borspal(const char *thePalette)	/* reset the palette registers */
{
    borflush();
    set_ctrans(thePalette);
}

static void
setup_colors(void)
{
    borfcol(gfcolor);
    borbcol(gbcolor);
}

static void
reset_colors(void)
{
    borfcol(7 /* not necessarily C_WHITE */ );
    borbcol(C_BLACK);
}

#else
#define setup_colors()		/* nothing */
#define reset_colors()		/* nothing */
#endif

static void
borflush(void)
{
    if (bufpos) {
	linebuf[bufpos] = '\0';
	cputs(linebuf);
	bufpos = 0;
    }
}

static void
bormove(int row, int col)
{
    borflush();
    gotoxy(col + 1, row + 1);
}

/* erase to the end of the line */
static void
boreeol(void)
{
    borflush();
    clreol();			/* pointer to the destination line */
}

/* put a character at the current position in the current colors */
static void
borputc(int ch)
{
    linebuf[bufpos++] = ch;
}

static void
boreeop(void)
{
    int x, y, i;
    struct text_info t;

    borflush();
    x = wherex();
    y = wherey();
    gettextinfo(&t);
    clreol();
    for (i = x + 1; i <= t.screenheight; i++) {
	gotoxy(1, i);
	clreol();
    }
    gotoxy(x, y);
}

#if OPT_VIDEO_ATTRS
static void
bor_attr(UINT attr)		/* change video attributes */
{
    static UINT last;
    UINT next;
    int colored = FALSE;

    if (attr != last) {
	last = attr;
	borflush();

	attr = VATTRIB(attr);
#if 0				//def GVAL_VIDEO
	if (REVERSED) {
	    if (attr & VASEL)
		attr ^= VASEL;
	    else
		attr ^= VAREV;
	}
#endif
	if (attr & VASPCOL) {
	    colored = TRUE;
	    attr = VCOLORATTR((VCOLORNUM(attr) & (ncolors - 1)));
	} else {
	    if (attr & VACOLOR) {
		attr &= (VCOLORATTR(ncolors - 1) | ~VACOLOR);
		colored = TRUE;
	    }
	    attr &= ~(VAML | VAMLFOC);
	}

	next = ctrans[VCOLORNUM(attr)];
	if (attr & (VASEL | VAREV)) {
	    textcolor((colored ? next : cbcolor) & 15);
	    textbackground(cfcolor & 7);
	} else {
	    textcolor((colored ? next : cfcolor) & 15);
	    textbackground(cbcolor & 7);
	}
    }
}
#else
static void
bor_rev(UINT reverse)		/* change reverse video state */
{
    borflush();
    if (reverse) {
	textbackground(cfcolor & 7);
	textcolor(cbcolor & 15);
    } else {
	textbackground(cbcolor & 7);
	textcolor(cfcolor & 15);
    }
}
#endif

static int
borcres(const char *res)	/* change screen resolution */
{
    char *dst;
    register int i;		/* index */
    int status = FALSE;

    strcpy(current_res_name, res);
    borflush();
    /* find the default configuration */
    if (!strcmp(res, "?")) {
	status = scinit(-1);
    } else {			/* specify a number */
	if ((i = (int) strtol(res, &dst, 0)) >= 0 && !*dst) {
	    /* only allow valid row selections */
	    /* Are these all valid under dos?  */
	    if (i == 2)
		status = scinit(25);
	    if (i == 4)
		status = scinit(43);
	    if (i == 5)
		status = scinit(50);
	    if (i == 6)
		status = scinit(60);

	    if (i > 6 && i < 28)
		status = scinit(25);

	    if (i >= 28 && i < 43)
		status = scinit(28);

	    if (i >= 43 && i < 50)
		status = scinit(43);

	    if (i >= 50 && i < 60)
		status = scinit(50);

	    if (i >= 60)
		status = scinit(60);

	}
    }
    sgarbf = TRUE;
    return status;
}

static void
borbeep(void)
{
    putch('\a');
}

static void
boropen(void)
{
    static int already_open;

    int i;

#if OPT_LOCALE
    vl_open_mbterm();
#endif

    if (!already_open) {
	already_open = TRUE;

	set_palette(initpalettestr);
	setup_colors();

	if (!borcres(current_res_name))
	    (void) scinit(-1);
	ttopen();
	for (i = TABLESIZE(keyseqs) - 1; i >= 0; i--)
	    addtosysmap(keyseqs[i].seq, 2, keyseqs[i].code);
    }
}

static void
borclose(void)
{
    int current_type = ibmtype;

    borflush();
#if OPT_ICURSOR
    _setcursortype(_NORMALCURSOR);
#endif
    ibmtype = current_type;	/* ...so subsequent term.open restores us */

    reset_colors();
    kbd_erase_to_end(0);
    kbd_flush();
#if OPT_LOCALE
    vl_close_mbterm();
#endif
}

static void
borkopen(void)			/* open the keyboard */
{
    setup_colors();
    term.curmove(term.rows - 1, 0);	/* cf: dumbterm.c */
    term.eeol();

    /* ms_install(); */
}

static void
borkclose(void)			/* close the keyboard */
{
    /* ms_deinstall(); */

    reset_colors();
    term.curmove(term.rows - 1, 0);	/* cf: dumbterm.c */
    term.eeol();
}

static
int
borttgetc(void)
{
    return (borlastchar = ttgetc());
}

/* bortttypahead:  Check to see if any characters are already in the
 * keyboard buffer.In Borland C OS/2 1.5, kbhit doesn't return non-zero for
 * the 2nd part of an extended character, but 1st part is still 0 so use
 * that as indicator as well (why it's saved in borttgetc).
 */
static int
bortttypahead(void)
{

    return (kbhit() != 0 || borlastchar == 0);

}

#if SYS_OS2			/* all modes are available under OS/2 */
static int
scinit(int rows)		/* initialize the screen head pointers */
{

    /* and set up the various parameters as needed */

    if (rows == -1) {
	struct text_info ti;
	gettextinfo(&ti);
	rows = ti.screenheight;
    }

    switch (rows) {

	/* these are enum's, and thus cannot easily be checked, ie. #ifdef C80X21 */
    case 21:			/* color C80X21 */
	textmode(C80X21);
	newscreensize(21, term.cols);
	(void) strcpy(screen_desc, "C80X21");
	break;

    default:
    case 25:			/* Color graphics adapter */
	textmode(C80);
	newscreensize(25, term.cols);
	(void) strcpy(screen_desc, "C80");
	break;

    case 28:			/* Enhanced graphics adapter */
	textmode(C80X28);
	newscreensize(28, term.cols);
	(void) strcpy(screen_desc, "C80X28");
	break;

    case 43:			/* Enhanced graphics adapter */
	textmode(C80X43);
	newscreensize(43, term.cols);
	(void) strcpy(screen_desc, "C80X43");
	break;

    case 50:			/* VGA adapter */
	textmode(C80X50);
	newscreensize(50, term.cols);
	(void) strcpy(screen_desc, "C80X50");
	break;

    case 60:			/* Enhanced graphics adapter */
	textmode(C80X60);
	newscreensize(60, term.cols);
	(void) strcpy(screen_desc, "C80X60");
	break;

    }

    ibmtype = rows;

    return (TRUE);
}

#else /* SYS_OS2 */

static int
scinit(int rows)		/* initialize the screen head pointers */
{

    /* and set up the various parameters as needed */

    struct text_info ti;
    int oldrows;

    gettextinfo(&ti);
    oldrows = ti.screenheight;
    if (rows == -1)
	rows = oldrows;

    switch (rows) {

	/* DOS has only BW40, C40, BW80, C80, MONO, and C4350 */

    default:
    case 25:			/* Color graphics adapter */
	if (oldrows != 25)
	    textmode(C80);
	newscreensize(25, term.cols);
	(void) strcpy(screen_desc, "C80");
	break;

    case 43:
    case 50:
    case 60:
	if (rows != oldrows)
	    textmode(C4350);
	gettextinfo(&ti);
	rows = ti.screenheight;
	newscreensize(rows, term.cols);
	sprintf(screen_desc, "C80X%d", rows);
	break;

    }

    ibmtype = rows;

    return (TRUE);
}

#endif /* SYS_OS2 */

/*
 * Move 'n' lines starting at 'from' to 'to'
 *
 * OPT_PRETTIER_SCROLL is prettier but slower -- it scrolls a line at a time
 *	instead of all at once.
 */

/* move howmany lines starting at from to to */
static void
borscroll(int from, int to, int n)
{
    int i;
    struct text_info t;

    borflush();
    if (to == from)
	return;
#if OPT_PRETTIER_SCROLL
    if (absol(from - to) > 1) {
	borscroll(from, (from < to) ? to - 1 : to + 1, n);
	if (from < to)
	    from = to - 1;
	else
	    from = to + 1;
    }
#endif
    gettextinfo(&t);
    if (to < from) {
	window(1, to + 1, t.screenwidth, from + n);
	gotoxy(1, 1);
	for (i = from - to; i > 0; i--)
	    delline();
	gotoxy(1, n + 1);
	for (i = from - to; i > 0; i--)
	    insline();
    } else {
	window(1, from + 1, t.screenwidth, to + n);
	gotoxy(1, n + 1);
	for (i = to - from; i > 0; i--)
	    delline();
	gotoxy(1, 1);
	for (i = to - from; i > 0; i--)
	    insline();
    }
    window(1, 1, t.screenwidth, t.screenheight);
}

/*--------------------------------------------------------------------------*/
/*
 * Standard terminal interface dispatch table. Most of the fields point into
 * "termio" code.
 */
TERM term =
{
    NROW,
    NROW,
    NCOL,
    NCOL,
    dumb_set_encoding,
    dumb_get_encoding,
    boropen,
    borclose,
    borkopen,
    borkclose,
    ttclean,
    ttunclean,
    nullterm_openup,
    borttgetc,
    borputc,
    bortttypahead,
    borflush,
    bormove,
    boreeol,
    boreeop,
    borbeep,
#if OPT_VIDEO_ATTRS
    bor_attr,
#else
    bor_rev,
#endif
    borcres,
#if OPT_COLOR
    borfcol,
    borbcol,
    borspal,
#else
    nullterm_setfore,
    nullterm_setback,
    nullterm_setpal,
#endif
    nullterm_setccol,
    borscroll,
    nullterm_pflush,
#if OPT_ICURSOR
    boricursor,
#else
    nullterm_icursor,
#endif
    nullterm_settitle,
    nullterm_watchfd,
    nullterm_unwatchfd,
    nullterm_cursorvis,
    nullterm_mopen,
    nullterm_mclose,
    nullterm_mevent,
};
