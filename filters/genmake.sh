#!/bin/sh
# $Id: genmake.sh,v 1.13 2023/01/14 00:16:36 tom Exp $
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

: "${FGREP:=grep -F}"

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
DATA=${TMPDIR-.}/data$PID.gen
TEMP=${TMPDIR-.}/temp$PID.gen
SORT=${TMPDIR-.}/sort$PID.gen
DIFF=${TMPDIR-.}/diff$PID.gen
$FGREP 'DefineOptFilter(' $SOURCE/*.[cl] | \
	sed	-e 's,^.*/,,' \
		-e 's/,.*//' \
		-e 's/:.*(/ /' \
		-e 's/\./ /' >$TEMP
$FGREP 'DefineFilter(' $SOURCE/*.[cl] | \
	sed	-e 's,^.*/,,' \
		-e 's/).*//' \
		-e 's/:.*(/ /' \
		-e 's/\./ /' >>$TEMP

# put the filter-name first, so it sorts by that field
sed -e 's/\([^ ]*\) \([^ ]*\) \([^ ]*\)/\3 \2 \1/' $TEMP | \
sort -u >$SORT
mv $SORT $TEMP

sed -e 's/@/$/g' >$DATA <<EOF
# @Id@
# This is a list of filter root names and whether .c or .l files define the
# filter.  Except for vile-crypt and vile-manfilt (which do not correspond to
# majormodes), the filter names are constructed as vile-{root}-filt.

EOF

$FGREP ' c ' $TEMP | \
while true
do
	read filter suffix filename
	test -z "$filter" && break
	test -z "$filename" && continue

	TEXT="$filter	$filename"
	CASE=`echo $filename | sed -e 's/././g'`
	case $CASE in #(vi
	........) #(vi
		;;
	*)
		TEXT="$TEXT	"
		;;
	esac

	echo "$TEXT	$suffix" >>$DATA
done

echo >>$DATA

$FGREP ' l ' $TEMP | \
while true
do
	read filter suffix filename
	test -z "$filter" && break
	test -z "$filename" && continue

	TEXT="$filter	$filename"
	CASE=`echo $filename | sed -e 's/././g'`
	case $CASE in #(vi
	........) #(vi
		;;
	*)
		TEXT="$TEXT	"
		;;
	esac

	case $filename in #(vi
	lex-filt|htmlfilt)
		FLEX="	flex"
		;;
	*)
		FLEX=
		;;
	esac

	echo "$TEXT	$suffix$FLEX" >>$DATA
done

if test -f $TARGET ; then
	sed -e 's/: .*\$/$/' $TARGET >$TEMP
fi

umask $oldmask

if test -f $TARGET ; then
	diff $TEMP $DATA >$DIFF
	if test -s $DIFF ; then
		cat $DIFF
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

rm -f $TEMP $SORT $DIFF $DATA
