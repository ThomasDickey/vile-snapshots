#!/usr/bin/perl -w
# $Id: make-toc.pl,v 1.2 2009/11/13 01:39:14 tom Exp $
use HTML::Toc;
use HTML::TocGenerator;

my $toc          = HTML::Toc->new();
my $tocGenerator = HTML::TocGenerator->new();

$toc->setOptions({'doLinkToFile' => 1});
$toc->setOptions({'doLinkToId' => 1});
$tocGenerator->generateFromFile($toc, ['vile-hlp.html']);
print $toc->format();
