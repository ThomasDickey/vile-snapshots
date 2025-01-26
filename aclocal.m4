dnl $Id: aclocal.m4,v 1.377 2025/01/26 10:06:47 tom Exp $
dnl ---------------------------------------------------------------------------
dnl
dnl Copyright 1996-2024,2025 by Thomas E. Dickey
dnl
dnl                         All Rights Reserved
dnl
dnl Permission is hereby granted, free of charge, to any person obtaining a
dnl copy of this software and associated documentation files (the
dnl "Software"), to deal in the Software without restriction, including
dnl without limitation the rights to use, copy, modify, merge, publish,
dnl distribute, sublicense, and/or sell copies of the Software, and to
dnl permit persons to whom the Software is furnished to do so, subject to
dnl the following conditions:
dnl
dnl The above copyright notice and this permission notice shall be included
dnl in all copies or substantial portions of the Software.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
dnl OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
dnl MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
dnl IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
dnl CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
dnl TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
dnl SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
dnl
dnl Except as contained in this notice, the name(s) of the above copyright
dnl holders shall not be used in advertising or otherwise to promote the
dnl sale, use or other dealings in this Software without prior written
dnl authorization.
dnl
dnl ---------------------------------------------------------------------------
dnl vile's local definitions for autoconf.
dnl
dnl See
dnl		https://invisible-island.net/autoconf/autoconf.html
dnl		https://invisible-island.net/autoconf/my-autoconf.html
dnl ---------------------------------------------------------------------------
dnl ---------------------------------------------------------------------------
dnl AM_ICONV version: 13 updated: 2024/10/16 03:56:13
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
      am_cv_proto_iconv_arg1="/* nothing */"
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
dnl AM_LANGINFO_CODESET version: 7 updated: 2023/01/11 04:05:23
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
	[AC_TRY_LINK([
$ac_includes_default
#include <langinfo.h>],
	[char* cs = nl_langinfo(CODESET); (void)cs],
	am_cv_langinfo_codeset=yes,
	am_cv_langinfo_codeset=no)
	])
	if test "$am_cv_langinfo_codeset" = yes; then
		AC_DEFINE(HAVE_LANGINFO_CODESET, 1,
		[Define if you have <langinfo.h> and nl_langinfo(CODESET).])
	fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ACVERSION_CHECK version: 5 updated: 2014/06/04 19:11:49
dnl ------------------
dnl Conditionally generate script according to whether we're using a given autoconf.
dnl
dnl $1 = version to compare against
dnl $2 = code to use if AC_ACVERSION is at least as high as $1.
dnl $3 = code to use if AC_ACVERSION is older than $1.
define([CF_ACVERSION_CHECK],
[
ifdef([AC_ACVERSION], ,[ifdef([AC_AUTOCONF_VERSION],[m4_copy([AC_AUTOCONF_VERSION],[AC_ACVERSION])],[m4_copy([m4_PACKAGE_VERSION],[AC_ACVERSION])])])dnl
ifdef([m4_version_compare],
[m4_if(m4_version_compare(m4_defn([AC_ACVERSION]), [$1]), -1, [$3], [$2])],
[CF_ACVERSION_COMPARE(
AC_PREREQ_CANON(AC_PREREQ_SPLIT([$1])),
AC_PREREQ_CANON(AC_PREREQ_SPLIT(AC_ACVERSION)), AC_ACVERSION, [$2], [$3])])])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ACVERSION_COMPARE version: 3 updated: 2012/10/03 18:39:53
dnl --------------------
dnl CF_ACVERSION_COMPARE(MAJOR1, MINOR1, TERNARY1,
dnl                      MAJOR2, MINOR2, TERNARY2,
dnl                      PRINTABLE2, not FOUND, FOUND)
define([CF_ACVERSION_COMPARE],
[ifelse(builtin([eval], [$2 < $5]), 1,
[ifelse([$8], , ,[$8])],
[ifelse([$9], , ,[$9])])])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_CFLAGS version: 15 updated: 2020/12/31 10:54:15
dnl -------------
dnl Copy non-preprocessor flags to $CFLAGS, preprocessor flags to $CPPFLAGS
dnl $1 = flags to add
dnl $2 = if given makes this macro verbose.
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
case "$cf_fix_cppflags" in
(no)
	case "$cf_add_cflags" in
	(-undef|-nostdinc*|-I*|-D*|-U*|-E|-P|-C)
		case "$cf_add_cflags" in
		(-D*)
			cf_tst_cflags=`echo "${cf_add_cflags}" |sed -e 's/^-D[[^=]]*='\''\"[[^"]]*//'`

			test "x${cf_add_cflags}" != "x${cf_tst_cflags}" \
				&& test -z "${cf_tst_cflags}" \
				&& cf_fix_cppflags=yes

			if test "$cf_fix_cppflags" = yes ; then
				CF_APPEND_TEXT(cf_new_extra_cppflags,$cf_add_cflags)
				continue
			elif test "${cf_tst_cflags}" = "\"'" ; then
				CF_APPEND_TEXT(cf_new_extra_cppflags,$cf_add_cflags)
				continue
			fi
			;;
		esac
		case "$CPPFLAGS" in
		(*$cf_add_cflags)
			;;
		(*)
			case "$cf_add_cflags" in
			(-D*)
				cf_tst_cppflags=`echo "x$cf_add_cflags" | sed -e 's/^...//' -e 's/=.*//'`
				CF_REMOVE_DEFINE(CPPFLAGS,$CPPFLAGS,$cf_tst_cppflags)
				;;
			esac
			CF_APPEND_TEXT(cf_new_cppflags,$cf_add_cflags)
			;;
		esac
		;;
	(*)
		CF_APPEND_TEXT(cf_new_cflags,$cf_add_cflags)
		;;
	esac
	;;
(yes)
	CF_APPEND_TEXT(cf_new_extra_cppflags,$cf_add_cflags)

	cf_tst_cflags=`echo "${cf_add_cflags}" |sed -e 's/^[[^"]]*"'\''//'`

	test "x${cf_add_cflags}" != "x${cf_tst_cflags}" \
		&& test -z "${cf_tst_cflags}" \
		&& cf_fix_cppflags=no
	;;
esac
done

if test -n "$cf_new_cflags" ; then
	ifelse([$2],,,[CF_VERBOSE(add to \$CFLAGS $cf_new_cflags)])
	CF_APPEND_TEXT(CFLAGS,$cf_new_cflags)
fi

if test -n "$cf_new_cppflags" ; then
	ifelse([$2],,,[CF_VERBOSE(add to \$CPPFLAGS $cf_new_cppflags)])
	CF_APPEND_TEXT(CPPFLAGS,$cf_new_cppflags)
fi

if test -n "$cf_new_extra_cppflags" ; then
	ifelse([$2],,,[CF_VERBOSE(add to \$EXTRA_CPPFLAGS $cf_new_extra_cppflags)])
	CF_APPEND_TEXT(EXTRA_CPPFLAGS,$cf_new_extra_cppflags)
fi

