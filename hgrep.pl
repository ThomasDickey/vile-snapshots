#!/usr/bin/perl -w
#
# hgrep.pl
#
#   This script is meant to be used with [x]vile's perl interface to
#   provide a nifty recursive grep facility complete with hypertext
#   links.
#
#   One of the things which makes it so nifty is that it doesn't
#   search binary files.  (If you want it to, just search for and
#   remove the -T in the code below.)  So it's perfectly safe to
#   just search * in most cases rather than using a restrictive
#   filter like *.[hc]
#
# Installation
# ------------
#   
#   Place hgrep.pl, glob2re.pl, and visit.pl in either ~/.vile/perl
#   or in /usr/local/share/vile/perl.  (The exact location of the
#   latter may vary depending on how you configured [x]vile.
#
# Usage
# -----
#
#   hgrep will be easier to use if the following procedure is defined
#   in your .vilerc file:
#
#   store-procedure hgrep
#       perl "require 'hgrep.pl'"
#       perl hgrep
#	; uncomment next line to use results with error-finder.
#	; error-buffer $cbufname
#   ~endm
#
#   Once this procedure, is defined, just type
#
#	:hgrep
#
#   and answer the ensuing questions.
#
#   A new buffer will be created with embedded hypertext commands to
#   vist the places in the files where matched text is found.  These
#   hypertext commands may be activated by double clicking in xvile
#   or using the "execute-hypertext-command" command from vile.  (See
#   the Hypertext section of vile.hlp for some convenient key bindings.)
#
# Additional Notes
# ----------------
#   
#   As not much has been written about it yet, this module is an
#   example of how to use the perl interface.
#
#				- kev (4/3/1998)
#


use File::Find;
use FileHandle;
use English;
require 'glob2re.pl';
require 'visit.pl';

my $rgrep_oldspat = '';
my $rgrep_oldroot = '.';

sub hgrep {

    my ($spat, $root, $fpat) = @_;

    if (!defined($spat)) {
	$spat = Vile::mlreply_no_opts("Pattern to search for? ", $rgrep_oldspat);
	return if !defined($spat);
    }
    $rgrep_oldspat = $spat;

    while (!defined($root)) {
	$root = Vile::mlreply_dir("Directory to search in? ", $rgrep_oldroot);
	return if !defined($root);
    }
    $rgrep_oldroot = $root;

    while (!defined($fpat)) {
	$fpat = Vile::mlreply_no_opts("File name pattern? ", "*");
	return if !defined($fpat);
    }

    my $resbuf = new Vile::Buffer "rgrep $spat $root $fpat";

    print $resbuf "Results of searching for /$spat/ in $root with filter $fpat...\n---------------\n";

    $fpat = glob2re($fpat);

    my $code = '
    find(
	sub { 
	    if (-f && -T && $_ ne "tags" && /' 
    .                  $fpat 
    .                      '/) {
		my $fname = $File::Find::name;
		if (open SFILE, "<$_") {
		    local($_);
		    while (<SFILE>) {
			if (/' 
    .                        $spat
    .                            '/) {
			    chomp;
			    s/^(.*?)('
    .			        $spat
    .                                ')/$1 . "\x01" 
                                           . length($2) 
                                           . q#BHperl "visit(\'#
					   . $fname
					   . qq(\',) 
					   . $INPUT_LINE_NUMBER
					   . q(,)
					   . length($1)
					   . qq#)"\0:#
					   . $2/e;
			    print $resbuf "$fname\[$INPUT_LINE_NUMBER]: $_\n";
			}
		    }
		    close SFILE;
		}
		else {
		    print $resbuf "Warning: Can\'t open $fname\n";
		    #print "Warning: Can\'t open $fname\n");
		}
	    }
	},
	$root);
    ';

    eval $code;
    if (defined($@) && $@) {
	print "$@";
    }
    else {
	print $resbuf "\n\n";
	$Vile::current_buffer = $resbuf;
	$resbuf->setregion(1,'$')
	       ->attribute_cntl_a_sequences
	       ->unmark
	       ->dot(3);
    }
}

1;
