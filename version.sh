#!/bin/sh
# $Id: version.sh,v 1.3 2023/01/01 17:32:14 tom Exp $
# Extract vile's version from source-files to make it simple to generate
# versioned binaries.
if test $# != 0 ; then
	SOURCE="$*"
else
	SOURCE=patchlev.h
fi

: "${FGREP:=grep -F}"

RELEASE=`$FGREP VILE_RELEASE "$SOURCE" |sed -e 's/^[^"]*"//' -e 's/".*//'`
VERSION=`$FGREP VILE_VERSION "$SOURCE" |sed -e 's/^[^"]*"//' -e 's/".*//'`
PATCHED=`$FGREP VILE_PATCHLE "$SOURCE" |sed -e 's/^[^"]*"//' -e 's/".*//'`

echo "${RELEASE}.${VERSION}${PATCHED}"
