/*	ESTRUCT:	Structure and preprocessor defines for
			vile.  Reshaped from the original, which
			was for MicroEMACS 3.9

			vile is by Paul Fox
			MicroEmacs was written by Dave G. Conroy
			modified by Steve Wilhite, George Jones
			substantially modified by Daniel Lawrence
*/

/*
 * $Header: /users/source/archives/vile.vcs/RCS/estruct.h,v 1.339 1998/03/22 12:06:15 kev Exp $
 */

#ifndef _estruct_h
#define _estruct_h 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef CHECK_PROTOTYPES
#define CHECK_PROTOTYPES 0
#endif

#ifndef os_chosen

/* All variants of UNIX should now be handled by the configure script */

#ifdef VMS		/* predefined by VAX/VMS compiler */
# define scrn_chosen
# define DISP_VMSVT  1
#endif

/* non-unix flavors */
#undef SYS_MSDOS
#undef SYS_WIN31

#define SYS_MSDOS	0			/* MS-DOS			*/
#define SYS_WIN31	0			/* Windows 3.1			*/
#define SYS_OS2		0			/* son of DOS			*/
#define SYS_OS2_EMX	0			/* Unix'ed OS2			*/
#define SYS_WINNT	0			/* Windows/NT			*/

/*	Compiler definitions			*/
#define CC_MSC		0	/* Microsoft C compile version 3 & 4 & 5 & 6 */
#define CC_TURBO	0	/* Turbo C/MSDOS or Borland C++ */
#define CC_WATCOM	0	/* WATCOM C/386 version 9.0 or above */
#define CC_DJGPP	0	/* DJ's GCC version 1.09 */
#define CC_CSETPP	0	/* IBM C Set ++ 2.x */
#define CC_MSVC		0	/* Microsoft Visual C++ 7 & 8 & 9 */

#ifdef __TURBOC__	/* predefined in Turbo C, both DOS and Windows */
# ifdef __FLAT__	/* predefined in Borland C for WIN32 */
#  undef SYS_WINNT
#  define SYS_WINNT  1
# else
#  ifdef _Windows	/* predefined in TurboC for Windows */
#   undef SYS_WIN31
#   define SYS_WIN31  1
#  else
#   undef SYS_MSDOS
#   define SYS_MSDOS  1
#  endif
# endif
# undef CC_TURBO
# define CC_TURBO  1
#endif

#ifdef _WIN32		/* predefined in Visual C/C++ 4.0 */
# undef SYS_WINNT
# define SYS_WINNT 1
#endif

#ifdef __WATCOMC__
#undef SYS_MSDOS
#define SYS_MSDOS  1
#undef CC_WATCOM
#define CC_WATCOM 1
#endif

#ifdef __IBMC__
# if __IBMC__ >= 200	/* IBM C Set ++ version 2.x */
#  undef  CC_CSETPP
#  define CC_CSETPP 1
# endif
#endif

#ifdef __OS2__
/* assume compiler already chosen */
#undef SYS_OS2
#undef SYS_MSDOS
#define SYS_OS2    1
#define SYS_MSDOS  0
#endif

#ifdef __GO32__  	/* DJ's GCC version 1.09 */
#undef SYS_MSDOS
#define SYS_MSDOS  1
#undef CC_DJGPP
#define CC_DJGPP   1
#endif

#ifdef _MSC_VER
#undef SYS_WINNT
#define SYS_WINNT	1
#undef CC_MSVC
#define CC_MSVC		1
#endif

/* As of version 3.51 of vile, CC_NEWDOSCC should be correct for Turbo,
 * Watcom, and the DJ gcc (GO32) compilers.  I'm betting that it's also
 * probably correct for MSC (Microsoft C) and ZTC (Zortech), but I'm not
 * sure of those.  (It implies a lot of ANSI and POSIX behavior.)
 */
#if CC_TURBO || CC_WATCOM || CC_MSC || CC_DJGPP || SYS_WINNT || CC_CSETPP || CC_MSVC
# define CC_NEWDOSCC 1
# define HAVE_GETCWD 1
#else
# define CC_NEWDOSCC 0
#endif

#if CC_TURBO
# if !SYS_WINNT
#  define off_t long		/* missing in TurboC 3.1 */
# endif
#endif

#if CC_CSETPP
# define HAVE_UTIME		1
# define HAVE_SYS_UTIME_H	1
# define CPP_SUBS_BEFORE_QUOTE	1
# define HAVE_LOSING_SWITCH_WITH_STRUCTURE_OFFSET	1
#endif

#if CC_DJGPP
# define HAVE_UNISTD_H		1
#endif

#endif /* os_chosen */

/*
 * This may apply to makefile.emx, or to builds with the configure-script
 */
#ifdef __EMX__
#define SYS_OS2_EMX 1
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

#ifndef HAVE_QSORT
# define HAVE_QSORT	1	/* if your system has the qsort() system call */
#endif

#ifndef HAVE_UTIME
# define HAVE_UTIME	0	/* if your system has the utime() system call */
#endif

#ifndef HAVE_SETJMP_H
# define HAVE_SETJMP_H  1	/* if your system has <setjmp.h> */
#endif

#ifndef HAVE_SIGNAL_H
# define HAVE_SIGNAL_H  1	/* if your system has <signal.h> */
#endif

#if !(defined(HAVE_STRCHR) || defined(HAVE_STRRCHR))
# define HAVE_STRCHR    1
# define HAVE_STRRCHR   1
#endif

#ifndef HAVE_STDLIB_H
# define HAVE_STDLIB_H  1	/* if your system has <stdlib.h> */
#endif

#ifndef HAVE_STRING_H
# define HAVE_STRING_H  1	/* if your system has <string.h> */
#endif

/* Some code uses these as values in expressions, so we always want them
 * defined, just in case we run into a substandard preprocessor.
 */
#ifndef DISP_X11
# define DISP_X11 0
#endif
#ifndef SYS_MSDOS
# define SYS_MSDOS 0
#endif
#ifndef SYS_OS2
# define SYS_OS2 0
#endif
#ifndef SYS_WIN31
# define SYS_WIN31 0
#endif
#ifndef SYS_WINNT
# define SYS_WINNT 	0
#endif
#if defined(VMS) || defined(__VMS) /* predefined by DEC's VMS compilers */
# define SYS_VMS    1
# define HAVE_GETCWD 1
# if defined(__DECC) && !defined(__alpha)
#  undef HAVE_ACCESS
#  define HAVE_ACCESS 0	/* 'access()' is reported to not work properly */
# endif
# if !defined(__DECC)
#  undef  HAVE_QSORT
#  define HAVE_QSORT 0	/* VAX-C's 'qsort()' is definitely broken */
# endif
# define SIGT void
# define SIGRET
#else
# define SYS_VMS    0
#endif
#ifdef apollo
# define SYS_APOLLO 1	/* FIXME: still more ifdefs to autoconf */
#endif
#ifdef sun
# define SYS_SUNOS 1	/* FIXME: need to tweak lint ifdefs */
#endif

#define IBM_KBD 	(SYS_MSDOS || SYS_OS2 || SYS_WINNT)
#define IBM_VIDEO 	(SYS_MSDOS || SYS_OS2 || SYS_WINNT)
#define CRLF_LINES 	(SYS_MSDOS || SYS_OS2 || SYS_WINNT)

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>	/* defines off_t */

#if HAVE_LIBC_H
#include <libc.h>
#endif
#if HAVE_FCNTL_H
#ifndef O_RDONLY	/* prevent multiple inclusion on lame systems */
#include <fcntl.h>	/* 'open()' */
#endif
#endif

#if HAVE_SYS_TIME_H && ! SYSTEM_LOOKS_LIKE_SCO
/* on SCO, sys/time.h conflicts with select.h, and we don't need it */
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
# include <time.h>
#endif
#else
#include <time.h>
#endif

#if HAVE_SYS_RESOURCE_H && ! SYSTEM_LOOKS_LIKE_SCO
/* On SunOS, struct rusage is referred to in <sys/wait.h>.  struct rusage
   is defined in <sys/resource.h>.   NeXT may be similar.  On SCO,
   resource.h needs time.h, which we excluded above.  */
#include <sys/resource.h>
#endif

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>	/* 'wait()' */
#endif

/* definitions for testing apollo version with gcc warnings */
#if SYS_APOLLO
# ifdef __GNUC__		/* only tested for SR10.3 with gcc 1.36 */
#  ifndef _APOLLO_SOURCE	/* ...defined in gcc 2.4.5 */
#  define _APOLLO_SOURCE	/* appease gcc by forcing extern-defs */
#  endif
#  define __attribute(s)
#  define APOLLO_STDLIB 1
# endif
# if defined(L_tmpnam)		/* SR10.3, CC 6.8 defines in <stdio.h> */
#  define APOLLO_STDLIB 1
# endif
#endif

#ifndef APOLLO_STDLIB
# define APOLLO_STDLIB 0
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

