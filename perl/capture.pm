package capture;

require Vile::Exporter;
@ISA = 'Vile::Exporter';
%REGISTRY = (
    'async-capture-command' => [
	\&capture_command,
	'run a command, capturing its output in the [Output] buffer',
	'^A-!',
    ],
);

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

    # Fetch user command from the message line
    my $command = Vile::mlreply_no_opts("async-!");
    return if !defined($command);

    # Get a handle for or create the [Output] buffer by whatever
    # means necessary
    my $b = edit Vile::Buffer '[Output]';
    if (!$b) {
	$b = new Vile::Buffer;
	$b->buffername('[Output]');
    }

    # Make sure we can write to the buffer
    $b->set(readonly => 0, view => 0);

    # Output a bold header (just the command name and a bunch of dashes)
    my @startdot = $b->dotq('$$');
    print $b "\n$command\n----------------\n";
    $b->set_region(@startdot, '$', '$$')->attribute('bold', color => 4);

    # Remember the starting dot for the error finder
    @startdot = $b->dotq('$$');

    # Set [read-only view-only loading] attributes for the mode line
    $b->set(readonly => 1, view => 1, loading => 1);

    # Find a window corresponding to the buffer
    my $w = undef;
    foreach my $i (0..Vile::window_count()-1) {
	if ($b->buffername eq Vile::window_at($i)->buffer->buffername) {
	    $w = Vile::window_at($i);
	    last;
	}
    }

    # Pop up a window and set dot if possible
    $w = Vile::Window->new($b)		unless $w;
    $w->dot($b->dot())			if $w;

    # Clear the [modified] attribute
    $b->unmark;

    # Run the command
    unless (open CAPTURE, "$command 2>&1 |") {
	print STDERR "Can't run command $command";
	return;
    }

    # Have vile watch for output from the command while waiting
    # for user input; when not otherwise busy, vile will call the
    # anonymous subroutine which we pass as the third parameter
    Vile::watchfd(
	fileno(CAPTURE),
	'read',
	sub {
	    my $buf = ' ' x 4096;

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

	    # Fetch data from input stream
	    if (!sysread CAPTURE, $buf, 4096) {
		# We're at the end of the stream; stop watching
		# for input on this file handle and close it down
		Vile::unwatchfd(fileno(CAPTURE));
		close CAPTURE;

		# Clear the [loading] attribute on the mode line
		$b->set(loading => 0);

		# Reset dot to the beginning of the text that was
		# just loaded for the error finder.  But only do
		# it if the user's dot was positioned at the end
		# of the buffer.  We won't move it on him if he
		# was browsing the buffer.
		$w->dot(@startdot)	if $w;

		# Update the screen
		Vile::update();

		return;
	    }

	    # Nuke ^M characters
	    $buf =~ s/\r\n/\n/gs;

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

    # Associate the [Output] buffer with the error finder.  When
    # invoked without an argument, this is its default action.
    Vile::command 'error-buffer';
}

1;
