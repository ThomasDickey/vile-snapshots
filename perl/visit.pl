#!/bin/echo not_a_script:

sub visit {
    my ($fname, @dot) = @_;

    die "visit: No filename!"		if !defined($fname);

    my $visbuf = Vile::Buffer->edit($fname);

    $visbuf->dot(@dot)			if (@dot == 1) || (@dot == 2);

    Vile->current_buffer($visbuf);

    1;
}

1;