#if HAVE_SIGNAL_H
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

#if HAVE_SETJMP_H
#include	<setjmp.h>
#endif

/* argument for 'exit()' or '_exit()' */
#if SYS_VMS
#include	<stsdef.h>
#define GOODEXIT	(STS$M_INHIB_MSG | STS$K_SUCCESS)
#define BADEXIT		(STS$M_INHIB_MSG | STS$K_ERROR)
#else
#define GOODEXIT	0
#define BADEXIT		1
#endif

/* has the select() or poll() call, only used for short sleeps in fmatch() */
#if HAVE_SELECT
#undef HAVE_POLL
#endif

#if SYS_UNIX || SYS_MSDOS || SYS_WIN31 || SYS_VMS || SYS_OS2 || SYS_WINNT
#define	ENVFUNC	1
#else
#define	ENVFUNC	0
#endif

/* ====================================================================== */
#ifndef scrn_chosen
/*	Terminal Output definitions		*/
/* choose ONLY one of the following */
#define DISP_TERMCAP	SYS_UNIX	/* Use TERMCAP			*/
#define DISP_ANSI	0		/* ANSI escape sequences	*/
#define DISP_AT386	0		/* AT style 386 unix console	*/
#define	DISP_VMSVT	SYS_VMS		/* various VMS terminal entries	*/
#define	DISP_BORLAND	0		/* Borland console I/O routines */
#define	DISP_IBMPC	(SYS_MSDOS && !DISP_BORLAND && !DISP_ANSI) /* IBM-PC CGA/MONO/EGA driver */
#define	DISP_X11	0		/* X Window System */
#define DISP_NTCONS	0		/* Windows/NT console		*/
#define DISP_NTWIN	0		/* Windows/NT screen/windows	*/
#define DISP_VIO	SYS_OS2		/* OS/2 VIO (text mode)		*/
#define	DISP_HP150	0		/* (not supported)		*/
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

#ifndef OPT_SHELL
#define OPT_SHELL 1	/* we'll disable this only as an exercise */
#endif

#ifndef OPT_LOCALE
#define OPT_LOCALE 0
#endif

/* various color stuff */
#define	OPT_COLOR (DISP_ANSI || IBM_VIDEO || DISP_TERMCAP)
#define	OPT_16_COLOR (IBM_VIDEO || DISP_TERMCAP)

#define OPT_DUMBTERM (DISP_TERMCAP || DISP_VMSVT)

/* Feature turnon/turnoff */
#define	OPT_DOSFILES	1	/* turn on code for DOS mode (lines that
				   end in crlf).  use DOSFILES, for instance,
				   if you edit DOS-created files under UNIX   */
#define	OPT_REVSTA	1	/* Status line appears in reverse video       */
#define	OPT_CFENCE	1	/* do fence matching in CMODE		      */
#define	OPT_LCKFILES	0	/* create lock files (file.lck style) 	      */
#define	OPT_TAGS	1	/* tags support  			      */
#define	OPT_PROCEDURES	1	/* named procedures			      */
#define	OPT_PATHLOOKUP	1	/* search $PATH for startup and help files    */
#define	OPT_SCROLLCODE	1	/* code in display.c for scrolling the screen.
				   Only useful if your display can scroll
				   regions, or at least insert/delete lines.
				   ANSI, TERMCAP, IBMPC, VMSVT and AT386 can
				   do this */
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
#define OPT_ICURSOR	0	/* use an insertion cursor if possible */
#define OPT_W32PIPES    0	/* SYS_WINNT: pipes (experimental) */

#ifndef OPT_EXEC_MACROS		/* total numbered macros (see mktbls.c) */
#if SMALLER
#define OPT_EXEC_MACROS 10
#else
#define OPT_EXEC_MACROS 40
#endif
#endif

  /* NOTE:  OPT_ICURSOR is _only_ supported by borland.c for a PC build
     and ntconio.c for a win32 build!! */
#define OPT_TITLE	(SYS_WINNT)		/* use a window title */

/* the "working..." message -- we must have the alarm() syscall, and
   system calls must be restartable after an interrupt by default or be
   made restartable with sigaction() */
#define OPT_WORKING (!SMALLER && HAVE_ALARM && HAVE_RESTARTABLE_PIPEREAD)

#define OPT_SCROLLBARS XTOOLKIT			/* scrollbars */
#define OPT_VMS_PATH    (SYS_VMS)  /* vax/vms path parsing (testing/porting)*/

/* systems with MSDOS-like filename syntax */
#define OPT_MSDOS_PATH  (SYS_MSDOS || SYS_WIN31 || SYS_OS2 || SYS_WINNT)
#define OPT_CASELESS	(SYS_WINNT || SYS_OS2 || SYS_OS2_EMX)
#define OPT_UNC_PATH	(SYS_WINNT)

/* individual features that are (normally) controlled by SMALLER */
#define OPT_AEDIT       !SMALLER		/* advanced editing options: e.g. en/detabbing	*/
#define OPT_B_LIMITS    !SMALLER		/* left-margin */
#define OPT_ENCRYPT     !SMALLER /* file encryption (not crypt(1) compatible!) */
#define OPT_ENUM_MODES  !SMALLER		/* fixed-string modes */
#define OPT_EVAL        !SMALLER		/* expression-evaluation */
#define OPT_FILEBACK    !SMALLER && !SYS_VMS	/* file backup style */
#define OPT_FINDERR     !SMALLER		/* finderr support. */
#define OPT_FLASH       !SMALLER || DISP_IBMPC	/* visible-bell */
#define OPT_FORMAT      !SMALLER		/* region formatting support. */
#define OPT_HILITEMATCH !SMALLER		/* highlight all matches of a search */
#define OPT_HISTORY     !SMALLER		/* command-history */
#define OPT_ISO_8859    !SMALLER		/* ISO 8859 characters */
#define OPT_ISRCH       !SMALLER		/* Incremental searches */
#define OPT_LINEWRAP    !SMALLER		/* line-wrap mode */
#define OPT_MAJORMODE   !SMALLER		/* majormode support */
#define OPT_MLFORMAT    !SMALLER		/* modeline-format */
#define OPT_MS_MOUSE    !SMALLER && DISP_IBMPC  /* MsDos-mouse */
#define OPT_NAMEBST     !SMALLER		/* name's stored in a bst */
#define OPT_ONLINEHELP  !SMALLER		/* short per-command help */
#define OPT_POPUPCHOICE !SMALLER		/* popup-choices mode */
#define OPT_POPUP_MSGS  !SMALLER		/* popup-msgs mode */
#define OPT_REBIND      !SMALLER		/* permit rebinding of keys at run-time	*/
#define OPT_TAGS_CMPL   !SMALLER && OPT_TAGS	/* name-completion for tags */
#define OPT_TERMCHRS    !SMALLER		/* set/show-terminal */
#define OPT_UPBUFF      !SMALLER		/* animated buffer-update */
#define OPT_WIDE_CTYPES !SMALLER		/* extra char-types tests */
#define OPT_WORDCOUNT   !SMALLER		/* "count-words" command" */

/* "show" commands for the optional features */
#define OPT_SHOW_CTYPE	!SMALLER		/* "show-printable" */
#define OPT_SHOW_EVAL   !SMALLER && OPT_EVAL	/* "show-variables" */
#define OPT_SHOW_MAPS   !SMALLER 		/* display mapping for ":map" */
#define OPT_SHOW_REGS   !SMALLER		/* "show-registers" */
#define OPT_SHOW_TAGS   !SMALLER && OPT_TAGS	/* ":tags" displays tag-stack */

/* selections and attributed regions */
#define OPT_VIDEO_ATTRS !SMALLER
#define OPT_SELECTIONS  OPT_VIDEO_ATTRS

/* OPT_PSCREEN permits a direct interface to the pscreen data structure
 * in display.c. This allows us to avoid storing the screen data on the
 * screen interface side.
 */
#define OPT_PSCREEN  (XTOOLKIT && OPT_VIDEO_ATTRS)

#if	DISP_TERMCAP && !SMALLER
/* the setting "xterm-mouse" is always available, i.e.  the modetbl entry
 * is not conditional.  but all of the code that supports xterm-mouse _is_
 * ifdefed.  this makes it easier for users to be able to put "set
 * xterm-mouse" in their .vilerc which is shared between vile and xvile.
 */
#define	OPT_XTERM	2	/* mouse-clicking support */
#else
#define	OPT_XTERM	0	/* vile doesn't recognize xterm mouse */
#endif

 	/* combine select/yank (for mouse support) */
#define OPT_SEL_YANK    ((DISP_X11 && XTOOLKIT) || SYS_WINNT || SYS_OS2)

	/* any mouse capability */
#define OPT_MOUSE       (OPT_SEL_YANK || OPT_XTERM || OPT_MS_MOUSE)

	/* menus */
#define	OPT_MENUS	(DISP_X11 && (MOTIF_WIDGETS||ATHENA_WIDGETS))

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
 * Special characters used in globbing
 */
