package Vile;

sub mlreply_opts {
    my ($prompt, @origopts) = @_;
    my ($i, $word, $lastchar) = (0, $origopts[$i], 9);
    my @opts = @origopts;
    print $prompt, $word;
    while (($char = Vile::keystroke) != 13) {
        if ($char == 9) {
            @opts = grep(/^$word/, @origopts) if ($lastchar != 9);
            $word = $opts[++$i%($#opts+1)] if ($#opts > -1);
        } elsif ($char == 8) {
            $word = substr($word, $[, length($word)-1) if (length $word);
            $i = 0;
        } elsif ($char == 21) {
            $word = "";
            $i = 0;
        } elsif ($char == 27) {
            undef $word;
            last;
        } else {
            $word = "" if ($lastchar == 9);
            $word .= sprintf "%c", $char;
            $i = 0;
        }
        print $prompt, $word;
        $lastchar = $char;
    }
    return $word;
}

1;

__END__

=head1 NAME

Vile - perl library additions to Vile package

=head1 SYNOPSIS

use Vile;

Vile::mlreply_opts($prompt, @options)

=head1 DESCRIPTION

This  module is supposed to  contain additions  to the  Vile
package which are written in perl.

mlreply_opts PROMPT OPTIONS_LIST

Prompts  the user with  the given  prompt and  first of  the
provided  option list. At the prompt, the user can <TAB> his
way  through all the possible options. He can also partially
fill  in a response and <TAB> through matching options only.
When  user presses <RET>, the user response is returned.  If
the  user aborts the query (via the use of the escape  key),
an undef is returned.

Only   backspace  and  kill-text  keys  work  correctly  for
editing  the mini-buffer. Vile's mini-buffer editing is  not
supported.  Expand characters are  also  not recognized  and
hence   this  command  resembles  mlreply_no_opts  in   that
respect despite the name.

=head1 CREDITS

Vile, Perl and Vile's Perl interface.

=head1 AUTHOR

Kuntal Daftary (daftary@cisco.com), 1998

=cut

