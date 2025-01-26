/*	ESTRUCT:	Structure and preprocessor defines for
			vile.  Reshaped from the original, which
			was for microemacs.

			vile is by Paul Fox, Tom Dickey, Kevin Buettner,
			Rick Sladkey and many others (see the CHANGES*
			files for details).

			MicroEmacs was written by Dave G. Conroy
			modified by Steve Wilhite, George Jones,
			greatly modified by Daniel Lawrence.
*/

/*
 * $Id: estruct.h,v 1.765 2025/01/26 14:31:19 tom Exp $
 */

#ifndef _estruct_h
#define _estruct_h 1
/* *INDENT-OFF* */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*
 * This is used in the configure script:
 */
#ifndef CHECK_PROTOTYPES
#define CHECK_PROTOTYPES 0
#endif

/* ====================================================================== */

#ifndef os_chosen

/* All variants of UNIX should now be handled by the configure script */

#ifdef VMS		/* predefined by VAX/VMS compiler */
# define scrn_chosen
# define DISP_VMSVT  1
#endif

#ifdef __TURBOC__	/* predefined in Turbo C, both DOS and Windows */
# ifdef __FLAT__	/* predefined in Borland C for WIN32 */
#  define SYS_WINNT  1
# else
#  define SYS_MSDOS  1
# endif
# define CC_TURBO  1
#endif

#ifdef _WIN32		/* predefined in Visual C/C++ 4.0 */
# define SYS_WINNT 1
#endif

#ifdef __WATCOMC__
#define SYS_MSDOS  1
#define CC_WATCOM 1
#endif

#ifdef __IBMC__
# if __IBMC__ >= 200	/* IBM C Set ++ version 2.x */
#  define CC_CSETPP 1
# endif
#endif

#ifdef __OS2__
/* assume compiler already chosen */
#define SYS_OS2    1
#endif

#ifdef __GO32__		/* DJ's GCC version 1.09 */
#define SYS_MSDOS  1
#define CC_DJGPP   1
#endif

#ifdef __LCC__
#define SYS_WINNT	1
#define CC_LCC_WIN32	1
#endif

#ifdef _MSC_VER
#define SYS_WINNT	1
#define CC_MSVC		1
#endif

#endif /* os_chosen */

#ifdef sun
# define SYS_SUNOS 1		/* FIXME: need to tweak lint ifdefs */
#endif

#if defined(__CYGWIN__) || defined(__CYGWIN32__)
# define SYS_CYGWIN 1
#endif

#ifdef __MINGW32__
#define SYS_MINGW 1
#endif

#ifdef __EMX__
# define SYS_OS2_EMX 1		/* makefile.emx, or configure-script */
#endif

#if defined(VMS) || defined(__VMS) /* predefined by DEC's VMS compilers */
# define SYS_VMS    1
#endif

/* ====================================================================== */

/* Some code uses these as values in expressions, so we always want them
 * defined, just in case we run into a substandard preprocessor.
 */
#ifndef CC_CSETPP
#define CC_CSETPP		0	/* IBM C Set ++ 2.x */
#endif

#ifndef CC_DJGPP
#define CC_DJGPP		0	/* DJ's GCC version 1.09 */
#endif

#ifndef CC_LCC_WIN32
#define CC_LCC_WIN32		0	/* Lcc-win32 */
#endif

#ifndef CC_MSC
#define CC_MSC			0	/* Microsoft C versions 3 & 4 & 5 & 6 */
#endif

#ifndef CC_MSVC
#define CC_MSVC			0	/* Microsoft Visual C++ 7 & 8 & 9 */
#endif

#ifndef CC_TURBO
#define CC_TURBO		0	/* Turbo C or Borland C++ */
#endif

#ifndef CC_WATCOM
#define CC_WATCOM		0	/* WATCOM C/386 version 9.0 or above */
#endif

#ifndef SYS_CYGWIN
#define SYS_CYGWIN		0	/* Unix'ed Win32		*/
#endif

#ifndef SYS_MINGW
#define SYS_MINGW		0	/* MINGW32			*/
#endif

#ifndef SYS_MSDOS
#define SYS_MSDOS		0	/* MS-DOS			*/
#endif

#ifndef SYS_OS2
#define SYS_OS2			0	/* son of DOS			*/
#endif

#ifndef SYS_OS2_EMX
#define SYS_OS2_EMX		0	/* Unix'ed OS2			*/
#endif

#ifndef SYS_SUNOS
#define SYS_SUNOS		0	/* SunOS 4.x			*/
#endif

#ifndef SYS_UNIX
#define SYS_UNIX		0	/* Unix & friends		*/
#endif

#ifndef SYS_VMS
#define SYS_VMS			0	/* OpenVMS			*/
#endif

#ifndef SYS_WINNT
#define SYS_WINNT		0	/* Windows/NT			*/
#endif

/* As of version 3.51 of vile, CC_NEWDOSCC should be correct for Turbo,
 * Watcom, and the DJ gcc (GO32) compilers.  I'm betting that it's also
 * probably correct for MSC (Microsoft C) and ZTC (Zortech), but I'm not
 * sure of those.  (It implies a lot of ANSI and POSIX behavior.)
 */
#if CC_TURBO || CC_WATCOM || CC_MSC || CC_DJGPP || SYS_WINNT || CC_CSETPP || CC_MSVC || CC_LCC_WIN32 || SYS_OS2_EMX
# define CC_NEWDOSCC 1
#endif

#ifndef CC_NEWDOSCC
#define CC_NEWDOSCC		0
#endif

/* ====================================================================== */

#ifndef HAVE_CONFIG_H		/* we did not run the configure script */

#if CC_NEWDOSCC
# define HAVE_GETCWD		1
# define HAVE_LIMITS_H		1
#endif

#if CC_CSETPP
# define HAVE_UTIME		1
# define HAVE_STRERROR		1
# define HAVE_SYS_UTIME_H	1
# define CPP_SUBS_BEFORE_QUOTE	1
#endif

#if CC_DJGPP
# define HAVE_UNISTD_H		1
#endif

#if SYS_WINNT && !CC_TURBO
# define HAVE_SYS_UTIME_H	1
# define HAVE_UTIME		1
#endif

#if SYS_WINNT
# define HAVE_ENVIRON		1
# define HAVE_PUTENV		1
# define HAVE_STRICMP		1
# define HAVE_STRINCMP		1
#endif

/*
 * Porting constraints: supply the normally assumed values that we get from
 * the "configure" script, for systems on which we cannot run "configure"
 * (e.g., VMS, OS2, MSDOS).
 */
#ifndef HAVE_ACCESS
# define HAVE_ACCESS    1	/* if your system has the access() system call */
#endif

#ifndef HAVE_MKDIR
# define HAVE_MKDIR	1	/* if your system has the mkdir() system call */
#endif

#ifndef HAVE_SETJMP_H
# define HAVE_SETJMP_H  1	/* if your system has <setjmp.h> */
#endif

#ifndef HAVE_SIGNAL_H
# define HAVE_SIGNAL_H  1	/* if your system has <signal.h> */
#endif

#ifndef HAVE_STDLIB_H
# define HAVE_STDLIB_H  1	/* if your system has <stdlib.h> */
#endif

#if !(defined(HAVE_STRCHR) || defined(HAVE_STRRCHR))
# define HAVE_STRCHR    1
# define HAVE_STRRCHR   1
#endif

#ifndef HAVE_STRFTIME
# define HAVE_STRFTIME	1	/* if your system has the strftime() function */
#endif

#ifndef HAVE_STRTOUL
# define HAVE_STRTOUL	1	/* if your system has the strtoul() function */
#endif

#ifndef HAVE_STRING_H
# define HAVE_STRING_H  1	/* if your system has <string.h> */
#endif

#if SYS_VMS
# define HAVE_GETCWD 1
# if defined(__DECC) && !defined(__alpha)
#  undef HAVE_ACCESS		/* 'access()' is reported to not work */
# endif
# if !defined(__DECC)
#  define CC_CANNOT_INIT_UNIONS 1
# endif
# define SIGT void
# define SIGRET
# if DISP_X11 && defined(NEED_X_INCLUDES)
#  define XTSTRINGDEFINES
# endif
#endif

#if SYS_MSDOS || SYS_OS2 || SYS_WINNT
#ifdef OPT_PERL
typedef unsigned short	mode_t;
/* perl's win32.h typedef's mode_t */
#else
#define mode_t int
#endif
#endif

#if SYS_OS2 && !SYS_OS2_EMX
#define inline /* nothing */
#endif

#if SYS_OS2 || SYS_OS2_EMX || SYS_WINNT || SYS_MSDOS
#define MISSING_EXTERN_CRYPT 1
#endif

#endif /* HAVE_CONFIG_H */

#if !(SYS_CYGWIN) && (defined(WIN32) || CC_TURBO || defined(WIN32_LEAN_AND_MEAN))
#include "w32vile.h"
#endif

#include <vl_stdio.h>

#ifdef HAVE_STDNORETURN_H
#undef GCC_NORETURN
#define GCC_NORETURN _Noreturn
#endif

#if SYS_VMS && (! defined(__DECC_VER))
# include <types.h>
# include <stat.h>
#else
# include <sys/types.h>
# include <sys/stat.h>
#endif

#ifdef HAVE_LIBC_H
#include <libc.h>
#endif
#ifdef HAVE_FCNTL_H
#ifndef O_RDONLY	/* prevent multiple inclusion on lame systems */
#include <fcntl.h>	/* 'open()' */
#endif
#endif

#if defined(HAVE_SYS_TIME_H) && (defined(SELECT_WITH_TIME) || !(defined(HAVE_SELECT_H) || defined(HAVE_SYS_SELECT_H)))
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
# include <time.h>
#endif
#else
#include <time.h>
#endif

#ifdef RESOURCE_WITH_WAIT
#include <sys/resource.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>	/* 'wait()' */
#endif

#ifndef SIGT
/* choose between void and int signal handler return type.
  "typedefs?  we don't need no steenking typedefs..." */
# if CC_NEWDOSCC
#  define SIGT void
#  define SIGRET
# else
#  define SIGT int
#  define SIGRET return 0
# endif
#endif /* SIGT */

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifndef ACTUAL_SIG_ARGS
# define ACTUAL_SIG_ARGS signo	/* some systems have more arguments... */
#endif

#if defined(__GNUC__)
# undef  SIG_DFL
# undef  SIG_IGN
# define SIG_DFL	(SIGT (*)(int ACTUAL_SIG_ARGS))0
# define SIG_IGN	(SIGT (*)(int ACTUAL_SIG_ARGS))1
#endif

#ifdef HAVE_SETJMP_H
#include	<setjmp.h>
#endif

/* argument for 'exit()' or '_exit()' */
#if SYS_VMS
#include	<stsdef.h>
#define GOODEXIT	(STS$M_INHIB_MSG | STS$K_SUCCESS)
#define BADEXIT		(STS$M_INHIB_MSG | STS$K_ERROR)
#else
#if defined(EXIT_SUCCESS) && defined(EXIT_FAILURE)
#define GOODEXIT	EXIT_SUCCESS
#define BADEXIT		EXIT_FAILURE
#else
#define GOODEXIT	0
#define BADEXIT		1
#endif
#endif

/* has the select() or poll() call, only used for short sleeps in fmatch() */
#ifdef HAVE_SELECT
#undef HAVE_POLL
#endif

/* ====================================================================== */
#ifndef scrn_chosen
/*	Terminal Output definitions		*/
/* choose ONLY one of the following */
#define DISP_TERMCAP	SYS_UNIX	/* Use TERMCAP			*/
#define DISP_ANSI	0		/* ansi escape sequences	*/
#define	DISP_VMSVT	SYS_VMS		/* various VMS terminal entries	*/
#define	DISP_BORLAND	0		/* Borland console I/O routines */
#define	DISP_X11	0		/* X Window System */
#define DISP_NTCONS	0		/* Windows/NT console		*/
#define DISP_NTWIN	0		/* Windows/NT screen/windows	*/
#define DISP_VIO	SYS_OS2		/* OS/2 VIO (text mode)		*/
#endif

/* Some code uses these as values in expressions, so we always want them
 * defined, just in case we run into a substandard preprocessor.
 */
#ifndef DISP_ANSI
#define DISP_ANSI	0
#endif

#ifndef DISP_BORLAND
#define DISP_BORLAND	0
#endif

#ifndef DISP_CURSES
#define DISP_CURSES	0
#endif

#ifndef DISP_NTCONS
#define DISP_NTCONS	0
#endif

#ifndef DISP_NTWIN
#define DISP_NTWIN	0
#endif

#ifndef DISP_TERMCAP
#define DISP_TERMCAP	0
#endif

#ifndef DISP_VIO
#define DISP_VIO	0
#endif

#ifndef DISP_VMSVT
#define DISP_VMSVT	0
#endif

#ifndef DISP_X11
#define DISP_X11	0
#endif

#ifndef USE_TERMINFO
#define USE_TERMINFO	0
#endif

#ifndef XTOOLKIT
#define XTOOLKIT	0
#endif

/* ====================================================================== */
/*	Configuration options... pick and choose as you wish */

/*	Code size options	*/
#ifndef FEWNAMES
#define	FEWNAMES 0	/* strip some names - will no longer be bindable */
#endif

#ifndef SMALLER
#define	SMALLER	0	/* strip some fluff -- not a lot smaller, but some */
#endif

#ifndef OPT_DEBUG
#define OPT_DEBUG 0	/* normally set by configure-script */
#endif

#ifndef OPT_FILTER
#define OPT_FILTER 0	/* normally set by configure-script */
#endif

#ifndef OPT_ICONV_FUNCS
#define OPT_ICONV_FUNCS 0 /* normally set by configure-script */
#endif

#ifndef OPT_LOCALE
#define OPT_LOCALE 0	/* normally set by configure-script */
#endif

#ifndef OPT_PERL
#define OPT_PERL 0	/* normally set by configure-script */
#endif

#ifndef OPT_PLUGIN
#define OPT_PLUGIN 0	/* normally set by configure-script */
#endif

#ifndef OPT_SHELL
#define OPT_SHELL 1	/* we'll disable this only as an exercise */
#endif

#ifndef OPT_TCL
#define OPT_TCL 0	/* normally set by configure-script */
#endif

/*
 * Widgets for xvile
 */
#ifndef ATHENA_WIDGETS
#define ATHENA_WIDGETS 0
#endif

#ifndef MOTIF_WIDGETS
#define MOTIF_WIDGETS 0
#endif

#ifndef NO_WIDGETS
#define NO_WIDGETS 0
#endif

/*
 * Constants for ifdef'ing out chunks of code
 */
#define VILE_MAYBE 0
#define VILE_NEVER 0
#define VILE_NEEDED 0
#define VILE_SOMEDAY 0

/* various terminal stuff */
#define IBM_VIDEO	(SYS_MSDOS || SYS_OS2 || SYS_WINNT)
#define CRLF_LINES	(SYS_MSDOS || SYS_OS2 || SYS_WINNT)

/* various color stuff */
#define	OPT_COLOR (DISP_ANSI || IBM_VIDEO || DISP_TERMCAP || DISP_CURSES || DISP_X11)
#define	OPT_16_COLOR (IBM_VIDEO || DISP_TERMCAP || DISP_CURSES || DISP_X11)

#define OPT_DUMBTERM (DISP_TERMCAP || DISP_CURSES || DISP_VMSVT || DISP_X11)

/* Feature turnon/turnoff */
#define OPT_CACHE_VCOL  !SMALLER /* cache mk_to_vcol() starting point          */
#define	OPT_DOSFILES	1	/* turn on code for DOS mode (lines that
				   end in crlf).  use DOSFILES, for instance,
				   if you edit DOS-created files under UNIX   */
