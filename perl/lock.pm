package lock;

require Vile::Exporter;
@ISA = 'Vile::Exporter';
%REGISTRY = ('lock' => [ \&lock, 'lock vile session' ]);

sub getpass {
    local ($text) = @_;
    print "Enter password to $text: ";
    undef $word;
    for (; ($char=Vile::keystroke())!=13; $word.=sprintf "%c",$char) {;}
    $word;
}

sub lock {
    my ($word, $pass, $salt);
    my $work = Vile::working(0);
    $pass = (getpwuid($<))[1];
    $salt = substr($pass, 0, 2);
    if ( $pass == "x" ) {
	# if the system has shadow passwords, make our own
	$word = getpass("lock");
        $salt = substr($word, 0, 2);
        $pass = crypt ($word, $salt);
    }
    while (1) {
        sleep 1;
	$word = getpass("unlock");
        last if (crypt ($word, $salt) eq $pass);
        print "Wrong password!";
     }
     Vile::working($work);
     print "";
}

1;

__END__

=head1 NAME

lock - lock a vile session

=head1 SYNOPSIS

In .vilerc:

    perl "use lock"

In [x]vile:

    :lock

=head1 DESCRIPTION

This command allows the user to lock his [x]vile session.

Once  the  session is locked,  "Enter password  to unlock: "
prompt  will appear in  the  status line  prompting for  the
password.  The session  will give  unlock  and give  control
back  to the user only when the correct password is entered.
This  password is the same as what the user used to login to
the machine.

=head1 CAVEATS

When  the session is locked,  ideally the  modeline and  the
editing  buffers should blank out  to prevent others  seeing
any data, but this does not happen presently.

=head1 CREDITS

Vile, Perl and Vile's Perl interface.

=head1 AUTHOR

Kuntal Daftary (daftary@cisco.com), 1998

=cut
