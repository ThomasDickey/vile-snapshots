#!/usr/bin/env perl
# $Id: make-hlp.pl,v 1.18 2021/01/01 12:40:57 tom Exp $
# Generate vile.hlp, using the dump feature of a text browser.
#
# Any of (e)links(2) or lynx would work.
#
# (e)links(2) and w3m provide similar boxed-tables, which (for the builtin
# functions) are more readable than the output from lynx.
#
# w3m is unsuitable because it does not provide a left margin.
#
# links was used because the differences between its dump and elinks are
# insignificant, and because (not being actively developed), fewer surprises a
# likely when regenerating vile.hlp -- except that links/etc rendering of
# blockquote+pre is ... not good.  So we fix that by preprocessing.
#
# Later, Debian created a surprise by equating links and links2.

use strict;
use warnings;
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
        while ( $text =~ /-\s-/ ) {
            $text =~ s/-\s-/---/g;
        }
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
        open( FP, "tidy -q -wrap 4096 < $name|" ) || do {
            print STDERR "Can't read $name: $!\n";
            return;
        };
        @input = <FP>;
        close(FP);
        open( FP, ">$temp_name" ) || do {
            print STDERR "Can't write $temp_name: $!\n";
            return;
        };
        my $blockq = 0;
        my $prefrm = 0;
        for $n ( 0 .. $#input ) {
            chomp $input[$n];
            $blockq = 1 if ( $input[$n] eq "<blockquote>" );
            $blockq = 0 if ( $input[$n] =~ /\<\/blockquote\>/ );
            $prefrm = 1 if ( $input[$n] =~ /^<pre\b/ );
            $prefrm = 0 if ( $input[$n] =~ /\<\/pre\>/ );
            $input[$n] =~ s,\<[/]?i\>,_,g;
            $input[$n] =~ s,\<[/]?b\>,*,g;
            $input[$n] = "\t" . $input[$n] if ( $blockq and $prefrm );

            # printf STDERR "%d:%d.%d\t%s\n", $n, $blockq, $prefrm, $input[$n];
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
        open( FP,
                "COLUMNS=80 "
              . "LC_ALL=C "
              . "LC_CTYPE=C "
              . "LANG=C "
              . "$PROG -dump $temp_name|" )
          || do {
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
        my $base = -1;
        for $n ( 0 .. $#input ) {

            # printf STDERR "READ %d:%s", $n+1, $input[$n];
            chomp( $input[$n] );
            $input[$n] = trim( $input[$n] );

            $base = $n if ( $base < 0 and $input[$n] =~ /[-]{20,}/ );

            # omit the website url on ".doc" files
            if (    $body == 0
                and $n > $base
                and $input[$n] =~ /[-]{20,}/
                and $input[$n] eq $input[$base] )
            {
                $body = $n + 1;
            }
            $input[$n] =~ s/\xa9/(c)/g;
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
    our $foot = "-- \@Id\@";
    $foot =~ s/@/\$/g;
    printf "%s\n", $foot;
}

&doit();
