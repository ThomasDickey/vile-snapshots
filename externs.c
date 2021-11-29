/*
 * This is just a place holder -- a place in which to pull in edef.h alone
 *
 * $Id: externs.c,v 1.13 2011/04/07 00:55:07 tom Exp $
 *
 */

#include	"estruct.h"	/* function declarations */

#include	"edef.h"

#define real_CMDFUNCS
#include	"nefunc.h"	/* function declarations */
#include	"nebind.h"	/* default key bindings */
#define real_NAMETBL
#include	"nename.h"	/* name table */

DECL_EXTERN_CONST(int nametbl_size) = TABLESIZE(nametbl);
DECL_EXTERN_CONST(int glbstbl_size) = TABLESIZE(glbstbl);
