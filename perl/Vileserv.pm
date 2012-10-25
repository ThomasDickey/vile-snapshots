#   Vileserv.pm (version 1.4) - Provides network command-server capability
#                               for Vile.
#
#   Copyright (C) 1998  J. Chris Coppick
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

package Vileserv;

use Socket;
use POSIX 'EINTR';
use Vile;
use Vile::Manual;
require Vile::Exporter;

@ISA = 'Vile::Exporter';
%REGISTRY = (
    'startserv'  => [\&start, 'start edit server' ],
    'stopserv' => [\&stop, 'stop edit server' ],
    'vileserv-writehook' => [\&writehook, 'support for "vileget -w ..."'],
    'vileserv-help' => [sub {&manual}, 'manual page for Vileserv.pm' ]
);

# path to PERL binary (needed to run server daemon)
$__perl = Vile::get('%vileserv-perl-path');
if ($__perl eq 'ERROR' || $__perl eq '') {
   if (-e '/usr/bin/perl') {
      $__perl = '/usr/bin/perl';
   } elsif (-e '/usr/local/bin/perl') {
      $__perl = '/usr/local/bin/perl';
   } else {
      die("can't find perl binary - try setting the %vileserv-perl-path \
	  variable in your .vilerc file");
   }
} elsif (! -e $__perl) {
   die("perl binary $__perl does not exist");
}

# path to local socket
$__vilesock = Vile::get('%vileserv-socket-path');
if ($__vilesock eq 'ERROR' || $__vilesock eq '') {
   if (defined($ENV{'VILESOCK'})) {
      $__vilesock = $ENV{'VILESOCK'};
   } else {
      $__vilesock = "$ENV{'HOME'}/.vilesock";
   }
}

# out-of-band delimiter :-)
$__esc = '+++';

# Setup auto start/stop of vileserv...

END {
   &stop;
}

sub import {
   Vile::Exporter::import(@_);
   start();
}

*unimport = *stop;

# Initiates the child "server" process (vileserv) and sets up the appropriate
# filehandle callbacks.
#
sub start {

   # make calling start twice a silent no-op
   return if defined $__pid;

   if (-e $__vilesock) {

      if (Vile::get('%vileserv-doh!') ne 'ERROR') {
	 die "Socket $__vilesock already exists. Vileserv already running?\n";
	 return;
      }

      socket(TESTSOCK, PF_UNIX, SOCK_STREAM, 0) || die "$0: socket: $!";

      if (connect(TESTSOCK, sockaddr_un($__vilesock))) {

	 close TESTSOCK;

	 # Assume there's a vileserv alive and well already...
	 print "Vileserv seems to be running already...";
	 return;
      }

      # Assume a Vileserv died, delete the sock and get on with it...
      close TESTSOCK;
      unlink($__vilesock);
   }

   die "pipe: $!" unless pipe DAEMON, EDITOR;
   die "fork: $!" unless defined ($__pid = fork);

   unless ($__pid) {
      # Child - close unused DAEMON handle, dup EDITOR as STDOUT, and
      #         exec server daemon...

      close DAEMON;
      untie *STDOUT;
      open(STDOUT, ">&EDITOR") || die "can't dup EDITOR to STDOUT";
      $| = 1;
      &vileserv;
   }

   # Parent - watch DAEMON handle...
   close EDITOR;

   Vile::watchfd (fileno(DAEMON), 'except', \&stop);
   Vile::watchfd (fileno(DAEMON), 'read', \&readfiles);

   hookwrites();

   print "Started vileserv...\n";

}

# Stops the server process.
#
sub stop {
   return unless defined $__pid;
   print "Shutting down vileserv...\n";

   Vile::unwatchfd fileno(DAEMON);
   close DAEMON;
   unlink $__vilesock;

   kill 15, $__pid;
   1 while (waitpid($__pid, 0) == -1 && $! == EINTR);
   undef $__pid;
}

