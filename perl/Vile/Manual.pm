package Vile::Manual;

use Pod::Text;
use FileHandle;

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(manual);

sub manual
{
    my $pkg = caller;
    my $path = find_package_file($pkg);
    if (!defined($path)) {
	print STDOUT "Can't locate path to $pkg";
	return;
    }
    my $b = edit Vile::Buffer "<${pkg}.pm>";
    print STDOUT $b;
    print STDOUT scalar($b->dotq('$'));
    print STDOUT $path;
    if (!$b or $b->dotq('$') <= 1) {
	$^W = 1;
	$b = new Vile::Buffer "<${pkg}.pm"	unless $b;

	# This is really ugly.  It would be nice to be able to do
	# something like
	#
	#	pod2text(\*{$pkg::__DATA__}, $b)
	#
	# in place of the 5 lines of code below.  But, after over a
	# day of hacking, I haven't been able to make any part of this
	# work.  I did notice some newer Pod utilities which look
	# promising, but they are not standard with the perl
	# distribution yet.
	my $tmpfile = FileHandle->new_tmpfile	or croak("Can't open temp file");
	pod2text($path, $tmpfile);
	$tmpfile->seek(0,0);
	print $b (<$tmpfile>);
	$tmpfile->close;

	$b->unmark;
    }
    Vile::current_window->buffer($b);
}

sub find_package_file
{
    my ($pkg) = @_;

    $pkg =~ s{::}{/}g;
    $pkg .= '.pm';

    foreach my $prefix (@INC) {
	my $path = "$prefix/$pkg";
	return $path			if -r $path;

    }
    return undef;
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
