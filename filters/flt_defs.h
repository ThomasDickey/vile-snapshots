/*
 * $Id: flt_defs.h,v 1.23 2025/01/26 15:00:32 tom Exp $
 */

#ifndef FLT_DEFS_H
#define FLT_DEFS_H 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _estruct_h

#ifdef HAVE_CONFIG_H
#include <config.h>
#else
/* assume ANSI C */
# define HAVE_STDLIB_H 1
# define HAVE_STRING_H 1
#endif

#ifndef OPT_FILTER
#define OPT_FILTER 0
#endif

#ifndef OPT_LOCALE
#define OPT_LOCALE 0
#endif

#define	NonNull(s)	((s == NULL) ? "" : s)
#define isEmpty(s)	((s) == NULL || *(s) == EOS)

#define EOS        '\0'

/* If we are using built-in filters, we can use many definitions from estruct.h
 * that may resolve to functions in ../vile
 */
#include <estruct.h>

#endif				/* _estruct_h */

#if OPT_LOCALE
#include <locale.h>
#endif

#include <ctype.h>

#ifndef TRACE
#define TRACE(params)		/* nothing */
#endif

#ifdef __cplusplus
}
#endif
#endif				/* FLT_DEFS_H */
