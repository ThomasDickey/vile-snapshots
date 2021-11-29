#!/bin/sh
# $Id: makelist.sh,v 1.6 2004/04/11 23:27:58 tom Exp $
#
# Make a list of the names for the specified syntax filters, for the configure
# script, etc.
#
# $1 is the path of genmake.mak
#
# $2 is the language key, i.e., "c", "l", or "lc"
#
# $3 is the nominal list, which may be "all", "none" or an explicit
# comma-separated list of the names from the first column of genmak.mak

all=`grep "^[^	#][^	]*	[	]*[^	]*	[	]*[$2]$" $1 2>/dev/null | sed -e 's/	.*//' |sort -u`
list="$3"

save_IFS="$IFS"
IFS=",$IFS"

key=none
for j in $list
do
	case $j in
	all)	
		key=$j
		;;
	none)	
		key=
		;;
	*)	
		if test "$key" = none ; then
			key=$j
		elif test "$key" != all ; then
			key=some
		fi
		;;
	esac
done

if test "$key" = all ; then
	key="$all"
elif test "$key" = none ; then
	key=
else
	key=
	for j in $list
	do
		if test "$j" != all ; then
			if test "$j" = none ; then
				key=
			else
				found=no
				for k in $all
				do
					if test "$k" = "$j" ; then
						found=yes
						break
					fi
				done
				if test "$found" = no ; then
					continue
				elif test -n "$key" ; then
					key="$key
$j"
				else
					key="$j"
				fi
			fi
		fi
	done
fi

IFS="$save_IFS"

# The result has no repeats, does not use the keywords "all" or "none", making
# it possible to compare two lists.
echo "$key" |sort -u | sed -e '/^$/d'
