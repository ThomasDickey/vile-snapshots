/*
 * This is just a place holder -- a place in which to pull in edef.h alone
 *
 * $Header: /users/source/archives/vile.vcs/RCS/externs.c,v 1.7 1998/07/23 09:19:56 cmorgan Exp $
 *
 */

#ifdef _WIN32
# include <windows.h>
#endif

#include	"estruct.h"	/* function declarations */

#define real_CMDFUNCS
#include	"nefunc.h"	/* function declarations */
#include	"nebind.h"	/* default key bindings */
#include	"nename.h"	/* name table */

EXTERN_CONST int nametblsize = TABLESIZE(nametbl);

