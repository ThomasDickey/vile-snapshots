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

# path to PERL binary (needed to run server daemon)
$__perl = '/usr/bin/perl';

# path to local socket
$__vilesock = "$ENV{'HOME'}/.vilesock";

# out-of-band delimiter :-)
$__esc = '+++';

END {
   &stop;
}

# Initiates the child "server" process and sets up the appropriate
# filehandle callbacks.
#
sub start {
   # make calling start twice a silent no-op
   return if defined $__pid;

   if (-e $__vilesock) {
      print "Socket $__vilesock already exists. Vileserv already running?\n";
      return;
   }

   die "pipe: $!" unless pipe DAEMON, EDITOR;
   die "fork: $!" unless defined ($__pid = fork);

   unless ($__pid) {
      # Child - close unused DAEMON handle, dup EDITOR as STDOUT, and
      #         exec server daemon...

      close DAEMON;
      open(STDOUT, ">&EDITOR") || die "can't dup EDITOR to STDOUT";
      $| = 1;
      &vileserv;
   }

   # Parent - watch DAEMON handle...
   close EDITOR;

   Vile::watchfd (fileno(DAEMON), 'except', \&stop);
   Vile::watchfd (fileno(DAEMON), 'read', \&readfiles);

   # Can't seem to actually do this yet...
   #&hookwrites;

   print "Started vileserv...\n";
}

# Stops the server process.
#
sub stop {
   return unless defined $__pid;
   print "shutting down vileserv...\n";

   Vile::unwatchfd fileno(DAEMON);
   close DAEMON;
   unlink $__vilesock;

   kill 15, $__pid;
   1 while (waitpid($__pid, 0) == -1 && $! == EINTR);
   undef $__pid;
}

# Reads data from the server process as necessary and performs the
# required editor commands.
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

      } else {

	 $Vile::current_buffer = new Vile::Buffer "$fileName";
      }
   }

   Vile::update;
}

# Setup vile write-hook so we can tell the vileserv process whenever
# vile writes a buffer.
#
#sub hookwrites {
#
# It would be nice to be able to do this...
#
#}

# Tell the vileserv process which file got written.
#
sub writehook {

   my ($file) = @_;

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

Vileserv - Provides file-load server capability for xvile.

=head1 SYNOPSIS

In xvile:

   :perl require Vileserv
   :perl Vileserv::start
   :perl Vileserv::stop

=head1 DESCRIPTION

Vileserv runs a server that listens for requests to load files into an
already running instance of xvile.  The vileget utility can
be used to make such requests from a shell command line.  Vileserv requires
that xvile be compiled with the built-in PERL interpreter.

Vileserv only works with I<xvile> under Unix.

Vileserv does not provide nearly the level of feeping creaturism as
Emacs' gnuserv, but it's a start.

=head1 INSTALLATION

Install Vileserv.pm somewhere in the @INC path.  Depending on your
vile installation, I</usr/local/share/vile/perl> might be a good place.

The B<$__perl> variable near the top of the Vileserv.pm file might need
to be tweaked if your PERL is not I</usr/bin/perl>.

The default socket file used is I<$HOME/.vilesock>.  If you need to change
that, you'll have to edit both Vileserv.pm and vileget.

The following commands are useful in the I<.vilerc> file for setting
things up:

	; Add ':startserv' vile command for starting Vileserv
	;
	perl "Vile::register 'startserv', 'Vileserv::start', \
	   'Start Edit Server', 'Vileserv.pm'"

	; Add ':stopserv' vile command for stopping Vileserv
	;
	perl "Vile::register 'stopserv', 'Vileserv::stop', \
	   'Stop Edit Server', 'Vileserv.pm'"

	; Start Vileserv now...
	;
	startserv

In order to use the B<-w> option of I<vileget>, you need to create a
writehook procedure something like this:

	store-procedure wh
	   perl &cat &cat "Vileserv::writehook '" $cfilname "'"
	~endm
	set write-hook wh

If you have vile-8.2 (patched) or greater, then the vileserv process
should be stopped when xvile exits.  If not, then you may need to add
an exit procedure that stops the vileserv process.  Something like the
following in your I<.vilerc> file should work:

	; Setup exit procedure to stop Vileserv when vile exits
	;
	store-procedure exitproc
   	   stopserv
	~endm
	set-variable $exit-hook exitproc

Even better, upgrade your vile!

=head1 BUGS

If the server is started, then stopped, then started again, you'll see
the B<warning> "Attempt to free unreferenced scalar..."  This is just
a warning.  The server startup probably succeeded.  The warning stems
from the Vile::watchfd commands and it is believed that the problem
lies within the vile PERL package and not within Vileserv.  (Since I
don't understand the problem, it must be someone else's fault...)

=head1 SEE ALSO

vileget(1), xvile(1)

=head1 CREDITS

Having a PERL interpreter in vile is very slick.  Kudos to everyone
who made it happen.  :-)

=head1 AUTHOR

S<J. Chris Coppick, 1998>

=cut
