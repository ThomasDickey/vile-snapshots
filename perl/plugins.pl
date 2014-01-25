# $Header: /users/source/archives/vile.vcs/perl/RCS/plugins.pl,v 1.6 2014/01/23 20:21:45 tom Exp $
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
