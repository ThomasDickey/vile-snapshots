package Glob2re;
require 5.000;
require Exporter;

use strict;

use vars qw(@ISA @EXPORT);

@ISA    = qw(Exporter);
@EXPORT = qw(glob2re);

sub glob2re {
    my ($glob) = @_;
    my $re = '(^|\/)';

    while ( $glob ne '' ) {
        if ( $glob =~ /^\*(.*)$/ ) {
            $re .= '.*';
            $glob = $1;
        }
        elsif ( $glob =~ /^\?(.*)$/ ) {
            $re .= '.';
            $glob = $1;
        }
        elsif ( $glob =~ /^\[(.+?)\](.*)$/ ) {
            $re .= "[$1]";
            $glob = $2;
        }
        elsif ( $glob =~ /^\{(.*?,.*?)\}(.*)$/ ) {
            my ($alts) = $1;
            $glob = $2;
            $re .=
              '(' . join( '|', map( quotemeta, split( /,/, $alts ) ) ) . ')';
        }
        elsif ( $glob =~ /^(.[^[{*?]*)(.*)$/ ) {
            $re .= quotemeta($1);
            $glob = $2;
        }
        else {

            # shouldn't get here.  If we do, give up
            $glob = '';
        }
    }
    $re .= '$';
    return $re;
}

1;


__DATA__

=head1 NAME

Glob2re - convert glob/wildcard to regular expression

=head1 SYNOPSIS

The module exports one function, B<glob2re>:

    use Glob2re;
    my $re = glob2re($glob);

=head1 DESCRIPTION

The B<glob2re> function converts a Unix-style file-globbing string to
a regular expression.  It is used by the B<dirlist> and B<hgrep> commands.

Unix-style file-globbing uses "*" to match zero or more characters of
any type, and "?" to match any single character.  Prefix "*" or "?" with
"\" if you want to use those literally, rather than as a metacharacter.

=head1 EXAMPLE

    my $glob = "*.*";
    my $re = glob2re($glob);

=head1 AUTHOR

Kevin Buettner

=cut