#define	GLOB_MULTI	'*'
#define	GLOB_SINGLE	'?'
#define	GLOB_ELLIPSIS	"..."
#define	GLOB_RANGE	"[]"
#define	GLOB_NEGATE	"^!"

/*
 * Configuration options for globbing
 */
#define	OPT_GLOB_ENVIRON	ENVFUNC && !SMALLER
#define	OPT_GLOB_ELLIPSIS	SYS_VMS || SYS_UNIX || SYS_OS2 || SYS_WINNT || (SYS_MSDOS && !SMALLER)
#define	OPT_GLOB_PIPE		SYS_UNIX && OPT_SHELL
#define	OPT_GLOB_RANGE		SYS_UNIX || SYS_OS2 || SYS_WINNT || (SYS_MSDOS && !SMALLER)

/*	Debugging options	*/
#define	OPT_RAMSIZE		0  /* dynamic RAM memory usage tracking */
#define OPT_DEBUGMACROS		0  /* $debug triggers macro debugging	*/
#define	OPT_VISIBLE_MACROS	0  /* update display during keyboard macros*/

#ifndef OPT_TRACE
#define OPT_TRACE		0  /* turn on debug/trace (link with trace.o) */
#endif

/* That's the end of the user selections -- the rest is static definition */
/* (i.e. you shouldn't need to touch anything below here */
/* ====================================================================== */

#include <errno.h>
#if SYS_VMS
#include <perror.h>	/* defines 'sys_errlist[]' */
#endif
#if SYS_UNIX
# ifdef DECL_ERRNO
extern	int	errno;	/* some systems don't define this in <errno.h> */
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
#define	lBIT(n)	((ULONG)(1L<<(n)))
#define	iBIT(n) ((UINT)(1 <<(n)))

/* FIXME: leftover definitions from K&R version */
#define SIZE_T  size_t
#define ALLOC_T size_t

#ifndef HAVE_GETHOSTNAME
#define HAVE_GETHOSTNAME 0
#endif

#if !(HAVE_STRCHR && HAVE_STRRCHR)
#define USE_INDEX 1
#endif

#ifdef USE_INDEX
#define strchr index
#define strrchr rindex
#if MISSING_EXTERN_RINDEX
extern char *index (const char *s, int c);
extern char *rindex (const char *s, int c);
#endif
#endif /* USE_INDEX */

#if STDC_HEADERS || HAVE_STRING_H
# include <string.h>
  /* An ANSI string.h and pre-ANSI memory.h might conflict.  */
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif /* not STDC_HEADERS and HAVE_MEMORY_H */
#else /* not STDC_HEADERS and not HAVE_STRING_H */
# if HAVE_STRINGS_H
#  include <strings.h>
  /* memory.h and strings.h conflict on some systems */
  /* FIXME: should probably define memcpy and company in terms of bcopy,
     et al here */
# endif
#endif /* not STDC_HEADERS and not HAVE_STRING_H */

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

/*	System dependent library redefinitions, structures and includes	*/

#if CC_NEWDOSCC && ! CC_CSETPP
#include <dos.h>
#endif

#if CC_NEWDOSCC && ! CC_DJGPP && ! CC_CSETPP
#undef peek
#undef poke
#define	peek(a,b,c,d)	movedata(a,b,FP_SEG(c),FP_OFF(c),d)
#define	poke(a,b,c,d)	movedata(FP_SEG(c),FP_OFF(c),a,b,d)
#define	movmem(a, b, c)		memcpy(b, a, c)
#endif

#if  CC_WATCOM
#include      <string.h>
#endif

#if  CC_WATCOM || CC_DJGPP
#define	movmem(a, b, c)		memcpy(b, a, c)
#endif

#if CC_MSC
#include <memory.h>
#endif


/* on MS-DOS we have to open files in binary mode to see the ^Z characters. */

#if SYS_MSDOS || SYS_WIN31 || SYS_OS2 || SYS_WINNT
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

#if OPT_MSDOS_PATH	/* DOS path / to \ conversions */
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

/*	define some ability flags */

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
#if !defined(SIGWINCH) && ! OPT_WORKING
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
#if !GETPGRP_VOID || MISSING_EXTERN_GETPGRP
# define GETPGRPCALL getpgrp(0)
#else
# define GETPGRPCALL getpgrp()
#endif

#if HAVE_KILLPG
# define signal_pg(sig) killpg( GETPGRPCALL, sig)
#else
# define signal_pg(sig)   kill(-GETPGRPCALL, sig)
#endif

#if	DISP_IBMPC
#define	MEMMAP	1
#else
#define	MEMMAP	0
#endif

#if SYS_OS2
/*
 * The OS/2 toolkit defines identical typedefs for UCHAR, etc.;
 * we use those definitions to avoid trouble when using OS/2 include
 * files.
 */
# include <os2def.h>
#else
# define UCHAR	unsigned char
# define UINT	unsigned int
# define USHORT	unsigned short
# define ULONG	unsigned long
#endif

/*	internal constants	*/

#if SYS_MSDOS || SYS_WIN31
#define	BITS_PER_INT	16
#endif

#ifndef	BITS_PER_INT
#define	BITS_PER_INT	32
#endif

#ifdef  MAXPATHLEN			/* usually in <sys/param.h>	*/
#define NFILEN	MAXPATHLEN		/* # of bytes, file name	*/
#else
#define NFILEN	256			/* # of bytes, file name	*/
#endif
#define NBUFN	21			/* # of bytes, buffername, incl. null*/
#define NLINE	256			/* # of bytes, input line	*/
#define	NSTRING	128			/* # of bytes, string buffers	*/
#define NPAT	128			/* # of bytes, pattern		*/
#define HUGE	(1<<(BITS_PER_INT-2))	/* Huge number			*/
#define	NLOCKS	100			/* max # of file locks active	*/
#if DISP_X11 || DISP_TERMCAP || IBM_VIDEO
#define	NCOLORS	16			/* number of supported colors	*/
#else
#define	NCOLORS	8			/* number of supported colors	*/
#endif
#define	KBLOCK	256			/* sizeof kill buffer chunks	*/
#if !OPT_SELECTIONS
#define	NKREGS	37			/* number of kill buffers	*/
#define KEYST_KREG (NKREGS-1)
#else
#define NKREGS	39			/* When selections are enabled, we 
					 * allocate an extra kill buffer for
					 * the current selection and another 
					 * for the clipboard. 
					 */
#define CLIP_KREG (NKREGS-1) 
#define SEL_KREG (NKREGS-2) 
#define KEYST_KREG (NKREGS-3) 
#endif
#define	NBLOCK	16			/* line block chunk size	*/
#define MINWLNS	3			/* min # lines, window/screen	*/
#define MAXROWS	200			/* max # lines per screen	*/
#define MAXCOLS	200			/* max # cols per screen	*/

#define C_BLACK 0
#define C_WHITE (ncolors-1)

#define N_chars 256		/* must be a power-of-2		*/
#define HIGHBIT	0x0080		/* the meta bit			*/
#define CTLA	0x0100		/* ^A flag, or'ed in		*/
#define CTLX	0x0200		/* ^X flag, or'ed in		*/
#define SPEC	0x0400		/* special key (function keys)	*/
#define NOREMAP	0x0800		/* unremappable */
#define YESREMAP 0x1000		/* override noremap */
#define REMAPFLAGS (NOREMAP|YESREMAP)

#define kcod2key(c)	((c) & (UINT)(N_chars-1)) /* strip off the above prefixes */
#define	isspecial(c)	(((UINT)(c) & (UINT)~(N_chars-1)) != 0)

#define	char2int(c)	((int)(c & 0xff)) /* mask off sign-extension, etc. */

#define	PLURAL(n)	((n!=1)?"s":"")

#define	EOS     '\0'

/* protect against losing namespaces */
#undef	FALSE
#undef	TRUE
#undef	ABORT
#undef	FAILED
#undef	SORTOFTRUE

#define FALSE	0			/* False, no, bad, etc. 	*/
#define TRUE	1			/* True, yes, good, etc.	*/
#define ABORT	2			/* Death, ESC, abort, etc.	*/
#define	FAILED	3			/* not-quite fatal false return	*/
#define	SORTOFTRUE	4		/* really!	*/

#define	STOP	0			/* keyboard macro not in use	*/
#define	PLAY	1			/*	"     "	  playing	*/
#define	RECORD	2			/*	"     "   recording	*/

#define QUOTED	TRUE
#define NOQUOTED	FALSE

#define DOMAP	TRUE
#define NODOMAP	FALSE

/* values for regionshape */
typedef enum {
	EXACT,
	FULLLINE,
	RECTANGLE
} REGIONSHAPE;

