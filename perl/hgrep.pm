package hgrep;

use File::Find;
use English;

use Visit;
use Glob2re;
use Vile::Manual;

require Vile::Exporter;
@ISA = 'Vile::Exporter';
%REGISTRY = (hgrep => [ \&hgrep, 'recursive grep' ],
             'hgrep-help' => [ sub {&manual}, 'manual page for hgrep' ]);

# Make &Visit::visit visible in main
*main::visit = \&Visit::visit;

my $hgrep_oldspat = '';
my $hgrep_oldroot = '.';

sub hgrep {

    my ($spat, $root, $fpat) = @_;

    if (!defined($spat)) {
	$spat = Vile::mlreply_no_opts("Pattern to search for? ", $hgrep_oldspat);
	return if !defined($spat);
    }
    $hgrep_oldspat = $spat;

    while (!defined($root)) {
	$root = Vile::mlreply_dir("Directory to search in? ", $hgrep_oldroot);
	return if !defined($root);
    }
    $hgrep_oldroot = $root;

    while (!defined($fpat)) {
	$fpat = Vile::mlreply_no_opts("File name pattern? ", "*");
	return if !defined($fpat);
    }

    my $resbuf = new Vile::Buffer "hgrep $spat $root $fpat";

    print $resbuf "Results of searching for /$spat/ in $root with filter $fpat...\n---------------\n";

    $fpat = glob2re($fpat);

    my $code = '
    find(
	sub {
	    if (-f && -T && $_ ne "tags" && /'
    .                  $fpat
    .                      '/) {
		my $fname = $File::Find::name;
		if (open SFILE, "<$_") {
		    local($_);
		    while (<SFILE>) {
			if (/'
    .                        $spat
    .                            '/) {
			    chomp;
			    s/^(.*?)('
    .			        $spat
    .                                ')/$1 . "\x01"
                                           . length($2)
                                           . q#BC4Hperl "visit(\'#
					   . $fname
					   . qq(\',)
					   . $INPUT_LINE_NUMBER
					   . q(,)
					   . length($1)
					   . qq#)"\0:#
					   . $2/e;
			    print $resbuf "$fname\[$INPUT_LINE_NUMBER]: $_\n";
			}
		    }
		    close SFILE;
		}
		else {
		    print $resbuf "Warning: Can\'t open $fname\n";
		    #print "Warning: Can\'t open $fname\n");
		}
	    }
	},
	$root);
    ';

    eval $code;
    if (defined($@) && $@) {
	print "$@";
    }
    else {
	print $resbuf "\n\n";
	$Vile::current_buffer = $resbuf;
	# Don't let syntax coloring wipe out the hypertext links
	$resbuf->set(autocolor => 0);
	# Turn on highlighting (and hypertext links), clear modified
	# status, and position the cursor on the first match
	$resbuf->setregion(1,'$')
	       ->attribute_cntl_a_sequences
	       ->unmark
	       ->dot(3);
	# Set up error finder
	my $bufname = $resbuf->buffername;
	$bufname =~ s/\\/\\\\/g;
	Vile::command("error-buffer " . $bufname);
    }
}

1;

__DATA__

=head1 NAME

hgrep - recursive grep with hypertext links

=head1 SYNOPSIS

In .vilerc:

    perl "use hgrep"

In [x]vile:

    :hgrep

and then provide responses to the ensuing prompts.

=head1 DESCRIPTION

The B<hgrep> module is used with [x]vile's perl interface to provide a
nifty recursive grep facility complete with hypertext links.

One of the things which makes it so nifty is that it doesn't search
binary files.  (If you want it to, just search for and remove the -T
in hgrep.pm.) So it's perfectly safe to just search * in most
cases rather than using a restrictive filter like *.[hc]

Assuming that the hgrep extension has been loaded in your .vilerc
file (see above), it may be invoked from the editor via

    :hgrep

You will then need to answer questions at three prompts.  The
first is

    Pattern to search for?

At the prompt, you should enter a regular expression.  Some of
the more useful patterns are

=over 16

=item string

Search for string.

=item \bword\b

Search for word.  Note that \b indicates a word boundary.

=item (?i)pattern

Search for the specified pattern, but ignore case.

=back

See the perlre man page for more information about Perl regular
expressions.

The second prompt is

    Directory to search in?

By default, either . (the current working directory) or the last
directory you entered will be displayed at this prompt.  This
is the directory hierarchy to be searched.

The third (and final prompt) is

    File name pattern?

This should be a glob expression which indicates the names to search.
By default it is * which means match all files.  (Only text files as
indicated by Perl's -T operator are searched however).

Once all three questions have been answered, B<hgrep> will create a
new buffer in which lines from files where matches occur are listed.
The highlighted text matching the regular expression is a hypertext
link which may be used to visit the places in the files corresponding
to the matched text.  These hypertext commands may be activated by
double clicking in xvile or using the "execute-hypertext-command"
command from vile.  (See the Hypertext section of vile.hlp for some
convenient key bindings.)

The error finder ^X^X (find-next-error) may also be used to quickly
go to the next match in the list without returning to the B<hgrep>
buffer.

=head1 AUTHOR

Kevin Buettner

=cut