# Reads data from the server process as necessary and performs the
# required editor commands.  (Shouldn't be called readfiles() anymore...
# it reads other stuff too.)
#
sub readfiles {

   my @files;
   my $fileName;

   do {
      $fileName = <DAEMON>;
      chomp $fileName;
      if ($fileName ne "") {
	 push @files,$fileName;
      }
   } until ($fileName eq "");

   while ($fileName = pop(@files)) {

      if ($fileName =~ /^\Q$__esc\E (.*)/) {

         my $path = $1;
         if ($path =~ /\S/) {
	    Vile::command "cd $path";
	 }

      } elsif ($fileName =~ /^\Q$__esc\Edo (.*)/) {

         my $cmd = $1;
         my $security = Vile::get('%vileserv-accept-commands');
	 if ($security ne 'ERROR' && $security ne 'false') {
	    Vile::command $cmd;
	 } else {
	    print "Rejected remote command \"$cmd\"\n";
	 }

      } else {

	 $Vile::current_buffer = new Vile::Buffer "$fileName";
      }
   }

   Vile::update;
}

# Setup vile write-hook so we can tell the vileserv process whenever
# vile writes a buffer.
#
sub hookwrites {

   my $do_this = Vile::get('%vileserv-no-writehook');
   return 0 if ($do_this ne 'ERROR');

   if (defined($INC{'CaptHook.pm'})) {

      # No worries, the Captain has the helm...

      Vile::command 'write-hook vileserv-writehook vileserv-writehook 1000';

   } else {

      my $oldhook = Vile::get('$write-hook');

      if ($oldhook ne 'ERROR' && $oldhook ne '' &&
          $oldhook ne 'vileserv-writehook') {

	 # Somebody got to the hook first...

	 print "Vileserv: \$write-hook already used. (You should install CaptHook.)";
	 return 0;
      }

      Vile::command 'setv $write-hook vileserv-writehook';
   }

   return 0;
}

# Tell the vileserv process which file got written.
#
sub writehook {

   my $file = $Vile::current_buffer->filename();

   socket(HSOCK, PF_UNIX, SOCK_STREAM, 0) || die "$0: socket: $!";
   connect(HSOCK, sockaddr_un($__vilesock)) || die "$0: connect: $!";

   select(HSOCK);
   $| = 1;

   print $__esc . "wrote $file\n";
   close HSOCK;
}

# Execs a simple named-socket server that listens for file-load requests,
# then passes the file names to the parent process (using STDOUT).
#
sub vileserv {

   my $vileserv = "exec $__perl -e ";
   $vileserv .= <<EOD;
   '
   use IO::Socket;

   \$SOCKET = "$__vilesock";
   \$esc = "$__esc";
EOD

   $vileserv .= <<'EOD';

   $0 = "vileserv";

   $uaddr = sockaddr_un($SOCKET);
   $proto = getprotobyname("tcp");
   $conn = 0;

   if (-e $SOCKET) {
      die "$0: socket $SOCKET already exists\n";
   }

   $SIG{"TERM"} = "quitter";
   $SIG{"HUP"} = "quitter";

   socket(Server, PF_UNIX, SOCK_STREAM, 0) || die "$0: socket: $!";
   unlink($SOCKET);
   bind(Server, $uaddr) || die "$0: bind: $!";
   listen(Server,SOMAXCONN) || die "$0: listen: $!";
   $| = 1;

   while(1) {
      $files = "";
      $conn++;
      eval("\$client_$conn = new IO::Socket");
      eval("accept(\$client_$conn, Server)") || die "$0: accept: $!";
      do {
	 eval("\$fileName = <\$client_$conn>");
	 chomp $fileName;

	 # vile tells us it wrote a buffer...
	 if ($fileName =~ /^\Q$esc\Ewrote (.*)/) {
	    $fileName = $1;
	    if (defined($waiting{$fileName})) {
	       foreach $savedConn (@{$waiting{$fileName}}) {
		  eval("print \$client_$savedConn \"$fileName\n\"");
		  if (! --$keepAlive{$savedConn}) {
		     delete $keepAlive{$savedConn};
		     eval("close \$client_$savedConn");
		  }
	       }
	       delete $waiting{$fileName};
	    }

	 } else {

	    # a client is giving us filenames for vile to edit
	    if ($fileName =~ /^\Q$esc\Ewaiton (.*)/) {
	       $fileName = $1;
	       unshift @{$waiting{$fileName}}, $conn;
	       $keepAlive{$conn}++;
	    }
	    $files .= "$fileName\n";

	 }
      } until ($fileName eq "");

      if ($files ne "") {
	 print "$files\n";
      }

      if (!defined($keepAlive{$conn})) {
         eval("close \$client_$conn");
      }

   }

   sub quitter {
      unlink $SOCKET;
      exit 0;
   }
   '
EOD

   exec $vileserv || die "can't exec vileserv";

}