/* flook options */
#define FL_EXECABLE  iBIT(0)	/* same as X_OK */
#define FL_WRITEABLE iBIT(1)	/* same as W_OK */
#define FL_READABLE  iBIT(2)	/* same as R_OK */
#define FL_HERE      iBIT(3)	/* look in current directory */
#define FL_HOME      iBIT(4)	/* look in home directory */
#define FL_EXECDIR   iBIT(5)	/* look in execution directory */
#define FL_TABLE     iBIT(6)	/* look in table */
#define FL_PATH      iBIT(7)	/* look along execution-path */

#define FL_ANYWHERE  (FL_HERE|FL_HOME|FL_EXECDIR|FL_TABLE|FL_PATH)

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

/* default option for 'mlreply' (used in modes.c also) */
#if !(SYS_MSDOS || SYS_WIN31 || SYS_OS2 || SYS_WINNT)
#define	KBD_NORMAL	KBD_EXPAND|KBD_QUOTES
#else
#define	KBD_NORMAL	KBD_EXPAND
#endif

/* reserve space for ram-usage option */
#if OPT_RAMSIZE
#define	LastMsgCol	(term.t_ncol - 10)
#else
#define	LastMsgCol	(term.t_ncol - 1)
#endif

/*
 * directions for the scan routines.
 */
#define	FORWARD	0			/* forward direction		*/
#define REVERSE	1			/* backwards direction		*/

	/* nonfatal codes */
#define FIOFUN  -1			/* File I/O, eod of file/bad line*/
#define FIOSUC  0			/* File I/O, success.		*/
#define FIOEOF  1			/* File I/O, end of file.	*/
	/* error codes */
#define FIOFNF  2			/* File I/O, file not found.	*/
#define FIOERR  3			/* File I/O, error.		*/
#define FIOMEM  4			/* File I/O, out of memory	*/
#define FIOABRT 5			/* File I/O, aborted		*/

/* three flavors of insert mode	*/
/* it's FALSE, or one of:	*/
#define INSERT 1
#define OVERWRITE 2
#define REPLACECHAR 3

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

/* define these so C-fence matching doesn't get confused when we're editing
	the cfence code itself */
#define LBRACE '{'
#define RBRACE '}'
#define LPAREN '('
#define RPAREN ')'
#define LBRACK '['
#define RBRACK ']'

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
#endif

#if OPT_MSDOS_PATH
#define	PATHCHR	';'
#endif

#ifndef PATHCHR				/* e.g., UNIX */
#define	PATHCHR	':'
#endif

/* how big is the ascii rep. of an int? */
#define	INTWIDTH	sizeof(int) * 3

/*	Macro argument token types					*/

#define	TKNUL	0			/* end-of-string		*/
#define	TKARG	1			/* interactive argument		*/
#define	TKBUF	2			/* buffer argument		*/
#define	TKVAR	3			/* user variables		*/
#define	TKENV	4			/* environment variables	*/
#define	TKFUN	5			/* function....			*/
#define	TKDIR	6			/* directive			*/
#define	TKLBL	7			/* line label			*/
#define	TKLIT	8			/* numeric literal		*/
#define	TKSTR	9			/* quoted string literal	*/
#define	TKCMD	10			/* command name			*/

/*	Internal defined functions					*/

#define	nextab(a)	(((a / curtabval) + 1) * curtabval)
#define	nextsw(a)	(((a / curswval) + 1) * curswval)

#define NEXT_COLUMN(col, c, list, tabs) \
		((c == '\t' && !list) \
		 ? (col + tabs - (col % tabs)) \
		 : (	(isPrint(c)) \
			? (col + 1) \
			: (col + ((c & HIGHBIT) ? 4 : 2))))

/* these are the bits that go into the _chartypes_ array */
/* the macros below test for them */
#if OPT_WIDE_CTYPES
#define chrBIT(n) lBIT(n)
#else
#define chrBIT(n) iBIT(n)
#endif

#define _upper    chrBIT(0)		/* upper case */
#define _lower    chrBIT(1)		/* lower case */
#define _digit    chrBIT(2)		/* digits */
#define _space    chrBIT(3)		/* whitespace */
#define _bspace   chrBIT(4)		/* backspace character (^H, DEL, and user's) */
#define _cntrl    chrBIT(5)		/* control characters, including DEL */
#define _print    chrBIT(6)		/* printable */
#define _punct    chrBIT(7)		/* punctuation */
#define _ident    chrBIT(8)		/* is typically legal in "normal" identifier */
#define _pathn    chrBIT(9)		/* is typically legal in a file's pathname */
#define _wild     chrBIT(10)		/* is typically a shell wildcard char */
#define _linespec chrBIT(11)		/* ex-style line range: 1,$ or 13,15 or % etc.*/
#define _fence    chrBIT(12)		/* a fence, i.e. (, ), [, ], {, } */
#define _nonspace chrBIT(13)		/* non-whitespace */
#define _qident   chrBIT(14)		/* is typically legal in "qualified" identifier */

#if OPT_WIDE_CTYPES
#define _scrtch   chrBIT(15)		/* legal in scratch-buffer names */
#define _shpipe   chrBIT(16)		/* legal in shell/pipe-buffer names */

#define	screen_to_bname(buf)\
	screen_string(buf,sizeof(buf),(CHARTYPE)(_pathn|_scrtch|_shpipe))
typedef	ULONG CHARTYPE;
#else
#define	screen_to_bname(buf)\
	screen_string(buf,sizeof(buf),(CHARTYPE)(_pathn))
typedef USHORT CHARTYPE;
#endif

/* these parallel the ctypes.h definitions, except that
	they force the char to valid range first */
#define istype(m,c) ((_chartypes_[((UINT)(c))&((UINT)(N_chars-1))] & (m)) != 0)

#define isAlnum(c)	istype(_lower|_upper|_digit, c)
#define isAlpha(c)	istype(_lower|_upper, c)
#define isCntrl(c)	istype(_cntrl, c)
#define isDigit(c)	istype(_digit, c)
#define isLower(c)	istype(_lower, c)
#define isPrint(c)	istype(_print, c)
#define isPunct(c)	istype(_punct, c)
#define isSpace(c)	istype(_space, c)
#define isUpper(c)	istype(_upper, c)

#define isbackspace(c)	(istype(_bspace, c) || (c) == backspc)
#define isfence(c)	istype(_fence, c)
#define isident(c)	istype(_ident, c)
#define islinespecchar(c)	istype(_linespec, c)
#define ispath(c)	istype(_pathn, c)
#define iswild(c)	istype(_wild, c)

/* macro for cases where return & newline are equivalent */
#define	isreturn(c)	((c == '\r') || (c == '\n'))

/* macro for whitespace (non-return) */
#define	isBlank(c)      ((c == '\t') || (c == ' '))

/* DIFCASE represents the difference between upper
   and lower case letters, DIFCNTRL the difference between upper case and
   control characters.	They are xor-able values.  */
#define	DIFCASE		0x20
#define	DIFCNTRL	0x40
#define toUpper(c)	((c)^DIFCASE)
#define toLower(c)	((c)^DIFCASE)
#define tocntrl(c)	((c)^DIFCNTRL)
#define toalpha(c)	((c)^DIFCNTRL)

#define nocase_eq(bc,pc)	((bc) == (pc) || \
			(isAlpha(bc) && (((bc) ^ DIFCASE) == (pc))))

#define ESC		tocntrl('[')
#define BEL		tocntrl('G')	/* ascii bell character		*/
#define CONTROL_A	tocntrl('A')	/* for cntl_a attribute sequences */

#if !(SYS_MSDOS && CC_DJGPP)
/* some systems need a routine to check for interrupts.  most don't, and
 * the routine overhead can be expensive in some places
 */
# define interrupted() (am_interrupted != 0)
#endif

#define ABORTED(c) ((c) == abortc || (c) == intrc || interrupted())

/*
 * Definitions etc. for regexp(3) routines.
 *
 *	the regexp code is:
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 */
#define NSUBEXP  10
typedef struct regexp {
	char *startp[NSUBEXP];
	char *endp[NSUBEXP];
	SIZE_T mlen;		/* convenience:  endp[0] - startp[0] */
	char regstart;		/* Internal use only. */
	char reganch;		/* Internal use only. */
	int regmust;		/* Internal use only. */
	int regmlen;		/* Internal use only. */
	SIZE_T size;		/* vile addition -- how big is this */
	char program[1];	/* Unwarranted chumminess with compiler. */
} regexp;

/*
 * The first byte of the regexp internal "program" is actually this magic
 * number; the start node begins in the second byte.
 */
#define	REGEXP_MAGIC	0234

#ifndef CHARBITS
#define	UCHAR_AT(p)	((int)*(UCHAR *)(p))
#else
#define	UCHAR_AT(p)	((int)*(p)&CHARBITS)
#endif

/* end of regexp stuff */

/*
 * Definitions for 'tbuff.c' (temporary/dynamic char-buffers)
 */
