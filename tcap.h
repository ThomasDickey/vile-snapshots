/*
 * Configurable headers used by termcap/terminfo driver for vile.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/tcap.h,v 1.8 2002/10/09 19:08:45 tom Exp $
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

#ifdef NEED_CURSES_H
#  ifdef HAVE_NCURSES_NCURSES_H
#    include <ncurses/ncurses.h>
#  else
#    ifdef HAVE_NCURSES_H
#      include <ncurses.h>
#    else
#      include <curses.h>
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
#  define TGETSTR(name, bufp) tigetstr(name)
#  define TGETNUM(name)       tigetnum(name) /* may be tigetint() */
#  define TGETFLAG(name)      tigetflag(name)
#  define CAPNAME(a,b)        b
#  define NO_CAP(s)           (s == 0 || s == (char *)-1)
#  if !defined(HAVE_TIGETNUM) && defined(HAVE_TIGETINT)
#    define tigetnum tigetint
#  endif
#else /* USE_TERMCAP */
#  define USE_TERMCAP 1
#  define TGETSTR(name, bufp) tgetstr(name, bufp)
#  define TGETNUM(name)       tgetnum(name)
#  define TGETFLAG(name)      tgetflag(name)
#  define CAPNAME(a,b)        a
#  define NO_CAP(s)           (s == 0)
#endif /* USE_TERMINFO */

#ifdef HAVE_EXTERN_TCAP_PC
extern char PC;			/* used in 'tputs()' */
#endif

#ifdef MISSING_EXTERN_TGETENT
extern	int	tgetent (char *buffer, char *termtype);
#endif
#if defined(MISSING_EXTERN_TGETFLAG) || defined(MISSING_EXTERN_TIGETFLAG)
extern	int	TGETFLAG (char *name);
#endif
#if defined(MISSING_EXTERN_TGETNUM) || defined(MISSING_EXTERN_TIGETNUM)
extern	int	TGETNUM (char *name);
#endif
#if defined(MISSING_EXTERN_TGETSTR) || defined(MISSING_EXTERN_TIGETSTR)
extern	char *	TGETSTR(const char *name, char **area);
#endif
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

#ifdef __cplusplus
}
#endif

#endif /* VILE_TCAP_H */
