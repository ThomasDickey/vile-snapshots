/*
 * debugging support -- tom dickey.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/trace.c,v 1.77 2008/01/22 00:13:57 tom Exp $
 *
 */

#include <estruct.h>
#include <edef.h>

#include <ctype.h>

#if OPT_ELAPSED

#if SYS_UNIX
typedef struct timeval ElapsedType;
#define BI_DATA0 {0, 0}
#elif SYS_WINNT
typedef DWORD ElapsedType;
#define BI_DATA0 0
#else
typedef time_t ElapsedType;
#define BI_DATA0 0
#endif

typedef struct {
    long total_calls;
    double total_time;
    ElapsedType this_time;	/* origin of begin_elapsed()    */
    int nesting_time;		/* recursion-guard */
} ELAPSED_DATA;

#define BI_DATA MY_BI_DATA

typedef struct {
    char *bi_key;		/* the name of the command      */
    ELAPSED_DATA *data;		/* actual data                  */
} BI_DATA;

#include <btree.h>
#endif

#if SYS_WINNT
#include <io.h>
#include <fcntl.h>
#endif

#undef fopen			/* avoid conflict with 'fakevms.c' */

#ifndef _trace_h
#include "trace.h"
#endif

#if SYS_UNIX
#include <sys/time.h>
#endif

#if	defined(sun) && !defined(__SVR4)
extern void perror(const char *s);
extern int gettimeofday(struct timeval *t, struct timezone *z);
extern int vfprintf(FILE *fp, const char *fmt, va_list v);
extern int fflush(FILE *fp);
#endif

#include <stdarg.h>

#if	DOALLOC
#undef	calloc
#undef	malloc
#undef	realloc
#undef	free
#endif /* DOALLOC */

/*
 * If we implement OPT_WORKING, this means we have a timer periodically
 * interrupting the program to write a "working..." message.  Some systems do
 * not work properly, especially if malloc/free are interrupted.  We use
 * beginDisplay/endofDisplay markers to delimit critical regions where we
 * should not be interrupted.
 */
#if OPT_WORKING
#define check_opt_working()	assert(!allow_working_msg())
#else
#define check_opt_working()	/* nothing */
#endif

static const char *bad_form;	/* magic address for fail_alloc() */

static char *visible_result;
static char *visible_indent;
static size_t used_visible;
static unsigned used_indent;

static int trace_depth;

#if OPT_ELAPSED

#if 0
static void ETrace(const char *fmt,...) GCC_PRINTFLIKE(1,2);

static void
ETrace(const char *fmt,...)
{
    static const char *mode = "w";

    FILE *fp = fopen("ETrace.out", mode);
    if (fp != 0) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	fclose(fp);
	mode = "a";
    }
}

#define ETRACE(p) ETrace p
#else
#define ETRACE(p)		/* nothing */
#endif

static void
old_elapsed(BI_NODE * a)
{
    beginDisplay();
    if (a != 0) {
	FreeIfNeeded(BI_KEY(a));
	FreeIfNeeded(a->value.data);
	free(a);
    }
    endofDisplay();
}

static BI_NODE *
new_elapsed(BI_DATA * a)
{
    BI_NODE *p;

    beginDisplay();
    if ((p = typecalloc(BI_NODE)) != 0) {
	BI_DATA *q = &(p->value);
	if ((BI_KEY(p) = strmalloc(a->bi_key)) == 0
	    || (q->data = typecalloc(ELAPSED_DATA)) == 0) {
	    old_elapsed(p);
	    p = 0;
	}
    }
    endofDisplay();

    return p;
}

/*ARGSUSED*/
static void
dpy_elapsed(BI_NODE * a, int level GCC_UNUSED)
{
    BI_DATA *p = &(a->value);
    ELAPSED_DATA *q = p->data;

    Trace("\t%-24s\t%f / %5ld = %g\n",
	  BI_KEY(a),
	  q->total_time,
	  q->total_calls,
	  q->total_time / q->total_calls);
}

#define BI_TREE0 {{0}, 0, {0, 0}}
static BI_TREE elapsed_tree =
{new_elapsed, old_elapsed, dpy_elapsed, 0, 0, BI_TREE0};

static BI_DATA *elapsed_stack[100];
static int elapsed_sp = 0;

