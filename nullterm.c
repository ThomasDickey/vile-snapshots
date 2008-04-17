/*
 * Define an empty terminal type for machines where we cannot use 'dumb_term',
 * so that command-line prompting will have something to talk to.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/nullterm.c,v 1.5 2007/12/24 01:54:15 tom Exp $
 */

#include	<estruct.h>
#include	<edef.h>

static void
nullterm_open(void)
{
}

static void
nullterm_close(void)
{
}

static int
nullterm_getch(void)
{
    return esc_c;
}

/*ARGSUSED*/
static OUTC_DCL
nullterm_putch(int c)
{
    OUTC_RET c;
}

static int
nullterm_typahead(void)
{
    return FALSE;
}

static void
nullterm_flush(void)
{
}

/*ARGSUSED*/
static void
nullterm_curmove(int row GCC_UNUSED, int col GCC_UNUSED)
{
}

static void
nullterm_eeol(void)
{
}

static void
nullterm_eeop(void)
{
}

static void
nullterm_beep(void)
{
}

/*ARGSUSED*/
static void
nullterm_rev(UINT state GCC_UNUSED)
{
}

#if OPT_COLOR
#define NO_COLOR(name,value) name = value;
#else
#define NO_COLOR(name,value)	/*nothing */
#endif

/*
 * These are public, since we'll use them as placeholders for unimplemented
 * device methods.
 */
/*ARGSUSED*/
int
nullterm_setdescrip(const char *res GCC_UNUSED)
{
    return (FALSE);
}

/*ARGSUSED*/
int
nullterm_watchfd(int fd GCC_UNUSED, WATCHTYPE type GCC_UNUSED, long *idp GCC_UNUSED)
{
    return 0;
}

/*ARGSUSED*/
void
nullterm_clean(int f GCC_UNUSED)
{
}

void
nullterm_unclean(void)
{
}

void
nullterm_openup(void)
{
}

/*ARGSUSED*/
void
nullterm_cursorvis(int flag GCC_UNUSED)
{
}

/*ARGSUSED*/
void
nullterm_icursor(int c GCC_UNUSED)
{
}

/*ARGSUSED*/
void
nullterm_kclose(void)
{
}

/*ARGSUSED*/
void
nullterm_kopen(void)
{
}

/*ARGSUSED*/
void
nullterm_pflush(void)
{
}

/*ARGSUSED*/
void
nullterm_scroll(int f GCC_UNUSED, int t GCC_UNUSED, int n GCC_UNUSED)
{
}

/*ARGSUSED*/
void
nullterm_setback(int b GCC_UNUSED)
{
    NO_COLOR(gbcolor, C_BLACK)
}

/*ARGSUSED*/
void
nullterm_setccol(int c GCC_UNUSED)
{
    NO_COLOR(gccolor, ENUM_UNKNOWN)
}

/*ARGSUSED*/
void
nullterm_setfore(int f GCC_UNUSED)
{
    NO_COLOR(gbcolor, C_WHITE)
}

/*ARGSUSED*/
void
nullterm_setpal(const char *p GCC_UNUSED)
{
}

/*ARGSUSED*/
void
nullterm_settitle(const char *t GCC_UNUSED)
{
}

/*ARGSUSED*/
void
nullterm_unwatchfd(int fd GCC_UNUSED, long id GCC_UNUSED)
{
}

/*ARGSUSED*/
void
nullterm_mopen(void)
{
}

/*ARGSUSED*/
void
nullterm_mclose(void)
{
}

/*ARGSUSED*/
void
nullterm_mevent(void)
{
}

TERM null_term =
{
    1,
    1,
    80,
    80,
    enc_DEFAULT,
    nullterm_open,
    nullterm_close,
    nullterm_kopen,
    nullterm_kclose,
    nullterm_clean,
    nullterm_unclean,
    nullterm_openup,
    nullterm_getch,
    nullterm_putch,
    nullterm_typahead,
    nullterm_flush,
    nullterm_curmove,
    nullterm_eeol,
    nullterm_eeop,
    nullterm_beep,
    nullterm_rev,
    nullterm_setdescrip,
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
