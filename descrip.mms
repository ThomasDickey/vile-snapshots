#
# VMS makefile for vile.  Requires "MMS"
#
# Tested with:
#	VMS system version 5.4-2
#	MMS version 2.6
#	VAX-C version 3.2
#
# To change screen driver modules, change SCREEN and SCRDEF below, OR edit
# estruct.h to make sure the correct one is #defined as "1", and the others
# all as "0".  If you use tcap.c, you'll need libtermcap.a too.  If you use
# x11.c, you'll need libX11.a too.
#
# $Header: /users/source/archives/vile.vcs/RCS/descrip.mms,v 1.39 1999/03/26 02:06:08 tom Exp $

.IFDEF __XVILE__

# for building the X-toolkit version:
SCREEN = x11
LIBS = #-lX11
TARGET = xvile.exe
SCRDEF = "NO_WIDGETS","XTOOLKIT","DISP_X11","scrn_chosen"

# for building the Motif version (untested):
#SCREEN = x11
#LIBS = #-lX11
#TARGET = xvile.exe
#SCRDEF = "MOTIF_WIDGETS","XTOOLKIT","DISP_X11","scrn_chosen"

.ELSE
# for regular vile, use these:
SCREEN = vmsvt
LIBS =
TARGET = vile.exe
SCRDEF = "DISP_VMSVT","scrn_chosen"
.ENDIF

LINKFLAGS = /MAP=$(MMS$TARGET_NAME)/CROSS_REFERENCE/EXEC=$(MMS$TARGET_NAME).EXE

INCS = []

MKTBLS = mktbls.EXE


SRC =	main.c \
	$(SCREEN).c \
	basic.c \
	bind.c \
	btree.c \
	buffer.c \
	csrch.c \
	display.c \
	dumbterm.c \
	eval.c \
	exec.c \
	externs.c \
	fences.c \
	file.c \
	filec.c \
	fileio.c \
	finderr.c \
	glob.c \
	globals.c \
	history.c \
	input.c \
	insert.c \
	itbuff.c \
	isearch.c \
	line.c \
	map.c \
	menu.c \
	modes.c \
	msgs.c \
	npopen.c \
	oneliner.c \
	opers.c \
	path.c \
	random.c \
	regexp.c \
	region.c \
	search.c \
	select.c \
	spawn.c \
	statevar.c \
	tags.c \
	tbuff.c \
	termio.c \
	ucrypt.c \
	undo.c \
	version.c \
	vms2unix.c \
	vmspipe.c \
	watch.c \
	window.c \
	word.c \
	wordmov.c

OBJ =	main.obj,\
	$(SCREEN).obj,\
	basic.obj,\
	bind.obj,\
	btree.obj,\
	buffer.obj,\
	csrch.obj,\
	display.obj,\
	dumbterm.obj,\
	eval.obj,\
	exec.obj,\
	externs.obj,\
	fences.obj,\
	file.obj,\
	filec.obj,\
	fileio.obj,\
	finderr.obj,\
	glob.obj, \
	globals.obj,\
	history.obj,\
	input.obj,\
	insert.obj,\
	itbuff.obj,\
	isearch.obj,\
	line.obj,\
	map.obj, \
	menu.obj,\
	modes.obj,\
	msgs.obj,\
	npopen.obj,\
	oneliner.obj,\
	opers.obj,\
	path.obj,\
	random.obj,\
	regexp.obj,\
	region.obj,\
	search.obj,\
	select.obj,\
	spawn.obj,\
	statevar.obj,\
	tags.obj,\
	tbuff.obj,\
	termio.obj,\
	ucrypt.obj,\
	undo.obj,\
	version.obj, \
	vms2unix.obj,\
	vmspipe.obj,\
	watch.obj,\
	window.obj,\
	word.obj,\
	wordmov.obj

all :

        $(MMS)$(MMSQUALIFIERS) $(TARGET)

#
# I've built on an Alpha with CC_OPTIONS set to
#	CC_OPTIONS = /STANDARD=VAXC		(for VAX-C compatibility)
#	CC_OPTIONS = /PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES	(DEC-C)
# The latter (DEC-C) gives better type-checking -- T.Dickey
#
# But on a VAX, the DEC-C compiler and/or its run-time library cause
# vile to change the "Revised time" of every file the editor _reads_.
# You won't like this when using mms or a make clone.  The VAXC
# compiler does not suffer from this problem.
#
# Configuration where problem was observed:
#                DEC C V5.6-003 on OpenVMS VAX V7.1
#                                                    --C. Morgan
#
.IFDEF __ALPHA__
CC_OPTIONS = /PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES
CC_DEFS = ,HAVE_ALARM
.ELSE
.IFDEF __DECC__
CC_OPTIONS = /DECC /PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES
CC_DEFS = ,HAVE_ALARM
.ELSE
CC_OPTIONS = /VAXC
CC_DEFS = ,HAVE_SYS_ERRLIST
.ENDIF
.ENDIF

nebind.h \
nefkeys.h \
nefunc.h \
nename.h :	cmdtbl $(MKTBLS)
	MKTBLS cmdtbl