#define	OPT_REVSTA	1	/* Status line appears in reverse video       */
#define	OPT_CFENCE	1	/* do fence matching in CMODE		      */
#define OPT_LCKFILES	0	/* create lock files (file.lck style)	      */
#define OPT_TAGS	1	/* tags support				      */
#define	OPT_PROCEDURES	1	/* macro language procedures		      */
#define	OPT_PATHLOOKUP	1	/* search $PATH for startup and help files    */
#define	OPT_SCROLLCODE	1	/* code in display.c for scrolling the screen.
				   Only useful if your display can scroll
				   regions, or at least insert/delete lines.
				   ANSI, TERMCAP/CURSES and VMSVT can do this */
#define OPT_CVMVAS	1	/* arguments to forward/back page and half page
				   are in pages	instead of rows (in vi,
				   they're rows, and the argument is "sticky",
				   i.e. it's remembered */
#define OPT_PRETTIER_SCROLL 0	/* can improve the appearance of a scrolling
				   screen, but it will in general be slower */
#define OPT_STUTTER_SEC_CMD 0	/* must the next/prev section commands (i.e.
				   ']]' and '[[' be stuttered?  they must be
				   stuttered in real vi, I prefer them not
				   to be */
#define OPT_VILE_ALLOC	1	/* use vile's malloc-macros */
#define OPT_VILE_CTYPE	1	/* use vile's character-testing macros */
#define OPT_VILE_REGEX	1	/* use vile's regular expressions */
#define OPT_W32PIPES    SYS_WINNT /* Win32 pipes */
#define WINMARK		0	/* experimental */

/*
 * use an insertion cursor if possible
 *
 * NOTE:  OPT_ICURSOR is _only_ supported by borland.c for a PC build and
 * for either win32 flavor.
 */
#if SYS_WINNT
# define OPT_ICURSOR    1
#else
# define OPT_ICURSOR    0
#endif

#if SYS_WINNT
# define MAX_RECENT_FILES 20
# define MAX_RECENT_FLDRS 20
#endif

/*
 * The $findpath statevar and find-cfg mode features require:
 *
 * - access to an OS that can handle potentially long command lines,
 *   in some cases transmitted via a pipe.
 *
 * - access to the unix find, xargs, and egrep commands.
 *
 * These restrictions make a port to DOS or VMS problematic.
 */
#if (SYS_WINNT || SYS_UNIX) && defined(OPT_SHELL)
# define OPT_FINDPATH   OPT_SHELL&&!SMALLER
#else
# define OPT_FINDPATH   0
#endif

#ifndef OPT_EXEC_MACROS		/* total numbered macros (see mktbls.c) */
#if SMALLER
#define OPT_EXEC_MACROS 10
#else
#define OPT_EXEC_MACROS 40
#endif
#endif

/* allow shift/ctrl/alt mods */
#define OPT_KEY_MODIFY	(SYS_WINNT | DISP_X11 | DISP_TERMCAP | DISP_CURSES)

/* the "working..." message -- we must have the alarm() syscall, and
   system calls must be restartable after an interrupt by default or be
   made restartable with sigaction() */
#if !SMALLER && defined(HAVE_ALARM) && defined(HAVE_RESTARTABLE_PIPEREAD)
#define OPT_WORKING 1
#else
#define OPT_WORKING 0
#endif

#define OPT_SCROLLBARS (XTOOLKIT | DISP_NTWIN)	/* scrollbars */

#ifndef OPT_VMS_PATH
#define OPT_VMS_PATH    (SYS_VMS)  /* vax/vms path parsing (testing/porting)*/
#endif

#if OPT_VMS_PATH
#undef SYSTEM_NAME
#define SYSTEM_NAME	"vms"	/* fakevms pretends to scripts it's vms */
#endif

/* systems with MSDOS-like filename syntax */
#define OPT_MSDOS_PATH  (SYS_MSDOS || SYS_OS2 || SYS_WINNT || SYS_OS2_EMX)
#define OPT_UNC_PATH	(SYS_WINNT || SYS_CYGWIN)

/* individual features that are (normally) controlled by SMALLER */
#define OPT_AUTOCOLOR	(!SMALLER && OPT_COLOR)	/* autocolor support */
#define OPT_BNAME_CMPL  !SMALLER		/* name-completion for buffers */
#define OPT_B_LIMITS    !SMALLER		/* left-margin */
#define OPT_CURTOKENS   !SMALLER		/* cursor-tokens mode */
#define OPT_ENUM_MODES  !SMALLER		/* fixed-string modes */
#define OPT_EVAL        !SMALLER		/* expression-evaluation */
#define OPT_EXTRA_COLOR (!SMALLER && OPT_COLOR)	/* modeline, etc */
#define OPT_FILEBACK    (!SMALLER && !SYS_VMS)	/* file backup style */
#define OPT_FINDERR     !SMALLER		/* finderr support. */
#define OPT_FLASH       !SMALLER		/* visible-bell */
#define OPT_FORMAT      !SMALLER		/* region formatting support. */
#define OPT_HILITEMATCH !SMALLER		/* highlight all matches of a search */
#define OPT_HISTORY     !SMALLER		/* command-history */
#define OPT_HOOKS	!SMALLER		/* read/write hooks, etc. */
#define OPT_ISO_8859    !SMALLER		/* ISO 8859 characters */
#define OPT_ISRCH       !SMALLER		/* Incremental searches */
#define OPT_KEEP_POS    !SMALLER		/* keep-position mode */
#define OPT_LINEWRAP    !SMALLER		/* line-wrap mode */
#define OPT_MAJORMODE   !SMALLER		/* majormode support */
#define OPT_MACRO_ARGS	(!SMALLER && OPT_EVAL)	/* macro argument parsing */
#define OPT_MLFORMAT    !SMALLER		/* modeline-format */
#define OPT_MODELINE    !SMALLER		/* vi-style modeline-support */
#define OPT_MULTIBYTE   (!SMALLER && OPT_LOCALE) /* multibyte support */
#define OPT_NAMEBST     !SMALLER		/* name's stored in a bst */
#define OPT_ONLINEHELP  !SMALLER		/* short per-command help */
#define OPT_POPUPCHOICE !SMALLER		/* popup-choices mode */
#define OPT_POPUPPOSITIONS !SMALLER		/* popup-positions mode */
#define OPT_POPUP_MSGS  !SMALLER		/* popup-msgs mode */
#define OPT_POSFORMAT   !SMALLER		/* position-format */
#define OPT_REBIND      !SMALLER		/* permit rebinding of keys at run-time	*/
#define OPT_REGS_CMPL   !SMALLER		/* name-completion for registers */
#define OPT_SHOW_WHICH	!SMALLER		/* which-source, etc. */
#define OPT_TAGS_CMPL   (!SMALLER && OPT_TAGS)	/* name-completion for tags */
#define OPT_TERMCHRS    !SMALLER		/* set/show-terminal */
#define OPT_UPBUFF      !SMALLER		/* animated buffer-update */
#define OPT_WIDE_CTYPES !SMALLER		/* extra char-types tests */
#define OPT_WORDCOUNT   !SMALLER		/* "count-words" command" */

/* "show" commands for the optional features */
#define OPT_SHOW_COLORS	(!SMALLER && OPT_COLOR)	/* "show-colors" */
#define OPT_SHOW_CTYPE	!SMALLER		/* "show-printable" */
#define OPT_SHOW_EVAL   (!SMALLER && OPT_EVAL)	/* "show-variables" */
#define OPT_SHOW_MAPS   !SMALLER		/* display mapping for ":map" */
#define OPT_SHOW_MARKS  !SMALLER		/* "show-marks" */
#define OPT_SHOW_REGS   !SMALLER		/* "show-registers" */
#define OPT_SHOW_TAGS   (!SMALLER && OPT_TAGS)	/* ":tags" displays tag-stack */

/* selections and attributed regions */
#define OPT_VIDEO_ATTRS (!SMALLER || XTOOLKIT)
#define OPT_SELECTIONS  OPT_VIDEO_ATTRS
#define OPT_HYPERTEXT	OPT_VIDEO_ATTRS
#define OPT_LINE_ATTRS	OPT_VIDEO_ATTRS

/* OPT_PSCREEN permits a direct interface to the pscreen data structure
 * in display.c. This allows us to avoid storing the screen data on the
 * screen interface side.
 */
#define OPT_PSCREEN  (XTOOLKIT)

#if	(DISP_TERMCAP || DISP_CURSES) && !SMALLER
/* the setting "xterm-mouse" is always available, i.e.  the modetbl entry
 * is not conditional.  but all of the code that supports xterm-mouse _is_
 * ifdefed.  this makes it easier for users to be able to put "set
 * xterm-mouse" in their .vilerc which is shared between vile and xvile.
 */
#define	OPT_XTERM	2	/* mouse-clicking support */
#else
#define	OPT_XTERM	0	/* vile doesn't recognize xterm mouse */
#endif

	/* implement window title */
#define OPT_TITLE	(SYS_WINNT | DISP_X11 | OPT_XTERM)

	/* combine select/yank (for mouse support) */
#define OPT_SEL_YANK    ((DISP_X11 && XTOOLKIT) || SYS_WINNT || SYS_OS2)

	/* any mouse capability */
#define OPT_MOUSE       (OPT_SEL_YANK || OPT_XTERM)

	/* menus */
#define	OPT_MENUS	(!SMALLER && DISP_X11 && (MOTIF_WIDGETS||ATHENA_WIDGETS))
#ifndef OPT_MENUS_COLORED
#define OPT_MENUS_COLORED 0
#endif

	/* icons */
#define OPT_X11_ICON	DISP_X11 /* use compiled-in X icon */

/*
 * If selections will be used to communicate between vile and other
 * applications, OWN_SELECTION must be defined to call the procedure
 * for establishing ownership of the selection.
 */
#if OPT_SELECTIONS && XTOOLKIT /* or others? */
#define OWN_SELECTION() own_selection()
#else
#define OWN_SELECTION()	/*EMPTY*/
#endif

/*
 * Symbols that turn on tables related to OPT_ENUM_MODES in nefsms.h
 */
#define OPT_COLOR_SCHEMES          (OPT_ENUM_MODES && !SMALLER && OPT_COLOR)

#define OPT_ACCESS_CHOICES         !SMALLER
#define OPT_BACKUP_CHOICES         (OPT_ENUM_MODES && OPT_FILEBACK)
#define OPT_BOOL_CHOICES           !SMALLER
#define OPT_BYTEORDER_MARK_CHOICES OPT_MULTIBYTE
#define OPT_CHARCLASS_CHOICES      OPT_SHOW_CTYPE
#define OPT_CMD_ENCODING_CHOICES   OPT_MULTIBYTE
#define OPT_COLOR_CHOICES          (OPT_ENUM_MODES && OPT_COLOR)
#define OPT_CURTOKENS_CHOICES      OPT_CURTOKENS
#define OPT_DIRECTIVE_CHOICES      !SMALLER
#define OPT_ENCRYPT                !SMALLER
#define OPT_FILE_ENCODING_CHOICES  OPT_MULTIBYTE
#define OPT_FORBUFFERS_CHOICES     !SMALLER
#define OPT_HILITE_CHOICES         (OPT_ENUM_MODES && OPT_HILITEMATCH)
#define OPT_KBD_ENCODING_CHOICES   OPT_MULTIBYTE
#define OPT_KEEP_POS_CHOICES       !SMALLER
#define OPT_LOOKUP_CHOICES         !SMALLER
#define OPT_MMQUALIFIERS_CHOICES   OPT_MAJORMODE
#define OPT_PARAMTYPES_CHOICES     OPT_MACRO_ARGS
#define OPT_PATH_CHOICES           !SMALLER
#define OPT_POPUP_CHOICES          (OPT_ENUM_MODES && OPT_POPUPCHOICE)
#define OPT_POPUPPOSITIONS_CHOICES (OPT_ENUM_MODES && OPT_POPUPPOSITIONS)
#define OPT_READERPOLICY_CHOICES   !SMALLER
#define OPT_RECORDATTRS_CHOICES    (OPT_ENUM_MODES && SYS_VMS)
#define OPT_RECORDFORMAT_CHOICES   (OPT_ENUM_MODES && SYS_VMS)
#define OPT_RECORDSEP_CHOICES      !SMALLER
#define OPT_SHOWFORMAT_CHOICES     !SMALLER
#define OPT_TITLE_ENCODING_CHOICES OPT_MULTIBYTE
#define OPT_VIDEOATTRS_CHOICES     (OPT_ENUM_MODES && OPT_COLOR_SCHEMES)
#define OPT_VTFLASHSEQ_CHOICES     (OPT_ENUM_MODES && VTFLASH_HOST && OPT_FLASH)
#define OPT_XCOLORS_CHOICES        (OPT_ENUM_MODES && OPT_EXTRA_COLOR)

/*
 * Special characters used in globbing
 */
#define	GLOB_MULTI	'*'
#define	GLOB_SINGLE	'?'
#define	GLOB_ELLIPSIS	"..."	/* implemented on VMS-only */
#define	GLOB_RANGE	"[]"
#define	GLOB_ENVIRON	"$"	/* unimplemented */
#define	GLOB_NEGATE	"^!"

/*
 * Enumeration used for selecting globbing features
 */
typedef enum {
	vl_GLOB_MULTI      = 1
	, vl_GLOB_SINGLE   = 2
	, vl_GLOB_ELLIPSIS = 4
	, vl_GLOB_RANGE    = 8
	, vl_GLOB_ENVIRON  = 16
} vl_GLOB_OPTS;

#define vl_GLOB_MIN (vl_GLOB_MULTI | vl_GLOB_SINGLE)
#define vl_GLOB_ALL (vl_GLOB_MIN | vl_GLOB_ELLIPSIS | vl_GLOB_RANGE | vl_GLOB_ENVIRON)

/*
 * Configuration options for globbing
 */
#define	OPT_GLOB_ENVIRON	!SMALLER
#define	OPT_GLOB_ELLIPSIS	SYS_VMS || SYS_UNIX || SYS_OS2 || SYS_WINNT || (SYS_MSDOS && !SMALLER)
#define	OPT_GLOB_PIPE		SYS_UNIX && OPT_SHELL
#define	OPT_GLOB_RANGE		SYS_UNIX || SYS_OS2 || SYS_WINNT || (SYS_MSDOS && !SMALLER)

/*	Debugging options	*/
#define	OPT_HEAPSIZE		0  /* track heap usage */
#define OPT_DEBUGMACROS		0  /* let $debug control macro tracing */

#ifndef OPT_ELAPSED
#define OPT_ELAPSED		0  /* turn on timing of traced calls */
#endif

#ifndef OPT_TRACE
#define OPT_TRACE		0  /* turn on debug/trace (link with trace.o) */
#endif

#ifndef CAN_TRACE
#define CAN_TRACE		OPT_TRACE  /* (link with trace.o) */
#endif

#ifndef CAN_VMS_PATH
#define CAN_VMS_PATH		1  /* (link with fakevms.o) */
#endif

/* That's the end of the user selections -- the rest is static definition */
/* (i.e. you shouldn't need to touch anything below here */
/* ====================================================================== */

#include <errno.h>

#if SYS_VMS
#include <perror.h>		/* defines 'sys_errlist[]' */
#endif

#if SYS_UNIX
# ifdef DECL_ERRNO
extern	int	errno;		/* some systems don't define this in <errno.h> */
# endif
# ifdef DECL_SYS_NERR
extern	int	sys_nerr;
# endif
# ifdef DECL_SYS_ERRLIST
extern	char *	sys_errlist[];
# endif
#endif
#define	set_errno(code)	errno = code

	/* bit-mask definitions */
#define	lBIT(n)	((ULONG)(1UL<<(n)))
#define	iBIT(n) ((UINT)(1U <<(n)))

#define clr_typed_flags(dst,type,flags)  (dst) = (type) ((dst) & ~(flags))
#define clr_flags(dst,flags)             (dst) = ((dst) & ~(flags))

#if !(defined(HAVE_STRCHR) && defined(HAVE_STRRCHR))
#define USE_INDEX 1
#endif

#ifdef USE_INDEX
#define strchr index
#define strrchr rindex
#ifdef MISSING_EXTERN_RINDEX
extern char *index (const char *s, int c);
extern char *rindex (const char *s, int c);
#endif
#endif /* USE_INDEX */

#define vl_index (strchr)

#if defined(STDC_HEADERS) || defined(HAVE_STRING_H)
# include <string.h>
  /* An ANSI string.h and pre-ANSI memory.h might conflict.  */
# if !defined(STDC_HEADERS) && defined(HAVE_MEMORY_H)
#  include <memory.h>
# endif /* not STDC_HEADERS and HAVE_MEMORY_H */
#else /* not STDC_HEADERS and not HAVE_STRING_H */
# ifdef HAVE_STRINGS_H
#  include <strings.h>
  /* memory.h and strings.h conflict on some systems */
  /* FIXME: should probably define memcpy and company in terms of bcopy,
     et al here */
# endif
#endif /* not STDC_HEADERS and not HAVE_STRING_H */

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

/*	System dependent library redefinitions, structures and includes	*/

#if CC_CSETPP
#elif CC_LCC_WIN32
#include <direct.h>		/* chdir */
#define cwait(i,j,k) _cwait(i,j,k)
#define utime(i,j)   _utime(i,j)
#define SP_NOTREPORTED 0x4000	/* wingdi.h */
#define WHEEL_DELTA 120		/* winuser.h */
#elif CC_NEWDOSCC && ! CC_CSETPP
#include <dos.h>
# define HAVE_STRERROR		1
#endif

#if  CC_WATCOM
#include      <string.h>
#endif

#if CC_MSC
#include <memory.h>
#endif


/* on MS-DOS we have to open files in binary mode to see the ^Z characters. */

#if SYS_MSDOS || SYS_OS2 || SYS_WINNT || SYS_CYGWIN || SYS_OS2_EMX
#define FOPEN_READ	"rb"
#define FOPEN_WRITE	"wb"
#define FOPEN_APPEND	"ab"
#define FOPEN_UPDATE	"w+b"
#else
#define FOPEN_READ	"r"
#define FOPEN_WRITE	"w"
#define FOPEN_APPEND	"a"
#define FOPEN_UPDATE	"w+"
#endif

#if SYS_VMS
#undef  FOPEN_READ
#define FOPEN_READ "r", "shr=get,upd"
#define RECORD_ATTRS(ftn,cr,prn,blk) \
			(ftn<<FAB$V_FTN) | \
			(cr <<FAB$V_CR)  | \
			(prn<<FAB$V_PRN) | \
			(blk<<FAB$V_BLK)
#endif

#if OPT_MSDOS_PATH && !SYS_OS2_EMX	/* DOS path / to \ conversions */
# define is_slashc(c) (c == '\\' || c == '/')
# define SL_TO_BSL(s)	sl_to_bsl(s)
#else
# define SL_TO_BSL(s)	(s)
# define bsl_to_sl_inplace(s)
#endif


#ifndef is_slashc
# define is_slashc(c) (c == '/')
#endif

#define SLASHC '/'	/* UNIX, MSDOS-compatibility, VMS-shell */


#if SYS_VMS
#define	unlink(a)	delete(a)
#define tempnam		vile_tempnam
#endif

#if SYS_OS2 && CC_WATCOM
#define unlink(a)	remove(a)
#endif

/*
 * Definitions for hardwired VT-compatible flash esc sequences.  These
 * defns must precede inclusion of nemode.h
 */
#define VTFLASH_HOST    (SYS_UNIX||SYS_VMS)
#if OPT_FLASH
#define VTFLASH_OFF     0
#define VTFLASH_REVERSE 1
#define VTFLASH_NORMAL  2
#define VTFLASH_REVSEQ  "\033[?5h"
#define VTFLASH_NORMSEQ "\033[?5l"
#endif


	/* intermediate config-controls for filec.c (needed in nemode.h) */
#if !SMALLER
#define COMPLETE_FILES  (SYS_UNIX || SYS_MSDOS || SYS_VMS || SYS_OS2 || SYS_WINNT)
#define	COMPLETE_DIRS   (SYS_UNIX || SYS_MSDOS || SYS_VMS || SYS_OS2 || SYS_WINNT)
#else
#define COMPLETE_FILES  0
#define COMPLETE_DIRS   0
#endif

#if DISP_NTWIN
extern int MainProgram(int argc, char *argv[]);
#else
#define MainProgram main
#endif

	/* semaphore may be needed to prevent interrupt of display-code */
#if OPT_WORKING && OPT_TRACE
extern void beginDisplay(void);
extern void endofDisplay(void);
#endif

#if defined(SIGWINCH) || OPT_WORKING
# if OPT_WORKING && (OPT_TRACE > 2)
	/* no macros */
# else
# define beginDisplay() ++im_displaying
# define endofDisplay() if (im_displaying > 0) --im_displaying; else assert(im_displaying > 0)
# endif
#else
# define beginDisplay() /* nothing */
# define endofDisplay() /* nothing */
#endif

#if OPT_WORKING
#define ShowWorking() (!global_b_val(MDTERSE) && global_g_val(GMDWORKING))
#else
#define ShowWorking() (!global_b_val(MDTERSE))
#endif

/* how to signal our process group: pass the 0 to 'getpgrp()' if we can,
 * since it's safer --- the machines where we can't are probably POSIX
 * machines with ANSI C.
 */
#if !defined(GETPGRP_VOID) || defined(MISSING_EXTERN_GETPGRP)
# define GETPGRPCALL getpgrp(0)
#else
# define GETPGRPCALL getpgrp()
#endif

#ifdef HAVE_KILLPG
# define signal_pg(sig) killpg( GETPGRPCALL, sig)
#else
# define signal_pg(sig)   kill(-GETPGRPCALL, sig)
#endif

/* only the raw pc screen driver has a memory-mapped "frame buffer" */
#if SYS_OS2
/*
 * The OS/2 toolkit defines identical typedefs for UCHAR, etc.;
 * we use those definitions to avoid trouble when using OS/2 include
 * files.
 */
#  include <os2def.h>
#elif SYS_CYGWIN || !(defined(WIN32) || defined(WIN32_LEAN_AND_MEAN))
#  define UCHAR  unsigned char
#  define UINT   unsigned int
#  define USHORT unsigned short
#  define ULONG  unsigned long
#endif

/*	internal constants	*/

#if SYS_MSDOS
#define	BITS_PER_INT	16
#endif

#ifndef	BITS_PER_INT
#define	BITS_PER_INT	32
#endif

#ifdef  MAXPATHLEN			/* usually in <sys/param.h>	*/
#define NFILEN	MAXPATHLEN		/* # of bytes, file name	*/
#else
#if SYS_MSDOS && CC_TURBO
#define NFILEN	128			/* # of bytes, file name	*/
#else
#define NFILEN	256			/* # of bytes, file name	*/
#endif
#endif

#define NBUFN	21			/* # of bytes, buffername, incl. null*/
#define NLINE	256			/* # of bytes, input line	*/
#define	NSTRING	144			/* # of bytes, string buffers	*/
#define NPAT	144			/* # of bytes, pattern		*/
#define NKEYLEN	20			/* # of bytes, crypt key	*/
#define VL_HUGE	(1<<(BITS_PER_INT-2))	/* Huge number			*/
#define	NLOCKS	100			/* max # of file locks active	*/
#if OPT_16_COLOR
#define	NCOLORS	16			/* number of supported colors	*/
#else
#define	NCOLORS	8			/* number of supported colors	*/
#endif
#define	KBLOCK	256			/* sizeof kill buffer chunks	*/

#if OPT_SELECTIONS
#define NKREGS	39			/* When selections are enabled, we
					 * allocate an extra kill buffer for
					 * the current selection and another
					 * for the clipboard.
					 */
#define CLIP_KREG  (NKREGS-1)
#define SEL_KREG   (NKREGS-2)
#define KEYST_KREG (NKREGS-3)
#else
#define	NKREGS	37			/* number of kill buffers	*/
#define KEYST_KREG (NKREGS-1)
#endif

#if SMALLER
#define NMARKS  26
#else
#define NMARKS  36
#endif

/* special characters assigned to kill-registers */
#define CLIP_KCHR  ';'
#define SEL_KCHR   '.'
#define KEYST_KCHR '<'
#define UNAME_KCHR '@'

#define	NBLOCK	16			/* line block chunk size	*/
#define MINWLNS	3			/* min # lines, window/screen	*/
#define MAXROWS	200			/* max # lines per screen	*/
#define MAXCOLS	200			/* max # cols per screen	*/

#define C_BLACK 0
#define C_WHITE (ncolors-1)

#define MinCBits   8			/* bits in N_chars		*/

#if OPT_MULTIBYTE && (DISP_TERMCAP || DISP_CURSES || DISP_BORLAND)
#define OPT_VL_OPEN_MBTERM 1		/* uses vl_open_mbterm		*/
#else
#define OPT_VL_OPEN_MBTERM 0
#endif

#if OPT_MULTIBYTE
#define MaxCBits   16			/* allow UTF-16 internal	*/
#else
#define MaxCBits   MinCBits		/* allow 8bit internal		*/
#endif

#define N_chars    (1 << MinCBits)	/* must be a power-of-2		*/
#define HIGHBIT    (1 << 7)		/* the meta bit			*/

#define FoldTo8bits(value) vl_mb_is_8bit(value)

#define COLS_CTRL  2			/* columns for "^X"		*/
#define COLS_8BIT  4			/* columns for "\xXX"		*/
#define COLS_UTF8  6			/* columns for "\uXXXX"		*/

#define MAX_UTF8 8			/* enough for any UTF-8 conversion */

#define CTLA       iBIT(MaxCBits+0)	/* ^A flag, or'ed in		*/
#define CTLX       iBIT(MaxCBits+1)	/* ^X flag, or'ed in		*/
#define SPEC       iBIT(MaxCBits+2)	/* special key (function keys)	*/
#define NOREMAP    iBIT(MaxCBits+3)	/* unremappable */
#define YESREMAP   iBIT(MaxCBits+4)	/* override noremap */
#define REMAPFLAGS (NOREMAP|YESREMAP)

#if OPT_KEY_MODIFY
#define mod_KEY    iBIT(MaxCBits+5)	/* special Win32 keys		*/
#define mod_SHIFT  iBIT(MaxCBits+6)	/* shift was held down		*/
#define mod_CTRL   iBIT(MaxCBits+7)	/* control was held down	*/
#define mod_ALT    iBIT(MaxCBits+8)	/* alt was held down		*/
#define mod_NOMOD  (~(mod_KEY|mod_SHIFT|mod_CTRL|mod_ALT))
#endif

#define kcod2key(c)	(int)((UINT)(c) & (UINT)(iBIT(MaxCBits)-1)) /* strip off the above prefixes */
#define	is8Bits(c)	(((UINT)(c) & (UINT)~0xff) == 0)
#define	isSpecial(c)	(((UINT)(c) & (UINT)~(iBIT(MaxCBits)-1)) != 0)

#define	char2int(c)	((int)kcod2key(c))	/* mask off sign-extension, etc. */

#define	PLURAL(n)	((n) != 1 ? "s" : "")
#define NONNULL(s)	((s) != NULL ? (s) : "")
#define isEmpty(s)	((s) == NULL || *(s) == EOS)

#define EOS        '\0'
#define BQUOTE     '`'
#define SQUOTE     '\''
#define DQUOTE     '"'
#define BACKSLASH  '\\'
#define CH_TILDE   '~'

#define isEscaped(s)	((s)[-1] == BACKSLASH)
#define isTab(c)	((c) == '\t')

#define isErrorVal(s)	((s) == error_val)
#define isLegalVal(s)	((s) != NULL && !isErrorVal(s))
#define isLegalExp(s,x) ((s = (x)) != NULL && !isErrorVal(s))

/* protect against losing namespaces */
#undef	VL_ERROR
#undef	FALSE
#undef	TRUE
#undef	ABORT
#undef	SORTOFTRUE

#define VL_ERROR	-1		/* Error			*/
#define FALSE		0		/* False, no, bad, etc.		*/
#define TRUE		1		/* True, yes, good, etc.	*/
#define ABORT		2		/* Death, ESC, abort, etc.	*/
#define	SORTOFTRUE	3		/* really!	*/

/* keystroke replay states */
#define	PLAY	'P'
#define	RECORD	'R'

#define MAP_QUOTED	TRUE
#define MAP_UNQUOTED	FALSE

#define DOMAP	TRUE
#define NODOMAP	FALSE

/* values for regionshape */
typedef enum {
	rgn_EXACT,
	rgn_FULLLINE,
	rgn_RECTANGLE
} REGIONSHAPE;

#define ENUM_ILLEGAL   (-3)
#define ENUM_FCOLOR    (-2)
#define ENUM_UNKNOWN   (-1)
#define END_CHOICES    { (char *)0, ENUM_ILLEGAL }

typedef struct {
	const char * choice_name;
	int    choice_code;
} FSM_CHOICES;

#include <blist.h>

typedef struct {
	const FSM_CHOICES *choices;
	BLIST blist;
} FSM_BLIST;

typedef struct {
	const char * mode_name;
	FSM_BLIST * lists;
} FSM_TABLE;

typedef enum {
	bom_NONE = 0
	, bom_UTF8
	, bom_LE_ASSUMED
	, bom_BE_ASSUMED
	, bom_UTF16LE
	, bom_UTF16BE
	, bom_UTF32LE
	, bom_UTF32BE
} BOM_CODES;

typedef enum {
	CT_BOTH = 0
	, CT_CCLASS
	, CT_REGEX
} CURTOKENS;

/*
 * Enumerated type for extra-colors.
 */
typedef enum {
	XCOLOR_NONE = 0
	, XCOLOR_ENUM
	, XCOLOR_HYPERTEXT
	, XCOLOR_ISEARCH
	, XCOLOR_LINEBREAK
	, XCOLOR_LINENUMBER
	, XCOLOR_MODELINE
	, XCOLOR_NUMBER
	, XCOLOR_REGEX
	, XCOLOR_STRING
	, XCOLOR_WARNING
	, XCOLOR_MAX		/* last entry, for array-size */
} XCOLOR_CODES;

/*	Directive definitions	*/

typedef	enum {
	D_UNKNOWN = ENUM_UNKNOWN,
	D_ENDM = 0
#if ! SMALLER
	, D_IF
	, D_ELSEIF
	, D_ELSE
	, D_ENDIF
	, D_GOTO
	, D_RETURN
	, D_WHILE
	, D_ENDWHILE
	, D_BREAK
	, D_FORCE
	, D_HIDDEN
	, D_QUIET
	, D_LOCAL
	, D_WITH
	, D_ELSEWITH
	, D_ENDWITH
	, D_TRACE
#endif
} DIRECTIVE;

typedef enum {
	FB_MIXED = 0
	, FB_GLOB
	, FB_REGEX
} FOR_BUFFERS;

typedef enum {
	bak_OFF = 0
	, bak_BAK
	, bak_TILDE
	, bak_TILDE_N0
	, bak_TILDE_N
} BAK_CHOICES;

typedef enum {
	enc_LOCALE = ENUM_UNKNOWN - 1
	, enc_AUTO = ENUM_UNKNOWN
	, enc_POSIX = 0
	, enc_8BIT
#if OPT_MULTIBYTE
	, enc_UTF8
	, enc_UTF16
	, enc_UTF32
#define enc_DEFAULT enc_LOCALE
#else
#define enc_DEFAULT enc_POSIX
#endif
} ENC_CHOICES;

#define b_is_enc_AUTO(bp)    ((ENC_CHOICES) b_val(bp, VAL_FILE_ENCODING) == enc_AUTO)
#define b_is_enc_LOCALE(bp)  ((ENC_CHOICES) b_val(bp, VAL_FILE_ENCODING) == enc_LOCALE)

/*
 * True if the buffer contents are in UTF-8 (or -16, -32).
 */
#if OPT_MULTIBYTE
#define global_is_utfXX()    ((global_b_val(VAL_FILE_ENCODING) >= enc_UTF8) \
			   || (global_b_val(VAL_FILE_ENCODING) == enc_LOCALE \
			    && vl_encoding >= enc_UTF8))
