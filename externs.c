/*
 * This is just a place holder -- a place in which to pull in edef.h alone
 *
 * $Header: /users/source/archives/vile.vcs/RCS/externs.c,v 1.11 2007/08/13 21:56:18 tom Exp $
 *
 */

#include	"estruct.h"	/* function declarations */

#include	"edef.h"

#define real_CMDFUNCS
#include	"nefunc.h"	/* function declarations */
#include	"nebind.h"	/* default key bindings */
#define real_NAMETBL
#include	"nename.h"	/* name table */

DECL_EXTERN_CONST(int nametblsize) = TABLESIZE(nametbl);
