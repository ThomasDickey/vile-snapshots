require "Comm.pl";

Comm::init();

sub shell {
    my $c;
    my $oldworking = Vile::working();		# fetch old value of working
    Vile::working(0);				# disable working... messages

    $| = 1;

    ( $Proc_pty_handle, $Proc_tty_handle, $pid ) = &open_proc( "/bin/sh" );
    die "open_proc failed" unless $Proc_pty_handle;
    &stty_sane($Proc_tty_handle);	# use $Proc_pty_handle for HP

    my $vbp = new Vile::Buffer;
    $vbp->buffername("shell-$pid");
    Vile->current_buffer($vbp);
    Vile::update();

    sleep(1);

    Vile::watchfd(
	fileno($Proc_pty_handle),
	'read',
	sub {
	    my $buf = ' ' x 4096;
	    my @olddot = $vbp->dot;
	    my $lastlnum;

	    # Fetch data from input stream
	    sysread $Proc_pty_handle, $buf, 4096;

	    # Set Position to end of buffer and retrieve this line number
	    $lastlnum = $vbp->dot('$','$$');

	    # Do some naive handling of control characters
	    $buf =~ s/\r//gs;			# nuke ^M's
	    $buf =~ s/[\x20-\x7f]\x08//gs;	# nuke embedded ^H's where
	    					# reasonable
	    my ($ctrl_hs,$rest) = ($buf =~ /^(\x08*)(.*)$/);
	    if ($ctrl_hs) {
		my $ctrl_hs_len = length($ctrl_hs);
		$vbp->setregion("${ctrl_hs_len}h")->delete;
		$buf = $rest;
	    }

	    # Write data to the editor's buffer
	    print $vbp $buf;

	    # Reset old position of dot if not near end of buffer
	    if ($olddot[0] < $lastlnum - 1) {
		$vbp->dot(@olddot);
	    }

	    # Update the screen
	    Vile::update();
	}
    );

    while (($c = Vile::keystroke()) != 7) {		# ^G escapes
	print $Proc_pty_handle chr($c);
    }

    Vile::working($oldworking);			# restore "working..." message
    						# to previous state
}

1;