#define b_is_utfXX(bp)       ((b_val(bp, VAL_FILE_ENCODING) >= enc_UTF8) \
			   || (b_val(bp, VAL_FILE_ENCODING) == enc_LOCALE \
			    && vl_encoding >= enc_UTF8))
#else
#define global_is_utfXX()    0
#define b_is_utfXX(bp)       0
#endif

/*
 * Resolve file-encoding and the special symbols "auto" and "locale" to
 * map to one of the "real" file-encoding values.
 */
#define buf_encoding(bp)     ((b_val(bp, VAL_FILE_ENCODING) == enc_LOCALE) \
			      ? vl_encoding \
			      : ((b_val(bp, VAL_FILE_ENCODING) == enc_AUTO) \
			          ? enc_8BIT \
			          : b_val(bp, VAL_FILE_ENCODING)))

typedef enum {
    	KPOS_VILE = 0
	, KPOS_NVI
	, KPOS_VI
} KEEP_POS_CHOICES;

typedef enum {
	MMQ_ANY = 0
	, MMQ_ALL
} MMQ_CHOICES;

typedef enum {
	UNI_MODE = 0
	, BUF_MODE
	, WIN_MODE
#if OPT_MAJORMODE
	, MAJ_MODE
	, SUB_MODE
#endif
	, END_MODE
} MODECLASS;