static double
Elapsed(ElapsedType * first, int begin)
{
    double result;

#if SYS_UNIX
#define	SECS(tv)	(tv.tv_sec + (tv.tv_usec / 1.0e6))
    ElapsedType tv1;
    gettimeofday(&tv1, 0);
    if (begin)
	*first = tv1;
    result = (SECS(tv1) - SECS((*first)));
#elif SYS_WINNT
    ElapsedType tv1 = GetTickCount();
    if (begin)
	*first = tv1;
    result = ((tv1 - *first) / 1000.0);
#else
    ElapsedType tv1 = time((time_t *) 0);
    if (begin)
	*first = tv1;
    result = (tv1 - *first);
#endif

    return result;
}

static BI_DATA *
find_elapsed(const char *txt)
{
    BI_DATA *result = 0;
    char temp[80];
    char *s = strchr(mktrimmed(strcpy(temp, txt)), L_PAREN);
    if (s != 0)
	*s = EOS;
    if (*temp != EOS) {
	if ((result = btree_search(&elapsed_tree, temp)) == 0) {
	    BI_DATA data;
	    memset(&data, 0, sizeof(data));
	    data.bi_key = temp;
	    result = btree_insert(&elapsed_tree, &data);
	}
    }
    return result;
}

static void
begin_elapsed(const char *txt)
{
    ETRACE(("%d:%s", elapsed_sp, txt));
    if (elapsed_sp + 1 < (int) TABLESIZE(elapsed_stack)) {
	BI_DATA *p = find_elapsed(txt + strlen(T_CALLED));
	if (p != 0) {
	    ELAPSED_DATA *q = p->data;
	    elapsed_stack[elapsed_sp] = p;
	    if (!(q->nesting_time++)) {
		(void) Elapsed(&(q->this_time), TRUE);
	    }
	}
    }
    ++elapsed_sp;
}

static void
end_elapsed(void)
{
    if (elapsed_sp > 0) {
	if (--elapsed_sp < (int) TABLESIZE(elapsed_stack)) {
	    BI_DATA *p = elapsed_stack[elapsed_sp];
	    ETRACE(("%d:%s::%s\n", elapsed_sp, T_RETURN, p->bi_key));
	    if (p != 0) {
		ELAPSED_DATA *q = p->data;
		if (!(--(q->nesting_time))) {
		    q->total_time += Elapsed(&(q->this_time), FALSE);
		    q->total_calls += 1;
		}
	    }
	}
    }
}

void
show_elapsed(void)
{
    Trace("ShowElapsed:\n");
    btree_printf(&elapsed_tree);
}

#else
#define begin_elapsed(txt)	/* nothing */
#define end_elapsed()		/* nothing */
#endif

/******************************************************************************/

void
Trace(const char *fmt,...)
{
    static int nested;

    int save_err;
    va_list ap;
    static FILE *fp;
#if SYS_WINNT
    HANDLE myMutex = CreateMutex(NULL, FALSE, NULL);
    WaitForSingleObject(myMutex, INFINITE);
#endif

    if (!nested++) {
	beginDisplay();
	save_err = errno;
	va_start(ap, fmt);

	if (fmt != bad_form) {
	    if (fp == NULL)
		fp = fopen("Trace.out", "w");
	    if (fp == NULL)
		abort();

	    fprintf(fp, "%s", trace_indent(trace_depth, '|'));
	    if (!strncmp(fmt, T_CALLED, T_LENGTH)) {
		begin_elapsed(fmt);
		++trace_depth;
	    } else if (!strncmp(fmt, T_RETURN, T_LENGTH)) {
		if (trace_depth == 0) {
		    fprintf(fp, "BUG: called/return mismatch\n");
		} else {
		    end_elapsed();
		    --trace_depth;
		}
	    }
	    vfprintf(fp, fmt, ap);
	    (void) fflush(fp);
	} else if (fp != 0) {
	    while (trace_depth > 0) {
		fprintf(fp, "%s", trace_indent(trace_depth--, '|'));
		fprintf(fp, T_RETURN "(close)\n");
	    }
	    (void) fclose(fp);
	    (void) fflush(stdout);
	    (void) fflush(stderr);
	}

	va_end(ap);
	errno = save_err;
	endofDisplay();
    }
    --nested;

#if SYS_WINNT
    ReleaseMutex(myMutex);
    CloseHandle(myMutex);
#endif
}

int
retrace_code(int code)
{
    Trace(T_RETURN "%d\n", code);
    return code;
}

