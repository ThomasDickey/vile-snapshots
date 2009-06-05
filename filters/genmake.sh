#!/bin/sh
# $Header: /users/source/archives/vile.vcs/filters/RCS/genmake.sh,v 1.8 2009/06/04 23:04:01 tom Exp $
# Scan the source-files in the "filters" directory to obtain the names which
# are used for the default symbol table for each filter.  Update genmake.mak
# if the lists differ.
#
# The genmake.mak file is used for constructing the filters/makefile (or
# filters/makefile.wnt).  For non-Unix platforms, it's relatively difficult
# to generate.

LANG=C;       export LANG
LC_ALL=C;     export LC_ALL
LC_CTYPE=C;   export LC_CTYPE
LANGUAGE=C;   export LANGUAGE
LC_COLLATE=C; export LC_COLLATE

TARGET=genmake.mak
SOURCE=.

if test $# != 0 ; then
	TARGET=$1
	shift
fi

if test $# != 0 ; then
	SOURCE=$1
	shift
fi

oldmask=`umask`
umask 077

PID=$$
DATA=${TMPDIR-.}/data.gen$PID
TEMP=${TMPDIR-.}/temp.gen$PID
SORT=${TMPDIR-.}/sort.gen$PID
fgrep 'DefineOptFilter("' $SOURCE/*.[cl] | \
	sed	-e 's,^.*/,,' \
		-e 's/",.*//' \
		-e 's/:.*"/ /' \
		-e 's/\./ /' >$TEMP
fgrep 'DefineFilter("' $SOURCE/*.[cl] | \
	sed	-e 's,^.*/,,' \
		-e 's/").*//' \
		-e 's/:.*"/ /' \
		-e 's/\./ /' >>$TEMP

# put the filter-name first, so it sorts by that field
sed -e 's/\([^ ]*\) \([^ ]*\) \([^ ]*\)/\3 \2 \1/' $TEMP | \
sort -u >$SORT
mv $SORT $TEMP

sed -e 's/@/$/g' >$DATA <<EOF
# @Header@
# This is a list of filter root names and whether .c or .l files define the
# filter.  Except for vile-crypt and vile-manfilt (which do not correspond to
# majormodes), the filter names are constructed as vile-{root}-filt.

EOF

fgrep ' c ' $TEMP >$SORT
while true
do
	read filter suffix filename
	test -z "$filter" && break

	TEXT="$filter	$filename"
	case `echo $filename | sed -e 's/././g'` in #(vi
	........) #(vi
		;;
	*)
		TEXT="$TEXT	"
		;;
	esac

	echo "$TEXT	$suffix" >>$DATA
done < $SORT

echo >>$DATA

fgrep ' l ' $TEMP >$SORT
while true
do
	read filter suffix filename
	test -z "$filter" && break

	TEXT="$filter	$filename"
	case `echo $filename | sed -e 's/././g'` in #(vi
	........) #(vi
		;;
	*)
		TEXT="$TEXT	"
		;;
	esac

	case $filename in #(vi
	lex-filt)
		FLEX="	flex"
		;;
	*)
		FLEX=
		;;
	esac

	echo "$TEXT	$suffix$FLEX" >>$DATA
done < $SORT

if test -f $TARGET ; then
	sed -e 's/: .*\$/$/' $TARGET >$TEMP
fi

umask $oldmask

if test -f $TARGET ; then
	diff $TEMP $DATA >$SORT
	if test -s $SORT ; then
		cat $SORT
		chmod u+w $TARGET
		mv $DATA $TARGET
		echo "** updated $TARGET"
	else
		echo "** no change to $TARGET"
	fi
else
	echo "** created $TARGET"
	mv $DATA $TARGET
fi

rm -f $TEMP $SORT $DATA
