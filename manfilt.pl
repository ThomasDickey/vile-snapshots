#!/usr/local/bin/perl
#
# This script replaces backspace sequences with Ctrl-A attribute sequences
# suitable for processing by vile.  It also attributes headings in bold.
#
# The C language version of this filter, also distributed with vile,
# is much faster.
#
# $Header: /users/source/archives/vile.vcs/RCS/manfilt.pl,v 1.1 1994/07/11 22:56:20 pgf Exp $
#
while (<>) {
    if (/^[A-Z][A-Z]/) {
	$_ = "\001" . length($_) . "B:" . $_;
	next;
    }
    next unless /[\b]/o;
    s/(.)([\b]\1)+/\002\1/go;
    for (;;) {
	last unless
	    $att =  /(_[\b]\002.)+/o && "UB:"
		 || /(_[\b].)+/o && "U:"
	         || /(\002.)+/o && "B:";
	$beg = $`; $cur = $&; $end = $';
	$cur =~ s/(_[\b])|\002//go;
	$_ = $beg . "\001" . length($cur) . $att . $cur . $end;
    }
}
continue {
    print;
}