typedef enum {
	PATH_UNKNOWN = ENUM_UNKNOWN
	, PATH_END
	, PATH_FULL
	, PATH_HEAD
	, PATH_ROOT
	, PATH_SHORT
	, PATH_TAIL
} PATH_CHOICES;

typedef enum {
	PT_UNKNOWN = ENUM_UNKNOWN
	, PT_BOOL
	, PT_BUFFER
	, PT_DIR
	, PT_ENUM
	, PT_FILE
	, PT_INT
	, PT_MODE
	, PT_REG
	, PT_STR
	, PT_VAR
#if OPT_MAJORMODE
	, PT_MAJORMODE
#endif
} PARAM_TYPES;

typedef struct {
	PARAM_TYPES pi_type;
	char *pi_text;		/* prompt, if customized */
#if OPT_ENUM_MODES
	FSM_BLIST *pi_choice;	/* if pi_type==PT_ENUM, points to table */
#endif
} PARAM_INFO;

typedef enum {
	RS_AUTO = -1
	, RS_DEFAULT = 0
	, RS_LF			/* unix */
	, RS_CR			/* mac */
	, RS_CRLF		/* dos */
} RECORD_SEP;

typedef enum {
	RP_QUICK
	, RP_SLOW
	, RP_BOTH
} READERPOLICY_CHOICES;

#if CRLF_LINES
#define RS_SYS_DEFAULT RS_CRLF
#else
#define RS_SYS_DEFAULT RS_LF
#endif

typedef enum {
	SF_NEVER = 0
	, SF_ALWAYS
	, SF_DIFFERS
	, SF_FOREIGN
	, SF_LOCAL
} SHOW_FORMAT;

/* cfg_locate options */
#define FL_EXECABLE  iBIT(0)	/* maps to X_OK */
#define FL_WRITEABLE iBIT(1)	/* maps to W_OK */
#define FL_READABLE  iBIT(2)	/* maps to R_OK */
#define FL_CDIR      iBIT(3)	/* look in current directory */
#define FL_HOME      iBIT(4)	/* look in environment $HOME */
#define FL_EXECDIR   iBIT(5)	/* look in execution directory */
#define FL_STARTPATH iBIT(6)	/* look in environment $VILE_STARTUP_PATH */
#define FL_PATH      iBIT(7)	/* look in environment $PATH */
#define FL_LIBDIR    iBIT(8)	/* look in environment $VILE_LIBDIR_PATH */
#define FL_ALWAYS    iBIT(9)	/* file is not a script, but data */
#define FL_INSECURE  iBIT(10)	/* do not care if insecure */

#define FL_ANYWHERE  (FL_CDIR|FL_HOME|FL_EXECDIR|FL_STARTPATH|FL_PATH|FL_LIBDIR)

/* These are the chief ways we use the cfg_locate options: */

#if SYS_MSDOS || SYS_OS2 || SYS_WINNT
#define LOCATE_SOURCE (FL_READABLE | FL_ANYWHERE)
#else
#define LOCATE_SOURCE (FL_READABLE | FL_CDIR | FL_HOME | FL_STARTPATH)
#endif

#define LOCATE_EXEC   (FL_PATH | FL_LIBDIR | FL_EXECABLE)
#define LOCATE_HELP   (FL_READABLE | FL_ALWAYS | FL_ANYWHERE)
#define LOCATE_TAGS   (FL_READABLE | FL_ALWAYS | FL_CDIR)

/* definitions for name-completion */
#define	NAMEC		name_cmpl /* char for forcing name-completion */
#define	TESTC		test_cmpl /* char for testing name-completion */

/* kbd_string options */
#define KBD_EXPAND	iBIT(0)	/* do we want to expand %, #, : */
#define KBD_QUOTES	iBIT(1)	/* do we add and delete '\' chars for the caller */
#define KBD_LOWERC	iBIT(2)	/* do we force input to lowercase */
#define KBD_UPPERC	iBIT(3)	/* do we force input to uppercase */
#define KBD_NOEVAL	iBIT(4)	/* disable 'tokval()' (e.g., from buffer) */
#define KBD_MAYBEC	iBIT(5)	/* may be completed -- or not */
#define KBD_NULLOK	iBIT(6)	/* may be empty -- or not */
#define KBD_EXPCMD	iBIT(7)	/* expand %, #, : only in shell-command */
#define KBD_SHPIPE	iBIT(8)	/* expand, assuming shell-command */
#define KBD_NOMAP	iBIT(9) /* don't permit mapping via kbd_key() */
#define KBD_EXPPAT	iBIT(10) /* expand ~ to last replacement */
#define KBD_0CHAR	iBIT(11) /* string can have embedded nulls */
#define KBD_STATED	iBIT(12) /* erasing buffer returns for prev-state */
#define KBD_CASELESS	iBIT(13) /* name-completion ignores case */
#define KBD_MAYBEC2	iBIT(14) /* completion optional if user-variable */
#define KBD_REGLUE	iBIT(15) /* glue tokens across calls */

typedef UINT KBD_OPTIONS;	/* big enough to hold bits defined above */

/* default option for 'mlreply' (used in modes.c also) */
#if !(SYS_MSDOS || SYS_OS2 || SYS_WINNT)
#define	KBD_NORMAL	KBD_EXPAND|KBD_QUOTES
#else
#define	KBD_NORMAL	KBD_EXPAND
#endif

/* This was an enum, but did not compile with TurboC */
#define WATCHREAD   iBIT(0)
#define WATCHWRITE  iBIT(1)
#define WATCHEXCEPT iBIT(2)
typedef UINT WATCHTYPE;

/* reserve space for ram-usage option */
#if OPT_HEAPSIZE
#define	LastMsgCol	(term.cols - 10)
#else
#define	LastMsgCol	(term.cols - 1)
#endif

/*
 * directions for the scan routines.
 */
#define	FORWARD	TRUE
#define REVERSE	FALSE

typedef enum {
	/* nonfatal codes */
    FIOBAD = -1			/* File I/O, truncation		*/
    ,FIOSUC			/* File I/O, success.		*/
    ,FIOEOF			/* File I/O, end of file.	*/
	/* error codes */
    ,FIONUL			/* File I/O, empty file		*/
    ,FIOFNF			/* File I/O, file not found.	*/
    ,FIOERR			/* File I/O, error.		*/
    ,FIOMEM			/* File I/O, out of memory	*/
    ,FIOABRT			/* File I/O, aborted		*/
} FIO_TYPES;

/* three flavors of insert mode	*/
/* it's FALSE, or one of:	*/
#define INSMODE_INS 1
#define INSMODE_OVR 2
#define INSMODE_RPL 3

/* kill register control -- values for kbflag */
#define KNEEDCLEAN   iBIT(0)		/* Kill register needs cleaning */
#define KYANK        iBIT(1)		/* Kill register resulted from yank */
#define KLINES       iBIT(2)		/* Kill register contains full lines */
#define KRECT        iBIT(3)		/* Kill register contains rectangle */
#define KAPPEND      iBIT(4)		/* Kill register should be appended */

/* operator types.  Needed mainly because word movement changes depending on
	whether operator is "delete" or not.  Aargh.  */
#define OPDEL 1
#define OPOTHER 2

/* popup-choices values */
#define POPUP_CHOICES_OFF      0
#define POPUP_CHOICES_IMMED    1
#define POPUP_CHOICES_DELAYED  2

/* popup-positions values */
#define POPUP_POSITIONS_NOTDOT  0
#define POPUP_POSITIONS_BOTTOM  1
#define POPUP_POSITIONS_TOP     2

/* define these so C-fence matching doesn't get confused when we're editing
	the cfence code itself */
#define L_CURLY '{'
#define R_CURLY '}'
#define L_PAREN '('
#define R_PAREN ')'
#define L_BLOCK '['
#define R_BLOCK ']'
#define L_ANGLE '<'
#define R_ANGLE '>'

/* these are the characters that are used in the expand-chars mode */
#define EXPC_THIS  '%'
#define EXPC_THAT  '#'
#define EXPC_SHELL '!'
#define EXPC_RPAT  '~'
#if OPT_VMS_PATH || OPT_MSDOS_PATH	/* ':' gets in the way of drives */
#define EXPC_TOKEN '&'
#else
#define EXPC_TOKEN ':'
#endif

/* separator used when scanning PATH environment variable */
#if SYS_VMS
#define	PATHCHR	','
#define PATHSEP '/'
#endif

#if OPT_MSDOS_PATH
#define	PATHCHR	';'
#define PATHSEP '\\'
#endif

#ifndef PATHCHR				/* e.g., UNIX */
#define	PATHCHR	':'
#define PATHSEP '/'
#endif


/* token types								*/
/* these must be kept in sync with the eval_func[] table in eval.c	*/
#define TOK_NULL        0   /* null string          */
#define TOK_QUERY       1   /* query response       */
#define TOK_BUFLINE     2   /* line from buffer     */
#define TOK_TEMPVAR     3   /* temp variables       */
#define TOK_STATEVAR    4   /* state variables      */
#define TOK_FUNCTION    5   /* functions            */
#define TOK_DIRECTIVE   6   /* macro lang directive */
#define TOK_LABEL       7   /* goto target          */
#define TOK_QUOTSTR     8   /* quoted string        */
#define TOK_LITSTR      9   /* unquoted string      */
#define	MAXTOKTYPE	9

#if OPT_VILE_ALLOC
#include <vl_alloc.h>
#endif

#if OPT_VILE_CTYPE
#include <vl_ctype.h>
#endif

#if OPT_VILE_REGEX
#define regcomp  vl_regcomp
#define regexec  vl_regexec
#include <vl_regex.h>
#endif

			/* see screen_to_bname() */
#if OPT_WIDE_CTYPES
#define	SCREEN_STRING (vl_pathn | vl_wild | vl_scrtch | vl_shpipe)
#else
#define	SCREEN_STRING (vl_pathn | vl_wild)
#endif

#define KEY_Space	' '
#define KEY_Tab		'\t'

#if OPT_KEY_MODIFY
#define isBackTab(c)	((c) == KEY_BackTab || (((c) & mod_SHIFT) != 0 && CharOf(c) == KEY_Tab))
#else
#define isBackTab(c)	((c) == KEY_BackTab)
#endif

#define	NonNull(s)	((s == NULL) ? "" : s)

#define ESC		tocntrl('[')
#define BEL		tocntrl('G')	/* ascii bell character		*/
#define CONTROL_A	tocntrl('A')	/* for cntl_a attribute sequences */

#if !(SYS_MSDOS && CC_DJGPP)
/* some systems need a routine to check for interrupts.  most don't, and
 * the routine overhead can be expensive in some places
 */
# define interrupted() (am_interrupted != 0)
#endif

#define ABORTED(c) ((c) == esc_c || (c) == intrc || interrupted())

/*
 * Data for read/write/etc, hooks
 */
typedef struct {
	char proc[NBUFN];
	int latch;
} HOOK;

/*
 * Definitions for 'tbuff.c' (temporary/dynamic char-buffers)
 */
typedef	struct	vl_tbuff	{
	char *	tb_data;	/* the buffer-data */
	size_t	tb_size;	/* allocated size */
	size_t	tb_used;	/* total used in */
	size_t	tb_last;	/* last put/get index */
	int	tb_endc;
	int	tb_errs;	/* true if we copied error_val here */
	} TBUFF;

#define isTB_ERRS(p) ((p) != NULL && (p)->tb_errs)

/*
 * Definitions for 'itbuff.c' (temporary/dynamic int-buffers)
 */
typedef	struct	vl_itbuff	{
	int *	itb_data;	/* the buffer-data */
	size_t	itb_size;	/* allocated size */
	size_t	itb_used;	/* total used in */
	size_t	itb_last;	/* last put/get index */
	int	itb_endc;
	} ITBUFF;

/*
 * Primitive types
 */
typedef	int		L_NUM;		/* line-number */
typedef	int		C_NUM;		/* column-number */
typedef	struct {
    USHORT flgs;
    USHORT cook;
} L_FLAG;		/* LINE-flags */

typedef	ULONG		CMDFLAGS;	/* CMDFUNC flags */
typedef	ULONG		B_COUNT;	/* byte-count */

/*
 * Normally we build with ANSI C compilers, but allow for the possibility of
 * building with C++.  There is an incompatibility between C/C++ in the
 * treatment of const:  C++ will not export const data unless it is declared
 * extern -- for statements where the data is given for initialization.  While
 * ANSI C permits (but does not require) putting the extern there as well, some
 * compilers that we support via unproto do not allow this syntax.
 */
#ifdef __cplusplus
#define EXTERN_CONST extern const
#else
#define EXTERN_CONST const
#endif

/*
 * The Intel compiler's warnings regarding externs which have not been
 * previously declared applies to data items as well as functions.  Use these
 * macros to hide the extra declaration.
 */
#if defined(__INTEL_COMPILER)
# define DECL_EXTERN_CONST(name) extern const name; EXTERN_CONST name
# define DECL_EXTERN(name)       extern       name;              name
#else
# define DECL_EXTERN_CONST(name) EXTERN_CONST name
# define DECL_EXTERN(name)                    name
#endif

/*
 * Control structures
 */
#define	for_ever for(;;)

	/* avoid "constant-in-conditional-context */
