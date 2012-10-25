#   Help.pm (version 0.1) - Provides Vile commands for viewing specific
#			    sections of the Vile help file.
#
#   Copyright (C) 2001  J. Chris Coppick
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

package Help;

=head1 NAME

Help

=head1 SYNOPSIS

   :perl "use Help"

   :help
   :help-<section heading>

=head1 DESCRIPTION

The Help package parses the Vile help file (normally displayed
within Vile using B<:help> or similar commands) and generates
a whole cornucopia of Vile commands that jump to specific sections of the
help file.  For instance, the command B<:help-color-basics> opens
the Vile help buffer and jumps to the "Color basics" section.

=head1 INSTALLATION

[Note: Help.pm may already be installed for you as part of your
Vile installation.]

Install Help.pm somewhere in perl's @INC path.  Depending on your
Vile installation, I</usr/local/share/vile/perl> might be a good place.

Add B<perl "use Help"> to your I<.vilerc> file.

As a bonus, if you do not like Vile's default behavior of splitting
the current window when displaying the help file, you can set
the variable B<%help-fullscreen> before you load the Help package.
For example:

   setv %help-fullscreen true
   perl "use Help"

If B<%help-fullscreen> is set, then all the new section-related help
commands, and the original B<:h> and B<:help> commands, display help
text in a full-sized window.  The default B<:list-help>, B<:show-help>, and
help-related key bindings retain their original behavior.

=head1 USAGE

=over 4

=item help

=item h

=item list-help

=item show-help

Display the Vile help buffer, at either the beginning
or the last-view point in the help text.  This is the default
behavior for these commands.  The B<h> and B<help> commands
are affected by the B<%help-fullscreen> variable.  The B<list-help>
and B<show-help> commands are not affected in any way by the
Help package.

=item help-<section heading>

Display the Vile help buffer, with the specific section heading positioned
at the top of the window.  [Note: This does not provide a new help
buffer, but rather a view into the default help buffer.  I.e. you can
still scroll to view other help sections.]

Section headings are determined dynamically when the Help package is
loaded, using arbitrary criteria defined by the author based on a
common-sense appraisal of the current formatting convention used
in the Vile help text.  Due to technical considerations, section-heading
text may be modified somewhat to allow for its use as a Vile command
name.  For example, the help section "Sample .vilerc" translates into
the Vile command B<help-sample-vilerc>.  (Note the use of lower-case
letters only and the removal of the '.'.)

Naturally, remembering every possible section heading would be a sad
waste of neuron-firing patterns, but you get to use auto-completion on
the B<help-...> commands, which is really the whole point.  For example:

   :help-<TAB><TAB>

effectively displays a complete "table of contents" for the Vile help text
(notwithstanding the state of the I<popup-choices> mode, of course).

=back

=head1 BUGS

I grow carnivorous plants.  Bugs are not really an issue for me...

=head1 SEE ALSO

vile(1)

=head1 CREDITS

All the Vile documentation contributors...

=head1 AUTHOR

S<J. Chris Coppick, 2001 (last updated: Oct. 10, 2001)>

=cut


use Vile;
use Vile::Manual;
require Vile::Exporter;

use IO::File;

# use warnings;

sub import {
   Vile::Exporter::import(@_);
   _setup();
}

sub _setup {

   my $fullscreen = Vile::get('%help-fullscreen');

   _addHelpCommands();

   if ($fullscreen ne 'ERROR') {

      eval <<EOD;
	 Vile::register \"h\",
			sub {
			   Vile::command \"list-help\";
			   Vile::command \"delete-other-windows\";
			   Vile::update;
			}
EOD

      eval <<EOD;
	 Vile::register \"help\",
			sub {
			   Vile::command \"list-help\";
			   Vile::command \"delete-other-windows\";
			   Vile::update;
			}
EOD

   }

   return 0;

}


sub _addHelpCommands {

   my ($startdir, $helpfile, $cmd, $section, $fh, $lastpos);

   $startdir = Vile::get('$startup-path');
   $helpfile = Vile::get('$helpfile');
   $fh = new IO::File "$startdir/$helpfile", "r";
   if (!defined($fh)) {
      print "Help:  couldn't open $startdir/$helpfile";
      return 0;
   }

   while (defined($_ = <$fh>)) {
      if (/^([[:upper:][:digit:]].*$)/) {
	 $section = $1;
	 next if (/^Copyright/);
	 next if (/^Credits/);
	 $lastpos = $fh->getpos;
	 last if (!defined($_ = <$fh>));
	 if (/^-+[-\s]*$/) {
	    $section =~ s/\s*\(.*\)$//;
	    $cmd = "help-" . $section;
	    $section = quotemeta($section);
	    $cmd =~ tr/ [A-Z]/-[a-z]/;
	    $cmd =~ s/\"//g;
	    $cmd =~ s/\:$//;
	    $cmd =~ s/\.//g;

	    eval <<EOD;
	       Vile::register \"$cmd\",
	                   sub {
				 Vile::command \"help\";
				 Vile::command \"search-forward \'^$section\'\";
				 Vile::command \"position-window t\";
				 Vile::update;
			   }
EOD

	 } else {
	    $fh->setpos($lastpos);
	 }
      }
   }

   undef $fh;
   return 0;
}

1;
