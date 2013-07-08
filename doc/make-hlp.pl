#!/usr/bin/perl -w
# $Id: make-hlp.pl,v 1.10 2013/07/07 13:53:13 tom Exp $
# Generate vile.hlp, using the dump feature of a text browser.
#
# Any of (e)links(2) or lynx would work.
#
# (e)links(2) and w3m provide similar boxed-tables, which (for the builtin
# functions) are more readable than the output from lynx.
#
# w3m is unsuitable because it does not provide a left margin.
#
# links is used because the differences between its dump and elinks are
# insignificant, and because (not being actively developed), fewer surprises a
# likely when regenerating vile.hlp

use strict;
use File::Temp qw/ tempfile /;

our $PROG = "links";

our $SKIP         = 999;
our $CAPS         = 0;
our $OOPS         = 0;
our $file_title   = "Getting along with vile";
our $file_version = "";

sub trim($) {
    my $text = $_[0];
    $text =~ s/\s+$//;

    return $text;
}

# This requires a running version of vile to get the version number.
sub header() {
    my $version = `vile -V`;

    chomp $version;
    $version =~ s/^vile version //;
    $version =~ s/[a-z ].*//;

    my $head_l = $file_title;
    my $head_r = "version $version";
    my $length = 78 - length($head_r);

    my $header = sprintf( "%-*.*s%s", $length, $length, $head_l, $head_r );

    return $header;
}

# A capital letter in the first column means a major heading.  Underline it.
# Adjust blank lines before/after a major header, to look more/less like the
# older hand-crafted version.
sub output($) {
    my $text = $_[0];
    my $caps = ( $text =~ /^[A-Z]/ );
    my $null = ( $text =~ /^$/ );
    my $oops = ( $text =~ /^\s+:/ );

    if ($caps) {
        while ( $SKIP < 2 ) {
            printf "\n";
            $SKIP += 1;
        }
    }
    elsif ( $CAPS and $null ) {
        $CAPS = 0;
        return;
    }

    printf "%s\n", $text;

    if ($caps) {
        $text =~ s/[^\s]/-/g;
        $text =~ s/- -/---/g;
        printf "%s\n", $text;
    }

    if ($null) {
        $SKIP += 1;
    }
    else {
        $SKIP = 0;
    }
    $CAPS = $caps;
    $OOPS = $oops;
}

sub doit() {
    our $name;
    my $ident_pattern = '\$' . 'Id:[^$]*' . '\$';
    my $first         = 1;

    foreach $name (@ARGV) {
        my $n;
        my $t;
        my (@input);
        my $text;

        my $temp_name;
        my $temp_fh;

        # use tidy to construct a file in consistent format, with the markdown
        # bold/italics pre-substituted.
        ( $temp_fh, $temp_name ) = tempfile();
        open( FP, "tidy -q -wrap 4096 $name|" ) || do {
            print STDERR "Can't read $name: $!\n";
            return;
        };
        @input = <FP>;
        close(FP);
        open( FP, ">$temp_name" ) || do {
            print STDERR "Can't write $temp_name: $!\n";
            return;
        };
        for $n ( 0 .. $#input ) {
            chomp $input[$n];
            $input[$n] =~ s,\<[/]?i\>,_,g;
            $input[$n] =~ s,\<[/]?b\>,*,g;
            printf FP "%s\n", $input[$n];
        }
        close(FP);

        # read the file directly to get the version-id and toplevel-title.
        open( FP, $temp_name ) || do {
            print STDERR "Can't read $temp_name: $!\n";
            return;
        };
        @input = <FP>;
        close(FP);

        for $n ( 0 .. $#input ) {
            chomp( $input[$n] );
        }

        for $n ( 0 .. $#input ) {
            if ( $input[$n] =~ /$ident_pattern/ ) {
                $text = $input[$n];
                $text =~ s/^.*\$Id:\s+//;
                $text =~ s/,v\s+/(/;
                $text =~ s/\s.*/)/;
                $file_version .= $text;
            }
            elsif ( $input[$n] =~ /<title>/i ) {
                $text = $input[$n];
                $text =~ s/^.*\<title\>//gi;
                $t = $n;
                while ( $text !~ /.*\<\/title\>/i and $t != $#input ) {
                    $text .= " " . $input[ ++$t ];
                }
                $text =~ s/\<\/title\>.*$//gi;
                $text =~ s/\(version.*//;
                $text       = trim($text);
                $file_title = $text;
                last;
            }
        }

        # read the formatted file
        open( FP, "$PROG -dump $temp_name|" ) || do {
            print STDERR "Can't dump $temp_name: $!\n";
            return;
        };
        @input = <FP>;
        close(FP);

        if ($first) {
            output( header() );
            printf "\n";
            $first = 0;
        }

        my $body = 0;
        for $n ( 0 .. $#input ) {
            chomp( $input[$n] );
            $input[$n] = trim( $input[$n] );

            # omit the website url on ".doc" files
            if (    $body == 0
                and $n > 0
                and $input[$n] =~ /[-]{20,}/
                and $input[$n] eq $input[0] )
            {
                $body = $n + 1;
            }
        }
        for $n ( $body .. $#input ) {
            if (
                    $OOPS
                and ( $n < $#input )
                and ( $input[$n] =~ /^$/
                    and ( $input[ $n + 1 ] =~ /^\s+:/ ) )
              )
            {
                next;
            }
            output( $input[$n] );
        }

        unlink $temp_name;
    }

    printf "\n";
    printf "-- (generated by make-hlp.pl from %s)\n", $file_version;
    printf "-- vile:txtmode fillcol=78\n";
    our $foot = "-- \@Header\@";
    $foot =~ s/@/\$/g;
    printf "%s\n", $foot;
}

&doit();
