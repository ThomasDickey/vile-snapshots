/*
 * The routines in this file provide support for ANSI style terminals
 * over a serial line. The serial I/O services are provided by routines in
 * "termio.c". It compiles into nothing if not an ANSI device.
 *
 *
 * $Id: ansi.c,v 1.54 2020/01/17 23:17:05 tom Exp $
 */

#include	"estruct.h"
#include	"edef.h"

#if DISP_ANSI

#define SCROLL_REG 1

#if SYS_MSDOS
#define NROW    25
#define NCOL    80
#define MAXNROW	60
#define MAXNCOL	132
#undef SCROLL_REG		/* ANSI.SYS can't do scrolling */
#define SCROLL_REG 0
#endif

#ifdef ANSI_RESIZE
#define NROW    77
#define NCOL    132
#endif

#if    !defined NROW && defined(linux)
#define NROW	25		/* Screen size.                 */
#define NCOL	80		/* Edit if you want to.         */
#endif

#ifndef NROW
#define NROW	24		/* Screen size.                 */
#define NCOL	80		/* Edit if you want to.         */
#endif
#ifndef MAXNROW
#define MAXNROW	NROW
#define MAXNCOL	NCOL
#endif

#if OPT_COLOR

static int cfcolor = -1;	/* current foreground color */
static int cbcolor = -1;	/* current background color */

	/* ANSI: black, red, green, yellow, blue, magenta, cyan, white   */
static const char ANSI_palette[] =
{"0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"};

#endif /* OPT_COLOR */

static void
csi(void)
{
    ttputc(ESC);
    ttputc('[');
}

static void
ansi_parm(register int n)
{
    register int q, r;

#ifdef optimize_works		/* i don't think it does, although it should, to be ANSI */
    if (n == 1)
	return;
#endif

    q = n / 10;
    if (q != 0) {
	r = q / 10;
	if (r != 0) {
	    ttputc((r % 10) + '0');
	}
	ttputc((q % 10) + '0');
    }
    ttputc((n % 10) + '0');
}

#if OPT_COLOR
static void
textcolor(int color)
{
    csi();
    ansi_parm(color >= 0 ? (color + 30) : 39);
    ttputc('m');
}

static void
textbackground(int color)
{
    csi();
    ansi_parm(color >= 0 ? (color + 40) : 49);
    ttputc('m');
}

static void
ansi_fcol(int color)		/* set the current output color */
{
    if (color < 0)
	color = C_WHITE;
    if (color == cfcolor)
	return;
    textcolor(color);
    cfcolor = color;
}

static void
ansi_bcol(int color)		/* set the current background color */
{
    if (color < 0)
	color = C_BLACK;
    if (color == cbcolor)
	return;
    textbackground(color);
    cbcolor = color;
}

static void
ansi_spal(const char *thePalette)
{				/* reset the palette registers */
    set_ctrans(thePalette);
}
#endif

static void
ansi_move(int row, int col)
{
    csi();
    ansi_parm(row + 1);
    ttputc(';');
    ansi_parm(col + 1);
    ttputc('H');
}

static void
ansi_eeol(void)
{
    csi();
    ttputc('K');
}

static void
ansi_eeop(void)
{
#if OPT_COLOR
    ansi_fcol(gfcolor);
    ansi_bcol(gbcolor);
#endif
    csi();
    ttputc('2');
    ttputc('J');
}

#if OPT_COLOR && !OPT_VIDEO_ATTRS
static void
force_colors(int fc, int bc)
{
    cfcolor =
	cbcolor = -1;
    ansi_fcol(fc);
    ansi_bcol(bc);
}
#endif

#if OPT_VIDEO_ATTRS
static void
ansi_attr(UINT attr)		/* change video attributes */
{
    static UINT last;
    UINT next;
    int colored = FALSE;

    if (attr != last) {
	last = attr;

	attr = VATTRIB(attr);
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
	    textcolor((colored ? next : cbcolor));
	    textbackground(cfcolor);
	} else {
	    textcolor((colored ? next : cfcolor));
	    textbackground(cbcolor);
	}
    }
}
#else /* ! OPT_VIDEO_ATTRS */
#ifdef BROKEN_REVERSE_VIDEO
/* there was something wrong with this "fix".  the "else" of
		the ifdef just uses "ESC [ 7 m" to set reverse
		video, and it works under DOS for me....  but then, i
		use an "after-market" ansi driver -- nnansi593.zip, from
		oak.oakland.edu, or any simtel mirror. */
static void
ansi_rev(			/* change reverse video state */
	    UINT state)		/* TRUE = reverse, FALSE = normal */
{
#if !OPT_COLOR
    static UINT revstate = SORTOFTRUE;
    if (state == revstate)
	return;
    revstate = state;
#endif

    csi();
#if OPT_COLOR && SYS_MSDOS
    ttputc('1');		/* bold-on */
#else
    if (state)
	ttputc('7');		/* reverse-video on */
#endif
    ttputc('m');

#if OPT_COLOR && !OPT_VIDEO_ATTRS
#if SYS_MSDOS
    /*
     * Setting reverse-video with ANSI.SYS seems to reset the colors to
     * monochrome.  Using the colors directly to simulate reverse video
     * works better. Bold-face makes the foreground colors "look" right.
     */
    if (state)
	force_colors(cbcolor, cfcolor);
    else
	force_colors(cfcolor, cbcolor);
#else /* normal ANSI-reverse */
    if (state == FALSE) {
	force_colors(cfcolor, cbcolor);
    }
#endif /* MSDOS vs ANSI-reverse */
#endif /* OPT_COLOR */
}

