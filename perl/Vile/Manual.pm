package Vile::Manual;

use Carp;

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(manual);

sub manual
{
    my $pm = (caller) . '.pm';
   (my $path = $pm) =~ s!::!/!g;
       $path = $INC{$path};

    croak "can't get path for module $pm" unless $path;

    my $b = edit Vile::Buffer "<$pm>";
    if (!$b or $b->dotq('$') <= 1) {
	$b = new Vile::Buffer "<$pm>" unless $b;

	local *P;
	my $pid = open P, '-|';

	croak "can't fork ($!)" unless defined $pid;

	unless ($pid)
	{
	    my $filt = Vile::get('libdir-path') . '/vile-manfilt';

	    if ($^O =~ /^(MSWin32|dos)$/ or !-x $filt)
	    {
		require Pod::Text;
		Pod::Text::pod2text($path);
	    }
	    else
	    {
		untie *STDERR;
		open STDERR, '>/dev/null'; # supress pod2man whining
		system "pod2man --lax $path|nroff -man|$filt";
	    }

	    exit;
	}

	print $b join '', <P>;
	close P;

	$b->setregion(1,'$')
	  ->attribute_cntl_a_sequences
	  ->unmark
	  ->dot(1);
    }

    Vile::current_window->buffer($b);
}

1;

__DATA__

=head1 NAME

manual - construct manual page from embedded pod documentation

=head1 SYNOPSIS

In a vile extension module:

    use Vile::Manual;
    %REGISTRY = (
	         ...
		 'vile-command-help' => [ sub {&manual},
		                          'manual page for vile-command'] );

=head1 DESCRIPTION

The Vile::Manual package provides a mechanism for extracting and
displaying as a manual page (in the editor) the pod documentation
embedded along with a vile extension.

=head1 BUGS

Does not do any sort of highlighting.

=head1 NOTES

I'm not convinced that this is the best mechanism for displaying man
pages for the perl extensions.

=head1 AUTHOR

Kevin Buettner

=cut
