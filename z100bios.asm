;History:46,1
; $Header: /users/source/archives/vile.vcs/RCS/z100bios.asm,v 1.4 1994/07/11 22:56:20 pgf Exp $
#error This module is not actively maintained as part of vile.
#error It can likely be made to work without much difficulty, but unless
#error  I know someone is using it, i have little incentive to fix it.
#error  If you use it when you build vile, please let me know.  -pgf

_TEXT	SEGMENT  BYTE PUBLIC 'CODE'
_TEXT	ENDS
_DATA	SEGMENT  WORD PUBLIC 'DATA'
_DATA	ENDS
CONST	SEGMENT  WORD PUBLIC 'CONST'
CONST	ENDS
_BSS	SEGMENT  WORD PUBLIC 'BSS'
_BSS	ENDS

bios_seg segment at 40h
	org	9
bios_conout	label	far
bios_seg ends

DGROUP	GROUP	CONST,	_BSS,	_DATA
	ASSUME  CS: _TEXT, DS: DGROUP, SS: DGROUP, ES: DGROUP

parm		equ	ss:[bp]

_TEXT	SEGMENT

 public		_asmputc

putc_stack		struc
 putc_bp	dw	?
 putc_return	dd	?
 char		db	?
putc_stack		ends

_asmputc	proc	far
		push	bp
		mov	bp,sp
		mov	al,parm.char
		call	bios_conout
		pop	bp
		ret
_asmputc	endp

_TEXT		ends
		end