1;

__END__

=head1 NAME

Vileserv - Provides file-load server capability for Vile.

=head1 SYNOPSIS

In Vile:

   :perl use Vileserv
   :startserv
   :stopserv

=head1 DESCRIPTION

Vileserv runs a server that listens for requests to load files into an
already running instance of Vile.  The vileget utility can
be used to make such requests from a shell command line.  Vileserv requires
that Vile be compiled with the built-in PERL interpreter.

Vileserv will likely only work with I<Vile> under Unix.

Vileserv does not provide nearly the level of feeping creaturism as
Emacs' gnuserv, but it's a start.

=head1 INSTALLATION

[Note: Vileserv may already be automagically installed for you as
part of your Vile or XVile installation.]

Install Vileserv.pm somewhere in the @INC path.  Depending on your
Vile installation, I</usr/local/share/vile/perl> might be a good place.

To start Vileserv when you start Vile, simply add the following to your
I<.vilerc> file:

   ; Import and start Vileserv (adds :startserv and :stopserv commands)
   perl "use Vileserv"

=head1 CUSTOMIZATION

Several variables settings can be used to modify Vileserv's default
behaviors.  For best results, any of these variables that you choose
to use should be set in your I<.vilerc> file B<before> Vileserv is
imported and started.

Vileserv looks for a perl binary in I</usr/bin/perl> and
I</usr/local/bin/perl> respectively.  You can override this
in your I<.vilerc> file using the B<%vileserv-perl-path> variable:

   setv %vileserv-perl-path /opt/local/bin/perl

The default socket file used is I<$HOME/.vilesock>.  This can be overridden
by setting the environment variable B<VILESOCK>.  You can also set it
explicitly in your I<.vilerc> file by using the variable
B<%vileserv-socket-path>.  However, using the environment variable is
recommended, since overriding it will stop the vileget program from starting
a new Vile with a new socket path on demand.

The Vileserv protocol (if you can call it a protocol) allows arbitrary
Vile commands to be executed.  This functionality is disabled by default,
but can enabled by adding the following to your I<.vilerc> file:

   setv %vileserv-accept-commands true

In order to support <B>vileget>'s I<-w> option, Vileserv will use
Vile's $write-hook variable.  (Use B<perldoc vileget> for details
about the I<-w> option.)  Vileserv refrains from using the
$write-hook variable if it is already being used, or Vileserv takes advantage
of the CaptHook package if it is loaded.  You can disabled this
behavior entirely by adding the following to your I<.vilerc> file
before you load Vileserv:

   setv %vileserv-no-writehook

If the Vileserv socket file already exists, Vileserv will attempt to
determine if another Vileserv is active or not.  If not, the old socket
file is removed and the Vileserv startup proceeds normally.
If another Vileserv seems to be active, the new Vileserv will slink away
fairly quietly.  If you prefer the old behavior in which Vileserv dies a
horrible death and brings your entire Vile startup to a grinding halt,
you can add the following to your I<.vilerc> file:

   setv %vileserv-doh! true


=head1 BUGS

If the server is started, then stopped, then started again, you may see
the B<warning> "Attempt to free unreferenced scalar..."  This is just
a warning.  The server startup probably succeeded.

=head1 SEE ALSO

vileget(1), vile(1)

=head1 CREDITS

Having a PERL interpreter in Vile is very slick.  Kudos to everyone
who made it happen.  :-)

=head1 AUTHOR

S<J. Chris Coppick, 1998 (last updated: Sept 29, 2001>

=cut
