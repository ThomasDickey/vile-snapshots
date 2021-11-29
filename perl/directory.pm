# $Id: directory.pm,v 1.12 2020/01/17 23:34:52 tom Exp $
# (see dir.doc)

package directory;

use strict;

require Vile::Exporter;

use vars qw(@ISA %REGISTRY);

@ISA = 'Vile::Exporter';
%REGISTRY = ( 'directory' => [ \&dir, 'interactive directory browser' ] );

require 'mime.pl' unless $^O eq 'MSWin32';

sub dir {
    my ($dir) = @_;
    my $width = Vile::get('$pagewid');
    my $len   = 30;
    my ( $cb, $cwd, $sub, @subdirs, @subfils, $i, $last, $spaces );
    my $work = Vile::working(0);

    # do not change the order of the file type
    my @color = (
        0,
        2,    #fifo
        5,    #char
        0,
        0,    #dir
        0,
        6,    #block
        0,
        0,    #reg
        0,
        4,    #symlink
        0,
        3,    #socket
        0,
        0,
        0,
        1,    # exe
        9,    # nonexistent symlink
    );

    $dir = Vile::mlreply_dir( "Directory? ", "." ) if ( !length($dir) );

    if ( $width >= 80 ) {
        $width = $width - 57;
    }
    else {
        $width = 14;
    }

    do { print "[Aborted]"; Vile::working($work); return; }
      if ( !defined $dir );
    $dir = scalar( Vile->get("&path full $dir") );
    chdir $dir || do { print "$dir: $!\n"; Vile::working($work); return; };

    # Keep the vile current directory in sync with the directory the browser is
    # displaying.
    Vile->set( "cwd", $dir );

    opendir( DIR, $dir ) || do {
        print "$dir: $!\n";
        Vile::working($work);
        return;
    };

    foreach $sub ( sort readdir(DIR) ) {
        my ( $mod, $uid, $ind ) = ( undef, undef, 0 );
        do { ( $mod, $uid ) = ( stat($sub) )[ 2, 4 ]; }
          || do { $ind = 17; };
        $uid = substr( ( getpwuid($uid) )[0], 0, 8 ) unless $^O eq 'MSWin32';
        if ( ( $mod & 0xF000 ) == 0x4000 ) {
            $ind = ( -l $sub ? 10 : $ind );
            push @subdirs, [ $sub, $ind, $mod & 0xFFF, $uid ];
        }
        else {
            $ind = sprintf( "%1X", ( $mod & 0xF000 ) >> 12 ) if ( $ind != 17 );
            $ind = ( ( -f $sub && -x $sub ) ? 16 : ( -l $sub ? 10 : $ind ) )
              if ( $ind != 17 );
            push @subfils, [ $sub, $ind, $mod & 0xFFF, $uid, ( -s $sub ) ];
        }
    }
    closedir DIR;
    foreach $cb ( Vile::buffers() ) {
        if ( $cb->buffername eq "<directory-browser>" ) {
            Vile->current_buffer($cb);
            $cb->setregion( 1, '$$' )->attribute("normal")->delete;
            last;
        }
    }
    $cb = $Vile::current_buffer;
    if ( $cb->buffername ne "<directory-browser>" ) {
        $cb = new Vile::Buffer;
        $cb->buffername("<directory-browser>");
        Vile->current_buffer($cb);
        $cb->set( "view",     1 );
        $cb->set( "readonly", 1 );
        $cb->unmark->dot('$$');
    }

    $spaces = ( ( Vile->get("pagewid") - length($dir) ) / 2 );
    print $cb "=" x 79, "\n", " " x $spaces;
    print $cb "$dir \n", "=" x 79, "\n";
    $cb->setregion( 2, $spaces - 1, 2, '$$' )->attribute( "reverse", "bold" );
    $cb->dot('$$');
    print $cb " Directories                  | Files\n";
    print $cb "-" x 30, "+", "-" x 48, "\n";
    $cb->dot('$$');
    print $cb "\n", "=" x 79;

    for (
        $i = 0 ;
        $i <= ( $#subdirs > $#subfils ? $#subdirs : $#subfils ) ;
        ++$i
      )
    {
        $cb->dot( $i + 1 + 5, 0 );
        if ( defined( $subdirs[$i] ) ) {
            my ( $sub, $ind, $mod, $uid, $siz ) = @{ $subdirs[$i] };
            my $substr = substr( $sub, 0, 14 );
            my $str = sprintf( " % 4o %8s %s", $mod, $uid, $substr );
            print $cb $str, " " x ( $len - length($str) ), "| ";
            $cb->setregion( $i + 1 + 5, 0, $i + 1 + 5, length($str) );
            $cb->attribute( "bold", "color", $color[$ind] ) if ( $color[$ind] );
            $cb->setregion( $i + 1 + 5, 15, $i + 1 + 5, 15 + length($substr) );
            $cb->attribute( "hyper", "directory '$sub'" );
        }
        else {
            print $cb " " x $len, "| ";
        }
        $cb->dot( $i + 1 + 5, '$$' );
        if ( defined( $subfils[$i] ) ) {
            my ( $sub, $ind, $mod, $uid, $siz ) = @{ $subfils[$i] };
            my $substr = substr( $sub, 0, $width );
            $siz = substr( $siz, 0, 10 );
            my $str = sprintf( "% 4o %8s %10s %s", $mod, $uid, $siz, $substr );
            print $cb $str, "\n";
            $cb->setregion( $i + 1 + 5, $len + 2, $i + 1 + 5, '$$' );
            $cb->attribute( "bold", "color", $color[$ind] ) if ( $color[$ind] );
            $cb->setregion( $i + 1 + 5, $len + 25, $i + 1 + 5, '$$' );
            $cb->attribute( "hyper", "perl \"mime::mime('$sub')\"" )
              unless $^O eq 'MSWin32';
        }
        else {
            print $cb "\n";
        }
    }
    $cb->dot('$$');
    $last = $cb->dot - 1;
    for ( $i = 1 ; $i < $last ; ++$i ) {
        $cb->setregion( $i, $i );
        my $line = <$cb>;
        $cb->delete if ( $line =~ m!^\s*$!o );
    }
    $cb->unmark()->dot( 1, 0 );
    print "";
    Vile::update();
    $cb->dot( 6, 15 );
    Vile::working($work);
    return;
}

1;

__END__

=head1 NAME

dir - directory browser in vile

=head1 SYNOPSIS

In .vilerc:

    perl "use directory"

In [x]vile:

    :directory
    :dir

=head1 DESCRIPTION

This  is  a directory browser written for use within [x]vile
using   the  perl  interface.  It  can  be  invoked  with  a
directory  name  as  an  optional argument. If the directory
argument   is  provided,  it  lists  the  contents  of  that
directory.  Otherwise  it prompts the user for the directory
name  and  defaults to the current directory.

The  listing  is a two panel output. The left panel contains
the  list  of directories while the right panel contains the
list  of  files.  The  directory  list  provides information
regarding  permissions  and  owner  of each directory listed
while  the  file  listing provides the file size in addition
to  the  permissions and owner for the files listed.

Both  the  lists  are  color  coded  to  provide at-a-glance
information  regarding  the  type  of the file or directory.
The  recognized  types  are  executable  files,  symlinks to
directories   and  files,  block  special  files,  character
special  files,  named pipes, sockets, and broken symlinks.

Additionally,    the    browser   utilized   the   hyperlink
capability  of  vile  to allow traversal to any directory in
the  listing  by  a  single  keypress. To enter a particular
directory  within  a  listing,  simply  press your hyperlink
key  on  the name of the directory or use mouse double-click
in  xvile.  (Personally,  I  find the <RETURN> key to be the
most  convenient  key as the hyperlink key.) This also means
that  you  can  refresh  the  current  directory  listing by
following  the  "." directory hyperlink.

The  file  listing is also hyperlinked such that when a file
name  hyperlink  is  followed,  depending  on  the file type
clicked  on  (file type is decided using the file extension)
and  the  mime  settings,  appropriate  application  will be
invoked  to  view  that  file. Which means you can view .gif
file  in  xv  by  simply clicking on it, and view a .ps file
in  ghostview  by  simply clicking on it, and can view a .fm
file  in  framereader by simply clicking on it, and ... :-)

