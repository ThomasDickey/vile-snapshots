
#   CaptHook.pm (version 0.1) - Provides useful wrappers for Vile's "hook"
#				variables
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

package CaptHook;

=head1 NAME

CaptHook

=over 5

Allows multiple, prioritized actions to be easily assigned to Vile's
various built-in hooks.

=back

=head1 SYNOPSIS

   :perl "use CaptHook"

   :autocolor-hook <command> <label> <priority>
   :autocolor-unhook <label>
   :buffer-hook <command> <label> <priority>
   :buffer-unhook <label>
   :cd-hook <command> <label> <priority>
   :cd-unhook <label>
   :exit-hook <command> <label> <priority>
   :exit-unhook <label>
   :read-hook <command> <label> <priority>
   :read-unhook <label>
   :write-hook <command> <label> <priority>
   :write-unhook <label>

   :do-autocolor-hooks
   :do-buffer-hooks
   :do-cd-hooks
   :do-exit-hooks
   :do-read-hooks
   :do-write-hooks

   :capthook-help

=head1 DESCRIPTION

Vile comes with the ability to assign certain actions to certain
events.  These actions are called "hooks".  (I use the term "hook"
a little loosely here and there in this manual page, but the intention
at each instance is generally clear, I hope.)  For more information
on Vile's built-in hooks, please see the I<Hooks> section of Vile's
help text (available from within Vile, as B<:help>).

First, let's be clear about what this package, affectionately known
as the "Captain", does B<NOT> allow you to do.  There are, currently,
six supported hooks in Vile:  autocolor, buffer, cd, exit, read, and
write.  The Captain does B<NOT> allow you to extend this set.  You get
those six hooks and that's it (as far as I know).

So what does the Captain do for you?  Normally you can only assign one
command to any one of Vile's hooks.  If, for example, you wanted to
have B<lots> of things happen whenever you changed buffers (i.e. activated
the buffer-hook), then you'd have to write a special procedure (in your
I<.vilerc> probably) that incorporated all the actions you wanted, and then
you'd assign that procedure to the B<$buffer-hook> variable.  But why
should you have to write a special procedure?  Especially since I went
to all this time and effort to build this package that comes with all
these neat commands and stuff?  The answer is, of course, that you
shouldn't.  When it comes to setting up your Vile hooks, you should
just let the Captain be your, um..., Captain...

The Captain provides commands that allow you to add (and remove) multiple
actions to any of Vile's built-in hooks.  Each action is assigned
a label and a priority.  Labels are used to reference the action later, e.g.
when removing an action.  The priorities determine in what order the
actions are executed when a particular hook is activated (see PRIORITIES).

Here's a simple example:

   perl "use CaptHook"

   autocolor-hook 'HighlightFilterMsg' 'filter' 10
   buffer-hook 'set-variable $iconname $cbufname' 'icon' 10
   buffer-hook 'perl Vile::beep' 'beep' 20
   read-hook 'search-forward \\\S' 'skipws' 20
   cd-hook 'write-message &cat "New directory: " $cwd' 'dirmsg' 100

In this example we have 1) setup the autocolor-hook in a more-or-less
standard fashion, 2) fixed it so that every time we change buffers we
set Vile's icon label to match the current buffer name, AND Vile beeps
(the beeping gets damn annoying real fast, by the way), 3) whenever
we edit a new file, Vile moves the cursor over any initial whitespace,
and 4) whenever we change Vile's current directory, the new directory
name is displayed in the message buffer.  Wasn't that easy?  No
scurvy, no large deceased sea birds around your neck, no problems.