nevars.h \
nemode.h :	modetbl $(MKTBLS)
	MKTBLS modetbl

# install to DESTDIR1 if it's writable, else DESTDIR2
install :
	@ WRITE SYS$ERROR "** no rule for $@"

clean :
	@- if f$search("*.obj") .nes. "" then delete *.obj;*
	@- if f$search("*.bak") .nes. "" then delete *.bak;*
	@- if f$search("*.lis") .nes. "" then delete *.lis;*
	@- if f$search("*.log") .nes. "" then delete *.log;*
	@- if f$search("*.map") .nes. "" then delete *.map;*
	@- if f$search("*.opt") .nes. "" then delete *.opt;*
	@- if f$search("ne*.h") .nes. "" then delete ne*.h;*
	@- if f$search("$(MKTBLS)") .nes. "" then delete $(MKTBLS);

clobber : clean
	@- if f$search("vile.com") .nes. "" then delete vile.com;*
	@- if f$search("xvile.com") .nes. "" then delete xvile.com;*
	@- if f$search("*.exe") .nes. "" then delete *.exe;*

$(OBJ) : estruct.h nemode.h nefkeys.h edef.h proto.h

bind.obj :	nefunc.h
eval.obj :	nevars.h
exec.obj :	nefunc.h
externs.obj :	nebind.h nename.h neproto.h nefunc.h
filec.obj :	dirstuff.h
glob.obj :	dirstuff.h
globals.obj :	nefunc.h
main.obj :	chgdfunc.h nevars.h
modes.obj :	chgdfunc.h
opers.obj :	nefunc.h
path.obj :	dirstuff.h
random.obj :	nefunc.h
select.obj :	nefunc.h
spawn.obj :	nefunc.h
termio.obj :	nefunc.h
version.obj :	patchlev.h
vms2unix.obj :	dirstuff.h
word.obj :	nefunc.h

.first :
	@ MKTBLS :== $SYS$DISK:'F$DIRECTORY()$(MKTBLS)	! make a foreign command

.last :
	@- if f$search("*.dia") .nes. "" then delete *.dia;*
	@- if f$search("*.lis") .nes. "" then purge *.lis
	@- if f$search("*.obj") .nes. "" then purge *.obj
	@- if f$search("*.map") .nes. "" then purge *.map
	@- if f$search("*.exe") .nes. "" then purge *.exe
	@- if f$search("*.log") .nes. "" then purge *.log
	@- if f$search("*.opt") .nes. "" then purge *.opt

# used /G_FLOAT with vaxcrtlg/share in vms_link.opt
# can also use /Debug /Listing, /Show=All
CFLAGS =-
	$(CC_OPTIONS)/Diagnostics /Define=("os_chosen",$(SCRDEF)$(CC_DEFS)) -
	/Object=$@ /Include=($(INCS))

.C.OBJ :
	$(CC) $(CFLAGS) $(MMS$SOURCE)
	@- delete $(MMS$TARGET_NAME).dia;*

$(MKTBLS) : mktbls.obj $(OPTFILE)
	$(LINK) $(LINKFLAGS) mktbls.obj $(OPTIONS)

$(TARGET) : $(OBJ), vms_link.opt, descrip.mms $(OPTFILE)
	$(LINK) $(LINKFLAGS) main.obj, $(SCREEN).obj, vms_link/opt

vms_link.opt :
	@vmsbuild vms_link_opt

# Runs VILE from the current directory (used for testing)
vile.com :
	@- if "''f$search("$@")'" .nes. "" then delete $@;*
	@- copy nl: $@
	@ open/append  test_script $@
	@ write test_script "$ temp = f$environment(""procedure"")"
	@ write test_script "$ temp = temp -"
	@ write test_script "		- f$parse(temp,,,""version"",""syntax_only"") -"
	@ write test_script "		- f$parse(temp,,,""type"",""syntax_only"")"
	@ write test_script "$ vile :== $ 'temp'.exe"
	@ write test_script "$ define/user_mode sys$input  sys$command"
	@ write test_script "$ define/user_mode sys$output sys$command"
	@ write test_script "$ vile 'p1 'p2 'p3 'p4 'p5 'p6 'p7 'p8"
	@ close test_script
	@ write sys$output "** made $@"

# Runs XVILE from the current directory (used for testing)
xvile.com :
	@- if "''f$search("$@")'" .nes. "" then delete $@;*
	@- copy nl: $@
	@ open/append  test_script $@
	@ write test_script "$ temp = f$environment(""procedure"")"
	@ write test_script "$ temp = temp -"
	@ write test_script "		- f$parse(temp,,,""name"",""syntax_only"") -"
	@ write test_script "		- f$parse(temp,,,""version"",""syntax_only"") -"
	@ write test_script "		- f$parse(temp,,,""type"",""syntax_only"")"
	@ write test_script "$ xvile :== $ 'temp'xvile.exe"
	@ write test_script "$ define/user_mode sys$input  sys$command"
	@ write test_script "$ define/user_mode sys$output sys$command"
	@ write test_script "$ xvile 'p1 'p2 'p3 'p4 'p5 'p6 'p7 'p8"
	@ close test_script
	@ write sys$output "** made $@"