typedef	struct	_tbuff	{
	char *	tb_data;	/* the buffer-data */
	ALLOC_T	tb_size;	/* allocated size */
	ALLOC_T	tb_used;	/* total used in */
	ALLOC_T	tb_last;	/* last put/get index */
	int	tb_endc;
	} TBUFF;

/*
 * Definitions for 'itbuff.c' (temporary/dynamic int-buffers)
 */
typedef	struct	_itbuff	{
	int *	itb_data;	/* the buffer-data */
	ALLOC_T	itb_size;	/* allocated size */
	ALLOC_T	itb_used;	/* total used in */
	ALLOC_T	itb_last;	/* last put/get index */
	int	itb_endc;
	} ITBUFF;

/*
 * Primitive types
 */
typedef	int		L_NUM;		/* line-number */
typedef	int		C_NUM;		/* column-number */
typedef	struct {
    unsigned short flgs;
    unsigned short cook;
} L_FLAG;		/* LINE-flags */

typedef	ULONG		CMDFLAGS;	/* CMDFUNC flags */
typedef	long		B_COUNT;	/* byte-count */

#ifdef __cplusplus
#define EXTERN_CONST extern const
#else
#define EXTERN_CONST const
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

	/* Some lint's do, many don't like this */
#ifdef lint
#undef  HAVE_LOSING_SWITCH_WITH_STRUCTURE_OFFSET
#define HAVE_LOSING_SWITCH_WITH_STRUCTURE_OFFSET 1
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
typedef	struct	LINE*	LINEPTR;

typedef struct	LINE {
	LINEPTR l_fp;			/* Link to the next line	*/
	LINEPTR l_bp;			/* Link to the previous line	*/
	union {
		SIZE_T	l_sze;		/* Allocated size 		*/
		C_NUM	l_fo;		/* forward undo dot offs (undo only) */
	} l_s_fo;
	union {
		L_NUM	l_nmbr;		/* line-# iff b_numlines > 0	*/
		C_NUM	l_bo;		/* backward undo dot offs (undo only) */
	} l_n_bo;
	int	l_used;			/* Used size (may be negative)	*/
	union {
	    char *l_txt;		/* The data for this line	*/
	    LINEPTR l_nxt;		/* if an undo stack separator,	*/
	} lt;				/*  a pointer to the next one	*/
	union
	{
	    LINEPTR	l_stklnk;	/* Link for undo stack		*/
	    L_FLAG	l_flg;		/* flags for undo ops		*/
	} l;
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
#define	for_each_line(lp,bp) for (lp = lforw(buf_head(bp)); \
					lp != buf_head(bp); \
					lp = lforw(lp))

#define l_nxtundo		l.l_stklnk

	/*
	 * Special values used in LINE.l_used
	 */
#define LINENOTREAL	((int)(-1)) /* for undo, marks an inserted line */
#define LINEUNDOPATCH	((int)(-2)) /* provides stack patching value for undo */
/* #define MARKPATCH	((int)(-3)) *//*	unused */
#define STACKSEP	((int)(-4)) /* delimit set of changes on undo stack */
#define PURESTACKSEP	((int)(-5)) /* as above, but buffer unmodified before */
					/* this change */

#define	null_ptr	(LINE *)0
#define set_lforw(a,b)	lforw(a) = (b)
#define set_lback(a,b)	lback(a) = (b)
#define lforw(lp)	(lp)->l_fp
#define lback(lp)	(lp)->l_bp

	/*
	 * Macros for referencing fields in the LINE struct.
	 */
#define lgetc(lp, n)		char2int((lp)->l_text[(n)])
#define lputc(lp, n, c) 	((lp)->l_text[(n)]=(c))
#define llength(lp)		((lp)->l_used)

#define liscopied(lp)		((lp)->l.l_undo_cookie == current_undo_cookie)
#define lsetcopied(lp)		((lp)->l.l_undo_cookie = current_undo_cookie)
#define lsetnotcopied(lp)	((lp)->l.l_undo_cookie = 0)

#define lismarked(lp)		((lp)->l.l_flag & LGMARK)
#define lsetmarked(lp)		((lp)->l.l_flag |= LGMARK)
#define lsetnotmarked(lp)	((lp)->l.l_flag &= ~LGMARK)
#define lflipmark(lp)		((lp)->l.l_flag ^= LGMARK)

#define listrimmed(lp)		((lp)->l.l_flag & LTRIMMED)
#define lsettrimmed(lp)		((lp)->l.l_flag |= LTRIMMED)
#define lsetnottrimmed(lp)	((lp)->l.l_flag &= ~LTRIMMED)
#define lsetclear(lp)		((lp)->l.l_flag = (lp)->l.l_undo_cookie = 0)

#define lisreal(lp)		((lp)->l_used >= 0)
#define lisnotreal(lp)		((lp)->l_used == LINENOTREAL)
#define lislinepatch(lp)	((lp)->l_used == LINEUNDOPATCH)
/* #define lismarkpatch(lp)	((lp)->l_used == MARKPATCH) */
#define lispatch(lp)		(lislinepatch(lp) /* || lismarkpatch(lp) */ )
#define lisstacksep(lp)		((lp)->l_used == STACKSEP || \
					(lp)->l_used == PURESTACKSEP)
#define lispurestacksep(lp)	((lp)->l_used == PURESTACKSEP)

/* marks are a line and an offset into that line */
typedef struct MARK {
	LINEPTR l;
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
	MARK 	r_orig;			/* Origin LINE address. 	*/
	MARK	r_end;			/* Ending LINE address. 	*/
	C_NUM	r_leftcol;		/* Leftmost column. 		*/
	C_NUM	r_rightcol;		/* Rightmost column. 		*/
	B_COUNT	r_size; 		/* Length in characters.	*/
#if OPT_SELECTIONS
	USHORT	r_attr_id;		/* id of corresponding display  */
#endif
}	REGION;

#if OPT_COLOR || DISP_X11 || OPT_HILITEMATCH
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
#define VASPCOL ((VIDEO_ATTR)0x0800)	/* specific color   		*/
#define VACOLOR ((VIDEO_ATTR)0xf000)	/* color mask			*/
#define VACOL_0 (VASPCOL)		/* color palette index 0 	*/
#define VACOL_1 (VASPCOL+1)		/* etc.				*/
#define VACOL_2 (VASPCOL+2)
#define VACOL_3 (VASPCOL+3)
#define VACOL_4 (VASPCOL+4)
#define VACOL_5 (VASPCOL+5)
#define VACOL_6 (VASPCOL+6)
#define VACOL_7 (VASPCOL+7)
#define VACOL_8 (VASPCOL+8)
#define VACOL_9 (VASPCOL+9)
#define VACOL_A (VASPCOL+0xA)
#define VACOL_B (VASPCOL+0xB)
#define VACOL_C (VASPCOL+0xC)
#define VACOL_D (VASPCOL+0xD)
#define VACOL_E (VASPCOL+0xE)
#define VACOL_F (VASPCOL+0xF)

#define VCOLORNUM(attr) (((attr) & VACOLOR) >> 12)
#define VCOLORATTR(num) ((UINT)(num) << 12)

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
#define VATTRIB(attr) ((attr) & (VIDEO_ATTR) ~(VAOWNER|VADIRTY))
#else
#define VADIRTY ((VIDEO_ATTR)0)		/* nop for all others */
#define VATTRIB(attr) ((attr) & (VIDEO_ATTR) ~(VAOWNER))
#endif

/* grow (or initially allocate) a vector of newsize types, pointed to by
 * ptr.  this is used primarily for resizing the screen
 * the studious will note this is a lot like realloc.   but realloc
 * doesn't guarantee to preserve contents if if fails, and this also
 * zeroes the new space.
 */
#define GROW(ptr, type, oldsize, newsize) \
{ \
	int tmpold = oldsize; \
	type *tmpp; \
	tmpp = typeallocn(type, newsize); \
	if (tmpp == NULL) \
		return FALSE; \
 \
	if (ptr) { \
		(void) memcpy((char *)tmpp, (char *)ptr, tmpold * sizeof(type)); \
		free((char *)ptr); \
	} else { \
		tmpold = 0; \
	} \
	ptr = tmpp; \
	(void) memset ((char *)(ptr+tmpold), 0, (newsize - tmpold) * sizeof(type)); \
}

/*
 * An attributed region is attached to a buffer and indicates how the
 * region should be displayed; eg. inverse video, underlined, etc.
 */

typedef struct _aregion {
	struct _aregion	*ar_next;
	REGION		ar_region;
	VIDEO_ATTR	ar_vattr;
	REGIONSHAPE	ar_shape;
}	AREGION;

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

#define CHGD_ARGS VALARGS *args, int glob_vals, int testing

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
#define WFSBAR	iBIT(9)			/* Update scroll bar(s) */

/* define indices for GLOBAL, BUFFER, WINDOW modes */
#ifdef realdef
#include "chgdfunc.h"
#endif

