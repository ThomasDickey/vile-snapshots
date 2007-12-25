/*	Dumb terminal driver, for I/O before we get into screen mode.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/dumbterm.c,v 1.23 2007/12/24 01:52:26 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"

static int this_col;
static int last_col;

static void
flush_blanks(void)
{
    if (last_col > 0) {
	while (last_col++ < this_col)
	    (void) putchar(' ');
	last_col = 0;
    }
    term.flush();
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

static OUTC_DCL
dumb_putc(int c)
{
    if (isSpace(c)) {
	if (last_col == 0)
	    last_col = this_col;
    } else {
	flush_blanks();
	(void) putchar(c);
    }
    this_col++;
    OUTC_RET c;
}

static int
dumb_typahead(void)
{
    return TRUE;
}

static void
dumb_flush(void)
{
    (void) fflush(stdout);
}

/*ARGSUSED*/
static void
dumb_move(int row GCC_UNUSED, int col)
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
dumb_cres(			/* change screen resolution */
	     const char *res GCC_UNUSED)
{
    return (FALSE);
}

/* ARGSUSED */
static void
dumb_rev(UINT state GCC_UNUSED)
{
}

static void
dumb_beep(void)
{
    putchar(BEL);
}

TERM dumb_term =
{
    1,
    1,
    80,
    80,
    enc_DEFAULT,
    0,				/* use this to put us into raw mode */
    0,				/* ...and this, just in case we exit */
    dumb_kopen,
    dumb_kclose,
    nullterm_clean,
    nullterm_unclean,
    nullterm_openup,
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
    nullterm_setfore,
    nullterm_setback,
    nullterm_setpal,
    nullterm_setccol,
    nullterm_scroll,
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
