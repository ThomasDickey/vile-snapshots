#!/bin/sh
# $Header: /users/source/archives/vile.vcs/RCS/xshell.sh,v 1.1 1997/10/08 10:32:38 tom Exp $
#
# This script is designed to be used from xvile to support the $xshell variable.
# If you wish to have shell commands of the form
#	:!command
# run in an xterm or equivalent, then set the $xshell variable like this:
#	:setv $xshell="xterm -e xshell.sh"
#
# Xvile uses the -e option to specify to the xterm which shell command to run.
#
if test $# != 0 ; then
	case "$1" in #(vi
	-e)	shift
		;;
	esac
fi
eval $@
echo Press return to exit
read