void *
retrace_ptr(void *code)
{
    Trace(T_RETURN "%p\n", TRACE_NULL(code));
    return code;
}

char *
retrace_string(char *code)
{
    Trace(T_RETURN "%s%s\n", TRACE_NULL(code),
	  (code == error_val) ? " (ERROR)" : "");
    return code;
}

void
retrace_void(void)
{
    Trace(T_RETURN "\n");
}

static char *
alloc_visible(size_t need)
{
    if (need > used_visible) {
	used_visible = need;

	if (visible_result == 0)
	    visible_result = typeallocn(char, need);
	else
	    visible_result = typereallocn(char, visible_result, need);

	if (visible_result != 0)
	    memset(visible_result, 0, need);
    }
    return visible_result;
}

char *
trace_indent(int level, int marker)
{
    unsigned need = 1 + (level * 3);
    int n;

    if (need > used_indent) {
	used_indent = need;

	if (visible_indent == 0)
	    visible_indent = typeallocn(char, need);
	else
	    visible_indent = typereallocn(char, visible_indent, need);

	if (visible_indent != 0)
	    memset(visible_indent, 0, need);
    }

    *visible_indent = EOS;
    for (n = 0; n < level; ++n)
	sprintf(visible_indent + (2 * n), "%c ", marker);
    return visible_indent;
}

char *
visible_buff(const char *buffer, int length, int eos)
{
    int j;
    unsigned k = 0;
    unsigned need = ((length > 0) ? (length * 6) : 0) + 1;
    char *result;

    beginDisplay();
    if (buffer == 0) {
	buffer = "";
	length = 0;
    }

    result = alloc_visible(need);

    for (j = 0; j < length; j++) {
	int c = buffer[j] & 0xff;
	if (eos && !c) {
	    break;
	} else {
	    char *dst = result + k;
	    vl_vischr(dst, c);
	    k += (unsigned) strlen(dst);
	}
    }
    result[k] = 0;
    endofDisplay();
    return result;
}

char *
visible_video_text(const VIDEO_TEXT * buffer, int length)
{
    int j;
    unsigned k = 0;
    unsigned need = ((length > 0) ? (length * 6) : 0) + 1;
    char *result;

    beginDisplay();
    if (buffer == 0) {
	static const VIDEO_TEXT dummy[] =
	{0};
	buffer = dummy;
	length = 0;
    }

    result = alloc_visible(need);

    for (j = 0; j < length; j++) {
	int c = buffer[j] & ((1 << (8 * sizeof(VIDEO_TEXT))) - 1);
	char *dst = result + k;
	vl_vischr(dst, c);
	k += (unsigned) strlen(dst);
    }
    result[k] = 0;
    endofDisplay();
    return result;
}

/*
 * Convert a string to visible form.
 */
char *
str_visible(const char *s)
{
    if (s == 0)
	return "<null>";
    return visible_buff(s, (int) strlen(s), FALSE);
}

/*
 * Convert a string to visible form, but include the trailing null.
 */
char *
str_visible0(const char *s)
{
    if (s == 0)
	return "<null>";
    return visible_buff(s, (int) strlen(s), TRUE);
}

char *
tb_visible(TBUFF *p)
{
    return visible_buff(tb_values(p), (int) tb_length(p), FALSE);
}

char *
itb_visible(ITBUFF * p)
{
    int *vec;
    int pass;
    size_t used;
    size_t len, n;
    char temp[80];
    char *result = 0;

    beginDisplay();
    vec = itb_values(p);
    len = itb_length(p);
    if (vec != 0 && len != 0) {
	used = 0;
	for (pass = 0; pass < 2; ++pass) {
	    for (n = 0; n < len; ++n) {
		kcod2escape_seq(vec[n], temp, sizeof(temp));
		if (used) {
		    if (pass)
			strcat(result, ",");
		    ++used;
		}
		if (pass)
		    strcat(result, temp);
		used += strlen(temp);
	    }
	    if (!pass)
		*(result = alloc_visible(1 + used)) = EOS;
	}
    } else {
	*(result = alloc_visible(1)) = EOS;
    }
    endofDisplay();
    return result;
}

char *
lp_visible(LINE *p)
{
    if (p == 0)
	return "";
    return visible_buff(lvalue(p), llength(p), FALSE);
}

