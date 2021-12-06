#!/usr/bin/env perl
# $Id: make-toc.pl,v 1.9 2021/12/02 00:27:24 tom Exp $

use strict;
use warnings;

use HTML::Toc;
use HTML::TocGenerator;

use POSIX qw(strftime);
our $now_string = strftime "%a %b %e %H:%M:%S %Y %Z", localtime;

print <<EOF
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<!--
  \$Id\$

  inputs: @ARGV
  update: $now_string
-->
EOF
;
print <<EOF
<HTML>
<HEAD>
<TITLE>
VI Like Emacs &mdash; Table of Contents
</TITLE>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
<link rel="author" href="mailto:dickey\@invisible-island.net">
<link rel="SHORTCUT ICON" href="/img/icons/vile.ico" type="image/x-icon">
<link rel="stylesheet" href="/css/simplestyle.css" type="text/css">
</HEAD>
<BODY>
<hr>
<a href=
"/">http://invisible-island.net/</a><a href="/vile/">vile/</a><br>
<hr>
EOF
;

my $toc          = HTML::Toc->new();
my $tocGenerator = HTML::TocGenerator->new();

$toc->setOptions({'doLinkToFile' => 1});
$toc->setOptions({'doLinkToId' => 1});
$tocGenerator->generateFromFile($toc, \@ARGV);
print $toc->format();

print <<EOF

</body>
EOF
;
