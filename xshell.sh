#!/bin/sh
# $Id: xshell.sh,v 1.6 2023/01/01 17:35:30 tom Exp $
#
# This script is designed to be used from xvile to support the $xshell variable.
# If you wish to have shell commands of the form
#	:!command
# run in an xterm or equivalent, then set the $xshell variable like this:
#	:setv $xshell="xterm -e xshell.sh"
#
# Xvile uses the -e option to specify to the xterm which shell command to run.
#
if [ $# != 0 ]; then
	case "$1" in #(vi
	-e)	shift
		;;
	esac
fi
if [ $# != 0 ]; then
	eval "$@"
	echo Press return to exit
	read reply
else
	eval "${SHELL-/bin/sh}"
fi
