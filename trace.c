/*
 * debugging support -- tom dickey.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/trace.c,v 1.26 2002/05/22 00:03:44 tom Exp $
 *
 */

#include "estruct.h"
#include "edef.h"
#include <ctype.h>

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
extern int vfprintf(FILE * fp, const char *fmt, va_list v);
extern int fflush(FILE * fp);
#endif

#include <stdarg.h>

#if	DOALLOC
#undef	calloc
#undef	malloc
#undef	realloc
#undef	free
#endif /* DOALLOC */

static const char *bad_form;

void
Trace(const char *fmt,...)
{
    int save_err;
    va_list ap;
    static FILE *fp;
#if SYS_WINNT
    HANDLE myMutex = CreateMutex(NULL, FALSE, NULL);
    WaitForSingleObject(myMutex, INFINITE);
#endif
    beginDisplay();
    save_err = errno;
    va_start(ap, fmt);

    if (fp == NULL)
	fp = fopen("Trace.out", "w");
    if (fp == NULL)
	abort();

    if (fmt != bad_form) {
	vfprintf(fp, fmt, ap);
	(void) fflush(fp);
    } else {
	(void) fclose(fp);
	(void) fflush(stdout);
	(void) fflush(stderr);
    }

    va_end(ap);
    errno = save_err;
    endofDisplay();
#if SYS_WINNT
    ReleaseMutex(myMutex);
    CloseHandle(myMutex);
#endif
}

char *
visible_buff(const char *buffer, int length, int eos)
{
    static char *result;
    int j;
    unsigned k = 0;
    unsigned need = ((length > 0) ? (length * 4) : 0) + 1;

    beginDisplay();
    if (buffer == 0)
	buffer = "";

    if (result != 0)
	free(result);
    result = malloc(need);

    for (j = 0; j < length; j++) {
	int c = buffer[j] & 0xff;
	if (eos && !c) {
	    break;
	} else if (isprint(c)) {
	    result[k++] = (char) c;
	} else {
	    if (c >= 128)
		sprintf(result + k, "\\%03o", c);
	    else if (c == 127)
		strcpy(result + k, "^?");
	    else
		sprintf(result + k, "^%c", c | '@');
	    k = strlen(result);
	}
    }
    result[k] = 0;
    endofDisplay();
    return result;
}

char *
str_visible(char *s)
{
    if (s == 0)
	return "(null)";
    return visible_buff(s, strlen(s), TRUE);
}

char *
tb_visible(TBUFF * p)
{
    return visible_buff(tb_values(p), tb_length(p), FALSE);
}

