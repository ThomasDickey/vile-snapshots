# $Id: plugins.pl,v 1.8 2014/01/28 21:37:40 tom Exp $
# (see dir.doc)
package plugins;

use strict;

our $RPM_Provides = 'plugins.pl perl(plugins.pl)';

sub gzip {
    my ($file) = @_;
    my ( $cb, $line );

    open( GZIP, "gunzip -c $file |" ) || do { print "$!\n"; return 0; };

    foreach $cb ( Vile::buffers() ) {
        if ( $cb->buffername eq "<gzip-viewer>" ) {
            Vile->current_buffer($cb);
            $cb->setregion( 1, '$$' )->attribute("normal")->delete;
            last;
        }
    }
    $cb = $Vile::current_buffer;
    if ( $cb->buffername ne "<gzip-viewer>" ) {
        $cb = new Vile::Buffer;
        $cb->buffername("<gzip-viewer>");
        Vile->current_buffer($cb);
        $cb->set( "view",     1 );
        $cb->set( "readonly", 1 );
        $cb->set( "cfilname", $file );
        $cb->unmark->dot('$$');
    }

    while ( $line = <GZIP> ) { print $cb $line; }

    close(GZIP);

    $cb->unmark()->dot( 1, 0 );
    Vile::update();
    return 1;
}

1;

__DATA__

=head1 NAME

plugins - additional perl module for vile

=head1 SYNOPSIS

    use plugins;
    plugins::gzip("filename");

=head1 DESCRIPTION

The B<plugins> module is used by the B<mime> module to display a file according
to this MIME-type in vile:

    x-vile-flags=plugin

=head1 AUTHOR

Kuntal Daftary

=cut
