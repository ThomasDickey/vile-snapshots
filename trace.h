/*
 * debugging support -- tom dickey.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/trace.h,v 1.5 1998/02/23 11:27:56 tom Exp $
 *
 */
#ifndef	_trace_h
#define	_trace_h

#ifndef	NO_LEAKS
#define NO_LEAKS 0
#endif

#ifndef DOALLOC
#define DOALLOC 0
#endif

#ifndef	show_alloc
extern	void	show_alloc (void);
#endif

#if	DOALLOC
extern	char *	doalloc (char *oldp, unsigned amount);
extern	char *	do_calloc (unsigned nmemb, unsigned size);
extern	void	dofree (void *oldp);
#undef	calloc
#define	calloc(n,m)	do_calloc(n, m)
#undef	malloc
#define	malloc(n)	doalloc((char *)0,n)
#undef	realloc
#define	realloc(p,n)	doalloc(p,n)
#undef	free
#define	free(p)		dofree(p)
#endif	/* DOALLOC */

#if !(defined(__GNUC__) || defined(__attribute__))
#define __attribute__(p)
#endif

extern	void	fail_alloc (char *msg, char *ptr);
extern	void	Trace ( const char *fmt, ... ) __attribute__ ((format(printf,1,2)));
extern	void	Elapsed ( char * msg);
extern	void	WalkBack (void);

#ifdef _estruct_h
extern	char *	tb_visible (TBUFF *p);
#endif

#undef TRACE
#define TRACE(p) Trace p;

#endif	/* _trace_h */