#ifdef lint
#define one_time while(malloc(1)==0)
#else
#define one_time while(0)
#endif

/*
 * All text is kept in circularly linked lists of "LINE" structures. These
 * begin at the header line. This line is pointed to by the "BUFFER".
 * Each line contains:
 *  number of bytes in the line (the "used" size),
 *  the size of the text array,
 *  the text.
 * The end of line is not stored as a byte; it's implied. Future
 * additions may include update hints, and a list of marks into the line.
 *
 * Lines are additionally sometimes stacked in undo lists.
 */
typedef struct	LINE {
	struct LINE *l_fp;		/* Link to the next line	*/
	struct LINE *l_bp;		/* Link to the previous line	*/
	union {
		size_t	l_sze;		/* Allocated size		*/
		C_NUM	l_fo;		/* forward undo dot offs (undo only) */
	} l_s_fo;
	union {
		L_NUM	l_nmbr;		/* line-# iff b_numlines > 0	*/
		C_NUM	l_bo;		/* backward undo dot offs (undo only) */
	} l_n_bo;
	int	l_used;			/* Used size (may be negative)	*/
	union {
	    char *l_txt;		/* The data for this line	*/
	    struct LINE *l_nxt;		/* if an undo stack separator,	*/
	} lt;				/*  a pointer to the next one	*/
	union
	{
	    struct LINE	*l_stklnk;	/* Link for undo stack		*/
	    L_FLAG	l_flg;		/* flags for undo ops		*/
	} l;
#if OPT_LINE_ATTRS
	UCHAR  *l_attrs;		/* indexes into the line_attr_tbl
					   hash table */
#endif
}	LINE;

#define l_size		l_s_fo.l_sze
#define l_forw_offs	l_s_fo.l_fo
#define l_number	l_n_bo.l_nmbr
#define l_back_offs	l_n_bo.l_bo
#define l_text		lt.l_txt
#define l_nextsep	lt.l_nxt

#define l_undo_cookie	l_flg.cook
#define l_flag		l_flg.flgs

/* LINE.l_flag values */
#define LCOPIED  lBIT(0)	/* original line is already on an undo stack */
#define LGMARK   lBIT(1)	/* line matched a global scan */
#define LTRIMMED lBIT(2)	/* line doesn't have newline to display */

/* macros to ease the use of lines */
#define dot_next_bol()  do { \
				DOT.l = lforw(DOT.l); \
				DOT.o = b_left_margin(curbp); \
			} while (0)
#define dot_prev_bol()  do { \
				DOT.l = lback(DOT.l); \
				DOT.o = b_left_margin(curbp); \
			} while (0)
#define	for_each_line(lp,bp) for (lp = lforw(buf_head(bp)); \
					(lp != NULL) && (lp != buf_head(bp)); \
					lp = lforw(lp))

#define l_nxtundo		l.l_stklnk

	/*
	 * Special values used in LINE.l_used
	 */
#define LINENOTREAL	((int)(-1)) /* for undo, marks an inserted line */
#define LINEUNDOPATCH	((int)(-2)) /* provides stack patching value for undo */
#define STACKSEP	((int)(-4)) /* delimit set of changes on undo stack */
#define PURESTACKSEP	((int)(-3)) /* as above, but buffer unmodified before */
					/* this change */

#define set_lforw(a,b)	lforw(a) = (b)
#define set_lback(a,b)	lback(a) = (b)
#define lforw(lp)	(lp)->l_fp
#define lback(lp)	(lp)->l_bp

	/*
	 * Macros for referencing fields in the LINE struct.
	 */
#define lgetc(lp, n)		CharOf(lvalue(lp)[(n)])
#define lputc(lp, n, c)		(lvalue(lp)[(n)]=(char)(c))
#define lvalue(lp)		((lp)->l_text)
#define llength(lp)		((lp)->l_used)
#define line_length(lp)		((B_COUNT)llength(lp)+len_rs) /* counting recordsep */

#define liscopied(lp)		((lp)->l.l_undo_cookie == current_undo_cookie)
#define lsetcopied(lp)		((lp)->l.l_undo_cookie = current_undo_cookie)
#define lsetnotcopied(lp)	((lp)->l.l_undo_cookie = 0)

#define lismarked(lp)		((lp)->l.l_flag & LGMARK)
#define lsetmarked(lp)		((lp)->l.l_flag |= LGMARK)
#define lsetnotmarked(lp)	(clr_typed_flags((lp)->l.l_flag, USHORT, LGMARK))
#define lflipmark(lp)		((lp)->l.l_flag ^= LGMARK)

#define listrimmed(lp)		((lp)->l.l_flag & LTRIMMED)
#define lsettrimmed(lp)		((lp)->l.l_flag |= LTRIMMED)
#define lsetnottrimmed(lp)	((lp)->l.l_flag &= (USHORT) ~LTRIMMED)
#define lsetclear(lp)		((lp)->l.l_flag = (lp)->l.l_undo_cookie = 0)

#define lisreal(lp)		(llength(lp) >= 0)
#define lisnotreal(lp)		(llength(lp) == LINENOTREAL)
#define lislinepatch(lp)	(llength(lp) == LINEUNDOPATCH)
#define lispatch(lp)		(lislinepatch(lp))
#define lisstacksep(lp)		(llength(lp) == STACKSEP || \
					llength(lp) == PURESTACKSEP)
#define lispurestacksep(lp)	(llength(lp) == PURESTACKSEP)

/* marks are a line and an offset into that line */
typedef struct MARK {
	LINE *l;
	C_NUM o;
} MARK;

/* some macros that take marks as arguments */
#define is_at_end_of_line(m)	(m.o == llength(m.l))
#define is_empty_line(m)	(llength(m.l) == 0)
#define sameline(m1,m2)		(m1.l == m2.l)
#define samepoint(m1,m2)	(sameline(m1,m2) && (m1.o == m2.o))
#define char_at(m)		(lgetc(m.l,m.o))
#define put_char_at(m,c)	(lputc(m.l,m.o,c))
#define is_header_line(m,bp)	(m.l == buf_head(bp))
#define is_last_line(m,bp)	(lforw(m.l) == buf_head(bp))
#define is_first_line(m,bp)	(lback(m.l) == buf_head(bp))

/*
 * The starting position of a region, and the size of the region in
 * characters, is kept in a region structure.  Used by the region commands.
 */
typedef struct	{
	MARK	r_orig;			/* Origin LINE address.		*/
	MARK	r_end;			/* Ending LINE address.		*/
	C_NUM	r_leftcol;		/* Leftmost column.		*/
	C_NUM	r_rightcol;		/* Rightmost column.		*/
	B_COUNT r_size;			/* Length in characters.	*/
#if OPT_SELECTIONS
	USHORT	r_attr_id;		/* id of corresponding display  */
#endif
}	REGION;

#if OPT_COLOR || OPT_HILITEMATCH
typedef USHORT VIDEO_ATTR;		/* assume short is at least 16 bits */
#else
typedef UCHAR VIDEO_ATTR;
#endif

#define VACURS	iBIT(0)			/* cursor -- this is intentionally
					 * the same as VADIRTY. It should
					 * not be used anywhere other than
					 * in specific places in the low
					 * level drivers (e.g, x11.c).
					 */
#define VAMLFOC	iBIT(1)			/* modeline w/ focus		*/
#define VAML	iBIT(2)			/* standard mode line (no focus)*/
#define VASEL	iBIT(3)			/* selection			*/
#define	VAREV	iBIT(4)			/* reverse video		*/
#define	VAUL	iBIT(5)			/* underline			*/
#define	VAITAL	iBIT(6)			/* italics			*/
#define	VABOLD	iBIT(7)			/* bold				*/
#define VAOWNER ((VIDEO_ATTR)0x0700)	/* owner mask			*/
#define VASPCOL ((VIDEO_ATTR)0x0800)	/* specific color		*/
#define VACOLOR ((VIDEO_ATTR)0xf000)	/* color mask			*/
#define VACOL_0 (VASPCOL|0x0000)	/* color palette index 0	*/
#define VACOL_1 (VASPCOL|0x1000)	/* etc.				*/
#define VACOL_2 (VASPCOL|0x2000)
#define VACOL_3 (VASPCOL|0x3000)
#define VACOL_4 (VASPCOL|0x4000)
#define VACOL_5 (VASPCOL|0x5000)
#define VACOL_6 (VASPCOL|0x6000)
#define VACOL_7 (VASPCOL|0x7000)
#define VACOL_8 (VASPCOL|0x8000)
#define VACOL_9 (VASPCOL|0x9000)
#define VACOL_A (VASPCOL|0xA000)
#define VACOL_B (VASPCOL|0xB000)
#define VACOL_C (VASPCOL|0xC000)
#define VACOL_D (VASPCOL|0xD000)
#define VACOL_E (VASPCOL|0xE000)
#define VACOL_F (VASPCOL|0xF000)

#define VCOLORNUM(attr) ((int) ((attr) & VACOLOR) >> 12)
#define VCOLORATTR(num) ((VIDEO_ATTR) ((UINT)(num) << 12))

/* who owns an attributed region -- so we can delete them independently */
#define VOWNER(attr)	((attr) & VAOWNER)
#define VOWN_MATCHES	0x0100
#define VOWN_OPERS	0x0200
#define VOWN_SELECT	0x0300
#define VOWN_CTLA	0x0400

/* The VATTRIB macro masks out those bits which should not be considered
 * for comparison purposes
 */

#if OPT_PSCREEN
#define VADIRTY	iBIT(0)			/* cell needs to be written out */
#define VATTRIB(attr) ((VIDEO_ATTR) ((attr) & (VIDEO_ATTR) ~(VAOWNER|VADIRTY)))
#else
#define VADIRTY ((VIDEO_ATTR)0)		/* nop for all others */
#define VATTRIB(attr) ((VIDEO_ATTR) ((attr) & (VIDEO_ATTR) ~(VAOWNER)))
#endif

/* grow (or initially allocate) a vector of newsize types, pointed to by
 * ptr.  this is used primarily for resizing the screen
 * the studious will note this is a lot like realloc.   but realloc
 * doesn't guarantee to preserve contents if it fails, and this also
 * zeroes the new space.
 */
#define GROW(ptr, type, oldsize, newsize) \
{ \
	size_t tmpold = (size_t) oldsize; \
	type *tmpp; \
	tmpp = typeallocn(type, (size_t) newsize); \
	if (tmpp == NULL) \
		return FALSE; \
 \
	if (ptr) { \
		(void) memcpy((void *)tmpp, (void *)ptr, tmpold * sizeof(type)); \
		free((void *)ptr); \
	} else { \
		tmpold = 0; \
	} \
	ptr = tmpp; \
	(void) memset ((void *)(ptr + tmpold), 0, \
		       ((size_t) newsize - tmpold) * sizeof(type)); \
}

/*
 * An attributed region is attached to a buffer and indicates how the
 * region should be displayed; eg. inverse video, underlined, etc.
 */

typedef struct vl_aregion {
	struct vl_aregion	*ar_next;
	REGION		ar_region;
	VIDEO_ATTR	ar_vattr;
	REGIONSHAPE	ar_shape;
#if OPT_HYPERTEXT
	char *		ar_hypercmd;
#endif
}	AREGION;

/* Hash table entry for line attribute indices. */
typedef struct vl_line_attr_entry {
	VIDEO_ATTR	vattr;
	char		in_use;
}	LINE_ATTR_ENTRY;

typedef	struct {
	char *pat;
	regexp *reg;
} REGEXVAL;

/* this is to ensure values can be of any type we wish.
   more can be added if needed.  */
union V {
	int i;
	char *p;
	REGEXVAL *r;
};

struct VAL {
	union V v;
	union V *vp;
};

typedef	struct	{
	const struct VALNAMES *names;
	struct VAL      *local;
	struct VAL      *global;
} VALARGS;

struct	BUFFER;

#define CHGD_ARGS struct BUFFER *bp, VALARGS *args, int glob_vals, int testing

typedef	int	(*ChgdFunc) ( CHGD_ARGS );

/* settable values have their names stored here, along with a synonym, and
	what type they are */
struct VALNAMES {
		const char *name;
		const char *shortname;
		char  type;
		ChgdFunc side_effect;
};
/* the values of VALNAMES->type */
#define VALTYPE_UNKNOWN  -1
#define VALTYPE_INT    0
#define VALTYPE_STRING 1
#define VALTYPE_BOOL   2
#define VALTYPE_REGEX  3
#define VALTYPE_ENUM   4
#define VALTYPE_MAJOR  5

	/*
	 * Values are either local or global. We distinguish the two cases
	 * by whether the value-pointer points into the VAL-struct or not.
	 */
#define is_local_val(lv,which)          (lv[which].vp == &(lv[which].v))
#define make_local_val(lv,which)        (lv[which].vp = &(lv[which].v))
#define make_global_val(lv,gv,which)    (lv[which].vp = &(gv[which].v))

/* these are masks for the WINDOW.w_flag hint */
#define WFFORCE iBIT(0)			/* Window needs forced reframe	*/
#define WFMOVE	iBIT(1)			/* Movement from line to line	*/
#define WFEDIT	iBIT(2)			/* Editing within a line	*/
#define WFHARD	iBIT(3)			/* Better do a full display	*/
#define WFMODE	iBIT(4)			/* Update mode line.		*/
#define WFCOLR	iBIT(5)			/* Needs a color change		*/
#define WFKILLS	iBIT(6)			/* something was deleted	*/
#define WFINS	iBIT(7)			/* something was inserted	*/
#define WFSTAT	iBIT(8)			/* Update mode line (info only).*/
#define WFSBAR	iBIT(9)			/* Update scroll bar(s)		*/

/* define indices for GLOBAL, BUFFER, WINDOW modes */
#ifdef realdef
#include "chgdfunc.h"
#endif

#if	CHECK_PROTOTYPES
	typedef long W_VALUES;
	typedef long M_VALUES;
	typedef long Q_VALUES;
	typedef long B_VALUES;
#else
#	include "nemode.h"
#endif

/* macros for setting GLOBAL modes */

#define global_g_val(which) global_g_values.gv[which].v.i
#define set_global_g_val(which,val) global_g_val(which) = val
#define global_g_val_ptr(which) global_g_values.gv[which].v.p
#define set_global_g_val_ptr(which,val) global_g_val_ptr(which) = val
#define global_g_val_rexp(which) global_g_values.gv[which].v.r
#define set_global_g_val_rexp(which,val) global_g_val_rexp(which) = val

/* these are window properties affecting window appearance _only_ */
typedef struct	{
	MARK	w_dt;		/* Line containing "."	       */
		/* i don't think "mark" needs to be here -- I think it
			could safely live only in the buffer -pgf */
#if WINMARK
	MARK	w_mk;		/* Line containing "mark"      */
#endif
	MARK	w_ld;		/* Line containing "lastdotmark"*/
	MARK	w_tld;		/* Line which may become "lastdotmark"*/
	MARK	w_ln;		/* Top line in the window (offset used in linewrap) */
#if OPT_MOUSE
	int	insmode;
#endif
	W_VALUES w_vals;
#if OPT_CACHE_VCOL
	MARK	w_left_dot;	/* nominal location of left-side of screen */
	int	w_left_col;	/* ...corresponding effective column */
	int	w_left_adj;	/* ...and margin of error */
	int	w_list_opt;	/* ...and list-mode setting */
	int	w_num_cols;	/* ...and screen-width */
#endif
} W_TRAITS;

#define global_w_val(which)             global_w_values.wv[which].v.i
#define global_w_val_ptr(which)         global_w_values.wv[which].v.p

