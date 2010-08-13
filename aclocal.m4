dnl vile's local definitions for autoconf.
dnl
dnl $Header: /users/source/archives/vile.vcs/RCS/aclocal.m4,v 1.225 2010/08/13 08:47:48 tom Exp $
dnl
dnl See
dnl		http://invisible-island.net/autoconf/autoconf.html
dnl ---------------------------------------------------------------------------
dnl ---------------------------------------------------------------------------
dnl AM_ICONV version: 12 updated: 2007/07/30 19:12:03
dnl --------
dnl Inserted as requested by gettext 0.10.40
dnl File from /usr/share/aclocal
dnl iconv.m4
dnl ====================
dnl serial AM2
dnl
dnl From Bruno Haible.
dnl
dnl ====================
dnl Modified to use CF_FIND_LINKAGE and CF_ADD_SEARCHPATH, to broaden the
dnl range of locations searched.  Retain the same cache-variable naming to
dnl allow reuse with the other gettext macros -Thomas E Dickey
AC_DEFUN([AM_ICONV],
[
  dnl Some systems have iconv in libc, some have it in libiconv (OSF/1 and
  dnl those with the standalone portable GNU libiconv installed).

  AC_ARG_WITH([libiconv-prefix],
[  --with-libiconv-prefix=DIR
                          search for libiconv in DIR/include and DIR/lib], [
    CF_ADD_OPTIONAL_PATH($withval, libiconv)
   ])

  AC_CACHE_CHECK(for iconv, am_cv_func_iconv, [
    CF_FIND_LINKAGE(CF__ICONV_HEAD,
      CF__ICONV_BODY,
      iconv,
      am_cv_func_iconv=yes,
      am_cv_func_iconv=["no, consider installing GNU libiconv"])])

  if test "$am_cv_func_iconv" = yes; then
    AC_DEFINE(HAVE_ICONV, 1, [Define if you have the iconv() function.])

    AC_CACHE_CHECK([if the declaration of iconv() needs const.],
		   am_cv_proto_iconv_const,[
      AC_TRY_COMPILE(CF__ICONV_HEAD [
extern
#ifdef __cplusplus
"C"
#endif
#if defined(__STDC__) || defined(__cplusplus)
size_t iconv (iconv_t cd, char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);
#else
size_t iconv();
#endif
],[], am_cv_proto_iconv_const=no,
      am_cv_proto_iconv_const=yes)])

    if test "$am_cv_proto_iconv_const" = yes ; then
      am_cv_proto_iconv_arg1="const"
    else
      am_cv_proto_iconv_arg1=""
    fi

    AC_DEFINE_UNQUOTED(ICONV_CONST, $am_cv_proto_iconv_arg1,
      [Define as const if the declaration of iconv() needs const.])
  fi

  LIBICONV=
  if test "$cf_cv_find_linkage_iconv" = yes; then
    CF_ADD_INCDIR($cf_cv_header_path_iconv)
    if test -n "$cf_cv_library_file_iconv" ; then
      LIBICONV="-liconv"
      CF_ADD_LIBDIR($cf_cv_library_path_iconv)
    fi
  fi

  AC_SUBST(LIBICONV)
])dnl
dnl ---------------------------------------------------------------------------
dnl AM_LANGINFO_CODESET version: 3 updated: 2002/10/27 23:21:42
dnl -------------------
dnl Inserted as requested by gettext 0.10.40
dnl File from /usr/share/aclocal
dnl codeset.m4
dnl ====================
dnl serial AM1
dnl
dnl From Bruno Haible.
AC_DEFUN([AM_LANGINFO_CODESET],
[
  AC_CACHE_CHECK([for nl_langinfo and CODESET], am_cv_langinfo_codeset,
    [AC_TRY_LINK([#include <langinfo.h>],
      [char* cs = nl_langinfo(CODESET);],
      am_cv_langinfo_codeset=yes,
      am_cv_langinfo_codeset=no)
    ])
  if test $am_cv_langinfo_codeset = yes; then
    AC_DEFINE(HAVE_LANGINFO_CODESET, 1,
      [Define if you have <langinfo.h> and nl_langinfo(CODESET).])
  fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_AC_PREREQ version: 2 updated: 1997/09/06 13:24:56
dnl ------------
dnl Conditionally generate script according to whether we're using the release
dnl version of autoconf, or a patched version (using the ternary component as
dnl the patch-version).
define(CF_AC_PREREQ,
[CF_PREREQ_COMPARE(
AC_PREREQ_CANON(AC_PREREQ_SPLIT(AC_ACVERSION)),
AC_PREREQ_CANON(AC_PREREQ_SPLIT([$1])), [$1], [$2], [$3])])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_CFLAGS version: 10 updated: 2010/05/26 05:38:42
dnl -------------
dnl Copy non-preprocessor flags to $CFLAGS, preprocessor flags to $CPPFLAGS
dnl The second parameter if given makes this macro verbose.
dnl
dnl Put any preprocessor definitions that use quoted strings in $EXTRA_CPPFLAGS,
dnl to simplify use of $CPPFLAGS in compiler checks, etc., that are easily
dnl confused by the quotes (which require backslashes to keep them usable).
AC_DEFUN([CF_ADD_CFLAGS],
[
cf_fix_cppflags=no
cf_new_cflags=
cf_new_cppflags=
cf_new_extra_cppflags=

for cf_add_cflags in $1
do
case $cf_fix_cppflags in
no)
	case $cf_add_cflags in #(vi
	-undef|-nostdinc*|-I*|-D*|-U*|-E|-P|-C) #(vi
		case $cf_add_cflags in
		-D*)
			cf_tst_cflags=`echo ${cf_add_cflags} |sed -e 's/^-D[[^=]]*='\''\"[[^"]]*//'`

			test "${cf_add_cflags}" != "${cf_tst_cflags}" \
				&& test -z "${cf_tst_cflags}" \
				&& cf_fix_cppflags=yes

			if test $cf_fix_cppflags = yes ; then
				cf_new_extra_cppflags="$cf_new_extra_cppflags $cf_add_cflags"
				continue
			elif test "${cf_tst_cflags}" = "\"'" ; then
				cf_new_extra_cppflags="$cf_new_extra_cppflags $cf_add_cflags"
				continue
			fi
			;;
		esac
		case "$CPPFLAGS" in
		*$cf_add_cflags) #(vi
			;;
		*) #(vi
			case $cf_add_cflags in #(vi
			-D*)
				cf_tst_cppflags=`echo "x$cf_add_cflags" | sed -e 's/^...//' -e 's/=.*//'`
				CF_REMOVE_DEFINE(CPPFLAGS,$CPPFLAGS,$cf_tst_cppflags)
				;;
			esac
			cf_new_cppflags="$cf_new_cppflags $cf_add_cflags"
			;;
		esac
		;;
	*)
		cf_new_cflags="$cf_new_cflags $cf_add_cflags"
		;;
	esac
	;;
yes)
	cf_new_extra_cppflags="$cf_new_extra_cppflags $cf_add_cflags"

	cf_tst_cflags=`echo ${cf_add_cflags} |sed -e 's/^[[^"]]*"'\''//'`

	test "${cf_add_cflags}" != "${cf_tst_cflags}" \
		&& test -z "${cf_tst_cflags}" \
		&& cf_fix_cppflags=no
	;;
esac
done

if test -n "$cf_new_cflags" ; then
	ifelse([$2],,,[CF_VERBOSE(add to \$CFLAGS $cf_new_cflags)])
	CFLAGS="$CFLAGS $cf_new_cflags"
fi

if test -n "$cf_new_cppflags" ; then
	ifelse([$2],,,[CF_VERBOSE(add to \$CPPFLAGS $cf_new_cppflags)])
	CPPFLAGS="$CPPFLAGS $cf_new_cppflags"
fi

if test -n "$cf_new_extra_cppflags" ; then
	ifelse([$2],,,[CF_VERBOSE(add to \$EXTRA_CPPFLAGS $cf_new_extra_cppflags)])
	EXTRA_CPPFLAGS="$cf_new_extra_cppflags $EXTRA_CPPFLAGS"
fi

