/*
 * debugging support -- tom dickey.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/trace.h,v 1.12 2001/03/04 01:26:52 tom Exp $
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
extern	void	dopoison (void *oldp, long len);
#undef	calloc
#define	calloc(n,m)	do_calloc(n, m)
#undef	malloc
#define	malloc(n)	doalloc((char *)0,n)
#undef	realloc
#define	realloc(p,n)	doalloc(p,n)
#undef	free
#define	free(p)		dofree(p)
#ifdef POISON
#undef	poison
#define	poison(p,s)	dopoison(p,s)
#endif
#endif	/* DOALLOC */

#if !(defined(__GNUC__) || defined(__attribute__))
#define __attribute__(p)
#endif

extern	void	fail_alloc (char *msg, char *ptr);
extern	void	Trace ( const char *fmt, ... ) __attribute__ ((format(printf,1,2)));
extern	void	Elapsed ( char * msg);
extern	void	WalkBack (void);

#ifdef _estruct_h
extern	char *	visible_buff (const char *p, int length, int eos);
extern	char *	str_visible (char *p);
extern	char *	lp_visible (LINE *p);
extern	char *	tb_visible (TBUFF *p);
extern	void	trace_line (LINE *p, BUFFER *q);
extern	void	trace_buffer (BUFFER *p);
extern	void	trace_window (WINDOW *p);
#endif

#undef TRACE
#if OPT_TRACE
#define TRACE(p) Trace p
#endif

#endif	/* _trace_h */
