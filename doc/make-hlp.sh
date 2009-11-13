#!/bin/sh
# $Id: make-hlp.sh,v 1.1 2009/11/13 00:12:54 tom Exp $
rm -f vile.hlp
perl make-hlp.pl vile-hlp.html >vile.hlp
