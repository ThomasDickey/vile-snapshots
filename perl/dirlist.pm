package dirlist;
use File::Find;
use FileHandle;
use English;
use Glob2re;
use Vile::Manual;
require Vile::Exporter;
@ISA = 'Vile::Exporter';
%REGISTRY = (dirlist => [ \&dirlist, 'flat hierarchical directory listing' ],
             'dirlist-help' => [ sub {&manual}, 'manual page for dirlist' ]);

my $dirlist_oldroot = '.';

sub dirlist {

    my ($root, $fpat) = @_;
    my $curbuf = $Vile::current_buffer;

    while (!defined($root)) {
	$root = Vile::mlreply_dir("Directory to search in? ", $dirlist_oldroot);
	return if !defined($root);
    }
    $dirlist_oldroot = $root;

    while (!defined($fpat)) {
	$fpat = Vile::mlreply_no_opts("File name pattern? ", "*");
	return if !defined($fpat);
    }

    my $resbuf = $curbuf->edit("dirlist $root $fpat");

    $fpat = glob2re($fpat);

    my @fnames = ();
    my $code = '
    find(
	sub {
	    if (/'
    .            $fpat
    .                '/) {
		push @fnames, $File::Find::name;
	    }
	},
	$root);
    ';

    eval $code;
    if (defined($@) && $@) {
	print "$@";
    }
    else {
	my $fname;
	my $halfyearago = time - 60*60*24*182;
	@fnames = sort @fnames;
	foreach $fname (@fnames) {
	    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
		  $atime,$mtime,$ctime,$blksize,$blocks)
		    = lstat $fname;
	    my $date = localtime($mtime);
	    if ($halfyearago >= $mtime) {
		$date =~ s/\S+\s+(\S+\s+\S+\s+)\S+(\s+\S+)/$1$2/;
	    }
	    else {
		$date =~ s/\S+\s+(\S+\s+\S+\s+\S+):\d\d \S+/$1/;
	    }
	    my $symlinkinfo = '';
	    if ($^O ne 'MSWin32' &&  -l _) {
		$symlinkinfo = ' -> ' . readlink $fname;
	    }
	    print $resbuf sprintf("%9d %12s %s%s\n",
				  $size, $date, $fname, $symlinkinfo);
	}
	print $resbuf "\n\n";
	$curbuf->current_buffer($resbuf)
	       ->unmark;
    }
}

1;

__DATA__

=head1 NAME

dirlist - list a directory hierarchy

=head1 SYNOPSIS

In .vilerc:

    perl "use dirlist"

In [x]vile:

    :dirlist

and then provide responses at the ensuing prompts.

=head1 DESCRIPTION

The B<dirlist> program will load a buffer with the names of files
matching a certain pattern in and below a specified directory.  It
provides no hypertext links, nor any kind of fancy coloring.  All it
provides is a list of pathnames relative to the starting point along
with certain information that the author finds useful, namely the
file's size, date, and symlink pointer (if there is one).

You can open a file simply by placing your cursor at the beginning of
one of these pathnames and then use ^X-e (edit-file) to do the actual
opening.

=head1 EXAMPLE

This example assumes that you've either placed

    perl "use dirlist"

in your .vilerc or simply

    use dirlist;

in your vileinit.pl file.

From either vile or xvile, do

    :dirlist

The editor will repond with the following prompt:

    Directory to search in? .

As usual, the '.' indicates the current directory and is the default
response.  The next prompt is:

    File name pattern? *

The '*' indicates that all filenames should be matched and is also
the default response.

If both default responses are chosen while in an xvile build
directory, you might see something like the following (abbreviated)
list populate a buffer named I<dirlist-.-*>.

     2048 Dec 29 23:28 .
    54336 Dec 29 21:15 ./api.o
    70288 Dec 29 21:14 ./basic.o
 ...
     2048 Dec 29 21:17 ./filters
    36500 Dec 29 21:16 ./filters/ada-filt.o
    38816 Dec 29 21:16 ./filters/awk-filt.o
 ...
   201740 Dec 29 21:14 ./x11.o
  2868484 Dec 29 23:28 ./xvile

=head1 AUTHOR

Kevin Buettner

=cut