static int count_alloc;
static int count_freed;

void
fail_alloc(char *msg, char *ptr)
{
    Trace("%s: %p\n", msg, ptr);
    Trace("allocs %d, frees %d\n", count_alloc, count_freed);
#if NO_LEAKS
    show_alloc();
#endif
    Trace(bad_form);
    abort();
}

#if	DOALLOC
typedef struct {
    unsigned long size;		/* ...its size */
    char *text;			/* the actual segment */
    int note;			/* ...last value of 'count_alloc' */
} AREA;

static AREA *area;

static long maxAllocated;	/* maximum # of bytes allocated */
static long nowAllocated;	/* current # of bytes allocated */
static int nowPending;		/* current end of 'area[]' table */
static int maxPending;		/* maximum # of segments allocated */

static void
InitArea(void)
{
    if (area == 0) {
	area = typecallocn(AREA, DOALLOC);
	if (area == 0)
	    abort();
	Trace("Initialized doalloc (%d * %d) = %ld\n",
	      DOALLOC,
	      sizeof(AREA),
	      (long) sizeof(AREA) * (long) DOALLOC);
    }
}

static int
FindArea(char *ptr)
{
    int j;

    InitArea();
    for (j = 0; j < DOALLOC; j++)
	if (area[j].text == ptr) {
	    if (j >= nowPending) {
		nowPending = j + 1;
		if (nowPending > maxPending)
		    maxPending = nowPending;
	    }
	    return j;
	}
    return -1;
}

static int
record_freed(char *ptr)
{
    int j;

    if ((j = FindArea(ptr)) >= 0) {
	nowAllocated -= area[j].size;
	area[j].size = 0;
	area[j].text = 0;
	area[j].note = count_freed;
	if ((j + 1) == nowPending) {
	    int k;
	    for (k = j; (k >= 0) && !area[k].size; k--)
		nowPending = k;
	}
    }
    return j;
}

static int
record_alloc(char *newp, char *oldp, unsigned len)
{
    int j;

    if (newp == oldp) {
	if ((j = FindArea(oldp)) >= 0) {
	    nowAllocated -= area[j].size;
	    area[j].size = len;
	    area[j].note = count_alloc;
	} else
	    fail_alloc("could not find", oldp);
    } else {
	if (oldp != 0)
	    record_freed(oldp);
	if ((j = FindArea((char *) 0)) >= 0) {
	    area[j].text = newp;
	    area[j].size = len;
	    area[j].note = count_alloc;
	} else {
	    fail_alloc("no room in table", newp);
	}
    }

    nowAllocated += len;
    if (nowAllocated > maxAllocated)
	maxAllocated = nowAllocated;
    return len;
}

#define	OK_ALLOC(p,q,n)	((p != 0) && (record_alloc(p,q,n) >= 0))
#define	OK_FREE(p)	((p != 0) && (record_freed(p) >= 0))
#else
#define	OK_ALLOC(p,q,n)	(p != 0)
#define	OK_FREE(p)	(p != 0)
#endif /* DOALLOC */

#ifdef	DEBUG2
#define	LOG_PTR(msg,num)	Trace("%s %p\n", msg, num)
#define	LOG_LEN(msg,num)	Trace("%s %d\n", msg, num)
#else
#define LOG_PTR(msg,num)
#define	LOG_LEN(msg,num)
#endif

/************************************************************************
 *	public entrypoints						*
 ************************************************************************/
#if DOALLOC
void *
doalloc(void *oldp, unsigned amount)
{
    void *newp;

    check_opt_working();
    count_alloc += (oldp == 0);
    LOG_LEN("allocate", amount);
    LOG_PTR("  old = ", oldp);

#if 1
    newp = ((oldp != 0)
	    ? typereallocn(char, oldp, amount)
	    : typeallocn(char, amount));
#else
    /* this is a little cleaner for valgrind's purpose, but slower */
    if (oldp != 0) {
	int j = FindArea(oldp);
	assert(j >= 0);

	newp = typecallocn(char, amount);
	if (newp != 0) {
	    unsigned limit = (amount > area[j].size) ? area[j].size : amount;
	    TRACE(("memcpy %p .. %p to %p .. %p (%ld -> %d)\n",
		   oldp, (char *) oldp + area[j].size - 1,
		   newp, (char *) newp + area[j].size - 1,
		   area[j].size, amount));
	    memcpy(newp, oldp, limit);
	    free(oldp);
	}
    } else {
	newp = typecallocn(char, amount);
    }
#endif
    if (!OK_ALLOC(newp, oldp, amount)) {
	perror("doalloc");
	fail_alloc("doalloc", oldp);
	/*NOT REACHED */
    }

    LOG_PTR("  new = ", newp);
    return (newp);
}

