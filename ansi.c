/*
 * The routines in this file provide support for ANSI style terminals
 * over a serial line. The serial I/O services are provided by routines in
 * "termio.c". It compiles into nothing if not an ANSI device.
 *
 *
 * $Header: /users/source/archives/vile.vcs/RCS/ansi.c,v 1.45 2004/06/07 21:57:41 tom Exp $
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

#define NPAUSE	100		/* # times thru update to pause */

#if OPT_COLOR

static int cfcolor = -1;	/* current forground color */
static int cbcolor = -1;	/* current background color */

#endif

static void
csi(void)
{
    ttputc(ESC);
    ttputc('[');
}

static void
ansiparm(register int n)
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
ansifcol(int color)		/* set the current output color */
{
    if (color < 0)
	color = C_WHITE;
    if (color == cfcolor)
	return;
    csi();
    ansiparm(color + 30);
    ttputc('m');
    cfcolor = color;
}

static void
ansibcol(int color)		/* set the current background color */
{
    if (color < 0)
	color = C_BLACK;
    if (color == cbcolor)
	return;
    csi();
    ansiparm(color + 40);
    ttputc('m');
    cbcolor = color;
}
#endif

static void
ansimove(int row, int col)
{
    csi();
    ansiparm(row + 1);
    ttputc(';');
    ansiparm(col + 1);
    ttputc('H');
}

static void
ansieeol(void)
{
    csi();
    ttputc('K');
}

static void
ansieeop(void)
{
#if OPT_COLOR
    ansifcol(gfcolor);
    ansibcol(gbcolor);
#endif
    csi();
    ttputc('2');
    ttputc('J');
}

#if OPT_COLOR
static void
force_colors(int fc, int bc)
{
    cfcolor =
	cbcolor = -1;
    ansifcol(fc);
    ansibcol(bc);
}
#endif

#ifdef BROKEN_REVERSE_VIDEO
/* there was something wrong with this "fix".  the "else" of
		the ifdef just uses "ESC [ 7 m" to set reverse
		video, and it works under DOS for me....  but then, i
		use an "after-market" ansi driver -- nnansi593.zip, from
		oak.oakland.edu, or any simtel mirror. */
static void
ansirev(			/* change reverse video state */
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

#if OPT_COLOR
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
ansirev(			/* change reverse video state */
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

static void
ansibeep(void)
{
    ttputc(BEL);
    ttflush();
}

#if SCROLL_REG
static void
ansiscrollregion(int top, int bot)
{
    csi();
    ansiparm(top + 1);
    ttputc(';');
    if (bot != term.rows - 1)
	ansiparm(bot + 1);
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
ansiscroll(int from, int to, int n)
{
    int i;
    if (to == from)
	return;
#if SCROLL_REG
    if (to < from) {
	ansiscrollregion(to, from + n - 1);
	ansimove(from + n - 1, 0);
	for (i = from - to; i > 0; i--)
	    ttputc('\n');
    } else {			/* from < to */
	ansiscrollregion(from, to + n - 1);
	ansimove(from, 0);
	for (i = to - from; i > 0; i--) {
	    ttputc(ESC);
	    ttputc('M');
	}
    }
    ansiscrollregion(0, term.maxrows - 1);

#else /* use insert and delete line */
#if OPT_PRETTIER_SCROLL
    if (absol(from - to) > 1) {
	ansiscroll(from, (from < to) ? to - 1 : to + 1, n);
	if (from < to)
	    from = to - 1;
	else
	    from = to + 1;
    }
#endif
    if (to < from) {
	ansimove(to, 0);
	csi();
	ansiparm(from - to);
	ttputc('M');		/* delete */
	ansimove(to + n, 0);
	csi();
	ansiparm(from - to);
	ttputc('L');		/* insert */
    } else {
	ansimove(from + n, 0);
	csi();
	ansiparm(to - from);
	ttputc('M');		/* delete */
	ansimove(from, 0);
	csi();
	ansiparm(to - from);
	ttputc('L');		/* insert */
    }
#endif
}

static void
ansiopen(void)
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
	ttopen();
    }
}

static void
ansiclose(void)
{
    term.curmove(term.rows - 1, 0);	/* cf: dumbterm.c */
    ansieeol();
#if OPT_COLOR
    ansifcol(C_WHITE);
    ansibcol(C_BLACK);
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
    NPAUSE,
    ansiopen,
    nullterm_kopen,
    nullterm_kclose,
    ansiclose,
    ttgetc,
    ttputc,
    tttypahead,
    ttflush,
    ansimove,
    ansieeol,
    ansieeop,
    ansibeep,
    ansirev,
    nullterm_setdescrip,
#if OPT_COLOR
    ansifcol,
    ansibcol,
#else
    nullterm_setfore,
    nullterm_setback,
#endif
    nullterm_setpal,		/* no palette */
    nullterm_setccol,
    ansiscroll,
    nullterm_pflush,
    nullterm_icursor,
    nullterm_settitle,
    nullterm_watchfd,
    nullterm_unwatchfd,
    nullterm_cursorvis,
};

#endif /* DISP_ANSI */