char *
itb_visible(ITBUFF * p)
{
    int *vec;
    int len, n, pass;
    unsigned used;
    char temp[80];

    static char *result;

    beginDisplay();
    if (result != 0)
	free(result);
    vec = itb_values(p);
    len = itb_length(p);
    if (vec != 0 && len != 0) {
	used = 0;
	for (pass = 0; pass < 2; ++pass) {
	    for (n = 0; n < len; ++n) {
		kcod2escape_seq(vec[n], temp);
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
		*(result = malloc(1 + used)) = EOS;
	}
    } else {
	result = strmalloc("");
    }
    endofDisplay();
    return result;
}

char *
lp_visible(LINE *p)
{
    if (p == 0)
	return "";
    return visible_buff(p->l_text, llength(p), FALSE);
}

#define	SECS(tv)	(tv.tv_sec + (tv.tv_usec / 1.0e6))

void
Elapsed(char *msg)
{
#if SYS_UNIX
    struct timeval tv1;
    struct timezone tz1;
    static struct timeval tv0;
    static int init;
    gettimeofday(&tv1, &tz1);
    if (!init++)
	tv0 = tv1;
    Trace("%10.3f %s\n", SECS(tv1) - SECS(tv0), msg);
    tv0 = tv1;
#endif
#if SYS_WINNT
    static DWORD tv0;
    static int init;
    DWORD tv1 = GetTickCount();
    if (!init++)
	tv0 = tv1;
    Trace("%10.3f %s\n", (tv1 - tv0) / 1000.0, msg);
    tv0 = tv1;
#endif
}

#ifdef	apollo
static int
contains(char *ref, char *tst)
{
    size_t len = strlen(ref);
    while (*tst) {
	if (!strncmp(ref, tst++, len))
	    return TRUE;
    }
    return FALSE;
}
#endif /* apollo */

void
WalkBack(void)
{
#ifdef	apollo
    static char *first = "\"WalkBack\"";
    static char *last = "\"unix_$main\"";
    auto FILE *pp;
    auto char bfr[BUFSIZ];
    auto int ok = FALSE;
    static int count;

    Trace("%s %d\n", first, ++count);
    sprintf(bfr, "/com/tb %d", getpid());
    if (!(pp = popen(bfr, "r")))
	perror(bfr);

    while (fgets(bfr, sizeof(bfr), pp)) {
	if (ok && contains(last, bfr))
	    break;
	else if (contains(first, bfr))
	    ok = TRUE;
	else if (ok)
	    Trace("%s", bfr);
    }
    (void) fclose(pp);
#endif /* apollo */
}

static int count_alloc;
static int count_freed;

void
fail_alloc(char *msg, char *ptr)
{
    Trace("%s: %p\n", msg, ptr);
    Trace("allocs %d, frees %d\n", count_alloc, count_freed);
    WalkBack();
#if NO_LEAKS
    show_alloc();
#endif
    Trace(bad_form);
    abort();
}

#if	DOALLOC
typedef struct {
    long size;			/* ...its size */
    char *text;			/* the actual segment */
    int note;			/* ...last value of 'count_alloc' */
} AREA;

static AREA *area;

static long maxAllocated,	/* maximum # of bytes allocated */
  nowAllocated;			/* current # of bytes allocated */
static int nowPending,		/* current end of 'area[]' table */
  maxPending;			/* maximum # of segments allocated */

static void
InitArea(void)
{
    if (area == 0) {
	area = (AREA *) calloc(DOALLOC, sizeof(AREA));
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
    register int j;

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
    register int j;

    if ((j = FindArea(ptr)) >= 0) {
	nowAllocated -= area[j].size;
	area[j].size = 0;
	area[j].text = 0;
	area[j].note = count_freed;
	if ((j + 1) == nowPending) {
	    register int k;
	    for (k = j; (k >= 0) && !area[k].size; k--)
		nowPending = k;
	}
    }
    return j;
}

static int
record_alloc(char *newp, char *oldp, unsigned len)
{
    register int j;

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
	} else
	    fail_alloc("no room in table", newp);
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
char *
doalloc(char *oldp, unsigned amount)
{
    register char *newp;

    count_alloc += (oldp == 0);
    LOG_LEN("allocate", amount);
    LOG_PTR("  old = ", oldp);

    newp = (oldp != 0) ? realloc(oldp, amount) : malloc(amount);
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
dopoison(void *oldp, long len)
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
#if	DOALLOC
    static char *fmt = ".. %-24.24s %10ld\n";

    printf("show_alloc\n");	/* patch */
    Trace("** allocator metrics:\n");
    Trace(fmt, "allocs:", count_alloc);
    Trace(fmt, "frees:", count_freed);
    {
	register int j, count = 0;
	register long total = 0;

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
	char *a = check_forw(lp) ? "" : "?";
	char *b = check_back(lp) ? "" : "?";
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
trace_buffer(BUFFER *bp)
{
    LINEPTR lp;
    Trace("trace_buffer(%s) dot=%p%s\n",
	  bp->b_bname,
	  bp->b_dot.l,
	  bp == curbp ? " (curbp)" : "");
    for_each_line(lp, bp)
	trace_line(lp, bp);
}

void
trace_window(WINDOW *wp)
{
    LINEPTR lp;
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
