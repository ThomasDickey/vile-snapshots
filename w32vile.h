/*
 * $Header: /users/source/archives/vile.vcs/RCS/w32vile.h,v 1.2 2001/12/24 15:03:20 tom Exp $
 *
 * Do the pragmas in a separate file to avoid contaminating portable code.
 */
#ifndef W32VILE_H_incl
#define W32VILE_H_incl 1

#pragma warning (disable : 4100) /* unreferenced formal parameter */
#pragma warning (disable : 4127) /* conditional expression is constant */
#pragma warning (disable : 4201) /* nameless struct/union */
#pragma warning (disable : 4214) /* bit field types other than int */
#pragma warning (disable : 4310) /* cast truncates constant value */
#pragma warning (disable : 4514) /* unreferenced inline function has been removed */

#include <windows.h>

#endif /* W32VILE_H_incl */
