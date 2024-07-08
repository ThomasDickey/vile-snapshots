$! $Id: vmsbuild.com,v 1.53 2024/07/08 08:10:22 tom Exp $
$! VMS build-script for vile.  Requires installed C compiler
$!
$! Screen Configurations
$! ---------------------
$! To build vile, type:
$!        $ @vmsbuild [vile [<compiler> [bld_target]]]
$! To build xvile, type
$!        $ @vmsbuild xvile [<compiler> [bld_target]]
$!
$! where:
$!        <compiler> :== { decc | vaxc }
$!
$! The default compiler on VAX hosts is vaxc, else decc (Alpha hosts).
$!
$! Configuration Note
$! ------------------
$! If you elect to build both vile and xvile from the same source
$! directory, be sure to execute these commands before each build:
$!
$!   $ del *.obj;*
$!   $ del ne*.h;*
$!
$! These commands create a clean build environment for the
$! editor's two different screen configurations (X versus SMG).
$!
$! -----------------------------------------------------------
$ hlp = f$edit("''p1'", "UPCASE")
$ if "''hlp'" .eqs. "HELP" .or. -
        "''hlp'" .eqs. "-H" .or. -
                "''hlp'" .eqs. "-?" .or. -
                        "''hlp'" .eqs. "?" then gosub usage
$ if "''hlp'" .eqs. "" .or. -
     "''hlp'" .eqs. "VILE" .or. -
     "''hlp'" .eqs. "VILE.EXE" .or. -
     "''hlp'" .eqs. "XVILE" .or. -
     "''hlp'" .eqs. "XVILE.EXE" then goto start