Something to note:  When the Captain is initially loaded, he assigns
to each of Vile's hook variables one of his own activation functions.
However, he does note if any of Vile's hook variables have been set
previously and, if so, he saves the settings, adding them to his own
sets of hook actions.  (The priority assigned to any extant actions
is fairly arbitrary... As a matter of fact, I'm making it up right
now.  It's, um, probably like 1542 or something.)  Since other Perl
packages or sourced macro files might setup some of the Vile hooks,
you should probably make loading the CaptHook package and setting up
your hooks the last thing that happens in your I<.vilerc> file.

Now, I know some of you may think this is all overkill, but just you
wait until some clever soul hacks up a package that dynamically
manipulates certain hook-action priorities based on changing editor
conditions (possibly related to the current temperature in Arlington, MA),
and then you will all be exclaiming, "Thank goodness we had the Captain
or implementing this highly contrived and vague example would have been
nearly, if not almost but not quite completely impossible!"

=head1 PRIORITIES

Command priorities are assigned by the user whenever a new hook "action"
is added.  Me thinks you can use any string for a priority.  However,
the Captain sorts priorities in a numerically ascending order, so you
would be wise to always use numbers to avoid confusion.  Adding actions
with priorities of 1, 2, 3, etc. is fine, until you want to add an
action after the first and before the second.  Then you'd have to
resort to fractions, e.g. 1.5, and how many of those could there be?
Um, nevermind.  The point is that it might be less awkward to use priorities
like 10, 20, 30, etc. instead.  Then you can always stick in a 12 or 25 or
whatever, and save those decimal points for professions that really need
them, like chartered accountancy for instance.  Ah, chartered accountancy,
there's the life!  I could have been a chartered accountant you know.  I
had the handwriting for it, no doubt.  But my mum insisted I become
a pirate instead.  "Pirating was good enough for your no-account father,
the bastard, and it'll be good enough for you!  Now finish your grog!",
she'd say.  Sigh.  Many's the hour I whiled away sitting in the crow's
nest dreaming of the life I could have had.  A solid floor under my feet,
my own desk, sharpened pencils all neat in their holder...  Oh, buggerit.
Let's get on with the installation...

=head1 INSTALLATION

[Note: CaptHook may already be pacing the deck for you as part of your
Vile installation.]

Install CaptHook.pm somewhere in perl's @INC path.  Depending on your
Vile installation, I</usr/local/share/vile/perl> might be a good place.

Add B<perl "use CaptHook"> to you I<.vilerc> file.  Then use the
Captain's B<...-hook> commands to set up your hooks.  Note that
other packages may set hooks too, so you should probably activate
CaptHook last, giving him a chance to come along side and board
any previously setup hooks... so to speak.

=head1 USAGE

=item perl "use CaptHook"

Invite the Captain on board...

=item autocolor-hook <command> <label> <priority>

=item buffer-hook <command> <label> <priority>

=item cd-hook <command> <label> <priority>

=item exit-hook <command> <label> <priority>

=item read-hook <command> <label> <priority>

=item write-hook <command> <label> <priority>

Add a command to be invoked during a particular hook (obviously
which hook depends on which variation you use).  Note that these
are cumulative.  Running B<:buffer-hook ...> twice with two
different labels gives you TWO commands that get executed whenever
you change buffers.  The order of execution will depend on the
priorities given (see PRIORITIES).  Reusing a label will result
in the replacement of any command already assigned to that label,
for a given hook.  I.E. You can have a cd-hook and a buffer-hook
with the same label, but you cannot have two cd-hooks with the same label.

=item autocolor-unhook <label>

=item buffer-unhook <label>

=item cd-unhook <label>

=item exit-unhook <label>

=item read-unhook <label>

=item write-unhook <label>

Delete a command assigned to a particular hook (which hook depends on
which variation you use).  The label given should match the label
used when the no-longer-needed command was originally "hooked".

=item do-autocolor-hooks

=item do-buffer-hooks

=item do-cd-hooks

=item do-exit-hooks

=item do-read-hooks

=item do-write-hooks

Invoke the set of commands assigned to a particular hook.  Each of
the B<do-*-hooks> commands is assigned to an appropriate one of
Vile's B<$*-hook> variables when the Captain boards the good ship Vile.
(The order in which a particular set of commands is invoked depends on
the priority assigned to each command in the set (see PRIORITIES).)
Normally these are used to automatically trigger a set of actions
whenever a hook is activated, but they could be invoked manually as
well.

=item capthook-help

Discover the Captain's treasure map.

=head1 BUGS

Probably result in some odd interactions here and there... For instance,
I've already noticed that this:

   autocolor-hook HighlightFilterMsg filter 1

uncovers a 'bug' in the Vile Perl extension.  Specifically, you'll
see a lot of the message "BUG: curwp not set to a visible window",
even though things seem to still work.

By the way, if anyone requests a B<show-hooks> command, I'll
make 'em walk the plank!

=head1 SEE ALSO

vile(1)

=head1 CREDITS

Drat.  No one to blame but myself...

=head1 AUTHOR

S<J. Chris Coppick, 2001 (last updated: Sept. 27, 2001)>

Ahoy, Maties!


=cut


use Vile;
use Vile::Manual;
require Vile::Exporter;

@ISA = 'Vile::Exporter';
%REGISTRY = (
    'autocolor-hook'  => ['CaptHook::addHook("autocolor")', 'add a new autocolor hook' ],
    'autocolor-unhook'  => ['CaptHook::delHook("autocolor")', 'remove an autocolor hook' ],
    'buffer-hook'  => ['CaptHook::addHook("buffer")', 'add a new buffer hook' ],
    'buffer-unhook'  => ['CaptHook::delHook("buffer")', 'remove a buffer hook' ],
    'cd-hook'  => ['CaptHook::addHook("cd")', 'add a new cd hook' ],
    'cd-unhook'  => ['CaptHook::delHook("cd")', 'remove a cd hook' ],
    'exit-hook'  => ['CaptHook::addHook("exit")', 'add a new exit hook' ],
    'exit-unhook'  => ['CaptHook::delHook("exit")', 'remove an exit hook' ],
    'read-hook'  => ['CaptHook::addHook("read")', 'add a new read hook' ],
    'read-unhook'  => ['CaptHook::delHook("read")', 'remove a read hook' ],
    'write-hook'  => ['CaptHook::addHook("write")', 'add a new write hook' ],
    'write-unhook'  => ['CaptHook::delHook("write")', 'remove a write hook' ],
    'do-autocolor-hooks'  => ['CaptHook::_do_hook("autocolor")', 'Execute autocolor hooks' ],
    'do-buffer-hooks'  => ['CaptHook::_do_hook("buffer")', 'Execute buffer hooks' ],
    'do-cd-hooks'  => ['CaptHook::_do_hook("cd")', 'Execute cd hooks' ],
    'do-exit-hooks'  => ['CaptHook::_do_hook("exit")', 'Execute exit hooks' ],
    'do-read-hooks'  => ['CaptHook::_do_hook("read")', 'Execute read hooks' ],
    'do-write-hooks'  => ['CaptHook::_do_hook("write")', 'Execute write hooks' ],
    'capthook-help' => [sub {&manual}, 'manual page for CaptHook.pm' ]
);

sub import {
   Vile::Exporter::import(@_);
   _setup();
}

*unimport = *_teardown;

my %_autocolor_hooks;
my %_buffer_hooks;
my %_cd_hooks;
my %_exit_hooks;
my %_read_hooks;
my %_write_hooks;

my @_autocolor_index;
my @_buffer_index;
my @_cd_index;
my @_exit_index;
my @_read_index;
my @_write_index;


# I shall fear no eval...

sub addHook {

   my ($hook) = @_;

   my ($label, $rank, $action);

   $action = Vile::mlreply_no_opts("${hook}-hook command?: ");
   return 0 if (!defined($action) || $action eq '' || $action =~ /^\s*$/);
   chomp($action);

   $label = Vile::mlreply_no_opts("${hook}-hook command label?: ");
   return 0 if (!defined($label) || $label eq '' || $label =~ /^\s*$/);
   chomp($label);

   $rank = Vile::mlreply_no_opts("${hook}-hook command priority?: ");
   return 0 if (!defined($rank) || $rank eq '' || $rank =~ /^\s*$/);
   chomp($rank);

   _del_hook($hook, $label);

   if (_add_hook($hook, $label, $rank, $action)) {

      print "Failed adding new hook: is ${hook}-hook supported?";
      return 1;
   }

   return 0;
}

sub delHook {

   my ($hook) = @_;
   my $label;

   $label = Vile::mlreply_no_opts("${hook}-hook command label?: ");
   return 0 if (!defined($label) || $label eq '' || $label =~ /^\s*$/);
   chomp($label);

   if (_del_hook($hook, $label)) {
      print "Failed deleting hook: is ${hook}-hook supported?";
      return 1;
   }

   return 0;

}

sub _do_hook {

   my ($hook) = @_;
   my $label = 1;
   my ($action, $i, $max);

   return 1 if (_not_supported($hook));

   $max = eval("scalar(\@_${hook}_index)");

   for ($i = 0; $i < $max; $i++) {
      $action = eval("\$_${hook}_hooks{\$_${hook}_index[\$i][\$label]}");
      if (defined($action) && $action ne '') {
	 package main;
	 eval { Vile::command $action }; warn $@ if $@;
	 Vile::update();
	 package CaptHook;
      }
   }

   return 0;
}

sub _add_hook {

   my ($hook, $label, $rank, $action) = @_;
   my $max;

   return 1 if (_not_supported($hook));

   $max = eval("scalar(\@_${hook}_index)");

   eval("\$_${hook}_index[\$max][0] = \$rank");
   eval("\$_${hook}_index[\$max][1] = \$label");
   eval("\@_${hook}_index = sort { \$a->[0] <=> \$b->[0] } \@_${hook}_index");

   eval("\$_${hook}_hooks{\$label} = \$action");

   return 0;
}

sub _del_hook {

   my ($hook, $label) = @_;
   my ($max, $i);

   return 1 if (_not_supported($hook));

   $max = eval("scalar(\@_${hook}_index)");

   for ($i = 0; $i < $max; $i++) {
      if (eval("\$_${hook}_index[\$i][1]") eq $label) {
	 eval("splice(\@_${hook}_index, \$i, 1)");
	 last;
      }
   }

   if (eval("defined(\$_${hook}_hooks{\$label})")) {
      eval("delete \$_${hook}_hooks{\$label}");
   }

   return 0;
}

sub _setup {

   my $oldhook;
   my $cb = $Vile::current_buffer;

   $oldhook = $cb->get('$autocolor-hook');
   Vile::command 'setv $autocolor-hook do-autocolor-hooks';
   if ($oldhook ne 'ERROR' && $oldhook ne '' &&
       $oldhook ne 'do-autocolor-hooks') {
      _add_hook("autocolor", "original", 1542, $oldhook);
   }

   $oldhook = $cb->get('$buffer-hook');
   Vile::command 'setv $buffer-hook do-buffer-hooks';
   if ($oldhook ne 'ERROR' && $oldhook ne '' &&
       $oldhook ne 'do-buffer-hooks') {
      _add_hook("buffer", "original", 1542, $oldhook);
   }

   $oldhook = $cb->get('$cd-hook');
   Vile::command 'setv $cd-hook do-cd-hooks';
   if ($oldhook ne 'ERROR' && $oldhook ne '' &&
       $oldhook ne 'do-cd-hooks') {
      _add_hook("cd", "original", 1542, $oldhook);
   }

   $oldhook = $cb->get('$exit-hook');
   Vile::command 'setv $exit-hook do-exit-hooks';
   if ($oldhook ne 'ERROR' && $oldhook ne '' &&
       $oldhook ne 'do-exit-hooks') {
      _add_hook("exit", "original", 1542, $oldhook);
   }

   $oldhook = $cb->get('$read-hook');
   Vile::command 'setv $read-hook do-read-hooks';
   if ($oldhook ne 'ERROR' && $oldhook ne '' &&
       $oldhook ne 'do-read-hooks') {
      _add_hook("read", "original", 1542, $oldhook);
   }

   $oldhook = $cb->get('$write-hook');
   Vile::command 'setv $write-hook do-write-hooks';
   if ($oldhook ne 'ERROR' && $oldhook ne '' &&
       $oldhook ne 'do-write-hooks') {
      _add_hook("write", "original", 1542, $oldhook);
   }

   return 0;
}

sub _teardown {

   Vile::command 'unsetv $autocolor-hook';
   Vile::command 'unsetv $buffer-hook';
   Vile::command 'unsetv $cd-hook';
   Vile::command 'unsetv $exit-hook';
   Vile::command 'unsetv $read-hook';
   Vile::command 'unsetv $write-hook';

   return 0;
}

sub _not_supported {

   my ($hook) = @_;

   return 1 if ($hook ne 'autocolor' &&
                $hook ne 'buffer' &&
                $hook ne 'cd' &&
                $hook ne 'exit' &&
                $hook ne 'read' &&
                $hook ne 'write');

   return 0;
}


1;
