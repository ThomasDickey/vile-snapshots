# $Id: dict.pm,v 1.6 2014/01/28 23:37:16 tom Exp $
# http://search.cpan.org/dist/Net-Dict/
package dict;

use strict;

use Net::Dict;
use Vile;
require Vile::Exporter;

use vars qw(@ISA %REGISTRY);

@ISA = 'Vile::Exporter';
%REGISTRY = ( dict => [ \&dict, 'dictionary' ] );

sub dict {
    my (
        $work, $wide,  $word, $qid, $find, $dict, $dref, %strt,
        $strt, $entry, $db,   $def, @mtch, $mtch, %DEF
    ) = ();
    my $cb;

    $work = Vile::working(0);
    $wide = scalar( Vile->get("pagewid") );

    if ( $#_ < 0 ) {
        $word = scalar( Vile->get("word") );
        $qid  = scalar( Vile->get("qidentifier") );
        $find = Vile::mlreply_opts( "Find meaning for? ", ( $qid, $word ) );
    }
    else {
        $find = $_[0];
    }

    if ( !defined $find || !length($find) ) {
        Vile::working($work);
        return;
    }

    print "Searching definitions of $find...\n";
    $dict = Net::Dict->new('dict.org');
    $dref = $dict->define($find);
    %strt = $dict->strategies;

    if ( @$dref == 0 ) {

        foreach $strt ( keys %strt ) {
            $dref = $dict->match( $find, $strt );
            foreach $entry (@$dref) {
                ( $db, $mtch ) = @$entry;
                push @mtch, $mtch;
            }
        }
        Vile::working($work);
        print "No definitions of $find...\n";
        sleep 1;
        $find = Vile::mlreply_opts( "Do you mean? ", @mtch );
        &dict($find);
        return;

    }
    else {

        foreach $entry (@$dref) {
            ( $db, $def ) = @$entry;
            push @{ $DEF{$db} }, $def;
        }

        foreach $cb ( Vile::buffers() ) {
            if ( $cb->buffername eq "<dictionary>" ) {
                Vile->current_buffer($cb);
                $cb->setregion( 1, '$$' )->attribute("normal")->delete;
                last;
            }
        }
        $cb = $Vile::current_buffer;
        if ( $cb->buffername ne "<dictionary>" ) {
            $cb = new Vile::Buffer;
            $cb->buffername("<dictionary>");
            Vile->current_buffer($cb);
            $cb->set( "view",     1 );
            $cb->set( "readonly", 1 );
            $cb->unmark->dot('$$');
        }

        foreach $db ( keys %DEF ) {
            print $cb "=" x ( $wide - 1 ), "\n";
            print $cb "Dictionary: ", $dict->dbTitle($db), "($db)\n";
            print $cb "-" x ( $wide - 1 ), "\n";
            foreach $def ( @{ $DEF{$db} } ) {
                print $cb $def, "\n";
                print $cb "-" x ( $wide - 1 ), "\n\n\n";
            }
        }
    }

    print "\n";
    $cb->unmark()->dot( 1, 0 );
    Vile::update();
    Vile::working($work);
}

1;

__END__

=head1 NAME

dict - dictionary interface in vile

=head1 SYNOPSIS

In .vilerc

    perl use dict;

In [x]vile

    :dict

=head1 DESCRIPTION

This is a dictionary and thesaurus extension for [x]vile  written
written  using  the  vile-perl-api. It uses the Dictionary Server
Protocol (DICT) described in RFC 2229.

On invocation, it prompts the user for the  word  or  phrase  for
which to search the meaning. By default it provides the choice of
the "punctuated-word" and the "whole-word" under the cursor.  The
user  can  cycle  through  the default options by hitting the TAB
key.

The response from the DICT server is  presented  in  a  different
buffer.  The responses from all the dictionary database supported
by the DICT server are presented.

If no responses are received  from  the  DICT  server,  then  the
server  is  queried  for  closely matching words based on various
matching algorithms and the user is prompted to choose  from  the
list  of  closely  matching  words. This functionality allows the
user to approximate the spelling of the word  (or  even  truncate
it) when the exact spelling or the word is not known.

=head1 CAVEATS

This extension utilizes the Net::Dict perl package. If it is  not
available  on  your  system,  it  can  be downloaded from CPAN or
http://www.dict.org and installed in  $HOME/.vile/perl  for  Unix
users.

=head1 CREDITS

Vile, Perl and Vile's Perl interface.

=head1 AUTHOR

Kuntal Daftary (daftary@cisco.com), 2001

=cut