$!
$! handle the <bldtarget>
$ gosub 'p1
$ exit
$!
$ start:
$! -----------------------------------------------------------
$! decide if we will use MMS, noting that MMS 3.8 is broken.
$! if you're stuck with a broken MMS, here's where to fix this script.
$ if f$search("SYS$SYSTEM:MMS.EXE").eqs.""
$ then
$	using_mms = 0
$ else
$	using_mms = 1
$ endif
$! -----------------------------------------------------------
$! pickup user's compiler choice, if any
$!
$	gosub parm2_compiler
$! -----------------------------------------------------------
$!      Build the option-file
$!
$ open/write optf vms_link.opt
$ write optf "Identification=""Vile 9.8"""
$ write optf "basic.obj"
$ write optf "bind.obj"
$ write optf "blist.obj"
$ write optf "btree.obj"
$ write optf "buffer.obj"
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
$ write optf "modes.obj"
$ write optf "msgs.obj"
$ write optf "npopen.obj"
$ write optf "nullterm.obj"
$ write optf "oneliner.obj"
$ write optf "opers.obj"
$ write optf "path.obj"
$ write optf "random.obj"
$ write optf "regexp.obj"
$ write optf "region.obj"
$ write optf "search.obj"
$ write optf "select.obj"
$ write optf "spawn.obj"
$ write optf "statevar.obj"
$ write optf "tags.obj"
$ write optf "tbuff.obj"
$ write optf "termio.obj"
$ write optf "ucrypt.obj"
$ write optf "undo.obj"
$ write optf "vl_ctype.obj"
$ write optf "version.obj"
$ write optf "vms2unix.obj"
$ write optf "vmspipe.obj"
$ write optf "watch.obj"
$ write optf "window.obj"
$ write optf "word.obj"
$ write optf "wordmov.obj"
$! ----------------------------------
$! Look for the compiler used and specify architecture.
$!
$!!!	gosub set_compiler
$!!!	gosub set_symbols
$!
$	CC = "CC"
$	arch = "UNKNOWN"
$!
$	if f$getsyi("ARCH_NAME") .eqs. "Alpha"
$	then
$		arch = "__alpha__=1"
$		if "''comp'" .eqs. "" then gosub decc_config
$	endif
$!
$	if f$getsyi("ARCH_NAME") .eqs. "IA64"
$	then
$		arch = "__ia64__=1"
$		if "''comp'" .eqs. "" then gosub decc_config
$	endif
$!
$	if f$getsyi("ARCH_NAME") .eqs. "VAX"
$	then
$		arch = "__vax__=1"
$		if "''comp'" .nes. "" then goto screen_config
$		if f$search("SYS$SYSTEM:VAXC.EXE").nes.""
$		then
$			gosub vaxc_config
$		else
$			if f$search("SYS$SYSTEM:DECC$COMPILER.EXE").nes.""
$			then
$				gosub decc_config
$			else
$				DEFS = ",HAVE_STRERROR"
$				if f$trnlnm("GNU_CC").eqs.""
$				then
$					write sys$output "C compiler required to rebuild vile"
$					close optf
$					exit
$				else
$					write optf "gnu_cc:[000000]gcclib.olb/lib"
$					comp = "__gcc__=1"
$					CC = "GCC"
$				endif
$			endif
$		endif
$	endif
$
$	if "''arch'" .eqs. "UNKNOWN"
$	then
$		write sys$output "Cannot determine architecture type"
$		exit 1
$	endif
$!
$ screen_config:
$!
$	MKTBLS :== $SYS$DISK:'F$DIRECTORY()MKTBLS.EXE	! make a foreign command
$!
$	if "''p1'" .nes. "XVILE" .and. "''p1'" .nes. "XVILE.EXE"
$	then
$! for regular vile, use these:
$		SCREEN := vmsvt
$		TARGET := vile
$		SCRDEF := "DISP_VMSVT,scrn_chosen"
$		mmstar = "__vile__=1"
$	else
$! for building the X toolkit version:
$		SCREEN := x11
$		TARGET := xvile
$		SCRDEF = "NO_WIDGETS,XTOOLKIT,DISP_X11,scrn_chosen"
$		mmstar = "__xvile__=1"
$!
$!  Find out which X-Version we're running.  This will fail for older
$!  VMS versions (i.e., v5.5-1).  Therefore, choose DECWindows XUI for
$!  default.
$!
$		On Error Then GoTo XUI
$		@sys$update:decw$get_image_version sys$share:decw$xlibshr.exe decw$version
$		if f$extract(4,3,decw$version).eqs."1.0"
$		then
$			write optf "Sys$share:DECW$DWTLIBSHR.EXE/Share"
$		endif
$		if f$extract(4,3,decw$version).eqs."1.1"
$		then
$			write optf "menu.obj"
$			write optf "x11menu.obj"
$			write optf "sys$share:decw$xmlibshr.exe/share"
$			write optf "sys$share:decw$xtshr.exe/share"
$			SCRDEF := "MOTIF_WIDGETS,XTOOLKIT,DISP_X11,scrn_chosen"
$			mmstar = "__xmvile__=1"
$			GoTo MAIN2
$		endif
$		if f$extract(4,3,decw$version).eqs."1.2"
$		then
$			write optf "menu.obj"
$			write optf "x11menu.obj"
$			write optf "sys$share:decw$xmlibshr12.exe/share"
$			write optf "sys$share:decw$xtlibshrr5.exe/share"
$			SCRDEF := "MOTIF_WIDGETS,XTOOLKIT,DISP_X11,scrn_chosen"
$			mmstar = "__xmvile__=1"
$			GoTo MAIN2
$		endif
$		GoTo MAIN
$!
$ XUI :
$		write optf "Sys$share:DECW$DWTLIBSHR.EXE/Share"
$!
$ MAIN :
$		write optf "sys$share:decw$xtshr.exe/share"
$		write optf "sys$share:decw$xlibshr.exe/share"
$	endif
$!
$ MAIN2 :
$	if using_vaxc .eq. 1 then write optf "sys$library:vaxcrtl.exe/share"
$		close optf
$! -------------- vms_link.opt is created -------------
$		if f$edit("''p1'", "UPCASE") .eqs. "VMS_LINK.OPT"
$		then
$!  mms called this script to build vms_link.opt.  all done
$			exit
$		endif
$!
$	if using_mms .eq. 0
$	then
$!  can also use /Debug /Listing, /Show=All
$
$		CFLAGS := 'CFLAGS/Diagnostics /Define=("os_chosen","''SCRDEF'''DEFS'") /Include=([])
$
$		if "''p3'" .nes. "" then gosub 'p3
$		if "''p3'" .nes. "" then exit 1
$!
$		gosub all
$!
$	else
$		mms/ignore=warning/macro=('comp','mmstar','arch') 'p3
$	endif
$	exit
$!
$ all:
$	if f$search("mktbls.exe") .eqs. ""
$	then
$		call make mktbls
$!      sys$library:vaxcrtl.olb did not ship with VMS V8.2 HP rx2600 IA64,
$!      nor with layered product HP C V7.1-011 on OpenVMS IA64 V8.2
$               if f$search("SYS$LIBRARY:VAXCRTL.OLB") .nes. ""
$               then
$                       link /exec=mktbls/map/cross mktbls.obj,SYS$LIBRARY:VAXCRTL/LIB
$               else
$                       link /exec=mktbls/map/cross mktbls.obj
$               endif
$	endif
$	if f$search("nebind.h") .eqs. "" then mktbls cmdtbl
$	if f$search("nemode.h") .eqs. "" then mktbls modetbl
$!
$	call make main
$	call make 'SCREEN
$	call make basic
$	call make bind
$	call make blist
$	call make btree
$	call make buffer
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
$	if "''mmstar'" .eqs. "__xmvile__=1" then call make menu
$	if "''mmstar'" .eqs. "__xmvile__=1" then call make x11menu
$	call make modes
$	call make msgs
$	call make npopen
$	call make nullterm
$	call make oneliner
$	call make opers
$	call make path
$	call make random
$	call make regexp
$	call make region
$	call make search
$	call make select
$	call make spawn
$	call make statevar
$	call make tags
$	call make tbuff
$	call make termio
$	call make ucrypt
$	call make undo
$	call make version
$	call make vms2unix
$	call make vmspipe
$	call make watch
$	call make window
$	call make word
$	call make wordmov
$!
$	link /exec='target/map/cross main.obj, 'SCREEN.obj, vms_link/opt
$	gosub build_last
$	return
$!
$ install:
$	WRITE SYS$ERROR "** no rule for install"
$	gosub build_last
$	return
$!
$ clobber :
$	write sys$output "clobbering build products"
$	if f$search("vile.com") .nes. "" then delete vile.com;*
$	if f$search("xvile.com") .nes. "" then delete xvile.com;*
$	if f$search("*.exe") .nes. "" then delete *.exe;*
$	gosub clean
$	return
$!
$ clean:
$	write sys$output "cleaning build by-products"
$	if f$search("*.obj") .nes. "" then delete *.obj;*
$	if f$search("*.bak") .nes. "" then delete *.bak;*
$	if f$search("*.lis") .nes. "" then delete *.lis;*
$	if f$search("*.log") .nes. "" then delete *.log;*
$	if f$search("*.map") .nes. "" then delete *.map;*
$	if f$search("*.opt") .nes. "" then delete *.opt;*
$	if f$search("ne*.h") .nes. "" then delete ne*.h;*
$	if f$search("$(MKTBLS)") .nes. "" then delete $(MKTBLS);
$	gosub build_last
$	return
$!
$ build_last :
$	if f$search("*.dia") .nes. "" then delete *.dia;*
$	if f$search("*.lis") .nes. "" then purge *.lis
$	if f$search("*.obj") .nes. "" then purge *.obj
$	if f$search("*.map") .nes. "" then purge *.map
$	if f$search("*.opt") .nes. "" then purge *.opt
$	if f$search("*.exe") .nes. "" then purge *.exe
$	if f$search("*.log") .nes. "" then purge *.log
$	return
$!
$ vms_link_opt :
$	exit 1
$!
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
$	return
$!
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
$	return
$!
$! Test-drivers
$ test_io :
$	gosub parm2_compiler 'p2
$	gosub set_compiler
$	gosub set_symbols
$!
$	call make test_io
$	call make vmsvt
$	call make nullterm
$	call linkit test_io test_io.obj,vmsvt.obj,nullterm.obj
$!
$	return
$! --------------------------------
$! Set compiler symbol "comp" based on 2nd parameter.
$ parm2_compiler:
$	comp = ""
$	using_vaxc = 0
$	if "''p2'" .nes. ""
$	then
$		comp = f$edit(p2, "UPCASE")
$		if "''comp'" .eqs. "VAXC"
$		then
$			gosub vaxc_config
$		else
$			if "''comp'" .eqs. "DECC"
$			then
$				gosub decc_config
$			else
$				gosub usage
$			endif
$		endif
$	endif
$	return
$! ----------------------------------
$! Look for the compiler used and specify architecture.
$ set_compiler:
$	CC = "CC"
$	arch = "UNKNOWN"
$!
$	if f$getsyi("ARCH_NAME") .eqs. "Alpha"
$	then
$		arch = "__alpha__=1"
$		if "''comp'" .eqs. "" then gosub decc_config
$	endif
$!
$	if f$getsyi("ARCH_NAME") .eqs. "IA64"
$	then
$		arch = "__ia64__=1"
$		if "''comp'" .eqs. "" then gosub decc_config
$	endif
$!
$	if f$getsyi("ARCH_NAME") .eqs. "VAX"
$	then
$		arch = "__vax__=1"
$		if "''comp'" .nes. "" then goto done_compiler
$		if f$search("SYS$SYSTEM:VAXC.EXE").nes.""
$		then
$			gosub vaxc_config
$		else
$			if f$search("SYS$SYSTEM:DECC$COMPILER.EXE").nes.""
$			then
$				gosub decc_config
$			else
$				DEFS = ",HAVE_STRERROR"
$				if f$trnlnm("GNU_CC").eqs.""
$				then
$					write sys$output "C compiler required to rebuild vile"
$					exit
$				else
$					comp = "__gcc__=1"
$					CC = "GCC"
$				endif
$			endif
$		endif
$	endif
$
$	if "''arch'" .eqs. "UNKNOWN"
$	then
$		write sys$output "Cannot determine architecture type"
$		exit 1
$	endif
$ done_compiler:
$	return
$! ----------------------------------
$! Look for the compiler used and specify architecture.
$ set_symbols:
$	MKTBLS :== $SYS$DISK:'F$DIRECTORY()MKTBLS.EXE	! make a foreign command
$!
$	if "''p1'" .eqs. "XVILE" .or. "''p1'" .eqs. "XVILE.EXE"
$	then
$! for building the X toolkit version:
$		SCREEN := x11
$		TARGET := xvile
$		SCRDEF = "NO_WIDGETS,XTOOLKIT,DISP_X11,scrn_chosen"
$		mmstar = "__xvile__=1"
$!
$!  Find out which X-Version we're running.  This will fail for older
$!  VMS versions (i.e., v5.5-1).  Therefore, choose DECWindows XUI for
$!  default.
$!
$		On Error Then GoTo done_symbol
$		@sys$update:decw$get_image_version sys$share:decw$xlibshr.exe decw$version
$		if f$extract(4,3,decw$version).eqs."1.1"
$		then
$			SCRDEF := "MOTIF_WIDGETS,XTOOLKIT,DISP_X11,scrn_chosen"
$			mmstar = "__xmvile__=1"
$		endif
$		if f$extract(4,3,decw$version).eqs."1.2"
$		then
$			SCRDEF := "MOTIF_WIDGETS,XTOOLKIT,DISP_X11,scrn_chosen"
$			mmstar = "__xmvile__=1"
$		endif
$	else
$! for regular vile, use these:
$		SCREEN := vmsvt
$		TARGET := vile
$		SCRDEF := "DISP_VMSVT,scrn_chosen"
$		mmstar = "__vile__=1"
$	endif
$!  can also use /Debug /Listing, /Show=All
$	CFLAGS := 'CFLAGS/Diagnostics /Define=("os_chosen","''SCRDEF'''DEFS'") /Include=([])
$	if using_mms .eq. 0
$	then
$		BUILDS = "with_dcl"
$	else
$		BUILDS = "with_mms"
$	endif
$ done_symbol :
$	return
$!
$ make: subroutine
$	if f$search("''p1'.obj") .eqs. ""
$	then
$		write sys$output "compiling ''p1'"
$		'CC 'CFLAGS 'p1.c
$		if f$search("''p1'.dia") .nes. "" then delete 'p1.dia;*
$	endif
$	exit
$ endsubroutine
$!
$ linkit: subroutine
$!      sys$library:vaxcrtl.olb did not ship with VMS V8.2 HP rx2600 IA64,
$!      nor with layered product HP C V7.1-011 on OpenVMS IA64 V8.2
$	if f$search("SYS$LIBRARY:VAXCRTL.OLB") .nes. ""
$	then
$		link /exec='p1/map/cross 'p2,SYS$LIBRARY:VAXCRTL/LIB
$	else
$		link /exec='p1/map/cross 'p2
$	endif
$	exit
$ endsubroutine
$!
$ vaxc_config:
$    comp       = "__vaxc__=1"
$    CFLAGS     = "/VAXC"
$    DEFS       = ",HAVE_STRERROR"
$    using_vaxc = 1
$    if f$trnlnm("SYS") .eqs. "" then define sys sys$library:
$    return
$!
$ decc_config:
$	comp   = "__decc__=1"
$	if f$getsyi("ARCH_NAME") .eqs. "Alpha"
$	then
$		CFLAGS = "/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES"
$		DEFS   = ",HAVE_ALARM,HAVE_STRERROR"
$	else
$		if f$getsyi("ARCH_NAME") .eqs. "IA64"
$		then
$			CFLAGS = "/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES"
$			DEFS   = ",HAVE_ALARM,HAVE_STRERROR,USE_IEEE_FLOAT"
$		else
$			CFLAGS = "/DECC/prefix=all"
$			DEFS   = ",HAVE_ALARM,HAVE_STRERROR"
$		endif
$	endif
$	if f$trnlnm("SYS") .eqs."" then define sys decc$library_include:
$	return
$!
$ usage:
$    write sys$output "usage: "
$    write sys$output "      $ @vmsbuild [vile [{decc | vaxc} [<bldtarget>]]]"
$    write sys$output "                   or"
$    write sys$output "      $ @vmsbuild xvile [{decc | vaxc} [<bldtarget>]]"
$    write sys$output "bldtarget is one of"
$    write sys$output "      all install clobber clean"
$    write sys$output "      vile_com xvile_com"
$    write sys$output "      test_io test_btree test_regexp"
$    exit 2
