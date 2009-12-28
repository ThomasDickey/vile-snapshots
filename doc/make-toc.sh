#!/bin/sh
# $Id: make-toc.sh,v 1.2 2009/12/28 14:40:25 tom Exp $
rm -f vile-toc.html
perl make-toc.pl "$@" >vile-toc.html
