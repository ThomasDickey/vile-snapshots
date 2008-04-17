package Visit;
require 5.000;
require Exporter;
use Carp;

@ISA = qw(Exporter);
@EXPORT = qw(visit);

sub visit {
    my ($fname, @dot) = @_;

    croak "visit: No filename!"		if !defined($fname);

    my $visbuf = Vile::Buffer->edit($fname);

    $visbuf->dot(@dot)			if (@dot == 1) || (@dot == 2);

    Vile->current_buffer($visbuf);

    1;
}

1;

