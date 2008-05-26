# perl extension module for vile
#
# Below are a small collection of utilities which I wrote to test
# out the Vile::Window facilities after I wrote them.  However, these
# do seem to be genuinely useful at times, so I collected them together
# into a single file.
#
# Also, they provide good examples of how to use the Vile::Window
# API.
#
#	- Kevin, December 1999.
#

package winops;

require Vile::Exporter;
@ISA = 'Vile::Exporter';
%REGISTRY = (
    'equalize-window-height' => [
	\&equalize_window_height,
	'make all window the same approximate size',
    ],
    'maximize-window-height' => [
	\&maximize_window_height,
	'make current window as large as possible without deleting other' .
	' windows',
    ],
    'swap-above' => [
	\&swap_above,
	'swap contents of current window and window above',
    ],
    'swap-below' => [
	\&swap_below,
	'swap contents of current window and window below',
    ],
    'new-window' => [
	\&new_window,
	'create a new window and make it current',
    ],
    'delete-windows-below' => [
	\&delete_windows_below,
	'delete windows below current window',
    ],
    'delete-windows-above' => [
	\&delete_windows_above,
	'delete windows above current window',
    ],
);

# Make all windows of approximately equal height
sub equalize_window_height {
    my $n = Vile::window_count;
    my $total_height = 0;
    my $i;

    # Determine total size that we have to work with
    for ($i = $n - 1; $i >= 0; $i--) {
	$total_height += Vile::window_at($i)->size;
    }

    # Make top windows really small (1 line each).  Don't bother
    # adjusting size of the last window or the penultimate window.
    # (We can't adjust the bottom window anyway.  There's no need
    # to adjust the penultimate window since that's the one we'll
    # start with when we readjust below.)
    for ($i = 0; $i < $n-2; $i++) {
	Vile::window_at($i)->size(1);
    }

    # Now start at the penultimate window and work our way to the
    # top, setting the size of each as we go.

    for ($i = $n-2; $i >= 0; $i--) {
	my $desired_height = int($total_height / $n);
	my $wtop = Vile::window_at($i);
	my $wbot = Vile::window_at($i+1);
	$wtop->size($wtop->size + $wbot->size - $desired_height);
	$total_height -= $desired_height;
	$n--;
    }
}

# Make the current window as big as possible without deleting other
# windows
sub maximize_window_height {
    my $n = Vile::window_count;
    my $curwin = Vile::current_window;
    my $curidx = $curwin->index;

    # Make all windows above the current window one line high
    for ($i=0; $i<$curidx; $i++) {
	Vile::window_at($i)->size(1);
    }

    # Make all windows below the current window one line high
    for ($i=$n-2; $i >=$curidx; $i--) {
	my $wtop = Vile::window_at($i);
	my $wbot = Vile::window_at($i+1);
	$wtop->size($wtop->size + $wbot->size - 1);
    }
}

# Helper subroutine for swapping buffers between windows
sub swap_windows {
    my ($win1, $win2) = @_;

    # Fetch the current position (dot) and the top lines for the buffers
    my @dot1 = $win1->dot;
    my @dot2 = $win2->dot;
    my $top1  = $win1->topline;
    my $top2  = $win2->topline;

    # Swap them
    my $b_tmp = $win1->buffer;
    $win1->buffer($win2->buffer);
    $win2->buffer($b_tmp);

    # Reset dot and the top lines for the swapped buffers
    $win1->dot(@dot2);
    $win2->dot(@dot1);
    $win1->topline($top2);
    $win2->topline($top1);
}

# Swap buffer associated with current window with window above
sub swap_above {
    my $w_below = Vile::current_window;
    my $w_above = Vile::window_at($w_below->index - 1)
    						or die "No window above";
    swap_windows($w_above, $w_below);
}

# Swap buffer associated with current window with window below
sub swap_below {
    my $w_above = Vile::current_window;
    my $w_below = Vile::window_at($w_above->index + 1)
    						or die "No window below";
    swap_windows($w_above, $w_below);
}

# Create a new window a make it current
sub new_window {
    my $new = Vile::Window->new		or die "Can't create new window";
    $new->current_window;
}

# Delete windows below current window
sub delete_windows_below {
    while (Vile::window_count() - 1 > Vile::current_window->index) {
	Vile::window_at(Vile::window_count() - 1)->delete;
    }
}

# Delete windows above current window
sub delete_windows_above {
    while (Vile::current_window->index > 0) {
	Vile::window_at(0)->delete;
    }
}

1;
