package Vile::Capture;
use FileHandle;

#
# This package contains a drop-in replacement for vile's capture-command
# command.
#
# It provides some additional functionality, however.  (Otherwise there's
# not much point in doing it.)  Rather than wait for the command to finish,
# this version will update the [Output] buffer as the results of the
# command become available thus permitting the user to do other editing
# tasks while the command is running.
#

sub capture_command {

    my $command = Vile::mlreply_no_opts("perl-!");
    return if !defined($command);

    my $b = edit Vile::Buffer '[Output]';
    if (!$b) {
	$b = new Vile::Buffer;
	$b->buffername('[Output]');
    }
    $b->set(readonly => 0, view => 0);
    $b->dotq('$$');
    print $b "\n$command\n..............\n";
    $b->set(readonly => 1, view => 1);
    my $w = undef;
    foreach my $i (0..Vile::window_count()-1) {
	if ($b->buffername eq Vile::window_at($i)->buffer->buffername) {
	    $w = Vile::window_at($i);
	    last;
	}
    }
    $w = Vile::Window->new($b)		unless $w;
    $w->dot($b->dot())			if $w;
    $b->unmark;

    unless (open CAPTURE, "$command 2>&1 |") {
	print STDERR "Can't run command $command";
	return;
    }

    Vile::watchfd(
	fileno(CAPTURE),
	'read',
	sub {
	    my $buf = ' ' x 4096;

	    # Fetch data from input stream
	    if (!sysread CAPTURE, $buf, 4096) {
		Vile::unwatchfd(fileno(CAPTURE));
		close CAPTURE;
		return;
	    }

	    # Nuke ^M characters
	    $buf =~ s/\r\n/\n/gs;

	    # Set "dot" to end of the buffer.  Note that we use
	    # dotq which does it quietly.  I.e, the buffer position
	    # will not be propagated back to the editor upon return
	    $b->dotq('$$');

	    # Try to find a window associated with the buffer which
	    # also has DOT near the end.
	    my $w = undef;
	    foreach my $i (0..Vile::window_count()-1) {
		$w = Vile::window_at($i);
		if ($b->buffername eq $w->buffer->buffername
		and $w->dot() >= $b->dot()-1 ) {
		    last;
		}
		$w = undef;
	    }

	    # Blast it out...
	    #print "Blast $blast"; $blast++;

	    # Make it writable
	    $b->set(readonly => 0, view => 0);

	    # Write data to the editor's buffer
	    print $b $buf;

	    # Restore view-only and read-only attributes
	    $b->set(readonly => 1, view => 1);

	    # Nuke the [modified] message
	    $b->unmark();

	    # Move dot for one of the windows (maybe)
	    $w->dot($b->dot())		if $w;

	    # Update the screen
	    Vile::update();
	}
    );
}

Vile::register 'perl-capture-command' => \&capture_command,
		"run a command, capturing its output in the [Output] buffer";

1;
