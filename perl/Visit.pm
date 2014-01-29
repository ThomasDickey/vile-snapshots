package Visit;
require 5.000;
require Exporter;

use strict;
use Carp;

use vars qw(@ISA @EXPORT);

@ISA    = qw(Exporter);
@EXPORT = qw(visit);

sub visit {
    my ( $fname, @dot ) = @_;

    croak "visit: No filename!" if !defined($fname);

    my $visbuf = Vile::Buffer->edit($fname);

    $visbuf->dot(@dot) if ( @dot == 1 ) || ( @dot == 2 );

    Vile->current_buffer($visbuf);

    1;
}

1;

__DATA__

=head1 NAME

Visit - visit a file in vile

=head1 SYNOPSIS

    use Visit;
    &visit("filename", 1);

=head1 DESCRIPTION

The B<Visit> module is used by C<hgrep> to visit a given line and column in
a file.

=head1 AUTHOR

Kevin Buettner

=cut
