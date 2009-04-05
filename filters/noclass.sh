#!/bin/sh
# $Id: noclass.sh,v 1.7 2009/04/04 13:24:27 tom Exp $
# support older versions of flex/lex which do not support character classes
# by expanding them into POSIX equivalents.
#
# Note: [:cntrl:] is omitted since it is hard to do portably, and is rarely
# used in lexical scanners.
#
# $1 is the lex/flex program to run
# $2+ are the parameters
LEX=$1
OPT=
SRC=
shift
while test $# != 0
do
	case $1 in
	*.l)
		SRC="$SRC $1"
		;;
	*)
		OPT="$OPT $1"
		;;
	esac
	shift
done
tmp=noclass$$.l
if test -n "$SRC" ; then
	blank=' \\t'
	lower='abcdefghijklmnopqrstuvwxyz'
	upper='ABCDEFGHIJKLMNOPQRSTUVWXYZ'
	digit='0123456789'
	punct='~!@#\$%\^\\\&*()_+\`{}|:\"<>?\\\[\\\]\\;'\'',.\/\-'
	sed	\
		-e '/^%pointer/d' \
		-e 's/\[:alpha:\]/'${lower}${upper}'/g' \
		-e 's/\[:upper:\]/'${upper}'/g' \
		-e 's/\[:lower:\]/'${lower}'/g' \
		-e 's/\[:alnum:\]/'${lower}${upper}${digit}'/g' \
		-e 's/\[:xdigit:\]/abcdefABCDEF'${digit}'/g' \
		-e 's/\[:blank:\]/'"${blank}"'/g' \
		-e 's/\[:space:\]/'"${blank}"'\\r\\n\\f/g' \
		-e 's/\[:digit:\]/'${digit}'/g' \
		-e 's/\[:punct:\]/'"${punct}"'/g' \
		-e 's/\[:graph:\]/'"${lower}${upper}${digit}${punct}"'/g' \
		-e 's/\[:print:\]/'"${lower}${upper}${digit}${blank}${punct}"'/g' \
		$SRC >$tmp
	$LEX $OPT $tmp
	code=$?
	rm -f $tmp
	#diff -c $SRC $tmp >&2
	#echo $tmp >&2
	exit $code
else
	echo '? no source found' >&2
	exit 1
fi
