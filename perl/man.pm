# $Id: man.pm,v 1.6 2014/01/28 23:35:26 tom Exp $
package man;

use strict;

use Vile;
use Vile::Manual;
require Vile::Exporter;

use vars qw(@ISA %REGISTRY);

@ISA      = 'Vile::Exporter';
%REGISTRY = (
    'man' => [ \&man, '' ],
    'man-help' => [ sub { &manual }, 'manual page' ]
);

sub man {
    my $work = Vile::working(0);

    my $word = scalar( Vile->get("word") );
    my $qid  = scalar( Vile->get("qidentifier") );
    my $man  = Vile::mlreply_opts( "Man? ", ( $word, $qid ) );

    my $s;

    if ( !defined $man || !length($man) ) {
        Vile::working($work);
        print "";
        return;
    }

    my $cb;
    foreach $cb ( Vile::buffers() ) {
        if ( $cb->buffername eq "<man:$man>" ) {
            Vile::working($work);
            Vile->current_buffer($cb);
            return;
        }
    }

    print "";
    open( MAN, "man $man |" )
      || do { print "$!\n"; Vile::working($work); return; };

    $cb = new Vile::Buffer;
    $cb->buffername("<man:$man>");
    Vile->current_buffer($cb);

    while (<MAN>) {
        next unless /\010/o;
        s/((.)\010)+\2/\002\2/g;
        s/(_\010)+/\003/go;
        s|((\002.)+)|sprintf("\001%dB:$1",(length($1)/2))|ge;
        s|((\003.)+)|sprintf("\001%dU:$1",(length($1)/2))|ge;
        s/(\002|\003)//go;
        s/\010.//go;
    }
    continue {
        print $cb $_ if /\S/ || !$s;
        $s = /^\s*$/;
    }
    close(MAN);
    print "";

    $cb->setregion( 1, '$$' )->attribute_cntl_a_sequences;
    $cb->set( "view",     1 );
    $cb->set( "readonly", 1 );
    $cb->unmark()->dot( 1, 0 );
    $cb->command("clear-and-redraw");
    Vile::update();
    Vile::working($work);
    return;
}

1;

__END__

=head1 NAME

man - Unix manual pages interface in vile

=head1 SYNOPSIS

In .vilerc

    perl use man;

In [x]vile

    :man

=head1 DESCRIPTION

This is a Unix manual pages viewer  for [x]vile written using the
vile-perl-api. On invocation, it prompts the user for the keyword
for which to show the manual page, and provides an initial choice
of the "punctuated-word"  and the  "whole-word" under the cursor.
The user can cycle through the default initial choices by hitting
the TAB key.

The manual page is presented  in a new buffer unless a buffer for
that manual page already exists. The  manual  page  is  formatted
with bold and underline attributes as appropriate.

=head1 CAVEATS

None.

=head1 CREDITS

Vile, Perl and Vile's Perl interface.

=head1 AUTHOR

Kuntal Daftary (daftary@cisco.com), 2001

=cut