#else

static void
ansi_rev(			/* change reverse video state */
	    UINT state)		/* TRUE = reverse, FALSE = normal */
{
    static UINT revstate = SORTOFTRUE;
    if (state == revstate)
	return;
    revstate = state;

    csi();
    if (state)
	ttputc('7');		/* reverse-video on */
    ttputc('m');
#if OPT_COLOR
    force_colors(cfcolor, cbcolor);
#endif
}

#endif
#endif /* ! OPT_VIDEO_ATTRS */

static void
ansi_beep(void)
{
    ttputc(BEL);
    ttflush();
}

#if SCROLL_REG
static void
ansi_scrollregion(int top, int bot)
{
    csi();
    ansi_parm(top + 1);
    ttputc(';');
    if (bot != term.rows - 1)
	ansi_parm(bot + 1);
    ttputc('r');
}
#endif

/* if your ansi terminal can scroll regions, like the vt100, then define
	SCROLL_REG.  If not, you can use delete/insert line code, which
	is prettier but slower if you do it a line at a time instead of
	all at once.
*/

/* move howmany lines starting at from to to */
static void
ansi_scroll(int from, int to, int n)
{
    int i;
    if (to == from)
	return;
#if SCROLL_REG
    if (to < from) {
	ansi_scrollregion(to, from + n - 1);
	ansi_move(from + n - 1, 0);
	for (i = from - to; i > 0; i--)
	    ttputc('\n');
    } else {			/* from < to */
	ansi_scrollregion(from, to + n - 1);
	ansi_move(from, 0);
	for (i = to - from; i > 0; i--) {
	    ttputc(ESC);
	    ttputc('M');
	}
    }
    ansi_scrollregion(0, term.maxrows - 1);

#else /* use insert and delete line */
#if OPT_PRETTIER_SCROLL
    if (absol(from - to) > 1) {
	ansi_scroll(from, (from < to) ? to - 1 : to + 1, n);
	if (from < to)
	    from = to - 1;
	else
	    from = to + 1;
    }
#endif
    if (to < from) {
	ansi_move(to, 0);
	csi();
	ansi_parm(from - to);
	ttputc('M');		/* delete */
	ansi_move(to + n, 0);
	csi();
	ansi_parm(from - to);
	ttputc('L');		/* insert */
    } else {
	ansi_move(from + n, 0);
	csi();
	ansi_parm(to - from);
	ttputc('M');		/* delete */
	ansi_move(from, 0);
	csi();
	ansi_parm(to - from);
	ttputc('L');		/* insert */
    }
#endif
}

static void
ansi_open(void)
{
    static int already_open = FALSE;
    if (!already_open) {
	already_open = TRUE;
	strcpy(screen_desc, "NORMAL");
	revexist = TRUE;
#if ! SYS_MSDOS
	/* Get screen size from system */
	getscreensize(&term.cols, &term.rows);
	if (term.rows <= 1)
	    term.rows = 24;

	if (term.cols <= 1)
	    term.cols = 80;
	/* we won't resize, but need the measured size */
	term.maxrows = term.rows;
	term.maxcols = term.cols;
#endif
#if OPT_COLOR
	set_palette(ANSI_palette);
#endif
	ttopen();
    }
}

static void
ansi_close(void)
{
    term.curmove(term.rows - 1, 0);	/* cf: dumbterm.c */
    ansi_eeol();
#if OPT_COLOR
    ansi_fcol(C_WHITE);
    ansi_bcol(C_BLACK);
#endif
}

/*
 * Standard terminal interface dispatch table. Most of the fields point into
 * "termio" code.
 */
TERM term =
{
    MAXNROW,			/* max */
    NROW,			/* current */
    MAXNCOL,			/* max */
    NCOL,			/* current */
    dumb_set_encoding,
    dumb_get_encoding,
    ansi_open,
    nullterm_kopen,
    nullterm_kclose,
    ansi_close,
    ttclean,
    ttunclean,
    nullterm_openup,
    ttgetc,
    ttputc,
    tttypahead,
    ttflush,
    ansi_move,
    ansi_eeol,
    ansi_eeop,
    ansi_beep,
#if OPT_VIDEO_ATTRS
    ansi_attr,
#else
    ansi_rev,
#endif
    nullterm_setdescrip,
#if OPT_COLOR
    ansi_fcol,
    ansi_bcol,
    ansi_spal,
#else
    nullterm_setfore,
    nullterm_setback,
    nullterm_setpal,
#endif
    nullterm_setccol,
    ansi_scroll,
    nullterm_pflush,
    nullterm_icursor,
    nullterm_settitle,
    nullterm_watchfd,
    nullterm_unwatchfd,
    nullterm_cursorvis,
    nullterm_mopen,
    nullterm_mclose,
    nullterm_mevent,
};

#endif /* DISP_ANSI */
