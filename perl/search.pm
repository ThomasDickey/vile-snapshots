#
# This package contains perl subroutines which are intended as a
# drop in replacement for vile's search facilities.  Not all features
# (such as visual matches) are implemented yet.
#
# These bindings are proper motions, however, so things like 'dn'
# or 'd/foo' will work as expected.
#
# Usage
# -----
# :perl use search;

package search;

require Vile::Exporter;
@ISA = 'Vile::Exporter';
%REGISTRY = (
  'perl-fsearch'     => [ motion => \&fsearch, "perl forward search", 'M-/' ],
  'perl-rsearch'     => [ motion => \&rsearch, "perl reverse search", 'M-?' ],
  'perl-search-next' => [ motion => \&searchnext, "perl search next", 'M-n' ],
  'perl-search-prev' => [ motion => \&searchprev, "perl search prev", 'M-N' ],
);

*CHUNKSIZE = \100;
my $direction = '';

#open TTY, ">/dev/tty";			# for debugging

#
# Get the pattern to search for, either from the user or from the
# previously stashed value in vile's search variable.
#

sub getpat {
    my ($how, $ldirection) = @_;

    if (defined($how) and $how eq 'noprompt') {
	$pat = Vile::get('search')
    }
    else {
	$direction = $ldirection;
	$pat = Vile::mlreply_no_opts($direction eq 'forward'
	                               ? 'Perl search: '
				       : 'Reverse perl search: ',
                                     scalar Vile::get('search'));
    }
    Vile::set(search => $pat) if defined($pat);
    return $pat;
}

#
# Back references (e.g, \1, \2, etc.) need to be adjusted to work
# properly since our search expression contains parenthesized
# expressions too.
#

sub fixbackreferences {
    my $pat = shift;
    my $adj = shift;
    my $lpcount = 0;		# number of unescaped left parens

    $lpcount++ while $pat =~ /(^|[^\\])\(/g;

    if ($lpcount > 0) {
	$pat =~ s/\\(\[1-9][0-9]*|.)/
	         "\\" . (($1 + 0 && $1 <= $lpcount) ? $1+$adj : $1)/gex;
    }

    return $pat;
}

#
# Search forward.  This is not as straightforward as it could be since
# we attempt to fetch the lines in chunks for efficient searching.
#

sub fsearch {
    my $pat = getpat(shift, 'forward');

    return 0 unless defined($pat);

    my $wrap       = 0;
    my $cb         = $Vile::current_buffer;
    my @start_dot  = $cb->current_position;
    my $lastline   = ($cb->setregion(1,'$'))[2];
    my $chunkstart = $start_dot[0];
    my $pos        = $start_dot[1]+1;

    if ($pos >= ($cb->setregion($chunkstart,0,$chunkstart,'$$'))[3]) {
	$pos = 0;
	$chunkstart++;
	$chunkstart = 1 if ($chunkstart > $lastline);
    }

    my $chunkend   = $chunkstart + $CHUNKSIZE;

    $pat = fixbackreferences($pat, 1);

    while (1) {
	$cb->set_region($chunkstart, $chunkend-1);
	$chunk = $cb->fetch;
	pos($chunk) = $pos;
	$pos = 0;
	if ($chunk =~ /($pat)/mg) {
	    my $lc = 0;
	    my $matchlen = length($1);
	    $chunk = substr($chunk, 0, pos($chunk));
	    $lc++ while $chunk =~ /\n/g;
	    $chunk =~ s/.*\n//g;
	    $cb->current_position($chunkstart + $lc, length($chunk) - $matchlen);
	    if ($wrap) {
		@dot = $cb->current_position;
		if ($start_dot[0] == $dot[0] and $start_dot[1] == $dot[1]) {
		    print "Only one occurrence of pattern";
		}
		else {
		    print "[Search wrapped past end of buffer]";
		}
	    }
	    return 1;
	}
    }
    continue {
	$chunkstart = $chunkend;
	if ($wrap) {
	    last if $chunkstart > $start_dot[0];
	}
	elsif ($chunkstart > $lastline) {
	    $wrap = 1;
	    $chunkstart = 1;
	}
	$chunkend = $chunkstart + $CHUNKSIZE;
    }

    print "Not found";
    return 0;
}

#
# Search backward
#

sub rsearch {
    my $pat = getpat(shift, 'backward');
    return 0 unless defined($pat);

    my $wrap       = 0;
    my $cb         = $Vile::current_buffer;
    my @start_dot  = $cb->current_position;
    my $lastline   = ($cb->setregion(1,'$'))[2];
    my $chunkend   = $start_dot[0]+1;
    my $pmpat;


    if ($start_dot[1] == 0) {
	if ($chunkend <= 2) {
	    $chunkend = $lastline+1;
	}
	else {
	    $chunkend--;
	}
	$pmpat = '.*';
    }
    else {
	$pmpat = ".{0,@{[$start_dot[1]-1]}}";
    }

    my $chunkstart = $chunkend - 1;

    $chunkstart = 1 unless $chunkstart > 0;
    $cb->set_region($chunkstart, 0, $chunkend-1, '$');

    $pat = fixbackreferences($pat, 2);

    # $ matches at both the newline and the position after the newline.
    # Eliminate one of these cases.
    $pat =~ s/(^|[^\\])\$$/$1(?=\n\$)/;

    while (1) {
	$chunk = $cb->fetch;
	pos($chunk) = $pos;
	$pos = 0;
	if (my ($prematch, $match) = $chunk =~ /\A($pmpat)($pat)/mg) {
	    my $lc = 0;
	    $lc++ while ($prematch =~ /\n/g);
	    $prematch =~ s/.*\n//g;
	    $cb->current_position($chunkstart + $lc, length($prematch) );
	    if ($wrap) {
		@dot = $cb->current_position;
		if ($start_dot[0] == $dot[0] and $start_dot[1] == $dot[1]) {
		    print "Only one occurrence of pattern";
		}
		else {
		    print "[Search wrapped past end of buffer]";
		}
	    }
	    return 1;
	}
    }
    continue {
	$chunkend = $chunkstart;
	if ($wrap) {
	    last if $chunkend <= $start_dot[0];
	}
	elsif ($chunkend <= 1) {
	    $wrap = 1;
	    $chunkend = $lastline + 1;
	}
	$chunkstart = $chunkend - $CHUNKSIZE;
	$chunkstart = 1 unless $chunkstart > 0;
	$cb->set_region($chunkstart, $chunkend-1);
	$pmpat = "[\000-\377]*";
    }

    print "Not found";
    return 0;
}

#
# Find next occurrence of pattern in the current direction
#

sub searchnext {
    $direction eq 'forward' ? fsearch('noprompt') : rsearch('noprompt');
}

#
# Find previous occurrence of pattern in current direction
#

sub searchprev {
    $direction eq 'forward' ? rsearch('noprompt') : fsearch('noprompt');
}

1;
