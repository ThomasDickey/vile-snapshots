package shell;

use IO::Pty;
require POSIX;
use Vile::Manual;
require Vile::Exporter;
use Fcntl;

@ISA = 'Vile::Exporter';
%REGISTRY = (
    'start-shell'  => [ \&shell, 'start an interactive shell' ],
    'xterm'        => [ \&xterm, 'start an interactive shell in an xterm' ],
    'resume-shell' => [ \&resume_shell, 'resume interactive shell' ],
    'shell-help'   => [ sub {&manual}, 'manual page for shell.pm' ],
);

my %shells = ();

# Provide some rudimentary terminal emulation
#
# Not meant to be complete.  Just enough so that I could use
# bash and limp along in vi (or vile) on a remote machine
#
# I found the vt220 manual at http://vt100.net/docs/vt220-rm/
# to be quite useful for this endeavor.
sub terminal_emulation {
    my ($b, $buf, $te_state) = @_;

    my (@dot) = $b->dotq;
    my (@enddot);

    if (length($te_state->{PUTBACK}) > 0) {
	$buf = $te_state->{PUTBACK} . $buf;
	$te_state->{PUTBACK} = '';
    }

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
	    $buf = $2;
	    overwrite($b, $te_state, \@dot, $1);
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
	elsif ($buf =~ /^\e\[(\d*)([AB])(.*)$/s) {
	    # ESC [ Pn A				-- up n lines
	    # ESC [ Pn B				-- down n lines
	    my $digs = $1 ? $1 : 1;
	    $digs = -$digs	if $2 eq 'A';
	    $buf = $3;
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
	elsif ($buf =~ /^\032/) {
	    # Possible gdb annotation
	    if ($buf =~ /^\032\032	# two ctrl-Z's start a gdb annotation
			([^\n]*)	# filename
			:		# colon separator
			(\d+)		# line number
			:
			(\d+)		# file offset
			:
			(beg|middle)	# indicator as to where we are
			:		#   in the statement
			(\S+)		# address that we stopped at
			\n		# newline terminates it
			(.*)		# remainder of buffer
			$/sx) {
		$buf = $6;
		open_gdb_file($b, $te_state, \@dot, $1, $2, $3, $4, $5);
	    }
	    elsif ($buf =~ /^\032(\032[^\n]*)?$/) {
		# Incomplete annotation
		$te_state->{PUTBACK} = $buf;
		$buf = '';
	    }
	    elsif ($buf =~ /^(\032\032?)(.*)$/s) { # should always match
		# Error in annotation
		#    (or perhaps an annotation level higher than 1)
		# Write out the control-Z(s) and let the chips
		# fall where they may
		$buf = $2;
		overwrite($b, $te_state, \@dot, $1);
	    }
	}
	elsif ($buf =~ /^\e(\[[^a-zA-Z?@]*)?$/) {
	    # Incomplete escape sequence
	    $te_state->{PUTBACK} = $buf;
	    $buf = '';
	}
	elsif ($buf =~ /^\0+(.*)$/s) {
	    # Null characters; these are for timing purposes.
	    # Just ignore them.
	    $buf = $1;
	}
	else {
	    # Unhandled control character(s)... Just print them out.
	    # (And when they annoy you enough, add a case to handle
	    # them above.)

	    $buf =~ /^(.)(.*)$/s;
	    $buf = $2;
	    overwrite($b, $te_state, \@dot, $1);
	}
    }
}

# Cause a sequence of characters to overwrite what's already there
sub overwrite {
    my ($b, $te_state, $dotp, $chars) = @_;
    $b->setregion(@{$dotp}, $dotp->[0], $dotp->[1]+length($chars))
	->delete;
    print $b $chars;
    @{$dotp} = $b->dotq;
    check_wrap($b, $te_state, $dotp);
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
	for ($i = $dotp->[0]+1; $i <= $endlinenum; $i++) {
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

# Open a file from information given in a gdb annotation
sub open_gdb_file {
    my ($b, $te_state, $dotp, $fname, $lnum, $foff, $mid, $addr) = @_;

    # Attempt to open the pathname given to us by gdb
    my $gb = Vile::Buffer->new($fname);

    # The above will nearly always work, but the results are not
    # useful to us if vile creates a new file to edit
    if (!defined($gb) or ($gb->set_region(1,'$'))[2] < $lnum) {
	$b->dotq(@{$dotp});
	print $b "Stopped at $addr ($fname:$lnum not found)\n";
	@{$dotp} = $b->dotq;
	return;
    }

    # If we get here, we've got a file to display along with
    # a line number.  Now we've got to find a place to display
    # it.  First see if there's a window open to that buffer...

    my $i = 0;
    my $w = undef;
    while ($w = Vile::window_at($i++)) {
	last if $w->buffer->buffername eq $gb->buffername;
    }

    # If $w is defined at this point, we have window open to the
    # buffer already.  Otherwise, we need to open one.  First, we'll
    # try to find an existing window that isn't one of our shell
    # windows.

    if (!defined($w)) {
	$i = 0;
	while ($w = Vile::window_at($i++)) {
	    last if $w->buffer->buffername ne $b->buffername;
	}
    }

    # If $w is still undefined, we'll need create a new window
    $w = Vile::Window->new		unless defined $w;

    # If it's still undefined, we'll just pick the first one.
    $w = Vile::window_at(0)		unless defined $w;

    # Now that we finally have a window to use, set it up.
    $w->buffer($gb);
    $w->dot($lnum);
    $gb->set_region($lnum,0,$lnum,'$$')->set_selection;

    # Put some visible indication of the fact that we've stopped in
    # the shell buffer.
    $b->dotq(@{$dotp});

    # Try not to let dot lie on the bottom line
    if ($w->size > 3 and $w->topline + $w->size - 1 == $w->dot) {
	$w->topline($w->topline + int($w->size * 2 / 3));
    }

    $fname =~ s#^.*/##;
    print $b "Stopped at $addr in $fname:$lnum\n";
    @{$dotp} = $b->dotq;
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
#	PUTBACK   - unconsumed characters (probably partial sequences)
#	WIDTH     - width of the terminal
#	HEIGHT    - height of the terminal
#	SR_TOP    - top line in scrolling region
#	SR_BOT    - bottom line in scrolling region
#
sub initial_te_state {
    return { IC => 0, PUTBACK => '',
            WIDTH => 80, HEIGHT => 24,
	    SR_TOP => 1, SR_BOT => 24 };
}

# Test to see if the terminal emulator *must* be called
sub must_emulate {
    return $_[0]->{IC} != 0 || length($_[0]->{PUTBACK}) > 0;
}

# Use ``do'' instead of ``require'' to bring in sys/ioctl.ph.  It isn't vital
# to have it, but it does add some nice functionality.
do 'sys/ioctl.ph';

# Make sure the size of the vile window holding the shell matches the
# width and height saved in the terminal emulator state.
sub check_size {
    return unless defined &TIOCSWINSZ;

    my ($w, $te_state, $shell_pty, $xterm_tty) = @_;
    my ($height, $width);

    if (defined $xterm_tty) {
	my $winsz = '\0' x 8;
	ioctl($xterm_tty, &TIOCGWINSZ, $winsz);
	($height, $width) = unpack 'S4', $winsz;
	$height = 24 			if $height <= 0;
	$width  = 80			if $width <= 0;
    }
    else {
	($height, $width) = $w->size;
    }
    if ($height != $te_state->{HEIGHT} or $width != $te_state->{WIDTH}) {
	$sizeparam = pack 'S4', $height, $width, 0, 0;
	if (ioctl($shell_pty, &TIOCSWINSZ, $sizeparam) == 0) {
	    #print TTY "After TIOCSWINSZ call: $height, $width\n";
	    $te_state->{WIDTH} = $width;
	    $te_state->{HEIGHT} = $height;
	    # Just reset the scrolling region to be the entire display.
	    $te_state->{SR_TOP} = 1;
	    $te_state->{SR_BOT} = $height;
	}
    }
}

# Try to find a window associated with the buffer which also has DOT
# near the passed in dot.  (Plus or minus one line.) Give preference
# to the current window if that matches our criteria.  May return
# undef if no suitable window is found.
sub find_best_window_for_buffer {
    my ($b, @dot) = @_;
    return undef unless @dot > 0;
    my $w = Vile::current_window;
    unless ($b->buffername eq $w->buffer->buffername
	and (my @wdot = $w->dot())[0] >= $dot[0] - 1
	and $wdot[0] <= $dot[0] + 1)
    {
	foreach my $i (0..Vile::window_count()-1) {
	    $w = Vile::window_at($i);
	    if ($b->buffername eq $w->buffer->buffername
	    and (my @wdot = $w->dot())[0] >= $dot[0] - 1
	    and $wdot[0] <= $dot[0] + 1) {
		last;
	    }
	    $w = undef;
	}
    }
    return $w;
}

# Find any window associated with the given buffer giving
# (possible) preference to one with dot near the indicated
# location.
sub find_window_for_buffer {
    my ($b, @dot) = @_;

    # Try to find the best one
    my $w = find_best_window_for_buffer($b, @dot);
    return $w				if defined $w;

    # No best one; just find one that's visible
    my $i = 0;
    while ($w = Vile::window_at($i++)) {
	if ($b->buffername eq $w->buffer->buffername) {
	    last;
	}
    }
    return $w				if defined $w;

    # Well, try to create one then
    $w = Vile::Window->new;

    # But if that doesn't work either, just use the first one.
    $w = Vile::window_at(0)		unless defined $w;

    # Set it to our buffer and then return
    $w->buffer($b);
    return $w;
}

# Return a string suitable for describing a character (using printable
# characters); especially useful for control characters.
sub char_name {
    my ($ch) = @_;
    return "ESC" 			if ($ch eq "\e");
    return "^" . chr(ord($ch) + 64)	if (ord($ch) < 32);
    return $ch;
}

# Do the low level work of starting a shell... Returns the pty's filehandle
# and the pid of the shell if successful.  If it's unsuccessful, the die
# message will tell you why.
sub start_shell {
    my ($shell, $termtype) = @_;
    $shell = $ENV{SHELL} 		unless defined $shell;
    $shell = '/bin/sh' 			unless defined $shell;

    my $pty     = new IO::Pty		or die "Can't open new pty: $!";

    my $pid = fork;
    die "Can't fork"			if $pid < 0;

    if ($pid == 0) {
	POSIX::setsid();
	my $tty	= $pty->slave;
	ioctl($tty, &TIOCSCTTY, 0)	if defined(&TIOCSCTTY);
	untie *STDIN;
	untie *STDOUT;
	untie *STDERR;
	open(STDIN, "<&".fileno($tty))	or die "Can't reopen STDIN: $!";
	open(STDOUT, ">&STDIN")		or die "Can't reopen STDOUT: $!";
	open(STDERR, ">&STDIN")		or die "Can't reopen STDERR: $!";
	system("stty sane");
	close $pty;
	close $tty;
	$ENV{TERM} = $termtype;
	exec $shell;
    }
    return ($pty, $pid);
}

sub start_xterm {
    my ($shell_pid) = @_;

    my $pty = new IO::Pty;

    unless ($pty) {
	die "Couldn't start xterm: can't open a pty.";
	return undef;
    }

    my $pid = fork;
    if ($pid < 0) {
	die "Couldn't start xterm: $!";
	return undef;
    }

    my $ttyname   = $pty->ttyname;
    if ($pid) {
	# Parent
	my $tty = $pty->slave;		# Open the tty-end
	system("stty raw -echo -icrnl <$ttyname");
	$tty->autoflush(1);
	return ($tty, $pid);
    }
    else {
	# Child
	my $fileno = fileno($pty);
	my ($twochars) = $ttyname =~ /(..)$/;

	fcntl($pty, F_SETFD, 0);

	# Pass the final two characters from the slave's ttyname
	# in addition to file descriptor number for the master.
	# Unfortunately, it does not work to call the TIOCSPGRP
	# ioctl from the parent (on the tty end of things) because
	# the parent process already has a controlling terminal.
	# (This would be desirable in order to receive SIGWINCH
	# signals in order to learn that the xterm had been
	# resized.)

	exec "xterm", "-S${twochars}$fileno",
	              "-title", "xvile shell-$shell_pid";
    }
}

# Start a shell w/ a slave xterm
sub xterm {
    my ($command) = @_;
    shell($command, start_xterm => 1);
}

# Start a shell and put it in it's own window if possible.  Invoked by
# user command
sub shell
{
    my ($command, %opts) = @_;
    # Uncomment for debugging (uncomment hexdump line below too)
    open TTY, ">/dev/tty";

    # Start a new shell
    my ($shell_pty, $shell_pid )
	= start_shell($command, $opts{start_xterm} ? "xterm" : "vt100");
    die "shell: start_shell failed" 		unless defined $shell_pty;

    # Start an xterm
    my ($xterm_tty, $xterm_pid);
    if ($opts{start_xterm}) {
	($xterm_tty, $xterm_pid) = start_xterm($shell_pid);
	die "shell: start_xterm failed"		unless defined $xterm_tty;
    }

    # Create a buffer to display the shell in
    my $b = new Vile::Buffer;
    die "shell: Couldn't create buffer"		unless defined $b;
    $b->buffername("shell-$shell_pid");

    # Create a unique name for the buffer and stow away info that we
    # may need later on  (e.g, for resuming the shell)
    my $bufid = &buffer_name_internal($b);
    $shells{$bufid}{PTY_HANDLE} = $shell_pty;
    $shells{$bufid}{PID}        = $shell_pid;
    $shells{$bufid}{BUF_HANDLE} = $b;
    if (defined $xterm_tty) {
	$shells{$bufid}{XTERM_HANDLE} = $xterm_tty;
	$shells{$bufid}{XTERM_PID}  = $xterm_pid;
    }

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

    if (defined $xterm_tty) {
	Vile::watchfd(
	    fileno($xterm_tty),
	    'read',
	    sub {
		my $buf = ' ' x 4096;

		# Fetch data from input stream
		if (!sysread $xterm_tty, $buf, 4096) {
		    Vile::unwatchfd(fileno($xterm_tty));
		    close $xterm_tty;
		    waitpid $xterm_pid, 0;
		    return;
		}

		if (!defined($shells{$bufid}{XTERM_ID})) {
		    my ($id, $rest) = $buf =~ /^([\dA-Fa-f]+)\n?(.*)/;
		    $shells{$bufid}{XTERM_ID} = $id;
		    $buf = $rest;
		    check_size($w, $te_state, $shell_pty, $xterm_tty);
		    return		if $buf eq '';
		}

		print $shell_pty $buf;

	    }
	);
    }

    # Have vile watch for shell output while waiting for user input;
    # when shell output is available (and when not otherwise busy), vile
    # will call the anonymous subroutine which we pass as the third parameter
    Vile::watchfd(
	fileno($shell_pty),
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
	    my $w = find_best_window_for_buffer($b, @mydot);

	    # Fetch data from input stream
	    if (!sysread $shell_pty, $buf, 4096) {
		Vile::unwatchfd(fileno($shell_pty));
		close $shell_pty;
		if (defined $shells{$bufid}{XTERM_PID}) {
		    kill 15, $shells{$bufid}{XTERM_PID};
		    close $shells{$bufid}{XTERM_PTY};
		    kill 9, $shells{$bufid}{XTERM_PID};
		}
		delete $shells{$bufid};
		waitpid $shell_pid, 0;
		print STDOUT "Shell $shell_pid is dead.  Press any character to resume editing.";
		Vile::update();
		return;
	    }

	    # Send raw buffer to xterm
	    print $xterm_tty $buf		if defined $xterm_tty;

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
		check_size($w, $te_state, $shell_pty, $xterm_tty);
	    }

	    # Update the screen
	    Vile::update();
	    # Uncomment the following line (of code) to get the current
	    # dot printed to the message line.  (This used to cause a
	    # segfault.)
	    #print "After update: ", join(',',$b->dot());
	}
    );

    # Stow away a reference to the character getting subroutine.
    # We use it immediately below and when we want character input
    # to be directed to the shell.  (I.e, when resume_shell is called.)
    $resume_sub{$bufid} = # Work-around for a perl bug. This line should not be needed!
    $shells{$bufid}{RESUME_SUB} =
	sub {
	    my ($escape, $quote) = @_;
	    my $w = find_window_for_buffer($b, @mydot);

	    # Restore dot to its rightful place
	    $w->dot(@mydot);
	    $w->current_window;
	    Vile::update();

	    # Disable working messages
	    my $oldworking = Vile->get('working');
	    Vile->set(working => 0);

	    # Tell user how to get back to vile
	    my $escape_name = char_name($escape);
	    my $quote_name  = char_name($quote);
	    print STDOUT "Use $escape_name to escape shell, $quote_name to escape characters";
	    Vile::update();

	    my $c;
	    my $quote_next = 0;
	    my $escape_disabled = 0;
	    while (1) {
		$c = Vile::keystroke();
		last unless exists $shells{$bufid};
		if ($quote_next) {
		    $quote_next = 0;
		    if ($c == ord('e')) {
			$escape_disabled = !$escape_disabled;
			print STDOUT "$escape_name toggled: $escape_name will ",
			      $escape_disabled ? "be sent to shell"
			                       : "escape from shell";
			Vile::update();
			next;
		    }
		    print STDOUT $quote_name, char_name(chr($c));
		    Vile::update();
		}
		else {
		    if ($c == ord($escape) && !$escape_disabled) {
			last;
		    }
		    if ($c == ord($quote)) {
			$quote_next = 1;
			print STDOUT $quote_name;
			Vile::update();
			next;
		    }
		}
		print $shell_pty chr($c);
	    }

	    if (exists $shells{$bufid}) {
		print STDOUT 'Editor resumed.  Use ":resume-shell" to return.'
	    }
	    else {
		# probably here because shell died in which case we've
		# already printed a message.  The line below will provide
		# a small amount of visual feedback to the user...
		# (by clearing the message which said to press any key
		# to continue).
		print STDOUT ' ';
	    }
	    Vile::update();

	    Vile->set(working => $oldworking);
	};

    Vile::update();

    if (defined($command)) {
	# FIXME: It'd be better to return an object
	return $bufid;
    }
    else {
	&{$shells{$bufid}{RESUME_SUB}}("\e", "\cV");	# Run the above loop
	clean_resume_sub();			# Workaround for a perl bug
    }
}

# Resume a shell at the user's request.
#
# We used to extract the pid from the user's buffer and use that as a key
# to look up the shell, but that won't always work because the user may
# rename the buffer!
sub resume_shell {
    my ($bufid) = @_;
    $bufid = buffer_name_internal(Vile::current_buffer)
    					unless defined $bufid;

    if (!defined($bufid) or !defined($shells{$bufid}{PTY_HANDLE})) {
	print "Not in a shell window!";
	return;
    }

    # Run the keyboard character fetching loop
    &{$shells{$bufid}{RESUME_SUB}}("\e", "\cV");
    clean_resume_sub();			# Workaround for a perl bug
}

# The following is only used to work around a segfaulting perl bug
sub clean_resume_sub {
    my ($bufid) = @_;
    delete $resume_sub{$bufid} 		if defined($bufid)
					&& !defined($shells{$bufid});
}

# See if a shell is dead
sub dead {
    my ($bufid) = @_;
    return !defined($bufid) || !defined($shells{$bufid}{PTY_HANDLE});
}

# Send some characters to a shell
sub send_chars {
    my ($bufid, $chars) = @_;
    return undef		unless exists $shells{$bufid};
    my $pty = $shells{$bufid}{PTY_HANDLE};
    print $pty $chars;
    1;
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

END {
    # Close handles associated with all open shells and slave xterms
    # and attempt to kill them gracefully using SIGTERM.
    foreach my $bufid (keys %shells) {
	if (defined $shells{$bufid}{PTY_HANDLE}) {
	    close $shells{$bufid}{PTY_HANDLE};
	}
	if (defined $shells{$bufid}{PID}) {
	    kill 15, $shells{$bufid}{PID};	# SIGTERM
	}
	if (defined $shells{$bufid}{XTERM_HANDLE}) {
	    close $shells{$bufid}{XTERM_HANDLE};
	}
	if (defined $shells{$bufid}{XTERM_PID}) {
	    kill 15, $shells{$bufid}{XTERM_PID}; # SIGTERM
	}
    }

    # See if any shells or slave are still alive; if so kill them
    # off with SIGKILL after waiting one second.
    my $slept = 0;
    foreach my $bufid (keys %shells) {
	if (defined $shells{$bufid}{PID} && kill(0, $shells{$bufid}{PID})) {
	    if (!$slept) {
		sleep 1;
		$slept = 1;
	    }
	    kill 9, $shells{$bufid}{PID};	# SIGKILL
	}
	if (defined $shells{$bufid}{XTERM_PID} 
	    && kill(0, $shells{$bufid}{XTERM_PID}))
	{
	    if (!$slept) {
		sleep 1;
		$slept = 1;
	    }
	    kill 9, $shells{$bufid}{XTERM_PID}; # SIGKILL
	}
    }
}

1;
__DATA__

=head1 NAME

start-shell, resume-shell - start/resume a shell in a vile window

=head1 SYNOPSIS

In .vilerc:

    perl "use shell"

In [x]vile:

    :start-shell

    :resume-shell

=head1 DESCRIPTION

The B<shell.pm> module provides facilities for running a shell in a
vile window.

A shell is started via

    :start-shell

By default, the escape character (ESC) is used to escape the shell and
the Control-V character is used to escape characters to send to the
shell (like either ESC or Control-V).  Since it can be quite annoying
to have to explicitly escape all of the ESC characters that you wish
to send to a shell or application running under the shell, a mechanism
is provided for temporarily allowing ESC to be sent to the shell
unfettered.  The sequence ^V-e (Control-V followed by 'e') will
toggle whether the ESC is sent to the shell or is used to escape the
shell.  (A message will be printed to the command line to indicate
which is the case.)

Once the shell has been escaped from, all of the usual editor
commands may be used to browse/edit the shell window text, move
to other windows, etc.  When you wish to resume interacting with
the shell, simply place the cursor anywhere in the shell window
and use

    :resume-shell

to resume interacting with the shell.  The cursor will be repositioned
at the correct place to continue the interaction.

=head1 GDB INTERACTIONS

The B<shell.pm> terminal emulation facilities have been made gdb
aware, but you need to invoke gdb with the '--fullname' option to
make use of these facilities.  E.g, you might invoke gdb as follows:

    gdb --fullname myprogram

When gdb is invoked in such a fashion, it will annotate its output to
include the name of the file to open when stopped (e.g, due to
breakpoint or after a single step).  Going "up" or "down" the stack
will also cause an appropriate annotation to be sent.

B<shell.pm> will watch for these annotations and attempt to open and
display the file specified by the annotation.  The line at which
execution has stopped will be highlighted using vile's selection
mechanism.

A gdb.pm module is under construction.

=head1 REQUIREMENTS

You will almost certainly need to be running on some sort of Unix
system for B<shell.pm> to work at all.

You will need the IO::Pty module.  This is available from

    http://www.cpan.org/modules/by-module/IO/IO-Tty-0.04.tar.gz

You may want to check CPAN for a more recent version prior to
downloading this version.

Resizing won't work unless you have sys/ioctl.ph installed.  This
file is created from your system's <sys/ioctl.h> file.  In order to
install sys/ioctl.ph, simply obtain whatever privileges are necessary
for writing to the perl installation directories and then do:

    cd /usr/include
    h2ph -a -l sys/*

If you are on a BSD (or BSD-like) system which requires that the
TIOCSCTTY ioctl() be used to acquire the controlling terminal, you
will need to follow the above instructions to make sure that
sys/ioctl.ph is install.

=head1 BUGS

Terminal emulation is incomplete.

The shell is not immediately notified of a resize when a window is
resized.  (It is notified, but not until the shell sends some output
to be placed in a buffer.)

The shell is not killed when the buffer is destroyed.  (It is killed
when vile exits though.)

Escaping the shell is clumsy.

Selecting with the mouse in xvile causes 'M' to be sent to the
shell.

It's possible to change active windows with the mouse in xvile,
but shell.pm is not made aware of the fact that a different window
has been made active.

This is a work in progress.  Expect many things to change.

=head1 AUTHOR

Kevin Buettner

=cut
