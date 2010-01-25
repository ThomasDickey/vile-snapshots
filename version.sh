#!/bin/sh
# $Header: /users/source/archives/vile.vcs/RCS/version.sh,v 1.1 2010/01/25 10:39:29 tom Exp $
# Extract vile's version from source-files to make it simple to generate
# versioned binaries.
if test $# != 0 ; then
	SOURCE="$*"
else
	SOURCE=patchlev.h
fi

RELEASE=`fgrep VILE_RELEASE "$SOURCE" |sed -e 's/^[^"]*"//' -e 's/".*//'`
VERSION=`fgrep VILE_VERSION "$SOURCE" |sed -e 's/^[^"]*"//' -e 's/".*//'`
PATCHED=`fgrep VILE_PATCHLE "$SOURCE" |sed -e 's/^[^"]*"//' -e 's/".*//'`

echo "${RELEASE}.${VERSION}${PATCHED}"