#define set_global_w_val(which,val)     global_w_val(which) = val
#define set_global_w_val_ptr(which,val) global_w_val_ptr(which) = val

#define w_val(wp,val)                   (wp->w_values.wv[val].vp->i)
#define w_val_ptr(wp,val)               (wp->w_values.wv[val].vp->p)

#define set_w_val(wp,which,val)         w_val(wp,which) = val
#define set_w_val_ptr(wp,which,val)     w_val_ptr(wp,which) = val

#define make_local_w_val(wp,which)  \
	make_local_val(wp->w_values.wv, which)
#define make_global_w_val(wp,which)  \
	make_global_val(wp->w_values.wv, global_wvalues.wv, which)

#define is_local_w_val(wp,which)  \
	is_local_val(wp->w_values.wv,which)

#if OPT_COLOR
#define gfcolor global_g_val(GVAL_FCOLOR)
#define gbcolor global_g_val(GVAL_BCOLOR)
#define gccolor global_g_val(GVAL_CCOLOR)
#else
#define gfcolor C_WHITE
#define gbcolor C_BLACK
#endif

#if OPT_MAJORMODE
/*
 * A majormode is a special set of buffer mode values, together with other
 * values (such as filename suffixes) which are used to determine when a
 * majormode should be attached to a buffer.  We allocate the structure in two
 * levels (MAJORMODE vs MAJORMODE_LIST) to avoid having to adjust pointers
 * within the VAL arrays (M_VALUES and B_VALUES) when we add or remove new
 * majormodes.
 */
typedef struct MINORMODE {
	struct MINORMODE *sm_next;
	char             *sm_name;	/* the name of this group */
	B_VALUES          sm_vals;	/* mode-values for the group */
} MINORMODE;

typedef struct {
	char *shortname;	/* name without "mode" suffix */
	char *longname;		/* name with "mode" suffix */
	M_VALUES mm;		/* majormode-specific flags, e.g., "preamble" */
	Q_VALUES mq;		/* qualifier values such as "group" */
	MINORMODE *sm;
} MAJORMODE;

#define is_c_mode(bp) (bp->majr != NULL && !strcmp(bp->majr->shortname, "c"))
#define fix_cmode(bp,value)	/* nothing */
#define for_each_modegroup(bp,n,m,vals) \
	for (vals = get_submode_vals(bp, n = m); vals != NULL; vals = get_submode_valx(bp, m, &n))
#else
#define is_c_mode(bp) (b_val(bp,MDCMOD))
#define fix_cmode(bp,value)	set_local_b_val(bp, MDCMOD, value)
#define for_each_modegroup(bp,n,m,vals) vals = bp->b_values.bv;
#endif

#if SYS_UNIX && !SYS_OS2_EMX
#define CAN_CHECK_INO 1
typedef struct {
	dev_t dev;
	ino_t ino;
	int   valid;		/* are dev and ino good? */
} FUID;
#else
typedef int FUID;
#endif

#if (OPT_AUTOCOLOR || OPT_ELAPSED) && !defined(VL_ELAPSED)
#ifdef HAVE_GETTIMEOFDAY
#define VL_ELAPSED struct timeval
#elif SYS_WINNT
/* this is in w32vile.h */
#else
#define VL_ELAPSED time_t
#endif
#endif

/*
 * Text is kept in buffers.  A buffer header, described below, exists
 * for every buffer in the system.  The buffers are kept in a big
 * list, so that commands that search for a buffer by name can find
 * the buffer header.  There is a safe store for the dot and mark in
 * the header, but this is only valid if the buffer is not being
 * displayed (that is, if "b_nwnd" is 0).  The text for the buffer is
 * kept in a circularly linked list of lines, with a pointer to the
 * header line in "b_line" Buffers may be "Inactive" which means the
 * files associated with them have not been read in yet.  These get
 * read in at "use buffer" time.
 */

typedef	int	(*UpBuffFunc) ( struct BUFFER * );

typedef struct	BUFFER {
	MARK	b_line;		/* Link to the header LINE (offset unused) */
	struct	BUFFER *b_bufp;		/* Link to next BUFFER		*/
	MARK	*b_nmmarks;		/* named marks a-z		*/
#if OPT_SELECTIONS
	AREGION	*b_attribs;		/* attributed regions		*/
#endif
#if OPT_MAJORMODE
	MAJORMODE *majr;		/* majormode, if any */
#endif
	B_VALUES b_values;		/* buffer traits we inherit from */
					/*  global values		*/
	W_TRAITS b_wtraits;		/* saved window traits, while we're */
					/*  not displayed		*/
	B_COUNT	b_bytecount;		/* # of chars			*/
	L_NUM	b_linecount;		/* no. lines in buffer		*/
	L_NUM	b_lines_on_disk;	/* no. lines as of last read/write */
	LINE	*b_udstks[2];		/* undo stack pointers		*/
	MARK	b_uddot[2];		/* Link to "." before undoable op */
	short	b_udstkindx;		/* which of above to use	*/
	LINE	*b_udtail;		/* tail of undo backstack	*/
	LINE	*b_udlastsep;		/* last stack separator pushed	*/
	int	b_udcount;		/* how many undo's we can do	*/
	LINE	*b_LINEs;		/* block-malloced LINE structs	*/
	LINE	*b_LINEs_end;		/* end of	"	"	*/
	LINE	*b_freeLINEs;		/* list of free "	"	*/
	UCHAR	*b_ltext;		/* block-malloced text		*/
	UCHAR	*b_ltext_end;		/* end of block-malloced text	*/
	LINE	*b_ulinep;		/* pointer at 'Undo' line	*/
	int	b_active;		/* window activated flag	*/
	int	b_refcount;		/* counts levels of source'ing	*/
	UINT	b_nwnd;			/* Count of windows on buffer	*/
	UINT	b_flag;			/* Flags			*/
	short	b_inuse;		/* nonzero if executing macro	*/
	short	b_acount;		/* auto-save count		*/
	const char *b_recordsep_str;	/* string for recordsep		*/
	int	b_recordsep_len;	/* ...its length		*/
	char	*b_fname;		/* File name			*/
	int	b_fnlen;		/* length of filename		*/
	char	b_bname[NBUFN];		/* Buffer name			*/
#if OPT_AUTOCOLOR
	double	last_autocolor_time;	/* millisecond for last autocolor */
	long	next_autocolor_time;	/* count for skipping autocolor */
#endif
#if OPT_CURTOKENS
	struct VAL buf_fname_expr;	/* $buf-fname-expr		*/
#endif
#if OPT_ENCRYPT
	char	b_cryptkey[NKEYLEN];	/* encryption key		*/
#endif
#if SYS_UNIX
	FUID	b_fileuid;		/* file unique id (dev/ino on unix) */
	FUID	b_fileuid_at_warn;	/* file unique id when user warned */
#endif
#ifdef	MDCHK_MODTIME
	time_t	b_modtime;		/* file's last-modification time */
	time_t	b_modtime_at_warn;	/* file's modtime when user warned */
#endif
#if	OPT_NAMEBST
	TBUFF	*b_procname;		/* full procedure name		*/
#endif
#if	OPT_UPBUFF
	UpBuffFunc b_upbuff;		/* call to recompute		*/
	UpBuffFunc b_rmbuff;		/* call on removal		*/
#endif
#if	OPT_B_LIMITS
	int	b_lim_left;		/* extra left-margin (cf:show-reg) */
#endif
	struct	BUFFER *b_relink;	/* Link to next BUFFER (sorting) */
	int	b_created;
	int	b_last_used;
#if OPT_HILITEMATCH
	USHORT	b_highlight;
#endif
#if OPT_MULTIBYTE
	BOM_CODES implied_BOM;		/* fix decode/encode if BOM missing */
	UINT	*decode_utf_buf;	/* workspace for decode_charset() */
	size_t	decode_utf_len;
	char	*encode_utf_buf;	/* workspace for encode_charset() */
	size_t	encode_utf_len;
#endif
#if OPT_PERL || OPT_TCL || OPT_PLUGIN
	void *	b_api_private;		/* pointer to private perl, tcl, etc.
					   data */
#endif
#if COMPLETE_FILES || COMPLETE_DIRS
	char ** b_index_list;		/* array to index into this buffer */
	size_t	b_index_size;
	int	b_index_counter;	/* iterator for the array */
#endif
}	BUFFER;

/*
 * Special symbols for scratch-buffer names.
 */
#define	SCRTCH_LEFT  "["
#define	SCRTCH_RIGHT "]"
#define	SHPIPE_LEFT  "!"

/* warning:  code in file.c and fileio.c knows how long the shell, pipe, and
	append prefixes are (e.g. fn += 2 when appending) */
#define	isShellOrPipe(s)  ((s)[0] == SHPIPE_LEFT[0])
#define	isInternalName(s) (isShellOrPipe(s) || is_internalname(s))
#define	isAppendToName(s) ((s)[0] == '>' && (s)[1] == '>')

/* shift-commands can be repeated when typed on :-command */
#define isRepeatable(c)   ((c) == '<' || (c) == '>')

/*
 * Macros for manipulating buffer-struct members.
 */
#define	for_each_buffer(bp) for (bp = bheadp; bp; bp = bp->b_bufp)

#define gbl_b_val(which)                 global_b_values.bv[which]

#define global_b_val(which)              gbl_b_val(which).v.i
#define global_b_val_ptr(which)          gbl_b_val(which).v.p
#define global_b_val_rexp(which)         gbl_b_val(which).v.r

#define set_global_b_val(which,val)      global_b_val(which) = val
#define set_global_b_val_ptr(which,val)  global_b_val_ptr(which) = val
#define set_global_b_val_rexp(which,val) global_b_val_rexp(which) = val

#define any_b_val(bp,which)              (bp)->b_values.bv[which]

#define b_val(bp,which)                  (any_b_val(bp,which).vp->i)
#define b_val_ptr(bp,which)              (any_b_val(bp,which).vp->p)
#define b_val_rexp(bp,which)             (any_b_val(bp,which).vp->r)

#define set_b_val(bp,which,val)          b_val(bp,which) = val
#define set_b_val_ptr(bp,which,val)      b_val_ptr(bp,which) = val
#define set_b_val_rexp(bp,which,val)     b_val_rexp(bp,which) = val

#define window_b_val(wp,val) \
	((wp != NULL && wp->w_bufp != NULL) \
		? b_val(wp->w_bufp,val) \
		: global_b_val(val))

#define make_local_b_val(bp,which)  \
		make_local_val(bp->b_values.bv, which)
#define make_global_b_val(bp,which)  \
		make_global_val(bp->b_values.bv, global_b_values.bv, which)

#define is_local_b_val(bp,which)  \
	is_local_val(bp->b_values.bv,which)

#define set_local_b_val(bp,mode,value) \
		make_local_b_val(bp, mode), \
		set_b_val(bp, mode, value)

#define is_empty_buf(bp) (lforw(buf_head(bp)) == buf_head(bp))

#define b_dot     b_wtraits.w_dt
#if WINMARK
#define b_mark    b_wtraits.w_mk
#endif
#define b_lastdot b_wtraits.w_ld
#define b_tentative_lastdot b_wtraits.w_tld
#define b_wline   b_wtraits.w_ln

#ifndef HAVE_STRICMP
#undef stricmp
#define stricmp vl_stricmp
#endif

#ifndef HAVE_STRNICMP
#undef strnicmp
#define strnicmp vl_strnicmp
#endif

#if (SYS_MSDOS || SYS_OS2 || SYS_WINNT || SYS_OS2_EMX || SYS_CYGWIN || SYS_VMS)
#define DFT_FILE_IC TRUE
#else
#define DFT_FILE_IC FALSE
#endif

#define COPY_B_VAL(dst,src,val) \
	if (is_local_b_val(src, val)) { \
	    set_local_b_val(dst, val, b_val(src, val)); \
	}

/* values for b_flag */
#define BFINVS     iBIT(0)	/* Internal invisible buffer	*/
#define BFCHG      iBIT(1)	/* Changed since last write	*/
#define BFDIRS     iBIT(2)	/* set for directory-buffers */
#define BFSCRTCH   iBIT(3)	/* scratch -- gone on last close */
#define BFARGS     iBIT(4)	/* set for ":args" buffers */
#define BFEXEC     iBIT(5)	/* set for ":source" buffers */
#define BFIMPLY    iBIT(6)	/* set for implied-# buffers */
#define BFSIZES    iBIT(7)	/* set if byte/line counts current */
#define BFUPBUFF   iBIT(8)	/* set if buffer should be updated */
#define BFRCHG     iBIT(9)	/* Changed since last reset of this flag*/
#define BFISREAD   iBIT(10)	/* set if we are reading data into buffer */
#define BFREGD     iBIT(11)	/* set if file path written to registry
				 * (winvile feature)
				 */

/* macros for manipulating b_flag */
#define b_is_set(bp,flags)        (((bp)->b_flag & (flags)) != 0)
#define b_is_argument(bp)         b_is_set(bp, BFARGS)
#define b_is_changed(bp)          b_is_set(bp, BFCHG)
#define b_is_counted(bp)          b_is_set(bp, BFSIZES)
#define b_is_directory(bp)        b_is_set(bp, BFDIRS)
#define b_is_implied(bp)          b_is_set(bp, BFIMPLY)
#define b_is_invisible(bp)        b_is_set(bp, BFINVS)
#define b_is_obsolete(bp)         b_is_set(bp, BFUPBUFF)
#define b_is_reading(bp)          b_is_set(bp, BFISREAD)
#define b_is_recentlychanged(bp)  b_is_set(bp, BFRCHG)
#define b_is_scratch(bp)          b_is_set(bp, BFSCRTCH)
#define b_is_temporary(bp)        b_is_set(bp, BFINVS|BFSCRTCH)
/*
 * don't write file paths to registry if already written _or_ if dealing
 * with a non-file and/or transient buffer.
 */
#define b_is_registered(bp)        b_is_set(bp, \
                                       BFINVS|BFSCRTCH|BFREGD|BFEXEC|BFDIRS)

#define b_set_flags(bp,flags)     (bp)->b_flag |= (flags)
#define b_set_changed(bp)         b_set_flags(bp, BFCHG)
#define b_set_counted(bp)         b_set_flags(bp, BFSIZES)
#define b_set_invisible(bp)       b_set_flags(bp, BFINVS)
#define b_set_obsolete(bp)        b_set_flags(bp, BFUPBUFF)
#define b_set_reading(bp)         b_set_flags(bp, BFISREAD)
#define b_set_recentlychanged(bp) b_set_flags(bp, BFRCHG)
#define b_set_scratch(bp)         b_set_flags(bp, BFSCRTCH)
#define b_set_registered(bp)      b_set_flags(bp, BFREGD)

#define b_clr_flags(bp,flags)     (bp)->b_flag &= ~(flags)
#define b_clr_changed(bp)         b_clr_flags(bp, BFCHG)
#define b_clr_counted(bp)         b_clr_flags(bp, BFSIZES)
#define b_clr_invisible(bp)       b_clr_flags(bp, BFINVS)
#define b_clr_obsolete(bp)        b_clr_flags(bp, BFUPBUFF)
#define b_clr_reading(bp)         b_clr_flags(bp, BFISREAD)
#define b_clr_recentlychanged(bp) b_clr_flags(bp, BFRCHG)
#define b_clr_scratch(bp)         b_clr_flags(bp, BFSCRTCH)
#define b_clr_registered(bp)      b_clr_flags(bp, BFREGD)

#if OPT_HILITEMATCH
#define b_match_attrs_dirty(bp)	(bp)->b_highlight |= HILITE_DIRTY
#else
#define b_match_attrs_dirty(bp)
#endif