char *
do_calloc(unsigned nmemb, unsigned size)
{
    unsigned len = nmemb * size;
    char *result = doalloc((char *) 0, len);
    memset(result, 0, len);
    return result;
}

/*
 * Entrypoint so we can validate pointers
 */
void
dofree(void *oldp)
{
    check_opt_working();
    count_freed++;
    LOG_PTR("dealloc ", oldp);

    if (OK_FREE(oldp)) {
	free(oldp);
	return;
    }

    fail_alloc("free (not found)", oldp);
}

/*
 * Write over the area if it is valid.
 */
void
dopoison(void *oldp, unsigned long len)
{
    int j;

    if ((j = FindArea(oldp)) >= 0) {
	if (area[j].size >= len)
	    (void) memset((char *) oldp, 0xdf, len);
	else
	    fail_alloc("incorrect length", oldp);
    } else {
	fail_alloc("cannot find to poison", oldp);
    }
}
#endif

#ifndef show_alloc
void
show_alloc(void)
{
#if DOALLOC
    static char *fmt = ".. %-24.24s %10ld\n";

    printf("show_alloc\n");	/* patch */
    Trace("** allocator metrics:\n");
    Trace(fmt, "allocs:", count_alloc);
    Trace(fmt, "frees:", count_freed);
    if (area != 0) {
	int j, count = 0;
	long total = 0;

	for (j = 0; j < nowPending; j++) {
	    if (area[j].text) {
		if (count++ < 50)
		    Trace("...%5d) %5ld bytes in alloc #%-5d @%p %s\n",
			  j,
			  area[j].size,
			  area[j].note,
			  area[j].text,
			  visible_buff(area[j].text,
				       area[j].size,
				       FALSE));
		total += area[j].size;
	    }
	}
	Trace(fmt, "total bytes allocated:", total);
	Trace(fmt, "current bytes allocated:", nowAllocated);
	Trace(fmt, "maximum bytes allocated:", maxAllocated);
	Trace(fmt, "segment-table length:", nowPending);
	Trace(fmt, "current # of segments:", (long) count);
	Trace(fmt, "maximum # of segments:", maxPending);
    }
#endif
}
#endif /* show_alloc */

static int oops;

static int
check_line(LINE *lp, BUFFER *bp)
{
    LINE *p;
    for_each_line(p, bp) {
	if (lp == p)
	    return TRUE;
    }
    return FALSE;
}

static int
check_forw(LINE *lp)
{
    LINE *p, *q;
    p = lforw(lp);
    q = lback(p);
    return (lp == q);
}

static int
check_back(LINE *lp)
{
    LINE *p, *q;
    p = lback(lp);
    q = lforw(p);
    return (lp == q);
}

void
trace_line(LINE *lp, BUFFER *bp)
{
    if (lp == 0) {
	Trace("? null LINE pointer\n");
    } else if (bp == 0) {
	Trace("? null BUFFER pointer\n");
    } else {
	const char *a = check_forw(lp) ? "" : "?";
	const char *b = check_back(lp) ? "" : "?";
	Trace("%4d%s:{%p, f %p%s, b %p%s}:%s\n",
	      line_no(bp, lp), check_line(lp, bp) ? "" : "?", lp,
	      lforw(lp), a,
	      lback(lp), b,
	      lp_visible(lp));
	if (*a || *b)
	    oops++;
    }
}

void
trace_mark(char *name, MARK *mk, BUFFER *bp)
{
    Trace("%s %d.%d\n", name, line_no(bp, mk->l), mk->o);
}

static const char *
visible_shape(REGIONSHAPE shape)
{
    return
	(shape == rgn_EXACT
	 ? "exact"
	 : (shape == rgn_FULLLINE
	    ? "full-line"
	    : (shape == rgn_RECTANGLE
	       ? "rectangle"
	       : "?")));
}