#if	CHECK_PROTOTYPES
	typedef long W_VALUES;
	typedef long M_VALUES;
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
	MARK 	w_dt;		/* Line containing "."	       */
		/* i don't think "mark" needs to be here -- I think it
			could safely live only in the buffer -pgf */
#if WINMARK
	MARK 	w_mk;	        /* Line containing "mark"      */
#endif
	MARK 	w_ld;	        /* Line containing "lastdotmark"*/
	MARK 	w_tld;	        /* Line which may become "lastdotmark"*/
	MARK 	w_ln;		/* Top line in the window (offset used in linewrap) */
#if OPT_MOUSE
	int	insmode;
#endif
	W_VALUES w_vals;
} W_TRAITS;

#define global_w_val(which) global_w_values.wv[which].v.i
#define set_global_w_val(which,val) global_w_val(which) = val
#define global_w_val_ptr(which) global_w_values.wv[which].v.p
#define set_global_w_val_ptr(which,val) global_w_val_ptr(which) = val

#define w_val(wp,val) (wp->w_values.wv[val].vp->i)
#define set_w_val(wp,which,val) w_val(wp,which) = val
#define w_val_ptr(wp,val) (wp->w_values.wv[val].vp->p)
#define set_w_val_ptr(wp,which,val) w_val_ptr(wp,which) = val

#define make_local_w_val(wp,which)  \
	make_local_val(wp->w_values.wv, which)
#define make_global_w_val(wp,which)  \
	make_global_val(wp->w_values.wv, global_wvalues.wv, which)

#define is_local_w_val(wp,which)  \
	is_local_val(wp->w_values.wv,which)

#if OPT_COLOR
#define gfcolor global_g_val(GVAL_FCOLOR)
#define gbcolor global_g_val(GVAL_BCOLOR)
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
typedef struct {
	char *name;
	M_VALUES mm;
	B_VALUES mb;
} MAJORMODE;

#define is_c_mode(bp) (bp->majr != 0 && !strcmp(bp->majr->name, "c"))
#define fix_cmode(bp,value)	/* nothing */
#else
#define is_c_mode(bp) (b_val(bp,MDCMOD))
#define fix_cmode(bp,value)	make_local_b_val(bp, MDCMOD), \
				set_b_val(bp, MDCMOD, value)
#endif

/*
 * Text is kept in buffers. A buffer header, described below, exists for every
 * buffer in the system. The buffers are kept in a big list, so that commands
 * that search for a buffer by name can find the buffer header. There is a
 * safe store for the dot and mark in the header, but this is only valid if
 * the buffer is not being displayed (that is, if "b_nwnd" is 0). The text for
 * the buffer is kept in a circularly linked list of lines, with a pointer to
 * the header line in "b_line"	Buffers may be "Inactive" which means the files associated with them
 * have not been read in yet. These get read in at "use buffer" time.
 */

typedef struct	BUFFER {
	MARK 	b_line;		/* Link to the header LINE (offset unused) */
	struct	BUFFER *b_bufp; 	/* Link to next BUFFER		*/
	MARK 	*b_nmmarks;		/* named marks a-z		*/
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
	LINEPTR b_udstks[2];		/* undo stack pointers		*/
	MARK 	b_uddot[2];		/* Link to "." before undoable op*/
	short	b_udstkindx;		/* which of above to use	*/
	LINEPTR b_udtail;		/* tail of undo backstack	*/
	LINEPTR b_udlastsep;		/* last stack separator pushed	*/
	int	b_udcount;		/* how many undo's we can do	*/
	LINEPTR	b_LINEs;		/* block-malloced LINE structs */
	LINEPTR	b_LINEs_end;		/* end of 	"	"	" */
	LINEPTR	b_freeLINEs;		/* list of free " 	"	" */
	UCHAR	*b_ltext;		/* block-malloced text */
	UCHAR	*b_ltext_end;		/* end of block-malloced text */
	LINEPTR	b_ulinep;		/* pointer at 'Undo' line	*/
	int	b_active;		/* window activated flag	*/
	UINT	b_nwnd;		        /* Count of windows on buffer   */
	UINT	b_flag;		        /* Flags 		        */
	short	b_acount;		/* auto-save count	        */
	char	*b_fname;		/* File name			*/
	int	b_fnlen;		/* length of filename		*/
	char	b_bname[NBUFN]; 	/* Buffer name			*/
#if	OPT_ENCRYPT
	char	b_key[NPAT];		/* current encrypted key	*/
#endif
#ifdef	MDCHK_MODTIME
	time_t	b_modtime;		/* file's last-modification time */
	time_t	b_modtime_at_warn;	/* file's modtime when user warned */
#endif
#if	OPT_UPBUFF
	int	(*b_upbuff) (struct BUFFER *bp); /* call to recompute  */
	int	(*b_rmbuff) (struct BUFFER *bp); /* call on removal	*/
#endif
#if	OPT_B_LIMITS
	int	b_lim_left;		/* extra left-margin (cf:show-reg) */
#endif
	struct	BUFFER *b_relink; 	/* Link to next BUFFER (sorting) */
	int	b_created;
	int	b_last_used;
#if OPT_HILITEMATCH
	USHORT	b_highlight;
#endif
#if OPT_PERL || OPT_TCL
	void *	b_api_private;		/* pointer to private perl, tcl, etc.
	                                   data */
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

#define global_b_val(which) global_b_values.bv[which].v.i
#define set_global_b_val(which,val) global_b_val(which) = val
#define global_b_val_ptr(which) global_b_values.bv[which].v.p
#define set_global_b_val_ptr(which,val) global_b_val_ptr(which) = val
#define global_b_val_rexp(which) global_b_values.bv[which].v.r
#define set_global_b_val_rexp(which,val) global_b_val_rexp(which) = val

#define b_val(bp,val) (bp->b_values.bv[val].vp->i)
#define set_b_val(bp,which,val) b_val(bp,which) = val
#define b_val_ptr(bp,val) (bp->b_values.bv[val].vp->p)
#define set_b_val_ptr(bp,which,val) b_val_ptr(bp,which) = val
#define b_val_rexp(bp,val) (bp->b_values.bv[val].vp->r)
#define set_b_val_rexp(bp,which,val) b_val_rexp(bp,which) = val

#define window_b_val(wp,val) \
 	((wp != 0 && wp->w_bufp != 0) \
 		? b_val(wp->w_bufp,val) \
		: global_b_val(val))

#define make_local_b_val(bp,which)  \
		make_local_val(bp->b_values.bv, which)
#define make_global_b_val(bp,which)  \
		make_global_val(bp->b_values.bv, global_b_values.bv, which)

#define is_local_b_val(bp,which)  \
	is_local_val(bp->b_values.bv,which)

#define is_empty_buf(bp) (lforw(buf_head(bp)) == buf_head(bp))

#define b_dot     b_wtraits.w_dt
#if WINMARK
#define b_mark    b_wtraits.w_mk
#endif
#define b_lastdot b_wtraits.w_ld
#define b_tentative_lastdot b_wtraits.w_tld
#define b_wline   b_wtraits.w_ln

#if OPT_CASELESS
#define eql_bname(bp,name) !stricmp(bp->b_bname, name)
#else
#define eql_bname(bp,name) !strcmp(bp->b_bname, name)
#endif

/* values for b_flag */
#define BFINVS     iBIT(0)	/* Internal invisible buffer	*/
#define BFCHG      iBIT(1)	/* Changed since last write	*/
#define BFSCRTCH   iBIT(2)	/* scratch -- gone on last close */
#define BFARGS     iBIT(3)	/* set for ":args" buffers */
#define BFIMPLY    iBIT(4)	/* set for implied-# buffers */
#define BFSIZES    iBIT(5)	/* set if byte/line counts current */
#define BFUPBUFF   iBIT(6)	/* set if buffer should be updated */
#define BFRCHG     iBIT(7)	/* Changed since last reset of this flag*/

/* macros for manipulating b_flag */
#define b_is_implied(bp)        ((bp)->b_flag & (BFIMPLY))
#define b_is_argument(bp)       ((bp)->b_flag & (BFARGS))
#define b_is_changed(bp)        ((bp)->b_flag & (BFCHG))
#define b_is_recentlychanged(bp) ((bp)->b_flag & (BFRCHG))
#define b_is_invisible(bp)      ((bp)->b_flag & (BFINVS))
#define b_is_scratch(bp)        ((bp)->b_flag & (BFSCRTCH))
#define b_is_temporary(bp)      ((bp)->b_flag & (BFINVS|BFSCRTCH))
#define b_is_counted(bp)        ((bp)->b_flag & (BFSIZES))
#define b_is_obsolete(bp)       ((bp)->b_flag & (BFUPBUFF))

#define b_set_flags(bp,flags)   (bp)->b_flag |= (flags)
#define b_set_changed(bp)       b_set_flags(bp, BFCHG)
#define b_set_recentlychanged(bp) b_set_flags(bp, BFRCHG)
#define b_set_counted(bp)       b_set_flags(bp, BFSIZES)
#define b_set_invisible(bp)     b_set_flags(bp, BFINVS)
#define b_set_obsolete(bp)      b_set_flags(bp, BFUPBUFF)
#define b_set_scratch(bp)       b_set_flags(bp, BFSCRTCH)

#define b_clr_flags(bp,flags)   (bp)->b_flag &= ~(flags)
#define b_clr_changed(bp)       b_clr_flags(bp, BFCHG)
#define b_clr_recentlychanged(bp) b_clr_flags(bp, BFRCHG)
#define b_clr_counted(bp)       b_clr_flags(bp, BFSIZES)
#define b_clr_obsolete(bp)      b_clr_flags(bp, BFUPBUFF)
#define b_clr_scratch(bp)       b_clr_flags(bp, BFSCRTCH)

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

/* macro for iterating over the marks associated with the current buffer */

#if OPT_PERL || OPT_TCL
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
	int	     dmi_idx;				\
	AREGION     *dmi_ap = curbp->b_attribs;		\
	if (curbp->b_nmmarks != NULL)			\
	    for (dmi_idx=0; dmi_idx < 26; dmi_idx++) {	\
		mp = &(curbp->b_nmmarks[dmi_idx]);	\
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
	if (curbp->b_nmmarks != NULL) {			\
	    struct MARK *mp;				\
	    int dmi_idx;				\
	    for (dmi_idx=0; dmi_idx < 26; dmi_idx++) {	\
		mp = &(curbp->b_nmmarks[dmi_idx]);	\
		statement				\
	    }						\
	}						\
	api_do_mark_iterate_helper(mp, statement)	\
    } one_time
#endif /* OPT_VIDEO_ATTRS */

/*
 * There is a window structure allocated for every active display window. The
 * windows are kept in a big list, in top to bottom screen order, with the
 * listhead at "wheadp". Each window contains its own values of dot and mark.
 * The flag field contains some bits that are set by commands to guide
 * redisplay. Although this is a bit of a compromise in terms of decoupling,
 * the full blown redisplay is just too expensive to run for every input
 * character.
 */

#define WINDOW	vile_WINDOW		/* avoid conflict with curses.h */

typedef struct	WINDOW {
	W_TRAITS w_traits;		/* features of the window we should */
					/*  remember between displays */
	struct	WINDOW *w_wndp; 	/* Next window			*/
	BUFFER  *w_bufp; 		/* Buffer displayed in window	*/
	int	w_toprow;	        /* Origin 0 top row of window   */
	int	w_ntrows;	        /* # of rows of text in window  */
	int	w_force; 	        /* If non-zero, forcing row.    */
	USHORT	w_flag;		        /* Flags.		        */
	ULONG	w_split_hist;		/* how to recombine deleted windows */
#ifdef WMDRULER
	int	w_ruler_line;
	int	w_ruler_col;
#endif
}	WINDOW;

#define	for_each_window(wp) for (wp = wheadp; wp; wp = wp->w_wndp)

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

	/* we use left-margin for protecting the prefix-area of [Registers]
	 * from cut/paste selection.
	 */
#define w_left_margin(wp) b_left_margin(wp->w_bufp)

	/* tputs uses a 3rd parameter (a function pointer).  We're stuck with
	 * making ttputc and TTputc the same type.
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
	int	t_mrow;			/* max number of rows allowable */
	int	t_nrow; 		/* current number of rows used	*/
	int	t_mcol; 		/* max Number of columns.	*/
	int	t_ncol; 		/* current Number of columns.	*/
	int	t_margin;		/* min margin for extended lines*/
	int	t_scrsiz;		/* size of scroll region "	*/
	int	t_pause;		/* # times thru update to pause */
	void	(*t_open) (void);	/* Open terminal at the start.	*/
	void	(*t_close) (void);	/* Close terminal at end.	*/
	void	(*t_kopen) (void);	/* Open keyboard		*/
	void	(*t_kclose) (void);	/* close keyboard		*/
	int	(*t_getchar) (void);	/* Get character from keyboard. */
	OUTC_DCL(*t_putchar) (OUTC_ARGS); /* Put character to display.	*/
	int	(*t_typahead) (void);	/* character ready?		*/
	void	(*t_flush) (void);	/* Flush output buffers.	*/
	void	(*t_move) (int row, int col); /* Move the cursor, origin 0. */
	void	(*t_eeol) (void);	/* Erase to end of line.	*/
	void	(*t_eeop) (void);	/* Erase to end of page.	*/
	void	(*t_beep) (void);	/* Beep.			*/
	void	(*t_rev) (UINT f);	/* set reverse video state	*/
	int	(*t_rez) (char *f);	/* change screen resolution	*/
	void	(*t_setfor) (int f);	/* set foreground color		*/
	void	(*t_setback) (int b);	/* set background color		*/
	void	(*t_setpal) (const char *p); /* set color palette	*/
	void	(*t_scroll) (int from, int to, int n); /* scroll region	*/
	void	(*t_pflush) (void);	/* really flush 		*/
	void	(*t_icursor) (int c);	/* set cursor shape for insertion */
	void	(*t_title) (char *t);	/* set window title		*/
}	TERM;

