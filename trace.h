/*
 * debugging support -- tom dickey.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/trace.h,v 1.22 2005/01/26 23:24:01 tom Exp $
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

/* Initialize SOME data to quiet memory-checking complaints about
 * references to uninitialized data.
 */
#undef init_alloc
#define init_alloc(s,n) memset(s,0,n)

#if	DOALLOC
extern	char *	doalloc (char *oldp, unsigned amount);
extern	char *	do_calloc (unsigned nmemb, unsigned size);
extern	void	dofree (void *oldp);
extern	void	dopoison (void *oldp, unsigned long len);
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

extern	char *	trace_indent(int level, int marker);

extern	int	retrace_code (int);
extern	char *	retrace_string (char *);
extern	void	retrace_void (void);

/*
 * Functions that require types from estruct.h:
 */
#ifdef _estruct_h
extern	char *	itb_visible (ITBUFF *p);
extern	char *	lp_visible (LINE *p);
extern	char *	str_visible (char *p);
extern	char *	str_visible0 (char *p);
extern	char *	tb_visible (TBUFF *p);
extern	char *	visible_buff (const char *p, int length, int eos);
extern	void	trace_buffer (BUFFER *p);
extern	void	trace_line (LINE *p, BUFFER *q);
extern	void	trace_mark (char *name, MARK *mk, BUFFER *bp);
extern	void	trace_region (REGION *rp, BUFFER *bp);
extern	void	trace_window (WINDOW *p);
#endif

#undef TRACE
#if OPT_TRACE
#define TRACE(p) Trace p
#define returnCode(c) return retrace_code(c)
#define returnString(c) return retrace_string(c)
#define returnVoid() { retrace_void(); return; }
#endif

/*
 * Markers to make it simple to construct indented traces:
 */
#define T_CALLED "called {{ "
#define T_RETURN "return }} "
#define T_LENGTH 10

#define TRACE_NULL(s) ((s) != 0 ? (s) : "<null>")

#endif	/* _trace_h */
