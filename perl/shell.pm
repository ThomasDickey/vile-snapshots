package shell;

use IO::Pty;
require POSIX;
require Vile::Exporter;

@ISA = 'Vile::Exporter';
%REGISTRY = (
    'start-shell'  => [ \&shell, 'start an interactive shell' ],
    'resume-shell' => [ \&resume_shell, 'resume interactive shell' ],
);

my %shells = ();

sub terminal_emulation ($$$);

#
# Not meant to be complete.  Just enough so that I could use
# bash and limp along in vi (or vile) on a remote machine
#

sub terminal_emulation ($$$) {
    my ($b, $buf, $te_state) = @_;

    my (@dot) = $b->dotq;
    my (@enddot);

    while (length($buf) > 0) {
	if ($te_state->{IC} > 0) {
	    # Handle manditory characters to insert
	    my $ic = $te_state->{IC};
	    $buf =~ /^(.{0,$ic})(.*)$/s;
	    print $b $1;
	    $buf = $2;
	    $te_state->{IC} -= length($1);
	    @dot = $b->dotq;
	    check_wrap($b, $te_state, \@dot);
	}
	elsif ($buf =~ /^([\x09\x20-\xff]+)(.*)$/s) {
	    # Handle sequence of printable characters
	    $b->setregion(@dot, $dot[0], $dot[1]+length($1))
	        ->delete;
	    print $b $1;
	    $buf = $2;
	    @dot = $b->dotq;
	    check_wrap($b, $te_state, \@dot);
	}
	elsif ($buf =~ /^\r(.*)$/s) {
	    # ^M				-- beginning of line
	    $dot[1] = 0;
	    $b->dotq(@dot);
	    $buf = $1;
	}
	elsif ($buf =~ /^\n(.*)$/s) {
	    # ^J				-- down a line
	    #	 (Normally, this should just move down one line,
	    #	  but we've combined \n's with \r's before getting
	    #	  here...)
	    $buf = $1;
	    if ($te_state->{HEIGHT} != $te_state->{SR_BOT}
	        or $te_state->{SR_TOP} != 1) {
		# We're probably in some sort of editor where we have
		# to be careful with the scrolling region
		cursor_down_maybe_scroll($b, $te_state, \@dot, 'E');
	    }
	    else {
		$dot[0]++;
		$dot[1] = 0;
		@enddot = $b->dotq('$','$$');
		if ($dot[0] > $enddot[0]) {
		    print $b "\n";
		    #if ($dot[1] > 0) {
		    #    print $b ' ' x $dot[1];
		    #}
		    @dot = $b->dotq;
		}
		else {
		    $b->dotq(@dot);
		}
	    }
	}
	elsif ($buf =~ /^(\x08+)(.*)$/s) {
	    # Handle ^H
	    $dot[1] -= length($1);
	    if ($dot[1] < 0) {
		$dot[1] = 0;
	    }
	    $buf = $2;
	    $b->dotq(@dot);
	    
	    if ($b->setregion(@dot, $dot[0], '$')->fetch =~ /^\s+$/) {
		# Delete 'til end of line if all spaces
		$b->delete;
	    }
	    $b->dotq(@dot);	# fetch moved dot on us
	}
	elsif ($buf =~ /^(\x07+)(.*)$/) {
	    Vile::beep();
	    $buf = $2;
	}
	elsif ($buf =~ /^\e\[(\d*)P(.*)$/s) {
	    # ESC [ optional-digits P		-- delete characters forward
	    my $digs;
	    $digs = ($1 eq "") ? 1 : $1;
	    $b->setregion(@dot, $dot[0], $dot[1]+$digs)->delete;
	    $buf = $2;
	}
	elsif ($buf =~ /^\e\[(\d*)@(.*)$/s) {
	    # ESC [ optional-digits @		-- insert N characters
	    my $digs;
	    $digs = ($1 eq "") ? 1 : $1;
	    $buf = $2;
	    $te_state->{IC} = $digs;
	}
	elsif ($buf =~ /^\e\[K(.*)$/s) {
	    # ESC [ K				-- erase 'til eol
	    $b->setregion(@dot, $dot[0], '$$')->delete;
	    $buf = $1;
	}
	elsif ($buf =~ /^\e\[(\d*);(\d*)[Hf](.*)$/s) {
	    # ESC [ Pl ; Pc H			-- set position
	    my $lnum = $1 ? $1 : 1;
	    my $cnum = $2 ? $2 : 1;
	    $buf = $3;
	    set_position($b, $te_state, \@dot, $lnum, $cnum)
	}
	elsif ($buf =~ /^\e\[(\d*)[Hf](.*)$/s) {
	    # ESC [ Pl H			-- set position (linenum only)
	    my $lnum = $1 ? $1 : 1;
	    $buf = $2;
	    set_position($b, $te_state, \@dot, $lnum, 1)
	}
	elsif ($buf =~ /^\e\[(\d*)A(.*)$/s) {
	    # ESC [ Pn A				-- up n lines
	    my $digs = $1 ? $1 : 1;
	    $buf = $2;
	    cursor_down($b, $te_state, \@dot, -$digs);
	}
	elsif ($buf =~ /^\e\[(\d*)A(.*)$/s) {
	    # ESC [ Pn B				-- down n lines
	    my $digs = $1 ? $1 : 1;
	    $buf = $2;
	    cursor_down($b, $te_state, \@dot, $digs);
	}
	elsif ($buf =~ /^\e\[(\d*)([CD])(.*)$/s) {
	    # ESC [ Pn C				-- forward n chars
	    # ESC [ Pn D				-- backward n chars
	    my $digs = $1 ? $1 : 1;
	    $digs = -$digs	if $2 eq 'D';
	    $buf = $3;
	    cursor_forward($b, $te_state, \@dot, $digs);
	}
	elsif ($buf =~ /^\e\[(\d*)J(.*)$/s) {
	    # ESC [ how J			-- erase in display
	    my $how = $1 ? $1 : 0;
	    $buf = $2;
	    erase_display($b, $te_state, \@dot, $how);
	}
	elsif ($buf =~ /^\e\[(\d*);(\d*)r(.*)$/s) {
	    # ESC [ top ; bottom r		-- set scrolling region
	    my $top = $1 ? $1 : 1;
	    my $bot = $2 ? $2 : $te_state->{HEIGHT};
	    $buf = $3;
	    set_scroll_region($b, $te_state, \@dot, $top, $bot);
	}
	elsif ($buf =~ /^\e([DME])(.*)$/s) {
	    # ESC D				-- Index (IND)
	    # ESC M				-- Reverse Index (RI)
	    # ESC E				-- Next Line (NEL)
	    my $how = $1;
	    $buf = $2;
	    cursor_down_maybe_scroll($b, $te_state, \@dot, $how);
	}
	elsif ($buf =~ /^\e\[(\d*)L(.*)$/s) {
	    # ESC [ Pn L			-- insert N lines
	    my $digs;
	    $digs = ($1 eq "") ? 1 : $1;
	    $buf = $2;
	    insert_lines($b, $te_state, \@dot, $digs);
	}
	elsif ($buf =~ /^\e\[(\d*)M(.*)$/s) {
	    # ESC [ Pn M			-- delete N lines
	    my $digs;
	    $digs = ($1 eq "") ? 1 : $1;
	    $buf = $2;
	    delete_lines($b, $te_state, \@dot, $digs);
	}
	elsif ($buf =~ /^\e\[\d*(?:;\d+)*m(.*)$/s) {
	    # ESC [ Ps ; Ps ... m		-- select graphic rendition
	    # Ignore for now
	    $buf = $1;
	}
	elsif ($buf =~ /^\e\[\?\d+(?:;\d+)*[hl](.*)$/s) {
	    # ESC [ ? digits ; ... h		-- set mode (set)
	    # ESC [ ? digits ; ... l		-- set mode (reset)
	    # ignore...
	    $buf = $1;
	}
	elsif ($buf =~ /^\e[>=](.*)$/s) {
	    # Alternate keypad: Application vs Numeric	-- ignore
	    $buf = $1;
	}
	elsif ($buf =~ /^[\x0e\x0f](.*)$/s) {
	    # SO, SI characters: These are supposed to shift in different
	    # character sets.  We'll just ignore them for now.
	    $buf = $1;
	}
	else {
	    # Unhandled control character(s)
	    # just print them out...
	    # (And when they annoy you enough, add a case to handle
	    # them above.)

	    $buf =~ /^(.)(.*)$/s;

	    $b->setregion(@dot, $dot[0], $dot[1]+length($1))
	        ->delete;
	    print $b $1;
	    $buf = $2;
	    @dot = $b->dotq;
	}
    }
}

# See if newly inserted characters extend line beyond edge of screen
# Cause "wrapping" behavior if so.
sub check_wrap {
    my ($b, $te_state, $dotp) = @_;
    my @eoldot = $b->dotq($dotp->[0], '$');
    while ($eoldot[1] >= $te_state->{WIDTH}) {
	$b->dotq($eoldot[0], $te_state->{WIDTH});
	$b->insert("\n");
	@eoldot = $b->dotq($eoldot[0]+1, '$');
	$b->set_region($eoldot[0],'$$',$eoldot[0]+1,$eoldot[1]+1)->delete;
	@eoldot = $b->dotq($eoldot[0], '$');
    }
    $dotp->[0] += int($dotp->[1] / $te_state->{WIDTH});
    $dotp->[1] = $dotp->[1] % $te_state->{WIDTH};
    $b->dotq(@{$dotp});
}

# Position cursor at the specified position, possibly creating or
# extending lines as necessary.
sub set_position {
    my ($b, $te_state, $dotp, $lnum, $cnum) = @_;
    $cnum--;

    # Sanity check the parameters
    $lnum = 1			if $lnum < 1;
    $lnum = $te_state->{HEIGHT}	if $lnum > $te_state->{HEIGHT};
    $cnum = 0			if $cnum < 0;
    $cnum = $te_state->{WIDTH}-1
    				if $cnum >= $te_state->{WIDTH};

    my $endlinenum = $b->dotq('$','$$');
    if ($endlinenum < $lnum) {
	$b->insert("\n" x ($lnum - $endlinenum));
	$endlinenum = $lnum;
    }

    my $h = $te_state->{HEIGHT};
    $h = $endlinenum		if ($h > $endlinenum);

    $dotp->[0] = $endlinenum - $h + $lnum;
    $dotp->[1] = $cnum;
    maybe_extend_line($b, $dotp);
}

# Move cursor down (or up when $nlines is negative)
# Account for what happens at the top and bottom margins as well
# as filling with spaces as necessary
sub cursor_down {
    my ($b, $te_state, $dotp, $nlines) = @_;
    my @eobdot = $b->dotq('$', 0);
    my $topline = $eobdot[0] - $te_state->{HEIGHT};
    $topline = 0		if $topline < 0;
    $dotp->[0] += $nlines;
    $dotp->[0] = $eobdot[0]	if $dotp->[0] > $eobdot[0];
    $dotp->[0] = $topline+1	if $dotp->[0] <= $topline;
    maybe_extend_line($b, $dotp);
}

# Move cursor forward (or backward).
# Don't let cursor move off the "screen".  Also account for filling
# line with spaces as necessary
sub cursor_forward {
    my ($b, $te_state, $dotp, $count) = @_;
    my $hpos = $dotp->[1] + $count;
    $hpos = 0 			if $hpos < 0;
    $hpos = $te_state->{WIDTH} - 1
    				if $hpos >= $te_state->{WIDTH};
    $dotp->[1] = $hpos;
    maybe_extend_line($b, $dotp);
}

# Attempt to set dot at the indicated position, extending the line
# if necessary
sub maybe_extend_line {
    my ($b, $dotp) = @_;
    my @actualdot = $b->dotq(@{$dotp});
    my $spacesneeded = $dotp->[1] - $actualdot[1];
    $b->insert(' ' x $spacesneeded)	if $spacesneeded > 0;
}

# Erase in Display
#   $how values:	
# 	0 - From cursor to end of screen (including cursor)
#	1 - From beginning of screen to cursor (including cursor)
#	2 - Everything.  Cursor does not move, however.
sub erase_display {
    my ($b, $te_state, $dotp, $how) = @_;
    my $endlinenum = $b->dotq('$','$$');

    # Make sure there are enough lines
    if ($endlinenum < $te_state->{HEIGHT}) {
	$b->insert("\n" x ($te_state->{HEIGHT} - $endlinenum));
	$endlinenum = $te_state->{HEIGHT};
    }

    if ($how == 0) {
	# Erase from cursor to end of screen
	$b->set_region(@{$dotp}, $dotp->[0], '$$')->delete;
	my $i;
	for ($i = $dotp->[0]+1; $i <= $linenum; $i++) {
	    $b->set_region($i, 0, $i, '$$')->delete;
	}
    }
    elsif ($how == 1) {
	# Erase from beginning of screen to cursor
	my $i;
	for ($i = $endlinenum - $te_state->{HEIGHT} + 1; $i < $dotp->[0]; $i++)
	{
	    $b->set_region($i, 0, $i, '$$')->delete;
	}
	$b->set_region($dotp->[0], 0, $dotp->[0], $dotp->[1] + 1)->delete;
    }
    else {
	# Erase everything
	my $i;
	for ($i = $endlinenum - $te_state->{HEIGHT} + 1; $i < $endlinenum; $i++)
	{
	    $b->set_region($i, 0, $i, '$$')->delete;
	}
    }
    maybe_extend_line($b, $dotp);
}

# Sets the scrolling region affected by cursor_down_maybe_scroll
sub set_scroll_region {
    my ($b, $te_state, $dotp, $top, $bot) = @_;
    $top = 1				if $top < 1;
    $top = $te_state->{HEIGHT} - 1	if $top >= $te_state->{HEIGHT};
    $bot = $top + 1			if $top >= $bot;
    $bot = $te_state->{HEIGHT}		if $bot > $te_state->{HEIGHT};
    $te_state->{SR_TOP} = $top;
    $te_state->{SR_BOT} = $bot;
}

# Move the cursor down (or up) by a line and scroll if positioned on
# one of the margins specified by the scroll region (saved in te_state).
# The ``how'' argument will be one of
#	M	- move up by a line
#	D	- move down by a line
#	E	- move down to the start of the next line
sub cursor_down_maybe_scroll {
    my ($b, $te_state, $dotp, $how) = @_;
    my $nlines = ($how eq 'M') ? -1 : 1;
    my @eobdot = $b->dotq('$', 0);
    my $topline = $eobdot[0] - $te_state->{HEIGHT};
    $topline = 0		if $topline < 0;
    if ($nlines < 0 and $dotp->[0] - $topline == $te_state->{SR_TOP}) {
	$b->set_region($topline + $te_state->{SR_BOT}, 0,
	               $topline + $te_state->{SR_BOT} + 1, 0)->delete;
	$b->dotq($topline + $te_state->{SR_TOP}, 0);
	$b->insert("\n");
    }
    elsif ($nlines > 0 and $dotp->[0] - $topline == $te_state->{SR_BOT}) {
	$b->dotq($topline + $te_state->{SR_BOT} + 1, 0);
	$b->insert("\n");
	$b->set_region($topline + $te_state->{SR_TOP}, 0,
	               $topline + $te_state->{SR_TOP} + 1, 0)->delete;
    }
    else {
	$dotp->[0] += $nlines;
	$dotp->[0] = $eobdot[0]	if $dotp->[0] > $eobdot[0];
	$dotp->[0] = $topline+1	if $dotp->[0] <= $topline;
    }

    $dotp->[1] = 0		if $how eq 'E';
    maybe_extend_line($b, $dotp);
}

# Insert lines within the scrolling region.  Has no affect if outside
# the scrolling region.  Cursor is reset to first column afterwards.
sub insert_lines {
    my ($b, $te_state, $dotp, $count) = @_;
    my @eobdot = $b->dotq('$', 0);
    my $topline = $eobdot[0] - $te_state->{HEIGHT};
    $topline = 0		if $topline < 0;
    if ($dotp->[0] >= $topline + $te_state->{SR_TOP}
    and $dotp->[0] <= $topline + $te_state->{SR_BOT}) {
	if ($count > $te_state->{SR_BOT} - ($dotp->[0] - $topline)) {
	    $count = $te_state->{SR_BOT} - ($dotp->[0] - $topline);
	}
	$b->set_region($topline + $te_state->{SR_BOT} - $count + 1, 0,
	               $topline + $te_state->{SR_BOT} + 1, 0)->delete;
	$dotp->[1] = 0;
        $b->dotq(@{$dotp});
	$b->insert("\n" x $count);
        $b->dotq(@{$dotp});
    }
}

# Delete lines within the scrolling region.  Has no affect if outside
# the scrolling region.  Cursor is reset to first column afterwards.
sub delete_lines {
    my ($b, $te_state, $dotp, $count) = @_;
    my @eobdot = $b->dotq('$', 0);
    my $topline = $eobdot[0] - $te_state->{HEIGHT};
    $topline = 0		if $topline < 0;
    if ($dotp->[0] >= $topline + $te_state->{SR_TOP}
    and $dotp->[0] <= $topline + $te_state->{SR_BOT}) {
	if ($count > $te_state->{SR_BOT} - ($dotp->[0] - $topline)) {
	    $count = $te_state->{SR_BOT} - ($dotp->[0] - $topline);
	}
	$b->dotq($topline + $te_state->{SR_BOT} + 1);
	$b->insert("\n" x $count);
	$dotp->[1] = 0;
	$b->set_region(@{$dotp}, $dotp->[0] + $count, 0)->delete;
        $b->dotq(@{$dotp});
    }
}

# Given a buffer, return a string that will be unique for that buffer
# We don't want to rely on vile's name for the buffer because the user
# can change that.  (We use this information for resuming / controlling
# shells.)
sub buffer_name_internal {
    my ($b) = @_;
    my $internal_name = undef;
    if (*{$b}{PACKAGE} eq "Vile::Buffer") {
	$internal_name = *{$b}{NAME};
    }
    return $internal_name;
}

# Initialize some of the state variables associated with the terminal
# emulator.
#
#	IC        - number of characters remaining to insert
#	WIDTH     - width of the terminal
#	HEIGHT    - height of the terminal
#	SR_TOP    - top line in scrolling region
#	SR_BOT    - bottom line in scrolling region
#
sub initial_te_state {
    return { IC => 0, WIDTH => 80, HEIGHT => 24, SR_TOP => 1, SR_BOT => 24 };
}

# Test to see if the terminal emulator *must* be called
sub must_emulate {
    return $_[0]->{IC} != 0;
}

# Use ``do'' instead of ``require'' to bring in sys/ioctl.ph.  It isn't vital
# to have it, but it does add some nice functionality.
do 'sys/ioctl.ph';

# Make sure the size of the vile window holding the shell matches the
# width and height saved in the terminal emulator state.
sub check_size {
    return unless defined &TIOCSWINSZ;

    my ($w, $te_state, $pty) = @_;
    my ($height, $width) = $w->size;
    if ($height != $te_state->{HEIGHT} or $width != $te_state->{WIDTH}) {
	$sizeparam = pack 'S4', $height, $width, 0, 0;
	if (ioctl($pty, &TIOCSWINSZ, $sizeparam) == 0) {
	    $te_state->{WIDTH} = $width;
	    $te_state->{HEIGHT} = $height;
	    # Just reset the scrolling region to be the entire display.
	    $te_state->{SR_TOP} = 1;
	    $te_state->{SR_BOT} = $height;
	}
    }
}

# Do the low level work of starting a shell... Returns the pty's filehandle
# and the pid of the shell if successful.  If it's unsuccessful, the die
# message will tell you why.
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

# Start a shell and put it in it's own window if possible.  Invoked by
# user command
sub shell
{
    # Uncomment for debugging (uncomment hexdump line below too)
    #open TTY, ">/dev/tty";

    # Disable working messages
    my $oldworking = Vile::working();
    Vile::working(0);

    # Start a new shell
    my ($pty, $pid ) = start_shell();
    die "shell: start_shell failed" 		unless defined $pty;

    # Create a buffer to display the shell in
    my $b = new Vile::Buffer;
    die "shell: Couldn't create buffer"		unless defined $b;
    $b->buffername("shell-$pid");

    # Create a unique name for the buffer and stow away info that we
    # may need later on  (e.g, for resuming the shell)
    my $bufid = &buffer_name_internal($b);
    $shells{$bufid}{PTY_HANDLE} = $pty;
    $shells{$bufid}{PID}        = $pid;
    $shells{$bufid}{BUF_HANDLE} = $b;

    # Set some buffer attributes
    $b->set(tabstop => 8, tabinsert => 0);

    # Pop up a new window with the buffer in it
    my $w = Vile::Window->new($b);

    # If we can't do that, use the current window for our buffer
    if (!$w) {
	$w = Vile::current_window;
	$w->buffer($b);
    }

    # Make $w the current window
    $w->current_window();

    # Put an initial newline in the buffer and then update
    print $b " \n";
    $w->dot($b->dotq('$$'));
    $b->unmark;
    Vile::update();

    my (@mydot) = $b->dotq;

    # Create the initial state for the terminal emulator
    my $te_state = initial_te_state();

    # Have vile watch for shell output while waiting for user input;
    # when not otherwise busy, vile will call the anonymous subroutine
    # which we pass as the third parameter
    Vile::watchfd(
	fileno($pty),
	'read',
	sub {
	    my $buf = ' ' x 4096;
	    my $lastlnum;

	    # Reset the buffer's dot.  Unfortunately, the perl
	    # interface trys to be a little too smart sometimes,
	    # so we must remember where dot was from the last time
	    # we were here.
	    $b->dotq(@mydot);

	    # Try to find a window associated with the buffer which
	    # also has DOT near @mydot.  (Plus or minus one line.)
	    # Give preference to the current window if that matches
	    # our criteria.
	    my $w = Vile::current_window;
	    unless ($b->buffername eq $w->buffer->buffername
	        and (my @wdot = $w->dot())[0] >= $mydot[0] - 1
	        and $wdot[0] <= $mydot[0] + 1)
	    {
		foreach my $i (0..Vile::window_count()-1) {
		    $w = Vile::window_at($i);
		    if ($b->buffername eq $w->buffer->buffername
		    and (my @wdot = $w->dot())[0] >= $mydot[0] - 1
		    and $wdot[0] <= $mydot[0] + 1) {
			last;
		    }
		    $w = undef;
		}
	    }

	    # Fetch data from input stream
	    if (!sysread $pty, $buf, 4096) {
		Vile::unwatchfd(fileno($pty));
		close $pty;
		delete $shells{$bufid};
		waitpid $pid, 0;
		print STDOUT "Shell $pid is dead.  Press any character to resume editing.";
		Vile::update();
		return;
	    }

	    $buf =~ s/\r\n/\n/gs;		# nuke ^M characters

	    my @enddot = $b->dotq('$','$$');

	    # Uncomment for debugging
	    #hexdump($buf);

	    if ($buf =~ /[\x01-\x08\x0b-\x1f]/
		|| $mydot[0] != $enddot[0] 
		|| $mydot[1] != $enddot[1]
		|| must_emulate($te_state))
	    {
		# Yuck, it contains control characters, or it's
		# not at the end of the buffer
		# ...we have some work to do
		$b->dotq(@mydot);
		terminal_emulation($b, $buf, $te_state)
	    } else {
		# Write data to the editor's buffer
		print $b $buf;
	    }
	    # Nuke the [modified] message
	    $b->unmark();

	    # Remember where we are
	    @mydot = $b->dotq();

	    # Move dot/scroll for one of the windows (maybe)
	    if ($w) {
		my $endline = $b->dotq('$');
		my $topline = $endline - $w->size + 1;
		$topline = 1 		if $topline < 1;
		$b->dotq(@mydot);
		$w->topline($topline);
		$w->dot(@mydot);
		check_size($w, $te_state, $pty);
	    }

	    # Update the screen
	    Vile::update();
	    # Uncomment the following line (of code) to get the current
	    # dot printed to the message line.  (This used to cause a
	    # segfault.)
	    #print "After update: ", join(',',$b->dot());
	}
    );

    print STDOUT "Shell started.  Use ^G to escape";
    Vile::update();

    # Stow away a reference to the character getting subroutine.
    # We use it immediately below and when we want character input
    # to be directed to the shell.  (I.e, when resume_shell is called.)
    $shells{$bufid}{RESUME_SUB} = 
	sub {
	    # Restore dot to its rightful place
	    Vile::current_window()->dot(@mydot);
	    Vile::update();

	    my $c;
	    while (1) {
		$c = Vile::keystroke();
		# FIXME: Need more flexible escape mechanism
		last if $c == 7;		# ^G escapes
		last unless exists $shells{$bufid};
		print $pty chr($c);
	    }
	};

    &{$shells{$bufid}{RESUME_SUB}};		# Run the above loop

    print STDOUT 'Editor resumed.  Use ":resume-shell" to return.'
    					if exists $shells{$bufid};
    Vile::update();

    # Restore "working..." message display to its previous state
    Vile::working($oldworking);
}

# Resume a shell at the user's request.
#
# We used to extract the pid from the user's buffer and use that as a key
# to look up the shell, but that won't always work because the user may
# rename the buffer!
sub resume_shell {
    my $bufid = buffer_name_internal(Vile::current_buffer);

    if (!defined($bufid) or !defined($shells{$bufid}{PTY_HANDLE})) {
	print "Not in a shell window!";
	return;
    }

    my $oldworking = Vile::working();		# fetch old value of working
    Vile::working(0);				# disable working... messages

    print STDOUT "Shell resumed.  Use ^G to escape";
    Vile::update();

    # Run the keyboard character fetching loop
    &{$shells{$bufid}{RESUME_SUB}};

    print STDOUT 'Editor resumed.  Use ":resume-shell" to return.'
    					if exists $shells{$bufid};

    Vile::working($oldworking);			# restore "working..." message
}

# Debugging subroutine
sub hexdump {
    my ($buffer) = @_;
    my ($hexstr);
    $hexstr = unpack("H*", $buffer);
    $hexstr =~ s/(..)/ $1/g;
    $buffer =~ s/[\x00-\x1f\x80-\xff]/./g;
    $buffer =~ s/(.)/  $1/g;
    print TTY "$hexstr\n$buffer\n";
}

1;