AC_SUBST(EXTRA_CPPFLAGS)

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_INCDIR version: 13 updated: 2010/05/26 16:44:57
dnl -------------
dnl Add an include-directory to $CPPFLAGS.  Don't add /usr/include, since it's
dnl redundant.  We don't normally need to add -I/usr/local/include for gcc,
dnl but old versions (and some misinstalled ones) need that.  To make things
dnl worse, gcc 3.x may give error messages if -I/usr/local/include is added to
dnl the include-path).
AC_DEFUN([CF_ADD_INCDIR],
[
if test -n "$1" ; then
  for cf_add_incdir in $1
  do
	while test $cf_add_incdir != /usr/include
	do
	  if test -d $cf_add_incdir
	  then
		cf_have_incdir=no
		if test -n "$CFLAGS$CPPFLAGS" ; then
		  # a loop is needed to ensure we can add subdirs of existing dirs
		  for cf_test_incdir in $CFLAGS $CPPFLAGS ; do
			if test ".$cf_test_incdir" = ".-I$cf_add_incdir" ; then
			  cf_have_incdir=yes; break
			fi
		  done
		fi

		if test "$cf_have_incdir" = no ; then
		  if test "$cf_add_incdir" = /usr/local/include ; then
			if test "$GCC" = yes
			then
			  cf_save_CPPFLAGS=$CPPFLAGS
			  CPPFLAGS="$CPPFLAGS -I$cf_add_incdir"
			  AC_TRY_COMPILE([#include <stdio.h>],
				  [printf("Hello")],
				  [],
				  [cf_have_incdir=yes])
			  CPPFLAGS=$cf_save_CPPFLAGS
			fi
		  fi
		fi

		if test "$cf_have_incdir" = no ; then
		  CF_VERBOSE(adding $cf_add_incdir to include-path)
		  ifelse([$2],,CPPFLAGS,[$2])="$ifelse([$2],,CPPFLAGS,[$2]) -I$cf_add_incdir"

		  cf_top_incdir=`echo $cf_add_incdir | sed -e 's%/include/.*$%/include%'`
		  test "$cf_top_incdir" = "$cf_add_incdir" && break
		  cf_add_incdir="$cf_top_incdir"
		else
		  break
		fi
	  fi
	done
  done
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_LIB version: 2 updated: 2010/06/02 05:03:05
dnl ----------
dnl Add a library, used to enforce consistency.
dnl
dnl $1 = library to add, without the "-l"
dnl $2 = variable to update (default $LIBS)
AC_DEFUN([CF_ADD_LIB],[CF_ADD_LIBS(-l$1,ifelse($2,,LIBS,[$2]))])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_LIBDIR version: 9 updated: 2010/05/26 16:44:57
dnl -------------
dnl	Adds to the library-path
dnl
dnl	Some machines have trouble with multiple -L options.
dnl
dnl $1 is the (list of) directory(s) to add
dnl $2 is the optional name of the variable to update (default LDFLAGS)
dnl
AC_DEFUN([CF_ADD_LIBDIR],
[
if test -n "$1" ; then
  for cf_add_libdir in $1
  do
    if test $cf_add_libdir = /usr/lib ; then
      :
    elif test -d $cf_add_libdir
    then
      cf_have_libdir=no
      if test -n "$LDFLAGS$LIBS" ; then
        # a loop is needed to ensure we can add subdirs of existing dirs
        for cf_test_libdir in $LDFLAGS $LIBS ; do
          if test ".$cf_test_libdir" = ".-L$cf_add_libdir" ; then
            cf_have_libdir=yes; break
          fi
        done
      fi
      if test "$cf_have_libdir" = no ; then
        CF_VERBOSE(adding $cf_add_libdir to library-path)
        ifelse([$2],,LDFLAGS,[$2])="-L$cf_add_libdir $ifelse([$2],,LDFLAGS,[$2])"
      fi
    fi
  done
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_LIBS version: 1 updated: 2010/06/02 05:03:05
dnl -----------
dnl Add one or more libraries, used to enforce consistency.
dnl
dnl $1 = libraries to add, with the "-l", etc.
dnl $2 = variable to update (default $LIBS)
AC_DEFUN([CF_ADD_LIBS],[ifelse($2,,LIBS,[$2])="$1 [$]ifelse($2,,LIBS,[$2])"])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_OPTIONAL_PATH version: 1 updated: 2007/07/29 12:33:33
dnl --------------------
dnl Add an optional search-path to the compile/link variables.
dnl See CF_WITH_PATH
dnl
dnl $1 = shell variable containing the result of --with-XXX=[DIR]
dnl $2 = module to look for.
AC_DEFUN([CF_ADD_OPTIONAL_PATH],[
  case "$1" in #(vi
  no) #(vi
      ;;
  yes) #(vi
      ;;
  *)
      CF_ADD_SEARCHPATH([$1], [AC_MSG_ERROR(cannot find $2 under $1)])
      ;;
  esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_SEARCHPATH version: 5 updated: 2009/01/11 20:40:21
dnl -----------------
dnl Set $CPPFLAGS and $LDFLAGS with the directories given via the parameter.
dnl They can be either the common root of include- and lib-directories, or the
dnl lib-directory (to allow for things like lib64 directories).
dnl See also CF_FIND_LINKAGE.
dnl
dnl $1 is the list of colon-separated directory names to search.
dnl $2 is the action to take if a parameter does not yield a directory.
AC_DEFUN([CF_ADD_SEARCHPATH],
[
AC_REQUIRE([CF_PATHSEP])
for cf_searchpath in `echo "$1" | tr $PATH_SEPARATOR ' '`; do
	if test -d $cf_searchpath/include; then
		CF_ADD_INCDIR($cf_searchpath/include)
	elif test -d $cf_searchpath/../include ; then
		CF_ADD_INCDIR($cf_searchpath/../include)
	ifelse([$2],,,[else
$2])
	fi
	if test -d $cf_searchpath/lib; then
		CF_ADD_LIBDIR($cf_searchpath/lib)
	elif test -d $cf_searchpath ; then
		CF_ADD_LIBDIR($cf_searchpath)
	ifelse([$2],,,[else
$2])
	fi
done
])
dnl ---------------------------------------------------------------------------
dnl CF_ADD_SUBDIR_PATH version: 3 updated: 2010/07/03 20:58:12
dnl ------------------
dnl Append to a search-list for a nonstandard header/lib-file
dnl	$1 = the variable to return as result
dnl	$2 = the package name
dnl	$3 = the subdirectory, e.g., bin, include or lib
dnl $4 = the directory under which we will test for subdirectories
dnl $5 = a directory that we do not want $4 to match
AC_DEFUN([CF_ADD_SUBDIR_PATH],
[
test "$4" != "$5" && \
test -d "$4" && \
ifelse([$5],NONE,,[(test $5 = NONE || test "$4" != "$5") &&]) {
	test -n "$verbose" && echo "	... testing for $3-directories under $4"
	test -d $4/$3 &&          $1="[$]$1 $4/$3"
	test -d $4/$3/$2 &&       $1="[$]$1 $4/$3/$2"
	test -d $4/$3/$2/$3 &&    $1="[$]$1 $4/$3/$2/$3"
	test -d $4/$2/$3 &&       $1="[$]$1 $4/$2/$3"
	test -d $4/$2/$3/$2 &&    $1="[$]$1 $4/$2/$3/$2"
}
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ANSI_CC_CHECK version: 9 updated: 2001/12/30 17:53:34
dnl ----------------
dnl This is adapted from the macros 'fp_PROG_CC_STDC' and 'fp_C_PROTOTYPES'
dnl in the sharutils 4.2 distribution.
AC_DEFUN([CF_ANSI_CC_CHECK],
[
AC_CACHE_CHECK(for ${CC-cc} option to accept ANSI C, cf_cv_ansi_cc,[
cf_cv_ansi_cc=no
cf_save_CFLAGS="$CFLAGS"
cf_save_CPPFLAGS="$CPPFLAGS"
# Don't try gcc -ansi; that turns off useful extensions and
# breaks some systems' header files.
# AIX			-qlanglvl=ansi
# Ultrix and OSF/1	-std1
# HP-UX			-Aa -D_HPUX_SOURCE
# SVR4			-Xc
# UnixWare 1.2		(cannot use -Xc, since ANSI/POSIX clashes)
for cf_arg in "-DCC_HAS_PROTOS" \
	"" \
	-qlanglvl=ansi \
	-std1 \
	-Ae \
	"-Aa -D_HPUX_SOURCE" \
	-Xc
do
	CF_ADD_CFLAGS($cf_arg)
	AC_TRY_COMPILE(
[
#ifndef CC_HAS_PROTOS
#if !defined(__STDC__) || (__STDC__ != 1)
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
CPPFLAGS="$cf_save_CPPFLAGS"
])

if test "$cf_cv_ansi_cc" != "no"; then
if test ".$cf_cv_ansi_cc" != ".-DCC_HAS_PROTOS"; then
	CF_ADD_CFLAGS($cf_cv_ansi_cc)
else
	AC_DEFINE(CC_HAS_PROTOS)
fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ANSI_CC_REQD version: 4 updated: 2008/03/23 14:48:54
dnl ---------------
dnl For programs that must use an ANSI compiler, obtain compiler options that
dnl will make it recognize prototypes.  We'll do preprocessor checks in other
dnl macros, since tools such as unproto can fake prototypes, but only part of
dnl the preprocessor.
AC_DEFUN([CF_ANSI_CC_REQD],
[AC_REQUIRE([CF_ANSI_CC_CHECK])
if test "$cf_cv_ansi_cc" = "no"; then
	AC_MSG_ERROR(
[Your compiler does not appear to recognize prototypes.
You have the following choices:
	a. adjust your compiler options
	b. get an up-to-date compiler
	c. use a wrapper such as unproto])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ARG_DISABLE version: 3 updated: 1999/03/30 17:24:31
dnl --------------
dnl Allow user to disable a normally-on option.
AC_DEFUN([CF_ARG_DISABLE],
[CF_ARG_OPTION($1,[$2],[$3],[$4],yes)])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ARG_ENABLE version: 3 updated: 1999/03/30 17:24:31
dnl -------------
dnl Allow user to enable a normally-off option.
AC_DEFUN([CF_ARG_ENABLE],
[CF_ARG_OPTION($1,[$2],[$3],[$4],no)])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ARG_OPTION version: 4 updated: 2010/05/26 05:38:42
dnl -------------
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
[AC_ARG_ENABLE([$1],[$2],[test "$enableval" != ifelse([$5],no,yes,no) && enableval=ifelse([$5],no,no,yes)
  if test "$enableval" != "$5" ; then
ifelse([$3],,[    :]dnl
,[    $3]) ifelse([$4],,,[
  else
    $4])
  fi],[enableval=$5 ifelse([$4],,,[
  $4
])dnl
  ])])dnl
dnl ---------------------------------------------------------------------------
dnl CF_AUTO_SYMLINK version: 2 updated: 2010/01/27 20:39:52
dnl ---------------
dnl If CF_WITH_SYMLINK is set, but the program transformation is inactive,
dnl override it to provide a version-suffix to the result.  This assumes that
dnl a "version.sh" script is in the source directory.
dnl
dnl $1 = variable to substitute
AC_DEFUN([CF_AUTO_SYMLINK],[
AC_REQUIRE([CF_WITH_SYMLINK])dnl

if test "$with_symlink" != no ; then
	if test "[$]$1" = NONE ; then
		cf_version=`$SHELL $srcdir/version.sh 2>/dev/null`
		if test -n "$cf_version" ; then
			if test "$program_suffix" = NONE ; then
				program_suffix="-$cf_version"
			else
				program_suffix="$program_suffix-$cf_version"
			fi
			CF_TRANSFORM_PROGRAM
			$1="$with_symlink"
		else
			AC_MSG_ERROR(No version number found in sources)
		fi
	fi
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_CC_INIT_UNIONS version: 2 updated: 1998/07/01 22:16:27
dnl -----------------
dnl Check if the C compiler supports initialization of unions.
AC_DEFUN([CF_CC_INIT_UNIONS],[
AC_CACHE_CHECK(if we can initialize unions,
cf_cv_init_unions,[
	AC_TRY_COMPILE([],
	[static struct foo {int x; union {double a; int b; } bar; } c = {0,{1.0}}],
	[cf_cv_init_unions=yes],
	[cf_cv_init_unions=no])
	])
test $cf_cv_init_unions = no && AC_DEFINE(CC_CANNOT_INIT_UNIONS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CHAR_DEVICE version: 3 updated: 2002/10/07 22:10:29
dnl --------------
dnl Check for existence of the given character-device
AC_DEFUN([CF_CHAR_DEVICE],
[
AC_MSG_CHECKING(for /dev/tty)
if test -c $1 ; then
	cf_result=yes
else
	cf_result=no
fi
AC_MSG_RESULT($cf_result)
if test "$cf_result" = yes ; then
	cf_result=`echo $1 | sed -e s%/%_%g`
	CF_UPPER(cf_result,$cf_result)
	AC_DEFINE_UNQUOTED(HAVE$cf_result)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CHECK_CACHE version: 11 updated: 2008/03/23 14:45:59
dnl --------------
dnl Check if we're accidentally using a cache from a different machine.
dnl Derive the system name, as a check for reusing the autoconf cache.
dnl
dnl If we've packaged config.guess and config.sub, run that (since it does a
dnl better job than uname).  Normally we'll use AC_CANONICAL_HOST, but allow
dnl an extra parameter that we may override, e.g., for AC_CANONICAL_SYSTEM
dnl which is useful in cross-compiles.
dnl
dnl Note: we would use $ac_config_sub, but that is one of the places where
dnl autoconf 2.5x broke compatibility with autoconf 2.13
AC_DEFUN([CF_CHECK_CACHE],
[
if test -f $srcdir/config.guess || test -f $ac_aux_dir/config.guess ; then
	ifelse([$1],,[AC_CANONICAL_HOST],[$1])
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
test -n "$cf_cv_system_name" && AC_MSG_RESULT(Configuring for $cf_cv_system_name)

if test ".$system_name" != ".$cf_cv_system_name" ; then
	AC_MSG_RESULT(Cached system name ($system_name) does not agree with actual ($cf_cv_system_name))
	AC_MSG_ERROR("Please remove config.cache and try again.")
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CHECK_CFLAGS version: 2 updated: 2001/12/30 19:09:58
dnl ---------------
dnl Conditionally add to $CFLAGS and $CPPFLAGS values which are derived from
dnl a build-configuration such as imake.  These have the pitfall that they
dnl often contain compiler-specific options which we cannot use, mixed with
dnl preprocessor options that we usually can.
AC_DEFUN([CF_CHECK_CFLAGS],
[
CF_VERBOSE(checking additions to CFLAGS)
cf_check_cflags="$CFLAGS"
cf_check_cppflags="$CPPFLAGS"
CF_ADD_CFLAGS($1,yes)
if test "$cf_check_cflags" != "$CFLAGS" ; then
AC_TRY_LINK([#include <stdio.h>],[printf("Hello world");],,
	[CF_VERBOSE(test-compile failed.  Undoing change to \$CFLAGS)
	 if test "$cf_check_cppflags" != "$CPPFLAGS" ; then
		 CF_VERBOSE(but keeping change to \$CPPFLAGS)
	 fi
	 CFLAGS="$cf_check_flags"])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CHECK_ENVIRON version: 3 updated: 2010/05/26 16:44:57
dnl ----------------
dnl Check for data that is usually declared in <unistd.h>, e.g., the 'environ'
dnl variable.  Define a DECL_xxx symbol if we must declare it ourselves.
dnl
dnl $1 = the name to check
dnl $2 = the assumed type
AC_DEFUN([CF_CHECK_ENVIRON],
[
AC_CACHE_CHECK(if external $1 is declared, cf_cv_dcl_$1,[
    AC_TRY_COMPILE([
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <unistd.h> ],
    ifelse([$2],,int,[$2]) x = (ifelse([$2],,int,[$2])) $1,
    [cf_cv_dcl_$1=yes],
    [cf_cv_dcl_$1=no])
])

if test "$cf_cv_dcl_$1" = no ; then
    CF_UPPER(cf_result,decl_$1)
    AC_DEFINE_UNQUOTED($cf_result)
fi

# It's possible (for near-UNIX clones) that the data doesn't exist
CF_CHECK_EXTERN_DATA($1,ifelse([$2],,int,[$2]))
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CHECK_ERRNO version: 11 updated: 2010/05/26 05:38:42
dnl --------------
dnl Check for data that is usually declared in <stdio.h> or <errno.h>, e.g.,
dnl the 'errno' variable.  Define a DECL_xxx symbol if we must declare it
dnl ourselves.
dnl
dnl $1 = the name to check
dnl $2 = the assumed type
AC_DEFUN([CF_CHECK_ERRNO],
[
AC_CACHE_CHECK(if external $1 is declared, cf_cv_dcl_$1,[
    AC_TRY_COMPILE([
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>
#include <sys/types.h>
#include <errno.h> ],
    ifelse([$2],,int,[$2]) x = (ifelse([$2],,int,[$2])) $1,
    [cf_cv_dcl_$1=yes],
    [cf_cv_dcl_$1=no])
])

if test "$cf_cv_dcl_$1" = no ; then
    CF_UPPER(cf_result,decl_$1)
    AC_DEFINE_UNQUOTED($cf_result)
fi

# It's possible (for near-UNIX clones) that the data doesn't exist
CF_CHECK_EXTERN_DATA($1,ifelse([$2],,int,[$2]))
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CHECK_EXTERN_DATA version: 3 updated: 2001/12/30 18:03:23
dnl --------------------
dnl Check for existence of external data in the current set of libraries.  If
dnl we can modify it, it's real enough.
dnl $1 = the name to check
dnl $2 = its type
AC_DEFUN([CF_CHECK_EXTERN_DATA],
[
AC_CACHE_CHECK(if external $1 exists, cf_cv_have_$1,[
    AC_TRY_LINK([
#undef $1
extern $2 $1;
],
    [$1 = 2],
    [cf_cv_have_$1=yes],
    [cf_cv_have_$1=no])
])

if test "$cf_cv_have_$1" = yes ; then
    CF_UPPER(cf_result,have_$1)
    AC_DEFINE_UNQUOTED($cf_result)
fi

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CHECK_FD_SET version: 4 updated: 2002/10/09 20:00:37
dnl ---------------
dnl Check if the fd_set type and corresponding macros are defined.
AC_DEFUN([CF_CHECK_FD_SET],
[
AC_REQUIRE([CF_TYPE_FD_SET])
AC_CACHE_CHECK([for fd_set macros],cf_cv_macros_fd_set,[
AC_TRY_COMPILE([
#include <sys/types.h>
#if USE_SYS_SELECT_H
# include <sys/select.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#  ifdef TIME_WITH_SYS_TIME
#   include <time.h>
#  endif
# else
#  include <time.h>
# endif
#endif
],[
	fd_set read_bits;
	FD_ZERO(&read_bits);
	FD_SET(0, &read_bits);],
	[cf_cv_macros_fd_set=yes],
	[cf_cv_macros_fd_set=no])])
test $cf_cv_macros_fd_set = yes && AC_DEFINE(HAVE_TYPE_FD_SET)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CRYPT_FUNC version: 4 updated: 2007/04/28 09:15:55
dnl -------------
dnl Check if we have a working crypt() function
AC_DEFUN([CF_CRYPT_FUNC],
[
AC_CACHE_CHECK(for crypt function,cf_cv_crypt_func,[
cf_cv_crypt_func=
AC_TRY_LINK([],[crypt()],[
	cf_cv_crypt_func=yes],[
	cf_save_LIBS="$LIBS"
	LIBS="-lcrypt $LIBS"
	AC_TRY_LINK([],[crypt()],[
		cf_cv_crypt_func="-lcrypt"],[
		cf_cv_crypt_func=no])
	LIBS="$cf_save_LIBS"
	])
])
if test "$cf_cv_crypt_func" != no ; then
	cf_save_LIBS="$LIBS"
	test "$cf_cv_crypt_func" != yes && LIBS="$cf_cv_crypt_func $LIBS"
AC_CACHE_CHECK(if crypt works,cf_cv_crypt_works,[
AC_TRY_RUN([
#include <string.h>
extern char *crypt();
int main() {
	char *s = crypt("vi-crypt", "vi");
	${cf_cv_main_return-return}(strcmp("vi6r2tczBYLvM", s) != 0);
}
	],[
	cf_cv_crypt_works=yes],[
	cf_cv_crypt_works=no],[
	cf_cv_crypt_works=unknown])
	LIBS="$cf_save_LIBS"])
	if test "$cf_cv_crypt_works" != no ; then
		AC_DEFINE(HAVE_CRYPT)
		if test "$cf_cv_crypt_func" != yes ; then
			LIBS="$cf_cv_crypt_func $LIBS"
		fi
	fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CURSES_CONFIG version: 2 updated: 2006/10/29 11:06:27
dnl ----------------
dnl Tie together the configure-script macros for curses.  It may be ncurses,
dnl but unless asked, we do not make a special search for ncurses.  However,
dnl still check for the ncurses version number, for use in other macros.
AC_DEFUN([CF_CURSES_CONFIG],
[
CF_CURSES_CPPFLAGS
CF_NCURSES_VERSION
CF_CURSES_LIBS
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CURSES_CPPFLAGS version: 10 updated: 2009/01/06 19:34:11
dnl ------------------
dnl Look for the curses headers.
AC_DEFUN([CF_CURSES_CPPFLAGS],[

AC_CACHE_CHECK(for extra include directories,cf_cv_curses_incdir,[
cf_cv_curses_incdir=no
case $host_os in #(vi
hpux10.*) #(vi
	test -d /usr/include/curses_colr && \
	cf_cv_curses_incdir="-I/usr/include/curses_colr"
	;;
sunos3*|sunos4*)
	test -d /usr/5lib && \
	test -d /usr/5include && \
	cf_cv_curses_incdir="-I/usr/5include"
	;;
esac
])
test "$cf_cv_curses_incdir" != no && CPPFLAGS="$CPPFLAGS $cf_cv_curses_incdir"

CF_CURSES_HEADER
CF_TERM_HEADER
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CURSES_FUNCS version: 14 updated: 2009/07/16 19:34:55
dnl ---------------
dnl Curses-functions are a little complicated, since a lot of them are macros.
AC_DEFUN([CF_CURSES_FUNCS],
[
AC_REQUIRE([CF_CURSES_CPPFLAGS])dnl
AC_REQUIRE([CF_XOPEN_CURSES])
AC_REQUIRE([CF_CURSES_TERM_H])
for cf_func in $1
do
	CF_UPPER(cf_tr_func,$cf_func)
	AC_MSG_CHECKING(for ${cf_func})
	CF_MSG_LOG(${cf_func})
	AC_CACHE_VAL(cf_cv_func_$cf_func,[
		eval cf_result='$ac_cv_func_'$cf_func
		if test ".$cf_result" != ".no"; then
			AC_TRY_LINK(CF__CURSES_HEAD,
			[
#ifndef ${cf_func}
long foo = (long)(&${cf_func});
${cf_cv_main_return-return}(foo == 0);
#endif
			],
			[cf_result=yes],
			[cf_result=no])
		fi
		eval 'cf_cv_func_'$cf_func'=$cf_result'
	])
	# use the computed/retrieved cache-value:
	eval 'cf_result=$cf_cv_func_'$cf_func
	AC_MSG_RESULT($cf_result)
	if test $cf_result != no; then
		AC_DEFINE_UNQUOTED(HAVE_${cf_tr_func})
	fi
done
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CURSES_HEADER version: 2 updated: 2010/04/28 06:02:16
dnl ----------------
dnl Find a "curses" header file, e.g,. "curses.h", or one of the more common
dnl variations of ncurses' installs.
dnl
dnl $1 = ncurses when looking for ncurses, or is empty
AC_DEFUN([CF_CURSES_HEADER],[
AC_CACHE_CHECK(if we have identified curses headers,cf_cv_ncurses_header,[
cf_cv_ncurses_header=none
for cf_header in ifelse($1,,,[ \
    $1/curses.h \
	$1/ncurses.h]) \
	curses.h \
	ncurses.h ifelse($1,,[ncurses/curses.h ncurses/ncurses.h])
do
AC_TRY_COMPILE([#include <${cf_header}>],
	[initscr(); tgoto("?", 0,0)],
	[cf_cv_ncurses_header=$cf_header; break],[])
done
])

if test "$cf_cv_ncurses_header" = none ; then
	AC_MSG_ERROR(No curses header-files found)
fi

# cheat, to get the right #define's for HAVE_NCURSES_H, etc.
AC_CHECK_HEADERS($cf_cv_ncurses_header)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CURSES_LIBS version: 30 updated: 2010/06/20 09:24:28
dnl --------------
dnl Look for the curses libraries.  Older curses implementations may require
dnl termcap/termlib to be linked as well.  Call CF_CURSES_CPPFLAGS first.
AC_DEFUN([CF_CURSES_LIBS],[

AC_REQUIRE([CF_CURSES_CPPFLAGS])dnl
AC_MSG_CHECKING(if we have identified curses libraries)
AC_TRY_LINK([#include <${cf_cv_ncurses_header-curses.h}>],
    [initscr(); tgoto("?", 0,0)],
    cf_result=yes,
    cf_result=no)
AC_MSG_RESULT($cf_result)

if test "$cf_result" = no ; then
case $host_os in #(vi
freebsd*) #(vi
    AC_CHECK_LIB(mytinfo,tgoto,[CF_ADD_LIBS(-lmytinfo)])
    ;;
hpux10.*) #(vi
    AC_CHECK_LIB(cur_colr,initscr,[
        CF_ADD_LIBS(-lcur_colr)
        ac_cv_func_initscr=yes
        ],[
    AC_CHECK_LIB(Hcurses,initscr,[
        # HP's header uses __HP_CURSES, but user claims _HP_CURSES.
        CF_ADD_LIBS(-lHcurses)
        CPPFLAGS="$CPPFLAGS -D__HP_CURSES -D_HP_CURSES"
        ac_cv_func_initscr=yes
        ])])
    ;;
linux*) # Suse Linux does not follow /usr/lib convention
    CF_ADD_LIBDIR(/lib)
    ;;
sunos3*|sunos4*)
    if test -d /usr/5lib ; then
      CF_ADD_LIBDIR(/usr/5lib)
      CF_ADD_LIBS(-lcurses -ltermcap)
    fi
    ac_cv_func_initscr=yes
    ;;
esac

if test ".$ac_cv_func_initscr" != .yes ; then
    cf_save_LIBS="$LIBS"
    cf_term_lib=""
    cf_curs_lib=""

    if test ".${cf_cv_ncurses_version-no}" != .no
    then
        cf_check_list="ncurses curses cursesX"
    else
        cf_check_list="cursesX curses ncurses"
    fi

    # Check for library containing tgoto.  Do this before curses library
    # because it may be needed to link the test-case for initscr.
    AC_CHECK_FUNC(tgoto,[cf_term_lib=predefined],[
        for cf_term_lib in $cf_check_list termcap termlib unknown
        do
            AC_CHECK_LIB($cf_term_lib,tgoto,[break])
        done
    ])

    # Check for library containing initscr
    test "$cf_term_lib" != predefined && test "$cf_term_lib" != unknown && LIBS="-l$cf_term_lib $cf_save_LIBS"
 	for cf_curs_lib in $cf_check_list xcurses jcurses pdcurses unknown
    do
        AC_CHECK_LIB($cf_curs_lib,initscr,[break])
    done
    test $cf_curs_lib = unknown && AC_MSG_ERROR(no curses library found)

    LIBS="-l$cf_curs_lib $cf_save_LIBS"
    if test "$cf_term_lib" = unknown ; then
        AC_MSG_CHECKING(if we can link with $cf_curs_lib library)
        AC_TRY_LINK([#include <${cf_cv_ncurses_header-curses.h}>],
            [initscr()],
            [cf_result=yes],
            [cf_result=no])
        AC_MSG_RESULT($cf_result)
        test $cf_result = no && AC_MSG_ERROR(Cannot link curses library)
    elif test "$cf_curs_lib" = "$cf_term_lib" ; then
        :
    elif test "$cf_term_lib" != predefined ; then
        AC_MSG_CHECKING(if we need both $cf_curs_lib and $cf_term_lib libraries)
        AC_TRY_LINK([#include <${cf_cv_ncurses_header-curses.h}>],
            [initscr(); tgoto((char *)0, 0, 0);],
            [cf_result=no],
            [
            LIBS="-l$cf_curs_lib -l$cf_term_lib $cf_save_LIBS"
            AC_TRY_LINK([#include <${cf_cv_ncurses_header-curses.h}>],
                [initscr()],
                [cf_result=yes],
                [cf_result=error])
            ])
        AC_MSG_RESULT($cf_result)
    fi
fi
fi

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CURSES_TERMCAP version: 10 updated: 2002/01/13 16:02:19
dnl -----------------
dnl Check if we should include <curses.h> to pick up prototypes for termcap
dnl functions.  On terminfo systems, these are normally declared in <curses.h>,
dnl but may be in <term.h>.  We check for termcap.h as an alternate, but it
dnl isn't standard (usually associated with GNU termcap).
dnl
dnl The 'tgoto()' function is declared in both terminfo and termcap.
dnl
dnl See CF_TYPE_OUTCHAR for more details.
AC_DEFUN([CF_CURSES_TERMCAP],
[
AC_REQUIRE([CF_CURSES_TERM_H])
AC_CACHE_CHECK(if we should include curses.h or termcap.h, cf_cv_need_curses_h,[
cf_save_CPPFLAGS="$CPPFLAGS"
cf_cv_need_curses_h=no

for cf_t_opts in "" "NEED_TERMCAP_H"
do
for cf_c_opts in "" "NEED_CURSES_H"
do

    CPPFLAGS="$cf_save_CPPFLAGS $CHECK_DECL_FLAG"
    test -n "$cf_c_opts" && CPPFLAGS="$CPPFLAGS -D$cf_c_opts"
    test -n "$cf_t_opts" && CPPFLAGS="$CPPFLAGS -D$cf_t_opts"

    AC_TRY_LINK([/* $cf_c_opts $cf_t_opts */
$CHECK_DECL_HDRS],
	[char *x = (char *)tgoto("")],
	[test "$cf_cv_need_curses_h" = no && {
	     cf_cv_need_curses_h=maybe
	     cf_ok_c_opts=$cf_c_opts
	     cf_ok_t_opts=$cf_t_opts
	}],
	[echo "Recompiling with corrected call (C:$cf_c_opts, T:$cf_t_opts)" >&AC_FD_CC
	AC_TRY_LINK([
$CHECK_DECL_HDRS],
	[char *x = (char *)tgoto("",0,0)],
	[cf_cv_need_curses_h=yes
	 cf_ok_c_opts=$cf_c_opts
	 cf_ok_t_opts=$cf_t_opts])])

	CPPFLAGS="$cf_save_CPPFLAGS"
	test "$cf_cv_need_curses_h" = yes && break
done
	test "$cf_cv_need_curses_h" = yes && break
done

if test "$cf_cv_need_curses_h" != no ; then
	echo "Curses/termcap test = $cf_cv_need_curses_h (C:$cf_ok_c_opts, T:$cf_ok_t_opts)" >&AC_FD_CC
	if test -n "$cf_ok_c_opts" ; then
		if test -n "$cf_ok_t_opts" ; then
			cf_cv_need_curses_h=both
		else
			cf_cv_need_curses_h=curses.h
		fi
	elif test -n "$cf_ok_t_opts" ; then
		cf_cv_need_curses_h=termcap.h
	elif test "$cf_cv_term_header" != no ; then
		cf_cv_need_curses_h=term.h
	else
		cf_cv_need_curses_h=no
	fi
fi
])

case $cf_cv_need_curses_h in
both) #(vi
	AC_DEFINE_UNQUOTED(NEED_CURSES_H)
	AC_DEFINE_UNQUOTED(NEED_TERMCAP_H)
	;;
curses.h) #(vi
	AC_DEFINE_UNQUOTED(NEED_CURSES_H)
	;;
term.h) #(vi
	AC_DEFINE_UNQUOTED(NEED_TERM_H)
	;;
termcap.h) #(vi
	AC_DEFINE_UNQUOTED(NEED_TERMCAP_H)
	;;
esac

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CURSES_TERM_H version: 7 updated: 2010/01/24 18:40:06
dnl ----------------
dnl SVr4 curses should have term.h as well (where it puts the definitions of
dnl the low-level interface).  This may not be true in old/broken implementations,
dnl as well as in misconfigured systems (e.g., gcc configured for Solaris 2.4
dnl running with Solaris 2.5.1).
AC_DEFUN([CF_CURSES_TERM_H],
[
AC_REQUIRE([CF_CURSES_CPPFLAGS])dnl

AC_CACHE_CHECK(for term.h, cf_cv_term_header,[

# If we found <ncurses/curses.h>, look for <ncurses/term.h>, but always look
# for <term.h> if we do not find the variant.
for cf_header in \
	`echo ${cf_cv_ncurses_header-curses.h} | sed -e 's%/.*%/%'`term.h \
	term.h
do
	AC_TRY_COMPILE([
#include <${cf_cv_ncurses_header-curses.h}>
#include <${cf_header}>],
	[WINDOW *x],
	[cf_cv_term_header=$cf_header
	 break],
	[cf_cv_term_header=no])
done

case $cf_cv_term_header in #(vi
no)
	# If curses is ncurses, some packagers still mess it up by trying to make
	# us use GNU termcap.  This handles the most common case.
	for cf_header in ncurses/term.h ncursesw/term.h
	do
		AC_TRY_COMPILE([
#include <${cf_cv_ncurses_header-curses.h}>
#ifdef NCURSES_VERSION
#include <${cf_header}>
#else
make an error
#endif],
			[WINDOW *x],
			[cf_cv_term_header=$cf_header
			 break],
			[cf_cv_term_header=no])
	done
	;;
esac
])

case $cf_cv_term_header in #(vi
term.h) #(vi
	AC_DEFINE(HAVE_TERM_H)
	;;
ncurses/term.h) #(vi
	AC_DEFINE(HAVE_NCURSES_TERM_H)
	;;
ncursesw/term.h)
	AC_DEFINE(HAVE_NCURSESW_TERM_H)
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_DIRNAME version: 4 updated: 2002/12/21 19:25:52
dnl ----------
dnl "dirname" is not portable, so we fake it with a shell script.
AC_DEFUN([CF_DIRNAME],[$1=`echo $2 | sed -e 's%/[[^/]]*$%%'`])dnl
dnl ---------------------------------------------------------------------------
dnl CF_DISABLE_ECHO version: 11 updated: 2009/12/13 13:16:57
dnl ---------------
dnl You can always use "make -n" to see the actual options, but it's hard to
dnl pick out/analyze warning messages when the compile-line is long.
dnl
dnl Sets:
dnl	ECHO_LT - symbol to control if libtool is verbose
dnl	ECHO_LD - symbol to prefix "cc -o" lines
dnl	RULE_CC - symbol to put before implicit "cc -c" lines (e.g., .c.o)
dnl	SHOW_CC - symbol to put before explicit "cc -c" lines
dnl	ECHO_CC - symbol to put before any "cc" line
dnl
AC_DEFUN([CF_DISABLE_ECHO],[
AC_MSG_CHECKING(if you want to see long compiling messages)
CF_ARG_DISABLE(echo,
	[  --disable-echo          display "compiling" commands],
	[
    ECHO_LT='--silent'
    ECHO_LD='@echo linking [$]@;'
    RULE_CC='@echo compiling [$]<'
    SHOW_CC='@echo compiling [$]@'
    ECHO_CC='@'
],[
    ECHO_LT=''
    ECHO_LD=''
    RULE_CC=''
    SHOW_CC=''
    ECHO_CC=''
])
AC_MSG_RESULT($enableval)
AC_SUBST(ECHO_LT)
AC_SUBST(ECHO_LD)
AC_SUBST(RULE_CC)
AC_SUBST(SHOW_CC)
AC_SUBST(ECHO_CC)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_DISABLE_RPATH_HACK version: 1 updated: 2010/04/11 10:54:00
dnl ---------------------
dnl The rpath-hack makes it simpler to build programs, particularly with the
dnl *BSD ports which may have essential libraries in unusual places.  But it
dnl can interfere with building an executable for the base system.  Use this
dnl option in that case.
AC_DEFUN([CF_DISABLE_RPATH_HACK],
[
AC_MSG_CHECKING(if rpath should be not be set)
CF_ARG_DISABLE(rpath-hack,
	[  --disable-rpath-hack    don't add rpath options for additional libraries],
	[cf_disable_rpath_hack=yes],
	[cf_disable_rpath_hack=no])
AC_MSG_RESULT($cf_disable_rpath_hack)
if test "$cf_disable_rpath_hack" = no ; then
	CF_RPATH_HACK
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_ENABLE_NARROWPROTO version: 3 updated: 2006/02/12 17:46:00
dnl ---------------------
dnl If this is not set properly, Xaw's scrollbars will not work.
dnl The so-called "modular" configuration for X.org omits most of the
dnl configure checks that would be needed to provide compatibility with
dnl older X builds.  This one breaks things noticeably.
AC_DEFUN([CF_ENABLE_NARROWPROTO],
[
AC_MSG_CHECKING(if you want narrow prototypes for X libraries)

case `$ac_config_guess` in #(vi
*cygwin*|*freebsd*|*gnu*|*irix5*|*irix6*|*linux-gnu*|*netbsd*|*openbsd*|*qnx*|*sco*|*sgi*) #(vi
	cf_default_narrowproto=yes
	;;
*)
	cf_default_narrowproto=no
	;;
esac

CF_ARG_OPTION(narrowproto,
	[  --enable-narrowproto    enable narrow prototypes for X libraries],
	[enable_narrowproto=$enableval],
	[enable_narrowproto=$cf_default_narrowproto],
	[$cf_default_narrowproto])
AC_MSG_RESULT($enable_narrowproto)
])
dnl ---------------------------------------------------------------------------
dnl CF_ERRNO version: 5 updated: 1997/11/30 12:44:39
dnl --------
dnl Check if 'errno' is declared in <errno.h>
AC_DEFUN([CF_ERRNO],
[
CF_CHECK_ERRNO(errno)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_FIND_LIBRARY version: 9 updated: 2008/03/23 14:48:54
dnl ---------------
dnl Look for a non-standard library, given parameters for AC_TRY_LINK.  We
dnl prefer a standard location, and use -L options only if we do not find the
dnl library in the standard library location(s).
dnl	$1 = library name
dnl	$2 = library class, usually the same as library name
dnl	$3 = includes
dnl	$4 = code fragment to compile/link
dnl	$5 = corresponding function-name
dnl	$6 = flag, nonnull if failure should not cause an error-exit
dnl
dnl Sets the variable "$cf_libdir" as a side-effect, so we can see if we had
dnl to use a -L option.
AC_DEFUN([CF_FIND_LIBRARY],
[
	eval 'cf_cv_have_lib_'$1'=no'
	cf_libdir=""
	AC_CHECK_FUNC($5,
		eval 'cf_cv_have_lib_'$1'=yes',[
		cf_save_LIBS="$LIBS"
		AC_MSG_CHECKING(for $5 in -l$1)
		LIBS="-l$1 $LIBS"
		AC_TRY_LINK([$3],[$4],
			[AC_MSG_RESULT(yes)
			 eval 'cf_cv_have_lib_'$1'=yes'
			],
			[AC_MSG_RESULT(no)
			CF_LIBRARY_PATH(cf_search,$2)
			for cf_libdir in $cf_search
			do
				AC_MSG_CHECKING(for -l$1 in $cf_libdir)
				LIBS="-L$cf_libdir -l$1 $cf_save_LIBS"
				AC_TRY_LINK([$3],[$4],
					[AC_MSG_RESULT(yes)
			 		 eval 'cf_cv_have_lib_'$1'=yes'
					 break],
					[AC_MSG_RESULT(no)
					 LIBS="$cf_save_LIBS"])
			done
			])
		])
eval 'cf_found_library=[$]cf_cv_have_lib_'$1
ifelse($6,,[
if test $cf_found_library = no ; then
	AC_MSG_ERROR(Cannot link $1 library)
fi
])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_FIND_LINKAGE version: 19 updated: 2010/05/29 16:31:02
dnl ---------------
dnl Find a library (specifically the linkage used in the code fragment),
dnl searching for it if it is not already in the library path.
dnl See also CF_ADD_SEARCHPATH.
dnl
dnl Parameters (4-on are optional):
dnl     $1 = headers for library entrypoint
dnl     $2 = code fragment for library entrypoint
dnl     $3 = the library name without the "-l" option or ".so" suffix.
dnl     $4 = action to perform if successful (default: update CPPFLAGS, etc)
dnl     $5 = action to perform if not successful
dnl     $6 = module name, if not the same as the library name
dnl     $7 = extra libraries
dnl
dnl Sets these variables:
dnl     $cf_cv_find_linkage_$3 - yes/no according to whether linkage is found
dnl     $cf_cv_header_path_$3 - include-directory if needed
dnl     $cf_cv_library_path_$3 - library-directory if needed
dnl     $cf_cv_library_file_$3 - library-file if needed, e.g., -l$3
AC_DEFUN([CF_FIND_LINKAGE],[

# If the linkage is not already in the $CPPFLAGS/$LDFLAGS configuration, these
# will be set on completion of the AC_TRY_LINK below.
cf_cv_header_path_$3=
cf_cv_library_path_$3=

CF_MSG_LOG([Starting [FIND_LINKAGE]($3,$6)])

cf_save_LIBS="$LIBS"

AC_TRY_LINK([$1],[$2],[
	cf_cv_find_linkage_$3=yes
	cf_cv_header_path_$3=/usr/include
	cf_cv_library_path_$3=/usr/lib
],[

LIBS="-l$3 $7 $cf_save_LIBS"

AC_TRY_LINK([$1],[$2],[
	cf_cv_find_linkage_$3=yes
	cf_cv_header_path_$3=/usr/include
	cf_cv_library_path_$3=/usr/lib
	cf_cv_library_file_$3="-l$3"
],[
	cf_cv_find_linkage_$3=no
	LIBS="$cf_save_LIBS"

    CF_VERBOSE(find linkage for $3 library)
    CF_MSG_LOG([Searching for headers in [FIND_LINKAGE]($3,$6)])

    cf_save_CPPFLAGS="$CPPFLAGS"
    cf_test_CPPFLAGS="$CPPFLAGS"

    CF_HEADER_PATH(cf_search,ifelse([$6],,[$3],[$6]))
    for cf_cv_header_path_$3 in $cf_search
    do
      if test -d $cf_cv_header_path_$3 ; then
        CF_VERBOSE(... testing $cf_cv_header_path_$3)
        CPPFLAGS="$cf_save_CPPFLAGS -I$cf_cv_header_path_$3"
        AC_TRY_COMPILE([$1],[$2],[
            CF_VERBOSE(... found $3 headers in $cf_cv_header_path_$3)
            cf_cv_find_linkage_$3=maybe
            cf_test_CPPFLAGS="$CPPFLAGS"
            break],[
            CPPFLAGS="$cf_save_CPPFLAGS"
            ])
      fi
    done

    if test "$cf_cv_find_linkage_$3" = maybe ; then

      CF_MSG_LOG([Searching for $3 library in [FIND_LINKAGE]($3,$6)])

      cf_save_LIBS="$LIBS"
      cf_save_LDFLAGS="$LDFLAGS"

      ifelse([$6],,,[
        CPPFLAGS="$cf_test_CPPFLAGS"
        LIBS="-l$3 $7 $cf_save_LIBS"
        AC_TRY_LINK([$1],[$2],[
            CF_VERBOSE(... found $3 library in system)
            cf_cv_find_linkage_$3=yes])
            CPPFLAGS="$cf_save_CPPFLAGS"
            LIBS="$cf_save_LIBS"
            ])

      if test "$cf_cv_find_linkage_$3" != yes ; then
        CF_LIBRARY_PATH(cf_search,$3)
        for cf_cv_library_path_$3 in $cf_search
        do
          if test -d $cf_cv_library_path_$3 ; then
            CF_VERBOSE(... testing $cf_cv_library_path_$3)
            CPPFLAGS="$cf_test_CPPFLAGS"
            LIBS="-l$3 $7 $cf_save_LIBS"
            LDFLAGS="$cf_save_LDFLAGS -L$cf_cv_library_path_$3"
            AC_TRY_LINK([$1],[$2],[
                CF_VERBOSE(... found $3 library in $cf_cv_library_path_$3)
                cf_cv_find_linkage_$3=yes
                cf_cv_library_file_$3="-l$3"
                break],[
                CPPFLAGS="$cf_save_CPPFLAGS"
                LIBS="$cf_save_LIBS"
                LDFLAGS="$cf_save_LDFLAGS"
                ])
          fi
        done
        CPPFLAGS="$cf_save_CPPFLAGS"
        LDFLAGS="$cf_save_LDFLAGS"
      fi

    else
      cf_cv_find_linkage_$3=no
    fi
    ],$7)
])

LIBS="$cf_save_LIBS"

if test "$cf_cv_find_linkage_$3" = yes ; then
ifelse([$4],,[
	CF_ADD_INCDIR($cf_cv_header_path_$3)
	CF_ADD_LIBDIR($cf_cv_library_path_$3)
	CF_ADD_LIB($3)
],[$4])
else
ifelse([$5],,AC_MSG_WARN(Cannot find $3 library),[$5])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_FP_ISREADY version: 2 updated: 1998/04/18 20:51:42
dnl -------------
dnl Test for the common variations of stdio structures that we can use to
dnl test if a character is available for reading.
AC_DEFUN([CF_FP_ISREADY],
[
AC_CACHE_CHECK(for file-pointer ready definition,
cf_cv_fp_isready,[
cf_cv_fp_isready=none
while true
do
	read definition
	test -z "$definition" && break
	echo "test-compile $definition" 1>&AC_FD_CC

	AC_TRY_COMPILE([
#include <stdio.h>
#define isready_c(p) $definition
],[int x = isready_c(stdin)],
	[echo "$definition" >conftest.env
	 break])

done <<'CF_EOF'
( (p)->_IO_read_ptr < (p)->_IO_read_end)
( (p)->__cnt > 0)
( (p)->__rptr < (p)->__rend)
( (p)->_cnt > 0)
( (p)->_gptr < (p)->_egptr)
( (p)->_r > 0)
( (p)->_rcount > 0)
( (p)->endr < (p)->endb)
CF_EOF

test -f conftest.env && cf_cv_fp_isready=`cat conftest.env`

])

test "$cf_cv_fp_isready" != none && AC_DEFINE_UNQUOTED(isready_c(p),$cf_cv_fp_isready)

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_FUNC_DLSYM version: 2 updated: 2010/05/29 16:31:02
dnl -------------
dnl Test for dlsym() and related functions, as well as libdl.
dnl
dnl Sets
dnl	$cf_have_dlsym
dnl	$cf_have_libdl
AC_DEFUN([CF_FUNC_DLSYM],[
cf_have_dlsym=no
AC_CHECK_FUNC(dlsym,cf_have_dlsym=yes,[

cf_have_libdl=no
AC_CHECK_LIB(dl,dlsym,[
	cf_have_dlsym=yes
	cf_have_libdl=yes])])

if test "$cf_have_dlsym" = yes ; then
	test "$cf_have_libdl" = yes && CF_ADD_LIB(dl)

	AC_MSG_CHECKING(whether able to link to dl*() functions)
	AC_TRY_LINK([#include <dlfcn.h>],[
		void *obj;
		if ((obj = dlopen("filename", 0)) != 0) {
			if (dlsym(obj, "symbolname") == 0) {
			dlclose(obj);
			}
		}],[
		AC_DEFINE(HAVE_LIBDL)],[
		AC_MSG_ERROR(Cannot link test program for libdl)])
	AC_MSG_RESULT(ok)
else
	AC_MSG_ERROR(Cannot find dlsym function)
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_GCC_ATTRIBUTES version: 13 updated: 2009/08/11 20:19:56
dnl -----------------
dnl Test for availability of useful gcc __attribute__ directives to quiet
dnl compiler warnings.  Though useful, not all are supported -- and contrary
dnl to documentation, unrecognized directives cause older compilers to barf.
AC_DEFUN([CF_GCC_ATTRIBUTES],
[
if test "$GCC" = yes
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
if test "$GCC" = yes
then
	AC_CHECKING([for $CC __attribute__ directives])
cat > conftest.$ac_ext <<EOF
#line __oline__ "${as_me-configure}"
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
int main(int argc GCC_UNUSED, char *argv[[]] GCC_UNUSED) { return 0; }
EOF
	cf_printf_attribute=no
	cf_scanf_attribute=no
	for cf_attribute in scanf printf unused noreturn
	do
		CF_UPPER(cf_ATTRIBUTE,$cf_attribute)
		cf_directive="__attribute__(($cf_attribute))"
		echo "checking for $CC $cf_directive" 1>&AC_FD_CC

		case $cf_attribute in #(vi
		printf) #(vi
			cf_printf_attribute=yes
			cat >conftest.h <<EOF
#define GCC_$cf_ATTRIBUTE 1
EOF
			;;
		scanf) #(vi
			cf_scanf_attribute=yes
			cat >conftest.h <<EOF
#define GCC_$cf_ATTRIBUTE 1
EOF
			;;
		*) #(vi
			cat >conftest.h <<EOF
#define GCC_$cf_ATTRIBUTE $cf_directive
EOF
			;;
		esac

		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... $cf_attribute)
			cat conftest.h >>confdefs.h
			case $cf_attribute in #(vi
			printf) #(vi
				if test "$cf_printf_attribute" = no ; then
					cat >>confdefs.h <<EOF
#define GCC_PRINTFLIKE(fmt,var) /* nothing */
EOF
				else
					cat >>confdefs.h <<EOF
#define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))
EOF
				fi
				;;
			scanf) #(vi
				if test "$cf_scanf_attribute" = no ; then
					cat >>confdefs.h <<EOF
#define GCC_SCANFLIKE(fmt,var) /* nothing */
EOF
				else
					cat >>confdefs.h <<EOF
#define GCC_SCANFLIKE(fmt,var)  __attribute__((format(scanf,fmt,var)))
EOF
				fi
				;;
			esac
		fi
	done
else
	fgrep define conftest.i >>confdefs.h
fi
rm -rf conftest*
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GCC_OPT_RDYNAMIC version: 1 updated: 2010/03/20 12:37:42
dnl -------------------
dnl "Newer" versions of gcc support the -rdynamic option:
dnl		Pass the flag -export-dynamic to the ELF linker, on targets that
dnl		support it. This instructs the linker to add all symbols, not only
dnl		used ones, to the dynamic symbol table. This option is needed for
dnl		some uses of "dlopen" or to allow obtaining backtraces from within
dnl		a program.
dnl Check for the option, and add it to $CFLAGS if available.
AC_DEFUN([CF_GCC_OPT_RDYNAMIC],[
AC_CACHE_CHECK([if $CC has -rdynamic option],cf_cv_gcc_opt_rdynamic,[

cf_save_CFLAGS="$CFLAGS"
CFLAGS="-Wall -rdynamic $CFLAGS"

# gcc usually does not return an error for unrecognized options.
AC_TRY_LINK([#include <stdio.h>],
	[printf("Hello");],
	[
	# refine check...
	case testing`eval $ac_link 2>&1` in #(vi
	*unrecog*) #(vi
		cf_cv_gcc_opt_rdynamic=no
		;;
	*)
		cf_cv_gcc_opt_rdynamic=yes
		;;
	esac
	],
	[cf_cv_gcc_opt_rdynamic=no])

CFLAGS="$cf_save_CFLAGS"
])
if test "$cf_cv_gcc_opt_rdynamic" = yes ; then
	CF_ADD_CFLAGS([-rdynamic])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GCC_VERSION version: 5 updated: 2010/04/24 11:02:31
dnl --------------
dnl Find version of gcc
AC_DEFUN([CF_GCC_VERSION],[
AC_REQUIRE([AC_PROG_CC])
GCC_VERSION=none
if test "$GCC" = yes ; then
	AC_MSG_CHECKING(version of $CC)
	GCC_VERSION="`${CC} --version 2>/dev/null | sed -e '2,$d' -e 's/^.*(GCC) //' -e 's/^[[^0-9.]]*//' -e 's/[[^0-9.]].*//'`"
	test -z "$GCC_VERSION" && GCC_VERSION=unknown
	AC_MSG_RESULT($GCC_VERSION)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GCC_WARNINGS version: 25 updated: 2010/04/24 11:03:31
dnl ---------------
dnl Check if the compiler supports useful warning options.  There's a few that
dnl we don't use, simply because they're too noisy:
dnl
dnl	-Wconversion (useful in older versions of gcc, but not in gcc 2.7.x)
dnl	-Wredundant-decls (system headers make this too noisy)
dnl	-Wtraditional (combines too many unrelated messages, only a few useful)
dnl	-Wwrite-strings (too noisy, but should review occasionally).  This
dnl		is enabled for ncurses using "--enable-const".
dnl	-pedantic
dnl
dnl Parameter:
dnl	$1 is an optional list of gcc warning flags that a particular
dnl		application might want to use, e.g., "no-unused" for
dnl		-Wno-unused
dnl Special:
dnl	If $with_ext_const is "yes", add a check for -Wwrite-strings
dnl
AC_DEFUN([CF_GCC_WARNINGS],
[
AC_REQUIRE([CF_GCC_VERSION])
CF_INTEL_COMPILER(GCC,INTEL_COMPILER,CFLAGS)

cat > conftest.$ac_ext <<EOF
#line __oline__ "${as_me-configure}"
int main(int argc, char *argv[[]]) { return (argv[[argc-1]] == 0) ; }
EOF

if test "$INTEL_COMPILER" = yes
then
# The "-wdXXX" options suppress warnings:
# remark #1419: external declaration in primary source file
# remark #1683: explicit conversion of a 64-bit integral type to a smaller integral type (potential portability problem)
# remark #1684: conversion from pointer to same-sized integral type (potential portability problem)
# remark #193: zero used for undefined preprocessing identifier
# remark #593: variable "curs_sb_left_arrow" was set but never used
# remark #810: conversion from "int" to "Dimension={unsigned short}" may lose significant bits
# remark #869: parameter "tw" was never referenced
# remark #981: operands are evaluated in unspecified order
# warning #279: controlling expression is constant

	AC_CHECKING([for $CC warning options])
	cf_save_CFLAGS="$CFLAGS"
	EXTRA_CFLAGS="-Wall"
	for cf_opt in \
		wd1419 \
		wd1683 \
		wd1684 \
		wd193 \
		wd593 \
		wd279 \
		wd810 \
		wd869 \
		wd981
	do
		CFLAGS="$cf_save_CFLAGS $EXTRA_CFLAGS -$cf_opt"
		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... -$cf_opt)
			EXTRA_CFLAGS="$EXTRA_CFLAGS -$cf_opt"
		fi
	done
	CFLAGS="$cf_save_CFLAGS"

elif test "$GCC" = yes
then
	AC_CHECKING([for $CC warning options])
	cf_save_CFLAGS="$CFLAGS"
	EXTRA_CFLAGS=
	cf_warn_CONST=""
	test "$with_ext_const" = yes && cf_warn_CONST="Wwrite-strings"
	for cf_opt in W Wall \
		Wbad-function-cast \
		Wcast-align \
		Wcast-qual \
		Winline \
		Wmissing-declarations \
		Wmissing-prototypes \
		Wnested-externs \
		Wpointer-arith \
		Wshadow \
		Wstrict-prototypes \
		Wundef $cf_warn_CONST $1
	do
		CFLAGS="$cf_save_CFLAGS $EXTRA_CFLAGS -$cf_opt"
		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... -$cf_opt)
			case $cf_opt in #(vi
			Wcast-qual) #(vi
				CPPFLAGS="$CPPFLAGS -DXTSTRINGDEFINES"
				;;
			Winline) #(vi
				case $GCC_VERSION in
				[[34]].*)
					CF_VERBOSE(feature is broken in gcc $GCC_VERSION)
					continue;;
				esac
				;;
			esac
			EXTRA_CFLAGS="$EXTRA_CFLAGS -$cf_opt"
		fi
	done
	CFLAGS="$cf_save_CFLAGS"
fi
rm -f conftest*

AC_SUBST(EXTRA_CFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GNU_SOURCE version: 6 updated: 2005/07/09 13:23:07
dnl -------------
dnl Check if we must define _GNU_SOURCE to get a reasonable value for
dnl _XOPEN_SOURCE, upon which many POSIX definitions depend.  This is a defect
dnl (or misfeature) of glibc2, which breaks portability of many applications,
dnl since it is interwoven with GNU extensions.
dnl
dnl Well, yes we could work around it...
AC_DEFUN([CF_GNU_SOURCE],
[
AC_CACHE_CHECK(if we must define _GNU_SOURCE,cf_cv_gnu_source,[
AC_TRY_COMPILE([#include <sys/types.h>],[
#ifndef _XOPEN_SOURCE
make an error
#endif],
	[cf_cv_gnu_source=no],
	[cf_save="$CPPFLAGS"
	 CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"
	 AC_TRY_COMPILE([#include <sys/types.h>],[
#ifdef _XOPEN_SOURCE
make an error
#endif],
	[cf_cv_gnu_source=no],
	[cf_cv_gnu_source=yes])
	CPPFLAGS="$cf_save"
	])
])
test "$cf_cv_gnu_source" = yes && CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_HEADER_PATH version: 12 updated: 2010/05/05 05:22:40
dnl --------------
dnl Construct a search-list of directories for a nonstandard header-file
dnl
dnl Parameters
dnl	$1 = the variable to return as result
dnl	$2 = the package name
AC_DEFUN([CF_HEADER_PATH],
[
$1=

# collect the current set of include-directories from compiler flags
cf_header_path_list=""
if test -n "${CFLAGS}${CPPFLAGS}" ; then
	for cf_header_path in $CPPFLAGS $CFLAGS
	do
		case $cf_header_path in #(vi
		-I*)
			cf_header_path=`echo ".$cf_header_path" |sed -e 's/^...//' -e 's,/include$,,'`
			CF_ADD_SUBDIR_PATH($1,$2,include,$cf_header_path,NONE)
			cf_header_path_list="$cf_header_path_list [$]$1"
			;;
		esac
	done
fi

# add the variations for the package we are looking for
CF_SUBDIR_PATH($1,$2,include)

test "$includedir" != NONE && \
test "$includedir" != "/usr/include" && \
test -d "$includedir" && {
	test -d $includedir &&    $1="[$]$1 $includedir"
	test -d $includedir/$2 && $1="[$]$1 $includedir/$2"
}

test "$oldincludedir" != NONE && \
test "$oldincludedir" != "/usr/include" && \
test -d "$oldincludedir" && {
	test -d $oldincludedir    && $1="[$]$1 $oldincludedir"
	test -d $oldincludedir/$2 && $1="[$]$1 $oldincludedir/$2"
}

$1="[$]$1 $cf_header_path_list"
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_HEADER_RESOURCE version: 4 updated: 2002/10/09 20:00:37
dnl ------------------
dnl On SunOS, struct rusage is referred to in <sys/wait.h>.  struct rusage is
dnl defined in <sys/resource.h>.  On SCO v4, resource.h needs time.h, which we
dnl may have excluded.
AC_DEFUN([CF_HEADER_RESOURCE],[
AC_REQUIRE([CF_HEADER_SELECT])
AC_CACHE_CHECK(if we may include sys/resource.h with sys/wait.h,
cf_cv_resource_with_wait,[
AC_TRY_COMPILE([
#if defined(HAVE_SYS_TIME_H) && (defined(SELECT_WITH_TIME) || !(defined(HAVE_SELECT_H || defined(HAVE_SYS_SELECT_H))))
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
# include <time.h>
#endif
#else
#include <time.h>
#endif
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
],[],[cf_cv_resource_with_wait=yes],[cf_cv_resource_with_wait=no])
])
test $cf_cv_resource_with_wait = yes && AC_DEFINE(RESOURCE_WITH_WAIT)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_HEADER_SELECT version: 4 updated: 2002/10/09 20:00:37
dnl ----------------
dnl like AC_HEADER_TIME, check for conflicts:
dnl on SCO v4, sys/time.h conflicts with select.h
AC_DEFUN([CF_HEADER_SELECT],[
AC_REQUIRE([AC_HEADER_TIME])
AC_CACHE_CHECK(if we can include select.h with time.h,
cf_cv_select_with_time,[
AC_TRY_COMPILE([
#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
# include <time.h>
#endif
#else
#include <time.h>
#endif
#ifdef HAVE_SELECT
# ifdef HAVE_SELECT_H
# include <select.h>
# endif
# ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
# endif
#endif
],[],[cf_cv_select_with_time=yes],[cf_cv_select_with_time=no])
])
test $cf_cv_select_with_time = yes && AC_DEFINE(SELECT_WITH_TIME)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_IMAKE_CFLAGS version: 31 updated: 2010/05/26 05:38:42
dnl ---------------
dnl Use imake to obtain compiler flags.  We could, in principle, write tests to
dnl get these, but if imake is properly configured there is no point in doing
dnl this.
dnl
dnl Parameters (used in constructing a sample Imakefile):
dnl	$1 = optional value to append to $IMAKE_CFLAGS
dnl	$2 = optional value to append to $IMAKE_LOADFLAGS
AC_DEFUN([CF_IMAKE_CFLAGS],
[
AC_PATH_PROGS(IMAKE,xmkmf imake)

if test -n "$IMAKE" ; then

case $IMAKE in # (vi
*/imake)
	cf_imake_opts="-DUseInstalled=YES" # (vi
	;;
*/util/xmkmf)
	# A single parameter tells xmkmf where the config-files are:
	cf_imake_opts="`echo $IMAKE|sed -e s,/config/util/xmkmf,,`" # (vi
	;;
*)
	cf_imake_opts=
	;;
esac

# If it's installed properly, imake (or its wrapper, xmkmf) will point to the
# config directory.
if mkdir conftestdir; then
	CDPATH=; export CDPATH
	cf_makefile=`cd $srcdir;pwd`/Imakefile
	cd conftestdir

	cat >fix_cflags.sed <<'CF_EOF'
s/\\//g
s/[[ 	]][[ 	]]*/ /g
s/"//g
:pack
s/\(=[[^ ]][[^ ]]*\) \([[^-]]\)/\1	\2/g
t pack
s/\(-D[[a-zA-Z0-9_]][[a-zA-Z0-9_]]*\)=\([[^\'0-9 ]][[^ ]]*\)/\1='\\"\2\\"'/g
s/^IMAKE[[ ]]/IMAKE_CFLAGS="/
s/	/ /g
s/$/"/
CF_EOF

	cat >fix_lflags.sed <<'CF_EOF'
s/^IMAKE[[ 	]]*/IMAKE_LOADFLAGS="/
s/$/"/
CF_EOF

	echo >./Imakefile
	test -f $cf_makefile && cat $cf_makefile >>./Imakefile

	cat >> ./Imakefile <<'CF_EOF'
findstddefs:
	@echo IMAKE ${ALLDEFINES}ifelse([$1],,,[ $1])       | sed -f fix_cflags.sed
	@echo IMAKE ${EXTRA_LOAD_FLAGS}ifelse([$2],,,[ $2]) | sed -f fix_lflags.sed
CF_EOF

	if ( $IMAKE $cf_imake_opts 1>/dev/null 2>&AC_FD_CC && test -f Makefile)
	then
		CF_VERBOSE(Using $IMAKE $cf_imake_opts)
	else
		# sometimes imake doesn't have the config path compiled in.  Find it.
		cf_config=
		for cf_libpath in $X_LIBS $LIBS ; do
			case $cf_libpath in # (vi
			-L*)
				cf_libpath=`echo .$cf_libpath | sed -e 's/^...//'`
				cf_libpath=$cf_libpath/X11/config
				if test -d $cf_libpath ; then
					cf_config=$cf_libpath
					break
				fi
				;;
			esac
		done
		if test -z "$cf_config" ; then
			AC_MSG_WARN(Could not find imake config-directory)
		else
			cf_imake_opts="$cf_imake_opts -I$cf_config"
			if ( $IMAKE -v $cf_imake_opts 2>&AC_FD_CC)
			then
				CF_VERBOSE(Using $IMAKE $cf_config)
			else
				AC_MSG_WARN(Cannot run $IMAKE)
			fi
		fi
	fi

	# GNU make sometimes prints "make[1]: Entering...", which
	# would confuse us.
	eval `make findstddefs 2>/dev/null | grep -v make`

	cd ..
	rm -rf conftestdir

	# We use ${ALLDEFINES} rather than ${STD_DEFINES} because the former
	# declares XTFUNCPROTO there.  However, some vendors (e.g., SGI) have
	# modified it to support site.cf, adding a kludge for the /usr/include
	# directory.  Try to filter that out, otherwise gcc won't find its
	# headers.
	if test -n "$GCC" ; then
	    if test -n "$IMAKE_CFLAGS" ; then
		cf_nostdinc=""
		cf_std_incl=""
		cf_cpp_opts=""
		for cf_opt in $IMAKE_CFLAGS
		do
		    case "$cf_opt" in
		    -nostdinc) #(vi
			cf_nostdinc="$cf_opt"
			;;
		    -I/usr/include) #(vi
			cf_std_incl="$cf_opt"
			;;
		    *) #(vi
			cf_cpp_opts="$cf_cpp_opts $cf_opt"
			;;
		    esac
		done
		if test -z "$cf_nostdinc" ; then
		    IMAKE_CFLAGS="$cf_cpp_opts $cf_std_incl"
		elif test -z "$cf_std_incl" ; then
		    IMAKE_CFLAGS="$cf_cpp_opts $cf_nostdinc"
		else
		    CF_VERBOSE(suppressed \"$cf_nostdinc\" and \"$cf_std_incl\")
		    IMAKE_CFLAGS="$cf_cpp_opts"
		fi
	    fi
	fi
fi

# Some imake configurations define PROJECTROOT with an empty value.  Remove
# the empty definition.
case $IMAKE_CFLAGS in
*-DPROJECTROOT=/*)
	;;
*)
	IMAKE_CFLAGS=`echo "$IMAKE_CFLAGS" |sed -e "s,-DPROJECTROOT=[[ 	]], ,"`
	;;
esac

fi

CF_VERBOSE(IMAKE_CFLAGS $IMAKE_CFLAGS)
CF_VERBOSE(IMAKE_LOADFLAGS $IMAKE_LOADFLAGS)

AC_SUBST(IMAKE_CFLAGS)
AC_SUBST(IMAKE_LOADFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_INTEL_COMPILER version: 4 updated: 2010/05/26 05:38:42
dnl -----------------
dnl Check if the given compiler is really the Intel compiler for Linux.  It
dnl tries to imitate gcc, but does not return an error when it finds a mismatch
dnl between prototypes, e.g., as exercised by CF_MISSING_CHECK.
dnl
dnl This macro should be run "soon" after AC_PROG_CC or AC_PROG_CPLUSPLUS, to
dnl ensure that it is not mistaken for gcc/g++.  It is normally invoked from
dnl the wrappers for gcc and g++ warnings.
dnl
dnl $1 = GCC (default) or GXX
dnl $2 = INTEL_COMPILER (default) or INTEL_CPLUSPLUS
dnl $3 = CFLAGS (default) or CXXFLAGS
AC_DEFUN([CF_INTEL_COMPILER],[
ifelse([$2],,INTEL_COMPILER,[$2])=no

if test "$ifelse([$1],,[$1],GCC)" = yes ; then
	case $host_os in
	linux*|gnu*)
		AC_MSG_CHECKING(if this is really Intel ifelse([$1],GXX,C++,C) compiler)
		cf_save_CFLAGS="$ifelse([$3],,CFLAGS,[$3])"
		ifelse([$3],,CFLAGS,[$3])="$ifelse([$3],,CFLAGS,[$3]) -no-gcc"
		AC_TRY_COMPILE([],[
#ifdef __INTEL_COMPILER
#else
make an error
#endif
],[ifelse([$2],,INTEL_COMPILER,[$2])=yes
cf_save_CFLAGS="$cf_save_CFLAGS -we147 -no-gcc"
],[])
		ifelse([$3],,CFLAGS,[$3])="$cf_save_CFLAGS"
		AC_MSG_RESULT($ifelse([$2],,INTEL_COMPILER,[$2]))
		;;
	esac
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_KILLPG version: 4 updated: 2007/05/05 10:45:50
dnl ---------
dnl Note: must follow AC_FUNC_SETPGRP, but cannot use AC_REQUIRE, since that
dnl messes up the messages...
AC_DEFUN([CF_KILLPG],
[
AC_CACHE_CHECK(if killpg is needed, cf_cv_need_killpg,[
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
    ${cf_cv_main_return-return}(1);
}],
	[cf_cv_need_killpg=no],
	[cf_cv_need_killpg=yes],
	[cf_cv_need_killpg=unknown])
])

test $cf_cv_need_killpg = yes && AC_DEFINE(HAVE_KILLPG)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LARGEFILE version: 7 updated: 2007/06/02 11:58:50
dnl ------------
dnl Add checks for large file support.
AC_DEFUN([CF_LARGEFILE],[
ifdef([AC_FUNC_FSEEKO],[
    AC_SYS_LARGEFILE
    if test "$enable_largefile" != no ; then
	AC_FUNC_FSEEKO

	# Normally we would collect these definitions in the config.h,
	# but (like _XOPEN_SOURCE), some environments rely on having these
	# defined before any of the system headers are included.  Another
	# case comes up with C++, e.g., on AIX the compiler compiles the
	# header files by themselves before looking at the body files it is
	# told to compile.  For ncurses, those header files do not include
	# the config.h
	test "$ac_cv_sys_large_files"      != no && CPPFLAGS="$CPPFLAGS -D_LARGE_FILES "
	test "$ac_cv_sys_largefile_source" != no && CPPFLAGS="$CPPFLAGS -D_LARGEFILE_SOURCE "
	test "$ac_cv_sys_file_offset_bits" != no && CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=$ac_cv_sys_file_offset_bits "

	AC_CACHE_CHECK(whether to use struct dirent64, cf_cv_struct_dirent64,[
		AC_TRY_COMPILE([
#include <sys/types.h>
#include <dirent.h>
		],[
		/* if transitional largefile support is setup, this is true */
		extern struct dirent64 * readdir(DIR *);
		struct dirent64 *x = readdir((DIR *)0);
		struct dirent *y = readdir((DIR *)0);
		int z = x - y;
		],
		[cf_cv_struct_dirent64=yes],
		[cf_cv_struct_dirent64=no])
	])
	test "$cf_cv_struct_dirent64" = yes && AC_DEFINE(HAVE_STRUCT_DIRENT64)
    fi
])
])
dnl ---------------------------------------------------------------------------
dnl CF_LD_RPATH_OPT version: 3 updated: 2010/06/02 05:03:05
dnl ---------------
dnl For the given system and compiler, find the compiler flags to pass to the
dnl loader to use the "rpath" feature.
AC_DEFUN([CF_LD_RPATH_OPT],
[
AC_REQUIRE([CF_CHECK_CACHE])

LD_RPATH_OPT=
AC_MSG_CHECKING(for an rpath option)
case $cf_cv_system_name in #(vi
irix*) #(vi
	if test "$GCC" = yes; then
		LD_RPATH_OPT="-Wl,-rpath,"
	else
		LD_RPATH_OPT="-rpath "
	fi
	;;
linux*|gnu*|k*bsd*-gnu) #(vi
	LD_RPATH_OPT="-Wl,-rpath,"
	;;
openbsd[[2-9]].*) #(vi
	LD_RPATH_OPT="-Wl,-rpath,"
	;;
freebsd*) #(vi
	LD_RPATH_OPT="-rpath "
	;;
netbsd*) #(vi
	LD_RPATH_OPT="-Wl,-rpath,"
	;;
osf*|mls+*) #(vi
	LD_RPATH_OPT="-rpath "
	;;
solaris2*) #(vi
	LD_RPATH_OPT="-R"
	;;
*)
	;;
esac
AC_MSG_RESULT($LD_RPATH_OPT)

case "x$LD_RPATH_OPT" in #(vi
x-R*)
	AC_MSG_CHECKING(if we need a space after rpath option)
	cf_save_LIBS="$LIBS"
	CF_ADD_LIBS(${LD_RPATH_OPT}$libdir)
	AC_TRY_LINK(, , cf_rpath_space=no, cf_rpath_space=yes)
	LIBS="$cf_save_LIBS"
	AC_MSG_RESULT($cf_rpath_space)
	test "$cf_rpath_space" = yes && LD_RPATH_OPT="$LD_RPATH_OPT "
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LEX_CHAR_CLASSES version: 6 updated: 2009/04/04 13:08:05
dnl -------------------
dnl Check if the lex/flex program accepts character-classes, i.e., [:alpha:],
dnl which are a POSIX feature.
AC_DEFUN([CF_LEX_CHAR_CLASSES],[
AC_MSG_CHECKING(if $LEX supports character classes)
cat >conftest.l <<CF_EOF
IDENT	[[[:alpha:]][[:alnum:]]]+
DATE	"#"[[:blank:]][[:alnum:]\,:./]+"#"
%%
{IDENT}	{ ECHO; }
{DATE}	{ ECHO; }
CF_EOF
cf_lex_char_classes="$LEX conftest.l 1>&AC_FD_CC"
if AC_TRY_EVAL(cf_lex_char_classes); then
	LEX_CHAR_CLASSES=yes
else
	LEX_CHAR_CLASSES=no
fi
AC_MSG_RESULT($LEX_CHAR_CLASSES)
rm -f conftest.* $LEX_OUTPUT_ROOT.c
if test "$LEX_CHAR_CLASSES" != yes ; then
	AC_MSG_WARN(Your $LEX program does not support POSIX character classes.  Get flex.)
fi
AC_SUBST(LEX_CHAR_CLASSES)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LEX_OPTIONS version: 1 updated: 2009/04/04 13:08:05
dnl --------------
dnl Check if the lex/flex program accepts options, i.e., %o.  This is for
dnl standard (POSIX) lex; there are some implementations that are nonstandard,
dnl or simply too buggy to consider.
AC_DEFUN([CF_LEX_OPTIONS],[
AC_MSG_CHECKING(if $LEX supports options)
cat >conftest.l <<CF_EOF
%o 1000
%%
%%
nothing	ECHO;
CF_EOF
cf_lex_options="$LEX conftest.l 1>&AC_FD_CC"
if AC_TRY_EVAL(cf_lex_options); then
cf_lex_options=yes
else
cf_lex_options=no
fi
AC_MSG_RESULT($cf_lex_options)
rm -f conftest.* $LEX_OUTPUT_ROOT.c
if test "$cf_lex_options" != yes ; then
	AC_MSG_WARN(Your $LEX program does not support POSIX options.  Get flex.)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LEX_POINTER version: 1 updated: 2009/04/04 13:08:05
dnl --------------
dnl Check if the lex/flex program accepts %pointer, for standard (POSIX) lex.
AC_DEFUN([CF_LEX_POINTER],[
AC_MSG_CHECKING(if $LEX supports %pointer)
cat >conftest.l <<CF_EOF
%pointer
%%
%%
nothing	ECHO;
CF_EOF
cf_lex_pointer="$LEX conftest.l 1>&AC_FD_CC"
if AC_TRY_EVAL(cf_lex_pointer); then
cf_lex_pointer=yes
else
cf_lex_pointer=no
fi
AC_MSG_RESULT($cf_lex_pointer)
rm -f conftest.* $LEX_OUTPUT_ROOT.c
if test "$cf_lex_pointer" != yes ; then
	AC_MSG_WARN(Your $LEX program does not support POSIX %pointer.  Get flex.)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LEX_STATES version: 5 updated: 2009/08/24 06:40:08
dnl -------------
dnl Check if the lex/flex program accepts states, i.e., %s and %x.  Older
dnl implementations do not support these.
dnl
dnl $1 = optional parameter for number of states needed
AC_DEFUN([CF_LEX_STATES],[
AC_MSG_CHECKING(if $LEX supports states)

cf_lex_statemax=ifelse($1,,32,$1)
cf_lex_statenum=0
cf_lex_states_s=
cf_lex_states_x=

cf_lex_states=no
while test 0 != `expr $cf_lex_statenum \< $cf_lex_statemax`
do
cf_lex_statenum=`expr $cf_lex_statenum + 1`
cf_lex_states_s="$cf_lex_states_s S$cf_lex_statenum"
cf_lex_states_x="$cf_lex_states_x X$cf_lex_statenum"
cat >conftest.l <<CF_EOF
%s $cf_lex_states_s
%x $cf_lex_states_x
%%
%%
nothing	ECHO;
CF_EOF
	cf_lex_states="$LEX conftest.l 1>&AC_FD_CC"
	if AC_TRY_EVAL(cf_lex_states); then
		if test $cf_lex_statenum = $cf_lex_statemax; then
			cf_lex_states=yes
			break
		else
			cf_lex_states=$cf_lex_statenum
		fi
	else
		break
	fi
done
AC_MSG_RESULT($cf_lex_states)
rm -f conftest.* $LEX_OUTPUT_ROOT.c
if test "$cf_lex_states" = no ; then
	AC_MSG_WARN(Your $LEX program does not support POSIX states.  Get flex.)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LEX_VERSION version: 4 updated: 2009/12/08 05:09:06
dnl --------------
dnl Check if "lex" is really "flex", and if so, set $LEX_VERSION to show its
dnl version.
AC_DEFUN([CF_LEX_VERSION],[
AC_REQUIRE([AC_PROG_LEX])

test -z "$LEX" && LEX=lex
test "x$LEX" = "x:" && LEX=lex  # : is not a usable default
AC_MSG_CHECKING(if $LEX is really flex)
if ( $LEX '-?' </dev/null 2>&1 |fgrep flex >/dev/null )
then
	AC_MSG_RESULT(yes)
	AC_MSG_CHECKING(version of $LEX)
	LEX_VERSION=`$LEX --version </dev/null 2>&1 | sed -e 's/^.* \([[0-9]]\)/\\1/;s/ .*$//;s/^[[^0-9]]*//'`
	AC_MSG_RESULT($LEX_VERSION)
else
	AC_MSG_RESULT(no)
fi

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LIBRARY_PATH version: 9 updated: 2010/03/28 12:52:50
dnl ---------------
dnl Construct a search-list of directories for a nonstandard library-file
dnl
dnl Parameters
dnl	$1 = the variable to return as result
dnl	$2 = the package name
AC_DEFUN([CF_LIBRARY_PATH],
[
$1=
cf_library_path_list=""
if test -n "${LDFLAGS}${LIBS}" ; then
	for cf_library_path in $LDFLAGS $LIBS
	do
		case $cf_library_path in #(vi
		-L*)
			cf_library_path=`echo ".$cf_library_path" |sed -e 's/^...//' -e 's,/lib$,,'`
			CF_ADD_SUBDIR_PATH($1,$2,lib,$cf_library_path,NONE)
			cf_library_path_list="$cf_library_path_list [$]$1"
			;;
		esac
	done
fi

CF_SUBDIR_PATH($1,$2,lib)

$1="$cf_library_path_list [$]$1"
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LIB_PREFIX version: 8 updated: 2008/09/13 11:34:16
dnl -------------
dnl Compute the library-prefix for the given host system
dnl $1 = variable to set
AC_DEFUN([CF_LIB_PREFIX],
[
	case $cf_cv_system_name in #(vi
	OS/2*|os2*) #(vi
        LIB_PREFIX=''
        ;;
	*)	LIB_PREFIX='lib'
        ;;
	esac
ifelse($1,,,[$1=$LIB_PREFIX])
	AC_SUBST(LIB_PREFIX)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LOCALE version: 4 updated: 2003/02/16 08:16:04
dnl ---------
dnl Check if we have setlocale() and its header, <locale.h>
dnl The optional parameter $1 tells what to do if we do have locale support.
AC_DEFUN([CF_LOCALE],
[
AC_MSG_CHECKING(for setlocale())
AC_CACHE_VAL(cf_cv_locale,[
AC_TRY_LINK([#include <locale.h>],
	[setlocale(LC_ALL, "")],
	[cf_cv_locale=yes],
	[cf_cv_locale=no])
	])
AC_MSG_RESULT($cf_cv_locale)
test $cf_cv_locale = yes && { ifelse($1,,AC_DEFINE(LOCALE),[$1]) }
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MAKEFLAGS version: 12 updated: 2006/10/21 08:27:03
dnl ------------
dnl Some 'make' programs support ${MAKEFLAGS}, some ${MFLAGS}, to pass 'make'
dnl options to lower-levels.  It's very useful for "make -n" -- if we have it.
dnl (GNU 'make' does both, something POSIX 'make', which happens to make the
dnl ${MAKEFLAGS} variable incompatible because it adds the assignments :-)
AC_DEFUN([CF_MAKEFLAGS],
[
AC_CACHE_CHECK(for makeflags variable, cf_cv_makeflags,[
	cf_cv_makeflags=''
	for cf_option in '-${MAKEFLAGS}' '${MFLAGS}'
	do
		cat >cf_makeflags.tmp <<CF_EOF
SHELL = /bin/sh
all :
	@ echo '.$cf_option'
CF_EOF
		cf_result=`${MAKE-make} -k -f cf_makeflags.tmp 2>/dev/null | sed -e 's,[[ 	]]*$,,'`
		case "$cf_result" in
		.*k)
			cf_result=`${MAKE-make} -k -f cf_makeflags.tmp CC=cc 2>/dev/null`
			case "$cf_result" in
			.*CC=*)	cf_cv_makeflags=
				;;
			*)	cf_cv_makeflags=$cf_option
				;;
			esac
			break
			;;
		.-)	;;
		*)	echo "given option \"$cf_option\", no match \"$cf_result\""
			;;
		esac
	done
	rm -f cf_makeflags.tmp
])

AC_SUBST(cf_cv_makeflags)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MAKE_TAGS version: 5 updated: 2010/04/03 20:07:32
dnl ------------
dnl Generate tags/TAGS targets for makefiles.  Do not generate TAGS if we have
dnl a monocase filesystem.
AC_DEFUN([CF_MAKE_TAGS],[
AC_REQUIRE([CF_MIXEDCASE_FILENAMES])

AC_CHECK_PROGS(CTAGS, exctags ctags)
AC_CHECK_PROGS(ETAGS, exetags etags)

AC_CHECK_PROG(MAKE_LOWER_TAGS, ${CTAGS-ctags}, yes, no)

if test "$cf_cv_mixedcase" = yes ; then
	AC_CHECK_PROG(MAKE_UPPER_TAGS, ${ETAGS-etags}, yes, no)
else
	MAKE_UPPER_TAGS=no
fi

if test "$MAKE_UPPER_TAGS" = yes ; then
	MAKE_UPPER_TAGS=
else
	MAKE_UPPER_TAGS="#"
fi

if test "$MAKE_LOWER_TAGS" = yes ; then
	MAKE_LOWER_TAGS=
else
	MAKE_LOWER_TAGS="#"
fi

AC_SUBST(CTAGS)
AC_SUBST(ETAGS)

AC_SUBST(MAKE_UPPER_TAGS)
AC_SUBST(MAKE_LOWER_TAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MISSING_CHECK version: 6 updated: 2002/10/09 20:00:37
dnl ----------------
dnl
AC_DEFUN([CF_MISSING_CHECK],
[
AC_MSG_CHECKING([for missing \"$1\" extern])
AC_CACHE_VAL([cf_cv_func_$1],[
cf_save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $CHECK_DECL_FLAG"
AC_TRY_LINK([
$CHECK_DECL_HDRS

#undef $1
struct zowie { int a; double b; struct zowie *c; char d; };
extern struct zowie *$1();
],
[
#ifdef HAVE_LIBXT		/* needed for SunOS 4.0.3 or 4.1 */
XtToolkitInitialize();
#endif
],
[eval 'cf_cv_func_'$1'=yes'],
[eval 'cf_cv_func_'$1'=no'])
CPPFLAGS="$cf_save_CPPFLAGS"
])
eval 'cf_result=$cf_cv_func_'$1
AC_MSG_RESULT($cf_result)
test $cf_result = yes && AC_DEFINE_UNQUOTED(MISSING_EXTERN_$2)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MISSING_EXTERN version: 3 updated: 1997/09/06 15:25:32
dnl -----------------
dnl
AC_DEFUN([CF_MISSING_EXTERN],
[for ac_func in $1
do
CF_UPPER(ac_tr_func,$ac_func)
CF_MISSING_CHECK(${ac_func}, ${ac_tr_func})dnl
done
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MIXEDCASE_FILENAMES version: 3 updated: 2003/09/20 17:07:55
dnl ----------------------
dnl Check if the file-system supports mixed-case filenames.  If we're able to
dnl create a lowercase name and see it as uppercase, it doesn't support that.
AC_DEFUN([CF_MIXEDCASE_FILENAMES],
[
AC_CACHE_CHECK(if filesystem supports mixed-case filenames,cf_cv_mixedcase,[
if test "$cross_compiling" = yes ; then
	case $target_alias in #(vi
	*-os2-emx*|*-msdosdjgpp*|*-cygwin*|*-mingw32*|*-uwin*) #(vi
		cf_cv_mixedcase=no
		;;
	*)
		cf_cv_mixedcase=yes
		;;
	esac
else
	rm -f conftest CONFTEST
	echo test >conftest
	if test -f CONFTEST ; then
		cf_cv_mixedcase=no
	else
		cf_cv_mixedcase=yes
	fi
	rm -f conftest CONFTEST
fi
])
test "$cf_cv_mixedcase" = yes && AC_DEFINE(MIXEDCASE_FILENAMES)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MKSTEMP version: 6 updated: 2010/05/22 14:44:30
dnl ----------
dnl Check for a working mkstemp.  This creates two files, checks that they are
dnl successfully created and distinct (AmigaOS apparently fails on the last).
AC_DEFUN([CF_MKSTEMP],[
AC_CACHE_CHECK(for working mkstemp, cf_cv_func_mkstemp,[
rm -f conftest*
AC_TRY_RUN([
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
int main()
{
	char *tmpl = "conftestXXXXXX";
	char name[2][80];
	int n;
	int result = 0;
	int fd;
	struct stat sb;

	umask(077);
	for (n = 0; n < 2; ++n) {
		strcpy(name[n], tmpl);
		if ((fd = mkstemp(name[n])) >= 0) {
			if (!strcmp(name[n], tmpl)
			 || stat(name[n], &sb) != 0
			 || (sb.st_mode & S_IFMT) != S_IFREG
			 || (sb.st_mode & 077) != 0) {
				result = 1;
			}
			close(fd);
		}
	}
	if (result == 0
	 && !strcmp(name[0], name[1]))
		result = 1;
	${cf_cv_main_return:-return}(result);
}
],[cf_cv_func_mkstemp=yes
],[cf_cv_func_mkstemp=no
],[AC_CHECK_FUNC(mkstemp)
])
])
if test "x$cf_cv_func_mkstemp" = xyes || test "x$ac_cv_func_mkstemp" = xyes ; then
	AC_DEFINE(HAVE_MKSTEMP)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MSG_LOG version: 4 updated: 2007/07/29 09:55:12
dnl ----------
dnl Write a debug message to config.log, along with the line number in the
dnl configure script.
AC_DEFUN([CF_MSG_LOG],[
echo "${as_me-configure}:__oline__: testing $* ..." 1>&AC_FD_CC
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_CC_CHECK version: 4 updated: 2007/07/29 10:39:05
dnl -------------------
dnl Check if we can compile with ncurses' header file
dnl $1 is the cache variable to set
dnl $2 is the header-file to include
dnl $3 is the root name (ncurses or ncursesw)
AC_DEFUN([CF_NCURSES_CC_CHECK],[
	AC_TRY_COMPILE([
]ifelse($3,ncursesw,[
#define _XOPEN_SOURCE_EXTENDED
#undef  HAVE_LIBUTF8_H	/* in case we used CF_UTF8_LIB */
#define HAVE_LIBUTF8_H	/* to force ncurses' header file to use cchar_t */
])[
#include <$2>],[
#ifdef NCURSES_VERSION
]ifelse($3,ncursesw,[
#ifndef WACS_BSSB
	make an error
#endif
])[
printf("%s\n", NCURSES_VERSION);
#else
#ifdef __NCURSES_H
printf("old\n");
#else
	make an error
#endif
#endif
	]
	,[$1=$2]
	,[$1=no])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_CONFIG version: 8 updated: 2010/07/08 05:17:30
dnl -----------------
dnl Tie together the configure-script macros for ncurses.
dnl Prefer the "-config" script from ncurses 6.x, to simplify analysis.
dnl Allow that to be overridden using the $NCURSES_CONFIG environment variable.
dnl
dnl $1 is the root library name (default: "ncurses")
AC_DEFUN([CF_NCURSES_CONFIG],
[
cf_ncuconfig_root=ifelse($1,,ncurses,$1)

echo "Looking for ${cf_ncuconfig_root}-config"
AC_PATH_PROGS(NCURSES_CONFIG,${cf_ncuconfig_root}6-config ${cf_ncuconfig_root}5-config,none)

if test "$NCURSES_CONFIG" != none ; then

CPPFLAGS="$CPPFLAGS `$NCURSES_CONFIG --cflags`"
CF_ADD_LIBS(`$NCURSES_CONFIG --libs`)

# even with config script, some packages use no-override for curses.h
CF_CURSES_HEADER(ifelse($1,,ncurses,$1))

dnl like CF_NCURSES_CPPFLAGS
AC_DEFINE(NCURSES)

dnl like CF_NCURSES_LIBS
CF_UPPER(cf_nculib_ROOT,HAVE_LIB$cf_ncuconfig_root)
AC_DEFINE_UNQUOTED($cf_nculib_ROOT)

dnl like CF_NCURSES_VERSION
cf_cv_ncurses_version=`$NCURSES_CONFIG --version`

else

CF_NCURSES_CPPFLAGS(ifelse($1,,ncurses,$1))
CF_NCURSES_LIBS(ifelse($1,,ncurses,$1))

fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_CPPFLAGS version: 19 updated: 2007/07/29 13:35:20
dnl -------------------
dnl Look for the SVr4 curses clone 'ncurses' in the standard places, adjusting
dnl the CPPFLAGS variable so we can include its header.
dnl
dnl The header files may be installed as either curses.h, or ncurses.h (would
dnl be obsolete, except that some packagers prefer this name to distinguish it
dnl from a "native" curses implementation).  If not installed for overwrite,
dnl the curses.h file would be in an ncurses subdirectory (e.g.,
dnl /usr/include/ncurses), but someone may have installed overwriting the
dnl vendor's curses.  Only very old versions (pre-1.9.2d, the first autoconf'd
dnl version) of ncurses don't define either __NCURSES_H or NCURSES_VERSION in
dnl the header.
dnl
dnl If the installer has set $CFLAGS or $CPPFLAGS so that the ncurses header
dnl is already in the include-path, don't even bother with this, since we cannot
dnl easily determine which file it is.  In this case, it has to be <curses.h>.
dnl
dnl The optional parameter gives the root name of the library, in case it is
dnl not installed as the default curses library.  That is how the
dnl wide-character version of ncurses is installed.
AC_DEFUN([CF_NCURSES_CPPFLAGS],
[AC_REQUIRE([CF_WITH_CURSES_DIR])

AC_PROVIDE([CF_CURSES_CPPFLAGS])dnl
cf_ncuhdr_root=ifelse($1,,ncurses,$1)

test -n "$cf_cv_curses_dir" && \
test "$cf_cv_curses_dir" != "no" && { \
  CF_ADD_INCDIR($cf_cv_curses_dir/include $cf_cv_curses_dir/include/$cf_ncuhdr_root)
}

AC_CACHE_CHECK(for $cf_ncuhdr_root header in include-path, cf_cv_ncurses_h,[
	cf_header_list="$cf_ncuhdr_root/curses.h $cf_ncuhdr_root/ncurses.h"
	( test "$cf_ncuhdr_root" = ncurses || test "$cf_ncuhdr_root" = ncursesw ) && cf_header_list="$cf_header_list curses.h ncurses.h"
	for cf_header in $cf_header_list
	do
		CF_NCURSES_CC_CHECK(cf_cv_ncurses_h,$cf_header,$1)
		test "$cf_cv_ncurses_h" != no && break
	done
])

CF_NCURSES_HEADER
CF_TERM_HEADER

# some applications need this, but should check for NCURSES_VERSION
AC_DEFINE(NCURSES)

CF_NCURSES_VERSION
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_HEADER version: 2 updated: 2008/03/23 14:48:54
dnl -----------------
dnl Find a "curses" header file, e.g,. "curses.h", or one of the more common
dnl variations of ncurses' installs.
dnl
dnl See also CF_CURSES_HEADER, which sets the same cache variable.
AC_DEFUN([CF_NCURSES_HEADER],[

if test "$cf_cv_ncurses_h" != no ; then
	cf_cv_ncurses_header=$cf_cv_ncurses_h
else

AC_CACHE_CHECK(for $cf_ncuhdr_root include-path, cf_cv_ncurses_h2,[
	test -n "$verbose" && echo
	CF_HEADER_PATH(cf_search,$cf_ncuhdr_root)
	test -n "$verbose" && echo search path $cf_search
	cf_save2_CPPFLAGS="$CPPFLAGS"
	for cf_incdir in $cf_search
	do
		CF_ADD_INCDIR($cf_incdir)
		for cf_header in \
			ncurses.h \
			curses.h
		do
			CF_NCURSES_CC_CHECK(cf_cv_ncurses_h2,$cf_header,$1)
			if test "$cf_cv_ncurses_h2" != no ; then
				cf_cv_ncurses_h2=$cf_incdir/$cf_header
				test -n "$verbose" && echo $ac_n "	... found $ac_c" 1>&AC_FD_MSG
				break
			fi
			test -n "$verbose" && echo "	... tested $cf_incdir/$cf_header" 1>&AC_FD_MSG
		done
		CPPFLAGS="$cf_save2_CPPFLAGS"
		test "$cf_cv_ncurses_h2" != no && break
	done
	test "$cf_cv_ncurses_h2" = no && AC_MSG_ERROR(not found)
	])

	CF_DIRNAME(cf_1st_incdir,$cf_cv_ncurses_h2)
	cf_cv_ncurses_header=`basename $cf_cv_ncurses_h2`
	if test `basename $cf_1st_incdir` = $cf_ncuhdr_root ; then
		cf_cv_ncurses_header=$cf_ncuhdr_root/$cf_cv_ncurses_header
	fi
	CF_ADD_INCDIR($cf_1st_incdir)

fi

# Set definitions to allow ifdef'ing for ncurses.h

case $cf_cv_ncurses_header in # (vi
*ncurses.h)
	AC_DEFINE(HAVE_NCURSES_H)
	;;
esac

case $cf_cv_ncurses_header in # (vi
ncurses/curses.h|ncurses/ncurses.h)
	AC_DEFINE(HAVE_NCURSES_NCURSES_H)
	;;
ncursesw/curses.h|ncursesw/ncurses.h)
	AC_DEFINE(HAVE_NCURSESW_NCURSES_H)
	;;
esac

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_LIBS version: 14 updated: 2010/06/20 09:24:28
dnl ---------------
dnl Look for the ncurses library.  This is a little complicated on Linux,
dnl because it may be linked with the gpm (general purpose mouse) library.
dnl Some distributions have gpm linked with (bsd) curses, which makes it
dnl unusable with ncurses.  However, we don't want to link with gpm unless
dnl ncurses has a dependency, since gpm is normally set up as a shared library,
dnl and the linker will record a dependency.
dnl
dnl The optional parameter gives the root name of the library, in case it is
dnl not installed as the default curses library.  That is how the
dnl wide-character version of ncurses is installed.
AC_DEFUN([CF_NCURSES_LIBS],
[AC_REQUIRE([CF_NCURSES_CPPFLAGS])

cf_nculib_root=ifelse($1,,ncurses,$1)
	# This works, except for the special case where we find gpm, but
	# ncurses is in a nonstandard location via $LIBS, and we really want
	# to link gpm.
cf_ncurses_LIBS=""
cf_ncurses_SAVE="$LIBS"
AC_CHECK_LIB(gpm,Gpm_Open,
	[AC_CHECK_LIB(gpm,initscr,
		[LIBS="$cf_ncurses_SAVE"],
		[cf_ncurses_LIBS="-lgpm"])])

case $host_os in #(vi
freebsd*)
	# This is only necessary if you are linking against an obsolete
	# version of ncurses (but it should do no harm, since it's static).
	if test "$cf_nculib_root" = ncurses ; then
		AC_CHECK_LIB(mytinfo,tgoto,[cf_ncurses_LIBS="-lmytinfo $cf_ncurses_LIBS"])
	fi
	;;
esac

CF_ADD_LIBS($cf_ncurses_LIBS)

if ( test -n "$cf_cv_curses_dir" && test "$cf_cv_curses_dir" != "no" )
then
	CF_ADD_LIBDIR($cf_cv_curses_dir/lib)
	CF_ADD_LIBS(-l$cf_nculib_root)
else
	CF_FIND_LIBRARY($cf_nculib_root,$cf_nculib_root,
		[#include <${cf_cv_ncurses_header-curses.h}>],
		[initscr()],
		initscr)
fi

if test -n "$cf_ncurses_LIBS" ; then
	AC_MSG_CHECKING(if we can link $cf_nculib_root without $cf_ncurses_LIBS)
	cf_ncurses_SAVE="$LIBS"
	for p in $cf_ncurses_LIBS ; do
		q=`echo $LIBS | sed -e "s%$p %%" -e "s%$p$%%"`
		if test "$q" != "$LIBS" ; then
			LIBS="$q"
		fi
	done
	AC_TRY_LINK([#include <${cf_cv_ncurses_header-curses.h}>],
		[initscr(); mousemask(0,0); tgoto((char *)0, 0, 0);],
		[AC_MSG_RESULT(yes)],
		[AC_MSG_RESULT(no)
		 LIBS="$cf_ncurses_SAVE"])
fi

CF_UPPER(cf_nculib_ROOT,HAVE_LIB$cf_nculib_root)
AC_DEFINE_UNQUOTED($cf_nculib_ROOT)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_VERSION version: 12 updated: 2007/04/28 09:15:55
dnl ------------------
dnl Check for the version of ncurses, to aid in reporting bugs, etc.
dnl Call CF_CURSES_CPPFLAGS first, or CF_NCURSES_CPPFLAGS.  We don't use
dnl AC_REQUIRE since that does not work with the shell's if/then/else/fi.
AC_DEFUN([CF_NCURSES_VERSION],
[
AC_REQUIRE([CF_CURSES_CPPFLAGS])dnl
AC_CACHE_CHECK(for ncurses version, cf_cv_ncurses_version,[
	cf_cv_ncurses_version=no
	cf_tempfile=out$$
	rm -f $cf_tempfile
	AC_TRY_RUN([
#include <${cf_cv_ncurses_header-curses.h}>
#include <stdio.h>
int main()
{
	FILE *fp = fopen("$cf_tempfile", "w");
#ifdef NCURSES_VERSION
# ifdef NCURSES_VERSION_PATCH
	fprintf(fp, "%s.%d\n", NCURSES_VERSION, NCURSES_VERSION_PATCH);
# else
	fprintf(fp, "%s\n", NCURSES_VERSION);
# endif
#else
# ifdef __NCURSES_H
	fprintf(fp, "old\n");
# else
	make an error
# endif
#endif
	${cf_cv_main_return-return}(0);
}],[
	cf_cv_ncurses_version=`cat $cf_tempfile`],,[

	# This will not work if the preprocessor splits the line after the
	# Autoconf token.  The 'unproto' program does that.
	cat > conftest.$ac_ext <<EOF
#include <${cf_cv_ncurses_header-curses.h}>
#undef Autoconf
#ifdef NCURSES_VERSION
Autoconf NCURSES_VERSION
#else
#ifdef __NCURSES_H
Autoconf "old"
#endif
;
#endif
EOF
	cf_try="$ac_cpp conftest.$ac_ext 2>&AC_FD_CC | grep '^Autoconf ' >conftest.out"
	AC_TRY_EVAL(cf_try)
	if test -f conftest.out ; then
		cf_out=`cat conftest.out | sed -e 's%^Autoconf %%' -e 's%^[[^"]]*"%%' -e 's%".*%%'`
		test -n "$cf_out" && cf_cv_ncurses_version="$cf_out"
		rm -f conftest.out
	fi
])
	rm -f $cf_tempfile
])
test "$cf_cv_ncurses_version" = no || AC_DEFINE(NCURSES)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NO_LEAKS_OPTION version: 4 updated: 2006/12/16 14:24:05
dnl ------------------
dnl see CF_WITH_NO_LEAKS
AC_DEFUN([CF_NO_LEAKS_OPTION],[
AC_MSG_CHECKING(if you want to use $1 for testing)
AC_ARG_WITH($1,
	[$2],
	[AC_DEFINE($3)ifelse([$4],,[
	 $4
])
	: ${with_cflags:=-g}
	: ${with_no_leaks:=yes}
	 with_$1=yes],
	[with_$1=])
AC_MSG_RESULT(${with_$1:-no})

case .$with_cflags in #(vi
.*-g*)
	case .$CFLAGS in #(vi
	.*-g*) #(vi
		;;
	*)
		CF_ADD_CFLAGS([-g])
		;;
	esac
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_OUTPUT_IF_CHANGED version: 2 updated: 1997/09/07 18:53:59
dnl --------------------
dnl Within AC_OUTPUT, check if the given file differs from the target, and
dnl update it if so.  Otherwise, remove the generated file.
dnl
dnl Parameters:
dnl $1 = input, which configure has done substitutions upon
dnl $2 = target file
dnl
AC_DEFUN([CF_OUTPUT_IF_CHANGED],[
if ( cmp -s $1 $2 2>/dev/null )
then
	echo "$2 is unchanged"
	rm -f $1
else
	echo "creating $2"
	rm -f $2
	mv $1 $2
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PATHSEP version: 5 updated: 2010/05/26 05:38:42
dnl ----------
dnl Provide a value for the $PATH and similar separator
AC_DEFUN([CF_PATHSEP],
[
	case $cf_cv_system_name in
	os2*)	PATH_SEPARATOR=';'  ;;
	*)	PATH_SEPARATOR=':'  ;;
	esac
ifelse([$1],,,[$1=$PATH_SEPARATOR])
	AC_SUBST(PATH_SEPARATOR)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PATH_SYNTAX version: 13 updated: 2010/05/26 05:38:42
dnl --------------
dnl Check the argument to see that it looks like a pathname.  Rewrite it if it
dnl begins with one of the prefix/exec_prefix variables, and then again if the
dnl result begins with 'NONE'.  This is necessary to work around autoconf's
dnl delayed evaluation of those symbols.
AC_DEFUN([CF_PATH_SYNTAX],[
if test "x$prefix" != xNONE; then
  cf_path_syntax="$prefix"
else
  cf_path_syntax="$ac_default_prefix"
fi

case ".[$]$1" in #(vi
.\[$]\(*\)*|.\'*\'*) #(vi
  ;;
..|./*|.\\*) #(vi
  ;;
.[[a-zA-Z]]:[[\\/]]*) #(vi OS/2 EMX
  ;;
.\[$]{*prefix}*) #(vi
  eval $1="[$]$1"
  case ".[$]$1" in #(vi
  .NONE/*)
    $1=`echo [$]$1 | sed -e s%NONE%$cf_path_syntax%`
    ;;
  esac
  ;; #(vi
.no|.NONE/*)
  $1=`echo [$]$1 | sed -e s%NONE%$cf_path_syntax%`
  ;;
*)
  ifelse([$2],,[AC_MSG_ERROR([expected a pathname, not \"[$]$1\"])],$2)
  ;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PKG_CONFIG version: 3 updated: 2009/01/25 10:55:09
dnl -------------
dnl Check for the package-config program, unless disabled by command-line.
AC_DEFUN([CF_PKG_CONFIG],
[
AC_MSG_CHECKING(if you want to use pkg-config)
AC_ARG_WITH(pkg-config,
	[  --with-pkg-config{=path} enable/disable use of pkg-config],
	[cf_pkg_config=$withval],
	[cf_pkg_config=yes])
AC_MSG_RESULT($cf_pkg_config)

case $cf_pkg_config in #(vi
no) #(vi
	PKG_CONFIG=none
	;;
yes) #(vi
	AC_PATH_PROG(PKG_CONFIG, pkg-config, none)
	;;
*)
	PKG_CONFIG=$withval
	;;
esac

test -z "$PKG_CONFIG" && PKG_CONFIG=none
if test "$PKG_CONFIG" != none ; then
	CF_PATH_SYNTAX(PKG_CONFIG)
fi

AC_SUBST(PKG_CONFIG)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_POSIX_C_SOURCE version: 8 updated: 2010/05/26 05:38:42
dnl -----------------
dnl Define _POSIX_C_SOURCE to the given level, and _POSIX_SOURCE if needed.
dnl
dnl	POSIX.1-1990				_POSIX_SOURCE
dnl	POSIX.1-1990 and			_POSIX_SOURCE and
dnl		POSIX.2-1992 C-Language			_POSIX_C_SOURCE=2
dnl		Bindings Option
dnl	POSIX.1b-1993				_POSIX_C_SOURCE=199309L
dnl	POSIX.1c-1996				_POSIX_C_SOURCE=199506L
dnl	X/Open 2000				_POSIX_C_SOURCE=200112L
dnl
dnl Parameters:
dnl	$1 is the nominal value for _POSIX_C_SOURCE
AC_DEFUN([CF_POSIX_C_SOURCE],
[
cf_POSIX_C_SOURCE=ifelse([$1],,199506L,[$1])

cf_save_CFLAGS="$CFLAGS"
cf_save_CPPFLAGS="$CPPFLAGS"

CF_REMOVE_DEFINE(cf_trim_CFLAGS,$cf_save_CFLAGS,_POSIX_C_SOURCE)
CF_REMOVE_DEFINE(cf_trim_CPPFLAGS,$cf_save_CPPFLAGS,_POSIX_C_SOURCE)

AC_CACHE_CHECK(if we should define _POSIX_C_SOURCE,cf_cv_posix_c_source,[
	CF_MSG_LOG(if the symbol is already defined go no further)
	AC_TRY_COMPILE([#include <sys/types.h>],[
#ifndef _POSIX_C_SOURCE
make an error
#endif],
	[cf_cv_posix_c_source=no],
	[cf_want_posix_source=no
	 case .$cf_POSIX_C_SOURCE in #(vi
	 .[[12]]??*) #(vi
		cf_cv_posix_c_source="-D_POSIX_C_SOURCE=$cf_POSIX_C_SOURCE"
		;;
	 .2) #(vi
		cf_cv_posix_c_source="-D_POSIX_C_SOURCE=$cf_POSIX_C_SOURCE"
		cf_want_posix_source=yes
		;;
	 .*)
		cf_want_posix_source=yes
		;;
	 esac
	 if test "$cf_want_posix_source" = yes ; then
		AC_TRY_COMPILE([#include <sys/types.h>],[
#ifdef _POSIX_SOURCE
make an error
#endif],[],
		cf_cv_posix_c_source="$cf_cv_posix_c_source -D_POSIX_SOURCE")
	 fi
	 CF_MSG_LOG(ifdef from value $cf_POSIX_C_SOURCE)
	 CFLAGS="$cf_trim_CFLAGS"
	 CPPFLAGS="$cf_trim_CPPFLAGS $cf_cv_posix_c_source"
	 CF_MSG_LOG(if the second compile does not leave our definition intact error)
	 AC_TRY_COMPILE([#include <sys/types.h>],[
#ifndef _POSIX_C_SOURCE
make an error
#endif],,
	 [cf_cv_posix_c_source=no])
	 CFLAGS="$cf_save_CFLAGS"
	 CPPFLAGS="$cf_save_CPPFLAGS"
	])
])

if test "$cf_cv_posix_c_source" != no ; then
	CFLAGS="$cf_trim_CFLAGS"
	CPPFLAGS="$cf_trim_CPPFLAGS"
	CF_ADD_CFLAGS($cf_cv_posix_c_source)
fi

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PREREQ_COMPARE version: 2 updated: 1997/09/06 13:24:56
dnl -----------------
dnl CF_PREREQ_COMPARE(MAJOR1, MINOR1, TERNARY1, MAJOR2, MINOR2, TERNARY2,
dnl                   PRINTABLE2, not FOUND, FOUND)
define(CF_PREREQ_COMPARE,
[ifelse(builtin([eval], [$3 < $6]), 1,
ifelse([$8], , ,[$8]),
ifelse([$9], , ,[$9]))])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PROG_EXT version: 10 updated: 2004/01/03 19:28:18
dnl -----------
dnl Compute $PROG_EXT, used for non-Unix ports, such as OS/2 EMX.
AC_DEFUN([CF_PROG_EXT],
[
AC_REQUIRE([CF_CHECK_CACHE])
case $cf_cv_system_name in
os2*)
    CFLAGS="$CFLAGS -Zmt"
    CPPFLAGS="$CPPFLAGS -D__ST_MT_ERRNO__"
    CXXFLAGS="$CXXFLAGS -Zmt"
    # autoconf's macro sets -Zexe and suffix both, which conflict:w
    LDFLAGS="$LDFLAGS -Zmt -Zcrtdll"
    ac_cv_exeext=.exe
    ;;
esac

AC_EXEEXT
AC_OBJEXT

PROG_EXT="$EXEEXT"
AC_SUBST(PROG_EXT)
test -n "$PROG_EXT" && AC_DEFINE_UNQUOTED(PROG_EXT,"$PROG_EXT")
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PROG_PERL version: 5 updated: 2001/12/10 01:28:30
dnl ------------
dnl Check for Perl, given the minimum version, to ensure that required features
dnl are present.
dnl $1 = the minimum version
AC_DEFUN([CF_PROG_PERL],
[# find perl binary
AC_MSG_CHECKING([for ifelse([$1],,perl,[perl-$1])])
AC_CACHE_VAL(cf_cv_prog_PERL,
[ifelse([$1],,,[echo "configure:__oline__: ...version $1 required" >&AC_FD_CC
  ])# allow user to override
  if test -n "$PERL"; then
    cf_try="$PERL"
  else
    cf_try="perl perl5"
  fi

  cf_version=`echo '[[]]'|sed -e 's/^./$/'`
  for cf_prog in $cf_try; do
    echo "configure:__oline__: trying $cf_prog" >&AC_FD_CC
    if ($cf_prog -e 'printf "found version %g\n",'$cf_version'][dnl
ifelse([$1],,,[;exit('$cf_version'<$1)])') 1>&AC_FD_CC 2>&1; then
      cf_cv_prog_PERL=$cf_prog
      break
    fi
  done])dnl
PERL="$cf_cv_prog_PERL"
if test -n "$PERL"; then
  AC_MSG_RESULT($PERL)
else
  AC_MSG_RESULT(no)
fi
AC_SUBST(PERL)dnl
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_REMOVE_DEFINE version: 3 updated: 2010/01/09 11:05:50
dnl ----------------
dnl Remove all -U and -D options that refer to the given symbol from a list
dnl of C compiler options.  This works around the problem that not all
dnl compilers process -U and -D options from left-to-right, so a -U option
dnl cannot be used to cancel the effect of a preceding -D option.
dnl
dnl $1 = target (which could be the same as the source variable)
dnl $2 = source (including '$')
dnl $3 = symbol to remove
define([CF_REMOVE_DEFINE],
[
$1=`echo "$2" | \
	sed	-e 's/-[[UD]]'"$3"'\(=[[^ 	]]*\)\?[[ 	]]/ /g' \
		-e 's/-[[UD]]'"$3"'\(=[[^ 	]]*\)\?[$]//g'`
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_RESTARTABLE_PIPEREAD version: 4 updated: 2007/04/28 09:17:29
dnl -----------------------
dnl CF_RESTARTABLE_PIPEREAD is a modified version of AC_RESTARTABLE_SYSCALLS
dnl from acspecific.m4, which uses a read on a pipe (surprise!) rather than a
dnl wait() as the test code.  apparently there is a POSIX change, which OSF/1
dnl at least has adapted to, which says reads (or writes) on pipes for which no
dnl data has been transferred are interruptable _regardless_ of the SA_RESTART
dnl bit.  yuck.
AC_DEFUN([CF_RESTARTABLE_PIPEREAD],
[
AC_CACHE_CHECK(for restartable reads on pipes, cf_cv_can_restart_read,[
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
      ${cf_cv_main_return-return} (0);
  }
  sigwrapper (SIGINT, ucatch);
  status = read(fd[0], buff, sizeof(buff));
  wait (&i);
  ${cf_cv_main_return-return} (status == -1);
}
],
[cf_cv_can_restart_read=yes],
[cf_cv_can_restart_read=no],
[cf_cv_can_restart_read=unknown])
])

test $cf_cv_can_restart_read = yes && AC_DEFINE(HAVE_RESTARTABLE_PIPEREAD)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_RPATH_HACK version: 8 updated: 2010/04/17 15:38:58
dnl -------------
AC_DEFUN([CF_RPATH_HACK],
[
AC_REQUIRE([CF_LD_RPATH_OPT])
AC_MSG_CHECKING(for updated LDFLAGS)
if test -n "$LD_RPATH_OPT" ; then
	AC_MSG_RESULT(maybe)

	AC_CHECK_PROGS(cf_ldd_prog,ldd,no)
	cf_rpath_list="/usr/lib /lib"
	if test "$cf_ldd_prog" != no
	then
AC_TRY_LINK([#include <stdio.h>],
		[printf("Hello");],
		[cf_rpath_list=`$cf_ldd_prog conftest$ac_exeext | fgrep / | sed -e 's%^.*[[ 	]]/%/%' -e 's%/[[^/]][[^/]]*$%%' |sort -u`])
	fi

	CF_VERBOSE(...checking EXTRA_LDFLAGS $EXTRA_LDFLAGS)

	CF_RPATH_HACK_2(LDFLAGS)
	CF_RPATH_HACK_2(LIBS)

	CF_VERBOSE(...checked EXTRA_LDFLAGS $EXTRA_LDFLAGS)
fi
AC_SUBST(EXTRA_LDFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_RPATH_HACK_2 version: 6 updated: 2010/04/17 16:31:24
dnl ---------------
dnl Do one set of substitutions for CF_RPATH_HACK, adding an rpath option to
dnl EXTRA_LDFLAGS for each -L option found.
dnl
dnl $cf_rpath_list contains a list of directories to ignore.
dnl
dnl $1 = variable name to update.  The LDFLAGS variable should be the only one,
dnl      but LIBS often has misplaced -L options.
AC_DEFUN([CF_RPATH_HACK_2],
[
CF_VERBOSE(...checking $1 [$]$1)

cf_rpath_dst=
for cf_rpath_src in [$]$1
do
	case $cf_rpath_src in #(vi
	-L*) #(vi

		# check if this refers to a directory which we will ignore
		cf_rpath_skip=no
		if test -n "$cf_rpath_list"
		then
			for cf_rpath_item in $cf_rpath_list
			do
				if test "x$cf_rpath_src" = "x-L$cf_rpath_item"
				then
					cf_rpath_skip=yes
					break
				fi
			done
		fi

		if test "$cf_rpath_skip" = no
		then
			# transform the option
			if test "$LD_RPATH_OPT" = "-R " ; then
				cf_rpath_tmp=`echo "$cf_rpath_src" |sed -e "s%-L%-R %"`
			else
				cf_rpath_tmp=`echo "$cf_rpath_src" |sed -e "s%-L%$LD_RPATH_OPT%"`
			fi

			# if we have not already added this, add it now
			cf_rpath_tst=`echo "$EXTRA_LDFLAGS" | sed -e "s%$cf_rpath_tmp %%"`
			if test "x$cf_rpath_tst" = "x$EXTRA_LDFLAGS"
			then
				CF_VERBOSE(...Filter $cf_rpath_src ->$cf_rpath_tmp)
				EXTRA_LDFLAGS="$cf_rpath_tmp $EXTRA_LDFLAGS"
			fi
		fi
		;;
	esac
	cf_rpath_dst="$cf_rpath_dst $cf_rpath_src"
done
$1=$cf_rpath_dst

CF_VERBOSE(...checked $1 [$]$1)
AC_SUBST(EXTRA_LDFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_SIGWINCH version: 1 updated: 2006/04/02 16:41:09
dnl -----------
dnl Use this macro after CF_XOPEN_SOURCE, but do not require it (not all
dnl programs need this test).
dnl
dnl This is really a MacOS X 10.4.3 workaround.  Defining _POSIX_C_SOURCE
dnl forces SIGWINCH to be undefined (breaks xterm, ncurses).  Oddly, the struct
dnl winsize declaration is left alone - we may revisit this if Apple choose to
dnl break that part of the interface as well.
AC_DEFUN([CF_SIGWINCH],
[
AC_CACHE_CHECK(if SIGWINCH is defined,cf_cv_define_sigwinch,[
	AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/signal.h>
],[int x = SIGWINCH],
	[cf_cv_define_sigwinch=yes],
	[AC_TRY_COMPILE([
#undef _XOPEN_SOURCE
#undef _POSIX_SOURCE
#undef _POSIX_C_SOURCE
#include <sys/types.h>
#include <sys/signal.h>
],[int x = SIGWINCH],
	[cf_cv_define_sigwinch=maybe],
	[cf_cv_define_sigwinch=no])
])
])

if test "$cf_cv_define_sigwinch" = maybe ; then
AC_CACHE_CHECK(for actual SIGWINCH definition,cf_cv_fixup_sigwinch,[
cf_cv_fixup_sigwinch=unknown
cf_sigwinch=32
while test $cf_sigwinch != 1
do
	AC_TRY_COMPILE([
#undef _XOPEN_SOURCE
#undef _POSIX_SOURCE
#undef _POSIX_C_SOURCE
#include <sys/types.h>
#include <sys/signal.h>
],[
#if SIGWINCH != $cf_sigwinch
make an error
#endif
int x = SIGWINCH],
	[cf_cv_fixup_sigwinch=$cf_sigwinch
	 break])

cf_sigwinch=`expr $cf_sigwinch - 1`
done
])

	if test "$cf_cv_fixup_sigwinch" != unknown ; then
		CPPFLAGS="$CPPFLAGS -DSIGWINCH=$cf_cv_fixup_sigwinch"
	fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_SIZECHANGE version: 8 updated: 2000/11/04 12:22:16
dnl -------------
dnl Check for definitions & structures needed for window size-changing
dnl FIXME: check that this works with "snake" (HP-UX 10.x)
AC_DEFUN([CF_SIZECHANGE],
[
AC_REQUIRE([CF_STRUCT_TERMIOS])
AC_CACHE_CHECK(declaration of size-change, cf_cv_sizechange,[
    cf_cv_sizechange=unknown
    cf_save_CPPFLAGS="$CPPFLAGS"

for cf_opts in "" "NEED_PTEM_H"
do

    CPPFLAGS="$cf_save_CPPFLAGS"
    test -n "$cf_opts" && CPPFLAGS="$CPPFLAGS -D$cf_opts"
    AC_TRY_COMPILE([#include <sys/types.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#else
#ifdef HAVE_TERMIO_H
#include <termio.h>
#endif
#endif
#ifdef NEED_PTEM_H
/* This is a workaround for SCO:  they neglected to define struct winsize in
 * termios.h -- it's only in termio.h and ptem.h
 */
#include        <sys/stream.h>
#include        <sys/ptem.h>
#endif
#if !defined(sun) || !defined(HAVE_TERMIOS_H)
#include <sys/ioctl.h>
#endif
],[
#ifdef TIOCGSIZE
	struct ttysize win;	/* FIXME: what system is this? */
	int y = win.ts_lines;
	int x = win.ts_cols;
#else
#ifdef TIOCGWINSZ
	struct winsize win;
	int y = win.ws_row;
	int x = win.ws_col;
#else
	no TIOCGSIZE or TIOCGWINSZ
#endif /* TIOCGWINSZ */
#endif /* TIOCGSIZE */
	],
	[cf_cv_sizechange=yes],
	[cf_cv_sizechange=no])

	CPPFLAGS="$cf_save_CPPFLAGS"
	if test "$cf_cv_sizechange" = yes ; then
		echo "size-change succeeded ($cf_opts)" >&AC_FD_CC
		test -n "$cf_opts" && cf_cv_sizechange="$cf_opts"
		break
	fi
done
])
if test "$cf_cv_sizechange" != no ; then
	AC_DEFINE(HAVE_SIZECHANGE)
	case $cf_cv_sizechange in #(vi
	NEED*)
		AC_DEFINE_UNQUOTED($cf_cv_sizechange )
		;;
	esac
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_STDIO_UNLOCKED version: 3 updated: 2009/10/06 19:54:39
dnl -----------------
dnl The four functions getc_unlocked(), getchar_unlocked(), putc_unlocked(),
dnl putchar_unlocked() are in POSIX.1-2001.
dnl
dnl Test for one or more of the "unlocked" stdio getc/putc functions, and (if
dnl the system requires it to declare the prototype) define _REENTRANT
dnl
dnl $1 = one or more stdio functions to check for existence and prototype.
AC_DEFUN([CF_STDIO_UNLOCKED],
[
cf_stdio_unlocked=no
AC_CHECK_FUNCS(ifelse([$1],,[getc_unlocked putc_unlocked],[$1]),
	[cf_stdio_unlocked=$ac_func])
if test "$cf_stdio_unlocked" != no ; then
	case "$CPPFLAGS" in #(vi
	*-D_REENTRANT*) #(vi
		;;
	*)
	AC_CACHE_CHECK(if we should define _REENTRANT,cf_cv_stdio_unlocked,[
	AC_TRY_COMPILE([#include <stdio.h>],[
		extern void *$cf_stdio_unlocked(void *);
		void *dummy = $cf_stdio_unlocked(0)],
			[cf_cv_stdio_unlocked=yes],
			[cf_cv_stdio_unlocked=no])])
		if test "$cf_cv_stdio_unlocked" = yes ; then
			AC_DEFINE(_REENTRANT)
		fi
		;;
	esac
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_STRUCT_TERMIOS version: 5 updated: 2000/11/04 12:22:46
dnl -----------------
dnl Some machines require _POSIX_SOURCE to completely define struct termios.
dnl If so, define SVR4_TERMIO
AC_DEFUN([CF_STRUCT_TERMIOS],[
AC_CHECK_HEADERS( \
termio.h \
termios.h \
unistd.h \
)
if test "$ISC" = yes ; then
	AC_CHECK_HEADERS( sys/termio.h )
fi
if test "$ac_cv_header_termios_h" = yes ; then
	case "$CFLAGS $CPPFLAGS" in
	*-D_POSIX_SOURCE*)
		termios_bad=dunno ;;
	*)	termios_bad=maybe ;;
	esac
	if test "$termios_bad" = maybe ; then
	AC_MSG_CHECKING(whether termios.h needs _POSIX_SOURCE)
	AC_TRY_COMPILE([#include <termios.h>],
		[struct termios foo; int x = foo.c_iflag],
		termios_bad=no, [
		AC_TRY_COMPILE([
#define _POSIX_SOURCE
#include <termios.h>],
			[struct termios foo; int x = foo.c_iflag],
			termios_bad=unknown,
			termios_bad=yes AC_DEFINE(SVR4_TERMIO))
			])
	AC_MSG_RESULT($termios_bad)
	fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_SUBDIR_PATH version: 6 updated: 2010/04/21 06:20:50
dnl --------------
dnl Construct a search-list for a nonstandard header/lib-file
dnl	$1 = the variable to return as result
dnl	$2 = the package name
dnl	$3 = the subdirectory, e.g., bin, include or lib
AC_DEFUN([CF_SUBDIR_PATH],
[
$1=

CF_ADD_SUBDIR_PATH($1,$2,$3,/usr,$prefix)
CF_ADD_SUBDIR_PATH($1,$2,$3,$prefix,NONE)
CF_ADD_SUBDIR_PATH($1,$2,$3,/usr/local,$prefix)
CF_ADD_SUBDIR_PATH($1,$2,$3,/opt,$prefix)
CF_ADD_SUBDIR_PATH($1,$2,$3,[$]HOME,$prefix)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_SUBST version: 4 updated: 2006/06/17 12:33:03
dnl --------
dnl	Shorthand macro for substituting things that the user may override
dnl	with an environment variable.
dnl
dnl	$1 = long/descriptive name
dnl	$2 = environment variable
dnl	$3 = default value
AC_DEFUN([CF_SUBST],
[AC_CACHE_VAL(cf_cv_subst_$2,[
AC_MSG_CHECKING(for $1 (symbol $2))
CF_SUBST_IF([-z "[$]$2"], [$2], [$3])
cf_cv_subst_$2=[$]$2
AC_MSG_RESULT([$]$2)
])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_SUBST_IF version: 2 updated: 2006/06/17 12:33:03
dnl -----------
dnl	Shorthand macro for substituting things that the user may override
dnl	with an environment variable.
dnl
dnl	$1 = condition to pass to "test"
dnl	$2 = environment variable
dnl	$3 = value if the test succeeds
dnl	$4 = value if the test fails
AC_DEFUN([CF_SUBST_IF],
[
if test $1 ; then
	$2=$3
ifelse($4,,,[else
	$2=$4])
fi
AC_SUBST($2)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_SYS_ERRLIST version: 6 updated: 2001/12/30 13:03:23
dnl --------------
dnl Check for declaration of sys_nerr and sys_errlist in one of stdio.h and
dnl errno.h.  Declaration of sys_errlist on BSD4.4 interferes with our
dnl declaration.  Reported by Keith Bostic.
AC_DEFUN([CF_SYS_ERRLIST],
[
    CF_CHECK_ERRNO(sys_nerr)
    CF_CHECK_ERRNO(sys_errlist)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_TERMCAP_LIBS version: 12 updated: 2010/06/20 09:24:28
dnl ---------------
dnl Look for termcap libraries, or the equivalent in terminfo.
dnl
dnl The optional parameter may be "ncurses", "ncursesw".
AC_DEFUN([CF_TERMCAP_LIBS],
[
AC_CACHE_VAL(cf_cv_termlib,[
cf_cv_termlib=none
AC_TRY_LINK([],[char *x=(char*)tgoto("",0,0)],
[AC_TRY_LINK([],[int x=tigetstr("")],
	[cf_cv_termlib=terminfo],
	[cf_cv_termlib=termcap])
	CF_VERBOSE(using functions in predefined $cf_cv_termlib LIBS)
],[
ifelse([$1],,,[
case "$1" in # (vi
ncurses*)
	CF_NCURSES_CONFIG($1)
	cf_cv_termlib=terminfo
	;;
esac
])
if test "$cf_cv_termlib" = none; then
	# FreeBSD's linker gives bogus results for AC_CHECK_LIB, saying that
	# tgetstr lives in -lcurses when it is only an unsatisfied extern.
	cf_save_LIBS="$LIBS"
	for cf_lib in curses ncurses termlib termcap
	do
	LIBS="-l$cf_lib $cf_save_LIBS"
	for cf_func in tigetstr tgetstr
	do
		AC_MSG_CHECKING(for $cf_func in -l$cf_lib)
		AC_TRY_LINK([],[int x=$cf_func("")],[cf_result=yes],[cf_result=no])
		AC_MSG_RESULT($cf_result)
		if test "$cf_result" = yes ; then
			if test "$cf_func" = tigetstr ; then
				cf_cv_termlib=terminfo
			else
				cf_cv_termlib=termcap
			fi
			break
		fi
	done
		test "$cf_result" = yes && break
	done
	test "$cf_result" = no && LIBS="$cf_save_LIBS"
fi
if test "$cf_cv_termlib" = none; then
	# allow curses library for broken AIX system.
	AC_CHECK_LIB(curses, initscr, [CF_ADD_LIBS(-lcurses) cf_cv_termlib=termcap])
	AC_CHECK_LIB(termcap, tgoto, [CF_ADD_LIBS(-ltermcap) cf_cv_termlib=termcap])
fi
])
if test "$cf_cv_termlib" = none; then
	AC_MSG_WARN([Cannot find -ltermlib, -lcurses, or -ltermcap])
fi
])])dnl
dnl ---------------------------------------------------------------------------
dnl CF_TERM_HEADER version: 1 updated: 2005/12/31 13:26:39
dnl --------------
dnl Look for term.h, which is part of X/Open curses.  It defines the interface
dnl to terminfo database.  Usually it is in the same include-path as curses.h,
dnl but some packagers change this, breaking various applications.
AC_DEFUN([CF_TERM_HEADER],[
AC_CACHE_CHECK(for terminfo header, cf_cv_term_header,[
case ${cf_cv_ncurses_header} in #(vi
*/ncurses.h|*/ncursesw.h) #(vi
	cf_term_header=`echo "$cf_cv_ncurses_header" | sed -e 's%ncurses[[^.]]*\.h$%term.h%'`
	;;
*)
	cf_term_header=term.h
	;;
esac

for cf_test in $cf_term_header "ncurses/term.h" "ncursesw/term.h"
do
AC_TRY_COMPILE([#include <stdio.h>
#include <${cf_cv_ncurses_header-curses.h}>
#include <$cf_test>
],[int x = auto_left_margin],[
	cf_cv_term_header="$cf_test"],[
	cf_cv_term_header=unknown
	])
	test "$cf_cv_term_header" != unknown && break
done
])

# Set definitions to allow ifdef'ing to accommodate subdirectories

case $cf_cv_term_header in # (vi
*term.h)
	AC_DEFINE(HAVE_TERM_H)
	;;
esac

case $cf_cv_term_header in # (vi
ncurses/term.h) #(vi
	AC_DEFINE(HAVE_NCURSES_TERM_H)
	;;
ncursesw/term.h)
	AC_DEFINE(HAVE_NCURSESW_TERM_H)
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_TRANSFORM_PROGRAM version: 2 updated: 2010/01/27 20:20:39
dnl --------------------
dnl From AC_ARG_PROGRAM, (re)compute $program_transform_name based on the
dnl $program_prefix and $program_suffix values.
AC_DEFUN([CF_TRANSFORM_PROGRAM],[
program_transform_name=s,x,x,
test "$program_prefix" != NONE &&
  program_transform_name="s,^,$program_prefix,;$program_transform_name"
# Use a double $ so make ignores it.
test "$program_suffix" != NONE &&
  program_transform_name="s,\$,$program_suffix,;$program_transform_name"
# Double any \ or $.  echo might interpret backslashes.
# By default was `s,x,x', remove it if useless.
cat <<\_ACEOF >conftest.sed
s/[[\\$]]/&&/g;s/;s,x,x,$//
_ACEOF
program_transform_name=`echo $program_transform_name | sed -f conftest.sed`
rm conftest.sed
])
dnl ---------------------------------------------------------------------------
dnl CF_TRY_PKG_CONFIG version: 4 updated: 2010/06/14 17:42:30
dnl -----------------
dnl This is a simple wrapper to use for pkg-config, for libraries which may be
dnl available in that form.
dnl
dnl $1 = package name
dnl $2 = extra logic to use, if any, after updating CFLAGS and LIBS
dnl $3 = logic to use if pkg-config does not have the package
AC_DEFUN([CF_TRY_PKG_CONFIG],[
AC_REQUIRE([CF_PKG_CONFIG])

if test "$PKG_CONFIG" != none && "$PKG_CONFIG" --exists $1; then
	CF_VERBOSE(found package $1)
	cf_pkgconfig_incs="`$PKG_CONFIG --cflags $1 2>/dev/null`"
	cf_pkgconfig_libs="`$PKG_CONFIG --libs   $1 2>/dev/null`"
	CF_VERBOSE(package $1 CFLAGS: $cf_pkgconfig_incs)
	CF_VERBOSE(package $1 LIBS: $cf_pkgconfig_libs)
	CF_ADD_CFLAGS($cf_pkgconfig_incs)
	CF_ADD_LIBS($cf_pkgconfig_libs)
	ifelse([$2],,:,[$2])
else
	ifelse([$3],,:,[$3])
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_TYPE_FD_SET version: 4 updated: 2008/03/25 20:56:03
dnl --------------
dnl Check for the declaration of fd_set.  Some platforms declare it in
dnl <sys/types.h>, and some in <sys/select.h>, which requires <sys/types.h>.
dnl Finally, if we are using this for an X application, Xpoll.h may include
dnl <sys/select.h>, so we don't want to do it twice.
AC_DEFUN([CF_TYPE_FD_SET],
[
AC_CHECK_HEADERS(X11/Xpoll.h)

AC_CACHE_CHECK(for declaration of fd_set,cf_cv_type_fd_set,
	[CF_MSG_LOG(sys/types alone)
AC_TRY_COMPILE([
#include <sys/types.h>],
	[fd_set x],
	[cf_cv_type_fd_set=sys/types.h],
	[CF_MSG_LOG(X11/Xpoll.h)
AC_TRY_COMPILE([
#ifdef HAVE_X11_XPOLL_H
#include <X11/Xpoll.h>
#endif],
	[fd_set x],
	[cf_cv_type_fd_set=X11/Xpoll.h],
	[CF_MSG_LOG(sys/select.h)
AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/select.h>],
	[fd_set x],
	[cf_cv_type_fd_set=sys/select.h],
	[cf_cv_type_fd_set=unknown])])])])
if test $cf_cv_type_fd_set = sys/select.h ; then
	AC_DEFINE(USE_SYS_SELECT_H)
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_TYPE_OUTCHAR version: 13 updated: 2002/10/09 20:00:37
dnl ---------------
dnl Check for return and param type of 3rd -- OutChar() -- param of tputs().
dnl
dnl For this check, and for CF_CURSES_TERMCAP, the $CHECK_DECL_HDRS variable
dnl must point to a header file containing this (or equivalent):
dnl
dnl	#ifdef NEED_CURSES_H
dnl	# ifdef HAVE_NCURSES_NCURSES_H
dnl	#  include <ncurses/ncurses.h>
dnl	# else
dnl	#  ifdef HAVE_NCURSES_H
dnl	#   include <ncurses.h>
dnl	#  else
dnl	#   include <curses.h>
dnl	#  endif
dnl	# endif
dnl	#endif
dnl
dnl	#ifdef HAVE_NCURSES_TERM_H
dnl	#  include <ncurses/term.h>
dnl	#else
dnl	# ifdef HAVE_TERM_H
dnl	#  include <term.h>
dnl	# endif
dnl	#endif
dnl
dnl	#if NEED_TERMCAP_H
dnl	# include <termcap.h>
dnl	#endif
dnl
AC_DEFUN([CF_TYPE_OUTCHAR],
[
AC_REQUIRE([CF_CURSES_TERMCAP])

AC_CACHE_CHECK(declaration of tputs 3rd param, cf_cv_type_outchar,[

cf_cv_type_outchar="int OutChar(int)"
cf_cv_found=no
cf_save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $CHECK_DECL_FLAG"

for P in int void; do
for Q in int void; do
for R in int char; do
for S in "" const; do
	CF_MSG_LOG(loop variables [P:[$]P, Q:[$]Q, R:[$]R, S:[$]S])
	AC_TRY_COMPILE([$CHECK_DECL_HDRS],
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

case $cf_cv_type_outchar in
int*)
	AC_DEFINE(OUTC_RETURN)
	;;
esac
case $cf_cv_type_outchar in
*char*)
	AC_DEFINE(OUTC_ARGS,char c)
	;;
esac

CPPFLAGS="$cf_save_CPPFLAGS"
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_UPPER version: 5 updated: 2001/01/29 23:40:59
dnl --------
dnl Make an uppercase version of a variable
dnl $1=uppercase($2)
AC_DEFUN([CF_UPPER],
[
$1=`echo "$2" | sed y%abcdefghijklmnopqrstuvwxyz./-%ABCDEFGHIJKLMNOPQRSTUVWXYZ___%`
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_UTF8_LIB version: 7 updated: 2010/06/20 09:24:28
dnl -----------
dnl Check for multibyte support, and if not found, utf8 compatibility library
AC_DEFUN([CF_UTF8_LIB],
[
AC_CACHE_CHECK(for multibyte character support,cf_cv_utf8_lib,[
	cf_save_LIBS="$LIBS"
	AC_TRY_LINK([
#include <stdlib.h>],[putwc(0,0);],
	[cf_cv_utf8_lib=yes],
	[CF_FIND_LINKAGE([
#include <libutf8.h>],[putwc(0,0);],utf8,
		[cf_cv_utf8_lib=add-on],
		[cf_cv_utf8_lib=no])
])])

# HAVE_LIBUTF8_H is used by ncurses if curses.h is shared between
# ncurses/ncursesw:
if test "$cf_cv_utf8_lib" = "add-on" ; then
	AC_DEFINE(HAVE_LIBUTF8_H)
	CF_ADD_INCDIR($cf_cv_header_path_utf8)
	CF_ADD_LIBDIR($cf_cv_library_path_utf8)
	CF_ADD_LIBS($cf_cv_library_file_utf8)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_VA_COPY version: 2 updated: 2010/03/04 05:37:29
dnl ----------
dnl check for va_copy, part of stdarg.h
dnl Also, workaround for glibc's __va_copy, by checking for both.
AC_DEFUN([CF_VA_COPY],[
AC_CACHE_CHECK(for va_copy, cf_cv_have_va_copy,[
AC_TRY_LINK([
#include <stdarg.h>
],[
	static va_list dst;
	static va_list src;
	va_copy(dst, src)],
	cf_cv_have_va_copy=yes,
	cf_cv_have_va_copy=no)])

test "$cf_cv_have_va_copy" = yes && AC_DEFINE(HAVE_VA_COPY)

AC_CACHE_CHECK(for __va_copy, cf_cv_have___va_copy,[
AC_TRY_LINK([
#include <stdarg.h>
],[
	static va_list dst;
	static va_list src;
	__va_copy(dst, src)],
	cf_cv_have___va_copy=yes,
	cf_cv_have___va_copy=no)])

test "$cf_cv_have___va_copy" = yes && AC_DEFINE(HAVE___VA_COPY)
])
dnl ---------------------------------------------------------------------------
dnl CF_VERBOSE version: 3 updated: 2007/07/29 09:55:12
dnl ----------
dnl Use AC_VERBOSE w/o the warnings
AC_DEFUN([CF_VERBOSE],
[test -n "$verbose" && echo "	$1" 1>&AC_FD_MSG
CF_MSG_LOG([$1])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WCTYPE version: 5 updated: 2003/05/05 00:42:17
dnl ---------
dnl Look for <wctype.h> and related functions.  This is needed with glibc to
dnl see the codes above 127.
AC_DEFUN([CF_WCTYPE],
[
AC_CACHE_CHECK(for <wctype.h> and functions, cf_cv_have_wctype,[
AC_TRY_COMPILE([
#include <wctype.h>],[
	wint_t temp = 101;
	int test = (wctype("alnum") != 0)
       		|| iswalnum(temp)
		|| iswalpha(temp)
		|| iswblank(temp)
		|| iswcntrl(temp)
		|| iswdigit(temp)
		|| iswgraph(temp)
		|| iswlower(temp)
		|| iswprint(temp)
		|| iswpunct(temp)
		|| iswspace(temp)
		|| iswupper(temp)
		|| iswxdigit(temp);],
	[cf_cv_have_wctype=yes],
	[cf_cv_have_wctype=no])
])
if test "$cf_cv_have_wctype" = yes ; then
	AC_SEARCH_LIBS(wctype,[w],[AC_DEFINE(HAVE_WCTYPE)])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_CURSES_DIR version: 2 updated: 2002/11/10 14:46:59
dnl ------------------
dnl Wrapper for AC_ARG_WITH to specify directory under which to look for curses
dnl libraries.
AC_DEFUN([CF_WITH_CURSES_DIR],[
AC_ARG_WITH(curses-dir,
	[  --with-curses-dir=DIR   directory in which (n)curses is installed],
	[CF_PATH_SYNTAX(withval)
	 cf_cv_curses_dir=$withval],
	[cf_cv_curses_dir=no])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_DBMALLOC version: 7 updated: 2010/06/21 17:26:47
dnl ----------------
dnl Configure-option for dbmalloc.  The optional parameter is used to override
dnl the updating of $LIBS, e.g., to avoid conflict with subsequent tests.
AC_DEFUN([CF_WITH_DBMALLOC],[
CF_NO_LEAKS_OPTION(dbmalloc,
	[  --with-dbmalloc         test: use Conor Cahill's dbmalloc library],
	[USE_DBMALLOC])

if test "$with_dbmalloc" = yes ; then
	AC_CHECK_HEADER(dbmalloc.h,
		[AC_CHECK_LIB(dbmalloc,[debug_malloc]ifelse([$1],,[],[,$1]))])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_DMALLOC version: 7 updated: 2010/06/21 17:26:47
dnl ---------------
dnl Configure-option for dmalloc.  The optional parameter is used to override
dnl the updating of $LIBS, e.g., to avoid conflict with subsequent tests.
AC_DEFUN([CF_WITH_DMALLOC],[
CF_NO_LEAKS_OPTION(dmalloc,
	[  --with-dmalloc          test: use Gray Watson's dmalloc library],
	[USE_DMALLOC])

if test "$with_dmalloc" = yes ; then
	AC_CHECK_HEADER(dmalloc.h,
		[AC_CHECK_LIB(dmalloc,[dmalloc_debug]ifelse([$1],,[],[,$1]))])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_IMAKE_CFLAGS version: 9 updated: 2010/05/26 05:38:42
dnl --------------------
dnl xterm and similar programs build more readily when propped up with imake's
dnl hand-tuned definitions.  If we do not use imake, provide fallbacks for the
dnl most common definitions that we're not likely to do by autoconf tests.
AC_DEFUN([CF_WITH_IMAKE_CFLAGS],[
AC_REQUIRE([CF_ENABLE_NARROWPROTO])

AC_MSG_CHECKING(if we should use imake to help)
CF_ARG_DISABLE(imake,
	[  --disable-imake         disable use of imake for definitions],
	[enable_imake=no],
	[enable_imake=yes])
AC_MSG_RESULT($enable_imake)

if test "$enable_imake" = yes ; then
	CF_IMAKE_CFLAGS(ifelse([$1],,,[$1]))
fi

if test -n "$IMAKE" && test -n "$IMAKE_CFLAGS" ; then
	CF_ADD_CFLAGS($IMAKE_CFLAGS)
else
	IMAKE_CFLAGS=
	IMAKE_LOADFLAGS=
	CF_VERBOSE(make fallback definitions)

	# We prefer config.guess' values when we can get them, to avoid
	# inconsistent results with uname (AIX for instance).  However,
	# config.guess is not always consistent either.
	case $host_os in
	*[[0-9]].[[0-9]]*)
		UNAME_RELEASE="$host_os"
		;;
	*)
		UNAME_RELEASE=`(uname -r) 2>/dev/null` || UNAME_RELEASE=unknown
		;;
	esac

	case .$UNAME_RELEASE in
	*[[0-9]].[[0-9]]*)
		OSMAJORVERSION=`echo "$UNAME_RELEASE" |sed -e 's/^[[^0-9]]*//' -e 's/\..*//'`
		OSMINORVERSION=`echo "$UNAME_RELEASE" |sed -e 's/^[[^0-9]]*//' -e 's/^[[^.]]*\.//' -e 's/\..*//' -e 's/[[^0-9]].*//' `
		test -z "$OSMAJORVERSION" && OSMAJORVERSION=1
		test -z "$OSMINORVERSION" && OSMINORVERSION=0
		IMAKE_CFLAGS="-DOSMAJORVERSION=$OSMAJORVERSION -DOSMINORVERSION=$OSMINORVERSION $IMAKE_CFLAGS"
		;;
	esac

	# FUNCPROTO is standard with X11R6, but XFree86 drops it, leaving some
	# fallback/fragments for NeedPrototypes, etc.
	IMAKE_CFLAGS="-DFUNCPROTO=15 $IMAKE_CFLAGS"

	# If this is not set properly, Xaw's scrollbars will not work
	if test "$enable_narrowproto" = yes ; then
		IMAKE_CFLAGS="-DNARROWPROTO=1 $IMAKE_CFLAGS"
	fi

	# Other special definitions:
	case $host_os in
	aix*)
		# imake on AIX 5.1 defines AIXV3.  really.
		IMAKE_CFLAGS="-DAIXV3 -DAIXV4 $IMAKE_CFLAGS"
		;;
	irix[[56]].*) #(vi
		# these are needed to make SIGWINCH work in xterm
		IMAKE_CFLAGS="-DSYSV -DSVR4 $IMAKE_CFLAGS"
		;;
	esac

	CF_ADD_CFLAGS($IMAKE_CFLAGS)

	AC_SUBST(IMAKE_CFLAGS)
	AC_SUBST(IMAKE_LOADFLAGS)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_NO_LEAKS version: 1 updated: 2006/12/14 18:00:21
dnl ----------------
AC_DEFUN([CF_WITH_NO_LEAKS],[

AC_REQUIRE([CF_WITH_DMALLOC])
AC_REQUIRE([CF_WITH_DBMALLOC])
AC_REQUIRE([CF_WITH_PURIFY])
AC_REQUIRE([CF_WITH_VALGRIND])

AC_MSG_CHECKING(if you want to perform memory-leak testing)
AC_ARG_WITH(no-leaks,
	[  --with-no-leaks         test: free permanent memory, analyze leaks],
	[AC_DEFINE(NO_LEAKS)
	 cf_doalloc=".${with_dmalloc}${with_dbmalloc}${with_purify}${with_valgrind}"
	 case ${cf_doalloc} in #(vi
	 *yes*) ;;
	 *) AC_DEFINE(DOALLOC,10000) ;;
	 esac
	 with_no_leaks=yes],
	[with_no_leaks=])
AC_MSG_RESULT($with_no_leaks)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_PATH version: 9 updated: 2010/05/26 05:38:42
dnl ------------
dnl Wrapper for AC_ARG_WITH to ensure that user supplies a pathname, not just
dnl defaulting to yes/no.
dnl
dnl $1 = option name
dnl $2 = help-text
dnl $3 = environment variable to set
dnl $4 = default value, shown in the help-message, must be a constant
dnl $5 = default value, if it's an expression & cannot be in the help-message
dnl
AC_DEFUN([CF_WITH_PATH],
[AC_ARG_WITH($1,[$2 ](default: ifelse([$4],,empty,[$4])),,
ifelse([$4],,[withval="${$3}"],[withval="${$3-ifelse([$5],,[$4],[$5])}"]))dnl
if ifelse([$5],,true,[test -n "$5"]) ; then
CF_PATH_SYNTAX(withval)
fi
$3="$withval"
AC_SUBST($3)dnl
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_PATHLIST version: 6 updated: 2009/01/11 20:30:23
dnl ----------------
dnl Process an option specifying a list of colon-separated paths.
dnl
dnl $1 = option name
dnl $2 = help-text
dnl $3 = environment variable to set
dnl $4 = default value, shown in the help-message, must be a constant
dnl $5 = default value, if it's an expression & cannot be in the help-message
dnl $6 = flag to tell if we want to define or substitute
dnl
AC_DEFUN([CF_WITH_PATHLIST],[
AC_REQUIRE([CF_PATHSEP])
AC_ARG_WITH($1,[$2 ](default: ifelse($4,,empty,$4)),,
ifelse($4,,[withval=${$3}],[withval=${$3-ifelse($5,,$4,$5)}]))dnl

IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${PATH_SEPARATOR}"
cf_dst_path=
for cf_src_path in $withval
do
  CF_PATH_SYNTAX(cf_src_path)
  test -n "$cf_dst_path" && cf_dst_path="${cf_dst_path}:"
  cf_dst_path="${cf_dst_path}${cf_src_path}"
done
IFS="$ac_save_ifs"

ifelse($6,define,[
# Strip single quotes from the value, e.g., when it was supplied as a literal
# for $4 or $5.
case $cf_dst_path in #(vi
\'*)
  cf_dst_path=`echo $cf_dst_path |sed -e s/\'// -e s/\'\$//`
  ;;
esac
cf_dst_path=`echo "$cf_dst_path" | sed -e 's/\\\\/\\\\\\\\/g'`
])

eval '$3="$cf_dst_path"'
AC_SUBST($3)dnl

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_PERL version: 5 updated: 2010/08/13 04:43:13
dnl ------------
dnl Check if perl-extension (using embedded perl interpreter) is wanted, and
dnl update symbols if we are able to use the extension.
AC_DEFUN([CF_WITH_PERL],[
AC_MSG_CHECKING(if you want to use perl as an extension language)
AC_ARG_WITH(perl,
	[  --with-perl             enable use of Perl as an extension language],
	[with_perl="$withval"],
	[with_perl=no])
AC_MSG_RESULT($with_perl)

PERLLIB=

if test "$with_perl" = yes ; then
	CF_PROG_PERL(5.004)
	if test "$PERL" = no; then
		AC_WARN([perl not found])
	else
		cf_perl_CFLAGS="$CFLAGS"
		cf_perl_CPPFLAGS="$CPPFLAGS"
		cf_perl_link="$ac_link"
		cf_perl_LIBS="$LIBS"

		cf_perl_prefix=`$PERL -MConfig -e 'print $Config{shrpenv}'`
		cf_perl_ccopts=`$PERL -MExtUtils::Embed -e ccopts`
		cf_perl_ldopts=`$PERL -MExtUtils::Embed -e ldopts`

		ac_link="$cf_perl_prefix $ac_link"
		CF_CHECK_CFLAGS($cf_perl_ccopts)
		LIBS="$LIBS $cf_perl_ldopts"

		AC_TRY_LINK([#define main perl_main
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#undef main],[
		PerlInterpreter* interp = perl_alloc();
		perl_construct(interp);
		perl_parse(interp, 0, 0, (char **)0, (char **)0);
		Perl_croak("Why:%s\n", "Bye!");
],[
		PERLLIB=`$PERL -MConfig -e 'print $Config{privlib}'`

		CF_VERBOSE(adding perl library $PERLLIB)

		AC_DEFINE(OPT_PERL)

		EXTRAOBJS="$EXTRAOBJS perl\$o"
		BUILTSRCS="$BUILTSRCS perl.c"

		EXTRA_INSTALL_DIRS="$EXTRA_INSTALL_DIRS \$(INSTALL_PERL_DIRS)"
		EXTRA_INSTALL_FILES="$EXTRA_INSTALL_FILES \$(INSTALL_PERL_FILES)"
],[
		AC_MSG_WARN(Cannot link with Perl interpreter)

		CF_VERBOSE([undoing changes to CFLAGS, etc])

		CFLAGS="$cf_perl_CFLAGS"
		CPPFLAGS="$cf_perl_CPPFLAGS"
		ac_link="$cf_perl_link"
		LIBS="$cf_perl_LIBS"
])
	fi
fi
AC_SUBST(PERLLIB)
AC_SUBST(PERL)
AC_SUBST(EXTRA_INSTALL_DIRS)
AC_SUBST(EXTRA_INSTALL_FILES)
])
dnl ---------------------------------------------------------------------------
dnl CF_WITH_PURIFY version: 2 updated: 2006/12/14 18:43:43
dnl --------------
AC_DEFUN([CF_WITH_PURIFY],[
CF_NO_LEAKS_OPTION(purify,
	[  --with-purify           test: use Purify],
	[USE_PURIFY],
	[LINK_PREFIX="$LINK_PREFIX purify"])
AC_SUBST(LINK_PREFIX)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_SYMLINK version: 4 updated: 2010/01/26 19:22:22
dnl ---------------
dnl If any of --program-prefix, --program-suffix or --program-transform-name is
dnl given, accept an option tell the makefile to create a symbolic link, e.g.,
dnl to the given program on install.
dnl
dnl $1 = variable to substitute
dnl $2 = program name
dnl $3 = option default
AC_DEFUN([CF_WITH_SYMLINK],[
$1=NONE
AC_SUBST($1)
if test "$program_transform_name" != "'CF__INIT_TRANSFORM'" ; then
cf_name=`echo "$program_transform_name" | sed -e '[s,\\$\\$,$,g]'`
cf_name=`echo $2 |sed -e "$cf_name"`
AC_MSG_CHECKING(for symbolic link to create to $cf_name)
AC_ARG_WITH(symlink,
	[  --with-symlink=XXX      make symbolic link to installed application],
	[with_symlink=$withval],
	[with_symlink=$3])
AC_MSG_RESULT($with_symlink)
test "$with_symlink" = yes && with_symlink=$2
test -n "$with_symlink" && \
	test "$with_symlink" != no && \
	test "$with_symlink" != $cf_name && \
	$1="$with_symlink"
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_WITH_VALGRIND version: 1 updated: 2006/12/14 18:00:21
dnl ----------------
AC_DEFUN([CF_WITH_VALGRIND],[
CF_NO_LEAKS_OPTION(valgrind,
	[  --with-valgrind         test: use valgrind],
	[USE_VALGRIND])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_WARNINGS version: 5 updated: 2004/07/23 14:40:34
dnl ----------------
dnl Combine the checks for gcc features into a configure-script option
dnl
dnl Parameters:
dnl	$1 - see CF_GCC_WARNINGS
AC_DEFUN([CF_WITH_WARNINGS],
[
if ( test "$GCC" = yes || test "$GXX" = yes )
then
AC_MSG_CHECKING(if you want to check for gcc warnings)
AC_ARG_WITH(warnings,
	[  --with-warnings         test: turn on gcc warnings],
	[cf_opt_with_warnings=$withval],
	[cf_opt_with_warnings=no])
AC_MSG_RESULT($cf_opt_with_warnings)
if test "$cf_opt_with_warnings" != no ; then
	CF_GCC_ATTRIBUTES
	CF_GCC_WARNINGS([$1])
fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_X_APP_DEFAULTS version: 2 updated: 2009/10/14 20:08:12
dnl ----------------------
dnl Option for specifying the location of the X app-defaults directory.
dnl
dnl The entire feature can be suppressed by --without-app-defaults, 
dnl which makes $no_appsdir become a comment-marker.
AC_DEFUN([CF_WITH_X_APP_DEFAULTS],
[
	AC_MSG_CHECKING(for directory to install resource files)
	CF_WITH_PATH(app-defaults,
		[  --with-app-defaults=DIR directory in which to install resource files],
		[appsdir],[EPREFIX/lib/X11/app-defaults],
		[\$(exec_prefix)/lib/X11/app-defaults])
	AC_MSG_RESULT($appsdir)
	AC_SUBST(appsdir)

	no_appsdir=
	if test "$appsdir" = no ; then
		no_appsdir="#"
	else
		EXTRA_INSTALL_DIRS="$EXTRA_INSTALL_DIRS \$(APPSDIR)"
	fi
	AC_SUBST(no_appsdir)
])
dnl ---------------------------------------------------------------------------
dnl CF_WITH_X_DESKTOP_UTILS version: 2 updated: 2010/01/25 20:34:54
dnl -----------------------
dnl Option for specifying desktop-utils program and flags.
AC_DEFUN([CF_WITH_X_DESKTOP_UTILS],
[
	# Comment-out the install-desktop rule if the desktop-utils are not found.
	AC_MSG_CHECKING(if you want to install desktop files)
	CF_ARG_OPTION(desktop,
		[  --disable-desktop       disable install of X desktop files],
		[enable_desktop=$enableval],
		[enable_desktop=$enableval],yes)
	AC_MSG_RESULT($enable_desktop)

	desktop_utils=
	if test "$enable_desktop" = yes ; then
	AC_CHECK_PROG(desktop_utils,desktop-file-install,yes,no)
	fi

	test "$desktop_utils" = yes && desktop_utils= || desktop_utils="#"
	AC_SUBST(DESKTOP_FLAGS)
])
dnl ---------------------------------------------------------------------------
dnl CF_WITH_X_ICONDIR version: 2 updated: 2009/10/14 20:08:12
dnl -----------------
dnl Option for specifying the location of the X icon directory.
dnl
dnl The entire feature can be suppressed by --without-icondir 
dnl which makes $no_icondir become a comment-marker.
AC_DEFUN([CF_WITH_X_ICONDIR],
[
	AC_MSG_CHECKING(for directory to install icons)
	CF_WITH_PATH(icondir,
		[  --with-icondir=DIR      directory in which to install icons],
		[icondir],[EPREFIX/share/pixmaps],
		[\$(exec_prefix)/share/pixmaps])
	AC_MSG_RESULT($icondir)
	AC_SUBST(icondir)

	no_icondir=
	if test "$icondir" = no ; then
		no_icondir="#"
	else
		EXTRA_INSTALL_DIRS="$EXTRA_INSTALL_DIRS \$(ICONDIR)"
	fi
	AC_SUBST(no_icondir)
])
dnl ---------------------------------------------------------------------------
dnl CF_XOPEN_CURSES version: 9 updated: 2010/04/28 06:02:16
dnl ---------------
dnl Test if we should define X/Open source for curses, needed on Digital Unix
dnl 4.x, to see the extended functions, but breaks on IRIX 6.x.
dnl
dnl The getbegyx() check is needed for HPUX, which omits legacy macros such
dnl as getbegy().  The latter is better design, but the former is standard.
AC_DEFUN([CF_XOPEN_CURSES],
[
AC_REQUIRE([CF_CURSES_CPPFLAGS])dnl
AC_CACHE_CHECK(if we must define _XOPEN_SOURCE_EXTENDED,cf_cv_need_xopen_extension,[
AC_TRY_LINK([
#include <stdlib.h>
#include <${cf_cv_ncurses_header-curses.h}>],[
#if defined(NCURSES_VERSION_PATCH)
if (NCURSES_VERSION_PATCH < 20100501) && (NCURSES_VERSION_PATCH >= 20100403)
	make an error
#endif
#endif
	long x = winnstr(stdscr, "", 0);
	int x1, y1;
	getbegyx(stdscr, y1, x1)],
	[cf_cv_need_xopen_extension=no],
	[AC_TRY_LINK([
#define _XOPEN_SOURCE_EXTENDED
#include <stdlib.h>
#include <${cf_cv_ncurses_header-curses.h}>],[
#ifdef NCURSES_VERSION
	cchar_t check;
	int check2 = curs_set((int)sizeof(check));
#endif
	long x = winnstr(stdscr, "", 0);
	int x1, y1;
	getbegyx(stdscr, y1, x1)],
	[cf_cv_need_xopen_extension=yes],
	[cf_cv_need_xopen_extension=unknown])])])
test $cf_cv_need_xopen_extension = yes && CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE_EXTENDED"
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_XOPEN_SOURCE version: 34 updated: 2010/05/26 05:38:42
dnl ---------------
dnl Try to get _XOPEN_SOURCE defined properly that we can use POSIX functions,
dnl or adapt to the vendor's definitions to get equivalent functionality,
dnl without losing the common non-POSIX features.
dnl
dnl Parameters:
dnl	$1 is the nominal value for _XOPEN_SOURCE
dnl	$2 is the nominal value for _POSIX_C_SOURCE
AC_DEFUN([CF_XOPEN_SOURCE],[

cf_XOPEN_SOURCE=ifelse([$1],,500,[$1])
cf_POSIX_C_SOURCE=ifelse([$2],,199506L,[$2])
cf_xopen_source=

case $host_os in #(vi
aix[[456]]*) #(vi
	cf_xopen_source="-D_ALL_SOURCE"
	;;
darwin[[0-8]].*) #(vi
	cf_xopen_source="-D_APPLE_C_SOURCE"
	;;
darwin*) #(vi
	cf_xopen_source="-D_DARWIN_C_SOURCE"
	;;
freebsd*|dragonfly*) #(vi
	# 5.x headers associate
	#	_XOPEN_SOURCE=600 with _POSIX_C_SOURCE=200112L
	#	_XOPEN_SOURCE=500 with _POSIX_C_SOURCE=199506L
	cf_POSIX_C_SOURCE=200112L
	cf_XOPEN_SOURCE=600
	cf_xopen_source="-D_BSD_TYPES -D__BSD_VISIBLE -D_POSIX_C_SOURCE=$cf_POSIX_C_SOURCE -D_XOPEN_SOURCE=$cf_XOPEN_SOURCE"
	;;
hpux11*) #(vi
	cf_xopen_source="-D_HPUX_SOURCE -D_XOPEN_SOURCE=500"
	;;
hpux*) #(vi
	cf_xopen_source="-D_HPUX_SOURCE"
	;;
irix[[56]].*) #(vi
	cf_xopen_source="-D_SGI_SOURCE"
	;;
linux*|gnu*|mint*|k*bsd*-gnu) #(vi
	CF_GNU_SOURCE
	;;
mirbsd*) #(vi
	# setting _XOPEN_SOURCE or _POSIX_SOURCE breaks <arpa/inet.h>
	;;
netbsd*) #(vi
	# setting _XOPEN_SOURCE breaks IPv6 for lynx on NetBSD 1.6, breaks xterm, is not needed for ncursesw
	;;
openbsd*) #(vi
	# setting _XOPEN_SOURCE breaks xterm on OpenBSD 2.8, is not needed for ncursesw
	;;
osf[[45]]*) #(vi
	cf_xopen_source="-D_OSF_SOURCE"
	;;
nto-qnx*) #(vi
	cf_xopen_source="-D_QNX_SOURCE"
	;;
sco*) #(vi
	# setting _XOPEN_SOURCE breaks Lynx on SCO Unix / OpenServer
	;;
solaris2.1[[0-9]]) #(vi
	cf_xopen_source="-D__EXTENSIONS__ -D_XOPEN_SOURCE=$cf_XOPEN_SOURCE"
	;;
solaris2.[[1-9]]) #(vi
	cf_xopen_source="-D__EXTENSIONS__"
	;;
*)
	AC_CACHE_CHECK(if we should define _XOPEN_SOURCE,cf_cv_xopen_source,[
	AC_TRY_COMPILE([#include <sys/types.h>],[
#ifndef _XOPEN_SOURCE
make an error
#endif],
	[cf_cv_xopen_source=no],
	[cf_save="$CPPFLAGS"
	 CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE=$cf_XOPEN_SOURCE"
	 AC_TRY_COMPILE([#include <sys/types.h>],[
#ifdef _XOPEN_SOURCE
make an error
#endif],
	[cf_cv_xopen_source=no],
	[cf_cv_xopen_source=$cf_XOPEN_SOURCE])
	CPPFLAGS="$cf_save"
	])
])
	if test "$cf_cv_xopen_source" != no ; then
		CF_REMOVE_DEFINE(CFLAGS,$CFLAGS,_XOPEN_SOURCE)
		CF_REMOVE_DEFINE(CPPFLAGS,$CPPFLAGS,_XOPEN_SOURCE)
		cf_temp_xopen_source="-D_XOPEN_SOURCE=$cf_cv_xopen_source"
		CF_ADD_CFLAGS($cf_temp_xopen_source)
	fi
	CF_POSIX_C_SOURCE($cf_POSIX_C_SOURCE)
	;;
esac

if test -n "$cf_xopen_source" ; then
	CF_ADD_CFLAGS($cf_xopen_source)
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_X_ATHENA version: 16 updated: 2010/06/14 17:42:30
dnl -----------
dnl Check for Xaw (Athena) libraries
dnl
dnl Sets $cf_x_athena according to the flavor of Xaw which is used.
AC_DEFUN([CF_X_ATHENA],
[
cf_x_athena=${cf_x_athena-Xaw}

AC_MSG_CHECKING(if you want to link with Xaw 3d library)
withval=
AC_ARG_WITH(Xaw3d,
	[  --with-Xaw3d            link with Xaw 3d library])
if test "$withval" = yes ; then
	cf_x_athena=Xaw3d
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING(if you want to link with neXT Athena library)
withval=
AC_ARG_WITH(neXtaw,
	[  --with-neXtaw           link with neXT Athena library])
if test "$withval" = yes ; then
	cf_x_athena=neXtaw
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING(if you want to link with Athena-Plus library)
withval=
AC_ARG_WITH(XawPlus,
	[  --with-XawPlus          link with Athena-Plus library])
if test "$withval" = yes ; then
	cf_x_athena=XawPlus
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

cf_x_athena_lib=""

if test "$PKG_CONFIG" != none ; then
	cf_athena_list=
	test "$cf_x_athena" = Xaw && cf_athena_list="xaw8 xaw7 xaw6"
	for cf_athena_pkg in \
		$cf_athena_list \
		${cf_x_athena} \
		${cf_x_athena}-devel \
		lib${cf_x_athena} \
		lib${cf_x_athena}-devel
	do
		CF_TRY_PKG_CONFIG($cf_athena_pkg,[
			cf_x_athena_lib="$cf_pkgconfig_libs"
			CF_UPPER(cf_x_athena_LIBS,HAVE_LIB_$cf_x_athena)
			AC_DEFINE_UNQUOTED($cf_x_athena_LIBS)
			break])
	done
fi

if test -z "$cf_x_athena_lib" ; then
	CF_X_EXT
	CF_X_TOOLKIT
	CF_X_ATHENA_CPPFLAGS($cf_x_athena)
	CF_X_ATHENA_LIBS($cf_x_athena)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_X_ATHENA_CPPFLAGS version: 5 updated: 2010/05/26 17:35:30
dnl --------------------
dnl Normally invoked by CF_X_ATHENA, with $1 set to the appropriate flavor of
dnl the Athena widgets, e.g., Xaw, Xaw3d, neXtaw.
AC_DEFUN([CF_X_ATHENA_CPPFLAGS],
[
cf_x_athena_root=ifelse([$1],,Xaw,[$1])
cf_x_athena_inc=""

for cf_path in default \
	/usr/contrib/X11R6 \
	/usr/contrib/X11R5 \
	/usr/lib/X11R5 \
	/usr/local
do
	if test -z "$cf_x_athena_inc" ; then
		cf_save="$CPPFLAGS"
		cf_test=X11/$cf_x_athena_root/SimpleMenu.h
		if test $cf_path != default ; then
			CPPFLAGS="$cf_save -I$cf_path/include"
			AC_MSG_CHECKING(for $cf_test in $cf_path)
		else
			AC_MSG_CHECKING(for $cf_test)
		fi
		AC_TRY_COMPILE([
#include <X11/Intrinsic.h>
#include <$cf_test>],[],
			[cf_result=yes],
			[cf_result=no])
		AC_MSG_RESULT($cf_result)
		if test "$cf_result" = yes ; then
			cf_x_athena_inc=$cf_path
			break
		else
			CPPFLAGS="$cf_save"
		fi
	fi
done

if test -z "$cf_x_athena_inc" ; then
	AC_MSG_WARN(
[Unable to successfully find Athena header files with test program])
elif test "$cf_x_athena_inc" != default ; then
	CPPFLAGS="$CPPFLAGS -I$cf_x_athena_inc"
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_X_ATHENA_LIBS version: 9 updated: 2010/06/02 05:03:05
dnl ----------------
dnl Normally invoked by CF_X_ATHENA, with $1 set to the appropriate flavor of
dnl the Athena widgets, e.g., Xaw, Xaw3d, neXtaw.
AC_DEFUN([CF_X_ATHENA_LIBS],
[AC_REQUIRE([CF_X_TOOLKIT])
cf_x_athena_root=ifelse([$1],,Xaw,[$1])
cf_x_athena_lib=""

for cf_path in default \
	/usr/contrib/X11R6 \
	/usr/contrib/X11R5 \
	/usr/lib/X11R5 \
	/usr/local
do
	for cf_lib in \
		"-l$cf_x_athena_root -lXmu" \
		"-l$cf_x_athena_root -lXpm -lXmu" \
		"-l${cf_x_athena_root}_s -lXmu_s"
	do
		if test -z "$cf_x_athena_lib" ; then
			cf_save="$LIBS"
			cf_test=XawSimpleMenuAddGlobalActions
			if test $cf_path != default ; then
				CF_ADD_LIBS(-L$cf_path/lib $cf_lib)
				AC_MSG_CHECKING(for $cf_lib in $cf_path)
			else
				CF_ADD_LIBS($cf_lib)
				AC_MSG_CHECKING(for $cf_test in $cf_lib)
			fi
			AC_TRY_LINK([],[$cf_test()],
				[cf_result=yes],
				[cf_result=no])
			AC_MSG_RESULT($cf_result)
			if test "$cf_result" = yes ; then
				cf_x_athena_lib="$cf_lib"
				break
			fi
			LIBS="$cf_save"
		fi
	done
done

if test -z "$cf_x_athena_lib" ; then
	AC_MSG_ERROR(
[Unable to successfully link Athena library (-l$cf_x_athena_root) with test program])
fi

CF_UPPER(cf_x_athena_LIBS,HAVE_LIB_$cf_x_athena)
AC_DEFINE_UNQUOTED($cf_x_athena_LIBS)
])
dnl ---------------------------------------------------------------------------
dnl CF_X_EXT version: 3 updated: 2010/06/02 05:03:05
dnl --------
AC_DEFUN([CF_X_EXT],[
CF_TRY_PKG_CONFIG(Xext,,[
	AC_CHECK_LIB(Xext,XextCreateExtension,
		[CF_ADD_LIB(Xext)])])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_X_MOTIF version: 3 updated: 2008/03/23 14:48:54
dnl ----------
dnl Check for Motif or Lesstif libraries (they should be indistinguishable)
AC_DEFUN([CF_X_MOTIF],
[AC_REQUIRE([CF_X_TOOLKIT])dnl
AC_CHECK_HEADERS(X11/IntrinsicI.h Xm/XmP.h)
AC_CHECK_LIB(gen,regcmp)
AC_CHECK_LIB(Xmu,XmuClientWindow)
AC_CHECK_LIB(Xp,XpStartDoc,,,[$LIBS $X_EXTRA_LIBS])
AC_CHECK_LIB(Xext,XextCreateExtension,
	[LIBS="-lXext $LIBS"])
AC_CHECK_LIB(Xpm, XpmCreatePixmapFromXpmImage,
	[LIBS="-lXpm $LIBS"],,
	[$LIBS $X_EXTRA_LIBS])
AC_CHECK_LIB(XIM,XmbTextListToTextProperty)dnl needed for Unixware's Xm
AC_CHECK_LIB(Xm, XmProcessTraversal, [LIBS="-lXm $LIBS"],
	AC_MSG_ERROR(
[Unable to successfully link Motif library (-lXm) with test program]),
	[$X_PRE_LIBS $LIBS $X_EXTRA_LIBS]) dnl
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_X_OPENLOOK version: 3 updated: 2008/03/23 14:48:54
dnl -------------
AC_DEFUN([CF_X_OPENLOOK],
[AC_REQUIRE([CF_X_TOOLKIT])dnl
if test -n "$OPENWINHOME"; then
	X_LIBS="$X_LIBS -L${OPENWINHOME}/lib"
	X_CFLAGS="$X_CFLAGS -I${OPENWINHOME}/include"
fi
LDFLAGS="$LDFLAGS $X_LIBS"
AC_CHECK_LIB(Xmu,XmuClientWindow)
AC_CHECK_LIB(Xol, OlToolkitInitialize,
	[LIBS="-lXol -lm $LIBS"],
	AC_MSG_ERROR(
[Unable to successfully link OpenLook library (-lXol) with test program])) dnl
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_X_TOOLKIT version: 15 updated: 2010/06/14 17:42:30
dnl ------------
dnl Check for X Toolkit libraries
dnl
AC_DEFUN([CF_X_TOOLKIT],
[
AC_REQUIRE([AC_PATH_XTRA])
AC_REQUIRE([CF_CHECK_CACHE])

cf_have_X_LIBS=no

CF_TRY_PKG_CONFIG(xt,[

	# workaround for broken ".pc" files used for X Toolkit.
	case "x$X_PRE_LIBS" in #(vi
	*-lICE*)
		case "x$LIBS" in #(vi
		*-lICE*) #(vi
			;;
		*)
			CF_VERBOSE(work around broken package)
			CF_VERBOSE(...before $LIBS)
			LIBS=`echo "$LIBS" | sed -e "s/[[ 	]][[ 	]]*/ /g" -e "s,-lXt ,-lXt $X_PRE_LIBS ," -e 's/  / /g'`
			CF_VERBOSE(...after  $LIBS)
			;;
		esac
		;;
	esac

	cf_have_X_LIBS=yes
],[

	LDFLAGS="$X_LIBS $LDFLAGS"
	CF_CHECK_CFLAGS($X_CFLAGS)

	AC_CHECK_FUNC(XOpenDisplay,,[
	AC_CHECK_LIB(X11,XOpenDisplay,
		[CF_ADD_LIB(X11)],,
		[$X_PRE_LIBS $LIBS $X_EXTRA_LIBS])])

	AC_CHECK_FUNC(XtAppInitialize,,[
	AC_CHECK_LIB(Xt, XtAppInitialize,
		[AC_DEFINE(HAVE_LIBXT)
		 cf_have_X_LIBS=Xt
		 LIBS="-lXt $X_PRE_LIBS $LIBS $X_EXTRA_LIBS"],,
		[$X_PRE_LIBS $LIBS $X_EXTRA_LIBS])])
])

if test $cf_have_X_LIBS = no ; then
	AC_MSG_WARN(
[Unable to successfully link X Toolkit library (-lXt) with
test program.  You will have to check and add the proper libraries by hand
to makefile.])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF__CURSES_HEAD version: 1 updated: 2009/07/16 19:32:31
dnl ---------------
dnl Define a reusable chunk which includes <curses.h> and <term.h> when they
dnl are both available.
define([CF__CURSES_HEAD],[
#ifdef HAVE_XCURSES
#include <xcurses.h>
char * XCursesProgramName = "test";
#else
#include <${cf_cv_ncurses_header-curses.h}>
#if defined(NCURSES_VERSION) && defined(HAVE_NCURSESW_TERM_H)
#include <ncursesw/term.h>
#elif defined(NCURSES_VERSION) && defined(HAVE_NCURSES_TERM_H)
#include <ncurses/term.h>
#elif defined(HAVE_TERM_H)
#include <term.h>
#endif
#endif
])
dnl ---------------------------------------------------------------------------
dnl CF__ICONV_BODY version: 2 updated: 2007/07/26 17:35:47
dnl --------------
dnl Test-code needed for iconv compile-checks
define([CF__ICONV_BODY],[
	iconv_t cd = iconv_open("","");
	iconv(cd,NULL,NULL,NULL,NULL);
	iconv_close(cd);]
)dnl
dnl ---------------------------------------------------------------------------
dnl CF__ICONV_HEAD version: 1 updated: 2007/07/26 15:57:03
dnl --------------
dnl Header-files needed for iconv compile-checks
define([CF__ICONV_HEAD],[
#include <stdlib.h>
#include <iconv.h>]
)dnl
dnl ---------------------------------------------------------------------------
dnl CF__INIT_TRANSFORM version: 1 updated: 2010/01/26 06:48:38
dnl ------------------
dnl The initial/inactive value for $program_transform_name is a dummy
dnl substitution.
define([CF__INIT_TRANSFORM],[s,x,x,])dnl