#if OPT_B_LIMITS
#define b_left_margin(bp)       bp->b_lim_left
#define b_set_left_margin(bp,n) b_left_margin(bp) = n
#else
#define b_left_margin(bp)       0
#define b_set_left_margin(bp,n)
#endif

#if OPT_HILITEMATCH
#define HILITE_ON	1
#define HILITE_DIRTY	2
#endif

/* macros related to external record separator */
#define len_record_sep(bp)	((bp)->b_recordsep_len)
#define get_record_sep(bp)	((bp)->b_recordsep_str)

/* macros related to internal record separator */
#define use_record_sep(bp)	((b_val(bp, VAL_RECORD_SEP) == RS_CR) ? '\r' : '\n')
#define is_record_sep(bp,c)	((c) == use_record_sep(bp))

/* macro for iterating over the marks associated with the current buffer */

#if OPT_PERL || OPT_TCL || OPT_PLUGIN
extern MARK *api_mark_iterator(BUFFER *bp, int *iter);
#define api_do_mark_iterate_helper(mp, statement)	\
	{						\
	    int dmi_iter = 0;				\
	    while ((mp = api_mark_iterator(curbp, &dmi_iter)) != NULL) { \
		statement				\
	    }						\
	}
#else
#define api_do_mark_iterate_helper(mp, statement)
#endif

#if OPT_VIDEO_ATTRS
#define do_mark_iterate(mp, statement)			\
    do {						\
	struct MARK *mp;				\
	int	     idx;				\
	AREGION     *dmi_ap = curbp->b_attribs;		\
	if (curbp->b_nmmarks != NULL)			\
	    for (idx=0; idx < NMARKS; idx++) {		\
		mp = &(curbp->b_nmmarks[idx]);		\
		statement				\
	    }						\
	if (dmi_ap != NULL) {				\
	    while (dmi_ap != NULL) {			\
		mp = &dmi_ap->ar_region.r_orig;		\
		statement				\
		mp = &dmi_ap->ar_region.r_end;		\
		statement				\
		dmi_ap = dmi_ap->ar_next;		\
	    }						\
	    sel_reassert_ownership(curbp);		\
	}						\
	api_do_mark_iterate_helper(mp, statement)	\
    } one_time
#else /* OPT_VIDEO_ATTRS */
#define do_mark_iterate(mp, statement)			\
    do {						\
	struct MARK *mp;				\
	if (curbp->b_nmmarks != NULL) {			\
	    int idx;					\
	    for (idx=0; idx < NMARKS; idx++) {		\
		mp = &(curbp->b_nmmarks[idx]);		\
		statement				\
	    }						\
	}						\
	api_do_mark_iterate_helper(mp, statement)	\
    } one_time
#endif /* OPT_VIDEO_ATTRS */

typedef int WIN_ID;

/*
 * There is a window structure allocated for every active display window. The
 * windows are kept in a big list, in top to bottom screen order, with the
 * listhead at "wheadp". Each window contains its own values of dot and mark.
 * The flag field contains some bits that are set by commands to guide
 * redisplay; although this is a bit of a compromise in terms of decoupling,
 * the full blown redisplay is just too expensive to run for every input
 * character.
 */

#define WINDOW	vile_WINDOW		/* avoid conflict with curses.h */

typedef struct	WINDOW {
	W_TRAITS w_traits;		/* features of the window we should */
					/* remember between displays	*/
	struct	WINDOW *w_wndp;		/* Next window			*/
	BUFFER	*w_bufp;		/* Buffer displayed in window	*/
	int	w_toprow;		/* Origin 0 top row of window	*/
	int	w_ntrows;		/* # of rows of text in window	*/
	int	w_force;		/* If non-zero, forcing row.	*/
	USHORT	w_flag;			/* Flags.			*/
	ULONG	w_split_hist;		/* how to recombine deleted windows */
	int	w_tabstop;		/* vtset's latest tabstop value */
#ifdef WMDRULER
	int	w_ruler_line;
	int	w_ruler_col;
#endif
#if OPT_PERL || OPT_TCL
	WIN_ID	w_id;			/* Unique window id */
#endif
}	WINDOW;

#define is_visible_window(wp) ((wp)->w_toprow >= 0)
#define is_fake_window(wp) (!(is_visible_window(wp)))

#if OPT_MULTIBYTE
#define is_delinked_bp(bp) ((bp) == bminip || (bp) == btempp)
#else
#define is_delinked_bp(bp) ((bp) == bminip)
#endif

#define	for_each_window(wp) for (wp = wheadp; wp; wp = wp->w_wndp)
#define for_each_visible_window(wp) \
		for_each_window(wp) if (is_visible_window(wp))

#define w_dot     w_traits.w_dt
#if WINMARK
#define w_mark    w_traits.w_mk
#endif
#define w_lastdot w_traits.w_ld
#define w_tentative_lastdot w_traits.w_tld
#define w_line    w_traits.w_ln
#define w_values  w_traits.w_vals

#define mode_row(wp)	((wp)->w_toprow + (wp)->w_ntrows)
#define	buf_head(bp)	((bp)->b_line.l)
#define	win_head(wp)	buf_head((wp)->w_bufp)

#define DOT curwp->w_dot
#if OPT_MOUSE
#define insertmode (curwp->w_traits.insmode)
#endif /* OPT_MOUSE */
#if WINMARK
#define MK curwp->w_mark
#else
#define MK Mark
#endif

/*
 * Data to save/restore when switching temporarily to the minibuffer for I/O.
 */
typedef struct {
	BUFFER *save_bp;
	WINDOW *save_wp;
	MARK save_mk;
} KBD_DATA;

	/* we use left-margin for protecting the prefix-area of [Registers]
	 * from cut/paste selection.
	 */
#define w_left_margin(wp) b_left_margin(wp->w_bufp)

	/* tputs uses a 3rd parameter (a function pointer).  We're stuck with
	 * making ttputc and term.putch the same type.
	 */
#ifdef OUTC_RETURN
#define OUTC_DCL int
#define OUTC_RET return
#else
#define OUTC_DCL void
#define OUTC_RET (void)
#endif

#ifndef OUTC_ARGS
#define OUTC_ARGS int c
#endif

/*
 * The editor communicates with the display using a high level interface. A
 * "TERM" structure holds useful variables, and indirect pointers to routines
 * that do useful operations. The low level get and put routines are here too.
 * This lets a terminal, in addition to having non standard commands, have
 * funny get and put character code too. The calls might get changed to
 * "termp->t_field" style in the future, to make it possible to run more than
 * one terminal type.
 */
typedef struct	{
	int	maxrows;		/* max rows count		*/
	int	rows;			/* current row count		*/
	int	maxcols;		/* max column count		*/
	int	cols;			/* current column count		*/
	void	(*set_enc)(ENC_CHOICES); /* tell what the display can do */
	ENC_CHOICES (*get_enc)(void);	/* tell what the display can do	*/
	void	(*open) (void);		/* Open terminal at the start.	*/
	void	(*close) (void);	/* Close terminal at end.	*/
	void	(*kopen) (void);	/* keyboard open		*/
	void	(*kclose) (void);	/* keyboard close		*/
	void	(*clean) (int f);	/* cleanup before shell-out	*/
	void	(*unclean) (void);	/* cleanup after shell-out	*/
	void	(*openup) (void);	/* open new line for prompt	*/
	int	(*getch) (void);	/* Get character from keyboard. */
	OUTC_DCL(*putch) (int c);	/* Put character to display.	*/
	int	(*typahead) (void);	/* character ready?		*/
	void	(*flush) (void);	/* Flush output buffers.	*/
	void	(*curmove) (int row, int col); /* Move the cursor, origin 0. */
	void	(*eeol) (void);		/* Erase to end of line.	*/
	void	(*eeop) (void);		/* Erase to end of page.	*/
	void	(*beep) (void);		/* Beep.			*/
	void	(*rev) (UINT f);	/* set reverse video state	*/
	int	(*setdescrip) (const char *f); /* reset display description */
	void	(*setfore) (int f);	/* set foreground color		*/
	void	(*setback) (int b);	/* set background color		*/
	int	(*setpal) (const char *p); /* set color palette	*/
	void	(*setccol) (int c);	/* set cursor color		*/
	void	(*scroll) (int from, int to, int n); /* scroll region	*/
	void	(*pflush) (void);	/* really flush			*/
	void	(*icursor) (int c);	/* set cursor shape for insertion */
	void	(*set_title) (const char *t); /* set window title	*/
	int	(*watchfd)(int, WATCHTYPE, long *);
					/* Watch a file descriptor for
					   input; execute associated
					   command when input is present*/
	void	(*unwatchfd)(int, long);
					/* Don't watch file descriptor	*/
	void	(*cursorvis)(int flag);	/* hide/show cursor		*/
	/* mouse interface, e.g., for xterm and clones */
	void	(*mopen) (void);	/* mouse open			*/
	void	(*mclose) (void);	/* mouse close			*/
	void	(*mevent) (void);	/* mouse event			*/
}	TERM;

#define term_is_utfXX()         (term.get_enc() >= enc_UTF8)

#if DISP_CURSES && defined(HAVE_ADDNWSTR)
#define WIDE_CURSES 1
#else
#define WIDE_CURSES 0
#endif

#if OPT_MULTIBYTE && ((DISP_NTCONS || DISP_NTWIN) && defined(UNICODE))
typedef USHORT VIDEO_TEXT;
typedef USHORT VIDEO_CHAR;
#elif OPT_MULTIBYTE && (DISP_TERMCAP || WIDE_CURSES || DISP_X11)
typedef unsigned VIDEO_TEXT;
typedef unsigned VIDEO_CHAR;
#else
typedef UCHAR VIDEO_TEXT;
typedef char  VIDEO_CHAR;
#endif

typedef struct  VIDEO {
	UINT	v_flag;			/* Flags */
#if	OPT_COLOR
	int	v_fcolor;		/* current foreground color */
	int	v_bcolor;		/* current background color */
	int	v_rfcolor;		/* requested foreground color */
	int	v_rbcolor;		/* requested background color */
#endif
#if	OPT_VIDEO_ATTRS
	VIDEO_ATTR *v_attrs;		/* screen data attributes */
#endif
	VIDEO_TEXT v_text[1];		/* Screen data. */
}	VIDEO;

#define VideoText(vp) (vp)->v_text
#define VideoAttr(vp) (vp)->v_attrs

#if OPT_COLOR
#define CurFcolor(vp) (vp)->v_fcolor
#define CurBcolor(vp) (vp)->v_bcolor
#define ReqFcolor(vp) (vp)->v_rfcolor
#define ReqBcolor(vp) (vp)->v_rbcolor
#else
#define CurFcolor(vp) gfcolor
#define CurBcolor(vp) gbcolor
#define ReqFcolor(vp) gfcolor
#define ReqBcolor(vp) gbcolor
#endif

#define VFCHG	iBIT(0)			/* Changed flag			*/
#define	VFEXT	iBIT(1)			/* extended (beyond column 80)	*/
#define	VFREV	iBIT(2)			/* reverse video status		*/
#define	VFREQ	iBIT(3)			/* reverse video request	*/
#define	VFCOL	iBIT(4)			/* color change requested	*/

/*
 * Menus are implemented by passing a string to a given callback function:
 */
typedef void (*ActionFunc) (char *);

/* Commands are represented as CMDFUNC structures, which contain a
 *	pointer to the actual function, and flags which help to classify it.
 *	(things like is it a MOTION, can it be UNDOne)
 *
 *	These structures are generated automatically from the cmdtbl file,
 *	and can be found in the file nefunc.h
 */
#define CMD_ARGS int f, int n

typedef	int	(*CmdFunc) (int f, int n);

typedef	struct {
#ifdef CC_CANNOT_INIT_UNIONS
	void	*c_union;
#define CMD_U_FUNC(p) (CmdFunc)((p)->c_union)
#define CMD_U_BUFF(p) (BUFFER*)((p)->c_union)
#define CMD_U_PERL(p) (void  *)((p)->c_union)
#define INIT_UNION(n) n
#else	/* C can init unions */
	union {
		CmdFunc c_func;
		BUFFER *c_buff;
#if OPT_PERL
		void *c_perl;	/* Perl 5 'AV' type */
#endif
	} cu;
	/* using the union gives us some type-checking and eliminates casts */
#define CMD_U_FUNC(p) ((p)->cu.c_func)
#define CMD_U_BUFF(p) ((p)->cu.c_buff)
#define CMD_U_PERL(p) ((p)->cu.c_perl)
#define INIT_UNION(n) {n}
#endif /* CC_CANNOT_INIT_UNIONS */
	CMDFLAGS c_flags;	/* what sort of command is it? */
	const char **c_alias;	/* all names by which this is known */
#if OPT_MACRO_ARGS
	PARAM_INFO *c_args;	/* if nonnull, lists types of parameters */
#endif
#if OPT_TRACE
	const char *c_name;	/* preferred name, for tracing */
#endif
#if OPT_ONLINEHELP
	const char *c_help;	/* short help message for the command */
#endif
}	CMDFUNC;

/*
 * Other useful argument templates
 */
#define EOL_ARGS  const char * buffer, size_t cpos, int c, int eolchar
#define DONE_ARGS KBD_OPTIONS flags, int c, char *buf, size_t *pos
#define LIST_ARGS int flag, void *ptr
#define REGN_ARGS void *flagp, int l, int r

#define PASS_DONE_ARGS flags, c, buf, pos

typedef	int	(*OpsFunc) (void);

/* when referencing a command by name (e.g ":e file") it is looked up in
 *	the nametbl, which is an array of NTAB structures, containing the
 *	name, and a pointer to the CMDFUNC structure.  There can be several
 *	entries pointing at a single CMDFUNC, since a command might have
 *	several synonymous names.
 *
 *	The nametbl array is generated automatically from the cmdtbl file,
 *	and can be found in the file nename.h
 */
typedef struct {
	const char *n_name;
	const CMDFUNC *n_cmd;
}	NTAB;

/*
 * a binary search tree of the above structure.  we use this so that we can
 * add in procedures as they are created.
 */
typedef struct {
	const char *bi_key;		/* the name of the command	*/
	const CMDFUNC *n_cmd;		/* command details		*/
	UCHAR n_flags;			/* flags (below)		*/
#define	NBST_READONLY	1		/* for builtin functions	*/
#define	NBST_DONE	2		/* temporary flag used by
					   bind.c:makebindlist()	*/
}	NBST_DATA;


/* when a command is referenced by bound key (like h,j,k,l, or "dd"), it
 *	is looked up one of two ways: single character 8-bit ascii commands (by
 *	far the majority) are simply indexed into an array of CMDFUNC pointers.
 *	Other commands (those with ^A, ^X, or SPEC prefixes) are searched for
 *	in a binding table, made up of KBIND structures.  This structure
 *	contains the command code, and again, a pointer to the CMDFUNC
 *	structure for the command
 *
 *	The asciitbl array, and the kbindtbl array are generated automatically
 *	from the cmdtbl file, and can be found in the file nebind.h
 */

#if OPT_REBIND
#define KBIND_LINK(code) ,code
#else
#define KBIND_LINK(code) /*nothing*/
#endif

typedef struct  k_bind {
	int	k_code;			/* Key code			*/
	const CMDFUNC *k_cmd;
#if OPT_REBIND
	struct  k_bind *k_link;
#endif
}	KBIND;

typedef struct {
	const char *bufname;
	const CMDFUNC **kb_normal;
	KBIND *kb_special;
#if OPT_REBIND
	KBIND *kb_extra;
#endif
} BINDINGS;