void
trace_region(REGION * rp, BUFFER *bp)
{
    B_COUNT total = 0;
    B_COUNT len_rs = len_record_sep(bp);

    L_NUM no_1st = line_no(bp, rp->r_orig.l);
    L_NUM no_2nd = line_no(bp, rp->r_end.l);

    LINE *lp1 = (no_1st > no_2nd) ? rp->r_end.l : rp->r_orig.l;
    LINE *lp2 = (no_1st > no_2nd) ? rp->r_orig.l : rp->r_end.l;

    C_NUM c_1st = (no_1st > no_2nd) ? rp->r_end.o : rp->r_orig.o;
    C_NUM c_2nd = (no_1st > no_2nd) ? rp->r_orig.o : rp->r_end.o;

    LINE *lp = lp1;

    if (lp1 == lp2) {
	if (c_1st > c_2nd) {
	    int n = c_1st;
	    c_1st = c_2nd;
	    c_2nd = n;
	}
    }

    Trace("region %d.%d .. %d.%d cols %d..%d (%ld:%s)\n",
	  no_1st, rp->r_orig.o,
	  no_2nd, rp->r_end.o,
	  rp->r_leftcol, rp->r_rightcol,
	  (long) rp->r_size,
	  visible_shape(regionshape));

    for (;;) {
	char *text = lvalue(lp);
	int size = llength(lp);
	int skip = 0;

	if (lp == lp1) {
	    text += c_1st;
	    skip = c_1st;
	    if (lp == lp2) {
		size = c_2nd - c_1st;
	    } else {
		size -= c_1st;
	    }
	} else if (lp == lp2) {
	    size = c_2nd;
	}

	if (skip) {
	    Trace("%5d%*s%s\n",
		  line_no(bp, lp),
		  skip + 2, "->",
		  visible_buff(text, size, 0));
	} else {
	    Trace("%5d%s%s\n",
		  line_no(bp, lp),
		  "->",
		  visible_buff(text, size, 0));
	}
	total += (size + len_rs);
	if (total > rp->r_size) {
	    if (lp == lp2 && (total - len_rs) == rp->r_size) {
		total -= len_rs;
	    }
	}

	if (lp == lp2)
	    break;
	lp = lforw(lp);
    }
    Trace("total %ld vs %ld\n", (long) total, (long) rp->r_size);
}

void
trace_buffer(BUFFER *bp)
{
    LINE *lp;
    Trace("trace_buffer(%s) dot=%p%s\n",
	  bp->b_bname,
	  bp->b_dot.l,
	  bp == curbp ? " (curbp)" : "");
    for_each_line(lp, bp) {
	trace_line(lp, bp);
    }
}

void
trace_window(WINDOW *wp)
{
    LINE *lp;
    int got_dot = FALSE;
    int had_line = FALSE;

    if (wp->w_bufp == 0
	|| wp->w_bufp->b_bname == 0)
	return;

    Trace("trace_window(%s) top=%d, rows=%d, head=%p, line=%p dot=%p%s\n",
	  wp->w_bufp->b_bname,
	  wp->w_toprow,
	  wp->w_ntrows,
	  win_head(wp),
	  wp->w_line.l,
	  wp->w_dot.l,
	  wp == curwp ? " (curwp)" : "");

    for (lp = wp->w_line.l; lp != win_head(wp); lp = lforw(lp)) {
	trace_line(lp, wp->w_bufp);
	if (lp == 0)
	    break;
	if (lp == wp->w_dot.l)
	    got_dot = TRUE;
	had_line = TRUE;
    }
    if (!got_dot && had_line) {
	Trace("DOT not found!\n");
	imdying(10);
    }
}

#if OPT_WORKING && (OPT_TRACE > 2)
#undef beginDisplay
void
beginDisplay(void)
{
    Trace(T_CALLED "beginDisplay(%d)\n", ++im_displaying);
}

#undef endofDisplay
void
endofDisplay(void)
{
    assert(im_displaying > 0);
    --im_displaying;
    returnVoid();		/* matches beginDisplay */
}
#endif

#if NO_LEAKS
static void
close_me(void)
{
#if DOALLOC
    FreeAndNull(area);
#endif
    FreeAndNull(visible_result);
    FreeAndNull(visible_indent);
    used_visible = 0;
    used_indent = 0;
#if OPT_ELAPSED
    btree_freeup(&elapsed_tree);
#endif
    Trace(bad_form);
}

void
trace_leaks(void)
{
    atexit(close_me);
}
#endif
