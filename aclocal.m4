dnl
dnl Local definitions for autoconf.
dnl ------------------------
dnl $Header: /users/source/archives/vile.vcs/RCS/aclocal.m4,v 1.22 1996/12/08 21:41:48 tom Exp $
dnl
dnl ---------------------------------------------------------------------------
dnl ---------------------------------------------------------------------------
dnl
dnl VC_PREREQ_COMPARE(MAJOR1, MINOR1, TERNARY1, MAJOR2, MINOR2, TERNARY2,
dnl                   PRINTABLE2, not FOUND, FOUND)
define(VC_PREREQ_COMPARE,
[ifelse(builtin([eval], [$3 < $6]), 1,
ifelse([$8], , ,[$8]),
ifelse([$9], , ,[$9]))])dnl
dnl
dnl Conditionally generate script according to whether we're using the release
dnl version of autoconf, or a patched version (using the ternary component as
dnl the patch-version).
define(VC_AC_PREREQ,
[VC_PREREQ_COMPARE(
AC_PREREQ_CANON(AC_PREREQ_SPLIT(AC_ACVERSION)),
AC_PREREQ_CANON(AC_PREREQ_SPLIT([$1])), [$1], [$2], [$3])])dnl
dnl
dnl ---------------------------------------------------------------------------
dnl Modifications/extensions to autoconf
dnl ---------------------------------------------------------------------------
dnl
dnl AC_MSG_CHECKING(FEATURE-DESCRIPTION)
define(AC_MSG_CHECKING,
[echo $ac_n "checking $1""... $ac_c" 1>&AC_FD_MSG
echo "configure:__oline__: checking $1" >&AC_FD_CC])dnl
dnl
dnl AC_CHECKING(FEATURE-DESCRIPTION)
define(AC_CHECKING,
[echo "checking $1" 1>&AC_FD_MSG
echo "configure:__oline__: checking $1" >&AC_FD_CC])dnl
dnl
define([AC_FUNC_SETPGRP],
[AC_CACHE_CHECK(whether setpgrp takes no argument, ac_cv_func_setpgrp_void,
AC_TRY_RUN([
/*
 * If this system has a BSD-style setpgrp, which takes arguments, exit
 * successfully.
 */
main()
{
    if (setpgrp(1,1) == -1)
	exit(0);
    else
	exit(1);
}
], ac_cv_func_setpgrp_void=no, ac_cv_func_setpgrp_void=yes,
   AC_MSG_ERROR(cannot check setpgrp if cross compiling))
)
if test $ac_cv_func_setpgrp_void = yes; then
  AC_DEFINE(SETPGRP_VOID)
fi
])dnl
dnl
dnl ---------------------------------------------------------------------------
dnl Vile-specific macros
dnl ---------------------------------------------------------------------------
dnl This is adapted from the macros 'fp_PROG_CC_STDC' and 'fp_C_PROTOTYPES'
dnl in the sharutils 4.2 distribution.
AC_DEFUN([VC_ANSI_CC],
[AC_MSG_CHECKING(for ${CC-cc} option to accept ANSI C)
AC_CACHE_VAL(vc_cv_ansi_cc,
[vc_cv_ansi_cc=no
vc_save_CFLAGS="$CFLAGS"
# Don't try gcc -ansi; that turns off useful extensions and
# breaks some systems' header files.
# AIX			-qlanglvl=ansi
# Ultrix and OSF/1	-std1
# HP-UX			-Aa -D_HPUX_SOURCE
# SVR4			-Xc
# UnixWare 1.2		(cannot use -Xc, since ANSI/POSIX clashes)
for vc_arg in "-DCC_HAS_PROTOS" "" -qlanglvl=ansi -std1 "-Aa -D_HPUX_SOURCE" -Xc
do
	CFLAGS="$vc_save_CFLAGS $vc_arg"
	AC_TRY_COMPILE(
[
#ifndef CC_HAS_PROTOS
#if !defined(__STDC__) || __STDC__ != 1
choke me
#endif
#endif
], [int test (int i, double x);
struct s1 {int (*f) (int a);};
struct s2 {int (*f) (double a);};],
[vc_cv_ansi_cc="$vc_arg"; break])
done
CFLAGS="$vc_save_CFLAGS"
])
AC_MSG_RESULT($vc_cv_ansi_cc)
if test "$vc_cv_ansi_cc" = "no"; then
	AC_WARN(
[Your compiler does not appear to recognize prototypes.  You have the following
choices:
	a. adjust your compiler options
	b. get an up-to-date compiler
	c. use a wrapper such as unproto])
	exit 1
elif test ".$vc_cv_ansi_cc" != ".-DCC_HAS_PROTOS"; then
	CC="$CC $vc_cv_ansi_cc"
fi
])
dnl ---------------------------------------------------------------------------
dnl Test if we should use ANSI-style prototype for qsort's compare-function
AC_DEFUN([VC_ANSI_QSORT],
[
AC_MSG_CHECKING([for standard qsort])
AC_CACHE_VAL(vc_cv_ansi_qsort,[
	AC_TRY_COMPILE([
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
	int compare(const void *a, const void *b)
	{ return (*(int *)a - *(int *)b); } ],
	[ extern long *vector;
	  qsort(vector, 1, 1, compare); ],
	[vc_cv_ansi_qsort=yes],
	[vc_cv_ansi_qsort=no])
])
AC_MSG_RESULT($vc_cv_ansi_qsort)
if test $vc_cv_ansi_qsort = yes; then
	AC_DEFINE(ANSI_QSORT,1)
else
	AC_DEFINE(ANSI_QSORT,0)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl
AC_DEFUN([VC_ERRNO],
[
AC_MSG_CHECKING([for errno external decl])
AC_CACHE_VAL(vc_cv_extern_errno,[
	AC_TRY_COMPILE([
#include <errno.h>],
		[int x = errno],
		[vc_cv_extern_errno=yes],
		[vc_cv_extern_errno=no])
	])
AC_MSG_RESULT($vc_cv_extern_errno)
test $vc_cv_extern_errno = yes && AC_DEFINE(HAVE_EXTERN_ERRNO)
])
dnl ---------------------------------------------------------------------------
dnl
dnl Note: must follow AC_FUNC_SETPGRP, but cannot use AC_REQUIRE, since that
dnl messes up the messages...
AC_DEFUN([VC_KILLPG],
[
AC_MSG_CHECKING([if killpg is needed])
AC_CACHE_VAL(vc_cv_need_killpg,[
AC_TRY_RUN([
#include <sys/types.h>
#include <signal.h>
RETSIGTYPE
handler(s)
    int s;
{
    exit(0);
}

main()
{
#ifdef SETPGRP_VOID
    (void) setpgrp();
#else
    (void) setpgrp(0,0);
#endif
    (void) signal(SIGINT, handler);
    (void) kill(-getpid(), SIGINT);
    exit(1);
}],
	[vc_cv_need_killpg=no],
	[vc_cv_need_killpg=yes],
	[vc_cv_need_killpg=unknown]
)])
AC_MSG_RESULT($vc_cv_need_killpg)
test $vc_cv_need_killpg = yes && AC_DEFINE(HAVE_KILLPG)
])dnl
dnl ---------------------------------------------------------------------------
dnl
AC_DEFUN([VC_MISSING_CHECK],
[
AC_MSG_CHECKING([for missing "$1" extern])
AC_CACHE_VAL([vc_cv_func_$1],[
AC_TRY_LINK([
#include <stdio.h>
#include <sys/types.h>
#include <setjmp.h>
#include <signal.h>
#include <errno.h>
#ifdef HAVE_TYPES_H
#include <types.h>
#endif
#ifdef HAVE_LIBC_H
#include <libc.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#else
#ifdef HAVE_VARARGS_H
#include <varargs.h>
#endif
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif
#if HAVE_UTIME_H
# include <utime.h>
#endif

#if STDC_HEADERS || HAVE_STRING_H
#include <string.h>
  /* An ANSI string.h and pre-ANSI memory.h might conflict.  */
#if !STDC_HEADERS && HAVE_MEMORY_H
#include <memory.h>
#endif /* not STDC_HEADERS and HAVE_MEMORY_H */
#else /* not STDC_HEADERS and not HAVE_STRING_H */
#if HAVE_STRINGS_H
#include <strings.h>
  /* memory.h and strings.h conflict on some systems */
#endif
#endif /* not STDC_HEADERS and not HAVE_STRING_H */

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

/* unistd.h defines _POSIX_VERSION on POSIX.1 systems.  */
#if defined(HAVE_DIRENT_H) || defined(_POSIX_VERSION)
#include <dirent.h>
#else /* not (HAVE_DIRENT_H or _POSIX_VERSION) */
#ifdef HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif /* HAVE_SYS_NDIR_H */
#ifdef HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif /* HAVE_SYS_DIR_H */
#ifdef HAVE_NDIR_H
#include <ndir.h>
#endif /* HAVE_NDIR_H */
#endif /* not (HAVE_DIRENT_H or _POSIX_VERSION) */

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#ifdef HAVE_STAT_H
#include <stat.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
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

#ifdef HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif
#ifdef HAVE_UCONTEXT_H
#include <ucontext.h>
#endif
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#else
#ifdef HAVE_BSD_REGEX_H
#include <bsd/regex.h>
#endif
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif
#ifdef HAVE_SYS_MSG_H
#include <sys/msg.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SELECT_H
#include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
/* SCO needs resource.h after select.h, to pick up timeval struct */
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif
#ifdef HAVE_IOCTL_H
# include <ioctl.h>
#else
# ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
# endif
#endif

#if USE_TERMINFO
# include <curses.h>
# include <term.h>
#else
#if HAVE_TERMCAP_H
#include <termcap.h>
#endif
#endif

#undef $1
struct zowie { int a; double b; struct zowie *c; char d; };
extern struct zowie *$1();
],
[
#if HAVE_LIBXT		/* needed for SunOS 4.0.3 or 4.1 */
XtToolkitInitialize();
#endif
],
[eval 'vc_cv_func_'$1'=yes'],
[eval 'vc_cv_func_'$1'=no'])])
eval 'vc_result=$vc_cv_func_'$1
AC_MSG_RESULT($vc_result)
test $vc_result = yes && AC_DEFINE_UNQUOTED(MISSING_EXTERN_$2)
])dnl
dnl ---------------------------------------------------------------------------
dnl
AC_DEFUN([VC_MISSING_EXTERN],
[for ac_func in $1
do
changequote(,)dnl
ac_tr_func=`echo $ac_func | tr '[a-z]' '[A-Z]'`
changequote([,])dnl
VC_MISSING_CHECK(${ac_func}, ${ac_tr_func})dnl
done
])dnl
dnl ---------------------------------------------------------------------------
dnl
dnl VC_RESTARTABLE_PIPEREAD is a modified version of AC_RESTARTABLE_SYSCALLS
dnl from acspecific.m4, which uses a read on a pipe (surprise!) rather than
dnl a wait() as the test code.  apparently there is a POSIX change, which OSF/1
dnl at least has adapted to, which says reads (or writes) on pipes for which
dnl no data has been transferred are interruptable _regardless_ of the 
dnl SA_RESTART bit.  yuck.
AC_DEFUN([VC_RESTARTABLE_PIPEREAD],
[
AC_MSG_CHECKING(for restartable reads on pipes)
AC_CACHE_VAL(vc_cv_can_restart_read,[
AC_TRY_RUN(
[/* Exit 0 (true) if wait returns something other than -1,
   i.e. the pid of the child, which means that wait was restarted
   after getting the signal.  */
#include <sys/types.h>
#include <signal.h>
#ifdef SA_RESTART
sigwrapper(sig, disp)
int sig;
void (*disp)();
{
    struct sigaction act, oact;

    act.sa_handler = disp;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;

    (void)sigaction(sig, &act, &oact);

}
#else
# define sigwrapper signal
#endif
ucatch (isig) { }
main () {
  int i, status;
  int fd[2];
  char buff[2];
  pipe(fd);
  i = fork();
  if (i == 0) {
      sleep (2);
      kill (getppid (), SIGINT);
      sleep (2);
      write(fd[1],"done",4);
      close(fd[1]);
      exit (0);
  }
  sigwrapper (SIGINT, ucatch);
  status = read(fd[0], buff, sizeof(buff));
  wait (&i);
  exit (status == -1);
}
],
[vc_cv_can_restart_read=yes],
[vc_cv_can_restart_read=no],
[vc_cv_can_restart_read=unknown])])
AC_MSG_RESULT($vc_cv_can_restart_read)
test $vc_cv_can_restart_read = yes && AC_DEFINE(HAVE_RESTARTABLE_PIPEREAD)
])dnl
dnl ---------------------------------------------------------------------------
dnl	Check for declarion of sys_errlist in one of stdio.h and errno.h.  
dnl	Declaration of sys_errlist on BSD4.4 interferes with our declaration.
dnl	Reported by Keith Bostic.
AC_DEFUN([VC_SYS_ERRLIST],
[
AC_MSG_CHECKING([declaration of sys_errlist])
AC_CACHE_VAL(vc_cv_dcl_sys_errlist,[
	AC_TRY_COMPILE([
#include <stdio.h>
#include <sys/types.h>
#include <errno.h> ],
	[ char *c = (char *) *sys_errlist; ],
	[vc_cv_dcl_sys_errlist=yes],
	[vc_cv_dcl_sys_errlist=no])
	])
AC_MSG_RESULT($vc_cv_dcl_sys_errlist)
test $vc_cv_dcl_sys_errlist = yes && AC_DEFINE(HAVE_EXTERN_SYS_ERRLIST)
])dnl
dnl ---------------------------------------------------------------------------
dnl	Check for return and param type of 3rd -- OutChar() -- param of tputs().
AC_DEFUN([VC_TYPE_OUTCHAR],
[AC_MSG_CHECKING([declaration of tputs 3rd param])
AC_CACHE_VAL(vc_cv_type_outchar,[
vc_cv_type_outchar="int OutChar(int)"
vc_cv_found=no
for P in int void; do
for Q in int char; do
for R in "" const; do
	AC_TRY_COMPILE([
#if USE_TERMINFO
#include <curses.h>
#include <term.h>
#else
#if HAVE_TERMCAP_H
#include <termcap.h>
#endif
#endif ],
	[extern $P OutChar($Q);
	extern int tputs ($R char *string, int nlines, $P (*_f)($Q));
	tputs("", 1, OutChar)],
	[vc_cv_type_outchar="$P OutChar($Q)"
	 vc_cv_found=yes
	 break])
done
	test $vc_cv_found = yes && break
done
	test $vc_cv_found = yes && break
done
	])
AC_MSG_RESULT($vc_cv_type_outchar)
case $vc_cv_type_outchar in
int*)
	AC_DEFINE(OUTC_RETURN)
	;;
esac
case $vc_cv_type_outchar in
*char*)
	AC_DEFINE(OUTC_ARGS,char c)
	;;
esac
])dnl