AC_SUBST(EXTRA_CPPFLAGS)

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_INCDIR version: 17 updated: 2021/09/04 06:35:04
dnl -------------
dnl Add an include-directory to $CPPFLAGS.  Don't add /usr/include, since it is
dnl redundant.  We don't normally need to add -I/usr/local/include for gcc,
dnl but old versions (and some misinstalled ones) need that.  To make things
dnl worse, gcc 3.x may give error messages if -I/usr/local/include is added to
dnl the include-path).
AC_DEFUN([CF_ADD_INCDIR],
[
if test -n "$1" ; then
  for cf_add_incdir in $1
  do
	while test "$cf_add_incdir" != /usr/include
	do
	  if test -d "$cf_add_incdir"
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
			  CF_APPEND_TEXT(CPPFLAGS,-I$cf_add_incdir)
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

		  cf_top_incdir=`echo "$cf_add_incdir" | sed -e 's%/include/.*$%/include%'`
		  test "$cf_top_incdir" = "$cf_add_incdir" && break
		  cf_add_incdir="$cf_top_incdir"
		else
		  break
		fi
	  else
		break
	  fi
	done
  done
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_LDFLAGS version: 1 updated: 2024/01/21 14:23:33
dnl --------------
dnl Copy non-library flags to $EXTRA_LDFLAGS, library flags to $LIBS
dnl $1 = flags to add
AC_DEFUN([CF_ADD_LDFLAGS],
[
AC_REQUIRE([CF_ADD_LIBS])
if test -n "$1"
then
	AC_MSG_CHECKING(for valid linker options in $1)
	cf_new_extra_ldflags=
	for cf_add_ldflags in $1
	do
		case "$cf_add_ldflags" in
		(-l*)
			CF_VERBOSE(adding $cf_add_ldflags)
			CF_ADD_LIBS($cf_add_ldflags)
			;;
		(*)
			cf_save_ldflags="$LDFLAGS"
			LDFLAGS="$EXTRA_LDFLAGS $cf_add_ldflags"
			AC_TRY_LINK(
				[#include <stdio.h>],
				[printf("Hello?\n")],
				[CF_VERBOSE(retain $cf_add_ldflags)
				 CF_APPEND_TEXT(cf_new_extra_ldflags,$cf_add_ldflag)],
				[CF_VERBOSE(ignore $cf_add_ldflags)])
			LDFLAGS="$cf_save_ldflags"
			;;
		esac
	done
	AC_MSG_RESULT($cf_new_extra_ldflags)

	if test -n "$cf_new_extra_ldflags" ; then
		CF_APPEND_TEXT(EXTRA_LDFLAGS,$cf_new_extra_ldflags)
	fi
fi

AC_SUBST(EXTRA_LDFLAGS)

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
dnl CF_ADD_LIBDIR version: 11 updated: 2020/12/31 20:19:42
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
		if test "$cf_add_libdir" = /usr/lib ; then
			:
		elif test -d "$cf_add_libdir"
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
dnl CF_ADD_LIBS version: 3 updated: 2019/11/02 16:47:33
dnl -----------
dnl Add one or more libraries, used to enforce consistency.  Libraries are
dnl prepended to an existing list, since their dependencies are assumed to
dnl already exist in the list.
dnl
dnl $1 = libraries to add, with the "-l", etc.
dnl $2 = variable to update (default $LIBS)
AC_DEFUN([CF_ADD_LIBS],[
cf_add_libs="[$]ifelse($2,,LIBS,[$2])"
# reverse order
cf_add_0lib=
for cf_add_1lib in $1; do cf_add_0lib="$cf_add_1lib $cf_add_0lib"; done
# filter duplicates
for cf_add_1lib in $cf_add_0lib; do
	for cf_add_2lib in $cf_add_libs; do
		if test "x$cf_add_1lib" = "x$cf_add_2lib"; then
			cf_add_1lib=
			break
		fi
	done
	test -n "$cf_add_1lib" && cf_add_libs="$cf_add_1lib $cf_add_libs"
done
ifelse($2,,LIBS,[$2])="$cf_add_libs"
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_LIB_AFTER version: 3 updated: 2013/07/09 21:27:22
dnl ----------------
dnl Add a given library after another, e.g., following the one it satisfies a
dnl dependency for.
dnl
dnl $1 = the first library
dnl $2 = its dependency
AC_DEFUN([CF_ADD_LIB_AFTER],[
CF_VERBOSE(...before $LIBS)
LIBS=`echo "$LIBS" | sed -e "s/[[ 	]][[ 	]]*/ /g" -e "s%$1 %$1 $2 %" -e 's%  % %g'`
CF_VERBOSE(...after  $LIBS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_OPTIONAL_PATH version: 6 updated: 2024/09/10 19:19:44
dnl --------------------
dnl Add an optional search-path to the compile/link variables.
dnl See CF_WITH_PATH
dnl
dnl $1 = shell variable containing the result of --with-XXX=[DIR]
dnl $2 = module to look for.
AC_DEFUN([CF_ADD_OPTIONAL_PATH],[
case "x$1" in
(xno|xyes|x)
	;;
(*)
	CF_ADD_SEARCHPATH([$1], [AC_MSG_ERROR(cannot find $2 under $1)])
	CF_VERBOSE(setting path value for ifelse([$2],,variable,[$2]) to $1)
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_SEARCHPATH version: 6 updated: 2020/12/31 20:19:42
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
	if test -d "$cf_searchpath/include" ; then
		CF_ADD_INCDIR($cf_searchpath/include)
	elif test -d "$cf_searchpath/../include" ; then
		CF_ADD_INCDIR($cf_searchpath/../include)
	ifelse([$2],,,[else
$2])
	fi
	if test -d "$cf_searchpath/lib" ; then
		CF_ADD_LIBDIR($cf_searchpath/lib)
	elif test -d "$cf_searchpath" ; then
		CF_ADD_LIBDIR($cf_searchpath)
	ifelse([$2],,,[else
$2])
	fi
done
])
dnl ---------------------------------------------------------------------------
dnl CF_ADD_SUBDIR_PATH version: 5 updated: 2020/12/31 20:19:42
dnl ------------------
dnl Append to a search-list for a nonstandard header/lib-file
dnl	$1 = the variable to return as result
dnl	$2 = the package name
dnl	$3 = the subdirectory, e.g., bin, include or lib
dnl $4 = the directory under which we will test for subdirectories
dnl $5 = a directory that we do not want $4 to match
AC_DEFUN([CF_ADD_SUBDIR_PATH],
[
test "x$4" != "x$5" && \
test -d "$4" && \
ifelse([$5],NONE,,[{ test -z "$5" || test "x$5" = xNONE || test "x$4" != "x$5"; } &&]) {
	test -n "$verbose" && echo "	... testing for $3-directories under $4"
	test -d "$4/$3" &&          $1="[$]$1 $4/$3"
	test -d "$4/$3/$2" &&       $1="[$]$1 $4/$3/$2"
	test -d "$4/$3/$2/$3" &&    $1="[$]$1 $4/$3/$2/$3"
	test -d "$4/$2/$3" &&       $1="[$]$1 $4/$2/$3"
	test -d "$4/$2/$3/$2" &&    $1="[$]$1 $4/$2/$3/$2"
}
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_APPEND_CFLAGS version: 3 updated: 2021/09/05 17:25:40
dnl ----------------
dnl Use CF_ADD_CFLAGS after first checking for potential redefinitions.
dnl $1 = flags to add
dnl $2 = if given makes this macro verbose.
define([CF_APPEND_CFLAGS],
[
for cf_add_cflags in $1
do
	case "x$cf_add_cflags" in
	(x-[[DU]]*)
		CF_REMOVE_CFLAGS($cf_add_cflags,CFLAGS,[$2])
		CF_REMOVE_CFLAGS($cf_add_cflags,CPPFLAGS,[$2])
		;;
	esac
	CF_ADD_CFLAGS([$cf_add_cflags],[$2])
done
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_APPEND_TEXT version: 1 updated: 2017/02/25 18:58:55
dnl --------------
dnl use this macro for appending text without introducing an extra blank at
dnl the beginning
define([CF_APPEND_TEXT],
[
	test -n "[$]$1" && $1="[$]$1 "
	$1="[$]{$1}$2"
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
dnl CF_ARG_OPTION version: 5 updated: 2015/05/10 19:52:14
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
dnl CF_AR_FLAGS version: 9 updated: 2021/01/01 13:31:04
dnl -----------
dnl Check for suitable "ar" (archiver) options for updating an archive.
dnl
dnl In particular, handle some obsolete cases where the "-" might be omitted,
dnl as well as a workaround for breakage of make's archive rules by the GNU
dnl binutils "ar" program.
AC_DEFUN([CF_AR_FLAGS],[
AC_REQUIRE([CF_PROG_AR])

AC_CACHE_CHECK(for options to update archives, cf_cv_ar_flags,[
	case "$cf_cv_system_name" in
	(*-msvc*)
		cf_cv_ar_flags=''
		cat >mk_static_lib.sh <<-EOF
		#!$SHELL
		MSVC_BIN="[$]AR"
		out="\[$]1"
		shift
		exec \[$]MSVC_BIN -out:"\[$]out" \[$]@
		EOF
		chmod +x mk_static_lib.sh
		AR=`pwd`/mk_static_lib.sh
		;;
	(*)
		cf_cv_ar_flags=unknown
		for cf_ar_flags in -curvU -curv curv -crv crv -cqv cqv -rv rv
		do

			# check if $ARFLAGS already contains this choice
			if test "x$ARFLAGS" != "x" ; then
				cf_check_ar_flags=`echo "x$ARFLAGS" | sed -e "s/$cf_ar_flags\$//" -e "s/$cf_ar_flags / /"`
				if test "x$ARFLAGS" != "$cf_check_ar_flags" ; then
					cf_cv_ar_flags=
					break
				fi
			fi

			rm -f "conftest.$ac_cv_objext"
			rm -f conftest.a

			cat >"conftest.$ac_ext" <<EOF
#line __oline__ "configure"
int	testdata[[3]] = { 123, 456, 789 };
EOF
			if AC_TRY_EVAL(ac_compile) ; then
				echo "$AR $ARFLAGS $cf_ar_flags conftest.a conftest.$ac_cv_objext" >&AC_FD_CC
				$AR $ARFLAGS "$cf_ar_flags" conftest.a "conftest.$ac_cv_objext" 2>&AC_FD_CC 1>/dev/null
				if test -f conftest.a ; then
					cf_cv_ar_flags="$cf_ar_flags"
					break
				fi
			else
				CF_VERBOSE(cannot compile test-program)
				break
			fi
		done
		rm -f conftest.a "conftest.$ac_ext" "conftest.$ac_cv_objext"
		;;
	esac
])

if test -n "$ARFLAGS" ; then
	if test -n "$cf_cv_ar_flags" ; then
		ARFLAGS="$ARFLAGS $cf_cv_ar_flags"
	fi
else
	ARFLAGS=$cf_cv_ar_flags
fi

AC_SUBST(ARFLAGS)
])
dnl ---------------------------------------------------------------------------
dnl CF_AUTO_SYMLINK version: 4 updated: 2020/12/31 20:19:42
dnl ---------------
dnl If CF_WITH_SYMLINK is set, but the program transformation is inactive,
dnl override it to provide a version-suffix to the result.  This assumes that
dnl a "version.sh" script is in the source directory.
dnl
dnl $1 = variable to substitute
AC_DEFUN([CF_AUTO_SYMLINK],[
AC_REQUIRE([CF_WITH_SYMLINK])dnl

if test "x$with_symlink" != xno ; then
	if test "[$]$1" = NONE ; then
		cf_version=`$SHELL "$srcdir/version.sh" 2>/dev/null`
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
dnl CF_BUILD_CC version: 14 updated: 2024/12/14 11:58:01
dnl -----------
dnl If we're cross-compiling, allow the user to override the tools and their
dnl options.  The configure script is oriented toward identifying the host
dnl compiler, etc., but we need a build compiler to generate parts of the
dnl source.
dnl
dnl $1 = default for $CPPFLAGS
dnl $2 = default for $LIBS
AC_DEFUN([CF_BUILD_CC],[
CF_ACVERSION_CHECK(2.52,,
	[AC_REQUIRE([CF_PROG_EXT])])
if test "$cross_compiling" = yes ; then

	# defaults that we might want to override
	: ${BUILD_CFLAGS:=''}
	: ${BUILD_CPPFLAGS:='ifelse([$1],,,[$1])'}
	: ${BUILD_LDFLAGS:=''}
	: ${BUILD_LIBS:='ifelse([$2],,,[$2])'}
	: ${BUILD_EXEEXT:='$x'}
	: ${BUILD_OBJEXT:='o'}

	AC_ARG_WITH(build-cc,
		[  --with-build-cc=XXX     the build C compiler ($BUILD_CC)],
		[BUILD_CC="$withval"],
		[AC_CHECK_PROGS(BUILD_CC, [gcc clang c99 c89 cc cl],none)])
	AC_MSG_CHECKING(for native build C compiler)
	AC_MSG_RESULT($BUILD_CC)

	AC_MSG_CHECKING(for native build C preprocessor)
	AC_ARG_WITH(build-cpp,
		[  --with-build-cpp=XXX    the build C preprocessor ($BUILD_CPP)],
		[BUILD_CPP="$withval"],
		[BUILD_CPP='${BUILD_CC} -E'])
	AC_MSG_RESULT($BUILD_CPP)

	AC_MSG_CHECKING(for native build C flags)
	AC_ARG_WITH(build-cflags,
		[  --with-build-cflags=XXX the build C compiler-flags ($BUILD_CFLAGS)],
		[BUILD_CFLAGS="$withval"])
	AC_MSG_RESULT($BUILD_CFLAGS)

	AC_MSG_CHECKING(for native build C preprocessor-flags)
	AC_ARG_WITH(build-cppflags,
		[  --with-build-cppflags=XXX the build C preprocessor-flags ($BUILD_CPPFLAGS)],
		[BUILD_CPPFLAGS="$withval"])
	AC_MSG_RESULT($BUILD_CPPFLAGS)

	AC_MSG_CHECKING(for native build linker-flags)
	AC_ARG_WITH(build-ldflags,
		[  --with-build-ldflags=XXX the build linker-flags ($BUILD_LDFLAGS)],
		[BUILD_LDFLAGS="$withval"])
	AC_MSG_RESULT($BUILD_LDFLAGS)

	AC_MSG_CHECKING(for native build linker-libraries)
	AC_ARG_WITH(build-libs,
		[  --with-build-libs=XXX   the build libraries (${BUILD_LIBS})],
		[BUILD_LIBS="$withval"])
	AC_MSG_RESULT($BUILD_LIBS)

	# this assumes we're on Unix.
	BUILD_EXEEXT=
	BUILD_OBJEXT=o

	: ${BUILD_CC:='${CC}'}

	AC_MSG_CHECKING(if the build-compiler "$BUILD_CC" works)

	cf_save_crossed=$cross_compiling
	cf_save_ac_link=$ac_link
	cross_compiling=no
	cf_build_cppflags=$BUILD_CPPFLAGS
	test "$cf_build_cppflags" = "#" && cf_build_cppflags=
	ac_link='$BUILD_CC -o "conftest$ac_exeext" $BUILD_CFLAGS $cf_build_cppflags $BUILD_LDFLAGS "conftest.$ac_ext" $BUILD_LIBS >&AS_MESSAGE_LOG_FD'

	AC_TRY_RUN([#include <stdio.h>
		int main(int argc, char *argv[])
		{
			${cf_cv_main_return:-return}(argc < 0 || argv == (void*)0 || argv[0] == (void*)0);
		}
	],
		cf_ok_build_cc=yes,
		cf_ok_build_cc=no,
		cf_ok_build_cc=unknown)

	cross_compiling=$cf_save_crossed
	ac_link=$cf_save_ac_link

	AC_MSG_RESULT($cf_ok_build_cc)

	if test "$cf_ok_build_cc" != yes
	then
		AC_MSG_ERROR([Cross-build requires two compilers.
Use --with-build-cc to specify the native compiler.])
	fi

else
	: ${BUILD_CC:='${CC}'}
	: ${BUILD_CPP:='${CPP}'}
	: ${BUILD_CFLAGS:='${CFLAGS}'}
	: ${BUILD_CPPFLAGS:='${CPPFLAGS}'}
	: ${BUILD_LDFLAGS:='${LDFLAGS}'}
	: ${BUILD_LIBS:='${LIBS}'}
	: ${BUILD_EXEEXT:='$x'}
	: ${BUILD_OBJEXT:='o'}
fi

AC_SUBST(BUILD_CC)
AC_SUBST(BUILD_CPP)
AC_SUBST(BUILD_CFLAGS)
AC_SUBST(BUILD_CPPFLAGS)
AC_SUBST(BUILD_LDFLAGS)
AC_SUBST(BUILD_LIBS)
AC_SUBST(BUILD_EXEEXT)
AC_SUBST(BUILD_OBJEXT)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_C11_NORETURN version: 4 updated: 2023/02/18 17:41:25
dnl ---------------
AC_DEFUN([CF_C11_NORETURN],
[
AC_MSG_CHECKING(if you want to use C11 _Noreturn feature)
CF_ARG_ENABLE(stdnoreturn,
	[  --enable-stdnoreturn    enable C11 _Noreturn feature for diagnostics],
	[enable_stdnoreturn=yes],
	[enable_stdnoreturn=no])
AC_MSG_RESULT($enable_stdnoreturn)

if test $enable_stdnoreturn = yes; then
AC_CACHE_CHECK([for C11 _Noreturn feature], cf_cv_c11_noreturn,
	[AC_TRY_COMPILE([
$ac_includes_default
#include <stdnoreturn.h>
static _Noreturn void giveup(void) { exit(0); }
	],
	[if (feof(stdin)) giveup()],
	cf_cv_c11_noreturn=yes,
	cf_cv_c11_noreturn=no)
	])
else
	cf_cv_c11_noreturn=no,
fi

if test "$cf_cv_c11_noreturn" = yes; then
	AC_DEFINE(HAVE_STDNORETURN_H, 1,[Define if <stdnoreturn.h> header is available and working])
	AC_DEFINE_UNQUOTED(STDC_NORETURN,_Noreturn,[Define if C11 _Noreturn keyword is supported])
	HAVE_STDNORETURN_H=1
else
	HAVE_STDNORETURN_H=0
fi

AC_SUBST(HAVE_STDNORETURN_H)
AC_SUBST(STDC_NORETURN)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CC_ENV_FLAGS version: 11 updated: 2023/02/20 11:15:46
dnl ---------------
dnl Check for user's environment-breakage by stuffing CFLAGS/CPPFLAGS content
dnl into CC.  This will not help with broken scripts that wrap the compiler
dnl with options, but eliminates a more common category of user confusion.
dnl
dnl In particular, it addresses the problem of being able to run the C
dnl preprocessor in a consistent manner.
dnl
dnl Caveat: this also disallows blanks in the pathname for the compiler, but
dnl the nuisance of having inconsistent settings for compiler and preprocessor
dnl outweighs that limitation.
AC_DEFUN([CF_CC_ENV_FLAGS],
[
# This should have been defined by AC_PROG_CC
: "${CC:=cc}"

AC_MSG_CHECKING(\$CFLAGS variable)
case "x$CFLAGS" in
(*-[[IUD]]*)
	AC_MSG_RESULT(broken)
	AC_MSG_WARN(your environment uses the CFLAGS variable to hold CPPFLAGS options)
	cf_flags="$CFLAGS"
	CFLAGS=
	for cf_arg in $cf_flags
	do
		CF_ADD_CFLAGS($cf_arg)
	done
	;;
(*)
	AC_MSG_RESULT(ok)
	;;
esac

AC_MSG_CHECKING(\$CC variable)
case "$CC" in
(*[[\ \	]]-*)
	AC_MSG_RESULT(broken)
	AC_MSG_WARN(your environment uses the CC variable to hold CFLAGS/CPPFLAGS options)
	# humor him...
	cf_prog=`echo "$CC" | sed -e 's/	/ /g' -e 's/[[ ]]* / /g' -e 's/[[ ]]*[[ ]]-[[^ ]].*//'`
	cf_flags=`echo "$CC" | sed -e "s%^$cf_prog%%"`
	CC="$cf_prog"
	for cf_arg in $cf_flags
	do
		case "x$cf_arg" in
		(x-[[IUDfgOW]]*)
			CF_ADD_CFLAGS($cf_arg)
			;;
		(*)
			CC="$CC $cf_arg"
			;;
		esac
	done
	CF_VERBOSE(resulting CC: '$CC')
	CF_VERBOSE(resulting CFLAGS: '$CFLAGS')
	CF_VERBOSE(resulting CPPFLAGS: '$CPPFLAGS')
	;;
(*)
	AC_MSG_RESULT(ok)
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CC_INIT_UNIONS version: 4 updated: 2023/12/13 18:02:34
dnl -----------------
dnl Check if the C compiler supports initialization of unions.
AC_DEFUN([CF_CC_INIT_UNIONS],[
AC_CACHE_CHECK(if we can initialize unions,
cf_cv_init_unions,[
	AC_TRY_COMPILE([],
	[static struct foo {int x; union {double a; int b; } bar; } c = {0,{1.0}}; (void)c],
	[cf_cv_init_unions=yes],
	[cf_cv_init_unions=no])
	])
test $cf_cv_init_unions = no && AC_DEFINE(CC_CANNOT_INIT_UNIONS,1,[Define to 1 if C compiler cannot initialize unions])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CHAR_DEVICE version: 4 updated: 2020/12/31 20:19:42
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
	cf_result="`echo "$1" | sed -e s%/%_%g`"
	CF_UPPER(cf_result,$cf_result)
	AC_DEFINE_UNQUOTED(HAVE$cf_result)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CHECK_CACHE version: 13 updated: 2020/12/31 10:54:15
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
if test -f "$srcdir/config.guess" || test -f "$ac_aux_dir/config.guess" ; then
	ifelse([$1],,[AC_CANONICAL_HOST],[$1])
	system_name="$host_os"
else
	system_name="`(uname -s -r) 2>/dev/null`"
	if test -z "$system_name" ; then
		system_name="`(hostname) 2>/dev/null`"
	fi
fi
test -n "$system_name" && AC_DEFINE_UNQUOTED(SYSTEM_NAME,"$system_name",[Define to the system name.])
AC_CACHE_VAL(cf_cv_system_name,[cf_cv_system_name="$system_name"])

test -z "$system_name" && system_name="$cf_cv_system_name"
test -n "$cf_cv_system_name" && AC_MSG_RESULT(Configuring for $cf_cv_system_name)

if test ".$system_name" != ".$cf_cv_system_name" ; then
	AC_MSG_RESULT(Cached system name ($system_name) does not agree with actual ($cf_cv_system_name))
	AC_MSG_ERROR("Please remove config.cache and try again.")
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CHECK_CFLAGS version: 4 updated: 2021/01/02 19:22:58
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
if test "x$cf_check_cflags" != "x$CFLAGS" ; then
AC_TRY_LINK([#include <stdio.h>],[printf("Hello world");],,
	[CF_VERBOSE(test-compile failed.  Undoing change to \$CFLAGS)
	 if test "x$cf_check_cppflags" != "x$CPPFLAGS" ; then
		 CF_VERBOSE(but keeping change to \$CPPFLAGS)
	 fi
	 CFLAGS="$cf_check_cflags"])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CHECK_ENVIRON version: 5 updated: 2023/02/18 17:41:25
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
$ac_includes_default],
    ifelse([$2],,void*,[$2]) x = (ifelse([$2],,void*,[$2])) $1; (void)x,
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
dnl CF_CHECK_ERRNO version: 14 updated: 2023/02/18 17:41:25
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
$ac_includes_default
#include <errno.h> ],
	ifelse([$2],,int,[$2]) x = (ifelse([$2],,int,[$2])) $1; (void)x,
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
dnl CF_CHECK_EXTERN_DATA version: 5 updated: 2021/09/04 06:35:04
dnl --------------------
dnl Check for existence of external data in the current set of libraries.  If
dnl we can modify it, it is real enough.
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
dnl CF_CHECK_FD_SET version: 6 updated: 2024/12/04 03:41:57
dnl ---------------
dnl Check if the fd_set type and corresponding macros are defined.
AC_DEFUN([CF_CHECK_FD_SET],
[
AC_REQUIRE([CF_TYPE_FD_SET])
AC_CACHE_CHECK([for fd_set macros],cf_cv_macros_fd_set,[
AC_TRY_COMPILE([
$ac_includes_default
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
test $cf_cv_macros_fd_set = yes && AC_DEFINE(HAVE_TYPE_FD_SET,1,[Define to 1 if type fd_set is declared])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CHECK_LIBSSP version: 1 updated: 2021/10/30 10:40:19
dnl ---------------
dnl Check if libssp is needed, e.g., to work around misconfigured libraries
dnl used in cross-compiling to MinGW.
AC_DEFUN([CF_CHECK_LIBSSP],[
AC_CACHE_CHECK(if ssp library is needed,cf_cv_need_libssp,[
AC_TRY_LINK([
#include <sys/types.h>
#include <dirent.h>
],[
       DIR *dp = opendir(".");
],cf_cv_need_libssp=no,[
	cf_save_LIBS="$LIBS"
	LIBS="$LIBS -lssp"
	AC_TRY_LINK([
#include <sys/types.h>
#include <dirent.h>
	],[
		   DIR *dp = opendir(".");
	],cf_cv_need_libssp=yes,
	  cf_cv_need_libssp=maybe)
	LIBS="$cf_save_LIBS"
])dnl
])

if test "x$cf_cv_need_libssp" = xyes
then
	CF_ADD_LIB(ssp)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CLANG_COMPILER version: 9 updated: 2023/02/18 17:41:25
dnl -----------------
dnl Check if the given compiler is really clang.  clang's C driver defines
dnl __GNUC__ (fooling the configure script into setting $GCC to yes) but does
dnl not ignore some gcc options.
dnl
dnl This macro should be run "soon" after AC_PROG_CC or AC_PROG_CPLUSPLUS, to
dnl ensure that it is not mistaken for gcc/g++.  It is normally invoked from
dnl the wrappers for gcc and g++ warnings.
dnl
dnl $1 = GCC (default) or GXX
dnl $2 = CLANG_COMPILER (default)
dnl $3 = CFLAGS (default) or CXXFLAGS
AC_DEFUN([CF_CLANG_COMPILER],[
ifelse([$2],,CLANG_COMPILER,[$2])=no

if test "$ifelse([$1],,[$1],GCC)" = yes ; then
	AC_MSG_CHECKING(if this is really Clang ifelse([$1],GXX,C++,C) compiler)
	cf_save_CFLAGS="$ifelse([$3],,CFLAGS,[$3])"
	AC_TRY_COMPILE([],[
#ifdef __clang__
#else
#error __clang__ is not defined
#endif
],[ifelse([$2],,CLANG_COMPILER,[$2])=yes
],[])
	ifelse([$3],,CFLAGS,[$3])="$cf_save_CFLAGS"
	AC_MSG_RESULT($ifelse([$2],,CLANG_COMPILER,[$2]))
fi

CLANG_VERSION=none

if test "x$ifelse([$2],,CLANG_COMPILER,[$2])" = "xyes" ; then
	case "$CC" in
	(c[[1-9]][[0-9]]|*/c[[1-9]][[0-9]])
		AC_MSG_WARN(replacing broken compiler alias $CC)
		CFLAGS="$CFLAGS -std=`echo "$CC" | sed -e 's%.*/%%'`"
		CC=clang
		;;
	esac

	AC_MSG_CHECKING(version of $CC)
	CLANG_VERSION="`$CC --version 2>/dev/null | sed -e '2,$d' -e 's/^.*(CLANG[[^)]]*) //' -e 's/^.*(Debian[[^)]]*) //' -e 's/^[[^0-9.]]*//' -e 's/[[^0-9.]].*//'`"
	test -z "$CLANG_VERSION" && CLANG_VERSION=unknown
	AC_MSG_RESULT($CLANG_VERSION)

	for cf_clang_opt in \
		-Qunused-arguments \
		-Wno-error=implicit-function-declaration
	do
		AC_MSG_CHECKING(if option $cf_clang_opt works)
		cf_save_CFLAGS="$CFLAGS"
		CFLAGS="$CFLAGS $cf_clang_opt"
		AC_TRY_LINK([
			#include <stdio.h>],[
			printf("hello!\\n");],[
			cf_clang_optok=yes],[
			cf_clang_optok=no])
		AC_MSG_RESULT($cf_clang_optok)
		CFLAGS="$cf_save_CFLAGS"
		if test "$cf_clang_optok" = yes; then
			CF_VERBOSE(adding option $cf_clang_opt)
			CF_APPEND_TEXT(CFLAGS,$cf_clang_opt)
		fi
	done
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_CONST_X_STRING version: 9 updated: 2024/12/04 03:49:57
dnl -----------------
dnl The X11R4-X11R6 Xt specification uses an ambiguous String type for most
dnl character-strings.
dnl
dnl It is ambiguous because the specification accommodated the pre-ANSI
dnl compilers bundled by more than one vendor in lieu of providing a standard C
dnl compiler other than by costly add-ons.  Because of this, the specification
dnl did not take into account the use of const for telling the compiler that
dnl string literals would be in readonly memory.
dnl
dnl As a workaround, one could (starting with X11R5) define XTSTRINGDEFINES, to
dnl let the compiler decide how to represent Xt's strings which were #define'd.
dnl That does not solve the problem of using the block of Xt's strings which
dnl are compiled into the library (and is less efficient than one might want).
dnl
dnl Xt specification 7 introduces the _CONST_X_STRING symbol which is used both
dnl when compiling the library and compiling using the library, to tell the
dnl compiler that String is const.
AC_DEFUN([CF_CONST_X_STRING],
[
AC_REQUIRE([AC_PATH_XTRA])

CF_SAVE_XTRA_FLAGS([CF_CONST_X_STRING])

AC_TRY_COMPILE(
[
$ac_includes_default
#include <X11/Intrinsic.h>
],
[String foo = malloc(1); free((void*)foo)],[

AC_CACHE_CHECK(for X11/Xt const-feature,cf_cv_const_x_string,[
	AC_TRY_COMPILE(
		[
#undef  _CONST_X_STRING
#define _CONST_X_STRING	/* X11R7.8 (perhaps) */
#undef  XTSTRINGDEFINES	/* X11R5 and later */
$ac_includes_default
#include <X11/Intrinsic.h>
		],[String foo = malloc(1); *foo = 0],[
			cf_cv_const_x_string=no
		],[
			cf_cv_const_x_string=yes
		])
])

CF_RESTORE_XTRA_FLAGS([CF_CONST_X_STRING])

case "$cf_cv_const_x_string" in
(no)
	CF_APPEND_TEXT(CPPFLAGS,-DXTSTRINGDEFINES)
	;;
(*)
	CF_APPEND_TEXT(CPPFLAGS,-D_CONST_X_STRING)
	;;
esac

])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CRYPT_FUNC version: 10 updated: 2023/01/14 07:48:15
dnl -------------
dnl Check if we have a working crypt() function
AC_DEFUN([CF_CRYPT_FUNC],
[
AC_CACHE_CHECK(for crypt function,cf_cv_crypt_func,[
cf_cv_crypt_func=
cf_crypt_headers="\
$ac_includes_default
extern char *crypt(const char *, const char *);
"
AC_TRY_LINK([$cf_crypt_headers],[crypt("","")],[
	cf_cv_crypt_func=yes],[
	cf_save_LIBS="$LIBS"
	LIBS="-lcrypt $LIBS"
	AC_TRY_LINK([$cf_crypt_headers],[crypt("","")],[
		cf_cv_crypt_func="-lcrypt"],[
		cf_cv_crypt_func=no])
	LIBS="$cf_save_LIBS"
	])
])
if test "$cf_cv_crypt_func" != no ; then
	cf_save_LIBS="$LIBS"
	test "$cf_cv_crypt_func" != yes && LIBS="$cf_cv_crypt_func $LIBS"
AC_CACHE_CHECK(if crypt works,cf_cv_crypt_works,[
AC_TRY_RUN([$cf_crypt_headers

int main(void) {
	char *s = crypt("vi-crypt", "vi");
	${cf_cv_main_return:-return}(strcmp("vi6r2tczBYLvM", s ? s : "") != 0);
}
	],[
	cf_cv_crypt_works=yes],[
	cf_cv_crypt_works=no],[
	cf_cv_crypt_works=unknown])
	LIBS="$cf_save_LIBS"])
	if test "$cf_cv_crypt_works" = no ; then
		cf_cv_crypt_func=no
	else
		AC_DEFINE(HAVE_CRYPT)
		if test "$cf_cv_crypt_func" != yes ; then
			LIBS="$cf_cv_crypt_func $LIBS"
		fi
	fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CURSES_CPPFLAGS version: 14 updated: 2021/01/02 09:31:20
dnl ------------------
dnl Look for the curses headers.
AC_DEFUN([CF_CURSES_CPPFLAGS],[

AC_CACHE_CHECK(for extra include directories,cf_cv_curses_incdir,[
cf_cv_curses_incdir=no
case "$host_os" in
(hpux10.*)
	if test "x$cf_cv_screen" = "xcurses_colr"
	then
		test -d /usr/include/curses_colr && \
		cf_cv_curses_incdir="-I/usr/include/curses_colr"
	fi
	;;
(sunos3*|sunos4*)
	if test "x$cf_cv_screen" = "xcurses_5lib"
	then
		test -d /usr/5lib && \
		test -d /usr/5include && \
		cf_cv_curses_incdir="-I/usr/5include"
	fi
	;;
esac
])
if test "$cf_cv_curses_incdir" != no
then
	CF_APPEND_TEXT(CPPFLAGS,$cf_cv_curses_incdir)
fi

CF_CURSES_HEADER
CF_TERM_HEADER
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CURSES_FUNCS version: 21 updated: 2024/06/11 20:24:35
dnl ---------------
dnl Curses-functions are a little complicated, since a lot of them are macros.
dnl
dnl $1 is a list of functions to test
AC_DEFUN([CF_CURSES_FUNCS],
[
AC_REQUIRE([CF_CURSES_CPPFLAGS])dnl
AC_REQUIRE([CF_XOPEN_CURSES])
AC_REQUIRE([CF_CURSES_TERM_H])
AC_REQUIRE([CF_CURSES_UNCTRL_H])

AC_FOREACH([AC_Func], [$1],
  [AH_TEMPLATE(AS_TR_CPP(HAVE_[]AC_Func),
               [Define if you have the `]AC_Func[' function.])])dnl

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
fprintf(stderr, "testing linkage of $cf_func:%p\\n", (void *)foo);
if (foo + 1234L > 5678L)
	${cf_cv_main_return:-return}(foo != 0);
#endif
			],
			[cf_result=yes],
			[cf_result=no])
		fi
		eval 'cf_cv_func_'$cf_func'="$cf_result"'
	])
	# use the computed/retrieved cache-value:
	eval 'cf_result=$cf_cv_func_'$cf_func
	AC_MSG_RESULT($cf_result)
	if test "$cf_result" != no; then
		AC_DEFINE_UNQUOTED(HAVE_${cf_tr_func})
	fi
done
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CURSES_HEADER version: 6 updated: 2022/12/02 20:06:52
dnl ----------------
dnl Find a "curses" header file, e.g,. "curses.h", or one of the more common
dnl variations of ncurses' installs.
dnl
dnl $1 = ncurses when looking for ncurses, or is empty
AC_DEFUN([CF_CURSES_HEADER],[
AC_CACHE_CHECK(if we have identified curses headers,cf_cv_ncurses_header,[
cf_cv_ncurses_header=none
for cf_header in \
	ncurses.h ifelse($1,,,[$1/ncurses.h]) \
	curses.h ifelse($1,,,[$1/curses.h]) ifelse($1,,[ncurses/ncurses.h ncurses/curses.h])
do
AC_TRY_COMPILE([#include <${cf_header}>],
	[initscr(); endwin()],
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
dnl CF_CURSES_LIBS version: 45 updated: 2022/12/02 20:06:52
dnl --------------
dnl Look for the curses libraries.  Older curses implementations may require
dnl termcap/termlib to be linked as well.  Call CF_CURSES_CPPFLAGS first.
AC_DEFUN([CF_CURSES_LIBS],[

AC_REQUIRE([CF_CURSES_CPPFLAGS])dnl
AC_MSG_CHECKING(if we have identified curses libraries)
AC_TRY_LINK([#include <${cf_cv_ncurses_header:-curses.h}>],
	[initscr(); endwin()],
	cf_result=yes,
	cf_result=no)
AC_MSG_RESULT($cf_result)

if test "$cf_result" = no ; then
case "$host_os" in
(freebsd*)
	AC_CHECK_LIB(mytinfo,tgoto,[CF_ADD_LIBS(-lmytinfo)])
	;;
(hpux10.*)
	# Looking at HPUX 10.20, the Hcurses library is the oldest (1997), cur_colr
	# next (1998), and xcurses "newer" (2000).  There is no header file for
	# Hcurses; the subdirectory curses_colr has the headers (curses.h and
	# term.h) for cur_colr
	if test "x$cf_cv_screen" = "xcurses_colr"
	then
		AC_CHECK_LIB(cur_colr,initscr,[
			CF_ADD_LIBS(-lcur_colr)
			ac_cv_func_initscr=yes
			],[
		AC_CHECK_LIB(Hcurses,initscr,[
			# HP's header uses __HP_CURSES, but user claims _HP_CURSES.
			CF_ADD_LIBS(-lHcurses)
			CF_APPEND_TEXT(CPPFLAGS,-D__HP_CURSES -D_HP_CURSES)
			ac_cv_func_initscr=yes
			])])
	fi
	;;
(linux*)
	case `arch 2>/dev/null` in
	(x86_64)
		if test -d /lib64
		then
			CF_ADD_LIBDIR(/lib64)
		else
			CF_ADD_LIBDIR(/lib)
		fi
		;;
	(*)
		CF_ADD_LIBDIR(/lib)
		;;
	esac
	;;
(sunos3*|sunos4*)
	if test "x$cf_cv_screen" = "xcurses_5lib"
	then
		if test -d /usr/5lib ; then
			CF_ADD_LIBDIR(/usr/5lib)
			CF_ADD_LIBS(-lcurses -ltermcap)
		fi
	fi
	ac_cv_func_initscr=yes
	;;
esac

if test ".$ac_cv_func_initscr" != .yes ; then
	cf_save_LIBS="$LIBS"

	if test ".${cf_cv_ncurses_version:-no}" != .no
	then
		cf_check_list="ncurses curses cursesX"
	else
		cf_check_list="cursesX curses ncurses"
	fi

	# Check for library containing tgoto.  Do this before curses library
	# because it may be needed to link the test-case for initscr.
	if test "x$cf_term_lib" = x
	then
		AC_CHECK_FUNC(tgoto,[cf_term_lib=predefined],[
			for cf_term_lib in $cf_check_list otermcap termcap tinfo termlib unknown
			do
				AC_CHECK_LIB($cf_term_lib,tgoto,[
					: "${cf_nculib_root:=$cf_term_lib}"
					break
				])
			done
		])
	fi

	# Check for library containing initscr
	test "$cf_term_lib" != predefined && test "$cf_term_lib" != unknown && LIBS="-l$cf_term_lib $cf_save_LIBS"
	if test "x$cf_curs_lib" = x
	then
		for cf_curs_lib in $cf_check_list xcurses jcurses pdcurses unknown
		do
			LIBS="-l$cf_curs_lib $cf_save_LIBS"
			if test "$cf_term_lib" = unknown || test "$cf_term_lib" = "$cf_curs_lib" ; then
				AC_MSG_CHECKING(if we can link with $cf_curs_lib library)
				AC_TRY_LINK([#include <${cf_cv_ncurses_header:-curses.h}>],
					[initscr()],
					[cf_result=yes],
					[cf_result=no])
				AC_MSG_RESULT($cf_result)
				test "$cf_result" = yes && break
			elif test "$cf_curs_lib" = "$cf_term_lib" ; then
				cf_result=no
			elif test "$cf_term_lib" != predefined ; then
				AC_MSG_CHECKING(if we need both $cf_curs_lib and $cf_term_lib libraries)
				AC_TRY_LINK([#include <${cf_cv_ncurses_header:-curses.h}>],
					[initscr(); endwin();],
					[cf_result=no],
					[
					LIBS="-l$cf_curs_lib -l$cf_term_lib $cf_save_LIBS"
					AC_TRY_LINK([#include <${cf_cv_ncurses_header:-curses.h}>],
						[initscr()],
						[cf_result=yes],
						[cf_result=error])
					])
				AC_MSG_RESULT($cf_result)
				test "$cf_result" != error && break
			fi
		done
	fi
	test "$cf_curs_lib" = unknown && AC_MSG_ERROR(no curses library found)
fi
fi

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CURSES_TERMCAP version: 15 updated: 2023/12/09 10:53:57
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
	cf_tgoto_decl="
	extern char *tgoto(char*,int,int);"
	test -n "${cf_c_opts}${cf_t_opts}" && cf_tgoto_decl=

    AC_TRY_LINK([/* $cf_c_opts $cf_t_opts */
$CHECK_DECL_HDRS $cf_tgoto_decl],
	[static char fmt[] = ""; char *x = tgoto(fmt); (void)x],
	[test "$cf_cv_need_curses_h" = no && {
	     cf_cv_need_curses_h=maybe
	     cf_ok_c_opts=$cf_c_opts
	     cf_ok_t_opts=$cf_t_opts
	}],
	[echo "Recompiling with corrected call (C:$cf_c_opts, T:$cf_t_opts)" >&AC_FD_CC
	AC_TRY_LINK([
$CHECK_DECL_HDRS $cf_tgoto_decl],
	[static char fmt[] = ""; char *x = tgoto(fmt,0,0); (void)x],
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
(both)
	AC_DEFINE_UNQUOTED(NEED_CURSES_H,1,[Define to 1 if we must include curses.h])
	AC_DEFINE_UNQUOTED(NEED_TERMCAP_H,1,[Define to 1 if we must include termcap.h])
	;;
(curses.h)
	AC_DEFINE_UNQUOTED(NEED_CURSES_H,1,[Define to 1 if we must include curses.h])
	;;
(term.h)
	AC_DEFINE_UNQUOTED(NEED_TERM_H,1,[Define to 1 if we must include term.h])
	;;
(termcap.h)
	AC_DEFINE_UNQUOTED(NEED_TERMCAP_H,1,[Define to 1 if we must include termcap.h])
	;;
esac

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CURSES_TERM_H version: 16 updated: 2024/01/07 06:34:16
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

cf_header_list="term.h ncurses/term.h ncursesw/term.h"

case "${cf_cv_ncurses_header:-curses.h}" in
(*/*)
	cf_header_item=`echo "${cf_cv_ncurses_header:-curses.h}" | sed -e 's%\..*%%' -e 's%/.*%/%'`term.h
	cf_header_list="$cf_header_item $cf_header_list"
	;;
esac

for cf_header in $cf_header_list
do
	AC_TRY_COMPILE([
#include <${cf_cv_ncurses_header:-curses.h}>
#include <${cf_header}>],
	[WINDOW *x; (void)x],
	[cf_cv_term_header=$cf_header
	 break],
	[cf_cv_term_header=no])
done

case "$cf_cv_term_header" in
(no)
	# If curses is ncurses, some packagers still mess it up by trying to make
	# us use GNU termcap.  This handles the most common case.
	for cf_header in ncurses/term.h ncursesw/term.h
	do
		AC_TRY_COMPILE([
#include <${cf_cv_ncurses_header:-curses.h}>
#ifdef NCURSES_VERSION
#include <${cf_header}>
#else
#error expected NCURSES_VERSION to be defined
#endif],
			[WINDOW *x; (void)x],
			[cf_cv_term_header=$cf_header
			 break],
			[cf_cv_term_header=no])
	done
	;;
esac
])

case "$cf_cv_term_header" in
(term.h)
	AC_DEFINE(HAVE_TERM_H,1,[Define to 1 if we have term.h])
	;;
(ncurses/term.h)
	AC_DEFINE(HAVE_NCURSES_TERM_H,1,[Define to 1 if we have ncurses/term.h])
	;;
(ncursesw/term.h)
	AC_DEFINE(HAVE_NCURSESW_TERM_H,1,[Define to 1 if we have ncursesw/term.h])
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CURSES_UNCTRL_H version: 8 updated: 2021/01/02 09:31:20
dnl ------------------
dnl Any X/Open curses implementation must have unctrl.h, but ncurses packages
dnl may put it in a subdirectory (along with ncurses' other headers, of
dnl course).  Packages which put the headers in inconsistent locations are
dnl broken).
AC_DEFUN([CF_CURSES_UNCTRL_H],
[
AC_REQUIRE([CF_CURSES_CPPFLAGS])dnl

AC_CACHE_CHECK(for unctrl.h, cf_cv_unctrl_header,[

# If we found <ncurses/curses.h>, look for <ncurses/unctrl.h>, but always look
# for <unctrl.h> if we do not find the variant.

cf_header_list="unctrl.h ncurses/unctrl.h ncursesw/unctrl.h"

case "${cf_cv_ncurses_header:-curses.h}" in
(*/*)
	cf_header_item=`echo "${cf_cv_ncurses_header:-curses.h}" | sed -e 's%\..*%%' -e 's%/.*%/%'`unctrl.h
	cf_header_list="$cf_header_item $cf_header_list"
	;;
esac

for cf_header in $cf_header_list
do
	AC_TRY_COMPILE([
#include <${cf_cv_ncurses_header:-curses.h}>
#include <${cf_header}>],
	[WINDOW *x; (void)x],
	[cf_cv_unctrl_header=$cf_header
	 break],
	[cf_cv_unctrl_header=no])
done
])

case "$cf_cv_unctrl_header" in
(no)
	AC_MSG_WARN(unctrl.h header not found)
	;;
esac

case "$cf_cv_unctrl_header" in
(unctrl.h)
	AC_DEFINE(HAVE_UNCTRL_H,1,[Define to 1 if we have unctrl.h])
	;;
(ncurses/unctrl.h)
	AC_DEFINE(HAVE_NCURSES_UNCTRL_H,1,[Define to 1 if we have ncurses/unctrl.h])
	;;
(ncursesw/unctrl.h)
	AC_DEFINE(HAVE_NCURSESW_UNCTRL_H,1,[Define to 1 if we have ncursesw/unctrl.h])
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_DIRNAME version: 5 updated: 2020/12/31 20:19:42
dnl ----------
dnl "dirname" is not portable, so we fake it with a shell script.
AC_DEFUN([CF_DIRNAME],[$1=`echo "$2" | sed -e 's%/[[^/]]*$%%'`])dnl
dnl ---------------------------------------------------------------------------
dnl CF_DISABLE_ECHO version: 14 updated: 2021/09/04 06:35:04
dnl ---------------
dnl You can always use "make -n" to see the actual options, but it is hard to
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
	[  --disable-echo          do not display "compiling" commands],
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
dnl CF_DISABLE_RPATH_HACK version: 3 updated: 2021/01/05 20:14:44
dnl ---------------------
dnl The rpath-hack makes it simpler to build programs, particularly with the
dnl *BSD ports which may have essential libraries in unusual places.  But it
dnl can interfere with building an executable for the base system.  Use this
dnl option in that case.
AC_DEFUN([CF_DISABLE_RPATH_HACK],
[
AC_MSG_CHECKING(if rpath-hack should be disabled)
CF_ARG_DISABLE(rpath-hack,
	[  --disable-rpath-hack    don't add rpath options for additional libraries],
	[enable_rpath_hack=no],
	[enable_rpath_hack=yes])
dnl TODO - drop cf_disable_rpath_hack
if test "x$enable_rpath_hack" = xno; then cf_disable_rpath_hack=yes; else cf_disable_rpath_hack=no; fi
AC_MSG_RESULT($cf_disable_rpath_hack)

if test "$enable_rpath_hack" = yes ; then
	CF_RPATH_HACK
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_ENABLE_NARROWPROTO version: 6 updated: 2020/12/31 18:40:20
dnl ---------------------
dnl If this is not set properly, Xaw's scrollbars will not work.
dnl The so-called "modular" configuration for X.org omits most of the
dnl configure checks that would be needed to provide compatibility with
dnl older X builds.  This one breaks things noticeably.
AC_DEFUN([CF_ENABLE_NARROWPROTO],
[
AC_MSG_CHECKING(if you want narrow prototypes for X libraries)

case `$ac_config_guess` in
(*freebsd*|*gnu*|*irix5*|*irix6*|*netbsd*|*openbsd*|*qnx*|*sco*|*sgi*)
	cf_default_narrowproto=yes
	;;
(*)
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
dnl CF_ENABLE_WARNINGS version: 9 updated: 2021/01/05 19:40:50
dnl ------------------
dnl Configure-option to enable gcc warnings
dnl
dnl $1 = extra options to add, if supported
dnl $2 = option for checking attributes.  By default, this is done when
dnl      warnings are enabled.  For other values:
dnl      yes: always do this, e.g., to use in generated library-headers
dnl      no: never do this
AC_DEFUN([CF_ENABLE_WARNINGS],[
if test "$GCC" = yes || test "$GXX" = yes
then
CF_FIX_WARNINGS(CFLAGS)
CF_FIX_WARNINGS(CPPFLAGS)
CF_FIX_WARNINGS(LDFLAGS)
AC_MSG_CHECKING(if you want to turn on gcc warnings)
CF_ARG_ENABLE(warnings,
	[  --enable-warnings       test: turn on gcc compiler warnings],
	[enable_warnings=yes],
	[enable_warnings=no])
AC_MSG_RESULT($enable_warnings)
if test "$enable_warnings" = "yes"
then
	ifelse($2,,[CF_GCC_ATTRIBUTES])
	CF_GCC_WARNINGS($1)
fi
ifelse($2,yes,[CF_GCC_ATTRIBUTES])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ERRNO version: 5 updated: 1997/11/30 12:44:39
dnl --------
dnl Check if 'errno' is declared in <errno.h>
AC_DEFUN([CF_ERRNO],
[
CF_CHECK_ERRNO(errno)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_FIND_LIBRARY version: 11 updated: 2021/01/02 09:31:20
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
	eval 'cf_cv_have_lib_'"$1"'=no'
	cf_libdir=""
	AC_CHECK_FUNC($5,
		eval 'cf_cv_have_lib_'"$1"'=yes',[
		cf_save_LIBS="$LIBS"
		AC_MSG_CHECKING(for $5 in -l$1)
		LIBS="-l$1 $LIBS"
		AC_TRY_LINK([$3],[$4],
			[AC_MSG_RESULT(yes)
			 eval 'cf_cv_have_lib_'"$1"'=yes'
			],
			[AC_MSG_RESULT(no)
			CF_LIBRARY_PATH(cf_search,$2)
			for cf_libdir in $cf_search
			do
				AC_MSG_CHECKING(for -l$1 in $cf_libdir)
				LIBS="-L$cf_libdir -l$1 $cf_save_LIBS"
				AC_TRY_LINK([$3],[$4],
					[AC_MSG_RESULT(yes)
			 		 eval 'cf_cv_have_lib_'"$1"'=yes'
					 break],
					[AC_MSG_RESULT(no)
					 LIBS="$cf_save_LIBS"])
			done
			])
		])
eval 'cf_found_library="[$]cf_cv_have_lib_'"$1"\"
ifelse($6,,[
if test "$cf_found_library" = no ; then
	AC_MSG_ERROR(Cannot link $1 library)
fi
])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_FIND_LINKAGE version: 22 updated: 2020/12/31 20:19:42
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
		if test -d "$cf_cv_header_path_$3" ; then
			CF_VERBOSE(... testing $cf_cv_header_path_$3)
			CPPFLAGS="$cf_save_CPPFLAGS"
			CF_APPEND_TEXT(CPPFLAGS,-I$cf_cv_header_path_$3)
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
				if test -d "$cf_cv_library_path_$3" ; then
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
dnl CF_FIX_WARNINGS version: 4 updated: 2021/12/16 18:22:31
dnl ---------------
dnl Warning flags do not belong in CFLAGS, CPPFLAGS, etc.  Any of gcc's
dnl "-Werror" flags can interfere with configure-checks.  Those go into
dnl EXTRA_CFLAGS.
dnl
dnl $1 = variable name to repair
define([CF_FIX_WARNINGS],[
if test "$GCC" = yes || test "$GXX" = yes
then
	case [$]$1 in
	(*-Werror=*)
		cf_temp_flags=
		for cf_temp_scan in [$]$1
		do
			case "x$cf_temp_scan" in
			(x-Werror=format*)
				CF_APPEND_TEXT(cf_temp_flags,$cf_temp_scan)
				;;
			(x-Werror=*)
				CF_APPEND_TEXT(EXTRA_CFLAGS,$cf_temp_scan)
				;;
			(*)
				CF_APPEND_TEXT(cf_temp_flags,$cf_temp_scan)
				;;
			esac
		done
		if test "x[$]$1" != "x$cf_temp_flags"
		then
			CF_VERBOSE(repairing $1: [$]$1)
			$1="$cf_temp_flags"
			CF_VERBOSE(... fixed [$]$1)
			CF_VERBOSE(... extra $EXTRA_CFLAGS)
		fi
		;;
	esac
fi
AC_SUBST(EXTRA_CFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_FP_ISREADY version: 5 updated: 2023/01/14 07:48:15
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
],[int x = isready_c(stdin); (void)x],
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

test -f conftest.env && cf_cv_fp_isready="`cat conftest.env`"

])

test "$cf_cv_fp_isready" != none && AC_DEFINE_UNQUOTED(isready_c(p),$cf_cv_fp_isready,[Define to 1 if stdio structures permit checking if character is ready])

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_FUNC_DLSYM version: 5 updated: 2024/12/14 16:09:34
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
	test "$cf_have_libdl" = yes && { CF_ADD_LIB(dl) }

	AC_MSG_CHECKING(whether able to link to dl*() functions)
	AC_TRY_LINK([
	#include <stdio.h>
	#include <dlfcn.h>
	],[
		void *obj;
		if ((obj = dlopen("filename", 0)) != NULL) {
			if (dlsym(obj, "symbolname") == 0) {
			dlclose(obj);
			}
		}],[
		AC_DEFINE(HAVE_LIBDL,1,[Define to 1 if we have dl library])],[
		AC_MSG_ERROR(Cannot link test program for libdl)])
	AC_MSG_RESULT(ok)
else
	AC_MSG_ERROR(Cannot find dlsym function)
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_GCC_ATTRIBUTES version: 24 updated: 2021/03/20 12:00:25
dnl -----------------
dnl Test for availability of useful gcc __attribute__ directives to quiet
dnl compiler warnings.  Though useful, not all are supported -- and contrary
dnl to documentation, unrecognized directives cause older compilers to barf.
AC_DEFUN([CF_GCC_ATTRIBUTES],
[AC_REQUIRE([AC_PROG_FGREP])dnl
AC_REQUIRE([CF_C11_NORETURN])dnl

if test "$GCC" = yes || test "$GXX" = yes
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
cat > "conftest.$ac_ext" <<EOF
#line __oline__ "${as_me:-configure}"
#include <stdio.h>
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
extern GCC_NORETURN void oops(char *,...) GCC_PRINTFLIKE(1,2);
extern GCC_NORETURN void foo(void);
int main(int argc GCC_UNUSED, char *argv[[]] GCC_UNUSED) { (void)argc; (void)argv; return 0; }
EOF
	cf_printf_attribute=no
	cf_scanf_attribute=no
	for cf_attribute in scanf printf unused noreturn
	do
		CF_UPPER(cf_ATTRIBUTE,$cf_attribute)
		cf_directive="__attribute__(($cf_attribute))"
		echo "checking for $CC $cf_directive" 1>&AC_FD_CC

		case "$cf_attribute" in
		(printf)
			cf_printf_attribute=yes
			cat >conftest.h <<EOF
#define GCC_$cf_ATTRIBUTE 1
EOF
			;;
		(scanf)
			cf_scanf_attribute=yes
			cat >conftest.h <<EOF
#define GCC_$cf_ATTRIBUTE 1
EOF
			;;
		(*)
			cat >conftest.h <<EOF
#define GCC_$cf_ATTRIBUTE $cf_directive
EOF
			;;
		esac

		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... $cf_attribute)
			cat conftest.h >>confdefs.h
			case "$cf_attribute" in
			(noreturn)
				AC_DEFINE_UNQUOTED(GCC_NORETURN,$cf_directive,[Define to noreturn-attribute for gcc])
				;;
			(printf)
				cf_value='/* nothing */'
				if test "$cf_printf_attribute" != no ; then
					cf_value='__attribute__((format(printf,fmt,var)))'
					AC_DEFINE(GCC_PRINTF,1,[Define to 1 if the compiler supports gcc-like printf attribute.])
				fi
				AC_DEFINE_UNQUOTED(GCC_PRINTFLIKE(fmt,var),$cf_value,[Define to printf-attribute for gcc])
				;;
			(scanf)
				cf_value='/* nothing */'
				if test "$cf_scanf_attribute" != no ; then
					cf_value='__attribute__((format(scanf,fmt,var)))'
					AC_DEFINE(GCC_SCANF,1,[Define to 1 if the compiler supports gcc-like scanf attribute.])
				fi
				AC_DEFINE_UNQUOTED(GCC_SCANFLIKE(fmt,var),$cf_value,[Define to sscanf-attribute for gcc])
				;;
			(unused)
				AC_DEFINE_UNQUOTED(GCC_UNUSED,$cf_directive,[Define to unused-attribute for gcc])
				;;
			esac
		fi
	done
else
	${FGREP-fgrep} define conftest.i >>confdefs.h
fi
rm -rf ./conftest*
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GCC_OPT_RDYNAMIC version: 7 updated: 2022/11/17 19:29:37
dnl -------------------
dnl "Newer" versions of gcc support the -rdynamic option:
dnl		Pass the flag -export-dynamic to the ELF linker, on targets that
dnl		support it. This instructs the linker to add all symbols, not only
dnl		used ones, to the dynamic symbol table. This option is needed for
dnl		some uses of "dlopen" or to allow obtaining backtraces from within
dnl		a program.
dnl Check for the option, and add it to $CFLAGS if available.
AC_DEFUN([CF_GCC_OPT_RDYNAMIC],[

if test "x$CLANG_COMPILER" = "xyes"
then
	AC_MSG_WARN(clang may only pretend to honor gcc -rdynamic option)
fi

AC_CACHE_CHECK([if $CC has -rdynamic option],cf_cv_gcc_opt_rdynamic,[

cf_save_CFLAGS="$CFLAGS"
CFLAGS="-Wall -rdynamic $CFLAGS"

# gcc usually does not return an error for unrecognized options.
AC_TRY_LINK([#include <stdio.h>],
	[printf("Hello");],
	[
	# refine check...
	case testing`eval "$ac_link" 2>&1` in
	(*unexpect*|*unrecog*)
		cf_cv_gcc_opt_rdynamic=no
		;;
	(*)
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
dnl CF_GCC_VERSION version: 9 updated: 2023/03/05 14:30:13
dnl --------------
dnl Find version of gcc, and (because icc/clang pretend to be gcc without being
dnl compatible), attempt to determine if icc/clang is actually used.
AC_DEFUN([CF_GCC_VERSION],[
AC_REQUIRE([AC_PROG_CC])
GCC_VERSION=none
if test "$GCC" = yes ; then
	AC_MSG_CHECKING(version of $CC)
	GCC_VERSION="`${CC} --version 2>/dev/null | sed -e '2,$d' -e 's/^[[^(]]*([[^)]][[^)]]*) //' -e 's/^[[^0-9.]]*//' -e 's/[[^0-9.]].*//'`"
	test -z "$GCC_VERSION" && GCC_VERSION=unknown
	AC_MSG_RESULT($GCC_VERSION)
fi
CF_INTEL_COMPILER(GCC,INTEL_COMPILER,CFLAGS)
CF_CLANG_COMPILER(GCC,CLANG_COMPILER,CFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GCC_WARNINGS version: 43 updated: 2024/12/21 08:44:12
dnl ---------------
dnl Check if the compiler supports useful warning options.  There's a few that
dnl we don't use, simply because they're too noisy:
dnl
dnl	-Wconversion (useful in older versions of gcc, but not in gcc 2.7.x)
dnl	-Winline (usually not worthwhile)
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
if test "x$have_x" = xyes; then CF_CONST_X_STRING fi
cat > "conftest.$ac_ext" <<EOF
#line __oline__ "${as_me:-configure}"
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
	EXTRA_CFLAGS="$EXTRA_CFLAGS -Wall"
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
elif test "$GCC" = yes && test "$GCC_VERSION" != "unknown"
then
	AC_CHECKING([for $CC warning options])
	cf_save_CFLAGS="$CFLAGS"
	cf_warn_CONST=""
	test "$with_ext_const" = yes && cf_warn_CONST="Wwrite-strings"
	cf_gcc_warnings="Wignored-qualifiers Wlogical-op Wvarargs"
	test "x$CLANG_COMPILER" = xyes && cf_gcc_warnings=
	for cf_opt in W Wall \
		Wbad-function-cast \
		Wcast-align \
		Wcast-qual \
		Wdeclaration-after-statement \
		Wextra \
		Winline \
		Wmissing-declarations \
		Wmissing-prototypes \
		Wnested-externs \
		Wpointer-arith \
		Wshadow \
		Wstrict-prototypes \
		Wundef Wno-inline $cf_gcc_warnings $cf_warn_CONST $1
	do
		CFLAGS="$cf_save_CFLAGS $EXTRA_CFLAGS -$cf_opt"
		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... -$cf_opt)
			case "$cf_opt" in
			(Winline)
				case "$GCC_VERSION" in
				([[34]].*)
					CF_VERBOSE(feature is broken in gcc $GCC_VERSION)
					continue;;
				esac
				;;
			(Wpointer-arith)
				case "$GCC_VERSION" in
				([[12]].*)
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
rm -rf ./conftest*

AC_SUBST(EXTRA_CFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GLOB_FULLPATH version: 2 updated: 2024/08/03 12:34:02
dnl ----------------
dnl Use this in case-statements to check for pathname syntax, i.e., absolute
dnl pathnames.  The "x" is assumed since we provide an alternate form for DOS.
AC_DEFUN([CF_GLOB_FULLPATH],[
AC_REQUIRE([CF_WITH_SYSTYPE])dnl
case "$cf_cv_system_name" in
(cygwin*|msys*|mingw32*|mingw64|os2*)
	GLOB_FULLPATH_POSIX='/*'
	GLOB_FULLPATH_OTHER='[[a-zA-Z]]:[[\\/]]*'
	;;
(*)
	GLOB_FULLPATH_POSIX='/*'
	GLOB_FULLPATH_OTHER=$GLOB_FULLPATH_POSIX
	;;
esac
AC_SUBST(GLOB_FULLPATH_POSIX)
AC_SUBST(GLOB_FULLPATH_OTHER)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GNU_SOURCE version: 10 updated: 2018/12/10 20:09:41
dnl -------------
dnl Check if we must define _GNU_SOURCE to get a reasonable value for
dnl _XOPEN_SOURCE, upon which many POSIX definitions depend.  This is a defect
dnl (or misfeature) of glibc2, which breaks portability of many applications,
dnl since it is interwoven with GNU extensions.
dnl
dnl Well, yes we could work around it...
dnl
dnl Parameters:
dnl	$1 is the nominal value for _XOPEN_SOURCE
AC_DEFUN([CF_GNU_SOURCE],
[
cf_gnu_xopen_source=ifelse($1,,500,$1)

AC_CACHE_CHECK(if this is the GNU C library,cf_cv_gnu_library,[
AC_TRY_COMPILE([#include <sys/types.h>],[
	#if __GLIBC__ > 0 && __GLIBC_MINOR__ >= 0
		return 0;
	#elif __NEWLIB__ > 0 && __NEWLIB_MINOR__ >= 0
		return 0;
	#else
	#	error not GNU C library
	#endif],
	[cf_cv_gnu_library=yes],
	[cf_cv_gnu_library=no])
])

if test x$cf_cv_gnu_library = xyes; then

	# With glibc 2.19 (13 years after this check was begun), _DEFAULT_SOURCE
	# was changed to help a little.  newlib incorporated the change about 4
	# years later.
	AC_CACHE_CHECK(if _DEFAULT_SOURCE can be used as a basis,cf_cv_gnu_library_219,[
		cf_save="$CPPFLAGS"
		CF_APPEND_TEXT(CPPFLAGS,-D_DEFAULT_SOURCE)
		AC_TRY_COMPILE([#include <sys/types.h>],[
			#if (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 19) || (__GLIBC__ > 2)
				return 0;
			#elif (__NEWLIB__ == 2 && __NEWLIB_MINOR__ >= 4) || (__GLIBC__ > 3)
				return 0;
			#else
			#	error GNU C library __GLIBC__.__GLIBC_MINOR__ is too old
			#endif],
			[cf_cv_gnu_library_219=yes],
			[cf_cv_gnu_library_219=no])
		CPPFLAGS="$cf_save"
	])

	if test "x$cf_cv_gnu_library_219" = xyes; then
		cf_save="$CPPFLAGS"
		AC_CACHE_CHECK(if _XOPEN_SOURCE=$cf_gnu_xopen_source works with _DEFAULT_SOURCE,cf_cv_gnu_dftsrc_219,[
			CF_ADD_CFLAGS(-D_DEFAULT_SOURCE -D_XOPEN_SOURCE=$cf_gnu_xopen_source)
			AC_TRY_COMPILE([
				#include <limits.h>
				#include <sys/types.h>
				],[
				#if (_XOPEN_SOURCE >= $cf_gnu_xopen_source) && (MB_LEN_MAX > 1)
					return 0;
				#else
				#	error GNU C library is too old
				#endif],
				[cf_cv_gnu_dftsrc_219=yes],
				[cf_cv_gnu_dftsrc_219=no])
			])
		test "x$cf_cv_gnu_dftsrc_219" = "xyes" || CPPFLAGS="$cf_save"
	else
		cf_cv_gnu_dftsrc_219=maybe
	fi

	if test "x$cf_cv_gnu_dftsrc_219" != xyes; then

		AC_CACHE_CHECK(if we must define _GNU_SOURCE,cf_cv_gnu_source,[
		AC_TRY_COMPILE([#include <sys/types.h>],[
			#ifndef _XOPEN_SOURCE
			#error	expected _XOPEN_SOURCE to be defined
			#endif],
			[cf_cv_gnu_source=no],
			[cf_save="$CPPFLAGS"
			 CF_ADD_CFLAGS(-D_GNU_SOURCE)
			 AC_TRY_COMPILE([#include <sys/types.h>],[
				#ifdef _XOPEN_SOURCE
				#error	expected _XOPEN_SOURCE to be undefined
				#endif],
				[cf_cv_gnu_source=no],
				[cf_cv_gnu_source=yes])
			CPPFLAGS="$cf_save"
			])
		])

		if test "$cf_cv_gnu_source" = yes
		then
		AC_CACHE_CHECK(if we should also define _DEFAULT_SOURCE,cf_cv_default_source,[
			CF_APPEND_TEXT(CPPFLAGS,-D_GNU_SOURCE)
			AC_TRY_COMPILE([#include <sys/types.h>],[
				#ifdef _DEFAULT_SOURCE
				#error	expected _DEFAULT_SOURCE to be undefined
				#endif],
				[cf_cv_default_source=no],
				[cf_cv_default_source=yes])
			])
			if test "$cf_cv_default_source" = yes
			then
				CF_APPEND_TEXT(CPPFLAGS,-D_DEFAULT_SOURCE)
			fi
		fi
	fi

fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_HEADER_PATH version: 15 updated: 2021/01/01 13:31:04
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
		case "$cf_header_path" in
		(-I*)
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
	test -d "$includedir" &&    $1="[$]$1 $includedir"
	test -d "$includedir/$2" && $1="[$]$1 $includedir/$2"
}

test "$oldincludedir" != NONE && \
test "$oldincludedir" != "/usr/include" && \
test -d "$oldincludedir" && {
	test -d "$oldincludedir"    && $1="[$]$1 $oldincludedir"
	test -d "$oldincludedir/$2" && $1="[$]$1 $oldincludedir/$2"
}

$1="[$]$1 $cf_header_path_list"
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_HEADER_RESOURCE version: 6 updated: 2023/12/09 10:54:48
dnl ------------------
dnl On SunOS, struct rusage is referred to in <sys/wait.h>.  struct rusage is
dnl defined in <sys/resource.h>.  On SCO v4, resource.h needs time.h, which we
dnl may have excluded.
AC_DEFUN([CF_HEADER_RESOURCE],[
AC_REQUIRE([CF_HEADER_SELECT])
AC_CACHE_CHECK(if we may include sys/resource.h with sys/wait.h,
cf_cv_resource_with_wait,[
AC_TRY_COMPILE([
#if defined(HAVE_SYS_TIME_H) && (defined(SELECT_WITH_TIME) || !(defined(HAVE_SELECT_H) || defined(HAVE_SYS_SELECT_H)))
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
test $cf_cv_resource_with_wait = yes && AC_DEFINE(RESOURCE_WITH_WAIT,1,[Define to 1 if we may include sys/resource.h with sys/wait.h])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_HEADER_SELECT version: 5 updated: 2012/10/06 11:17:15
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
test $cf_cv_select_with_time = yes && AC_DEFINE(SELECT_WITH_TIME,1,[Define to 1 if we can include select.h with time.h])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_HELP_MESSAGE version: 4 updated: 2019/12/31 08:53:54
dnl ---------------
dnl Insert text into the help-message, for readability, from AC_ARG_WITH.
AC_DEFUN([CF_HELP_MESSAGE],
[CF_ACVERSION_CHECK(2.53,[],[
AC_DIVERT_HELP($1)])dnl
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_IMAKE_CFLAGS version: 34 updated: 2020/12/31 18:40:20
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

case $IMAKE in
(*/imake)
	cf_imake_opts="-DUseInstalled=YES"
	;;
(*/util/xmkmf)
	# A single parameter tells xmkmf where the config-files are:
	cf_imake_opts="`echo "$IMAKE"|sed -e s,/config/util/xmkmf,,`"
	;;
(*)
	cf_imake_opts=
	;;
esac

# If it's installed properly, imake (or its wrapper, xmkmf) will point to the
# config directory.
if mkdir conftestdir; then
	CDPATH=; export CDPATH
	cf_makefile=`cd "$srcdir" || exit;pwd`/Imakefile
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
	test -f "$cf_makefile" && cat "$cf_makefile" >>./Imakefile

	cat >> ./Imakefile <<'CF_EOF'
findstddefs:
	@echo IMAKE ${ALLDEFINES}ifelse([$1],,,[ $1])       | sed -f fix_cflags.sed
	@echo IMAKE ${EXTRA_LOAD_FLAGS}ifelse([$2],,,[ $2]) | sed -f fix_lflags.sed
CF_EOF

	if ( $IMAKE "$cf_imake_opts" 1>/dev/null 2>&AC_FD_CC && test -f Makefile)
	then
		CF_VERBOSE(Using $IMAKE $cf_imake_opts)
	else
		# sometimes imake doesn't have the config path compiled in.  Find it.
		cf_config=
		for cf_libpath in $X_LIBS $LIBS ; do
			case "$cf_libpath" in
			(-L*)
				cf_libpath=`echo ".$cf_libpath" | sed -e 's/^...//'`
				cf_libpath="$cf_libpath/X11/config"
				if test -d "$cf_libpath" ; then
					cf_config="$cf_libpath"
					break
				fi
				;;
			esac
		done
		if test -z "$cf_config" ; then
			AC_MSG_WARN(Could not find imake config-directory)
		else
			cf_imake_opts="$cf_imake_opts -I$cf_config"
			if ( "$IMAKE" -v "$cf_imake_opts" 2>&AC_FD_CC)
			then
				CF_VERBOSE(Using $IMAKE $cf_config)
			else
				AC_MSG_WARN(Cannot run $IMAKE)
			fi
		fi
	fi

	# GNU make sometimes prints "make[1]: Entering...", which
	# would confuse us.
	eval "`make findstddefs 2>/dev/null | grep -v make`"

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
		    (-nostdinc)
			cf_nostdinc="$cf_opt"
			;;
		    (-I/usr/include)
			cf_std_incl="$cf_opt"
			;;
		    (*)
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
(*-DPROJECTROOT=/*)
	;;
(*)
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
dnl CF_INSTALL_OPTS version: 3 updated: 2023/06/03 15:17:30
dnl ---------------
dnl prompt for/fill-in useful install-program options
AC_DEFUN([CF_INSTALL_OPTS],
[
CF_INSTALL_OPT_S
CF_INSTALL_OPT_P
CF_INSTALL_OPT_O
CF_INSTALL_OPT_STRIP_PROG
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_INSTALL_OPT_O version: 3 updated: 2020/12/31 20:19:42
dnl ----------------
dnl Almost all "install" programs default to the current user's ownership.
dnl Almost - MINIX is an exception.
AC_DEFUN([CF_INSTALL_OPT_O],
[
AC_MSG_CHECKING(if install needs to be told about ownership)
case `$ac_config_guess` in
(*minix)
	with_install_o=yes
	;;
(*)
	with_install_o=no
	;;
esac

AC_MSG_RESULT($with_install_o)
if test "x$with_install_o" = xyes
then
	INSTALL_OPT_O="`id root|sed -e 's/uid=[[0-9]]*(/ -o /' -e 's/gid=[[0-9]]*(/ -g /' -e 's/ [[^=[:space:]]][[^=[:space:]]]*=.*/ /' -e 's/)//g'`"
else
	INSTALL_OPT_O=
fi

AC_SUBST(INSTALL_OPT_O)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_INSTALL_OPT_P version: 3 updated: 2021/01/01 13:31:04
dnl ----------------
dnl Some install-programs accept a "-p" option to preserve file modification
dnl timestamps.  That can be useful as an install option, as well as a way to
dnl avoid the need for ranlib after copying a static archive.
AC_DEFUN([CF_INSTALL_OPT_P],
[
: "${INSTALL:=install}"
AC_CACHE_CHECK(if install accepts -p option, cf_cv_install_p,[
	rm -rf ./conftest*
	date >conftest.in
	mkdir conftest.out
	sleep 3
	if $INSTALL -p conftest.in conftest.out 2>/dev/null
	then
		if test -f conftest.out/conftest.in
		then
			test conftest.in -nt conftest.out/conftest.in 2>conftest.err && \
			test conftest.out/conftest.in -nt conftest.in 2>conftest.err
			if test -s conftest.err
			then
				cf_cv_install_p=no
			else
				cf_cv_install_p=yes
			fi
		else
			cf_cv_install_p=no
		fi
	else
		cf_cv_install_p=no
	fi
	rm -rf ./conftest*
])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_INSTALL_OPT_S version: 3 updated: 2021/01/05 19:23:48
dnl ----------------
dnl By default, we should strip executables which are installed, but leave the
dnl ability to suppress that for unit-testing.
AC_DEFUN([CF_INSTALL_OPT_S],
[
AC_MSG_CHECKING(if you want to install stripped executables)
CF_ARG_DISABLE(stripping,
	[  --disable-stripping     do not strip (debug info) installed executables],
	[enable_stripping=no],
	[enable_stripping=yes])
AC_MSG_RESULT($enable_stripping)

if test "$enable_stripping" = yes
then
	INSTALL_OPT_S="-s"
else
	INSTALL_OPT_S=
fi
AC_SUBST(INSTALL_OPT_S)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_INSTALL_OPT_STRIP_PROG version: 1 updated: 2023/06/03 15:17:30
dnl -------------------------
dnl Provide an option for overriding the strip program used in install "-s"
dnl
dnl coreutils install provides a --strip-program option
dnl FreeBSD uses STRIPBIN environment variable, while NetBSD and OpenBSD use
dnl STRIP environment variable.  Other versions of install do not support this.
AC_DEFUN([CF_INSTALL_OPT_STRIP_PROG],
[
AC_REQUIRE([CF_INSTALL_OPT_S])
if test -n "$INSTALL_OPT_S"
then
	AC_MSG_CHECKING(if you want to specify strip-program)
	AC_ARG_WITH(strip-program,
		[  --with-strip-program=XX specify program to use when stripping in install],
		[with_strip_program=$withval],
		[with_strip_program=no])
	AC_MSG_RESULT($with_strip_program)
	if test "$with_strip_program" != no
	then
		AC_MSG_CHECKING(if strip-program is supported with this installer)
		cf_install_program=`echo "$INSTALL" | sed -e 's%[[ ]]*[[ ]]-.%%'`
		check_install_strip=no
		if test -f "$cf_install_program"
		then
			check_install_version=`"$cf_install_program" --version 2>/dev/null | head -n 1 | grep coreutils`
			if test -n "$check_install_version"
			then
				check_install_strip="option"
			else
				for check_strip_variable in STRIPBIN STRIP
				do
					if strings "$cf_install_program" | grep "^$check_strip_variable[$]" >/dev/null
					then
						check_install_strip="environ"
						break
					fi
				done
			fi
		fi
		AC_MSG_RESULT($check_install_strip)
		case "$check_install_strip" in
		(no)
			AC_MSG_WARN($cf_install_program does not support strip program option)
			with_strip_program=no
			;;
		(environ)
			cat >install.tmp <<-CF_EOF
			#! $SHELL
			STRIPBIN="$with_strip_program" \\
			STRIP="$with_strip_program" \\
			$INSTALL "[$]@"
			CF_EOF
			INSTALL="`pwd`/install.tmp"
			chmod +x "$INSTALL"
			CF_VERBOSE(created $INSTALL)
			;;
		(option)
			INSTALL_OPT_S="$INSTALL_OPT_S --strip-program=\"$with_strip_program\""
			;;
		esac
	fi
fi
AC_SUBST(INSTALL_OPT_S)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_INTEL_COMPILER version: 9 updated: 2023/02/18 17:41:25
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
AC_REQUIRE([AC_CANONICAL_HOST])
ifelse([$2],,INTEL_COMPILER,[$2])=no

if test "$ifelse([$1],,[$1],GCC)" = yes ; then
	case "$host_os" in
	(linux*|gnu*)
		AC_MSG_CHECKING(if this is really Intel ifelse([$1],GXX,C++,C) compiler)
		cf_save_CFLAGS="$ifelse([$3],,CFLAGS,[$3])"
		ifelse([$3],,CFLAGS,[$3])="$ifelse([$3],,CFLAGS,[$3]) -no-gcc"
		AC_TRY_COMPILE([],[
#ifdef __INTEL_COMPILER
#else
#error __INTEL_COMPILER is not defined
#endif
],[ifelse([$2],,INTEL_COMPILER,[$2])=yes
cf_save_CFLAGS="$cf_save_CFLAGS -we147"
],[])
		ifelse([$3],,CFLAGS,[$3])="$cf_save_CFLAGS"
		AC_MSG_RESULT($ifelse([$2],,INTEL_COMPILER,[$2]))
		;;
	esac
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_KILLPG version: 11 updated: 2023/12/09 10:53:57
dnl ---------
dnl Note: relies upon AC_FUNC_SETPGRP, but cannot use AC_REQUIRE, since that
dnl messes up the messages...
define([CF_KILLPG],
[
if test "$cross_compiling" = yes ; then
	AC_CHECK_FUNC(setpgrp,[
	AC_TRY_COMPILE([
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <signal.h>
],[
    (void) setpgrp();
],
	[AC_DEFINE(SETPGRP_VOID)])])
else
	AC_FUNC_SETPGRP
fi

if test "$cross_compiling" = yes ; then
	AC_CHECK_FUNC(getpgrp,[
	AC_TRY_COMPILE([
$ac_includes_default

#include <signal.h>
],[
    (void) getpgrp();
],
	[AC_DEFINE(GETPGRP_VOID)])])
else
	AC_FUNC_GETPGRP
fi

AC_CACHE_CHECK(if killpg is needed, cf_cv_need_killpg,[
AC_TRY_RUN([
$ac_includes_default

#include <signal.h>

static RETSIGTYPE
handler(int s)
{
	(void) s;
    exit(0);
}

int main(void)
{
#ifdef SETPGRP_VOID
    (void) setpgrp();
#else
    (void) setpgrp(0,0);
#endif
    (void) signal(SIGINT, handler);
    (void) kill(-getpid(), SIGINT);
    ${cf_cv_main_return:-return}(1);
}],
	[cf_cv_need_killpg=no],
	[cf_cv_need_killpg=yes],
	[cf_cv_need_killpg=unknown])
])

test $cf_cv_need_killpg = yes && AC_DEFINE(HAVE_KILLPG)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LARGEFILE version: 13 updated: 2023/12/03 19:09:59
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
	if test "$ac_cv_sys_large_files" != no
	then
		CF_APPEND_TEXT(CPPFLAGS,-D_LARGE_FILES)
	fi
	if test "$ac_cv_sys_largefile_source" != no
	then
		CF_APPEND_TEXT(CPPFLAGS,-D_LARGEFILE_SOURCE)
	fi
	if test "$ac_cv_sys_file_offset_bits" != no
	then
		CF_APPEND_TEXT(CPPFLAGS,-D_FILE_OFFSET_BITS=$ac_cv_sys_file_offset_bits)
	fi

	AC_CACHE_CHECK(whether to use struct dirent64, cf_cv_struct_dirent64,[
		AC_TRY_COMPILE([
#pragma GCC diagnostic error "-Wincompatible-pointer-types"
#include <sys/types.h>
#include <dirent.h>

#ifndef __REDIRECT
/* if transitional largefile support is setup, this is true */
extern struct dirent64 * readdir(DIR *);
#endif
		],[
		DIR *dp = opendir(".");
		struct dirent64 *x = readdir(dp);
		struct dirent *y = readdir(dp);
		int z = x - y;
		(void)z;
		],
		[cf_cv_struct_dirent64=yes],
		[cf_cv_struct_dirent64=no])
	])
	test "$cf_cv_struct_dirent64" = yes && AC_DEFINE(HAVE_STRUCT_DIRENT64,1,[Define to 1 if we have struct dirent64])
	fi
])
])
dnl ---------------------------------------------------------------------------
dnl CF_LD_RPATH_OPT version: 9 updated: 2021/01/01 13:31:04
dnl ---------------
dnl For the given system and compiler, find the compiler flags to pass to the
dnl loader to use the "rpath" feature.
AC_DEFUN([CF_LD_RPATH_OPT],
[
AC_REQUIRE([CF_CHECK_CACHE])

LD_RPATH_OPT=
if test "x$cf_cv_enable_rpath" != xno
then
	AC_MSG_CHECKING(for an rpath option)
	case "$cf_cv_system_name" in
	(irix*)
		if test "$GCC" = yes; then
			LD_RPATH_OPT="-Wl,-rpath,"
		else
			LD_RPATH_OPT="-rpath "
		fi
		;;
	(linux*|gnu*|k*bsd*-gnu|freebsd*)
		LD_RPATH_OPT="-Wl,-rpath,"
		;;
	(openbsd[[2-9]].*|mirbsd*)
		LD_RPATH_OPT="-Wl,-rpath,"
		;;
	(dragonfly*)
		LD_RPATH_OPT="-rpath "
		;;
	(netbsd*)
		LD_RPATH_OPT="-Wl,-rpath,"
		;;
	(osf*|mls+*)
		LD_RPATH_OPT="-rpath "
		;;
	(solaris2*)
		LD_RPATH_OPT="-R"
		;;
	(*)
		;;
	esac
	AC_MSG_RESULT($LD_RPATH_OPT)

	case "x$LD_RPATH_OPT" in
	(x-R*)
		AC_MSG_CHECKING(if we need a space after rpath option)
		cf_save_LIBS="$LIBS"
		CF_ADD_LIBS(${LD_RPATH_OPT}$libdir)
		AC_TRY_LINK(, , cf_rpath_space=no, cf_rpath_space=yes)
		LIBS="$cf_save_LIBS"
		AC_MSG_RESULT($cf_rpath_space)
		test "$cf_rpath_space" = yes && LD_RPATH_OPT="$LD_RPATH_OPT "
		;;
	esac
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LEX_CHAR_CLASSES version: 7 updated: 2020/01/05 20:03:16
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
rm -rf conftest.* $LEX_OUTPUT_ROOT.c
if test "$LEX_CHAR_CLASSES" != yes ; then
	AC_MSG_WARN(Your $LEX program does not support POSIX character classes.  Get flex.)
fi
AC_SUBST(LEX_CHAR_CLASSES)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LEX_OPTIONS version: 2 updated: 2020/01/05 20:03:16
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
rm -rf conftest.* $LEX_OUTPUT_ROOT.c
if test "$cf_lex_options" != yes ; then
	AC_MSG_WARN(Your $LEX program does not support POSIX options.  Get flex.)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LEX_POINTER version: 2 updated: 2020/01/05 20:03:16
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
rm -rf conftest.* $LEX_OUTPUT_ROOT.c
if test "$cf_lex_pointer" != yes ; then
	AC_MSG_WARN(Your $LEX program does not support POSIX %pointer.  Get flex.)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LEX_STATES version: 8 updated: 2020/12/31 20:19:42
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
while test "$cf_lex_statenum" -lt $cf_lex_statemax
do
cf_lex_statenum="`expr "$cf_lex_statenum" + 1`"
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
		if test "$cf_lex_statenum" = "$cf_lex_statemax" ; then
			cf_lex_states=yes
			break
		else
			cf_lex_states="$cf_lex_statenum"
		fi
	else
		break
	fi
done
AC_MSG_RESULT($cf_lex_states)
rm -rf conftest.* $LEX_OUTPUT_ROOT.c
if test "$cf_lex_states" = no ; then
	AC_MSG_WARN(Your $LEX program does not support POSIX states.  Get flex.)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LEX_VERSION version: 7 updated: 2021/01/03 19:29:49
dnl --------------
dnl Check if "lex" is really "flex", and if so, set $LEX_VERSION to show its
dnl version.
AC_DEFUN([CF_LEX_VERSION],[
AC_REQUIRE([AC_PROG_FGREP])dnl
AC_REQUIRE([AC_PROG_LEX])dnl

test -z "$LEX" && LEX="lex"
test "x$LEX" = "x:" && LEX="lex"  # : is not a usable default
AC_MSG_CHECKING(if $LEX is really flex)
if ( $LEX '-?' </dev/null 2>&1 |${FGREP-fgrep} flex >/dev/null )
then
	AC_MSG_RESULT(yes)
	AC_MSG_CHECKING(version of $LEX)
	LEX_VERSION="`$LEX --version </dev/null 2>&1 | sed -e 's/^.* \([[0-9]]\)/\1/;s/ .*$//;s/^[[^0-9]]*//'`"
	AC_MSG_RESULT($LEX_VERSION)
else
	AC_MSG_RESULT(no)
fi

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LIBRARY_PATH version: 11 updated: 2021/01/01 13:31:04
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
		case "$cf_library_path" in
		(-L*)
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
dnl CF_LIB_PREFIX version: 14 updated: 2021/01/01 13:31:04
dnl -------------
dnl Compute the library-prefix for the given host system
dnl $1 = variable to set
define([CF_LIB_PREFIX],
[
	case "$cf_cv_system_name" in
	(OS/2*|os2*)
		if test "$DFT_LWR_MODEL" = libtool; then
			LIB_PREFIX='lib'
		else
			LIB_PREFIX=''
		fi
		;;
	(*-msvc*)
		LIB_PREFIX=''
		;;
	(*)	LIB_PREFIX='lib'
		;;
	esac
ifelse($1,,,[$1=$LIB_PREFIX])
	AC_SUBST(LIB_PREFIX)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LINK_PREFIX version: 2 updated: 2021/01/06 16:19:35
dnl --------------
dnl Use xterm's plink.sh script as a link-prefix, to trim unneeded libraries.
dnl This is optional since in some obscure cases of weak-linkage it may be
dnl possible to trim too much.
AC_DEFUN([CF_LINK_PREFIX],
[
AC_MSG_CHECKING(if you want to trim unneeded libraries)
CF_ARG_DISABLE(link-prefix,
	[  --disable-link-prefix   do not trim unneeded libraries from link command],
	[enable_link_prefix=no],
	[enable_link_prefix=yes])
AC_MSG_RESULT($enable_link_prefix)
if test $enable_link_prefix = yes
then
	LINK_PREFIX='$(SHELL) $(top_srcdir)/plink.sh'
else
	LINK_PREFIX=
fi
AC_SUBST(LINK_PREFIX)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LOCALE version: 7 updated: 2023/01/11 04:05:23
dnl ---------
dnl Check if we have setlocale() and its header, <locale.h>
dnl The optional parameter $1 tells what to do if we do have locale support.
AC_DEFUN([CF_LOCALE],
[
AC_MSG_CHECKING(for setlocale())
AC_CACHE_VAL(cf_cv_locale,[
AC_TRY_LINK([
$ac_includes_default
#include <locale.h>],
	[setlocale(LC_ALL, "")],
	[cf_cv_locale=yes],
	[cf_cv_locale=no])
	])
AC_MSG_RESULT($cf_cv_locale)
test "$cf_cv_locale" = yes && { ifelse($1,,AC_DEFINE(LOCALE,1,[Define to 1 if we have locale support]),[$1]) }
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MAKEFLAGS version: 21 updated: 2021/09/04 06:47:34
dnl ------------
dnl Some 'make' programs support ${MAKEFLAGS}, some ${MFLAGS}, to pass 'make'
dnl options to lower-levels.  It is very useful for "make -n" -- if we have it.
dnl (GNU 'make' does both, something POSIX 'make', which happens to make the
dnl ${MAKEFLAGS} variable incompatible because it adds the assignments :-)
AC_DEFUN([CF_MAKEFLAGS],
[AC_REQUIRE([AC_PROG_FGREP])dnl

AC_CACHE_CHECK(for makeflags variable, cf_cv_makeflags,[
	cf_cv_makeflags=''
	for cf_option in '-${MAKEFLAGS}' '${MFLAGS}'
	do
		cat >cf_makeflags.tmp <<CF_EOF
SHELL = $SHELL
all :
	@ echo '.$cf_option'
CF_EOF
		cf_result=`${MAKE:-make} -k -f cf_makeflags.tmp 2>/dev/null | ${FGREP-fgrep} -v "ing directory" | sed -e 's,[[ 	]]*$,,'`
		case "$cf_result" in
		(.*k|.*kw)
			cf_result="`${MAKE:-make} -k -f cf_makeflags.tmp CC=cc 2>/dev/null`"
			case "$cf_result" in
			(.*CC=*)	cf_cv_makeflags=
				;;
			(*)	cf_cv_makeflags=$cf_option
				;;
			esac
			break
			;;
		(.-)
			;;
		(*)
			CF_MSG_LOG(given option \"$cf_option\", no match \"$cf_result\")
			;;
		esac
	done
	rm -f cf_makeflags.tmp
])

AC_SUBST(cf_cv_makeflags)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MAKE_PHONY version: 3 updated: 2021/01/08 16:08:21
dnl -------------
dnl Check if the make-program handles a ".PHONY" target, e.g,. a target which
dnl acts as a placeholder.
dnl
dnl The ".PHONY" feature was proposed in 2011 here
dnl     https://www.austingroupbugs.net/view.php?id=523
dnl and is scheduled for release in P1003.1 Issue 8 (late 2022).
dnl
dnl This is not supported by SVr4 make (or SunOS 4, 4.3SD, etc), but works with
dnl a few others (i.e., GNU make and the non-POSIX "BSD" make):
dnl
dnl + This is a GNU make feature (since April 1988, but in turn from binutils,
dnl   date unspecified).
dnl
dnl + It was adopted in NetBSD make in June 1995.
dnl
dnl + The other BSD make programs are derived from the NetBSD make (and for
dnl   that reason are not actually different "implementations").
dnl
dnl + Some features of NetBSD make were actually adapted from pmake, which
dnl   began as a modified GNU make starting in 1993.
dnl
dnl + Version 3.8 of the dmake program in January 1992 also implemented this
dnl   GNU make extension, but is less well known than the BSD make.
AC_DEFUN([CF_MAKE_PHONY],[
AC_CACHE_CHECK(for \".PHONY\" make-support, cf_cv_make_PHONY,[
	rm -rf conftest*
	(
		mkdir conftest || exit 1
		cd conftest
		cat >makefile <<'CF_EOF'
.PHONY: always
DATA=0
always:	always.out
	@echo "** making [$]@ [$](DATA)"
once: once.out
	@echo "** making [$]@ [$](DATA)"
always.out:
	@echo "** making [$]@ [$](DATA)"
	echo [$](DATA) > [$]@
once.out:
	@echo "** making [$]@ [$](DATA)"
	echo [$](DATA) > [$]@
CF_EOF
		for cf_data in 1 2 3
		do
			${MAKE:-make} always DATA=$cf_data
			${MAKE:-make} once   DATA=$cf_data
			${MAKE:-make} -t always once
			if test -f always ; then
				echo "no (case 1)" > ../conftest.tmp
			elif test ! -f always.out ; then
				echo "no (case 2)" > ../conftest.tmp
			elif test ! -f once.out ; then
				echo "no (case 3)" > ../conftest.tmp
			elif ! cmp -s always.out once.out ; then
				echo "no (case 4)" > ../conftest.tmp
				diff always.out once.out
			else
				cf_check="`cat always.out`"
				if test "x$cf_check" != "x$cf_data" ; then
					echo "no (case 5)" > ../conftest.tmp
				else
					echo yes > ../conftest.tmp
					rm -f ./*.out
					continue
				fi
			fi
			break
		done
	) >&AC_FD_CC 2>&1
	cf_cv_make_PHONY="`cat conftest.tmp`"
	rm -rf conftest*
])
MAKE_NO_PHONY="#"
MAKE_PHONY="#"
test "x$cf_cv_make_PHONY" = xyes && MAKE_PHONY=
test "x$cf_cv_make_PHONY" != xyes && MAKE_NO_PHONY=
AC_SUBST(MAKE_NO_PHONY)
AC_SUBST(MAKE_PHONY)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MAKE_TAGS version: 6 updated: 2010/10/23 15:52:32
dnl ------------
dnl Generate tags/TAGS targets for makefiles.  Do not generate TAGS if we have
dnl a monocase filesystem.
AC_DEFUN([CF_MAKE_TAGS],[
AC_REQUIRE([CF_MIXEDCASE_FILENAMES])

AC_CHECK_PROGS(CTAGS, exctags ctags)
AC_CHECK_PROGS(ETAGS, exetags etags)

AC_CHECK_PROG(MAKE_LOWER_TAGS, ${CTAGS:-ctags}, yes, no)

if test "$cf_cv_mixedcase" = yes ; then
	AC_CHECK_PROG(MAKE_UPPER_TAGS, ${ETAGS:-etags}, yes, no)
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
dnl CF_MERGE_EXTRA_CFLAGS version: 2 updated: 2021/01/02 09:31:20
dnl ---------------------
dnl CF_FIX_WARNINGS moves problematic flags into EXTRA_CFLAGS, but some scripts
dnl may depend on being able to override that variable at build-time.  Move it
dnl all back.
define([CF_MERGE_EXTRA_CFLAGS],[
if test "$GCC" = yes || test "$GXX" = yes
then
	CF_APPEND_TEXT(CFLAGS,$EXTRA_CFLAGS)
	EXTRA_CFLAGS=
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_MISSING_CHECK version: 9 updated: 2023/01/15 05:38:19
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
extern struct zowie *$1(struct zowie *);
],
[
#ifdef HAVE_LIBXT		/* needed for SunOS 4.0.3 or 4.1 */
extern void XtToolkitInitialize(void);
#endif
],
[eval 'cf_cv_func_'"$1"'=yes'],
[eval 'cf_cv_func_'"$1"'=no'])
CPPFLAGS="$cf_save_CPPFLAGS"
])
eval 'cf_result="$cf_cv_func_'"$1"\"
AC_MSG_RESULT($cf_result)
test "$cf_result" = yes && AC_DEFINE_UNQUOTED(MISSING_EXTERN_$2)
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
dnl CF_MIXEDCASE_FILENAMES version: 9 updated: 2021/01/01 16:53:59
dnl ----------------------
dnl Check if the file-system supports mixed-case filenames.  If we're able to
dnl create a lowercase name and see it as uppercase, it doesn't support that.
AC_DEFUN([CF_MIXEDCASE_FILENAMES],
[
AC_CACHE_CHECK(if filesystem supports mixed-case filenames,cf_cv_mixedcase,[
if test "$cross_compiling" = yes ; then
	case "$target_alias" in
	(*-os2-emx*|*-msdosdjgpp*|*-cygwin*|*-msys*|*-mingw*|*-uwin*|darwin*)
		cf_cv_mixedcase=no
		;;
	(*)
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
test "$cf_cv_mixedcase" = yes && AC_DEFINE(MIXEDCASE_FILENAMES,1,[Define to 1 if filesystem supports mixed-case filenames.])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MKSTEMP version: 13 updated: 2023/12/01 17:22:50
dnl ----------
dnl Check for a working mkstemp.  This creates two files, checks that they are
dnl successfully created and distinct (AmigaOS apparently fails on the last).
AC_DEFUN([CF_MKSTEMP],[
AC_CHECK_HEADERS( \
unistd.h \
)
AC_CACHE_CHECK(for working mkstemp, cf_cv_func_mkstemp,[
rm -rf ./conftest*
AC_TRY_RUN([
$ac_includes_default

int main(void)
{
	static char tmpl[] = "conftestXXXXXX";
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
],[cf_cv_func_mkstemp=maybe])
])
if test "x$cf_cv_func_mkstemp" = xmaybe ; then
	AC_CHECK_FUNC(mkstemp)
fi
if test "x$cf_cv_func_mkstemp" = xyes || test "x$ac_cv_func_mkstemp" = xyes ; then
	AC_DEFINE(HAVE_MKSTEMP,1,[Define to 1 if mkstemp() is available and working.])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MSG_LOG version: 5 updated: 2010/10/23 15:52:32
dnl ----------
dnl Write a debug message to config.log, along with the line number in the
dnl configure script.
AC_DEFUN([CF_MSG_LOG],[
echo "${as_me:-configure}:__oline__: testing $* ..." 1>&AC_FD_CC
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_CC_CHECK version: 6 updated: 2023/02/18 17:47:58
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
	#error WACS_BSSB is not defined
#endif
])[
printf("%s\\n", NCURSES_VERSION);
#else
#ifdef __NCURSES_H
printf("old\\n");
#else
	#error __NCURSES_H is not defined
#endif
#endif
	]
	,[$1=$2]
	,[$1=no])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_CONFIG version: 29 updated: 2025/01/10 19:55:54
dnl -----------------
dnl Tie together the configure-script macros for ncurses, preferring these in
dnl order:
dnl a) ".pc" files for pkg-config, using $NCURSES_CONFIG_PKG
dnl b) the "-config" script from ncurses, using $NCURSES_CONFIG
dnl c) just plain libraries
dnl
dnl $1 is the root library name (default: "ncurses")
AC_DEFUN([CF_NCURSES_CONFIG],[
AC_REQUIRE([CF_PKG_CONFIG])
cf_ncuconfig_root=ifelse($1,,ncurses,$1)
cf_have_ncuconfig=no

if test "x${PKG_CONFIG:=none}" != xnone; then
	AC_MSG_CHECKING(pkg-config for $cf_ncuconfig_root)
	if "$PKG_CONFIG" --exists $cf_ncuconfig_root ; then
		AC_MSG_RESULT(yes)

		AC_MSG_CHECKING(if the $cf_ncuconfig_root package files work)
		cf_have_ncuconfig=unknown

		cf_save_CFLAGS="$CFLAGS"
		cf_save_CPPFLAGS="$CPPFLAGS"
		cf_save_LIBS="$LIBS"

		cf_pkg_cflags="`$PKG_CONFIG --cflags $cf_ncuconfig_root`"
		cf_pkg_libs="`$PKG_CONFIG --libs $cf_ncuconfig_root`"

		# while -W for passing linker flags is prevalent, it is not "standard".
		# At least one wrapper for c89/c99 (in Apple's xcode) has its own
		# incompatible _and_ non-standard -W option which gives an error.  Work
		# around that pitfall.
		case "x${CC}@@${cf_pkg_libs}@${cf_pkg_cflags}" in
		(x*c[[89]]9@@*-W*)
			CF_ADD_CFLAGS($cf_pkg_cflags)
			CF_ADD_LIBS($cf_pkg_libs)

			AC_TRY_LINK([#include <${cf_cv_ncurses_header:-curses.h}>],
				[initscr(); mousemask(0,0); tigetstr((char *)0);],
				[AC_TRY_RUN([#include <${cf_cv_ncurses_header:-curses.h}>
					int main(void)
					{ const char *xx = curses_version(); return (xx == 0); }],
					[cf_test_ncuconfig=yes],
					[cf_test_ncuconfig=no],
					[cf_test_ncuconfig=maybe])],
				[cf_test_ncuconfig=no])

			CFLAGS="$cf_save_CFLAGS"
			CPPFLAGS="$cf_save_CPPFLAGS"
			LIBS="$cf_save_LIBS"

			if test "x$cf_test_ncuconfig" != xyes; then
				cf_temp=`echo "x$cf_pkg_cflags" | sed -e s/^x// -e 's/-W[[^ 	]]*//g'`
				cf_pkg_cflags="$cf_temp"
				cf_temp=`echo "x$cf_pkg_libs" | sed -e s/^x// -e 's/-W[[^ 	]]*//g'`
				cf_pkg_libs="$cf_temp"
			fi
			;;
		esac

		CF_REQUIRE_PKG($cf_ncuconfig_root)
		CF_APPEND_CFLAGS($cf_pkg_cflags)
		CF_ADD_LIBS($cf_pkg_libs)

		AC_TRY_LINK([#include <${cf_cv_ncurses_header:-curses.h}>],
			[initscr(); mousemask(0,0); tigetstr((char *)0);],
			[AC_TRY_RUN([#include <${cf_cv_ncurses_header:-curses.h}>
				int main(void)
				{ const char *xx = curses_version(); return (xx == 0); }],
				[cf_have_ncuconfig=yes],
				[cf_have_ncuconfig=no],
				[cf_have_ncuconfig=maybe])],
			[cf_have_ncuconfig=no])
		AC_MSG_RESULT($cf_have_ncuconfig)
		test "$cf_have_ncuconfig" = maybe && cf_have_ncuconfig=yes
		if test "$cf_have_ncuconfig" != "yes"
		then
			CPPFLAGS="$cf_save_CPPFLAGS"
			LIBS="$cf_save_LIBS"
			NCURSES_CONFIG_PKG=none
		else
			AC_DEFINE(NCURSES,1,[Define to 1 if we are using ncurses headers/libraries])
			NCURSES_CONFIG_PKG=$cf_ncuconfig_root
			CF_TERM_HEADER
		fi

	else
		AC_MSG_RESULT(no)
		NCURSES_CONFIG_PKG=none
	fi
else
	NCURSES_CONFIG_PKG=none
fi

if test "x$cf_have_ncuconfig" = "xno"; then
	cf_ncurses_config="${cf_ncuconfig_root}${NCURSES_CONFIG_SUFFIX}-config"; echo "Looking for ${cf_ncurses_config}"

	CF_ACVERSION_CHECK(2.52,
		[AC_CHECK_TOOLS(NCURSES_CONFIG, ${cf_ncurses_config} ${cf_ncuconfig_root}6-config ${cf_ncuconfig_root}6-config ${cf_ncuconfig_root}5-config, none)],
		[AC_PATH_PROGS(NCURSES_CONFIG,  ${cf_ncurses_config} ${cf_ncuconfig_root}6-config ${cf_ncuconfig_root}6-config ${cf_ncuconfig_root}5-config, none)])

	if test "$NCURSES_CONFIG" != none ; then

		CF_APPEND_CFLAGS(`$NCURSES_CONFIG --cflags`)
		CF_ADD_LIBS(`$NCURSES_CONFIG --libs`)

		# even with config script, some packages use no-override for curses.h
		CF_CURSES_HEADER(ifelse($1,,ncurses,$1))

		dnl like CF_NCURSES_CPPFLAGS
		AC_DEFINE(NCURSES,1,[Define to 1 if we are using ncurses headers/libraries])

		dnl like CF_NCURSES_LIBS
		CF_UPPER(cf_nculib_ROOT,HAVE_LIB$cf_ncuconfig_root)
		AC_DEFINE_UNQUOTED($cf_nculib_ROOT)

		dnl like CF_NCURSES_VERSION
		cf_cv_ncurses_version="`$NCURSES_CONFIG --version`"

	else

		CF_NCURSES_CPPFLAGS(ifelse($1,,ncurses,$1))
		CF_NCURSES_LIBS(ifelse($1,,ncurses,$1))

	fi
else
	NCURSES_CONFIG=none
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_CPPFLAGS version: 22 updated: 2021/01/02 09:31:20
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
  CF_ADD_INCDIR($cf_cv_curses_dir/include/$cf_ncuhdr_root)
}

AC_CACHE_CHECK(for $cf_ncuhdr_root header in include-path, cf_cv_ncurses_h,[
	cf_header_list="$cf_ncuhdr_root/curses.h $cf_ncuhdr_root/ncurses.h"
	{ test "$cf_ncuhdr_root" = ncurses || test "$cf_ncuhdr_root" = ncursesw; } && cf_header_list="$cf_header_list curses.h ncurses.h"
	for cf_header in $cf_header_list
	do
		CF_NCURSES_CC_CHECK(cf_cv_ncurses_h,$cf_header,$1)
		test "$cf_cv_ncurses_h" != no && break
	done
])

CF_NCURSES_HEADER
CF_TERM_HEADER

# some applications need this, but should check for NCURSES_VERSION
AC_DEFINE(NCURSES,1,[Define to 1 if we are using ncurses headers/libraries])

CF_NCURSES_VERSION
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_HEADER version: 7 updated: 2021/01/04 19:33:05
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
	test -n "$verbose" && echo "search path $cf_search"
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
				test -n "$verbose" && echo $ECHO_N "	... found $ECHO_C" 1>&AC_FD_MSG
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
	cf_cv_ncurses_header="`basename "$cf_cv_ncurses_h2"`"
	if test "`basename "$cf_1st_incdir"`" = "$cf_ncuhdr_root" ; then
		cf_cv_ncurses_header="$cf_ncuhdr_root/$cf_cv_ncurses_header"
	fi
	CF_ADD_INCDIR($cf_1st_incdir)

fi

# Set definitions to allow ifdef'ing for ncurses.h

case "$cf_cv_ncurses_header" in
(*ncurses.h)
	AC_DEFINE(HAVE_NCURSES_H,1,[Define to 1 if we have ncurses.h])
	;;
esac

case "$cf_cv_ncurses_header" in
(ncurses/curses.h|ncurses/ncurses.h)
	AC_DEFINE(HAVE_NCURSES_NCURSES_H,1,[Define to 1 if we have ncurses/ncurses.h])
	;;
(ncursesw/curses.h|ncursesw/ncurses.h)
	AC_DEFINE(HAVE_NCURSESW_NCURSES_H,1,[Define to 1 if we have ncursesw/ncurses.h])
	;;
esac

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_LIBS version: 21 updated: 2021/09/04 06:37:12
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

case "$host_os" in
(freebsd*)
	# This is only necessary if you are linking against an obsolete
	# version of ncurses (but it should do no harm, since it is static).
	if test "$cf_nculib_root" = ncurses ; then
		AC_CHECK_LIB(mytinfo,tgoto,[cf_ncurses_LIBS="-lmytinfo $cf_ncurses_LIBS"])
	fi
	;;
esac

CF_ADD_LIBS($cf_ncurses_LIBS)

if test -n "$cf_cv_curses_dir" && test "$cf_cv_curses_dir" != "no"
then
	CF_ADD_LIBS(-l$cf_nculib_root)
else
	CF_FIND_LIBRARY($cf_nculib_root,$cf_nculib_root,
		[#include <${cf_cv_ncurses_header:-curses.h}>],
		[initscr()],
		initscr)
fi

if test -n "$cf_ncurses_LIBS" ; then
	AC_MSG_CHECKING(if we can link $cf_nculib_root without $cf_ncurses_LIBS)
	cf_ncurses_SAVE="$LIBS"
	for p in $cf_ncurses_LIBS ; do
		q=`echo "$LIBS" | sed -e "s%$p %%" -e "s%$p$%%"`
		if test "$q" != "$LIBS" ; then
			LIBS="$q"
		fi
	done
	AC_TRY_LINK([#include <${cf_cv_ncurses_header:-curses.h}>],
		[initscr(); mousemask(0,0); tigetstr((char *)0);],
		[AC_MSG_RESULT(yes)],
		[AC_MSG_RESULT(no)
		 LIBS="$cf_ncurses_SAVE"])
fi

CF_UPPER(cf_nculib_ROOT,HAVE_LIB$cf_nculib_root)
AC_DEFINE_UNQUOTED($cf_nculib_ROOT)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_VERSION version: 18 updated: 2024/01/07 06:34:16
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
	rm -f "$cf_tempfile"
	AC_TRY_RUN([
$ac_includes_default

#include <${cf_cv_ncurses_header:-curses.h}>

int main(void)
{
	FILE *fp = fopen("$cf_tempfile", "w");
#ifdef NCURSES_VERSION
# ifdef NCURSES_VERSION_PATCH
	fprintf(fp, "%s.%d\\n", NCURSES_VERSION, NCURSES_VERSION_PATCH);
# else
	fprintf(fp, "%s\\n", NCURSES_VERSION);
# endif
#else
# ifdef __NCURSES_H
	fprintf(fp, "old\\n");
# else
	#error expected ncurses header to define __NCURSES_H
# endif
#endif
	${cf_cv_main_return:-return}(0);
}],[
	cf_cv_ncurses_version=`cat $cf_tempfile`],,[

	# This will not work if the preprocessor splits the line after the
	# Autoconf token.  The 'unproto' program does that.
	cat > "conftest.$ac_ext" <<EOF
#include <${cf_cv_ncurses_header:-curses.h}>
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
		cf_out=`sed -e 's%^Autoconf %%' -e 's%^[[^"]]*"%%' -e 's%".*%%' conftest.out`
		test -n "$cf_out" && cf_cv_ncurses_version="$cf_out"
		rm -f conftest.out
	fi
])
	rm -f "$cf_tempfile"
])
test "$cf_cv_ncurses_version" = no || AC_DEFINE(NCURSES,1,[Define to 1 if we are using ncurses headers/libraries])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NO_LEAKS_OPTION version: 9 updated: 2021/06/13 19:45:41
dnl ------------------
dnl see CF_WITH_NO_LEAKS
dnl
dnl $1 = option/name
dnl $2 = help-text
dnl $3 = symbol to define if the option is set
dnl $4 = additional actions to take if the option is set
AC_DEFUN([CF_NO_LEAKS_OPTION],[
AC_MSG_CHECKING(if you want to use $1 for testing)
AC_ARG_WITH($1,
	[$2],
	[case "x$withval" in
	(x|xno) ;;
	(*)
		: "${with_cflags:=-g}"
		: "${enable_leaks:=no}"
		with_$1=yes
		AC_DEFINE_UNQUOTED($3,1,"Define to 1 if you want to use $1 for testing.")ifelse([$4],,[
	 $4
])
		;;
	esac],
	[with_$1=])
AC_MSG_RESULT(${with_$1:-no})

case ".$with_cflags" in
(.*-g*)
	case .$CFLAGS in
	(.*-g*)
		;;
	(*)
		CF_ADD_CFLAGS([-g])
		;;
	esac
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PATHSEP version: 8 updated: 2021/01/01 13:31:04
dnl ----------
dnl Provide a value for the $PATH and similar separator (or amend the value
dnl as provided in autoconf 2.5x).
AC_DEFUN([CF_PATHSEP],
[
	AC_MSG_CHECKING(for PATH separator)
	case "$cf_cv_system_name" in
	(os2*)	PATH_SEPARATOR=';'  ;;
	(*)	${PATH_SEPARATOR:=':'}  ;;
	esac
ifelse([$1],,,[$1=$PATH_SEPARATOR])
	AC_SUBST(PATH_SEPARATOR)
	AC_MSG_RESULT($PATH_SEPARATOR)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PATH_SYNTAX version: 19 updated: 2024/08/03 13:08:58
dnl --------------
dnl Check the argument to see that it looks like a pathname.  Rewrite it if it
dnl begins with one of the prefix/exec_prefix variables, and then again if the
dnl result begins with 'NONE'.  This is necessary to work around autoconf's
dnl delayed evaluation of those symbols.
AC_DEFUN([CF_PATH_SYNTAX],[
AC_REQUIRE([CF_GLOB_FULLPATH])dnl

if test "x$prefix" != xNONE; then
	cf_path_syntax="$prefix"
else
	cf_path_syntax="$ac_default_prefix"
fi

case "x[$]$1" in
(x\[$]\(*\)*|x\'*\'*)
	;;
(x.|x$GLOB_FULLPATH_POSIX|x$GLOB_FULLPATH_OTHER)
	;;
(x\[$]\{*prefix\}*|x\[$]\{*dir\}*)
	eval $1="[$]$1"
	case "x[$]$1" in
	(xNONE/*)
		$1=`echo "[$]$1" | sed -e s%NONE%$cf_path_syntax%`
		;;
	esac
	;;
(xno|xNONE/*)
	$1=`echo "[$]$1" | sed -e s%NONE%$cf_path_syntax%`
	;;
(*)
	ifelse([$2],,[AC_MSG_ERROR([expected a pathname, not \"[$]$1\"])],$2)
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PKG_CONFIG version: 13 updated: 2023/10/28 11:59:01
dnl -------------
dnl Check for the package-config program, unless disabled by command-line.
dnl
dnl Sets $PKG_CONFIG to the pathname of the pkg-config program.
AC_DEFUN([CF_PKG_CONFIG],
[
AC_MSG_CHECKING(if you want to use pkg-config)
AC_ARG_WITH(pkg-config,
	[[  --with-pkg-config[=CMD] enable/disable use of pkg-config and its name CMD]],
	[cf_pkg_config=$withval],
	[cf_pkg_config=yes])
AC_MSG_RESULT($cf_pkg_config)

case "$cf_pkg_config" in
(no)
	PKG_CONFIG=none
	;;
(yes)
	CF_ACVERSION_CHECK(2.52,
		[AC_PATH_TOOL(PKG_CONFIG, pkg-config, none)],
		[AC_PATH_PROG(PKG_CONFIG, pkg-config, none)])
	;;
(*)
	PKG_CONFIG=$withval
	;;
esac

test -z "$PKG_CONFIG" && PKG_CONFIG=none
if test "$PKG_CONFIG" != none ; then
	CF_PATH_SYNTAX(PKG_CONFIG)
elif test "x$cf_pkg_config" != xno ; then
	AC_MSG_WARN(pkg-config is not installed)
fi

AC_SUBST(PKG_CONFIG)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_POSIX_C_SOURCE version: 12 updated: 2023/02/18 17:41:25
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
[AC_REQUIRE([CF_POSIX_VISIBLE])dnl

if test "$cf_cv_posix_visible" = no; then

cf_POSIX_C_SOURCE=ifelse([$1],,199506L,[$1])

cf_save_CFLAGS="$CFLAGS"
cf_save_CPPFLAGS="$CPPFLAGS"

CF_REMOVE_DEFINE(cf_trim_CFLAGS,$cf_save_CFLAGS,_POSIX_C_SOURCE)
CF_REMOVE_DEFINE(cf_trim_CPPFLAGS,$cf_save_CPPFLAGS,_POSIX_C_SOURCE)

AC_CACHE_CHECK(if we should define _POSIX_C_SOURCE,cf_cv_posix_c_source,[
	CF_MSG_LOG(if the symbol is already defined go no further)
	AC_TRY_COMPILE([#include <sys/types.h>],[
#ifndef _POSIX_C_SOURCE
#error _POSIX_C_SOURCE is not defined
#endif],
	[cf_cv_posix_c_source=no],
	[cf_want_posix_source=no
	 case .$cf_POSIX_C_SOURCE in
	 (.[[12]]??*)
		cf_cv_posix_c_source="-D_POSIX_C_SOURCE=$cf_POSIX_C_SOURCE"
		;;
	 (.2)
		cf_cv_posix_c_source="-D_POSIX_C_SOURCE=$cf_POSIX_C_SOURCE"
		cf_want_posix_source=yes
		;;
	 (.*)
		cf_want_posix_source=yes
		;;
	 esac
	 if test "$cf_want_posix_source" = yes ; then
		AC_TRY_COMPILE([#include <sys/types.h>],[
#ifdef _POSIX_SOURCE
#error _POSIX_SOURCE is defined
#endif],[],
		cf_cv_posix_c_source="$cf_cv_posix_c_source -D_POSIX_SOURCE")
	 fi
	 CF_MSG_LOG(ifdef from value $cf_POSIX_C_SOURCE)
	 CFLAGS="$cf_trim_CFLAGS"
	 CPPFLAGS="$cf_trim_CPPFLAGS"
	 CF_APPEND_TEXT(CPPFLAGS,$cf_cv_posix_c_source)
	 CF_MSG_LOG(if the second compile does not leave our definition intact error)
	 AC_TRY_COMPILE([#include <sys/types.h>],[
#ifndef _POSIX_C_SOURCE
#error _POSIX_C_SOURCE is not defined
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

fi # cf_cv_posix_visible

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_POSIX_VISIBLE version: 1 updated: 2018/12/31 20:46:17
dnl ----------------
dnl POSIX documents test-macros which an application may set before any system
dnl headers are included to make features available.
dnl
dnl Some BSD platforms (originally FreeBSD, but copied by a few others)
dnl diverged from POSIX in 2002 by setting symbols which make all of the most
dnl recent features visible in the system header files unless the application
dnl overrides the corresponding test-macros.  Doing that introduces portability
dnl problems.
dnl
dnl This macro makes a special check for the symbols used for this, to avoid a
dnl conflicting definition.
AC_DEFUN([CF_POSIX_VISIBLE],
[
AC_CACHE_CHECK(if the POSIX test-macros are already defined,cf_cv_posix_visible,[
AC_TRY_COMPILE([#include <stdio.h>],[
#if defined(__POSIX_VISIBLE) && ((__POSIX_VISIBLE - 0L) > 0) \
	&& defined(__XSI_VISIBLE) && ((__XSI_VISIBLE - 0L) > 0) \
	&& defined(__BSD_VISIBLE) && ((__BSD_VISIBLE - 0L) > 0) \
	&& defined(__ISO_C_VISIBLE) && ((__ISO_C_VISIBLE - 0L) > 0)
#error conflicting symbols found
#endif
],[cf_cv_posix_visible=no],[cf_cv_posix_visible=yes])
])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PROG_AR version: 1 updated: 2009/01/01 20:15:22
dnl ----------
dnl Check for archiver "ar".
AC_DEFUN([CF_PROG_AR],[
AC_CHECK_TOOL(AR, ar, ar)
])
dnl ---------------------------------------------------------------------------
dnl CF_PROG_CC version: 5 updated: 2019/12/31 08:53:54
dnl ----------
dnl standard check for CC, plus followup sanity checks
dnl $1 = optional parameter to pass to AC_PROG_CC to specify compiler name
AC_DEFUN([CF_PROG_CC],[
CF_ACVERSION_CHECK(2.53,
	[AC_MSG_WARN(this will incorrectly handle gnatgcc choice)
	 AC_REQUIRE([AC_PROG_CC])],
	[])
ifelse($1,,[AC_PROG_CC],[AC_PROG_CC($1)])
CF_GCC_VERSION
CF_ACVERSION_CHECK(2.52,
	[AC_PROG_CC_STDC],
	[CF_ANSI_CC_REQD])
CF_CC_ENV_FLAGS
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PROG_EXT version: 15 updated: 2021/01/02 09:31:20
dnl -----------
dnl Compute $PROG_EXT, used for non-Unix ports, such as OS/2 EMX.
AC_DEFUN([CF_PROG_EXT],
[
AC_REQUIRE([CF_CHECK_CACHE])
case "$cf_cv_system_name" in
(os2*)
	CFLAGS="$CFLAGS -Zmt"
	CF_APPEND_TEXT(CPPFLAGS,-D__ST_MT_ERRNO__)
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
test -n "$PROG_EXT" && AC_DEFINE_UNQUOTED(PROG_EXT,"$PROG_EXT",[Define to the program extension (normally blank)])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PROG_GROFF version: 3 updated: 2018/01/07 13:16:19
dnl -------------
dnl Check if groff is available, for cases (such as html output) where nroff
dnl is not enough.
AC_DEFUN([CF_PROG_GROFF],[
AC_PATH_PROG(GROFF_PATH,groff,no)
AC_PATH_PROGS(NROFF_PATH,nroff mandoc,no)
AC_PATH_PROG(TBL_PATH,tbl,cat)
if test "x$GROFF_PATH" = xno
then
	NROFF_NOTE=
	GROFF_NOTE="#"
else
	NROFF_NOTE="#"
	GROFF_NOTE=
fi
AC_SUBST(GROFF_NOTE)
AC_SUBST(NROFF_NOTE)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PROG_LINT version: 7 updated: 2024/11/30 14:37:45
dnl ------------
AC_DEFUN([CF_PROG_LINT],
[
AC_CHECK_PROGS(LINT, lint cppcheck splint)
case "x$LINT" in
(xlint|x*/lint) # NetBSD 10
	test -z "$LINT_OPTS" && LINT_OPTS="-chapbrxzgFS -v -Ac11"
	;;
(xcppcheck|x*/cppcheck)
	test -z "$LINT_OPTS" && LINT_OPTS="--enable=all -D__CPPCHECK__"
	;;
esac
AC_SUBST(LINT_OPTS)
AC_SUBST(LINT_LIBS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PROG_PERL version: 6 updated: 2020/12/31 20:19:42
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

  cf_version="`echo '[[]]'|sed -e 's/^./$/'`"
  for cf_prog in $cf_try; do
    echo "configure:__oline__: trying $cf_prog" >&AC_FD_CC
    if ($cf_prog -e 'printf "found version %g\n",'"$cf_version"'][dnl
ifelse([$1],,,[;exit('"$cf_version"'<$1)])') 1>&AC_FD_CC 2>&1; then
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
dnl CF_REMOVE_CFLAGS version: 3 updated: 2021/09/05 17:25:40
dnl ----------------
dnl Remove a given option from CFLAGS/CPPFLAGS
dnl $1 = option to remove
dnl $2 = variable to update
dnl $3 = nonempty to allow verbose message
define([CF_REMOVE_CFLAGS],
[
cf_tmp_cflag=`echo "x$1" | sed -e 's/^.//' -e 's/=.*//'`
while true
do
	cf_old_cflag=`echo "x[$]$2" | sed -e 's/^.//' -e 's/[[ 	]][[ 	]]*-/ -/g' -e "s%$cf_tmp_cflag\\(=[[^ 	]][[^ 	]]*\\)\?%%" -e 's/^[[ 	]]*//' -e 's%[[ ]][[ ]]*-D% -D%g' -e 's%[[ ]][[ ]]*-I% -I%g'`
	test "[$]$2" != "$cf_old_cflag" || break
	ifelse([$3],,,[CF_VERBOSE(removing old option $1 from $2)])
	$2="$cf_old_cflag"
done
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
dnl CF_REQUIRE_PKG version: 1 updated: 2025/01/10 19:55:54
dnl --------------
dnl Update $REQUIRE_PKG, which lists the known required packages for this
dnl program.
dnl
dnl $1 = package(s) to require, e.g., in the generated ".pc" file
define([CF_REQUIRE_PKG],
[
for cf_required in $1
do
	# check for duplicates
	for cf_require_pkg in $REQUIRE_PKG
	do
		if test "$cf_required" = "$cf_require_pkg"
		then
			cf_required=
			break
		fi
	done
	test -n "$cf_required" && REQUIRE_PKG="$REQUIRE_PKG $cf_required"
done
AC_SUBST(REQUIRE_PKG)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_RESTARTABLE_PIPEREAD version: 12 updated: 2023/12/09 10:53:57
dnl -----------------------
dnl CF_RESTARTABLE_PIPEREAD is a modified version of AC_RESTARTABLE_SYSCALLS
dnl from acspecific.m4, which uses a read on a pipe (surprise!) rather than a
dnl wait() as the test code.  apparently there is a POSIX change, which OSF/1
dnl at least has adapted to, which says reads (or writes) on pipes for which no
dnl data has been transferred are interruptible _regardless_ of the SA_RESTART
dnl bit.  yuck.
AC_DEFUN([CF_RESTARTABLE_PIPEREAD],
[
AC_REQUIRE([AC_TYPE_SIGNAL])
AC_CACHE_CHECK(for restartable reads on pipes, cf_cv_can_restart_read,[
AC_TRY_RUN(
[/* Exit 0 (true) if wait returns something other than -1,
   i.e. the pid of the child, which means that wait was restarted
   after getting the signal.  */
$ac_includes_default

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <signal.h>

#ifdef SA_RESTART
static void sigwrapper(int sig, RETSIGTYPE (*disp)(int))
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
static RETSIGTYPE ucatch (int isig) { (void) isig; }
int main (void) {
  int i, status;
  int fd[2];
  char buff[2];
  if (pipe(fd) == -1) ${cf_cv_main_return:-return} (1);
  i = fork();
  if (i == 0) {
      sleep (2);
      kill (getppid (), SIGINT);
      sleep (2);
      if (write(fd[1],"done",4) == -1) ${cf_cv_main_return:-return} (1);
      close(fd[1]);
      ${cf_cv_main_return:-return} (0);
  }
  sigwrapper (SIGINT, ucatch);
  status = (int) read(fd[0], buff, sizeof(buff));
  wait (&i);
  ${cf_cv_main_return:-return} (status == -1);
}
],
[cf_cv_can_restart_read=yes],
[cf_cv_can_restart_read=no],
[cf_cv_can_restart_read=unknown])
])

test $cf_cv_can_restart_read = yes && AC_DEFINE(HAVE_RESTARTABLE_PIPEREAD,1,[Define to 1 if we can make restartable reads on pipes])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_RESTORE_XTRA_FLAGS version: 1 updated: 2020/01/11 16:47:45
dnl ---------------------
dnl Restore flags saved in CF_SAVE_XTRA_FLAGS
dnl $1 = name of current macro
define([CF_RESTORE_XTRA_FLAGS],
[
LIBS="$cf_save_LIBS_$1"
CFLAGS="$cf_save_CFLAGS_$1"
CPPFLAGS="$cf_save_CPPFLAGS_$1"
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_RPATH_HACK version: 13 updated: 2021/01/03 18:30:50
dnl -------------
AC_DEFUN([CF_RPATH_HACK],
[AC_REQUIRE([AC_PROG_FGREP])dnl
AC_REQUIRE([CF_LD_RPATH_OPT])dnl

AC_MSG_CHECKING(for updated LDFLAGS)
if test -n "$LD_RPATH_OPT" ; then
	AC_MSG_RESULT(maybe)

	AC_CHECK_PROGS(cf_ldd_prog,ldd,no)
	cf_rpath_list="/usr/lib /lib"
	if test "$cf_ldd_prog" != no
	then
		cf_rpath_oops=

AC_TRY_LINK([#include <stdio.h>],
		[printf("Hello");],
		[cf_rpath_oops=`"$cf_ldd_prog" "conftest$ac_exeext" | ${FGREP-fgrep} ' not found' | sed -e 's% =>.*$%%' |sort | uniq`
		 cf_rpath_list=`"$cf_ldd_prog" "conftest$ac_exeext" | ${FGREP-fgrep} / | sed -e 's%^.*[[ 	]]/%/%' -e 's%/[[^/]][[^/]]*$%%' |sort | uniq`])

		# If we passed the link-test, but get a "not found" on a given library,
		# this could be due to inept reconfiguration of gcc to make it only
		# partly honor /usr/local/lib (or whatever).  Sometimes this behavior
		# is intentional, e.g., installing gcc in /usr/bin and suppressing the
		# /usr/local libraries.
		if test -n "$cf_rpath_oops"
		then
			for cf_rpath_src in $cf_rpath_oops
			do
				for cf_rpath_dir in \
					/usr/local \
					/usr/pkg \
					/opt/sfw
				do
					if test -f "$cf_rpath_dir/lib/$cf_rpath_src"
					then
						CF_VERBOSE(...adding -L$cf_rpath_dir/lib to LDFLAGS for $cf_rpath_src)
						LDFLAGS="$LDFLAGS -L$cf_rpath_dir/lib"
						break
					fi
				done
			done
		fi
	fi

	CF_VERBOSE(...checking EXTRA_LDFLAGS $EXTRA_LDFLAGS)

	CF_RPATH_HACK_2(LDFLAGS)
	CF_RPATH_HACK_2(LIBS)

	CF_VERBOSE(...checked EXTRA_LDFLAGS $EXTRA_LDFLAGS)
else
	AC_MSG_RESULT(no)
fi
AC_SUBST(EXTRA_LDFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_RPATH_HACK_2 version: 8 updated: 2021/01/01 13:31:04
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
	case "$cf_rpath_src" in
	(-L*)

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
dnl CF_SAVE_XTRA_FLAGS version: 1 updated: 2020/01/11 16:46:44
dnl ------------------
dnl Use this macro to save CFLAGS/CPPFLAGS/LIBS before checks against X headers
dnl and libraries which do not update those variables.
dnl
dnl $1 = name of current macro
define([CF_SAVE_XTRA_FLAGS],
[
cf_save_LIBS_$1="$LIBS"
cf_save_CFLAGS_$1="$CFLAGS"
cf_save_CPPFLAGS_$1="$CPPFLAGS"
LIBS="$LIBS ${X_PRE_LIBS} ${X_LIBS} ${X_EXTRA_LIBS}"
for cf_X_CFLAGS in $X_CFLAGS
do
	case "x$cf_X_CFLAGS" in
	x-[[IUD]]*)
		CPPFLAGS="$CPPFLAGS $cf_X_CFLAGS"
		;;
	*)
		CFLAGS="$CFLAGS $cf_X_CFLAGS"
		;;
	esac
done
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_SIGWINCH version: 7 updated: 2023/02/18 17:41:25
dnl -----------
dnl Use this macro after CF_XOPEN_SOURCE, but do not require it (not all
dnl programs need this test).
dnl
dnl This is really a Mac OS X 10.4.3 workaround.  Defining _POSIX_C_SOURCE
dnl forces SIGWINCH to be undefined (breaks xterm, ncurses).  Oddly, the struct
dnl winsize declaration is left alone - we may revisit this if Apple choose to
dnl break that part of the interface as well.
AC_DEFUN([CF_SIGWINCH],
[
AC_CACHE_CHECK(if SIGWINCH is defined,cf_cv_define_sigwinch,[
	AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/signal.h>
],[int x = SIGWINCH; (void)x],
	[cf_cv_define_sigwinch=yes],
	[AC_TRY_COMPILE([
#undef _XOPEN_SOURCE
#undef _POSIX_SOURCE
#undef _POSIX_C_SOURCE
#include <sys/types.h>
#include <sys/signal.h>
],[int x = SIGWINCH; (void)x],
	[cf_cv_define_sigwinch=maybe],
	[cf_cv_define_sigwinch=no])
])
])

if test "$cf_cv_define_sigwinch" = maybe ; then
AC_CACHE_CHECK(for actual SIGWINCH definition,cf_cv_fixup_sigwinch,[
cf_cv_fixup_sigwinch=unknown
cf_sigwinch=32
while test "$cf_sigwinch" != 1
do
	AC_TRY_COMPILE([
#undef _XOPEN_SOURCE
#undef _POSIX_SOURCE
#undef _POSIX_C_SOURCE
#include <sys/types.h>
#include <sys/signal.h>
],[
#if SIGWINCH != $cf_sigwinch
#error SIGWINCH is not $cf_sigwinch
#endif
int x = SIGWINCH; (void)x],
	[cf_cv_fixup_sigwinch=$cf_sigwinch
	 break])

cf_sigwinch="`expr "$cf_sigwinch" - 1`"
done
])

	if test "$cf_cv_fixup_sigwinch" != unknown ; then
		CPPFLAGS="$CPPFLAGS -DSIGWINCH=$cf_cv_fixup_sigwinch"
	fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_SIG_ATOMIC_T version: 5 updated: 2020/03/10 18:53:47
dnl ---------------
dnl signal handler, but there are some gcc dependencies in that recommendation.
dnl Try anyway.
AC_DEFUN([CF_SIG_ATOMIC_T],
[
AC_MSG_CHECKING(for signal global datatype)
AC_CACHE_VAL(cf_cv_sig_atomic_t,[
	for cf_type in \
		"volatile sig_atomic_t" \
		"sig_atomic_t" \
		"int"
	do
	AC_TRY_COMPILE([
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>

extern $cf_type x;
$cf_type x;
static void handler(int sig)
{
	(void)sig;
	x = 5;
}],
		[signal(SIGINT, handler);
		 x = 1],
		[cf_cv_sig_atomic_t=$cf_type],
		[cf_cv_sig_atomic_t=no])
		test "$cf_cv_sig_atomic_t" != no && break
	done
	])
AC_MSG_RESULT($cf_cv_sig_atomic_t)
test "$cf_cv_sig_atomic_t" != no && AC_DEFINE_UNQUOTED(SIG_ATOMIC_T, $cf_cv_sig_atomic_t,[Define to signal global datatype])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_SIZECHANGE version: 18 updated: 2021/09/04 06:35:04
dnl -------------
dnl Check for definitions & structures needed for window size-changing
dnl
dnl https://stackoverflow.com/questions/18878141/difference-between-structures-ttysize-and-winsize/50769952#50769952
AC_DEFUN([CF_SIZECHANGE],
[
AC_REQUIRE([CF_STRUCT_TERMIOS])
AC_CACHE_CHECK(declaration of size-change, cf_cv_sizechange,[
	cf_cv_sizechange=unknown
	cf_save_CPPFLAGS="$CPPFLAGS"

for cf_opts in "" "NEED_PTEM_H"
do

	CPPFLAGS="$cf_save_CPPFLAGS"
	if test -n "$cf_opts"
	then
		CF_APPEND_TEXT(CPPFLAGS,-D$cf_opts)
	fi
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
 * termios.h -- it is only in termio.h and ptem.h
 */
#include <sys/stream.h>
#include <sys/ptem.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
],[
#ifdef TIOCGSIZE
	struct ttysize win;	/* SunOS 3.0... */
	int y = win.ts_lines = 2;
	int x = win.ts_cols = 1;
	(void)y;
	(void)x;
#else
#ifdef TIOCGWINSZ
	struct winsize win;	/* everything else */
	int y = win.ws_row = 2;
	int x = win.ws_col = 1;
	(void)y;
	(void)x;
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
	AC_DEFINE(HAVE_SIZECHANGE,1,[Define to 1 if sizechange declarations are provided])
	case "$cf_cv_sizechange" in
	(NEED*)
		AC_DEFINE_UNQUOTED($cf_cv_sizechange )
		;;
	esac
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_STDIO_UNLOCKED version: 7 updated: 2023/12/13 18:02:34
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
	[cf_stdio_unlocked="$ac_func"])
if test "$cf_stdio_unlocked" != no ; then
	case "$CPPFLAGS" in
	(*-D_REENTRANT*)
		;;
	(*)
	AC_CACHE_CHECK(if we should define _REENTRANT,cf_cv_stdio_unlocked,[
	AC_TRY_COMPILE([#include <stdio.h>],[
		extern void *$cf_stdio_unlocked(void *);
		void *dummy = $cf_stdio_unlocked(0); (void)dummy],
			[cf_cv_stdio_unlocked=yes],
			[cf_cv_stdio_unlocked=no])])
		if test "$cf_cv_stdio_unlocked" = yes ; then
			AC_DEFINE(_REENTRANT,1,[Define to 1 if we should define _REENTRANT])
		fi
		;;
	esac
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_STRUCT_TERMIOS version: 13 updated: 2023/12/03 19:38:54
dnl -----------------
dnl Some machines require _POSIX_SOURCE to completely define struct termios.
AC_DEFUN([CF_STRUCT_TERMIOS],[
AC_REQUIRE([CF_XOPEN_SOURCE])

AC_CHECK_HEADERS( \
termio.h \
termios.h \
unistd.h \
sys/ioctl.h \
sys/termio.h \
)

if test "$ac_cv_header_termios_h" = yes ; then
	case "$CFLAGS $CPPFLAGS" in
	(*-D_POSIX_SOURCE*)
		termios_bad=dunno ;;
	(*)	termios_bad=maybe ;;
	esac
	if test "$termios_bad" = maybe ; then
	AC_MSG_CHECKING(whether termios.h needs _POSIX_SOURCE)
	AC_TRY_COMPILE([#include <termios.h>],
		[struct termios foo; int x = (int)(foo.c_iflag = 1); (void)x],
		termios_bad=no, [
		AC_TRY_COMPILE([
#define _POSIX_SOURCE
#include <termios.h>],
			[struct termios foo; int x = (int)(foo.c_iflag = 2); (void)x],
			termios_bad=unknown,
			termios_bad=yes AC_DEFINE(_POSIX_SOURCE,1,[Define to 1 if we must define _POSIX_SOURCE]))
			])
	AC_MSG_RESULT($termios_bad)
	fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_SUBDIR_PATH version: 7 updated: 2014/12/04 04:33:06
dnl --------------
dnl Construct a search-list for a nonstandard header/lib-file
dnl	$1 = the variable to return as result
dnl	$2 = the package name
dnl	$3 = the subdirectory, e.g., bin, include or lib
AC_DEFUN([CF_SUBDIR_PATH],
[
$1=

CF_ADD_SUBDIR_PATH($1,$2,$3,$prefix,NONE)

for cf_subdir_prefix in \
	/usr \
	/usr/local \
	/usr/pkg \
	/opt \
	/opt/local \
	[$]HOME
do
	CF_ADD_SUBDIR_PATH($1,$2,$3,$cf_subdir_prefix,$prefix)
done
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
dnl CF_TERMCAP_LIBS version: 18 updated: 2023/01/14 07:19:05
dnl ---------------
dnl Look for termcap libraries, or the equivalent in terminfo.
dnl
dnl The optional parameter may be "ncurses", "ncursesw".
AC_DEFUN([CF_TERMCAP_LIBS],
[
AC_CACHE_VAL(cf_cv_termlib,[
cf_cv_termlib=none
AC_TRY_LINK(
	[extern char *tgoto(const char*,int,int);],
	[char *x=tgoto("",0,0); (void)x;],
[AC_TRY_LINK(
	[extern char *tigetstr(const char *);],
	[char *x=tigetstr(""); (void)x;],
	[cf_cv_termlib=terminfo],
	[cf_cv_termlib=termcap])
	CF_VERBOSE(using functions in predefined $cf_cv_termlib LIBS)
],[
ifelse([$1],,,[
case "$1" in
(ncurses*)
	CF_NCURSES_CONFIG($1)
	cf_cv_termlib=terminfo
	;;
esac
])
if test "$cf_cv_termlib" = none; then
	# FreeBSD's linker gives bogus results for AC_CHECK_LIB, saying that
	# tgetstr lives in -lcurses when it is only an unsatisfied extern.
	cf_save_LIBS="$LIBS"
	for cf_lib in tinfo curses ncurses termlib termcap
	do
		LIBS="-l$cf_lib $cf_save_LIBS"
		for cf_func in tigetstr tgetstr
		do
			AC_MSG_CHECKING(for $cf_func in -l$cf_lib)
			AC_TRY_LINK(
				[extern char *$cf_func(const char *);],
				[char *x = $cf_func(""); (void)x],
				[cf_result=yes],
				[cf_result=no])
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
	AC_CHECK_LIB(curses, initscr, [CF_ADD_LIBS(-lcurses)])
	AC_CHECK_LIB(termcap, tgoto, [CF_ADD_LIBS(-ltermcap) cf_cv_termlib=termcap])
fi
])
if test "$cf_cv_termlib" = none; then
	AC_MSG_WARN([Cannot find -ltermlib, -lcurses, or -ltermcap])
fi
])])dnl
dnl ---------------------------------------------------------------------------
dnl CF_TERMIOS_TYPES version: 2 updated: 2020/03/10 18:53:47
dnl ----------------
dnl https://pubs.opengroup.org/onlinepubs/009695399/basedefs/termios.h.html
dnl says that tcflag_t, speed_t and cc_t are typedef'd.  If they are not,
dnl fallback to historical values.
AC_DEFUN([CF_TERMIOS_TYPES],[

AC_CACHE_CHECK(for termios type tcflag_t, cf_cv_havetype_tcflag_t,[
	AC_TRY_COMPILE([#include <termios.h>],[
		tcflag_t x = 0; (void)x],
		[cf_cv_havetype_tcflag_t=yes],
		[cf_cv_havetype_tcflag_t=no])
])
test "$cf_cv_havetype_tcflag_t" = no && AC_DEFINE(tcflag_t,unsigned long,[Define usable value of tcflag_t if not declared])

AC_CACHE_CHECK(for termios type speed_t, cf_cv_havetype_speed_t,[
	AC_TRY_COMPILE([#include <termios.h>],[
		speed_t x = 0; (void)x],
		[cf_cv_havetype_speed_t=yes],
		[cf_cv_havetype_speed_t=no])
])
test "$cf_cv_havetype_speed_t" = no && AC_DEFINE(speed_t,unsigned short,[Define usable value of speed_t if not declared])

AC_CACHE_CHECK(for termios type cc_t, cf_cv_havetype_cc_t,[
	AC_TRY_COMPILE([#include <termios.h>],[
		cc_t x = 0; (void)x],
		[cf_cv_havetype_cc_t=yes],
		[cf_cv_havetype_cc_t=no])
])
test "$cf_cv_havetype_cc_t" = no && AC_DEFINE(cc_t,unsigned char,[Define usable value of cc_t if not declared])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_TERM_HEADER version: 6 updated: 2021/01/02 09:31:20
dnl --------------
dnl Look for term.h, which is part of X/Open curses.  It defines the interface
dnl to terminfo database.  Usually it is in the same include-path as curses.h,
dnl but some packagers change this, breaking various applications.
AC_DEFUN([CF_TERM_HEADER],[
AC_CACHE_CHECK(for terminfo header, cf_cv_term_header,[
case "${cf_cv_ncurses_header}" in
(*/ncurses.h|*/ncursesw.h)
	cf_term_header=`echo "$cf_cv_ncurses_header" | sed -e 's%ncurses[[^.]]*\.h$%term.h%'`
	;;
(*)
	cf_term_header=term.h
	;;
esac

for cf_test in $cf_term_header "ncurses/term.h" "ncursesw/term.h"
do
AC_TRY_COMPILE([#include <stdio.h>
#include <${cf_cv_ncurses_header:-curses.h}>
#include <$cf_test>
],[int x = auto_left_margin; (void)x],[
	cf_cv_term_header="$cf_test"],[
	cf_cv_term_header=unknown
	])
	test "$cf_cv_term_header" != unknown && break
done
])

# Set definitions to allow ifdef'ing to accommodate subdirectories

case "$cf_cv_term_header" in
(*term.h)
	AC_DEFINE(HAVE_TERM_H,1,[Define to 1 if we have term.h])
	;;
esac

case "$cf_cv_term_header" in
(ncurses/term.h)
	AC_DEFINE(HAVE_NCURSES_TERM_H,1,[Define to 1 if we have ncurses/term.h])
	;;
(ncursesw/term.h)
	AC_DEFINE(HAVE_NCURSESW_TERM_H,1,[Define to 1 if we have ncursesw/term.h])
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_TRANSFORM_PROGRAM version: 3 updated: 2020/12/31 20:19:42
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
program_transform_name="`echo "$program_transform_name" | sed -f conftest.sed`"
rm conftest.sed
])
dnl ---------------------------------------------------------------------------
dnl CF_TRIM_X_LIBS version: 3 updated: 2015/04/12 15:39:00
dnl --------------
dnl Trim extra base X libraries added as a workaround for inconsistent library
dnl dependencies returned by "new" pkg-config files.
AC_DEFUN([CF_TRIM_X_LIBS],[
	for cf_trim_lib in Xmu Xt X11
	do
		case "$LIBS" in
		(*-l$cf_trim_lib\ *-l$cf_trim_lib*)
			LIBS=`echo "$LIBS " | sed -e 's/  / /g' -e 's%-l'"$cf_trim_lib"' %%' -e 's/ $//'`
			CF_VERBOSE(..trimmed $LIBS)
			;;
		esac
	done
])
dnl ---------------------------------------------------------------------------
dnl CF_TRY_PKG_CONFIG version: 7 updated: 2025/01/10 19:55:54
dnl -----------------
dnl This is a simple wrapper to use for pkg-config, for libraries which may be
dnl available in that form.
dnl
dnl $1 = package name, which may be a shell variable
dnl $2 = extra logic to use, if any, after updating CFLAGS and LIBS
dnl $3 = logic to use if pkg-config does not have the package
AC_DEFUN([CF_TRY_PKG_CONFIG],[
AC_REQUIRE([CF_PKG_CONFIG])

if test "$PKG_CONFIG" != none && "$PKG_CONFIG" --exists "$1"; then
	CF_VERBOSE(found package $1)
	cf_pkgconfig_incs="`$PKG_CONFIG --cflags "$1" 2>/dev/null`"
	cf_pkgconfig_libs="`$PKG_CONFIG --libs   "$1" 2>/dev/null`"
	CF_VERBOSE(package $1 CFLAGS: $cf_pkgconfig_incs)
	CF_VERBOSE(package $1 LIBS: $cf_pkgconfig_libs)
	CF_REQUIRE_PKG($1)
	CF_ADD_CFLAGS($cf_pkgconfig_incs)
	CF_ADD_LIBS($cf_pkgconfig_libs)
	ifelse([$2],,:,[$2])
else
	cf_pkgconfig_incs=
	cf_pkgconfig_libs=
	ifelse([$3],,:,[$3])
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_TRY_XOPEN_SOURCE version: 4 updated: 2022/09/10 15:16:16
dnl -------------------
dnl If _XOPEN_SOURCE is not defined in the compile environment, check if we
dnl can define it successfully.
AC_DEFUN([CF_TRY_XOPEN_SOURCE],[
AC_CACHE_CHECK(if we should define _XOPEN_SOURCE,cf_cv_xopen_source,[
	AC_TRY_COMPILE(CF__XOPEN_SOURCE_HEAD,CF__XOPEN_SOURCE_BODY,
	[cf_cv_xopen_source=no],
	[cf_save="$CPPFLAGS"
	 CF_APPEND_TEXT(CPPFLAGS,-D_XOPEN_SOURCE=$cf_XOPEN_SOURCE)
	 AC_TRY_COMPILE(CF__XOPEN_SOURCE_HEAD,CF__XOPEN_SOURCE_BODY,
		[cf_cv_xopen_source=no],
		[cf_cv_xopen_source=$cf_XOPEN_SOURCE])
		CPPFLAGS="$cf_save"
	])
])

if test "$cf_cv_xopen_source" != no ; then
	CF_REMOVE_DEFINE(CFLAGS,$CFLAGS,_XOPEN_SOURCE)
	CF_REMOVE_DEFINE(CPPFLAGS,$CPPFLAGS,_XOPEN_SOURCE)
	cf_temp_xopen_source="-D_XOPEN_SOURCE=$cf_cv_xopen_source"
	CF_APPEND_CFLAGS($cf_temp_xopen_source)
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_TYPE_FD_SET version: 6 updated: 2020/03/10 18:53:47
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
	[fd_set x; (void)x],
	[cf_cv_type_fd_set=sys/types.h],
	[CF_MSG_LOG(X11/Xpoll.h)
AC_TRY_COMPILE([
#ifdef HAVE_X11_XPOLL_H
#include <X11/Xpoll.h>
#endif],
	[fd_set x; (void)x],
	[cf_cv_type_fd_set=X11/Xpoll.h],
	[CF_MSG_LOG(sys/select.h)
AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/select.h>],
	[fd_set x; (void)x],
	[cf_cv_type_fd_set=sys/select.h],
	[cf_cv_type_fd_set=unknown])])])])
if test $cf_cv_type_fd_set = sys/select.h ; then
	AC_DEFINE(USE_SYS_SELECT_H,1,[Define to 1 to include sys/select.h to declare fd_set])
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_TYPE_OUTCHAR version: 16 updated: 2023/12/09 10:53:57
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
	AC_TRY_COMPILE([$CHECK_DECL_HDRS
	extern $Q OutChar($R);
	extern $P tputs ($S char *string, int nlines, $Q (*_f)($R));
	static char fmt[] = "";],
	[tputs(fmt, 1, OutChar)],
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
(int*)
	AC_DEFINE(OUTC_RETURN,1,[Define to 1 if tputs outc function returns a value])
	;;
esac
case $cf_cv_type_outchar in
(*char*)
	AC_DEFINE(OUTC_ARGS,char c,[Define to actual type to override tputs outc parameter type])
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
dnl CF_UTF8_LIB version: 11 updated: 2024/08/10 10:23:45
dnl -----------
dnl Check for multibyte support, and if not found, utf8 compatibility library
AC_DEFUN([CF_UTF8_LIB],
[
AC_CHECK_HEADERS(wchar.h)
AC_CACHE_CHECK(for multibyte character support,cf_cv_utf8_lib,[
	cf_save_LIBS="$LIBS"
	AC_TRY_LINK([
$ac_includes_default
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
],[putwc(0,0);],
	[cf_cv_utf8_lib=yes],
	[CF_FIND_LINKAGE([
#include <libutf8.h>],[putwc(0,0);],utf8,
		[cf_cv_utf8_lib=add-on],
		[cf_cv_utf8_lib=no])
])])

# HAVE_LIBUTF8_H is used by ncurses if curses.h is shared between
# ncurses/ncursesw:
if test "$cf_cv_utf8_lib" = "add-on" ; then
	AC_DEFINE(HAVE_LIBUTF8_H,1,[Define to 1 if we should include libutf8.h])
	CF_ADD_INCDIR($cf_cv_header_path_utf8)
	CF_ADD_LIBDIR($cf_cv_library_path_utf8)
	CF_ADD_LIBS($cf_cv_library_file_utf8)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_VA_COPY version: 6 updated: 2018/12/04 18:14:25
dnl ----------
dnl check for va_copy, part of stdarg.h starting with ISO C 1999.
dnl Also, workaround for glibc's __va_copy, by checking for both.
dnl Finally, try to accommodate pre-ISO C 1999 headers.
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

if test "$cf_cv_have_va_copy" = yes;
then
	AC_DEFINE(HAVE_VA_COPY,1,[Define to 1 if we have va_copy])
else # !cf_cv_have_va_copy

AC_CACHE_CHECK(for __va_copy, cf_cv_have___va_copy,[
AC_TRY_LINK([
#include <stdarg.h>
],[
	static va_list dst;
	static va_list src;
	__va_copy(dst, src)],
	cf_cv_have___va_copy=yes,
	cf_cv_have___va_copy=no)])

if test "$cf_cv_have___va_copy" = yes
then
	AC_DEFINE(HAVE___VA_COPY,1,[Define to 1 if we have __va_copy])
else # !cf_cv_have___va_copy

AC_CACHE_CHECK(for __builtin_va_copy, cf_cv_have___builtin_va_copy,[
AC_TRY_LINK([
#include <stdarg.h>
],[
	static va_list dst;
	static va_list src;
	__builtin_va_copy(dst, src)],
	cf_cv_have___builtin_va_copy=yes,
	cf_cv_have___builtin_va_copy=no)])

test "$cf_cv_have___builtin_va_copy" = yes &&
	AC_DEFINE(HAVE___BUILTIN_VA_COPY,1,[Define to 1 if we have __builtin_va_copy])

fi # cf_cv_have___va_copy

fi # cf_cv_have_va_copy

case "${cf_cv_have_va_copy}${cf_cv_have___va_copy}${cf_cv_have___builtin_va_copy}" in
(*yes*)
	;;

(*)
	AC_CACHE_CHECK(if we can simply copy va_list, cf_cv_pointer_va_list,[
AC_TRY_LINK([
#include <stdarg.h>
],[
	va_list dst;
	va_list src;
	dst = src],
	cf_cv_pointer_va_list=yes,
	cf_cv_pointer_va_list=no)])

	if test "$cf_cv_pointer_va_list" = no
	then
		AC_CACHE_CHECK(if we can copy va_list indirectly, cf_cv_array_va_list,[
AC_TRY_LINK([
#include <stdarg.h>
],[
	va_list dst;
	va_list src;
	*dst = *src],
			cf_cv_array_va_list=yes,
			cf_cv_array_va_list=no)])
		test "$cf_cv_array_va_list" = yes && AC_DEFINE(ARRAY_VA_LIST,1,[Define to 1 if we can copy va_list indirectly])
	fi
	;;
esac
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
dnl CF_WCTYPE version: 7 updated: 2023/01/14 07:48:15
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
		|| iswxdigit(temp); (void)test],
	[cf_cv_have_wctype=yes],
	[cf_cv_have_wctype=no])
])
if test "$cf_cv_have_wctype" = yes ; then
	AC_SEARCH_LIBS(wctype,[w],[AC_DEFINE(HAVE_WCTYPE,1,[Define to 1 if we have usable wctype.h])])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_APP_CLASS version: 3 updated: 2015/04/12 15:39:00
dnl -----------------
dnl Handle configure option "--with-app-class", setting the $APP_CLASS
dnl variable, used for X resources.
dnl
dnl $1 = default value.
AC_DEFUN([CF_WITH_APP_CLASS],[
AC_MSG_CHECKING(for X applications class)
AC_ARG_WITH(app-class,
	[  --with-app-class=XXX    override X applications class (default $1)],
	[APP_CLASS=$withval],
	[APP_CLASS=$1])

case x$APP_CLASS in
(*[[/@,%]]*)
	AC_MSG_WARN(X applications class cannot contain punctuation)
	APP_CLASS=$1
	;;
(x[[A-Z]]*)
	;;
(*)
	AC_MSG_WARN([X applications class must start with capital, ignoring $APP_CLASS])
	APP_CLASS=$1
	;;
esac

AC_MSG_RESULT($APP_CLASS)

AC_SUBST(APP_CLASS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_APP_DEFAULTS version: 6 updated: 2015/01/02 09:05:50
dnl --------------------
dnl Handle configure option "--with-app-defaults", setting these shell
dnl variables:
dnl
dnl $APPSDIR is the option value, used for installing app-defaults files.
dnl $no_appsdir is a "#" (comment) if "--without-app-defaults" is given.
dnl
dnl Most Linux's use this:
dnl 	/usr/share/X11/app-defaults
dnl Debian uses this:
dnl 	/etc/X11/app-defaults
dnl DragonFlyBSD ports uses this:
dnl 	/usr/pkg/lib/X11/app-defaults
dnl FreeBSD ports use these:
dnl 	/usr/local/lib/X11/app-defaults
dnl 	/usr/local/share/X11/app-defaults
dnl Mandriva has these:
dnl 	/usr/lib/X11/app-defaults
dnl 	/usr/lib64/X11/app-defaults
dnl NetBSD has these
dnl 	/usr/X11R7/lib/X11/app-defaults
dnl OpenSolaris uses
dnl 	32-bit:
dnl 	/usr/X11/etc/X11/app-defaults
dnl 	/usr/X11/share/X11/app-defaults
dnl 	/usr/X11/lib/X11/app-defaults
dnl OSX uses
dnl		/opt/local/share/X11/app-defaults (MacPorts)
dnl		/opt/X11/share/X11/app-defaults (non-ports)
dnl	64-bit:
dnl 	/usr/X11/etc/X11/app-defaults
dnl 	/usr/X11/share/X11/app-defaults (I mkdir'd this)
dnl 	/usr/X11/lib/amd64/X11/app-defaults
dnl Solaris10 uses (in this order):
dnl 	/usr/openwin/lib/X11/app-defaults
dnl 	/usr/X11/lib/X11/app-defaults
AC_DEFUN([CF_WITH_APP_DEFAULTS],[
AC_MSG_CHECKING(for directory to install resource files)
AC_ARG_WITH(app-defaults,
	[  --with-app-defaults=DIR directory in which to install resource files (EPREFIX/lib/X11/app-defaults)],
	[APPSDIR=$withval],
	[APPSDIR='${exec_prefix}/lib/X11/app-defaults'])

if test "x[$]APPSDIR" = xauto
then
	APPSDIR='${exec_prefix}/lib/X11/app-defaults'
	for cf_path in \
		/opt/local/share/X11/app-defaults \
		/opt/X11/share/X11/app-defaults \
		/usr/share/X11/app-defaults \
		/usr/X11/share/X11/app-defaults \
		/usr/X11/lib/X11/app-defaults \
		/usr/lib/X11/app-defaults \
		/etc/X11/app-defaults \
		/usr/pkg/lib/X11/app-defaults \
		/usr/X11R7/lib/X11/app-defaults \
		/usr/X11R6/lib/X11/app-defaults \
		/usr/X11R5/lib/X11/app-defaults \
		/usr/X11R4/lib/X11/app-defaults \
		/usr/local/lib/X11/app-defaults \
		/usr/local/share/X11/app-defaults \
		/usr/lib64/X11/app-defaults
	do
		if test -d "$cf_path" ; then
			APPSDIR="$cf_path"
			break
		fi
	done
else
	cf_path=$APPSDIR
	CF_PATH_SYNTAX(cf_path)
fi

AC_MSG_RESULT($APPSDIR)
AC_SUBST(APPSDIR)

no_appsdir=
if test "$APPSDIR" = no
then
	no_appsdir="#"
else
	EXTRA_INSTALL_DIRS="$EXTRA_INSTALL_DIRS \$(APPSDIR)"
fi
AC_SUBST(no_appsdir)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_CURSES_DIR version: 4 updated: 2021/01/02 19:22:58
dnl ------------------
dnl Wrapper for AC_ARG_WITH to specify directory under which to look for curses
dnl libraries.
AC_DEFUN([CF_WITH_CURSES_DIR],[

AC_MSG_CHECKING(for specific curses-directory)
AC_ARG_WITH(curses-dir,
	[  --with-curses-dir=DIR   directory in which (n)curses is installed],
	[cf_cv_curses_dir=$withval],
	[cf_cv_curses_dir=no])
AC_MSG_RESULT($cf_cv_curses_dir)

if test -n "$cf_cv_curses_dir" && test "$cf_cv_curses_dir" != "no"
then
	CF_PATH_SYNTAX(withval)
	if test -d "$cf_cv_curses_dir"
	then
		CF_ADD_INCDIR($cf_cv_curses_dir/include)
		CF_ADD_LIBDIR($cf_cv_curses_dir/lib)
	fi
fi
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
dnl CF_WITH_ICONDIR version: 5 updated: 2012/07/22 09:18:02
dnl ---------------
dnl Handle configure option "--with-icondir", setting these shell variables:
dnl
dnl $ICONDIR is the option value, used for installing icon files.
dnl $no_icondir is a "#" (comment) if "--without-icondir" is given.
AC_DEFUN([CF_WITH_ICONDIR],[
AC_MSG_CHECKING(for directory to install icons)
AC_ARG_WITH(icondir,
	[  --with-icondir=DIR      directory in which to install icons for desktop],
	[ICONDIR=$withval],
	[test -z "$ICONDIR" && ICONDIR=no])

if test "x[$]ICONDIR" = xauto
then
	ICONDIR='${datadir}/icons'
	for cf_path in \
		/usr/share/icons \
		/usr/X11R6/share/icons
	do
		if test -d "$cf_path" ; then
			ICONDIR="$cf_path"
			break
		fi
	done
else
	cf_path=$ICONDIR
	CF_PATH_SYNTAX(cf_path)
fi
AC_MSG_RESULT($ICONDIR)
AC_SUBST(ICONDIR)

no_icondir=
if test "$ICONDIR" = no
then
	no_icondir="#"
else
	EXTRA_INSTALL_DIRS="$EXTRA_INSTALL_DIRS \$(ICONDIR)"
fi
AC_SUBST(no_icondir)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_ICON_THEME version: 14 updated: 2023/11/23 06:40:35
dnl ------------------
dnl If asked, check for prerequisites and setup symbols to permit installing
dnl one or more application icons in the Red Hat icon-theme directory
dnl hierarchy.
dnl
dnl If the prerequisites are missing, give a warning and revert to the long-
dnl standing pixmaps directory.
dnl
dnl Parameters:
dnl
dnl $1 = application icon.  This can be a list, and is not optional.
dnl $2 = default theme (defaults to hicolor)
dnl $3 = formats (defaults to list [.svg .png .xpm])
dnl $4 = alternate icon if no theme is used (defaults to $1).
dnl
dnl Result:
dnl ICON_NAME = basename of first item in $1, unless already set
dnl ICON_LIST = reprocessed $1
dnl ICON_THEME = reprocessed $2
dnl ICON_FORMAT = reprocessed $3
AC_DEFUN([CF_WITH_ICON_THEME],
[
ifelse([$1],,[
	AC_MSG_ERROR([macro [CF_WITH_ICON_THEME] requires application-icon name])
],[

CF_WITH_PIXMAPDIR
CF_WITH_ICONDIR

AC_MSG_CHECKING(if icon theme should be used)
AC_ARG_WITH(icon-theme,
	[[  --with-icon-theme[=XXX] install icons into desktop theme (hicolor)]],
	[ICON_THEME=$withval],
	[ICON_THEME=no])

case "x$ICON_THEME" in
(xno)
	;;
(x|xyes)
	ICON_THEME=ifelse([$2],,hicolor,$2)
	;;
esac
AC_MSG_RESULT($ICON_THEME)

if test "x$ICON_THEME" = xno
then
	if test "x$ICONDIR" != xno
	then
		CF_VERBOSE(ignoring icondir without theme)
		no_icondir="#"
	fi
else
	if test "x$ICONDIR" = xno
	then
		AC_MSG_ERROR(icondir must be set for icon theme)
	fi
fi

: ${ICON_FORMAT:=ifelse([$3],,[".svg .png .xpm"],[$3])}

ICON_LIST=

ifelse([$4],,[cf_icon_list=$1],[
if test "x$ICON_THEME" != xno
then
	cf_icon_list="$1"
else
	cf_icon_list="$4"
fi
])

AC_MSG_CHECKING([for icon(s) to install])
for cf_name in $cf_icon_list
do
	CF_VERBOSE(using $ICON_FORMAT)
	for cf_suffix in $ICON_FORMAT
	do
		cf_icon="${cf_name}${cf_suffix}"
		cf_left=`echo "$cf_icon" | sed -e 's/:.*//'`
		if test ! -f "${cf_left}"
		then
			if test "x$srcdir" != "x."
			then
				cf_icon="${srcdir}/${cf_left}"
				cf_left=`echo "$cf_icon" | sed -e 's/:.*//'`
				if test ! -f "${cf_left}"
				then
					continue
				fi
			else
				continue
			fi
		fi
		if test "x$ICON_THEME" != xno
		then
			cf_base=`basename "$cf_left"`
			cf_trim=`echo "$cf_base" | sed -e 's/_[[0-9]][[0-9]]x[[0-9]][[0-9]]\././'`
			case "x${cf_base}" in
			(*:*)
				cf_next=$cf_base
				# user-defined mapping
				;;
			(*.png)
				cf_size=`file "$cf_left"|sed -e 's/^[[^:]]*://' -e 's/^.*[[^0-9]]\([[0-9]][[0-9]]* x [[0-9]][[0-9]]*\)[[^0-9]].*$/\1/' -e 's/ //g'`
				if test -z "$cf_size"
				then
					AC_MSG_WARN(cannot determine size of $cf_left)
					continue
				fi
				cf_next="$cf_size/apps/$cf_trim"
				;;
			(*.svg)
				cf_next="scalable/apps/$cf_trim"
				;;
			(*.xpm)
				CF_VERBOSE(ignored XPM file in icon theme)
				continue
				;;
			(*_[[0-9]][[0-9]]*x[[0-9]][[0-9]]*.*)
				cf_size=`echo "$cf_left"|sed -e 's/^.*_\([[0-9]][[0-9]]*x[[0-9]][[0-9]]*\)\..*$/\1/'`
				cf_left=`echo "$cf_left"|sed -e 's/^\(.*\)_\([[0-9]][[0-9]]*x[[0-9]][[0-9]]*\)\(\..*\)$/\1\3/'`
				cf_next="$cf_size/apps/$cf_base"
				;;
			esac
			CF_VERBOSE(adding $cf_next)
			cf_icon="${cf_icon}:${cf_next}"
		fi
		test -n "$ICON_LIST" && ICON_LIST="$ICON_LIST "
		ICON_LIST="$ICON_LIST${cf_icon}"
		if test -z "$ICON_NAME"
		then
			ICON_NAME=`basename "$cf_icon" | sed -e 's/[[.:]].*//'`
		fi
	done
done

if test -n "$verbose"
then
	AC_MSG_CHECKING(result)
fi
AC_MSG_RESULT($ICON_LIST)

if test -z "$ICON_LIST"
then
	AC_MSG_ERROR(no icons found)
fi
])

AC_MSG_CHECKING(for icon name)
AC_MSG_RESULT($ICON_NAME)

AC_SUBST(ICON_FORMAT)
AC_SUBST(ICON_THEME)
AC_SUBST(ICON_LIST)
AC_SUBST(ICON_NAME)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_IMAKE_CFLAGS version: 13 updated: 2024/07/05 19:34:53
dnl --------------------
dnl xterm and similar programs build more readily when propped up with imake's
dnl hand-tuned definitions.  If we do not use imake, provide fallbacks for the
dnl most common definitions that we're not likely to do by autoconf tests.
AC_DEFUN([CF_WITH_IMAKE_CFLAGS],[
AC_REQUIRE([CF_ENABLE_NARROWPROTO])

AC_MSG_CHECKING(if we should use imake to help)
CF_ARG_ENABLE(imake,
	[  --enable-imake          enable use of imake for definitions],
	[enable_imake=yes],
	[enable_imake=no])
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
	(*[[0-9]].[[0-9]]*)
		UNAME_RELEASE="$host_os"
		;;
	(*)
		UNAME_RELEASE=`(uname -r) 2>/dev/null` || UNAME_RELEASE=unknown
		;;
	esac

	case .$UNAME_RELEASE in
	(*[[0-9]].[[0-9]]*)
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
	(aix*)
		# imake on AIX 5.1 defines AIXV3.  really.
		IMAKE_CFLAGS="-DAIXV3 -DAIXV4 $IMAKE_CFLAGS"
		;;
	(irix[[56]].*)
		# these are needed to make SIGWINCH work in xterm
		IMAKE_CFLAGS="-DSYSV -DSVR4 $IMAKE_CFLAGS"
		;;
	esac

	# "modern" systems install X applications in /usr/bin.  Other systems may
	# use one of the X release-based directories.
	case "$CFLAGS $CPPFLAGS $IMAKE_CFLAGS" in
	(*-DPROJECTROOT*)
		;;
	(*)
		for cf_dir in /usr/X11R7 /usr/X11R6 /usr/X11R5
		do
			if test -d "$cf_dir/bin"
			then
				IMAKE_CFLAGS="$IMAKE_CFLAGS -DPROJECTROOT=\\\"$cf_dir\\\""
				break
			fi
		done
		;;
	esac

	CF_ADD_CFLAGS($IMAKE_CFLAGS)

	AC_SUBST(IMAKE_CFLAGS)
	AC_SUBST(IMAKE_LOADFLAGS)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_MAN2HTML version: 14 updated: 2024/09/09 17:17:46
dnl ----------------
dnl Check for man2html and groff.  Prefer man2html over groff, but use groff
dnl as a fallback.  See
dnl
dnl		https://invisible-island.net/scripts/man2html.html
dnl
dnl Generate a shell script which hides the differences between the two.
dnl
dnl We name that "man2html.tmp".
dnl
dnl The shell script can be removed later, e.g., using "make distclean".
AC_DEFUN([CF_WITH_MAN2HTML],[
AC_REQUIRE([CF_PROG_GROFF])dnl
AC_REQUIRE([AC_PROG_FGREP])dnl

case "x${with_man2html}" in
(xno)
	cf_man2html=no
	;;
(x|xyes)
	AC_PATH_PROG(cf_man2html,man2html,no)
	case "x$cf_man2html" in
	(x/*)
		AC_MSG_CHECKING(for the modified Earl Hood script)
		if ( $cf_man2html -help 2>&1 | grep 'Make an index of headers at the end' >/dev/null )
		then
			cf_man2html_ok=yes
		else
			cf_man2html=no
			cf_man2html_ok=no
		fi
		AC_MSG_RESULT($cf_man2html_ok)
		;;
	(*)
		cf_man2html=no
		;;
	esac
esac

AC_MSG_CHECKING(for program to convert manpage to html)
AC_ARG_WITH(man2html,
	[[  --with-man2html[=XXX]   use XXX rather than groff]],
	[cf_man2html=$withval],
	[cf_man2html=$cf_man2html])

cf_with_groff=no

case $cf_man2html in
(yes)
	AC_MSG_RESULT(man2html)
	AC_PATH_PROG(cf_man2html,man2html,no)
	;;
(no|groff|*/groff*)
	cf_with_groff=yes
	cf_man2html=$GROFF_PATH
	AC_MSG_RESULT($cf_man2html)
	;;
(*)
	AC_MSG_RESULT($cf_man2html)
	;;
esac

MAN2HTML_TEMP="man2html.tmp"
	cat >$MAN2HTML_TEMP <<CF_EOF
#!$SHELL
# Temporary script generated by CF_WITH_MAN2HTML
# Convert inputs to html, sending result to standard output.
#
# Parameters:
# \${1} = rootname of file to convert
# \${2} = suffix of file to convert, e.g., "1"
# \${3} = macros to use, e.g., "man"
#
ROOT=\[$]1
TYPE=\[$]2
MACS=\[$]3

unset LANG
unset LC_ALL
unset LC_CTYPE
unset LANGUAGE
GROFF_NO_SGR=stupid
export GROFF_NO_SGR

CF_EOF

NROFF_OPTS=
if test "x$cf_with_groff" = xyes
then
	MAN2HTML_NOTE="$GROFF_NOTE"
	MAN2HTML_PATH="$GROFF_PATH"
	cat >>$MAN2HTML_TEMP <<CF_EOF
$SHELL -c "$TBL_PATH \${ROOT}.\${TYPE} | $GROFF_PATH -P -o0 -I\${ROOT}_ -Thtml -\${MACS}"
CF_EOF
else
	# disable hyphenation if this is groff
	if test "x$GROFF_PATH" != xno
	then
		AC_MSG_CHECKING(if nroff is really groff)
		cf_check_groff="`$NROFF_PATH --version 2>/dev/null | grep groff`"
		test -n "$cf_check_groff" && cf_check_groff=yes
		test -n "$cf_check_groff" || cf_check_groff=no
		AC_MSG_RESULT($cf_check_groff)
		test "x$cf_check_groff" = xyes && NROFF_OPTS="-rHY=0"
	fi
	MAN2HTML_NOTE=""
	CF_PATH_SYNTAX(cf_man2html)
	MAN2HTML_PATH="$cf_man2html"
	AC_MSG_CHECKING(for $cf_man2html top/bottom margins)

	# for this example, expect 3 lines of content, the remainder is head/foot
	cat >conftest.in <<CF_EOF
.TH HEAD1 HEAD2 HEAD3 HEAD4 HEAD5
.SH SECTION
MARKER
CF_EOF

	LC_ALL=C LC_CTYPE=C LANG=C LANGUAGE=C $NROFF_PATH -man conftest.in >conftest.out

	cf_man2html_1st="`${FGREP-fgrep} -n MARKER conftest.out |sed -e 's/^[[^0-9]]*://' -e 's/:.*//'`"
	cf_man2html_top=`expr "$cf_man2html_1st" - 2`
	cf_man2html_bot="`wc -l conftest.out |sed -e 's/[[^0-9]]//g'`"
	cf_man2html_bot=`expr "$cf_man2html_bot" - 2 - "$cf_man2html_top"`
	cf_man2html_top_bot="-topm=$cf_man2html_top -botm=$cf_man2html_bot"

	AC_MSG_RESULT($cf_man2html_top_bot)

	AC_MSG_CHECKING(for pagesize to use)
	for cf_block in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
	do
	cat >>conftest.in <<CF_EOF
.nf
0
1
2
3
4
5
6
7
8
9
CF_EOF
	done

	LC_ALL=C LC_CTYPE=C LANG=C LANGUAGE=C $NROFF_PATH -man conftest.in >conftest.out
	cf_man2html_page="`${FGREP-fgrep} -n HEAD1 conftest.out |sed -n '$p' |sed -e 's/^[[^0-9]]*://' -e 's/:.*//'`"
	test -z "$cf_man2html_page" && cf_man2html_page=99999
	test "$cf_man2html_page" -gt 100 && cf_man2html_page=99999

	rm -rf conftest*
	AC_MSG_RESULT($cf_man2html_page)

	cat >>$MAN2HTML_TEMP <<CF_EOF
: \${MAN2HTML_PATH=$MAN2HTML_PATH}
MAN2HTML_OPTS="\$MAN2HTML_OPTS -index -title=\"\$ROOT(\$TYPE)\" -compress -pgsize $cf_man2html_page"
case \${TYPE} in
(ms)
	$TBL_PATH \${ROOT}.\${TYPE} | $NROFF_PATH $NROFF_OPTS -\${MACS} | \$MAN2HTML_PATH -topm=0 -botm=0 \$MAN2HTML_OPTS
	;;
(*)
	$TBL_PATH \${ROOT}.\${TYPE} | $NROFF_PATH $NROFF_OPTS -\${MACS} | \$MAN2HTML_PATH $cf_man2html_top_bot \$MAN2HTML_OPTS
	;;
esac
CF_EOF
fi

chmod 700 $MAN2HTML_TEMP

AC_SUBST(MAN2HTML_NOTE)
AC_SUBST(MAN2HTML_PATH)
AC_SUBST(MAN2HTML_TEMP)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_NO_LEAKS version: 6 updated: 2021/11/25 17:10:01
dnl ----------------
AC_DEFUN([CF_WITH_NO_LEAKS],[
AC_REQUIRE([CF_WITH_DMALLOC])
AC_REQUIRE([CF_WITH_DBMALLOC])
AC_REQUIRE([CF_WITH_PURIFY])
AC_REQUIRE([CF_WITH_VALGRIND])

AC_MSG_CHECKING(if you want to perform memory-leak testing)
AC_ARG_WITH(no-leaks,
	[  --with-no-leaks         test: free permanent memory, analyze leaks],
	[case "x$withval" in
	 (xno|x)
		: ${enable_leaks:=yes}
	 	;;
	 (*)
		enable_leaks=no
	 	;;
	esac],
	[: ${enable_leaks:=yes}])

dnl TODO - drop with_no_leaks
if test "x$enable_leaks" = xno
then
	with_no_leaks=yes
else
	with_no_leaks=no
fi

AC_MSG_RESULT($with_no_leaks)

if test "x$enable_leaks" = xno
then
	AC_DEFINE(NO_LEAKS,1,[Define to 1 to enable leak-checking])
	cf_doalloc=".${with_dmalloc}${with_dbmalloc}${with_purify}${with_valgrind}"
	case ${cf_doalloc} in
	(*yes*)
		;;
	(*)
		AC_DEFINE(DOALLOC,10000,[Define to size of malloc-array])
		;;
	esac
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_PATHLIST version: 13 updated: 2021/09/04 06:35:04
dnl ----------------
dnl Process an option specifying a list of colon-separated paths.
dnl
dnl $1 = option name
dnl $2 = help-text
dnl $3 = environment variable to set
dnl $4 = default value, shown in the help-message, must be a constant
dnl $5 = default value, if it is an expression & cannot be in the help-message
dnl $6 = flag to tell if we want to define or substitute
dnl
AC_DEFUN([CF_WITH_PATHLIST],[
AC_REQUIRE([CF_PATHSEP])
AC_ARG_WITH($1,[$2 ](default: ifelse($4,,empty,$4)),,
ifelse($4,,[withval=${$3}],[withval=${$3:-ifelse($5,,$4,$5)}]))dnl

IFS="${IFS:- 	}"; ac_save_ifs="$IFS"; IFS="${PATH_SEPARATOR}"
cf_dst_path=
for cf_src_path in $withval
do
  CF_PATH_SYNTAX(cf_src_path)
  test -n "$cf_dst_path" && cf_dst_path="${cf_dst_path}$PATH_SEPARATOR"
  cf_dst_path="${cf_dst_path}${cf_src_path}"
done
IFS="$ac_save_ifs"

ifelse($6,define,[
# Strip single quotes from the value, e.g., when it was supplied as a literal
# for $4 or $5.
case "$cf_dst_path" in
(\'*)
  cf_dst_path="`echo "$cf_dst_path" |sed -e s/\'// -e s/\'\$//`"
  ;;
esac
cf_dst_path=`echo "$cf_dst_path" | sed -e 's/\\\\/\\\\\\\\/g'`
])

# This may use the prefix/exec_prefix symbols which will only yield "NONE"
# so we have to check/work around.  We do prefer the result of "eval"...
eval cf_dst_eval="$cf_dst_path"
case "x$cf_dst_eval" in
(xNONE*)
	$3=$cf_dst_path
	;;
(*)
	$3="$cf_dst_eval"
	;;
esac
AC_SUBST($3)dnl

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_PERL version: 11 updated: 2023/01/15 08:35:14
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

PERL_XSUBPP=

if test "$with_perl" = yes ; then
	CF_PROG_PERL(5.004)
	if test "x$PERL" = xno; then
		AC_WARN([perl not found])
	else
		cf_perl_CFLAGS="$CFLAGS"
		cf_perl_CPPFLAGS="$CPPFLAGS"
		cf_perl_link="$ac_link"
		cf_perl_LIBS="$LIBS"

		cf_perl_prefix="`$PERL -MConfig -e 'print $Config{shrpenv}'`"
		cf_perl_ccopts="`$PERL -MExtUtils::Embed -e ccopts`"
		cf_perl_ldopts="`$PERL -MExtUtils::Embed -e ldopts`"

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
		perl_parse(interp, NULL, 0, NULL, NULL);
		Perl_croak((void*)"Why:%s\\n", "Bye!");
],[
		eval `$PERL -le 'for $f (qw/xsubpp typemap/) {
				   @p = grep -f, map "$_/ExtUtils/$f", @INC;
				   print "cf_path_$f=", $p[[0]];
				 }'`

		PERL_XSUBPP="${cf_path_xsubpp:?} -typemap ${cf_path_typemap:?}"

		CF_VERBOSE(setting perl xs compiler to $PERL_XSUBPP)

		AC_DEFINE(OPT_PERL,1,[Define to 1 if we should compile-in the perl extension])

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
AC_SUBST(PERL)
AC_SUBST(PERL_XSUBPP)
AC_SUBST(EXTRA_INSTALL_DIRS)
AC_SUBST(EXTRA_INSTALL_FILES)
])
dnl ---------------------------------------------------------------------------
dnl CF_WITH_PIXMAPDIR version: 3 updated: 2012/07/22 09:18:02
dnl -----------------
dnl Handle configure option "--with-pixmapdir", setting these shell variables:
dnl
dnl $PIXMAPDIR is the option value, used for installing pixmap files.
dnl $no_pixmapdir is a "#" (comment) if "--without-pixmapdir" is given.
AC_DEFUN([CF_WITH_PIXMAPDIR],[
AC_MSG_CHECKING(for directory to install pixmaps)
AC_ARG_WITH(pixmapdir,
	[  --with-pixmapdir=DIR    directory in which to install pixmaps (DATADIR/pixmaps)],
	[PIXMAPDIR=$withval],
	[test -z "$PIXMAPDIR" && PIXMAPDIR='${datadir}/pixmaps'])

if test "x[$]PIXMAPDIR" = xauto
then
	PIXMAPDIR='${datadir}/pixmaps'
	for cf_path in \
		/usr/share/pixmaps \
		/usr/X11R6/share/pixmaps
	do
		if test -d "$cf_path" ; then
			PIXMAPDIR="$cf_path"
			break
		fi
	done
else
	cf_path=$PIXMAPDIR
	CF_PATH_SYNTAX(cf_path)
fi
AC_MSG_RESULT($PIXMAPDIR)
AC_SUBST(PIXMAPDIR)

no_pixmapdir=
if test "$PIXMAPDIR" = no
then
	no_pixmapdir="#"
else
	EXTRA_INSTALL_DIRS="$EXTRA_INSTALL_DIRS \$(PIXMAPDIR)"
fi
AC_SUBST(no_pixmapdir)
])dnl
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
dnl CF_WITH_SYMLINK version: 7 updated: 2023/11/22 19:51:11
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
if test "$program_transform_name" != "CF__INIT_TRANSFORM" ; then
cf_name=`echo "$program_transform_name" | sed -e '[s,\\$\\$,$,g]'`
cf_name=`echo "$2" |sed -e "$cf_name"`
AC_MSG_CHECKING(for symbolic link to create to $cf_name)
AC_ARG_WITH(symlink,
	[[  --with-symlink[=XXX]      make symbolic link to installed application]],
	[with_symlink=$withval],
	[with_symlink=$3])
AC_MSG_RESULT($with_symlink)
test "$with_symlink" = yes && with_symlink=$2
test -n "$with_symlink" && \
	test "$with_symlink" != no && \
	test "$with_symlink" != "$cf_name" && \
	$1="$with_symlink"
else
	with_symlink=$3
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_WITH_SYSTYPE version: 1 updated: 2013/01/26 16:26:12
dnl ---------------
dnl For testing, override the derived host system-type which is used to decide
dnl things such as the linker commands used to build shared libraries.  This is
dnl normally chosen automatically based on the type of system which you are
dnl building on.  We use it for testing the configure script.
dnl
dnl This is different from the --host option: it is used only for testing parts
dnl of the configure script which would not be reachable with --host since that
dnl relies on the build environment being real, rather than mocked up.
AC_DEFUN([CF_WITH_SYSTYPE],[
CF_CHECK_CACHE([AC_CANONICAL_SYSTEM])
AC_ARG_WITH(system-type,
	[  --with-system-type=XXX  test: override derived host system-type],
[AC_MSG_WARN(overriding system type to $withval)
	cf_cv_system_name=$withval
	host_os=$withval
])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_VALGRIND version: 1 updated: 2006/12/14 18:00:21
dnl ----------------
AC_DEFUN([CF_WITH_VALGRIND],[
CF_NO_LEAKS_OPTION(valgrind,
	[  --with-valgrind         test: use valgrind],
	[USE_VALGRIND])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_XPM version: 4 updated: 2023/11/23 06:40:35
dnl -----------
dnl Test for Xpm library, update compiler/loader flags if it is wanted and
dnl found.
dnl
dnl Also sets ICON_SUFFIX
AC_DEFUN([CF_WITH_XPM],
[
ICON_SUFFIX=.xbm

cf_save_cppflags="${CPPFLAGS}"
cf_save_ldflags="${LDFLAGS}"

AC_MSG_CHECKING(if you want to use the Xpm library for colored icon)
AC_ARG_WITH(xpm,
[[  --with-xpm[=DIR]        use Xpm library for colored icon, may specify path]],
	[cf_Xpm_library="$withval"],
	[cf_Xpm_library=yes])
AC_MSG_RESULT($cf_Xpm_library)

if test "$cf_Xpm_library" != no ; then
    if test "$cf_Xpm_library" != yes ; then
	CPPFLAGS="$CPPFLAGS -I$withval/include"
	LDFLAGS="$LDFLAGS -L$withval/lib"
    fi
    AC_CHECK_HEADER(X11/xpm.h,[
	AC_CHECK_LIB(Xpm, XpmCreatePixmapFromData,[
	    AC_DEFINE(HAVE_LIBXPM,1,[Define to 1 if we should use Xpm library])
	    ICON_SUFFIX=.xpm
	    LIBS="-lXpm $LIBS"],
	    [CPPFLAGS="${cf_save_cppflags}" LDFLAGS="${cf_save_ldflags}"],
	    [-lX11 $X_LIBS])],
	[CPPFLAGS="${cf_save_cppflags}" LDFLAGS="${cf_save_ldflags}"])
fi

AC_SUBST(ICON_SUFFIX)
])dnl
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
dnl CF_XOPEN_CURSES version: 20 updated: 2024/01/07 06:54:12
dnl ---------------
dnl Test if we should define X/Open source for curses, needed on Digital Unix
dnl 4.x, to see the extended functions, but breaks on IRIX 6.x.
dnl
dnl The getbegyx() check is needed for HPUX, which omits legacy macros such
dnl as getbegy().  The latter is better design, but the former is standard.
AC_DEFUN([CF_XOPEN_CURSES],
[
AC_REQUIRE([CF_CURSES_CPPFLAGS])dnl
AC_CACHE_CHECK(definition to turn on extended curses functions,cf_cv_need_xopen_extension,[
cf_cv_need_xopen_extension=unknown
AC_TRY_LINK([
$ac_includes_default
#include <${cf_cv_ncurses_header:-curses.h}>],[
#if defined(NCURSES_VERSION_PATCH)
#if (NCURSES_VERSION_PATCH < 20100501) && (NCURSES_VERSION_PATCH >= 20100403)
	#error disallow ncurses versions between 2020/04/03 and 2010/05/01
#endif
#endif
#ifdef NCURSES_WIDECHAR
#error prefer to fall-through on the second checks
#endif
	static char dummy[10];
	cchar_t check;
	int check2 = curs_set((int)sizeof(check));
	long x = winnstr(stdscr, dummy, 5);
	int x1, y1;
	(void)check2;
	getbegyx(stdscr, y1, x1);
	(void)x;
	(void)y1;
	(void)x1;
	],
	[cf_cv_need_xopen_extension=none],
	[
	for cf_try_xopen_extension in _XOPEN_SOURCE_EXTENDED NCURSES_WIDECHAR
	do
		AC_TRY_LINK([
#define $cf_try_xopen_extension 1
$ac_includes_default
#include <${cf_cv_ncurses_header:-curses.h}>],[
		static char dummy[10];
		cchar_t check;
		int check2 = curs_set((int)sizeof(check));
		long x = winnstr(stdscr, dummy, 5);
		int x1, y1;
		getbegyx(stdscr, y1, x1);
		(void)check2;
		(void)x;
		(void)y1;
		(void)x1;
		],
		[cf_cv_need_xopen_extension=$cf_try_xopen_extension; break])
	done
	])
])

case "$cf_cv_need_xopen_extension" in
(*_*)
	CF_APPEND_TEXT(CPPFLAGS,-D$cf_cv_need_xopen_extension)
	;;
esac

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_XOPEN_SOURCE version: 68 updated: 2024/11/09 18:07:29
dnl ---------------
dnl Try to get _XOPEN_SOURCE defined properly that we can use POSIX functions,
dnl or adapt to the vendor's definitions to get equivalent functionality,
dnl without losing the common non-POSIX features.
dnl
dnl Parameters:
dnl	$1 is the nominal value for _XOPEN_SOURCE
dnl	$2 is the nominal value for _POSIX_C_SOURCE
dnl
dnl The default case prefers _XOPEN_SOURCE over _POSIX_C_SOURCE if the
dnl implementation predefines it, because X/Open and most implementations agree
dnl that the latter is a legacy or "aligned" value.
dnl
dnl Because _XOPEN_SOURCE is preferred, if defining _POSIX_C_SOURCE turns
dnl that off, then refrain from setting _POSIX_C_SOURCE explicitly.
dnl
dnl References:
dnl https://pubs.opengroup.org/onlinepubs/007904975/functions/xsh_chap02_02.html
dnl https://docs.oracle.com/cd/E19253-01/816-5175/standards-5/index.html
dnl https://www.gnu.org/software/libc/manual/html_node/Feature-Test-Macros.html
AC_DEFUN([CF_XOPEN_SOURCE],[
AC_REQUIRE([AC_CANONICAL_HOST])
AC_REQUIRE([CF_POSIX_VISIBLE])

if test "$cf_cv_posix_visible" = no; then

cf_XOPEN_SOURCE=ifelse([$1],,500,[$1])
cf_POSIX_C_SOURCE=ifelse([$2],,199506L,[$2])
cf_xopen_source=

case "$host_os" in
(aix[[4-7]]*)
	cf_xopen_source="-D_ALL_SOURCE"
	;;
(darwin[[0-8]].*)
	cf_xopen_source="-D_APPLE_C_SOURCE"
	;;
(darwin*)
	cf_xopen_source="-D_DARWIN_C_SOURCE"
	cf_XOPEN_SOURCE=
	;;
(freebsd*|dragonfly*|midnightbsd*)
	# 5.x headers associate
	#	_XOPEN_SOURCE=600 with _POSIX_C_SOURCE=200112L
	#	_XOPEN_SOURCE=500 with _POSIX_C_SOURCE=199506L
	cf_POSIX_C_SOURCE=200112L
	cf_XOPEN_SOURCE=600
	cf_xopen_source="-D_BSD_TYPES -D__BSD_VISIBLE -D_POSIX_C_SOURCE=$cf_POSIX_C_SOURCE -D_XOPEN_SOURCE=$cf_XOPEN_SOURCE"
	;;
(hpux11*)
	cf_xopen_source="-D_HPUX_SOURCE -D_XOPEN_SOURCE=500"
	;;
(hpux*)
	cf_xopen_source="-D_HPUX_SOURCE"
	;;
(irix[[56]].*)
	cf_xopen_source="-D_SGI_SOURCE"
	cf_XOPEN_SOURCE=
	;;
(linux*gnu|linux*gnuabi64|linux*gnuabin32|linux*gnueabi|linux*gnueabihf|linux*gnux32|uclinux*|gnu*|mint*|k*bsd*-gnu|cygwin|msys|mingw*|linux*uclibc)
	CF_GNU_SOURCE($cf_XOPEN_SOURCE)
	;;
linux*musl)
	cf_xopen_source="-D_BSD_SOURCE"
	;;
(minix*)
	cf_xopen_source="-D_NETBSD_SOURCE" # POSIX.1-2001 features are ifdef'd with this...
	;;
(mirbsd*)
	# setting _XOPEN_SOURCE or _POSIX_SOURCE breaks <sys/select.h> and other headers which use u_int / u_short types
	cf_XOPEN_SOURCE=
	CF_POSIX_C_SOURCE($cf_POSIX_C_SOURCE)
	;;
(netbsd*)
	cf_xopen_source="-D_NETBSD_SOURCE" # setting _XOPEN_SOURCE breaks IPv6 for lynx on NetBSD 1.6, breaks xterm, is not needed for ncursesw
	;;
(openbsd[[6-9]]*)
	# OpenBSD 6.x has broken locale support, both compile-time and runtime.
	# see https://www.mail-archive.com/bugs@openbsd.org/msg13200.html
	# Abusing the conformance level is a workaround.
	AC_MSG_WARN(this system does not provide usable locale support)
	cf_xopen_source="-D_BSD_SOURCE"
	cf_XOPEN_SOURCE=700
	;;
(openbsd[[4-5]]*)
	# setting _XOPEN_SOURCE lower than 500 breaks g++ compile with wchar.h, needed for ncursesw
	cf_xopen_source="-D_BSD_SOURCE"
	cf_XOPEN_SOURCE=600
	;;
(openbsd*)
	# setting _XOPEN_SOURCE breaks xterm on OpenBSD 2.8, is not needed for ncursesw
	;;
(osf[[45]]*)
	cf_xopen_source="-D_OSF_SOURCE"
	;;
(nto-qnx*)
	cf_xopen_source="-D_QNX_SOURCE"
	;;
(sco*)
	# setting _XOPEN_SOURCE breaks Lynx on SCO Unix / OpenServer
	;;
(solaris2.*)
	cf_xopen_source="-D__EXTENSIONS__"
	cf_cv_xopen_source=broken
	;;
(sysv4.2uw2.*) # Novell/SCO UnixWare 2.x (tested on 2.1.2)
	cf_XOPEN_SOURCE=
	cf_POSIX_C_SOURCE=
	;;
(*)
	CF_TRY_XOPEN_SOURCE
	cf_save_xopen_cppflags="$CPPFLAGS"
	CF_POSIX_C_SOURCE($cf_POSIX_C_SOURCE)
	# Some of these niche implementations use copy/paste, double-check...
	if test "$cf_cv_xopen_source" = no ; then
		CF_VERBOSE(checking if _POSIX_C_SOURCE interferes with _XOPEN_SOURCE)
		AC_TRY_COMPILE(CF__XOPEN_SOURCE_HEAD,CF__XOPEN_SOURCE_BODY,,[
			AC_MSG_WARN(_POSIX_C_SOURCE definition is not usable)
			CPPFLAGS="$cf_save_xopen_cppflags"])
	fi
	;;
esac

if test -n "$cf_xopen_source" ; then
	CF_APPEND_CFLAGS($cf_xopen_source,true)
fi

dnl In anything but the default case, we may have system-specific setting
dnl which is still not guaranteed to provide all of the entrypoints that
dnl _XOPEN_SOURCE would yield.
if test -n "$cf_XOPEN_SOURCE" && test -z "$cf_cv_xopen_source" ; then
	AC_MSG_CHECKING(if _XOPEN_SOURCE really is set)
	AC_TRY_COMPILE([#include <stdlib.h>],[
#ifndef _XOPEN_SOURCE
#error _XOPEN_SOURCE is not defined
#endif],
	[cf_XOPEN_SOURCE_set=yes],
	[cf_XOPEN_SOURCE_set=no])
	AC_MSG_RESULT($cf_XOPEN_SOURCE_set)
	if test "$cf_XOPEN_SOURCE_set" = yes
	then
		AC_TRY_COMPILE([#include <stdlib.h>],[
#if (_XOPEN_SOURCE - 0) < $cf_XOPEN_SOURCE
#error (_XOPEN_SOURCE - 0) < $cf_XOPEN_SOURCE
#endif],
		[cf_XOPEN_SOURCE_set_ok=yes],
		[cf_XOPEN_SOURCE_set_ok=no])
		if test "$cf_XOPEN_SOURCE_set_ok" = no
		then
			AC_MSG_WARN(_XOPEN_SOURCE is lower than requested)
		fi
	else
		CF_TRY_XOPEN_SOURCE
	fi
fi
fi # cf_cv_posix_visible
])
dnl ---------------------------------------------------------------------------
dnl CF_X_ATHENA version: 25 updated: 2023/01/11 04:05:23
dnl -----------
dnl Check for Xaw (Athena) libraries
dnl
dnl Sets $cf_x_athena according to the flavor of Xaw which is used.
AC_DEFUN([CF_X_ATHENA],
[
cf_x_athena=${cf_x_athena:-Xaw}

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

AC_MSG_CHECKING(if you want to link with Xaw 3d xft library)
withval=
AC_ARG_WITH(Xaw3dxft,
	[  --with-Xaw3dxft         link with Xaw 3d xft library])
if test "$withval" = yes ; then
	cf_x_athena=Xaw3dxft
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

			CF_TRIM_X_LIBS

AC_CACHE_CHECK(for usable $cf_x_athena/Xmu package,cf_cv_xaw_compat,[
AC_TRY_LINK([
$ac_includes_default
#include <X11/Xmu/CharSet.h>
],[
int check = XmuCompareISOLatin1("big", "small");
(void)check;
],[cf_cv_xaw_compat=yes],[cf_cv_xaw_compat=no])])

			if test "$cf_cv_xaw_compat" = no
			then
				# workaround for broken ".pc" files...
				case "$cf_x_athena_lib" in
				(*-lXmu*)
					;;
				(*)
					CF_VERBOSE(work around broken package)
					cf_save_xmu="$LIBS"
					cf_first_lib=`echo "$cf_save_xmu" | sed -e 's/^[ ][ ]*//' -e 's/ .*//'`
					CF_TRY_PKG_CONFIG(xmu,[
							LIBS="$cf_save_xmu"
							CF_ADD_LIB_AFTER($cf_first_lib,$cf_pkgconfig_libs)
						],[
							CF_ADD_LIB_AFTER($cf_first_lib,-lXmu)
						])
					CF_TRIM_X_LIBS
					;;
				esac
			fi

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
dnl CF_X_ATHENA_CPPFLAGS version: 9 updated: 2020/12/31 10:54:15
dnl --------------------
dnl Normally invoked by CF_X_ATHENA, with $1 set to the appropriate flavor of
dnl the Athena widgets, e.g., Xaw, Xaw3d, neXtaw.
AC_DEFUN([CF_X_ATHENA_CPPFLAGS],
[
AC_REQUIRE([AC_PATH_XTRA])
cf_x_athena_root=ifelse([$1],,Xaw,[$1])
cf_x_athena_inc=""

for cf_path in default \
	/usr/contrib/X11R6 \
	/usr/contrib/X11R5 \
	/usr/lib/X11R5 \
	/usr/local
do
	if test -z "$cf_x_athena_inc" ; then
		CF_SAVE_XTRA_FLAGS([CF_X_ATHENA_CPPFLAGS])
		cf_test=X11/$cf_x_athena_root/SimpleMenu.h
		if test "$cf_path" != default ; then
			CF_APPEND_TEXT(CPPFLAGS,-I$cf_path/include)
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
		CF_RESTORE_XTRA_FLAGS([CF_X_ATHENA_CPPFLAGS])
		if test "$cf_result" = yes ; then
			test "$cf_path"  = default && cf_x_athena_inc=default
			test "$cf_path" != default && cf_x_athena_inc="$cf_path/include"
			break
		fi
	fi
done

if test -z "$cf_x_athena_inc" ; then
	AC_MSG_WARN([Unable to find Athena header files])
elif test "$cf_x_athena_inc" != default ; then
	CF_APPEND_TEXT(CPPFLAGS,-I$cf_x_athena_inc)
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_X_ATHENA_LIBS version: 14 updated: 2023/01/11 04:05:23
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
		${cf_x_athena_root} \
		${cf_x_athena_root}7 \
		${cf_x_athena_root}6
	do
	for cf_libs in \
		"-l$cf_lib -lXmu" \
		"-l$cf_lib -lXpm -lXmu" \
		"-l${cf_lib}_s -lXmu_s"
	do
		test -n "$cf_x_athena_lib" && break

		CF_SAVE_XTRA_FLAGS([CF_X_ATHENA_LIBS])
		cf_test=XawSimpleMenuAddGlobalActions
		test "$cf_path" != default && cf_libs="-L$cf_path/lib $cf_libs"
		CF_ADD_LIBS($cf_libs)
		AC_MSG_CHECKING(for $cf_test in $cf_libs)
		AC_TRY_LINK([
$ac_includes_default
#include <X11/Intrinsic.h>
#include <X11/$cf_x_athena_root/SimpleMenu.h>
],[
$cf_test((XtAppContext) 0)],
			[cf_result=yes],
			[cf_result=no])
		AC_MSG_RESULT($cf_result)
		CF_RESTORE_XTRA_FLAGS([CF_X_ATHENA_LIBS])

		if test "$cf_result" = yes ; then
			cf_x_athena_lib="$cf_libs"
			break
		fi
	done # cf_libs
		test -n "$cf_x_athena_lib" && break
	done # cf_lib
done

if test -z "$cf_x_athena_lib" ; then
	AC_MSG_ERROR(
[Unable to successfully link Athena library (-l$cf_x_athena_root) with test program])
fi

CF_ADD_LIBS($cf_x_athena_lib)
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
dnl CF_X_FONTCONFIG version: 8 updated: 2024/11/25 04:10:12
dnl ---------------
dnl Check for fontconfig library, a dependency of the X FreeType library.
AC_DEFUN([CF_X_FONTCONFIG],
[
AC_REQUIRE([CF_X_FREETYPE])

if test "$cf_cv_found_freetype" = yes ; then
AC_CACHE_CHECK(for usable Xft/fontconfig package,cf_cv_xft_compat,[
AC_TRY_LINK([
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
],[
	XftPattern *pat = 0;
	XftPatternBuild(pat,
					XFT_FAMILY, XftTypeString, "mono",
					(void *) 0);
],[cf_cv_xft_compat=yes],[cf_cv_xft_compat=no])
])

if test "$cf_cv_xft_compat" = no
then
	# workaround for broken ".pc" files used for Xft.
	case "$cf_cv_x_freetype_libs" in
	(*-lfontconfig*)
		;;
	(*)
		CF_VERBOSE(work around broken package)
		cf_save_fontconfig="$LIBS"
		CF_TRY_PKG_CONFIG(fontconfig,[
				CF_ADD_CFLAGS($cf_pkgconfig_incs)
				LIBS="$cf_save_fontconfig"
				CF_ADD_LIB_AFTER(-lXft,$cf_pkgconfig_libs)
			],[
				CF_ADD_LIB_AFTER(-lXft,-lfontconfig)
			])
		;;
	esac
fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_X_FREETYPE version: 28 updated: 2020/03/10 18:53:47
dnl -------------
dnl Check for X FreeType headers and libraries (XFree86 4.x, etc).
dnl
dnl First check for the appropriate config program, since the developers for
dnl these libraries change their configuration (and config program) more or
dnl less randomly.  If we cannot find the config program, do not bother trying
dnl to guess the latest variation of include/lib directories.
dnl
dnl If either or both of these configure-script options are not given, rely on
dnl the output of the config program to provide the cflags/libs options:
dnl	--with-freetype-cflags
dnl	--with-freetype-libs
AC_DEFUN([CF_X_FREETYPE],
[
AC_REQUIRE([CF_PKG_CONFIG])

cf_cv_x_freetype_incs=no
cf_cv_x_freetype_libs=no
cf_extra_freetype_libs=
FREETYPE_CONFIG=none
FREETYPE_PARAMS=

AC_MSG_CHECKING(for FreeType configuration script)
AC_ARG_WITH(freetype-config,
	[  --with-freetype-config  configure script to use for FreeType],
	[cf_cv_x_freetype_cfgs="$withval"],
	[cf_cv_x_freetype_cfgs=auto])
test -z $cf_cv_x_freetype_cfgs && cf_cv_x_freetype_cfgs=auto
test $cf_cv_x_freetype_cfgs = no && cf_cv_x_freetype_cfgs=none
AC_MSG_RESULT($cf_cv_x_freetype_cfgs)

case $cf_cv_x_freetype_cfgs in
(none)
	AC_MSG_CHECKING(if you specified -D/-I options for FreeType)
	AC_ARG_WITH(freetype-cflags,
		[  --with-freetype-cflags  -D/-I options for compiling with FreeType],
		[cf_cv_x_freetype_incs="$with_freetype_cflags"],
		[cf_cv_x_freetype_incs=no])
	AC_MSG_RESULT($cf_cv_x_freetype_incs)

	AC_MSG_CHECKING(if you specified -L/-l options for FreeType)
	AC_ARG_WITH(freetype-libs,
		[  --with-freetype-libs    -L/-l options to link FreeType],
		[cf_cv_x_freetype_libs="$with_freetype_libs"],
		[cf_cv_x_freetype_libs=no])
	AC_MSG_RESULT($cf_cv_x_freetype_libs)
	;;
(auto)
	if test "$PKG_CONFIG" != none && "$PKG_CONFIG" --exists xft; then
		FREETYPE_CONFIG=$PKG_CONFIG
		FREETYPE_PARAMS=xft
	else
		AC_PATH_PROG(FREETYPE_CONFIG, freetype-config, none)
		if test "$FREETYPE_CONFIG" != none; then
			FREETYPE_CONFIG=$FREETYPE_CONFIG
			cf_extra_freetype_libs="-lXft"
		else
			AC_PATH_PROG(FREETYPE_OLD_CONFIG, xft-config, none)
			if test "$FREETYPE_OLD_CONFIG" != none; then
				FREETYPE_CONFIG=$FREETYPE_OLD_CONFIG
			fi
		fi
	fi
	;;
(pkg*)
	if test "$PKG_CONFIG" != none && "$PKG_CONFIG" --exists xft; then
		FREETYPE_CONFIG=$cf_cv_x_freetype_cfgs
		FREETYPE_PARAMS=xft
	else
		AC_MSG_WARN(cannot find pkg-config for Xft)
	fi
	;;
(*)
	AC_PATH_PROG(FREETYPE_XFT_CONFIG, $cf_cv_x_freetype_cfgs, none)
	if test "$FREETYPE_XFT_CONFIG" != none; then
		FREETYPE_CONFIG=$FREETYPE_XFT_CONFIG
	else
		AC_MSG_WARN(cannot find config script for Xft)
	fi
	;;
esac

if test "$FREETYPE_CONFIG" != none ; then
	AC_MSG_CHECKING(for FreeType config)
	AC_MSG_RESULT($FREETYPE_CONFIG $FREETYPE_PARAMS)

	if test "$cf_cv_x_freetype_incs" = no ; then
		AC_MSG_CHECKING(for $FREETYPE_CONFIG cflags)
		cf_cv_x_freetype_incs="`$FREETYPE_CONFIG $FREETYPE_PARAMS --cflags 2>/dev/null`"
		AC_MSG_RESULT($cf_cv_x_freetype_incs)
	fi

	if test "$cf_cv_x_freetype_libs" = no ; then
		AC_MSG_CHECKING(for $FREETYPE_CONFIG libs)
		cf_cv_x_freetype_libs="$cf_extra_freetype_libs `$FREETYPE_CONFIG $FREETYPE_PARAMS --libs 2>/dev/null`"
		AC_MSG_RESULT($cf_cv_x_freetype_libs)
	fi
fi

if test "$cf_cv_x_freetype_incs" = no ; then
	cf_cv_x_freetype_incs=
fi

if test "$cf_cv_x_freetype_libs" = no ; then
	cf_cv_x_freetype_libs=-lXft
fi

AC_MSG_CHECKING(if we can link with FreeType libraries)

cf_save_LIBS="$LIBS"
cf_save_INCS="$CPPFLAGS"

CF_ADD_LIBS($cf_cv_x_freetype_libs)
CPPFLAGS="$CPPFLAGS $cf_cv_x_freetype_incs"

AC_TRY_LINK([
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>],[
	XftPattern  *pat = XftNameParse ("name"); (void)pat],
	[cf_cv_found_freetype=yes],
	[cf_cv_found_freetype=no])
AC_MSG_RESULT($cf_cv_found_freetype)

LIBS="$cf_save_LIBS"
CPPFLAGS="$cf_save_INCS"

if test "$cf_cv_found_freetype" = yes ; then
	CF_ADD_LIBS($cf_cv_x_freetype_libs)
	CF_ADD_CFLAGS($cf_cv_x_freetype_incs)
	AC_DEFINE(XRENDERFONT,1,[Define to 1 if we can/should link with FreeType libraries])

AC_CHECK_FUNCS( \
	XftDrawCharSpec \
	XftDrawSetClip \
	XftDrawSetClipRectangles \
)

else
	AC_MSG_WARN(No libraries found for FreeType)
	CPPFLAGS=`echo "$CPPFLAGS" | sed -e s/-DXRENDERFONT//`
fi

# FIXME: revisit this if needed
AC_SUBST(HAVE_TYPE_FCCHAR32)
AC_SUBST(HAVE_TYPE_XFTCHARSPEC)
])
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
dnl CF_X_TOOLKIT version: 27 updated: 2023/01/11 04:05:23
dnl ------------
dnl Check for X Toolkit libraries
AC_DEFUN([CF_X_TOOLKIT],
[
AC_REQUIRE([AC_PATH_XTRA])
AC_REQUIRE([CF_CHECK_CACHE])

# OSX is schizoid about who owns /usr/X11 (old) versus /opt/X11 (new) (and
# in some cases has installed dummy files in the former, other cases replaced
# it with a link to the new location).  This complicates the configure script.
# Check for that pitfall, and recover using pkg-config
#
# If none of these are set, the configuration is almost certainly broken.
if test -z "${X_CFLAGS}${X_PRE_LIBS}${X_LIBS}${X_EXTRA_LIBS}"
then
	CF_TRY_PKG_CONFIG(x11,,[AC_MSG_WARN(unable to find X11 library)])
	CF_TRY_PKG_CONFIG(ice,,[AC_MSG_WARN(unable to find ICE library)])
	CF_TRY_PKG_CONFIG(sm,,[AC_MSG_WARN(unable to find SM library)])
	CF_TRY_PKG_CONFIG(xt,,[AC_MSG_WARN(unable to find Xt library)])
else
	LIBS="$X_PRE_LIBS $LIBS $X_EXTRA_LIBS"
fi

cf_have_X_LIBS=no

CF_TRY_PKG_CONFIG(xt,[

	case "x$LIBS" in
	(*-lX11*)
		;;
	(*)
# we have an "xt" package, but it may omit Xt's dependency on X11
AC_CACHE_CHECK(for usable X dependency,cf_cv_xt_x11_compat,[
AC_TRY_LINK([
$ac_includes_default
#include <X11/Xlib.h>
],[
	int rc1 = XDrawLine((Display*) 0, (Drawable) 0, (GC) 0, 0, 0, 0, 0);
	int rc2 = XClearWindow((Display*) 0, (Window) 0);
	int rc3 = XMoveWindow((Display*) 0, (Window) 0, 0, 0);
	int rc4 = XMoveResizeWindow((Display*)0, (Window)0, 0, 0, 0, 0);
],[cf_cv_xt_x11_compat=yes],[cf_cv_xt_x11_compat=no])])
		if test "$cf_cv_xt_x11_compat" = no
		then
			CF_VERBOSE(work around broken X11 dependency)
			# 2010/11/19 - good enough until a working Xt on Xcb is delivered.
			CF_TRY_PKG_CONFIG(x11,,[CF_ADD_LIB_AFTER(-lXt,-lX11)])
		fi
		;;
	esac

AC_CACHE_CHECK(for usable X Toolkit package,cf_cv_xt_ice_compat,[
AC_TRY_LINK([
$ac_includes_default
#include <X11/Shell.h>
],[int num = IceConnectionNumber(0); (void) num
],[cf_cv_xt_ice_compat=yes],[cf_cv_xt_ice_compat=no])])

	if test "$cf_cv_xt_ice_compat" = no
	then
		# workaround for broken ".pc" files used for X Toolkit.
		case "x$X_PRE_LIBS" in
		(*-lICE*)
			case "x$LIBS" in
			(*-lICE*)
				;;
			(*)
				CF_VERBOSE(work around broken ICE dependency)
				CF_TRY_PKG_CONFIG(ice,
					[CF_TRY_PKG_CONFIG(sm)],
					[CF_ADD_LIB_AFTER(-lXt,$X_PRE_LIBS)])
				;;
			esac
			;;
		esac
	fi

	cf_have_X_LIBS=yes
],[

	LDFLAGS="$X_LIBS $LDFLAGS"
	CF_CHECK_CFLAGS($X_CFLAGS)

	AC_CHECK_FUNC(XOpenDisplay,,[
	AC_CHECK_LIB(X11,XOpenDisplay,
		[CF_ADD_LIB(X11)])])

	AC_CHECK_FUNC(XtAppInitialize,,[
	AC_CHECK_LIB(Xt, XtAppInitialize,
		[AC_DEFINE(HAVE_LIBXT,1,[Define to 1 if we can compile with the Xt library])
		 cf_have_X_LIBS=Xt
		 LIBS="-lXt $LIBS"])])
])

if test "$cf_have_X_LIBS" = no ; then
	AC_MSG_WARN(
[Unable to successfully link X Toolkit library (-lXt) with
test program.  You will have to check and add the proper libraries by hand
to makefile.])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF__CURSES_HEAD version: 2 updated: 2010/10/23 15:54:49
dnl ---------------
dnl Define a reusable chunk which includes <curses.h> and <term.h> when they
dnl are both available.
define([CF__CURSES_HEAD],[
#ifdef HAVE_XCURSES
#include <xcurses.h>
char * XCursesProgramName = "test";
#else
#include <${cf_cv_ncurses_header:-curses.h}>
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
dnl ---------------------------------------------------------------------------
dnl CF__XOPEN_SOURCE_BODY version: 2 updated: 2023/02/18 17:41:25
dnl ---------------------
dnl body of test when test-compiling for _XOPEN_SOURCE check
define([CF__XOPEN_SOURCE_BODY],
[
#ifndef _XOPEN_SOURCE
#error _XOPEN_SOURCE is not defined
#endif
])
dnl ---------------------------------------------------------------------------
dnl CF__XOPEN_SOURCE_HEAD version: 2 updated: 2023/02/18 17:41:25
dnl ---------------------
dnl headers to include when test-compiling for _XOPEN_SOURCE check
define([CF__XOPEN_SOURCE_HEAD],
[
$ac_includes_default
])
