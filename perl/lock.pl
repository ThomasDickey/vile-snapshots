sub lock {
    my ($word, $pass, $salt);
    my $work = Vile::working(0);
    $salt = substr($pass = (getpwuid($<))[1], 0, 2);
    while (1) {
        undef $word;
        print "Enter password to unlock: ";
        for (; ($char=Vile::keystroke())!=13; $word.=sprintf "%c",$char) {;}
        last if (crypt ($word, $salt) eq $pass);
        print "Wrong password!";
        sleep 1;
     }
     Vile::working($work);
}

1;

__END__

=head1 NAME

lock - lock a vile session

=head1 SYNOPSIS

require "lock.pl"

In .vilerc

perl "Vile::register 'lock', 'lock', 'Lock Vile Session', 'lock.pl'"

In [x]vile

:lock

:perl lock

=head1 DESCRIPTION

This command allows the user to lock his [x]vile session.

Once  the  session is locked,  "Enter password  to unlock: "
prompt  will appear in  the  status line  prompting for  the
password.  The session  will give  unlock  and give  control
back  to the user only when the correct password is entered.
This  password is the same as what the user used to login to
the machine.

=head1 CAEVATS

When  the session is locked,  ideally the  modeline and  the
editing  buffers should blank out  to prevent others  seeing
any data, but this does not happen presently.

=head1 CREDITS

Vile, Perl and Vile's Perl interface.

=head1 AUTHOR

Kuntal Daftary (daftary@cisco.com), 1998

=cut