/*	TEMPORARY macros for terminal I/O  (to be placed in a machine
					    dependent place later)	*/

#define	TTopen		(*term.t_open)
#define	TTclose		(*term.t_close)
#define	TTkopen		(*term.t_kopen)
#define	TTkclose	(*term.t_kclose)
#define	TTgetc		(*term.t_getchar)
#define	TTputc		(*term.t_putchar)
#define	TTtypahead	(*term.t_typahead)
#define	TTflush		(*term.t_flush)
#define	TTmove		(*term.t_move)
#define	TTeeol		(*term.t_eeol)
#define	TTeeop		(*term.t_eeop)
#define	TTbeep		(*term.t_beep)
#define	TTrev		(*term.t_rev)
#define	TTrez		(*term.t_rez)
#define	TTforg(f)	(*term.t_setfor)(f)
#define	TTbacg(b)	(*term.t_setback)(b)
#define	TTspal(p)	(*term.t_setpal)(p)
#define	TTscroll(f,t,n)	(*term.t_scroll)(f,t,n)
#define	TTpflush()	(*term.t_pflush)()
#define	TTicursor(c)	(*term.t_icursor)(c)
#define	TTtitle(t)	(*term.t_title)(t)

typedef struct  VIDEO {
        UINT	v_flag;                 /* Flags */
#if	OPT_COLOR
	int	v_fcolor;		/* current forground color */
	int	v_bcolor;		/* current background color */
	int	v_rfcolor;		/* requested forground color */
	int	v_rbcolor;		/* requested background color */
#endif
#if	OPT_VIDEO_ATTRS
	VIDEO_ATTR *v_attrs;		/* screen data attributes */
#endif
	/* allocate 4 bytes here, and malloc 4 bytes less than we need,
		to keep malloc from rounding up. */
        char    v_text[4];              /* Screen data. */
}       VIDEO;

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

#define VFCHG	iBIT(0) 		/* Changed flag			*/
#define	VFEXT	iBIT(1)			/* extended (beyond column 80)	*/
#define	VFREV	iBIT(2)			/* reverse video status		*/
#define	VFREQ	iBIT(3)			/* reverse video request	*/
#define	VFCOL	iBIT(4)			/* color change requested	*/

#if DISP_IBMPC
/*
 * these need to go into edef.h eventually!
 */
#define	CDCGA	0			/* color graphics card		*/
#define	CDMONO	1			/* monochrome text card		*/
#define	CDEGA	2			/* EGA color adapter		*/
#define	CDVGA	3			/* VGA color adapter		*/
#define	CDSENSE	-1			/* detect the card type		*/

#if OPT_COLOR
#define	CD_25LINE	CDCGA
#else
#define	CD_25LINE	CDMONO
#endif

#endif


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
	CmdFunc  c_func;	/* function name is bound to */
	CMDFLAGS c_flags;	/* what sort of command is it? */
#if OPT_ONLINEHELP
	const char *c_help;	/* short help message for the command */
#endif
}	CMDFUNC;

/*
 * Other useful argument templates
 */
#define EOL_ARGS  const char * buffer, unsigned cpos, int c, int eolchar
#define DONE_ARGS int c, char *buf, unsigned *pos
#define LIST_ARGS int flag, void *ptr
#define REGN_ARGS void *flagp, int l, int r

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
	char *n_name;
	const CMDFUNC *n_cmd;
}	NTAB;

/*
 * a binary search tree of the above structure.  we use this so that we can
 * add in procedures as they are created.
 */
typedef struct {
	const char *bi_key;		/* the name of the command	*/
	const CMDFUNC *n_cmd;		/* if NULL, stored procedure	*/
	int n_readonly;			/* original commands readonly	*/
}	NBST_DATA;


