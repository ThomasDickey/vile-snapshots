$! $Header: /users/source/archives/vile.vcs/RCS/vmsbuild.com,v 1.9 1996/03/24 13:38:16 pgf Exp $
$! VMS build-script for vile.  Requires "VAX-C"
$!
$! Tested with:
$!	VMS system version 5.4-2
$!	VAX-C version 3.2
$!
$! To change screen driver modules, change SCREEN and SCRDEF below
$!
$
$ define/nolog SYS SYS$LIBRARY		! fix includes to <sys/...>
$ MKTBLS :== $SYS$DISK:'F$DIRECTORY()MKTBLS.EXE	! make a foreign command
$
$! for regular vile, use these:
$ SCREEN := vmsvt
$ TARGET := vile
$ SCRDEF := "DISP_VMSVT,scrn_chosen"
$
$! for building the X version, xvile, use these:
$!SCREEN == x11simp
$!TARGET == xvile
$!SCRDEF == "DISP_X11,scrn_chosen"
$
$! for building the X-toolkit version:
$!SCREEN := x11
$!TARGET := xvile
$!SCRDEF := "NO_WIDGETS,XTOOLKIT,DISP_X11,scrn_chosen"
$
$! for building the Motif version (untested):
$!SCREEN := x11
$!TARGET := xvile
$!SCRDEF := "MOTIF_WIDGETS,XTOOLKIT,DISP_X11,scrn_chosen"
$
$! used /G_FLOAT with vaxcrtlg/share in vms_link.opt
$! can also use /Debug /Listing, /Show=All
$
$!
$ axp  = f$getsyi("HW_MODEL").ge.1024
$ CC   = "CC"
$ DEFS = ""
$ OPTS = ""
$
$ if axp
$ then
$! assume we have DEC C
$	CFLAGS = "/standard=vaxc"
$	DEFS = ",HAVE_ALARM"
$ else
$! we have either VAX C or GNU C
$	CFLAGS = ""
$	DEFS = ",HAVE_SYS_ERRLIST"
$	OPTS = ",VMSSHARE.OPT/OPTIONS"
$	if (f$search("SYS$SYSTEM:VAXC.EXE").eqs."" .and. -
		f$trnlnm("GNU_CC").nes."") .or. (p1.eqs."GCC")
$	then
$		CC = "gcc"
$		OPTS = "''OPTS',GNU_CC:[000000]GCCLIB.OLB/LIB"
$	endif
$ endif
$
$ CFLAGS := 'CFLAGS/Diagnostics /Define=("os_chosen","''SCRDEF'''DEFS'") /Include=([])
$
$	if "''p1'" .nes. "" then goto 'p1
$
$
$ all :
$	if f$search("mktbls.exe") .eqs. ""
$	then
$		call make mktbls
$		link /exec=mktbls/map/cross mktbls.obj,SYS$LIBRARY:VAXCRTL/LIB
$	endif
$	if f$search("nebind.h") .eqs. "" then mktbls cmdtbl
$	if f$search("nemode.h") .eqs. "" then mktbls modetbl
$
$	call make main
$	call make 'SCREEN
$	call make basic
$	call make bind
$	call make buffer
$	call make crypt
$	call make csrch
$	call make display
$	call make dumbterm
$	call make eval
$	call make exec
$	call make externs
$	call make fences
$	call make file
$	call make filec
$	call make fileio
$	call make finderr
$	call make glob
$	call make globals
$	call make history
$	call make input
$	call make insert
$	call make itbuff
$	call make isearch
$	call make line
$	call make map
$	call make modes
$	call make msgs
$	call make npopen
$	call make oneliner
$	call make opers
$	call make path
$	call make random
$	call make regexp
$	call make region
$	call make search
$	call make select
$	call make spawn
$	call make tags
$	call make tbuff
$	call make termio
$	call make undo
$	call make version
$	call make vms2unix
$	call make vmspipe
$	call make window
$	call make word
$	call make wordmov
$
$	link /exec='target/map/cross main.obj, 'SCREEN.obj, vms_link/opt 'OPTS
$	goto build_last
$
$ install :
$	WRITE SYS$ERROR "** no rule for install"
$	goto build_last
$	
$ clean :
$	if f$search("*.obj") .nes. "" then delete *.obj;*
$	if f$search("*.bak") .nes. "" then delete *.bak;*
$	if f$search("*.lis") .nes. "" then delete *.lis;*
$	if f$search("*.log") .nes. "" then delete *.log;*
$	if f$search("*.map") .nes. "" then delete *.map;*
$	if f$search("ne*.h") .nes. "" then delete ne*.h;
$	if f$search("$(MKTBLS)") .nes. "" then delete $(MKTBLS);
$	goto build_last
$
$ clobber :
$	if f$search("vile.com") .nes. "" then delete vile.com;*
$	if f$search("xvile.com") .nes. "" then delete xvile.com;*
$	if f$search("*.exe") .nes. "" then delete *.exe;*
$	goto build_last
$
$ build_last :
$	if f$search("*.dia") .nes. "" then delete *.dia;*
$	if f$search("*.lis") .nes. "" then purge *.lis
$	if f$search("*.obj") .nes. "" then purge *.obj
$	if f$search("*.map") .nes. "" then purge *.map
$	if f$search("*.exe") .nes. "" then purge *.exe
$	if f$search("*.log") .nes. "" then purge *.log
$	exit
$
$! Runs VILE from the current directory (used for testing)
$ vile_com :
$	if "''f$search("vile.com")'" .nes. "" then delete vile.com;*
$	copy nl: vile.com
$	open/append  test_script vile.com
$	write test_script "$ temp = f$environment(""procedure"")"
$	write test_script "$ temp = temp -"
$	write test_script "		- f$parse(temp,,,""version"",""syntax_only"") -"
$	write test_script "		- f$parse(temp,,,""type"",""syntax_only"")"
$	write test_script "$ vile :== $ 'temp'.exe"
$	write test_script "$ define/user_mode sys$input  sys$command"
$	write test_script "$ define/user_mode sys$output sys$command"
$	write test_script "$ vile 'p1 'p2 'p3 'p4 'p5 'p6 'p7 'p8"
$	close test_script
$	write sys$output "** made vile.com"
$	exit
$
$! Runs XVILE from the current directory (used for testing)
$ xvile_com :
$	if "''f$search("xvile.com")'" .nes. "" then delete xvile.com;*
$	copy nl: xvile.com
$	open/append  test_script xvile.com
$	write test_script "$ temp = f$environment(""procedure"")"
$	write test_script "$ temp = temp -"
$	write test_script "		- f$parse(temp,,,""name"",""syntax_only"") -"
$	write test_script "		- f$parse(temp,,,""version"",""syntax_only"") -"
$	write test_script "		- f$parse(temp,,,""type"",""syntax_only"")"
$	write test_script "$ xvile :== $ 'temp'xvile.exe"
$	write test_script "$ define/user_mode sys$input  sys$command"
$	write test_script "$ define/user_mode sys$output sys$command"
$	write test_script "$ xvile 'p1 'p2 'p3 'p4 'p5 'p6 'p7 'p8"
$	close test_script
$	write sys$output "** made xvile.com"
$	exit
$
$ make: subroutine
$	if f$search("''p1'.obj") .eqs. ""
$	then
$		write sys$output "compiling ''p1'"
$		'CC 'CFLAGS 'p1
$		if f$search("''p1'.dia") .nes. "" then delete 'p1.dia;*
$	endif
$exit
$	return
$ endsubroutine
