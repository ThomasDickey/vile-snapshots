sub visit { 
    my ($fname, @dot) = @_; 
 
    die "visit: No filename!"		if !defined($fname); 
 
    my $visscr = $curscr->Edit($fname); 
 
    $visscr->dot(@dot)			if (@dot == 1) || (@dot == 2); 
 
    $curscr->SwitchScreen($visscr); 
 
    print join(',',$visscr->dot); 
 
    1; 
} 
 
1; 
