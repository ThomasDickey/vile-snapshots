/*	Dumb terminal driver, for I/O before we get into screen mode.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/dumbterm.c,v 1.7 1996/04/14 23:37:50 pgf Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"

#define MARGIN	8
#define SCRSIZ	64
#define NPAUSE	10			/* # times thru update to pause */

static int  dumb_cres     ( char * res);
static int  dumb_getc     (void);
static int  dumb_typahead (void);
static void dumb_beep     (void);
static void dumb_eeol     (void);
static void dumb_eeop     (void);
static void dumb_flush    (void);
static void dumb_kclose   (void);
static void dumb_kopen    (void);
static void dumb_move     ( int row, int col );
static void dumb_putc     ( int c );
static void dumb_rev      ( int state );

static void flush_blanks  (void);

TERM dumb_term = {
	1,
	1,
	80,
	80,
	MARGIN,
	SCRSIZ,
	NPAUSE,
	0,		/* use this to put us into raw mode */
	0,		/* ...and this, just in case we exit */
	dumb_kopen,
	dumb_kclose,
	dumb_getc,
	dumb_putc,
	dumb_typahead,
	dumb_flush,
	dumb_move,
	dumb_eeol,
	dumb_eeop,
	dumb_beep,
	dumb_rev,
	dumb_cres,
	null_t_setfor,
	null_t_setback,
	null_t_setpal,
	null_t_scroll,
	null_t_pflush,
	null_t_icursor,
	null_t_title,
};

static	int	this_col;
static	int	last_col;

static void
flush_blanks(void)
{
	if (last_col > 0) {
		while (last_col++ < this_col)
			(void)putchar(' ');
		last_col = 0;
	}
	TTflush();
}

static void
dumb_kopen(void)
{
}

static void
dumb_kclose(void)
{
}

static int
dumb_getc(void)
{
	flush_blanks();
	return getchar();
}

static void
dumb_putc(int c)
{
	if (isspace(c)) {
		if (last_col == 0)
			last_col = this_col;
	} else {
		flush_blanks();
		(void)putchar(c);
	}
	this_col++;
}

static int
dumb_typahead(void)
{
	return TRUE;
}

static void
dumb_flush(void)
{
	(void)fflush(stdout);
}

/*ARGSUSED*/
static void
dumb_move(int row, int col)
{
	if (last_col == 0)
		last_col = this_col;
	if (col == 0) {
		putchar('\r');
		if (last_col != 0)
			putchar('\n');
	} else if (last_col > col) {
		while (last_col-- > col)
			putchar('\b');
	} else if (last_col < col) {
		while (last_col++ < col)
			putchar(' ');
	}
	last_col = 0;
	this_col = col;
}

static void
dumb_eeol(void)
{
}

static void
dumb_eeop(void)
{
}

/*ARGSUSED*/
static int
dumb_cres(	/* change screen resolution */
char *	res)
{
	return(FALSE);
}

/* ARGSUSED */
static void
dumb_rev(int state)
{
}

static void
dumb_beep(void)
{
	putchar(BEL);
}
