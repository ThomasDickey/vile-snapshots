/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/flt_defs.h,v 1.14 2009/10/07 23:30:31 tom Exp $
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

#define	NonNull(s)	((s == 0) ? "" : s)
#define isEmpty(s)	((s) == 0 || *(s) == EOS)

#define EOS        '\0'

/* If we are using built-in filters, we can use many definitions from estruct.h
 * that may resolve to functions in ../vile
 */
#if OPT_FILTER

#include <estruct.h>

#else

#ifndef NO_LEAKS
#define NO_LEAKS 0
#endif

#ifndef OPT_TRACE
#define OPT_TRACE 0
#endif

#ifndef CAN_TRACE
#define CAN_TRACE		OPT_TRACE  /* (link with trace.o) */
#endif

#ifndef SMALLER
#define SMALLER 0
#endif

#include <sys/types.h>		/* sometimes needed to get size_t */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#else
# if !defined(HAVE_CONFIG_H) || defined(MISSING_EXTERN_MALLOC)
extern	char *	malloc	( size_t len );
# endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <vl_stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef MISSING_EXTERN__FILBUF
extern	int	_filbuf	( FILE *fp );
#endif

#ifdef MISSING_EXTERN__FLSBUF
extern	int	_flsbuf	( int len, FILE *fp );
#endif

#ifdef MISSING_EXTERN_FCLOSE
extern	int	fclose	( FILE *fp );
#endif

#ifdef MISSING_EXTERN_FPRINTF
extern	int	fprintf	( FILE *fp, const char *fmt, ... );
#endif

#ifdef MISSING_EXTERN_FPUTS
extern	int	fputs	( const char *s, FILE *fp );
#endif

#ifdef MISSING_EXTERN_PRINTF
extern	int	printf	( const char *fmt, ... );
#endif

#ifdef MISSING_EXTERN_SSCANF
extern	int	sscanf	( const char *src, const char *fmt, ... );
#endif


#if defined(VMS)
#include	<stsdef.h>
#define GOODEXIT	(STS$M_INHIB_MSG | STS$K_SUCCESS)
#define BADEXIT		(STS$M_INHIB_MSG | STS$K_ERROR)
#else
#if defined(EXIT_SUCCESS) && defined(EXIT_FAILURE)
#define GOODEXIT	EXIT_SUCCESS
#define BADEXIT		EXIT_FAILURE
#else
#define GOODEXIT	0
#define BADEXIT		1
#endif
#endif

#ifndef GCC_UNUSED
#define GCC_UNUSED /*nothing*/
#endif

#ifndef VILE_PRINTF
#define VILE_PRINTF(a,b) /*nothing*/
#endif

#define BACKSLASH '\\'

#define	TABLESIZE(v)	(sizeof(v)/sizeof(v[0]))
#define NONNULL(s)	((s) != 0) ? (s) : "<null>"
#define isBlank(c)	((c) == ' ' || (c) == '\t')

#define	typealloc(cast)			(cast *)malloc(sizeof(cast))
#define	typeallocn(cast,ntypes)		(cast *)malloc((ntypes)*sizeof(cast))
#define	typecallocn(cast,ntypes)	(cast *)calloc(sizeof(cast),ntypes)
#define	typereallocn(cast,ptr,ntypes)	(cast *)realloc((char *)(ptr),\
							(ntypes)*sizeof(cast))

#define	FreeAndNull(p)	if ((p) != 0)	{ free(p); p = 0; }
#define	FreeIfNeeded(p)	if ((p) != 0)	free(p)

#if defined(__GNUC__) && defined(_FORTIFY_SOURCE)
extern int ignore_unused;
#define IGNORE_RC(func) ignore_unused = func
#else
#define IGNORE_RC(func) (void) func
#endif /* gcc workarounds */

extern	char *home_dir(void);

typedef enum { D_UNKNOWN = -1, D_ENDM = 0 } DIRECTIVE;
extern DIRECTIVE dname_to_dirnum(char **cmdp, size_t length);

typedef struct { int dummy; } CMDFUNC;
extern const CMDFUNC * engl2fnc(const char *fname);

#if NO_LEAKS
extern	void	filters_leaks (void);
extern	void	flt_leaks (void);
#endif

#endif /* OPT_FILTER */
#endif /* _estruct_h */

#if OPT_LOCALE
#include <locale.h>
#endif

#include <ctype.h>

#ifdef __cplusplus
}
#endif

#endif /* FLT_DEFS_H */
