#
# makefile for Watcom C using Watcom MAKE 
# based on the original makefile of vile 3.46 (see the original makefile)
# T.DANG (dang@cogit.ign.fr)
#
# $Header: /users/source/archives/vile.vcs/RCS/makefile.wat,v 1.24 1997/11/26 16:17:33 tom Exp $
#
# if you use the watcom version of vile, you may want to "set DOS4G=quiet"
# to suppress the DOS 4G/W banner that comes up from the Rational Systems
# DOS extender otherwise.
#
# Tested with Watcom 10.0a (95/9/26)

SCREENDEF = DISP_IBMPC
SCREEN= ibmpc 
#SCREEN= ansi 
#SCREENDEF = DISP_ANSI

# for regular compiler (and for version 10.x?)
#CC=wcl386
# for protected mode compiler (and for version 9.x?)
CC=wcl386/p

#define PVGA for Paradise VGA (because there are some problems with this card)
#To fix it, use /dPVGA=1
#CFLAGS= /d$(SCREENDEF)=1 /dscrn_chosen=1 /dPVGA=1 

# debugging
#CFLAGS= /d$(SCREENDEF)=1 /dscrn_chosen=1 /d2
# normal
CFLAGS= /d$(SCREENDEF)=1 /dscrn_chosen=1 /ols 

# these are normal editable headers
HDRS = estruct.h edef.h proto.h dirstuff.h

# these headers are built by the mktbls program from the information in cmdtbl
# and in modetbl
BUILTHDRS = nebind.h neproto.h nefunc.h nemode.h nename.h nevars.h nefkeys.h nefsms.h

SRC = 	main.c $(SCREEN).c &
	basic.c bind.c btree.c buffer.c crypt.c &
	csrch.c display.c eval.c exec.c externs.c &
	fences.c file.c filec.c &
	fileio.c finderr.c glob.c globals.c history.c &
	input.c insert.c itbuff.c isearch.c &
	line.c map.c modes.c msgs.c npopen.c &
	oneliner.c opers.c path.c random.c regexp.c &
	region.c search.c select.c spawn.c &
	tags.c tbuff.c termio.c undo.c &
	version.c window.c word.c wordmov.c

OBJ = 	main.obj $(SCREEN).obj &
	basic.obj bind.obj btree.obj buffer.obj crypt.obj &
      	csrch.obj display.obj eval.obj exec.obj externs.obj &
	fences.obj file.obj filec.obj &
	fileio.obj finderr.obj glob.obj globals.obj history.obj &
	input.obj insert.obj itbuff.obj isearch.obj &
	line.obj map.obj modes.obj msgs.obj npopen.obj &
	oneliner.obj opers.obj path.obj random.obj regexp.obj &
	region.obj search.obj select.obj spawn.obj &
	tags.obj tbuff.obj termio.obj undo.obj &
	version.obj window.obj word.obj wordmov.obj


vile.exe: $(BUILTHDRS) $(OBJ) vile.lnk
	wlink @vile 

vile.lnk: makefile.wat
	echo DEBUG ALL >$^@
        echo NAME vile >>$^@
        echo OPTION MAP >>$^@
        echo OPTION STACK=16384 >>$^@
	for %i in ($(OBJ)) do echo FILE %i >>$^@

$(OBJ):	estruct.h nemode.h edef.h neproto.h proto.h config.h
.c.obj:	.AUTODEPEND
	$(CC) $[* /c $(CFLAGS) 

nebind.h &
nefkeys.h &
nefunc.h &
neproto.h &
nename.h :	cmdtbl MKTBLS.EXE
	MKTBLS.EXE cmdtbl

nevars.h &
nefsms.h &
nemode.h:	modetbl MKTBLS.EXE
	MKTBLS.EXE modetbl

MKTBLS.EXE:  mktbls.c
	$(CC) mktbls.c
	del mktbls.obj

clean:	.SYMBOLIC
	-del *.err
	-del *.obj
	-del vile.lnk
	-del ne*.h
	-del MKTBLS.EXE

bind.obj:	nefunc.h
eval.obj:	nevars.h
exec.obj:	nefunc.h
externs.obj:	nebind.h nename.h neproto.h nefunc.h
filec.obj:	dirstuff.h
glob.obj:	dirstuff.h
globals.obj:	nefunc.h
main.obj:	chgdfunc.h nevars.h
modes.obj:	chgdfunc.h
opers.obj:	nefunc.h
path.obj:	dirstuff.h
random.obj:	nefunc.h
select.obj:	nefunc.h
spawn.obj:	nefunc.h
termio.obj:	nefunc.h
version.obj:	patchlev.h
word.obj:	nefunc.h
