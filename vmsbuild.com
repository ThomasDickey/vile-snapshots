$! $Header: /users/source/archives/vile.vcs/RCS/vmsbuild.com,v 1.19 1998/05/30 20:04:10 tom Exp $
$! VMS build-script for vile.  Requires installed C compiler
$!
$! Tested with:
$!	VMS system version 5.4-2
$!	VAX-C version 3.2
$! and
$!	VMS system version 6.1, 6.2
$!	DEC C version 5.0, 5.2
$!
$! To build vile invoke @vmsbuild 
$! To build xvile invoke @vmsbuild xvile
$!
$!
$!
$!      Build the option-file
$!
$ open/write optf vms_link.opt
$ write optf "Identification=""Vile 7.5"""
$ write optf "basic.obj"
$ write optf "bind.obj"
$ write optf "btree.obj"
$ write optf "buffer.obj"
$ write optf "crypt.obj"
$ write optf "csrch.obj"
$ write optf "display.obj"
$ write optf "dumbterm.obj"
$ write optf "eval.obj"
$ write optf "exec.obj"
$ write optf "externs.obj"
$ write optf "fences.obj"
$ write optf "file.obj"
$ write optf "filec.obj"
$ write optf "fileio.obj"
$ write optf "finderr.obj"
$ write optf "glob.obj"
$ write optf "globals.obj"
$ write optf "history.obj"
$ write optf "input.obj"
$ write optf "insert.obj"
$ write optf "itbuff.obj"
$ write optf "isearch.obj"
$ write optf "line.obj"
$ write optf "map.obj"
$ write optf "menu.obj"
$ write optf "modes.obj"
$ write optf "msgs.obj"
$ write optf "npopen.obj"
$ write optf "oneliner.obj"
$ write optf "opers.obj"
$ write optf "path.obj"
$ write optf "random.obj"
$ write optf "regexp.obj"
$ write optf "region.obj"
$ write optf "search.obj"
$ write optf "select.obj"
$ write optf "spawn.obj"
$ write optf "tags.obj"
$ write optf "tbuff.obj"
$ write optf "termio.obj"
$ write optf "undo.obj"
$ write optf "version.obj"
$ write optf "vms2unix.obj"
$ write optf "vmspipe.obj"
$ write optf "window.obj"
$ write optf "word.obj"
$ write optf "wordmov.obj"
$! Look for the compiler used
$!
$ CC = "CC"
$ if f$getsyi("HW_MODEL").ge.1024
$ then
$  arch = "__alpha__=1"
$  comp  = "__decc__=1"
$  CFLAGS = "/prefix=all"
$  DEFS = ",HAVE_ALARM"
$  if f$trnlnm("SYS").eqs."" then define sys sys$library:
$ else
$  arch = "__vax__=1"
$  if f$search("SYS$SYSTEM:DECC$COMPILER.EXE").eqs.""
$   then
$    if f$trnlnm("SYS").eqs."" then define sys sys$library:
$    DEFS = ",HAVE_SYS_ERRLIST"
$    write optf "sys$library:vaxcrtl.exe/share"
$    if f$search("SYS$SYSTEM:VAXC.EXE").eqs.""
$     then
$     if f$trnlnm("GNU_CC").eqs.""
$      then
$       write sys$output "C compiler required to rebuild vile"
$       close optf
$       exit
$     else
$      write optf "gnu_cc:[000000]gcclib.olb/lib"
$      comp = "__gcc__=1"
$      CC = "GCC"
$     endif 
$    else
$    comp  = "__vaxc__=1"
$    endif
$   else
$    if f$trnlnm("SYS").eqs."" then define sys decc$library_include:
$    comp  = "__decc__=1"
$  endif
$ endif
$
$ MKTBLS :== $SYS$DISK:'F$DIRECTORY()MKTBLS.EXE	! make a foreign command
$
$ if "''p1'" .nes. "XVILE" .and. "''p1'" .nes. "XVILE.EXE"
$  then
$! for regular vile, use these:
$   SCREEN := vmsvt
$   TARGET := vile
$   SCRDEF := "DISP_VMSVT,scrn_chosen"
$   mmstar = "__vile__=1"
$ else
$! for building the Motif version (untested):
$   SCREEN := x11
$   TARGET := xvile
$   SCRDEF := "MOTIF_WIDGETS,XTOOLKIT,DISP_X11,scrn_chosen"
$   mmstar = "__xvile__=1"
$!
$!  Find out which X-Version we're running.  This will fail for older
$!  VMS versions (i.e., v5.5-1).  Therefore, choose DECWindows XUI for
$!  default.
$!
$   On Error Then GoTo XUI
$   @sys$update:decw$get_image_version sys$share:decw$xlibshr.exe decw$version
$   if f$extract(4,3,decw$version).eqs."1.0"
$   then
$     write optf "Sys$share:DECW$DWTLIBSHR.EXE/Share"
$   endif
$   if f$extract(4,3,decw$version).eqs."1.1"
$   then
$     write optf "sys$share:decw$xmlibshr.exe/share"
$     write optf "sys$share:decw$xtshr.exe/share"
$   endif
$   if f$extract(4,3,decw$version).eqs."1.2"
$   then
$     write optf "sys$share:decw$xmlibshr12.exe/share"
$     write optf "sys$share:decw$xtlibshrr5.exe/share"
$   endif
$   GoTo MAIN
$!
$XUI:
$!
$   write optf "Sys$share:DECW$DWTLIBSHR.EXE/Share"
$MAIN:
$!
$   write optf "sys$share:decw$xlibshr.exe/share"
$ endif
$ close optf
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
$
$! used /G_FLOAT with vaxcrtlg/share in vms_link.opt
$! can also use /Debug /Listing, /Show=All
$
$ if f$search("SYS$SYSTEM:MMS.EXE").eqs.""
$  then
$
$   CFLAGS := 'CFLAGS/Diagnostics /Define=("os_chosen","''SCRDEF'''DEFS'") /Include=([])
$
$  	if "''p2'" .nes. "" then goto 'p2
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
$	call make btree
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
$	call make menu
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
$	link /exec='target/map/cross main.obj, 'SCREEN.obj, vms_link/opt 
$	goto build_last
$
$ install :
$	WRITE SYS$ERROR "** no rule for install"
$	goto build_last
$	
$ clobber :
$	if f$search("vile.com") .nes. "" then delete vile.com;*
$	if f$search("xvile.com") .nes. "" then delete xvile.com;*
$	if f$search("*.exe") .nes. "" then delete *.exe;*
$! fallthru
$
$ clean :
$	if f$search("*.obj") .nes. "" then delete *.obj;*
$	if f$search("*.bak") .nes. "" then delete *.bak;*
$	if f$search("*.lis") .nes. "" then delete *.lis;*
$	if f$search("*.log") .nes. "" then delete *.log;*
$	if f$search("*.map") .nes. "" then delete *.map;*
$	if f$search("*.opt") .nes. "" then delete *.opt;*
$	if f$search("ne*.h") .nes. "" then delete ne*.h;
$	if f$search("$(MKTBLS)") .nes. "" then delete $(MKTBLS);
$! fallthru
$
$ build_last :
$	if f$search("*.dia") .nes. "" then delete *.dia;*
$	if f$search("*.lis") .nes. "" then purge *.lis
$	if f$search("*.obj") .nes. "" then purge *.obj
$	if f$search("*.map") .nes. "" then purge *.map
$	if f$search("*.opt") .nes. "" then purge *.opt
$	if f$search("*.exe") .nes. "" then purge *.exe
$	if f$search("*.log") .nes. "" then purge *.log
$! fallthru
$
$ vms_link_opt :
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
$  else
$   mms/ignore=warning/macro=('comp','mmstar','arch') 'p2
$  endif
$ exit
$ make: subroutine
$	if f$search("''p1'.obj") .eqs. ""
$	then
$		write sys$output "compiling ''p1'"
$		'CC 'CFLAGS 'p1.c
$		if f$search("''p1'.dia") .nes. "" then delete 'p1.dia;*
$	endif
$exit
$	return
$ endsubroutine