#define DefaultKeyBinding(c) kcod2fnc(&dft_bindings, c)
#define InsertKeyBinding(c)  kcod2fnc(&ins_bindings, c)
#define CommandKeyBinding(c) kcod2fnc(&cmd_bindings, c)
#define SelectKeyBinding(c)  kcod2fnc(&sel_bindings, c)

/* These are the flags which can appear in the CMDFUNC structure, describing a
 * command.
 */
#define NONE    0L
#define cmdBIT(n) lBIT(n)	/* ...to simplify typing */
/* bits 0-12 */
#define UNDO    cmdBIT(0)	/* command is undo-able, so clean up undo lists */
#define REDO    cmdBIT(1)	/* command is redo-able, record it for dotcmd */
#define MOTION  cmdBIT(2)	/* command causes motion, okay after operator cmds */
#define FL      cmdBIT(3)	/* if command causes motion, opers act on full lines */
#define ABSM    cmdBIT(4)	/* command causes absolute (i.e. non-relative) motion */
#define GOAL    cmdBIT(5)	/* column goal should be retained */
#define GLOBOK  cmdBIT(6)	/* permitted after global command */
#define OPER    cmdBIT(7)	/* function is an operator, affects a region */
#define LISTED  cmdBIT(8)	/* internal use only -- used in describing
				 * bindings to only describe each once */
#define NOMOVE  cmdBIT(9)	/* dot doesn't move (although address may be used) */
#define VIEWOK  cmdBIT(10)	/* command is okay in view mode, even though it
				 * _may_ be undoable (macros and maps) */
#define VL_RECT cmdBIT(11)	/* motion causes rectangular operation */
#define MINIBUF cmdBIT(12)	/* may use in minibuffer edit */

/* These flags are 'ex' argument descriptors, adapted from elvis.  Not all are
 * used or honored or implemented.
 */
#define argBIT(n) cmdBIT(n+13)	/* ...to simplify adding bits */
/* bits 13-27 */
#define FROM    argBIT(0)	/* allow a linespec */
#define TO      argBIT(1)	/* allow a second linespec */
#define BANG    argBIT(2)	/* allow a ! after the command name */
#define EXTRA   argBIT(3)	/* allow extra args after command name */
#define XFILE   argBIT(4)	/* expand wildcards in extra part */
#define NOSPC   argBIT(5)	/* no spaces allowed in the extra part */
#define DFLALL  argBIT(6)	/* default file range is 1,$ */
#define DFLNONE argBIT(7)	/* no default file range */
#define NODFL   argBIT(8)	/* do not default to the current file name */
#define EXRCOK  argBIT(9)	/* can be in a .exrc file */
#define VI_NL   argBIT(10)	/* if !exmode, then write a newline first */
#define PLUS    argBIT(11)	/* allow a line number, as in ":e +32 foo" */
#define ZERO    argBIT(12)	/* allow 0 to be given as a line number */
#define OPTREG  argBIT(13)	/* allow optional register-name */
#define USEREG  argBIT(14)	/* expect register-name */
#define FROM_TO argBIT(15)	/* % is all lines */
#define FILES   (XFILE | EXTRA)	/* multiple extra files allowed */
#define WORD1   (EXTRA | NOSPC)	/* one extra word allowed */
#define FILE1   (FILES | NOSPC)	/* 1 file allowed, defaults to current file */
#define NAMEDF  (FILE1 | NODFL)	/* 1 file allowed, defaults to "" */
#define NAMEDFS (FILES | NODFL)	/* multiple files allowed, default is "" */
#define RANGE   (FROM  | TO)	/* range of linespecs allowed */

/* these flags determine the type of cu.* */
#define typBIT(n) cmdBIT(n+29)	/* ...to simplify adding bits */
/* bits 29-31 */
#define CMD_FUNC 0L		/* this is the default (CmdFunc) */
#define CMD_PROC typBIT(0)	/* named procedure (BUFFER *) */
#define CMD_OPER typBIT(1)	/* user-defined operator */
#define CMD_PERL typBIT(2)	/* perl subroutine (AV *) */
#define CMD_TYPE (CMD_PROC | CMD_OPER | CMD_PERL) /* type mask */

#define SPECIAL_BANG_ARG -42	/* arg passed as 'n' to functions which
					were invoked by their "xxx!" name */

/* definitions for fileio.c's ffXXX() functions */
typedef enum {
	file_is_closed
	,file_is_unbuffered
	,file_is_external
	,file_is_pipe
	,file_is_internal
	,file_is_new          /* winvile-specific functionality */
} FFType;

/* definitions for 'mlreply_file()' and other filename-completion */
#define	FILEC_WRITE    1
#define	FILEC_WRITE2   2
#define	FILEC_UNKNOWN  3
#define	FILEC_READ     4
#define	FILEC_REREAD   5

#define	FILEC_PROMPT   8	/* always prompt (never from screen) */
#define	FILEC_EXPAND   16	/* allow glob-expansion to multiple files */

#ifndef P_tmpdir		/* not all systems define this */
#if SYS_MSDOS || SYS_OS2 || SYS_WINNT
#define P_tmpdir ""
#endif
#if SYS_UNIX
#define P_tmpdir "/usr/tmp"
#endif
#if SYS_VMS
#define P_tmpdir "sys$scratch:"
#endif
#endif	/* P_tmpdir */

#undef TMPDIR

#if OPT_EVAL
#define TMPDIR get_directory()
#else
#define TMPDIR P_tmpdir		/* defined in <stdio.h> */
#endif	/* OPT_EVAL */

/*	The editor holds deleted text chunks in the KILL registers. The
	kill registers are logically a stream of ascii characters, however
	due to unpredictable size, are implemented as a linked
	list of chunks. (The d_ prefix is for "deleted" text, as k_
	was taken up by the keycode structure.)
*/

typedef	struct KILL {
	struct KILL *d_next;	/* link to next chunk, NULL if last */
	UCHAR d_chunk[KBLOCK];	/* deleted text */
} KILL;

typedef struct KILLREG {
	struct KILL *kbufp;	/* current kill register chunk pointer */
	struct KILL *kbufh;	/* kill register header pointer	*/
	unsigned kused;		/* # of bytes used in kill last chunk	*/
	C_NUM kbwidth;		/* width of chunk, if rectangle */
	USHORT kbflag;		/* flags describing kill register	*/
} KILLREG;

#define	KbSize(i,p)	((p->d_next != NULL) ? KBLOCK : kbs[i].kused)

#ifndef NULL
# define NULL 0
#endif

/*
 * General purpose includes
 */

#include <stdarg.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if SYS_VMS
#include <unixio.h>
#include <unixlib.h>
#include <file.h>	/* aka <sys/file.h> */
#include <rms.h>	/* required to compile nefsms.h */
#define stricmp(a,b)	strcasecmp(a,b)
#define strnicmp(a,b,n)	strncasecmp(a,b,n)
#elif OPT_VMS_PATH
#define strnicmp(a,b,n)	strncasecmp(a,b,n)
#endif

#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#else
extern void exit (int code);
extern void _exit (int code);
#endif	/* HAVE_STDLIB_H */

#if !(defined(intptr_t))
/* get intptr_t */
#if defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include <stdint.h>
#elif !(defined(WIN32) || defined(WIN32_LEAN_AND_MEAN))
typedef long intptr_t;
#endif
#endif

/* array/table size */
#define	TABLESIZE(v)	(sizeof(v)/sizeof(v[0]))

#define PERCENT(num,den) ((den) ? (int)((100.0 * (num))/(den)) : 100)

/* Quiet compiler warnings on places where we're being blamed incorrectly,
 * e.g., for casting away const, or for alignment problems.
 */
#ifdef __GNUC__
#define TYPECAST(type,ptr) (type*)((intptr_t)(ptr))
#else
#define TYPECAST(type,ptr) (type*)(ptr)
#endif

/*
 * We cannot define these in config.h, since they require parameters to be
 * passed (that's non-portable).
 */
#ifdef __cplusplus
#undef GCC_PRINTF
#undef GCC_NORETURN
#undef GCC_UNUSED
#endif

#ifndef GCC_PRINTFLIKE
#ifdef GCC_PRINTF
#define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))
#else
#define GCC_PRINTFLIKE(fmt,var) /*nothing*/
#endif
#endif /* GCC_PRINTFLIKE */

#ifndef	GCC_NORETURN
#define	GCC_NORETURN /* nothing */
#endif

#ifndef	GCC_UNUSED
#define	GCC_UNUSED /* nothing */
#endif

#if defined(VILE_ERROR_ABORT)
extern void ExitProgram(int code) GCC_NORETURN;
#endif

#if SYS_WINNT && defined(VILE_OLE) && DISP_NTWIN
#define ExitProgram(code)   oleauto_exit(code)
#else
#if !defined(VILE_ERROR_ABORT)
#define	ExitProgram(code)	exit_program(code)
#endif
#endif

#if 1				/* requires a patch to gcc */
#define VILE_PRINTF(fmt,var) GCC_PRINTFLIKE(fmt,var)
#else
#define VILE_PRINTF(fmt,var) /*nothing*/
#endif

#ifdef HAVE_SELECT
# ifdef HAVE_SELECT_H
# include <select.h>
# endif
# ifdef USE_SYS_SELECT_H
# include <sys/select.h>
# endif
#endif

#ifdef HAVE_UTIME_H
# include <utime.h>
#endif

#ifdef HAVE_SYS_UTIME_H
# include <sys/utime.h>
#endif

/*
 * If we have va_copy(), use it.
 */
#if defined(HAVE___VA_COPY)
#define begin_va_copy(dst,src)	__va_copy(dst, src)
#define end_va_copy(dst)	va_end(dst)
#elif defined(va_copy) || defined(HAVE_VA_COPY)
#define begin_va_copy(dst,src)	va_copy(dst, src)
#define end_va_copy(dst)	va_end(dst)
#else
#define begin_va_copy(dst,src) (dst) = (src)
#define end_va_copy(dst)	/* nothing */
#endif

/*
 * We will make the default unix globbing code use 'echo' rather than our
 * internal globber if we do not configure the 'glob' string-mode.
 */
#if SYS_MSDOS || SYS_OS2 || SYS_WINNT
# define UNIX_GLOBBING OPT_GLOB_ENVIRON
#endif

#if SYS_UNIX && defined(GVAL_GLOB) && !OPT_VMS_PATH
# define UNIX_GLOBBING 1
#endif

#ifndef UNIX_GLOBBING
# define UNIX_GLOBBING 0
#endif

/*
 * Debugging/memory-leak testing
 */

#ifndef	DOALLOC		/* record info for 'show_alloc()' */
#define	DOALLOC		0
#endif
#ifndef	USE_DBMALLOC	/* test malloc/free/strcpy/memcpy, etc. */
#define	USE_DBMALLOC	0
#endif
#ifndef	USE_DMALLOC	/* test malloc/free/strcpy/memcpy, etc. */
#define	USE_DMALLOC	0
#endif
#ifndef	USE_MPATROL	/* test malloc/free/strcpy/memcpy, etc. */
#define	USE_MPATROL	0
#endif
#ifndef	NO_LEAKS	/* free permanent memory, analyze leaks */
#define	NO_LEAKS	0
#endif
#ifndef TEST_DOS_PIPES
#define TEST_DOS_PIPES	0
#endif

/* heap size tracking */
#if	OPT_HEAPSIZE
#undef	realloc
#define	realloc	track_realloc
#undef	calloc
#define	calloc(n,m)	track_malloc((n)*(m))
#undef	malloc
#define	malloc	track_malloc
#undef	free
#define	free	track_free
#endif


#undef TRACE

#if USE_DBMALLOC || USE_DMALLOC
#  undef strchr
#  undef strrchr
#  undef memcpy
#  undef memccpy
#  undef malloc
#  undef realloc
#  undef free
#  define strmalloc strdup
#  if USE_DBMALLOC
#    include <dbmalloc.h>		/* renamed from dbmalloc's convention */
#    define show_alloc() malloc_dump(fileno(stderr))
#  endif
#  if USE_DMALLOC
#    include <dmalloc.h>
#    define show_alloc() dmalloc_log_unfreed()
#  endif
#  if USE_MPATROL
#    include <mpatrol.h>
#    define show_alloc() void __mp_summary()
#  endif
#  if CAN_TRACE && OPT_TRACE
#    include <trace.h>
#  endif
#elif CAN_TRACE
#  if (NO_LEAKS || DOALLOC || OPT_TRACE)
#    include <trace.h>
#  elif !defined(show_alloc) && NO_LEAKS
#    include <trace.h>
#  endif
#endif	/* USE_DBMALLOC */

#ifndef init_alloc
#define init_alloc(s,n) /* nothing */
#endif

#if OPT_ELAPSED && OPT_TRACE
extern void show_elapsed(void);
#else
#define show_elapsed() /* nothing */
#endif

/* extra checking if we're tracing */
#if !OPT_TRACE
#undef  NDEBUG
#define NDEBUG			/* turn off assert's */
#define valid_buffer(bp)        ((bp) != NULL)
#define valid_window(wp)        ((wp) != NULL)
#define valid_line_bp(lp,bp)    ((lp) != NULL)
#define valid_line_wp(lp,wp)    ((lp) != NULL)
#endif

/* this must be after NDEBUG is defined */
#include <assert.h>

/* Normally defined in "trace.h" */
#ifndef TRACE
#define TRACE(p) /* nothing */
#define returnCString(c) return(c)
#define returnCode(c)    return(c)
#define returnPtr(c)     return(c)
#define returnString(c)  return(c)
#define returnVoid()     return
#endif

#if OPT_TRACE > 1
#define TRACE2(params)    TRACE(params)
#define return2CString(c) returnCString(c)
#define return2Code(c)    returnCode(c)
#define return2Ptr(c)     returnPtr(c)
#define return2String(c)  returnString(c)
#define return2Void()     returnVoid()
#else
#define TRACE2(params) /*nothing*/
#define return2CString(c) return(c)
#define return2Code(c)    return(c)
#define return2Ptr(c)     return(c)
#define return2String(c)  return(c)
#define return2Void()     return
#endif

#if OPT_TRACE > 2
#define TRACE3(params)    TRACE(params)
#else
#define TRACE3(params) /*nothing*/
#endif

#if OPT_EVAL || OPT_DEBUGMACROS
#define TPRINTF(p) { TRACE(p); if (tracemacros) tprintf p; }
#else
#define TPRINTF(p) /* nothing */
#endif

#if DISP_X11 && defined(NEED_X_INCLUDES)
#include	<X11/Intrinsic.h>
#include	<X11/StringDefs.h>
#endif

/*
 * workarounds for defective versions of gcc, e.g., defining a random set of
 * names as "built-in".
 */
#if defined(__GNUC__) && defined(__GNUC_MINOR__)
#if (__GNUC__ == 3 && __GNUC_MINOR__ == 3)
#define exp vl_exp	/* gcc 3.3 */
#endif
#endif /* gcc workarounds */

#if defined(__GNUC__) && defined(_FORTIFY_SOURCE)
#define IGNORE_RC(func) ignore_unused = (int) func
#else
#define IGNORE_RC(func) (void) func
#endif /* gcc workarounds */

/*
 * Local prototypes (must follow NO_LEAKS definition)
 */

#if	!CHECK_PROTOTYPES
#include "neproto.h"
#include "proto.h"
#endif

/*
 * the list of generic function key bindings
 */
#if	!CHECK_PROTOTYPES
#include "nefkeys.h"
#endif

/* for debugging VMS pathnames on UNIX... */
#if CAN_VMS_PATH && (SYS_UNIX && OPT_VMS_PATH)
#include "fakevms.h"
#endif

/* *INDENT-ON* */

#endif /* _estruct_h */
