# $Header: /users/source/archives/vile.vcs/perl/RCS/syntax.pl,v 1.4 1998/11/02 02:03:10 Ryan.Murray Exp $
#
# See 'hilite.doc' for an overview.  This (with hilite.pl) provide a simple
# syntax highlighting mode.
#
# Caveat: this is experimental code; the chief drawback is that it is slow.
#
# The following two lines need to be added to .vilerc
#
# ----------------------------------------
# perl "Vile::register 'synon', 'synon', 'Syntax Hilighting, do', 'syntax.pl'"
# perl "Vile::register 'synoff', 'synoff', 'Syntax Hilighting, undo', 'syntax.pl'"
# ----------------------------------------
#
# this way :synon turns on hiliting and :synoff turns off hiliting.
# (You must have a majormode defined, as well).
#
require 'hilite.pl';
sub synon {
    my $cb = $Vile::current_buffer;
    my $mode = scalar($cb->get("majormode"));
    return if (!defined($mode) || !length($mode));
    my $syntaxer = "syntax$mode";
    if (defined(&$syntaxer)) {
        $cb->setregion(1,'$')->attribute("normal");
        my $work = Vile::working(0);
        print "[Syntax hiliting for $mode...]";
        &$syntaxer($cb);
        &syntax($cb);
        print "[Syntax hiliting for $mode... done]";
        Vile::working($work);
        Vile::update;
    } else {
        print "[No syntax hilighting defined for $mode]"
    }
}

sub synoff {
    my $cb = $Vile::current_buffer;
    my $mode = scalar($cb->get("majormode"));
    return if (!defined($mode) || !length($mode));
    $cb->setregion(1,'$')->attribute("normal");
    Vile::update;
}

sub syntax {
    my ($cb) = @_;
    my (%kdata, $kdata, @kdot, $koff);
    my (%mdata, $mdata, @mdot, $moff);
    my (%rdata, $rdata, @rdot, $roff);
    my ($line, $group, $l, @data, $pat, $start, $patt);
    my ($save_line, $save_delim, $save_flag);

    %kdata = %syntax'kdata;
    %mdata = %syntax'mdata;
    %rdata = %syntax'rdata;

    # mangle the keyword lists into a regexp
    foreach $kdata (keys %kdata) {
        $kdata{$kdata}
          = "(^|\\b)(" . join("|", split(/\s+/, $kdata{$kdata})) . ")(\\b|\$)";
    }

    @data = <$cb>;
    for ($l = 0; $l <= $#data; $l++) {
        $save_line = $data[$l];

        @kdot = @mdot = ($l + 1, 0, $l + 1, 0);
        foreach $group (@hilite) {
            $line = $save_line;
            next if (!defined $kdata{$group});
            while ($line =~ m!$kdata{$group}!g) {
                $koff = pos($line);
                $kdot[3] = $koff;
                $kdot[1] = length($`);
                $cb->setregion(@kdot);
                $cb->attribute("normal");
                $cb->attribute(@{$hilite{$group}});
            }
        }
        foreach $group (@hilite) {
            next if (!defined @{$mdata{$group}});
            foreach $mdata (@{$mdata{$group}}) {
                my ($pat, $hls, $hle) = @$mdata;
                $line = $save_line;
                while ($line =~ m!$pat!g) {
                    $moff = pos($line);
                    $mdot[3] = $moff + $hle;
                    $mdot[1] = length($`) + $hls;
                    $cb->setregion(@mdot);
                    $cb->attribute("normal");
                    $cb->attribute(@{$hilite{$group}});
                }
            }
        }
    }
    $save_delim = $/; $save_flag = $*;
    $/ = ""; $* = 1;
    $save_line = $cb->setregion(1,'$')->fetch;
    foreach $group (@hilite) {
        next if (!defined @{$rdata{$group}});
        foreach $rdata (@{$rdata{$group}}) {
            my ($rps, $rpe, $rskip, $hls, $hle) = @$rdata;
            $line = $save_line; $l = 1; $start = 1;
            $patt = $rps; @rdot = (1, 0);
            while ($line =~ m!($patt|\n)!g) {

                if ($& eq "\n") {
                    ++$l;
                    $line = substr($line, length($`)+1, length($line));
                } else {
                    $roff = pos($line);
                    if ( $start == 1 ) {
                        $rdot[0] = $l;
                        $rdot[1] = length($`) + $hls;
                        $patt = $rpe;
                        $start = 0;
                    } elsif ( $start == 0 ) {
                        next if (defined ${rskip} and "$`$&" =~ m!${rskip}$!);
                        $rdot[2] = $l;
                        $rdot[3] = length($`) + $hle + 1;
                        $cb->setregion(@rdot);
                        $cb->attribute("normal");
                        $cb->attribute(@{$hilite{$group}});
                        $patt = $rps;
                        $start = 1;
                    }
                }
            }
        }
    }
    $/ = $save_delim; $* = $save_flag;
}


1;
