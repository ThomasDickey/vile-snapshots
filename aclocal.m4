dnl Local definitions for autoconf.
dnl
dnl $Header: /users/source/archives/vile.vcs/RCS/aclocal.m4,v 1.33 1997/09/07 21:22:42 tom Exp $
dnl
dnl ---------------------------------------------------------------------------
dnl ---------------------------------------------------------------------------
dnl CF_PREREQ_COMPARE(MAJOR1, MINOR1, TERNARY1, MAJOR2, MINOR2, TERNARY2,
dnl                   PRINTABLE2, not FOUND, FOUND)
define(CF_PREREQ_COMPARE,
[ifelse(builtin([eval], [$3 < $6]), 1,
ifelse([$8], , ,[$8]),
ifelse([$9], , ,[$9]))])dnl
dnl ---------------------------------------------------------------------------
dnl Conditionally generate script according to whether we're using the release
dnl version of autoconf, or a patched version (using the ternary component as
dnl the patch-version).
define(CF_AC_PREREQ,
[CF_PREREQ_COMPARE(
AC_PREREQ_CANON(AC_PREREQ_SPLIT(AC_ACVERSION)),
AC_PREREQ_CANON(AC_PREREQ_SPLIT([$1])), [$1], [$2], [$3])])dnl
dnl ---------------------------------------------------------------------------
dnl This is adapted from the macros 'fp_PROG_CC_STDC' and 'fp_C_PROTOTYPES'
dnl in the sharutils 4.2 distribution.
AC_DEFUN([CF_ANSI_CC_CHECK],
[
AC_MSG_CHECKING(for ${CC-cc} option to accept ANSI C)
AC_CACHE_VAL(cf_cv_ansi_cc,[
cf_cv_ansi_cc=no
cf_save_CFLAGS="$CFLAGS"
# Don't try gcc -ansi; that turns off useful extensions and
# breaks some systems' header files.
# AIX			-qlanglvl=ansi
# Ultrix and OSF/1	-std1
# HP-UX			-Aa -D_HPUX_SOURCE
# SVR4			-Xc
# UnixWare 1.2		(cannot use -Xc, since ANSI/POSIX clashes)
for cf_arg in "-DCC_HAS_PROTOS" "" -qlanglvl=ansi -std1 "-Aa -D_HPUX_SOURCE" -Xc
do
	CFLAGS="$cf_save_CFLAGS $cf_arg"
	AC_TRY_COMPILE(
[
#ifndef CC_HAS_PROTOS
#if !defined(__STDC__) || __STDC__ != 1
choke me
#endif
#endif
],[
	int test (int i, double x);
	struct s1 {int (*f) (int a);};
	struct s2 {int (*f) (double a);};],
	[cf_cv_ansi_cc="$cf_arg"; break])
done
CFLAGS="$cf_save_CFLAGS"
])
AC_MSG_RESULT($cf_cv_ansi_cc)

if test "$cf_cv_ansi_cc" != "no"; then
if test ".$cf_cv_ansi_cc" != ".-DCC_HAS_PROTOS"; then
	CFLAGS="$CFLAGS $cf_cv_ansi_cc"
else
	AC_DEFINE(CC_HAS_PROTOS)
fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl For programs that must use an ANSI compiler, obtain compiler options that
dnl will make it recognize prototypes.  We'll do preprocessor checks in other
dnl macros, since tools such as unproto can fake prototypes, but only part of
dnl the preprocessor.
AC_DEFUN([CF_ANSI_CC_REQD],
[AC_REQUIRE([CF_ANSI_CC_CHECK])
if test "$cf_cv_ansi_cc" = "no"; then
	AC_ERROR(
[Your compiler does not appear to recognize prototypes.
You have the following choices:
	a. adjust your compiler options
	b. get an up-to-date compiler
	c. use a wrapper such as unproto])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Test if we should use ANSI-style prototype for qsort's compare-function
AC_DEFUN([CF_ANSI_QSORT],
[
AC_MSG_CHECKING([for standard qsort])
AC_CACHE_VAL(cf_cv_ansi_qsort,[
	AC_TRY_COMPILE([
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
	int compare(const void *a, const void *b)
	{ return (*(int *)a - *(int *)b); } ],
	[ extern long *vector;
	  qsort(vector, 1, 1, compare); ],
	[cf_cv_ansi_qsort=yes],
	[cf_cv_ansi_qsort=no])
])
AC_MSG_RESULT($cf_cv_ansi_qsort)
if test $cf_cv_ansi_qsort = yes; then
	AC_DEFINE(ANSI_QSORT,1)
else
	AC_DEFINE(ANSI_QSORT,0)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Allow user to disable a normally-on option.
AC_DEFUN([CF_ARG_DISABLE],
[CF_ARG_OPTION($1,[$2 (default: on)],[$3],[$4],yes)])dnl
dnl ---------------------------------------------------------------------------
dnl Restricted form of AC_ARG_ENABLE that ensures user doesn't give bogus
dnl values.
dnl
dnl Parameters:
dnl $1 = option name
dnl $2 = help-string 
dnl $3 = action to perform if option is not default
dnl $4 = action if perform if option is default
dnl $5 = default option value (either 'yes' or 'no')
AC_DEFUN([CF_ARG_OPTION],
[AC_ARG_ENABLE($1,[$2],[test "$enableval" != ifelse($5,no,yes,no) && enableval=ifelse($5,no,no,yes)
  if test "$enableval" != "$5" ; then
ifelse($3,,[    :]dnl
,[    $3]) ifelse($4,,,[
  else
    $4])
  fi],[enableval=$5 ifelse($4,,,[
  $4
])dnl
  ])])dnl
dnl ---------------------------------------------------------------------------
dnl Check if we're accidentally using a cache from a different machine.
dnl Derive the system name, as a check for reusing the autoconf cache.
dnl
dnl If we've packaged config.guess and config.sub, run that (since it does a
dnl better job than uname). 
AC_DEFUN([CF_CHECK_CACHE],
[
if test -f $srcdir/config.guess ; then
	AC_CANONICAL_HOST
	system_name="$host_os"
else
	system_name="`(uname -s -r) 2>/dev/null`"
	if test -z "$system_name" ; then
		system_name="`(hostname) 2>/dev/null`"
	fi
fi
test -n "$system_name" && AC_DEFINE_UNQUOTED(SYSTEM_NAME,"$system_name")
AC_CACHE_VAL(cf_cv_system_name,[cf_cv_system_name="$system_name"])

test -z "$system_name" && system_name="$cf_cv_system_name"
test -n "$cf_cv_system_name" && AC_MSG_RESULT("Configuring for $cf_cv_system_name")

if test ".$system_name" != ".$cf_cv_system_name" ; then
	AC_MSG_RESULT("Cached system name does not agree with actual")
	AC_ERROR("Please remove config.cache and try again.")
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl SVr4 curses should have term.h as well (where it puts the definitions of
dnl the low-level interface).  This may not be true in old/broken implementations,
dnl as well as in misconfigured systems (e.g., gcc configured for Solaris 2.4
dnl running with Solaris 2.5.1).
AC_DEFUN([CF_CURSES_TERM_H],
[
AC_MSG_CHECKING([for term.h])
AC_CACHE_VAL(cf_cv_have_term_h,[
	AC_TRY_COMPILE([
#include <curses.h>
#include <term.h>],
	[WINDOW *x],
	[cf_cv_have_term_h=yes],
	[cf_cv_have_term_h=no])
	])
AC_MSG_RESULT($cf_cv_have_term_h)
test $cf_cv_have_term_h = yes && AC_DEFINE(HAVE_TERM_H)
])dnl
dnl ---------------------------------------------------------------------------
dnl You can always use "make -n" to see the actual options, but it's hard to
dnl pick out/analyze warning messages when the compile-line is long.
AC_DEFUN([CF_DISABLE_ECHO],[
AC_MSG_CHECKING(if you want to see long compiling messages)
CF_ARG_DISABLE(echo,
	[  --disable-echo          test: display \"compiling\" commands],
	[
    ECHO_LD='@echo linking [$]@;'
    SHOW_CC='	@echo compiling [$]@'
    ECHO_CC='@'
],[
    ECHO_LD=''
    SHOW_CC='# compiling'
    ECHO_CC=''
])
AC_MSG_RESULT($enableval)
AC_SUBST(ECHO_LD)
AC_SUBST(SHOW_CC)
AC_SUBST(ECHO_CC)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if 'errno' is declared in <errno.h>
AC_DEFUN([CF_ERRNO],
[
AC_MSG_CHECKING([for errno external decl])
AC_CACHE_VAL(cf_cv_extern_errno,[
    AC_TRY_COMPILE([
#include <errno.h>],
        [int x = errno],
        [cf_cv_extern_errno=yes],
        [cf_cv_extern_errno=no])])
AC_MSG_RESULT($cf_cv_extern_errno)
test $cf_cv_extern_errno = no && AC_DEFINE(DECL_ERRNO)
])dnl
dnl ---------------------------------------------------------------------------
dnl Test for availability of useful gcc __attribute__ directives to quiet
dnl compiler warnings.  Though useful, not all are supported -- and contrary
dnl to documentation, unrecognized directives cause older compilers to barf.
AC_DEFUN([CF_GCC_ATTRIBUTES],
[
if test -n "$GCC"
then
cat > conftest.i <<EOF
#ifndef GCC_PRINTF
#define GCC_PRINTF 0
#endif
#ifndef GCC_SCANF
#define GCC_SCANF 0
#endif
#ifndef GCC_NORETURN
#define GCC_NORETURN /* nothing */
#endif
#ifndef GCC_UNUSED
#define GCC_UNUSED /* nothing */
#endif
EOF
if test -n "$GCC"
then
	AC_CHECKING([for gcc __attribute__ directives])
	changequote(,)dnl
cat > conftest.$ac_ext <<EOF
#line __oline__ "configure"
#include "confdefs.h"
#include "conftest.h"
#include "conftest.i"
#if	GCC_PRINTF
#define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))
#else
#define GCC_PRINTFLIKE(fmt,var) /*nothing*/
#endif
#if	GCC_SCANF
#define GCC_SCANFLIKE(fmt,var)  __attribute__((format(scanf,fmt,var)))
#else
#define GCC_SCANFLIKE(fmt,var)  /*nothing*/
#endif
extern void wow(char *,...) GCC_SCANFLIKE(1,2);
extern void oops(char *,...) GCC_PRINTFLIKE(1,2) GCC_NORETURN;
extern void foo(void) GCC_NORETURN;
int main(int argc GCC_UNUSED, char *argv[] GCC_UNUSED) { return 0; }
EOF
	changequote([,])dnl
	for cf_attribute in scanf printf unused noreturn
	do
		CF_UPPER(CF_ATTRIBUTE,$cf_attribute)
		cf_directive="__attribute__(($cf_attribute))"
		echo "checking for gcc $cf_directive" 1>&AC_FD_CC
		case $cf_attribute in
		scanf|printf)
		cat >conftest.h <<EOF
#define GCC_$CF_ATTRIBUTE 1
EOF
			;;
		*)
		cat >conftest.h <<EOF
#define GCC_$CF_ATTRIBUTE $cf_directive
EOF
			;;
		esac
		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... $cf_attribute)
			cat conftest.h >>confdefs.h
#		else
#			sed -e 's/__attr.*/\/*nothing*\//' conftest.h >>confdefs.h
		fi
	done
else
	fgrep define conftest.i >>confdefs.h
fi
rm -rf conftest*
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if the compiler supports useful warning options.  There's a few that
dnl we don't use, simply because they're too noisy:
dnl
dnl	-Wconversion (useful in older versions of gcc, but not in gcc 2.7.x)
dnl	-Wredundant-decls (system headers make this too noisy)
dnl	-Wtraditional (combines too many unrelated messages, only a few useful)
dnl	-Wwrite-strings (too noisy, but should review occasionally)
dnl	-pedantic
dnl
AC_DEFUN([CF_GCC_WARNINGS],
[EXTRA_CFLAGS=""
if test -n "$GCC"
then
	changequote(,)dnl
	cat > conftest.$ac_ext <<EOF
#line __oline__ "configure"
int main(int argc, char *argv[]) { return argv[argc-1] == 0; }
EOF
	changequote([,])dnl
	AC_CHECKING([for gcc warning options])
	cf_save_CFLAGS="$CFLAGS"
	EXTRA_CFLAGS="-W -Wall"
	for cf_opt in \
		Wbad-function-cast \
		Wcast-align \
		Wcast-qual \
		Winline \
		Wmissing-declarations \
		Wmissing-prototypes \
		Wnested-externs \
		Wpointer-arith \
		Wshadow \
		Wstrict-prototypes
	do
		CFLAGS="$cf_save_CFLAGS $EXTRA_CFLAGS -$cf_opt"
		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... -$cf_opt)
			EXTRA_CFLAGS="$EXTRA_CFLAGS -$cf_opt"
			test "$cf_opt" = Wcast-qual && EXTRA_CFLAGS="$EXTRA_CFLAGS -DXTSTRINGDEFINES"
		fi
	done
	rm -f conftest*
	CFLAGS="$cf_save_CFLAGS"
fi
AC_SUBST(EXTRA_CFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl Note: must follow AC_FUNC_SETPGRP, but cannot use AC_REQUIRE, since that
dnl messes up the messages...
AC_DEFUN([CF_KILLPG],
[
AC_MSG_CHECKING([if killpg is needed])
AC_CACHE_VAL(cf_cv_need_killpg,[
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
	[cf_cv_need_killpg=no],
	[cf_cv_need_killpg=yes],
	[cf_cv_need_killpg=unknown]
)])
AC_MSG_RESULT($cf_cv_need_killpg)
test $cf_cv_need_killpg = yes && AC_DEFINE(HAVE_KILLPG)
])dnl
dnl ---------------------------------------------------------------------------
dnl
AC_DEFUN([CF_MISSING_CHECK],
[
AC_MSG_CHECKING([for missing "$1" extern])
AC_CACHE_VAL([cf_cv_func_$1],[
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
# if HAVE_TERM_H
#  include <term.h>
# endif
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
[eval 'cf_cv_func_'$1'=yes'],
[eval 'cf_cv_func_'$1'=no'])])
eval 'cf_result=$cf_cv_func_'$1
AC_MSG_RESULT($cf_result)
test $cf_result = yes && AC_DEFINE_UNQUOTED(MISSING_EXTERN_$2)
])dnl
dnl ---------------------------------------------------------------------------
dnl
AC_DEFUN([CF_MISSING_EXTERN],
[for ac_func in $1
do
CF_UPPER(ac_tr_func,$ac_func)
CF_MISSING_CHECK(${ac_func}, ${ac_tr_func})dnl
done
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_RESTARTABLE_PIPEREAD is a modified version of AC_RESTARTABLE_SYSCALLS
dnl from acspecific.m4, which uses a read on a pipe (surprise!) rather than a
dnl wait() as the test code.  apparently there is a POSIX change, which OSF/1
dnl at least has adapted to, which says reads (or writes) on pipes for which no
dnl data has been transferred are interruptable _regardless_ of the SA_RESTART
dnl bit.  yuck.
AC_DEFUN([CF_RESTARTABLE_PIPEREAD],
[
AC_MSG_CHECKING(for restartable reads on pipes)
AC_CACHE_VAL(cf_cv_can_restart_read,[
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
[cf_cv_can_restart_read=yes],
[cf_cv_can_restart_read=no],
[cf_cv_can_restart_read=unknown])])
AC_MSG_RESULT($cf_cv_can_restart_read)
test $cf_cv_can_restart_read = yes && AC_DEFINE(HAVE_RESTARTABLE_PIPEREAD)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for declaration of sys_errlist in one of stdio.h and errno.h.
dnl Declaration of sys_errlist on BSD4.4 interferes with our declaration.
dnl Reported by Keith Bostic.
AC_DEFUN([CF_SYS_ERRLIST],
[
AC_MSG_CHECKING([declaration of sys_errlist])
AC_CACHE_VAL(cf_cv_dcl_sys_errlist,[
    AC_TRY_COMPILE([
#include <stdio.h>
#include <sys/types.h>
#include <errno.h> ],
    [char *c = (char *) *sys_errlist],
    [cf_cv_dcl_sys_errlist=yes],
    [cf_cv_dcl_sys_errlist=no])])
AC_MSG_RESULT($cf_cv_dcl_sys_errlist)

# It's possible (for near-UNIX clones) that sys_errlist doesn't exist
if test $cf_cv_dcl_sys_errlist = no ; then
    AC_DEFINE(DECL_SYS_ERRLIST)
    AC_MSG_CHECKING([existence of sys_errlist])
    AC_CACHE_VAL(cf_cv_have_sys_errlist,[
        AC_TRY_LINK([#include <errno.h>],
            [char *c = (char *) *sys_errlist],
            [cf_cv_have_sys_errlist=yes],
            [cf_cv_have_sys_errlist=no])])
    AC_MSG_RESULT($cf_cv_have_sys_errlist)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for return and param type of 3rd -- OutChar() -- param of tputs().
AC_DEFUN([CF_TYPE_OUTCHAR],
[AC_MSG_CHECKING([declaration of tputs 3rd param])
AC_CACHE_VAL(cf_cv_type_outchar,[
cf_cv_type_outchar="int OutChar(int)"
cf_cv_found=no
for P in int void; do
for Q in int void; do
for R in int char; do
for S in "" const; do
	AC_TRY_COMPILE([
#ifdef USE_TERMINFO
#include <curses.h>
#if HAVE_TERM_H
#include <term.h>
#endif
#else
#if HAVE_CURSES_H
#include <curses.h>	/* FIXME: this should be included only for terminfo */
#endif
#if HAVE_TERMCAP_H
#include <termcap.h>
#endif
#endif ],
	[extern $Q OutChar($R);
	extern $P tputs ($S char *string, int nlines, $Q (*_f)($R));
	tputs("", 1, OutChar)],
	[cf_cv_type_outchar="$Q OutChar($R)"
	 cf_cv_found=yes
	 break])
done
	test $cf_cv_found = yes && break
done
	test $cf_cv_found = yes && break
done
	test $cf_cv_found = yes && break
done
	])
AC_MSG_RESULT($cf_cv_type_outchar)
case $cf_cv_type_outchar in
int*)
	AC_DEFINE(OUTC_RETURN)
	;;
esac
case $cf_cv_type_outchar in
*char*)
	AC_DEFINE(OUTC_ARGS,char)
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl Make an uppercase version of a variable
dnl $1=uppercase($2)
AC_DEFUN([CF_UPPER],
[
changequote(,)dnl
$1=`echo $2 | tr '[a-z]' '[A-Z]'`
changequote([,])dnl
])dnl
