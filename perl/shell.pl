use IO::Pty;
require POSIX;

my %shells = ();

sub terminal_emulation ($$);

#
# Not meant to be complete.  Just enough so that I could use
# bash
#

sub terminal_emulation ($$) {
    my ($vbp, $buf) = @_;

    my (@dot,@endot,@donedot);

    # Figure out what to do about DOT.  If it's near the end, then
    # leave it alone.  Otherwise, move it to the end.

    @donedot = @dot = $vbp->dot;
    @enddot = $vbp->dot('$','$$');

    if ($dot[0] >= $endot[0]-1) {
	$vbp->dot(@dot);		# restore dot
	@donedot = ();			# no finish dot
    }
    else {
	@dot = @enddot;
    }

    while (length($buf) > 0) {
	if ($buf =~ /^([\x09\x20-\x377]+)(.*)$/s) {
	    # Handle sequence of printable characters
	    $vbp->setregion(@dot, $dot[0], $dot[1]+length($1))
	        ->delete;
	    print $vbp $1;
	    $buf = $2;
	    @dot = $vbp->dot;
	}
	elsif ($buf =~ /^\r(.*)$/s) {
	    # ^M				-- beginning of line
	    $dot[1] = 0;
	    $vbp->dot(@dot);
	    $buf = $1;
	}
	elsif ($buf =~ /^\n(.*)$/s) {
	    # ^J				-- down a line
	    #	 (Normally, this should just move down one line,
	    #	  but we've combined \n's with \r's before getting
	    #	  here...)
	    $dot[0]++;
	    @enddot = $vbp->dot('$','$$');
	    if ($dot[0] > $enddot[0]) {
		print $vbp "\n";
		#if ($dot[1] > 0) {
		#    print $vbp ' ' x $dot[1];
		#}
		@dot = $vbp->dot;
	    }
	    else {
		$vbp->dot(@dot);
	    }
	    $buf = $1;
	}
	elsif ($buf =~ /^(\x08+)(.*)$/s) {
	    # Handle ^H
	    $dot[1] -= length($1);
	    if ($dot[1] < 0) {
		$dot[1] = 0;
	    }
	    $buf = $2;
	    $vbp->dot(@dot);
	    
	    if ($vbp->setregion(@dot, $dot[0], '$')->fetch =~ /^\s+$/) {
		# Delete 'til end of line if all spaces
		$vbp->delete;
	    }
	    $vbp->dot(@dot);	# fetch moved dot on us
	}
	elsif ($buf =~ /^(\x07+)(.*)$/) {
	    Vile::beep();
	    $buf = $2;
	}
	elsif ($buf =~ /^\e\[(\d*)P(.*)$/s) {
	    # ESC, [, optional-digits, P	-- delete characters forward
	    my $digs;
	    $digs = ($1 eq "") ? 1 : $1;
	    $vbp->setregion(@dot, $dot[0], $dot[1]+$digs)->delete;
	    $buf = $2;
	}
	elsif ($buf =~ /^\e\[(\d*)@(.*)$/s) {
	    # ESC, [, optional-digits, @	-- insert N characters
	    my $digs;
	    $digs = ($1 eq "") ? 1 : $1;
	    $buf = $2;
	    # To do this right, we probably have an "insert" mode..
	    $buf =~ /^(.{,$digs})(.*)$/s;
	    print $vbp $1;
	    $buf = $2;
	}
	elsif ($buf =~ /^\e\[K(.*)$/s) {
	    # ESC, [, K				-- erase 'til eol
	    $vbp->setregion(@dot, $dot[0], '$$')->delete;
	    $buf = $1;
	}
	else {
	    # Unhandled control character(s)
	    # just print them out...
	    # (And when they annoy you enough, add a case to handle
	    # them above.)

	    $buf =~ /^(.)(.*)$/s;

	    $vbp->setregion(@dot, $dot[0], $dot[1]+length($1))
	        ->delete;
	    print $vbp $1;
	    $buf = $2;
	    @dot = $vbp->dot;
	}
    }

    if (@donedot) {
	$vbp->dot(@donedot);
    }
}

sub start_shell {
    my $shell = $ENV{SHELL};
    $shell = '/bin/sh' unless defined($shell);

    my $pty     = new IO::Pty		or die "Can't open new pty: $!";

    my $pid = fork;
    die "Can't fork"			if $pid < 0;

    if ($pid == 0) {
	POSIX::setsid();
	my $tty	= $pty->slave;
	open(STDIN, "<&".fileno($tty))	or die "Can't reopen STDIN";
	open(STDOUT, ">&STDIN")		or die "Can't reopen STDOUT";
	open(STDERR, ">&STDIN")		or die "Can't reopen STDERR";
	system("stty sane");
	close $pty;
	close $tty;
	exec $shell;
    }
    return ($pty, $pid);
}



sub shell {
    my $oldworking = Vile::working();		# fetch old value of working
    Vile::working(0);				# disable working... messages

    $| = 1;

    my ($pty, $pid ) = start_shell();
    die "start_shell failed" unless $pty;

    my $vbp = new Vile::Buffer;
    $vbp->buffername("shell-$pid");
    Vile->current_buffer($vbp);
    print $vbp " \n";
    $vbp->unmark()->dot('$$');
    Vile::update();


    $shells{$pid}{PTY_HANDLE} = $pty;
    $shells{$pid}{BUF_HANDLE} = $vbp;

    my $sanity_initialized = 0;

    Vile::watchfd(
	fileno($pty),
	'read',
	sub {
	    my $buf = ' ' x 4096;
	    my @olddot = $vbp->dot;
	    my $lastlnum;

	    # Fetch data from input stream
	    if (!sysread $pty, $buf, 4096) {
		Vile::unwatchfd(fileno($pty));
		close $pty;
		delete $shells{$pid};
		waitpid $pid, 0;
		print STDOUT "End of file from shell.  Use ^G to escape";
		Vile::update();
		return;
	    }

	    $buf =~ s/\r\n/\n/gs;		# nuke ^M characters

	    @dot = $vbp->dot('$','$$');


	    if (length($buf) < 256
	        && ($buf =~ /[\x01-\x08\x11-\x1f]/
	            || $olddot[0] != $dot[0] 
		    || $olddot[1] != $dot[1]))
	    {
		# Yuck, it contain's control characters, or it's
		# not at the end of the buffer
		# ...we have some work to do
		$vbp->dot(@olddot);
		terminal_emulation($vbp, $buf)
	    }
	    else {
		# Blast it out...
		#print "Blast $blast"; $blast++;

		# Set Position to end of buffer and retrieve this line number

		# Write data to the editor's buffer
		print $vbp $buf;

		# Reset old position of dot if not near end of buffer
		if ($olddot[0] < $dot[0] - 1) {
		    $vbp->dot(@olddot);
		}
	    }
	    # Nuke the [modified] message
	    $vbp->unmark();

	    # Update the screen
	    Vile::update();
	    # Something to look into... uncomment the following line: segfault
	    #print "After update: ", join(',',$vbp->dot());
	}
    );

    print STDOUT "Shell started.  Use ^G to escape";
    Vile::update();

    my $c;
    while (($c = Vile::keystroke()) != 7) {		# ^G escapes
	print $pty chr($c);
    }

    print STDOUT 'Editor resumed.  Use ":perl resume_shell" to return.';
    Vile::update();

    Vile::working($oldworking);			# restore "working..." message
    						# to previous state
}

sub resume_shell {
    my $pid;
    ($pid) = ($Vile::current_buffer->buffername() =~ /^shell-(\d+)$/);

    if (!defined($pid) or !defined($shells{$pid}{PTY_HANDLE})) {
	print "Not in a shell window!";
	return;
    }

    my $pty = $shells{$pid}{PTY_HANDLE};
    my $vbp = $shells{$pid}{BUF_HANDLE};

    my $oldworking = Vile::working();		# fetch old value of working
    Vile::working(0);				# disable working... messages

    $vbp->dot('$','$$');
    Vile::update();

    print STDOUT "Shell resumed.  Use ^G to escape";
    Vile::update();

    my $c;
    while (($c = Vile::keystroke()) != 7) {		# ^G escapes
	print $pty chr($c);
    }

    Vile::working($oldworking);			# restore "working..." message
}

1;
