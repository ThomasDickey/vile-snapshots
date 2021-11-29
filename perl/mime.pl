# $Id: mime.pl,v 1.11 2020/01/17 23:34:15 tom Exp $
# (see dir.doc)
require 'plugins.pl';

use strict;

package mime;

our $RPM_Provides = 'mime.pl perl(mime.pl)';

our @mc;
our @mt;

our %type;
our %flag;
our %prog;

our %gprog;
our %gflag;
our %gtype;
our %gdesc;

our %timestamp;

sub mime {
    my $work = Vile::working(0);
    my ($file) = @_;
    my ( $cwd, $ext, $nam, $val, $wildcard, $type );

    chop( $cwd = `pwd` );

    $ENV{MAILCAP}   .= ":$ENV{HOME}/.vile/mailcap";
    $ENV{MIMETYPES} .= ":$ENV{HOME}/.vile/mime.types";

    readmc( *prog, *flag, split( ":", $ENV{MAILCAP} ) );
    readmt( *type, *desc, split( ":", $ENV{MIMETYPES} ) );

    $file = "$cwd/$file" if ( $file !~ m!^/! );
    $ext = "" if ( !( ( $ext = $file ) =~ s/.*\.//g ) );

    if ( !-e $file ) {
        print "[No such file or directory]";
        Vile::working($work);
        return;
    }

    if ( defined $type{$ext} ) {
        ( $wildcard = $type{$ext} ) =~ s:/.*$:/\*:;
        foreach $type ( $type{$ext}, $wildcard, "*/*", "*" ) {
            if ( defined $flag{$type} ) {
                ( $nam, $val ) = split( ":", $flag{$type} );
                if ( $nam eq "x-vile-flags=plugin" ) {
                    eval "plugins::$val(\"$file\")" || print "$file: $@\n";
                    Vile::working($work);
                    return;
                }
            }
            if ( defined $prog{$type} ) {
                if ( fork == 0 ) {
                    if ( !exec sprintf( $prog{$type}, $file ) ) {
                        print "[Failed executing \"$prog{$type}\"]";
                        Vile::update();
                    }
                    exit;
                }
                Vile::working($work);
                return;
            }
        }
    }

    $Vile::current_buffer = new Vile::Buffer $file;
    Vile::working($work);
}

sub readmc {
    ( *gprog, *gflag, @mc ) = @_;
    my ( $line, $type, $prog, $flag, $mc );
    foreach $mc (@mc) {
        next if ( ( !-e $mc ) || ( $timestamp{$mc} > ( stat($mc) )[9] ) );
        $timestamp{$mc} = time;
        open( MC, $mc ) || next;
        while (<MC>) {
            chop;
            next if (/^\s*(#|$)/);
            $line .= $_;
            if ( $line !~ s/\\$// ) {
                ( $type, $prog, $flag ) = split( "\;", $line );
                $type =~ s/(^\s*|\s*$)//g;
                $prog =~ s/(^\s*|\s*$)//g;
                $flag =~ s/(^\s*|\s*$)//g;
                $gprog{$type} = $prog if ( defined $prog && length($prog) );
                $gflag{$type} = $flag if ( defined $flag && length($flag) );
                undef $line;
            }
        }
        close(MC);
    }
}

sub readmt {
    ( *gtype, *gdesc, @mt ) = @_;
    my ( $line, $mt, $nam, $val, $type, $exts, $desc, $ext );
    foreach $mt (@mt) {
        open( MT, $mt ) || next;
        next if ( ( !-e $mt ) || ( $timestamp{$mt} > ( stat($mt) )[9] ) );
        $timestamp{$mt} = time;
        while (<MT>) {
            chop;
            next if (/^\s*(#|$)/);
            $line .= $_;
            if ( $line !~ s/\\$// ) {
                while ( $line =~ s/(^|\b)(type|desc|exts)=([^"]\S+|"[^"]*")// )
                {
                    $nam = $2;
                    $val = $3;
                    $val =~ s/(^"|"$)//g;
                    $val = "\"$val\"";
                    eval "\$$nam=$val";
                }
                ( $type, $exts ) = split( /\s+/, $line, 2 )
                  if ( $line =~ /\S/ );
                foreach $ext ( split( /[\,\s]+/, $exts ) ) {
                    $ext =~ s/^\.//;
                    if ( defined $ext && length($ext) ) {
                        $gtype{$ext} = $type;
                        $gdesc{$ext} = $desc;
                    }
                }
                undef $line;
            }
        }
        close(MC);
    }
}

1;

__END__

=head1 NAME

mime - MIME library used within vile via perl interface

=head1 SYNOPSIS

    require "mime.pl"

    package mime;

    readmc *prog, *flag, @mailcaps;
    readmt *type, *desc, @mimetypes;
    mime "filename";

=head1 DESCRIPTION

This  library  is  written  with a view to provide file type
recognition  capability  to perl extensions written for vile
and  to  open  the  appropriate application for viewing that
file.  It  consists of three functions currently.

The  "readmc"  function  reads the mailcap files provided as
a  list  with  the arguments, parses the files in that order
and  puts  the results in the two global hashes keyed on the
mime  type,  also passed with the arguments. The "prog" hash
contains  the  application to invoke to view that file while
the  "flag"  hash  contains  any  flags  provided  for  that
mime-type  in  the  mailcap  file.  The  flags  are  used to
recognize  whether  a plugin should be used to view the file
or   an   external  application  should  be  used,  as  with
netscape.

The  "readmt"  function  reads the mime.types files provided
as  a  list  with  the  arguments,  parses the files in that
order  and  puts  the results in the two global hashes keyed
on  the  extension. The "type" hash contains which mime type
does   that   extension   belong  to  and  the  "desc"  hash
contains   the  description  of  the  file  type  with  that
extension.

Both  the  above  functions  also  keep  a timestamp of last
time   each   file  was  read  and  re-reads  that  file  on
re-invocation,  only  if  it  has  changed  since last read.
This  may  not  be required, but I left this in anyway since
it  might come  in  handy in certain situations since "mime"
function  calls  the above routines with every invocation.

Finally,  the  "mime"  function  takes  path to a file as an
argument.  If  the  absolute  path is not provided, the path
is  assumed  to be relative to the current directory (duh!).
It  uses  the MAILCAP and MIMETYPES environment variables to
figure  out  the appropriate plugin or application to invoke
(in  that  order)  depending on the file extension. The mime
settings   in   files  provided  in  MAILCAP  and  MIMETYPES
environment    variables    can   be   overridden   in   the
~/.vile/mailcap  and  ~/.vile/mime.types  files. The default
action  currently  is to simply open the file in vile (maybe
a  more  correct  way  would  be  to  have  a new plugin for
displaying  the  file  in  vile attributed to the "*/*" mime
type,  but  oh well...).

In case, the library decides that a plugin should be used to
view the file,  it simply calls the function provided as the
plugin  with  the complete path to the file as the argument.
The function should belong to the plugins package.

A   sample   plugin   script   for  .gz  files  is  provided
along with this.  Sample
	~/.vile/mailcap
and
	~/.vile/mime.types
files  are  also  provided  along with this script.

=head1  CAVEATS

The  mime  parsing  is  not  fully  (or even partially?) RFC
compliant.  I  have  written  this  library  by simply going
through  all  the mailcap and mime.types files I found on my
system.  It  works  most satisfactorily for me and it should
for  anyone  else, but I cannot guarantee anything.

The  library  recognizes  wildcards  in  mime  types to some
extent.   Which   means  if  it  cannot  find  a  plugin  or
application  to  use  for  "image/gif",  it  will  look  for
plugin  or  application  for  "image/*",  then for "*/*" and
finally  for  "*" mime type. But it will not honor wildcards
of  the  format  "ima*/gif" or  "image/g*" (and I don't even
know  if  the RFC allows such wildcards in mime types).

The  library  currently  does  not  support  the mailcap and
mime.types  files  in  the system-wide vile directory (where
the  help  file resides).


=head1 ENVIRONMENT

MAILCAP    contains the list of mailcap files to use in the
           proper order

MIMETYPES  contains the list of mime type files to use in the
           proper order

=head1 CREDITS

J. Chris Coppick, once wrote:
Having a Perl interpreter in vile is very slick. Kudos to everyone who made it
happen.

Kuntal Daftary writes:
Amen!

=head1 AUTHOR

Kuntal Daftary (daftary@cisco.com), 1998

=cut
