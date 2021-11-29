#!/bin/sh
# $Id: sinstall.sh,v 1.2 1998/11/14 15:24:04 tom Exp $
#
# Install Perl scripts, adjusting for the correct pathname
#	$1 = name of perl program
#	$2 = install-program 
# The last two arguments are the source and target, install's options, if any,
# fall between.

SOURCE=
TARGET=
INSTALL=

if test $# = 0 ; then
	echo '? no parameter for $PERL'
	exit 1
else
	test -z "$PERL" && PERL="$1"
	shift
fi

while test $# != 0
do
	if test $# = 1 ; then
		TARGET="$1"
	elif test $# = 2 ; then
		SOURCE="$1"
	else
		INSTALL="$INSTALL $1"
	fi
	shift
done

if test -z "$INSTALL" ; then
	echo '? no parameter for $INSTALL'
	exit 1
fi

if test -z "$SOURCE" ; then
	echo '? no parameter for $SOURCE'
	exit 1
fi

if test -z "$TARGET" ; then
	echo '? no parameter for $TARGET'
	exit 1
fi

case $PERL in #(vi
/*)	#(vi
	;;
*)
	IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:"
	for p in $PATH
	do
		if test -f $p/$PERL ; then
			PERL="$p/$PERL"
			break
		fi
	done
	IFS="$ac_save_ifs"
	;;
esac

TEMP=sinstall.$$
trap "rm -f $TEMP; exit 99" 1 2 5 15
sed -e "s@/usr/bin/perl@$PERL@g" $SOURCE >$TEMP

$INSTALL $TEMP $TARGET
if test $? != 0 ; then
	rm -f $TEMP
	exit 1
fi
rm -f $TEMP

