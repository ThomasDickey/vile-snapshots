package Glob2re;
require 5.000;
require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(glob2re);

sub glob2re {
    my ($glob) = @_;
    my $re = '(^|\/)';

    while ($glob ne '') {
	if ($glob =~ /^\*(.*)$/) {
	    $re .= '.*';
	    $glob = $1;
	}
	elsif ($glob =~ /^\?(.*)$/) {
	    $re .= '.';
	    $glob = $1;
	}
	elsif ($glob =~ /^\[(.+?)\](.*)$/) {
	    $re .= "[$1]";
	    $glob= $2;
	}
	elsif ($glob =~ /^\{(.*?,.*?)\}(.*)$/) {
	    my ($alts) = $1;
	    $glob = $2;
	    $re .= '(' . join('|',map(quotemeta,split(/,/, $alts))) . ')';
	}
	elsif ($glob =~ /^(.[^[{*?]*)(.*)$/) {
	    $re .= quotemeta($1);
	    $glob = $2;
	}
	else {
	    # shouldn't get here.  If we do, give up
	    $glob = '';
	}
    }
    $re .= '$';
    return $re;
}

1;