/* when a command is referenced by bound key (like h,j,k,l, or "dd"), it
 *	is looked up one of two ways: single character 7-bit ascii commands (by
 *	far the majority) are simply indexed into an array of CMDFUNC pointers.
 *	Other commands (those with ^A, ^X, or SPEC prefixes) are searched for
 *	in a binding table, made up of KBIND structures.  This structure
 *	contains the command code, and again, a pointer to the CMDFUNC
 *	structure for the command
 *
 *	The asciitbl array, and the kbindtbl array are generated automatically
 *	from the cmdtbl file, and can be found in the file nebind.h
 */
typedef struct  k_bind {
	short	k_code; 		/* Key code			*/
	const CMDFUNC *k_cmd;
#if OPT_REBIND
	struct  k_bind *k_link;
#endif
}	KBIND;


/* These are the flags which can appear in the CMDFUNC structure, describing a
 * command.
 */
#define NONE    0L
#define cmdBIT(n) lBIT(n)	/* ...to simplify typing */
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
#define RECT    cmdBIT(11)	/* motion causes rectangular operation */

/* These flags are 'ex' argument descriptors, adapted from elvis.  Not all are
 * used or honored or implemented.
 */
#define argBIT(n) cmdBIT(n+11)	/* ...to simplify adding bits */
#define FROM    argBIT(1)	/* allow a linespec */
#define TO      argBIT(2)	/* allow a second linespec */
#define BANG    argBIT(3)	/* allow a ! after the command name */
#define EXTRA   argBIT(4)	/* allow extra args after command name */
#define XFILE   argBIT(5)	/* expand wildcards in extra part */
#define NOSPC   argBIT(6)	/* no spaces allowed in the extra part */
#define DFLALL  argBIT(7)	/* default file range is 1,$ */
#define DFLNONE argBIT(9)	/* no default file range */
#define NODFL   argBIT(10)	/* do not default to the current file name */
#define EXRCOK  argBIT(11)	/* can be in a .exrc file */
#define NL      argBIT(12)	/* if !exmode, then write a newline first */
#define PLUS    argBIT(13)	/* allow a line number, as in ":e +32 foo" */
#define ZERO    argBIT(14)	/* allow 0 to be given as a line number */
#define OPTREG  argBIT(15)	/* allow optional register-name */
#define FILES   (XFILE | EXTRA)	/* multiple extra files allowed */
#define WORD1   (EXTRA | NOSPC)	/* one extra word allowed */
#define FILE1   (FILES | NOSPC)	/* 1 file allowed, defaults to current file */
#define NAMEDF  (FILE1 | NODFL)	/* 1 file allowed, defaults to "" */
#define NAMEDFS (FILES | NODFL)	/* multiple files allowed, default is "" */
#define RANGE   (FROM  | TO)	/* range of linespecs allowed */

#define SPECIAL_BANG_ARG -42	/* arg passed as 'n' to functions which
 					were invoked by their "xxx!" name */

/* definitions for 'mlreply_file()' and other filename-completion */
#define	FILEC_REREAD   4
#define	FILEC_READ     3
#define	FILEC_UNKNOWN  2
#define	FILEC_WRITE    1

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
#define TMPDIR gtenv("directory")
#else
#define TMPDIR P_tmpdir		/* defined in <stdio.h> */
#endif	/* OPT_EVAL */

/*	The editor holds deleted text chunks in the KILL registers. The
	kill registers are logically a stream of ascii characters, however
	due to unpredictable size, are implemented as a linked
	list of chunks. (The d_ prefix is for "deleted" text, as k_
	was taken up by the keycode structure)
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

#define	KbSize(i,p)	((p->d_next != 0) ? KBLOCK : kbs[i].kused)

/*
 * Incremental search defines.
 */
#if	OPT_ISRCH

#define	CMDBUFLEN	256	/* Length of our command buffer */

#define IS_REVERSE	tocntrl('R')	/* Search backward */
#define	IS_FORWARD	tocntrl('F')	/* Search forward */

#endif

#ifndef NULL
# define NULL 0
#endif

/*
 * General purpose includes
 */

#include <stdarg.h>

#if DISP_X11 && SYS_APOLLO
#define SYSV_STRINGS	/* <strings.h> conflicts with <string.h> */
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if SYS_VMS
#include <unixio.h>
#include <unixlib.h>
#include <file.h>	/* aka <sys/file.h> */
#endif

#if HAVE_STDDEF_H
#include <stddef.h>
#endif

#if (HAVE_STDLIB_H || SYS_VMS || CC_NEWDOSCC)
#include <stdlib.h>
#else
extern void exit (int code);
extern void _exit (int code);
#endif	/* HAVE_STDLIB_H */

/* array/table size */
#define	TABLESIZE(v)	(sizeof(v)/sizeof(v[0]))

/* Quiet compiler warnings on places where we're being blamed incorrectly,
 * e.g., for casting away const, or for alignment problems.  It's always
 * legal to cast a pointer to long w/o loss of precision.
 */
#define TYPECAST(type,ptr) (type*)((long)(ptr))

/* structure-allocate, for linting */
#ifdef	lint
#define	castalloc(cast,nbytes)		((cast *)0)
#define	castrealloc(cast,ptr,nbytes)	((ptr)+(nbytes))
#define	typecalloc(cast)		((cast *)0)
#define	typecallocn(cast,ntypes)	(((cast *)0)+(ntypes))
#define	typealloc(cast)			((cast *)0)
#define	typeallocn(cast,ntypes)		(((cast *)0)+(ntypes))
#define	typereallocn(cast,ptr,ntypes)	((ptr)+(ntypes))
#define	typeallocplus(cast,extra)	(((cast *)0)+(extra))
#else
#define	castalloc(cast,nbytes)		(cast *)malloc(nbytes)
#define	castrealloc(cast,ptr,nbytes)	(cast *)realloc((char *)(ptr),(nbytes))
#define	typecalloc(cast)		(cast *)calloc(sizeof(cast),1)
#define	typecallocn(cast,ntypes)	(cast *)calloc(sizeof(cast),ntypes)
#define	typealloc(cast)			(cast *)malloc(sizeof(cast))
#define	typeallocn(cast,ntypes)		(cast *)malloc((ntypes)*sizeof(cast))
#define	typereallocn(cast,ptr,ntypes)	(cast *)realloc((char *)(ptr),\
							(ntypes)*sizeof(cast))
#define	typeallocplus(cast,extra)	(cast *)malloc((extra)+sizeof(cast))
#endif

#define	FreeAndNull(p)	if ((p) != 0) { free((char *)p); p = 0; }
#define	FreeIfNeeded(p)	if ((p) != 0) free((char *)(p))

#define	ExitProgram(code)	exit(code)

/*
 * We cannot define these in config.h, since they require parameters to be
 * passed (that's non-portable).
 */
#ifdef __cplusplus
#undef GCC_PRINTF
#undef GCC_NORETURN
#undef GCC_UNUSED
#endif

#if GCC_PRINTF
#define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))
#else
#define GCC_PRINTFLIKE(fmt,var) /*nothing*/
#endif

#ifndef	GCC_NORETURN
#define	GCC_NORETURN /* nothing */
#endif

#ifndef	GCC_UNUSED
#define	GCC_UNUSED /* nothing */
#endif

#if HAVE_SELECT
# if HAVE_SELECT_H
# include <select.h>
# endif
# if HAVE_SYS_SELECT_H
# include <sys/select.h>
# endif
#endif

#if HAVE_UTIME_H
# include <utime.h>
#endif

#if HAVE_SYS_UTIME_H
# include <sys/utime.h>
#endif

/*
 * Comparison-function for 'qsort()'
 */
#ifndef ANSI_QSORT
#  if __STDC__ || defined(CC_TURBO) || CC_WATCOM
#    define	ANSI_QSORT 1
#  else
#    define	ANSI_QSORT 0
#  endif
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
#ifndef	NO_LEAKS	/* free permanent memory, analyze leaks */
#define	NO_LEAKS	0
#endif
#ifndef TEST_DOS_PIPES
#define TEST_DOS_PIPES	0
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
#  if OPT_TRACE
#    include "trace.h"
#  endif
#else
#  if NO_LEAKS || DOALLOC || OPT_TRACE
#    include "trace.h"
#  endif
#endif	/* USE_DBMALLOC */

/* Normally defined in "trace.h" */
#ifndef TRACE
#define TRACE(p) /* nothing */
#endif

#if DISP_X11 && NEED_X_INCLUDES
#include	<X11/Intrinsic.h>
#include	<X11/StringDefs.h>
#endif

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

/*	Dynamic RAM tracking and reporting redefinitions	*/
#if	OPT_RAMSIZE
#undef	realloc
#define	realloc	reallocate
#undef	calloc
#define	calloc(n,m)	allocate((n)*(m))
#undef	malloc
#define	malloc	allocate
#undef	free
#define	free	release
#endif

/* for debugging VMS pathnames on UNIX... */
#if SYS_UNIX && OPT_VMS_PATH
#include "fakevms.h"
#endif

#endif /* _estruct_h */
