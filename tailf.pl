use FileHandle;

sub tailf {
    my ($fname) = @_;
    my $vbuf;
    my $fh = new FileHandle;

    open $fh, "tail -f $fname |"	or die "Can't open $fname";

    $vbuf = Vile::Buffer->new();
    Vile->current_buffer($vbuf);
    $vbuf->buffername($fname);

    Vile::watchfd(
	fileno($fh),
	'read',
	sub { 
	    my $buf = ' ' x 4096;
	    my @olddot = $vbuf->dot;
	    my $lastlnum;

	    # Fetch data from input stream
	    sysread $fh, $buf, 4096;

	    # Set Position to end of buffer and retrieve this line number
	    $lastlnum = $vbuf->dot('$$');

	    # Write data to the editor's buffer
	    print $vbuf $buf;

	    # Reset old position of dot if not near end of buffer
	    if ($olddot[0] < $lastlnum - 1) {
		$vbuf->dot(@olddot);
	    }

	    # Update the screen
	    Vile::update();
	}
    );
}

1;
