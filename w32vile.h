/*
 * $Header: /users/source/archives/vile.vcs/RCS/w32vile.h,v 1.4 2006/04/25 22:21:20 tom Exp $
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

#if (_MSC_VER >= 1300) && (_MSC_VER < 1400)
#define CWAIT_PARAM_TYPE   intptr_t
#define DIALOG_PROC_RETVAL INT_PTR	/* but it still returns TRUE or FALSE */
#define SETTIMER_RETVAL    UINT_PTR
#define WINDOW_PROC_RETVAL LRESULT
#else
#define CWAIT_PARAM_TYPE   int
#define DIALOG_PROC_RETVAL BOOL
#define SETTIMER_RETVAL    UINT
#define WINDOW_PROC_RETVAL LONG
#endif

#endif /* W32VILE_H_incl */
