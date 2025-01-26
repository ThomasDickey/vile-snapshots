/*
 * Configurable headers used by termcap/terminfo driver for vile.
 *
 * $Id: tcap.h,v 1.22 2025/01/26 11:58:52 tom Exp $
 */

#ifndef VILE_TCAP_H
#define VILE_TCAP_H 1

#ifdef __cplusplus
extern "C" {
#endif

#undef TRUE
#undef FALSE
#undef WINDOW		/* avoid conflict with <curses.h> or <term.h> */

#undef MK
#define MK other_MK	/* workaround for bug in NetBSD 1.5 curses */

/* _XOPEN_SOURCE_EXTENDED is needed for the wide-character X/Open functions */
#ifdef NCURSES
#  ifndef _XOPEN_SOURCE_EXTENDED
#    define _XOPEN_SOURCE_EXTENDED 1
#  endif
#endif

/*
 * Workaround for breakage in FreeBSD's header files (updates to wchar.h were
 * not reflected in updates to curses.h).
 */
#if DISP_CURSES && defined(__FreeBSD__) && defined(FREEBSD_BROKE_NCURSES)
#define __wint_t
#define __wchar_t
#endif

/*
 * Similar breakage on OpenBSD (updates to stddef.h were not reflected in
 * updates to ncurses.h).
 */
#if DISP_CURSES && defined(__OpenBSD__)
#if defined(_XOPEN_SOURCE_EXTENDED) && !defined(_WCHAR_T)
#define _WCHAR_T
#endif
#endif

#ifdef NEED_CURSES_H
#  ifdef HAVE_NCURSESW_NCURSES_H
#    include <ncursesw/ncurses.h>
#  else
#    ifdef HAVE_NCURSES_NCURSES_H
#      include <ncurses/ncurses.h>
#    else
#      ifdef HAVE_NCURSES_H
#        include <ncurses.h>
#      else
#        include <curses.h>
#      endif
#    endif
#  endif
#endif

#ifdef HAVE_NCURSES_TERM_H
#  include <ncurses/term.h>
#else
#  ifdef HAVE_TERM_H
#    include <term.h>
#  endif
#endif

#ifdef NEED_TERMCAP_H
#  include <termcap.h>
#endif

#undef MK
#if WINMARK
#define MK curwp->w_mark
#else
#define MK Mark
#endif

#ifndef TRUE
#  define TRUE 1
#endif
#ifndef FALSE
#  define FALSE 0
#endif

#undef USE_TERMCAP

#if USE_TERMINFO
#  define USE_TERMCAP 0
#  define CAPNAME(a,b)        b
#  define NO_CAP(s)           (s == NULL || s == (char *)-1)
#  if !defined(HAVE_TIGETNUM) && defined(HAVE_TIGETINT)
#    define tigetnum tigetint
#  endif
#else /* USE_TERMCAP */
#  define USE_TERMCAP 1
#  define CAPNAME(a,b)        a
#  define NO_CAP(s)           (s == 0)
#endif /* USE_TERMINFO */

#ifdef HAVE_EXTERN_TCAP_PC
extern char PC;			/* used in 'tputs()' */
#endif

#define I_AM_XTERM(given) \
    if (given != NULL && \
    	(strstr(given, "xterm") != NULL || strstr(given, "rxvt") != NULL)) { \
	i_am_xterm = TRUE; \
    } else if ((t = TGETSTR(CAPNAME("Km", "kmous"), &p)) != NULL \
	       && (t != (char *) (-1)) \
	       && !strcmp(t, "\033[M")) { \
	i_am_xterm = TRUE; \
    } else if (TGETFLAG(CAPNAME("XT", "XT")) > 0) {  \
	i_am_xterm = TRUE; \
    } else { \
	i_am_xterm = FALSE; \
    }

/* suppress external when using this in configure script */
#if defined(CHECK_PROTOTYPES) && CHECK_PROTOTYPES
#define vl_strncpy(d,s,l) strncpy(d,s,l)
#endif

#if DISP_TERMCAP

#ifdef MISSING_EXTERN_TGOTO
extern	char *	tgoto (const char *cstring, int hpos, int vpos);
#endif
#ifdef MISSING_EXTERN_TPARAM
extern	char *	tparam (char *cstring, char *buf, int size, ...);
#endif
#ifdef MISSING_EXTERN_TPARM
extern	char *	tparm (const char *fmt, ...);
#endif
#ifdef MISSING_EXTERN_TPUTS
extern	int	tputs (char *string, int nlines, OUTC_DCL (*_f)(OUTC_ARGS) );
#endif

/*****************************************************************************/

#if USE_TERMCAP

#ifdef MISSING_EXTERN_TGETNUM
extern	int	tgetnum (char *name);
#endif

static int
vl_tgetnum(const char *name)
{
    char temp[10];
    return tgetnum(vl_strncpy(temp, name, sizeof(temp)));
}
#define TGETNUM(name) vl_tgetnum(name)

#else	/* !USE_TERMCAP */

#ifdef MISSING_EXTERN_TIGETNUM
extern	int	tigetnum (char *name);
#endif

static int
vl_tgetnum(const char *name)
{
    char temp[10];
    return tigetnum(vl_strncpy(temp, name, sizeof(temp)));
}
#define TGETNUM(name) vl_tgetnum(name)

#endif	/* USE_TERMCAP */

#endif /* DISP_TCAP */

/*****************************************************************************/

#if USE_TERMCAP

#ifdef MISSING_EXTERN_TGETENT
extern	int	tgetent (char *buffer, char *termtype);
#endif
#ifdef MISSING_EXTERN_TGETFLAG
extern	int	tgetflag (char *name);
#endif
#ifdef MISSING_EXTERN_TGETSTR
extern	char *	tgetstr (const char *name, char **area);
#endif

static int
vl_tgetflag(const char *name)
{
    char temp[10];
    return tgetflag(vl_strncpy(temp, name, sizeof(temp)));
}
#define TGETFLAG(name) vl_tgetflag(name)

static char *
vl_tgetstr(const char *name, char **area)
{
    char temp[10];
    return tgetstr(vl_strncpy(temp, name, sizeof(temp)), area);
}
#define TGETSTR(name, bufp) vl_tgetstr(name, bufp)

#else	/* !USE_TERMCAP */

#ifdef MISSING_EXTERN_TIGETFLAG
extern	int	tigetflag (char *name);
#endif
#ifdef MISSING_EXTERN_TIGETSTR
extern	char *	tigetstr (const char *name);
#endif

static int
vl_tgetflag(const char *name)
{
    char temp[10];
    return tigetflag(vl_strncpy(temp, name, sizeof(temp)));
}
#define TGETFLAG(name) vl_tgetflag(name)

static char *
vl_tgetstr(const char *name)
{
    char temp[10];
    return tigetstr(vl_strncpy(temp, name, sizeof(temp)));
}
#define TGETSTR(name, bufp) vl_tgetstr(name)

#endif	/* USE_TERMCAP */

#ifdef __cplusplus
}
#endif

#endif /* VILE_TCAP_H */
