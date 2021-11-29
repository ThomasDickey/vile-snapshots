# $Id: spell.pm,v 1.10 2014/01/28 23:35:55 tom Exp $
package spell;

use strict;
use File::Temp qw/ tempfile tempdir /;
use Vile;

require Vile::Exporter;

our @ISA = 'Vile::Exporter';
our %REGISTRY =
  ( spell => [ \&spell, 'spellcheck the current buffer (ispell)' ] );

sub checkline($) {
    my @reply;
    ( my $fh, my $filename ) = tempfile();
    print $fh $_[0];
    close $fh;
    open my $fh, "ispell -a < $filename 2>/dev/null |" or return @reply;
    @reply = <$fh>;
    close $fh;
    unlink $filename;
    return @reply;
}

sub spell {

    my $work  = Vile::working(0);
    my %color = (
        "&" => [ "color", "2" ],
        "?" => [ "color", "5" ],
        "#" => [ "color", "1" ],
    );
    my $prompt = "[S]kip, [R]eplace, [A]ccept, [I]nsert, [U]ncap, [Q]uit? ";

    my $cb     = $Vile::current_buffer;
    my $blines = scalar( Vile->get("\$blines") );
    $cb->setregion( 1, '$' )->attribute("normal");

    my @dot = $cb->dot;
    my ( $resp, @H, @O, @cmds, %accepted, %replaced, $off );
    my $i = 1;
    while (<$cb>) {
        $off = 0;
        undef @cmds, %accepted, %replaced;
        $cb->dot($i);
        Vile::update();
        foreach $resp ( checkline($_) ) {
            chop $resp;
            if ( $resp =~ m:^(\&|\?|\#): ) {
                @H = split( " ", ( split( ": ", $resp ) )[0] );
                next if ( defined $accepted{ $H[1] } );
                @O = split( ", ", ( split( ": ", $resp ) )[1] );
                if ( $H[0] eq "#" ) {
                    $H[3] = $H[2];
                    $H[2] = 0;
                }
                if ( defined $replaced{ $H[1] } ) {
                    my $prior = -1;
                    for my $n ( 0 .. $#O ) {
                        if ( $O[$n] eq $replaced{ $H[1] } ) {
                            $prior = $n;
                            last;
                        }
                    }
                    if ( $prior >= 0 ) {
                        for my $n ( 1 .. $prior ) {
                            $O[$n] = $O[ $n - 1 ];
                        }
                        $O[0] = $replaced{ $H[1] };
                    }
                    else {
                        unshift @O, $replaced{ $H[1] };
                        $H[2]++;
                    }
                }
                @O = splice( @O, 0, $H[2] );
                $cb->setregion( $i, $H[3] + $off,
                    $i, $H[3] + length( $H[1] ) + $off )
                  ->attribute( "reverse", @{ $color{ $H[0] } } );
                Vile::update();
                print $prompt;
                while ( $resp = uc( sprintf( "%c", Vile::keystroke() ) ) ) {
                    if ( $resp eq "A" ) {
                        $accepted{ $H[1] } = 1;
                        push @cmds, "\@$H[1]";
                        $cb->setregion( $i, $H[3] + $off,
                            $i, $H[3] + length( $H[1] ) + $off )
                          ->attribute("normal")
                          ->attribute( "bold", @{ $color{ $H[0] } } );
                        last;
                    }
                    elsif ( $resp eq "S" ) {
                        $cb->setregion( $i, $H[3] + $off,
                            $i, $H[3] + length( $H[1] ) + $off )
                          ->attribute("normal")
                          ->attribute( "bold", @{ $color{ $H[0] } } );
                        last;
                    }
                    elsif ( $resp eq "R" ) {
                        my $word = Vile::mlreply_opts( "Replace with? ", @O );
                        if ( defined $word ) {
                            $replaced{ $H[1] } = $word;
                            $cb->setregion( $i, $H[3] + $off,
                                $i, $H[3] + length( $H[1] ) + $off )
                              ->attribute("normal")->delete;
                            $cb->dot( $i, $H[3] + $off );
                            print $cb $word;
                            $off = $off + length($word) - length( $H[1] );
                        }
                        else {
                            print $prompt;
                            next;
                        }
                        last;
                    }
                    elsif ( $resp eq "I" ) {
                        $accepted{ $H[1] } = 1;
                        push @cmds, "\@$H[1]";
                        push @cmds, "*$H[1]";
                        push @cmds, "#";
                        $cb->setregion( $i, $H[3] + $off,
                            $i, $H[3] + length( $H[1] ) + $off )
                          ->attribute("normal")
                          ->attribute( "bold", @{ $color{ $H[0] } } );
                        last;
                    }
                    elsif ( $resp eq "U" ) {
                        $accepted{ $H[1] } = 1;
                        push @cmds, "\@$H[1]";
                        push @cmds, "&$H[1]";
                        push @cmds, "#";
                        $cb->setregion( $i, $H[3] + $off,
                            $i, $H[3] + length( $H[1] ) + $off )
                          ->attribute("normal")
                          ->attribute( "bold", @{ $color{ $H[0] } } );
                        last;
                    }
                    elsif ( $resp eq "Q" ) {
                        $cb->setregion( $i, $H[3] + $off,
                            $i, $H[3] + length( $H[1] ) + $off )
                          ->attribute("normal")
                          ->attribute( "bold", @{ $color{ $H[0] } } );
                        Vile::working($work);
                        return;
                    }
                }
            }
        }
        last if ( ++$i > $blines );
        $cb->setregion( $i, '$' );
    }
    $cb->dot(@dot);
    Vile::working($work);
}

1;

__END__

=head1 NAME

spell - spelling checker in vile

=head1 SYNOPSIS

    require "spell.pl"

In .vilerc

    perl "Vile::register 'spell', 'spell', 'Spell Check', 'spell.pl'"

In [x]vile

    :spell

    :perl spell

=head1 DESCRIPTION

This  is a spelling checker written  for use within  [x]vile
using  the perl  interface.  It  uses  the "ispell"  command
internally  and hence is compatible  with ispell's usage  of
public and private dictionaries.

On  invocation, it goes through the  current buffer line  by
line,  finding spelling  errors  and  highlighting  them.  The
current  error is highlighted according to the following color
code and in reverse.

If  the word is not in  the dictionary,  but there are  near
misses,  then the  error is  highlighted  in green  indicating
mild  error. If there are no near misses but the word  could
be  formed by adding (illegal) affixes to a known root,  the
error  is highlighted in  cyan  indicating moderately  serious
error.  Finally, if the word is not in the dictionary, there
are  no near misses and no  possible derivations found  then
the error is highlighted in red indicating a grave error.

As  each error  is  highlighted  in  succession,  the user  is
prompted  to enter  a single  character  command as  follows
(case is ignored):

    R    Replace the misspelled word completely

    S    Accept the word this time only

    A    Accept the word for the rest of this session

    I    Accept the word as it is, update private dictionary

    U    Accept the word, add lower case version to private
         dictionary

    Q    Quit the spell checker session

If  the user  chooses  to  replace  the  word,  then  he  is
prompted  to  enter  the  replacement  word.  If  there  are
possible  replacements found by  the  spell checker  itself,
the  first is used as  an initial  value in  the prompt.  At
this  prompt, the user can <TAB>  his way  through  all  the
possible  replacements suggested by  the  spell checker.  He
can  also partially fill in a replacement and <TAB>  through
all  matching suggested replacements. On pressing <RET>, the
word  is replaced with  the  one provided  by  the user.  On
pressing   <ESC>,  replacement  is  aborted  and  the  spell
checker moves on to the next error.

As  each new error is highlighted successively, old errors are
switched  from reverse to bold highlighting to allow the  user
to  follow the progress of the spell checker and at the same
time  leave a trace of errors for later changes from outside
the spell checker.

=head1 CAVEATS

Since   the  spell   checker   utilizes   "ispell"   program
internally,  it is mandatory that  "ispell" be available  in
the users path.

Since  the spell checker  works in  line by  line mode,  the
"insert"  and  "uncap"  commands  don't  update  the  private
dictionary  immediately but only on reaching the end of  the
current line.

=head1 CREDITS

Vile, Perl and Vile's Perl interface.

=head1 AUTHOR

Kuntal Daftary (daftary@cisco.com), 1998

=cut