Lastly,  the  mime library utilized by the directory browser
(and    provided   along with)   for   handling   the   mime
capabilities   is   plugin   aware  (and  you  thought  only
netscape  can  have  plugins).  Which means that for certain
file  types, like  .gz and .tar, you don't need to take help
of  an  external viewer by can use vile itself to view those
files.  And  the perl interface in vile makes it very simple
to  write  new plugins :-)

A   sample   plugin   script   for  .gz  files  is  provided
along with.   For   more   information   on   mime   library
capabilities, please  refer  to  the  pod  documentation  in
mime.pl.

=head1 CAVEATS

When  the  directory  browser  changes directory, it changes
the   current   directory   as   pointed   to  by  the  perl
environment  and  hence  the current directory as vile knows
it  does  not  change. This may cause a little confusion. If
you  want  to change  the  vile current directory along with
the  directory  browser,  please  uncomment  the appropriate
line  in  the dir.pl file.


=head1 SEE ALSO

1. pod documentation on mime.pl

=head1 CREDITS

J. Chris Coppick, once wrote:
Having a Perl interpreter in vile is very slick. Kudos to everyone who made it
happen.

Kuntal Daftary writes:
Amen!

=head1 AUTHOR

Kuntal Daftary (daftary@cisco.com), 1998

=cut
