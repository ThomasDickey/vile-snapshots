@hilite = ('todo', 'variable',  'special',  'identifier',
           'type', 'preproc', 'error', 'statement', 'constant', 'comment');

%hilite = ("comment"    => [ "color", "04" ],
           "constant"   => [ "color", "06" ],
           "identifier" => [ "color", "02", "bold" ],
           "statement"  => [ "color", "03" ],
           "preproc"    => [ "color", "05" ],
           "type"       => [ "color", "14" ],
           "special"    => [ "color", "01" ],
           "error"      => [ "color", "11" ],
           "todo"       => [ "color", "12" ],
           "variable"   => [ "color", "13" ]);

sub syntaxperl {

    $syntax'kdata{statement} =
      "foreach for if while return next last exit do until unless sub "
    . "elsif else eq ne gt lt ge le cmp";

    $syntax'kdata{identifier} =
      "abs  accept  alarm atan2 bind binmode bless caller chdir chmod chomp "
    . "chop  chown  chr  chroot  closedir  close connect continue cos crypt "
    . "dbmclose  dbmopen  defined  delete die dump each endgrent endhostent "
    . "endnetent  endprotoent  endpwent  endservent  eof  eval  exec exists "
    . "exp  fcntl  fileno  flock  fork  format formline getc getgrent "
    . "getgrgid  getgrnam  gethostbyaddr  gethostbyname gethostent getlogin "
    . "getnetbyaddr  getnetbyname  getnetent  getpeername  getpgrp  getppid "
    . "getpriority  getprotobyname  getprotobynumber  getprotoent  getpwent "
    . "getpwnam    getpwuid    getservbyname    getservbyport    getservent "
    . "getsockname  getsockopt  glob  gmtime goto grep hex import index int "
    . "ioctl  join  keys  kill  lcfirst  lc  length link listen local "
    . "localtime  log  lstat  map mkdir msgctl msgget msgrcv msgsnd my "
    . "no  oct  opendir  open  ord  package  pack pipe pop pos printf print "
    . "push  quotemeta  rand  readdir  read  readlink  recv redo ref rename "
    . "require  reset  reverse rewinddir rindex rmdir scalar seekdir "
    . "seek  select  semctl semget semop send setgrent sethostent setnetent "
    . "setpgrp   setpriority  setprotoent  setpwent  setservent  setsockopt "
    . "shift  shmctl  shmget  shmread  shmwrite  shutdown  sin sleep socket "
    . "socketpair  sort  splice  split  sprintf  sqrt  srand stat study "
    . "substr  symlink  syscall  sysread  system  syswrite telldir tell tie "
    . "time  times  truncate  ucfirst  uc umask undef unlink unpack unshift "
    . "untie  use  utime values vec wait waitpid wantarray warn write";

    %syntax'mdata = (
        "comment" => [
            [ '^\s*#.*$', 0, 0 ],
        ],
        "special" => [
            [ 'proc \S+', 5, 0 ],
        ],
        "variable" => [
            [ '[\$@%\&][\w:]+', 0, 0 ],
            [ '<[^>]*>', 1, -1],
        ],
        "special" => [
            [ 'sub \S+', 4, 0 ],
        ],
        "constant" => [
            [ '-?\d+[.\d]*', 0, 0 ],
        ],
    );

    %syntax'rdata = (
        "constant" => [
            [ '"', '"', '\\"', 1, -1 ],
            [ '\'', '\'', '\\\'', 1, -1 ],
        ],
    );
}

sub syntaxtcl {

    $syntax'kdata{statement} =
      "foreach for if case switch while return break continue exit "
    . "uplevel source proc global upvar error catch bgerror unknown "
    . "for_array_keys for_recursive_glob loop else elseif";

    $syntax'kdata{identifier} =
      "cd after append close concat eof exec linsert format gets "
    . "glob incr join lappend lindex list llength lrange lreplace lsearch "
    . "lsort open pid puts pwd read regexp regsub scan seek socket split "
    . "subst tell time vwait popd pushd recursive_glob showproc alarm chgrp "
    . "chmod chown convertclock execl fmtclock chroot fork getclock kill link "
    . "mkdir nice rmdir sleep system sync times umask unlink wait bsearch dup "
    . "copyfile fcntl flock funlock fstat lgets frename pipe read_file select "
    . "write_file scancontext scanfile scanmatch abs ceil double exp floor "
    . "fmod pow round sqrt max min random intersect intersect3 lassign lempty "
    . "lmatch lrmdups lvarcat lvarpop lvarpush union keyldel keylget keylkeys "
    . "cequal cindex clength crange csubstr ctoken keylset cexpand ctype "
    . "expr eval set unset rename replicate translit array clock file history "
    . "trace infox id signal info string";

    %syntax'mdata = (
        "comment" => [
            [ '^\s*#.*$', 0, 0 ],
            [ '\;#.*$'  , 1, 0 ],
        ],
        "special" => [
            [ 'proc \S+', 5, 0 ],
        ],
    );

    %syntax'rdata = (
        "constant" => [
            [ '"', '"', '\\\\"', 1, -1 ],
        ],
    );
}

sub syntaxc {

    $syntax'kdata{type} = 
      "void register short enum extern int struct static long char unsigned "
    . "double float volatile union auto class friend protected private public "
    . "const mutable virtual inline inherited enum";

    $syntax'kdata{statement} = 
      "for if else while catch throw operator asm this continue break switch "
    . "case delete default goto return exit";

    $syntax'kdata{preproc} =
      "#ifdef #ifndef #if #endif #define #include";

    %syntax'mdata = (
        "comment" => [
            [ '//.*$', 0, 0 ],
        ],
        "constant" => [
            [ '\b\.?[\d]+\b', 0, 0 ],
        ],
    );

    %syntax'rdata = (
        "constant" => [
            [ '"', '"', '\\\\"', 1, -1 ],
        ],
        "comment" => [
            [ '/\*', '\*/', undef, 0, 0 ],
        ],
    );
}

sub syntaxmail {
    my ($cb) = @_;
    my $line = 1;
    $cb->dot('$$');
    $last = scalar($cb->dot)-1;
    $cb->set_region(1,'$$');
    while (<$cb>) {
        last if $_ eq "\n";
        $line++;
    }
    $cb->set_region(1,$line)->attribute("bold");
    $cb->set_region($line,'$$');

    while (<$cb>) {
        if (/^[>\s]*$/) {
            $cb->set_region($line,0,$line,'$')->delete;
        } elsif (/^[>\s]*>/) {
            $cb->set_region($line,$line)->attribute('color'=>length($&)%16);
        }
        $cb->set_region(++$line,'$');
        last if ($line >= $last);
    }
    $cb->dot(0,0);
}

1;
