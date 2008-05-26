package Vile::Exporter;

=head1 NAME

Vile::Exporter - provide import method for vile extensions

=head1 SYNOPSIS

In module foo.pm:

    package foo;
    require Vile::Exporter;
    @ISA = 'Vile::Exporter';
    %REGISTRY = (
	cmd-a => \&cmd_a,
	cmd-b => [ \&cmd_b, 'Description', '#1' ],
	cmd-c => [ motion => \&cmd_c, 'Motion' ],
    );

From vile:

    :perl use foo                    # register all commands with
     				     # default bindings (if any)

    :perl use foo ()		     # register no commands
    :perl use foo 'cmd-a'            # register only cmd-a
    :perl use foo 'cmd-b' => '#2'    # register cmd-b, bound to F2
    :perl use foo 'cmd-b' => ''      # register cmd-b, not bound

=head1 DESCRIPTION

The Vile::Exporter package provides a default import method to make
perl extensions visible from vile.

Extensions simply need to include `Vile::Exporter' in @ISA and define
a hash %REGISTRY to map vile command names to perl subroutines.

The hash values may either be a simple subroutine reference, or an an
array containing:

    [motion|oper,] reference [, description [, key, ...]]

giving the reference, description and a list of default keybindings. 
The optional prefix of `motion' or `oper' causes vile to register the
subroutine as a motion or operator respectively.

=head1 AUTHOR

Brendan O'Dea <bod@compusol.com.au>

=cut

require Carp;

sub import
{
    my $pkg = shift;
    local *registry = *{"${pkg}::REGISTRY"};
    my @imports = @_ ? @_ : keys %registry;
    my $name;

    while ($name = shift @imports)
    {
	unless (exists $registry{$name})
	{
	    Carp::carp("$name is not registered by $pkg");
	    next;
	}

	my @args;
	my @keys;
	local *register = *Vile::register;

	for ($registry{$name})
	{
	    if (ref eq 'ARRAY')
	    {
		@args = @$_;
		*register = *{"Vile::register_" . shift @args}
		    if $args[0] eq 'motion' or $args[0] eq 'oper';

		@keys = splice @args, 2 if @args > 2;
	    }
	    else
	    {
		@args = $_;
	    }
	}

	# override default binding
	@keys = shift @imports if @imports and $imports[0] =~ m{
	    ^(	(\^[AX]-?)?			# optional ^A or ^X prefix
	        ((\#|FN)-?)?			# optional function prefix
		(M-)?				# optional meta prefix
		(   \^.				# ^C
		  | \\[0-7]{1,3}		# or \NNN (octal)
		  | \\[xX][0-9a-f]{1,2}	# or \xNN (hex)
		  | \\[nrtbfaes]		# or \n, \r, etc
		  | .				# or a single character
		)
	      | # nothing
	     )$
	}x;

	register($name, @args);
	for (@keys) { Vile::command("bind-key $name $_") if length }
    }
}
