/*
 * This is just a place holder -- a place in which to pull in edef.h alone
 *
 * $Header: /users/source/archives/vile.vcs/RCS/externs.c,v 1.10 2004/12/02 01:12:54 tom Exp $
 *
 */

#include	"estruct.h"	/* function declarations */

#include	"edef.h"

#define real_CMDFUNCS
#include	"nefunc.h"	/* function declarations */
#include	"nebind.h"	/* default key bindings */
#include	"nename.h"	/* name table */

DECL_EXTERN_CONST(int nametblsize) = TABLESIZE(nametbl);
