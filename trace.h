/*
 * debugging support -- tom dickey.
 *
 * $Id: trace.h,v 1.40 2025/01/26 17:03:16 tom Exp $
 */
#ifndef	_trace_h
#define	_trace_h

#ifndef GCC_PRINTFLIKE
#ifdef GCC_PRINTF
#define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))
#else
#define GCC_PRINTFLIKE(fmt,var) /*nothing*/
#endif
#endif /* GCC_PRINTFLIKE */

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
extern	void *	doalloc (void *oldp, unsigned amount);
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
extern	void	Trace ( const char *fmt, ... ) GCC_PRINTFLIKE(1,2);

extern	char *	alloc_indent(int level, int marker);

extern char *   retrace_string (char *);
extern const char * retrace_cstring (const char *);
extern int      retrace_code (int);
extern void *   retrace_ptr (void *);
extern void     retrace_void (void);

/*
 * Functions that require types from estruct.h:
 */
#ifdef _estruct_h
extern	char *	itb_visible (ITBUFF *p);
extern	char *	lp_visible (LINE *p);
extern	char *	str_visible (const char *p);
extern	char *	str_visible0 (const char *p);
extern	char *	tb_visible (TBUFF *p);
extern	char *	visible_buff (const char *p, int length, int eos);
extern	char *	visible_video_attr (const VIDEO_ATTR *p, int length);
extern	char *	visible_video_text (const VIDEO_TEXT *p, int length);
extern	void	trace_attribs (BUFFER *p, char *fn, int ln);
extern	void	trace_buffer (BUFFER *p);
extern	void	trace_ctype (CHARTYPE *table, int first, int last);
extern	void	trace_line (LINE *p, BUFFER *q);
extern	void	trace_mark (const char *name, MARK *mk, BUFFER *bp);
extern	void	trace_region (REGION *rp, BUFFER *bp);
extern	void	trace_window (WINDOW *p);
#endif

extern void trace_all_buffers(const char *fn, int ln);
extern void trace_all_windows(const char *fn, int ln);

#undef TRACE
#if OPT_TRACE
#define TRACE(p) Trace p
#define returnCode(c)    return retrace_code(c)
#define returnCString(c) return retrace_cstring(c)
#define returnPtr(c)     return retrace_ptr(c)
#define returnString(c)  return retrace_string(c)
#define returnVoid()     { retrace_void(); return; }
#endif

/*
 * Markers to make it simple to construct indented traces:
 */
#define T_CALLED "called {{ "
#define T_RETURN "return }} "
#define T_LENGTH 10

#define TRACE_NULL(s) ((s) != NULL ? (s) : "<null>")

#define TRACE_CMDFUNC(p) \
	   ((p)->c_flags & CMD_PERL \
	    ? "perl" \
	    : ((p)->c_flags & CMD_PROC \
	       ? "proc" \
	       : "func")), \
	   ((p)->c_name \
	    ? (p)->c_name \
	    : "?")

#define TRACE_BINDINGS(p) \
	    (p) == &dft_bindings ? "default" : \
	    (p) == &sel_bindings ? "select" : \
	    (p) == &ins_bindings ? "insert" : \
	    (p) == &cmd_bindings ? "command" : \
	    "?"

#endif	/* _trace_h */
