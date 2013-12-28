#!/bin/sh
# $Id: make-toc.sh,v 1.3 2013/12/27 20:43:40 tom Exp $
rm -f vile-toc.html
perl make-toc.pl "$@" | sed -e 's,-toc",",g' >vile-toc.html
