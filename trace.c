/*
 * debugging support -- tom dickey.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/trace.c,v 1.2 1997/02/09 17:44:58 tom Exp $
 *
 */
#include "estruct.h"

#undef fopen	/* avoid conflict with 'fakevms.c' */

#ifndef _trace_h
#include "trace.h"
#endif

#if SYS_UNIX
#include <sys/time.h>
#endif

#if	defined(sun)
extern	void	perror ( const char *s );
extern	int	gettimeofday ( struct timeval *t, struct timezone *z );
extern	int	vfprintf ( FILE *fp, const char *fmt, va_list v );
extern	int	fflush ( FILE *fp );
#endif

#include <stdarg.h>

#if	DOALLOC
#undef	calloc
#undef	malloc
#undef	realloc
#undef	free
#endif	/* DOALLOC */

void
Trace(const char *fmt, ...)
{
	static	FILE	*fp;
	va_list ap;

	if (!fp)
		fp = fopen("Trace.out", "w");
	if (!fp)
		abort();

	va_start(ap,fmt);
	if (fmt != 0) {
		vfprintf(fp, fmt, ap);
		va_end(ap);
		(void)fflush(fp);
	} else {
		(void)fclose(fp);
		(void)fflush(stdout);
		(void)fflush(stderr);
	}
}

#define	SECS(tv)	(tv.tv_sec + (tv.tv_usec / 1.0e6))

void
Elapsed(char *msg)
{
#if SYS_UNIX
	static	struct	timeval		tv0, tv1;
	static	struct	timezone	tz0, tz1;
	static	int	init;
	if (!init++)
		gettimeofday(&tv0, &tz0);
	gettimeofday(&tv1, &tz1);
	Trace("%10.6f %s\n", SECS(tv1) - SECS(tv0), msg);
	tv0 = tv1;
#endif
}

#ifdef	apollo
static int
contains(char *ref, char *tst)
{
	size_t	len	= strlen(ref);
	while (*tst) {
		if (!strncmp(ref,tst++,len))
			return TRUE;
	}
	return FALSE;
}
#endif	/* apollo */

void
WalkBack(void)
{
#ifdef	apollo
	static	char	*first	= "\"WalkBack\"",
			*last	= "\"unix_$main\"";
	auto	FILE	*pp;
	auto	char	bfr[BUFSIZ];
	auto	int	ok	= FALSE;
	static	int	count;

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
	(void)fclose(pp);
#endif	/* apollo */
}

static	int	count_alloc,
		count_freed;

void
fail_alloc(char *msg, char *ptr)
{
	Trace("%s: %p\n", msg, ptr);
	Trace("allocs %d, frees %d\n", count_alloc, count_freed);
	WalkBack();
#if NO_LEAKS
	show_alloc();
#endif
	Trace((char *)0);
	abort();
}

#if	DOALLOC
typedef	struct	{
	long	size;	/* ...its size */
	char	*text;	/* the actual segment */
	int	note;	/* ...last value of 'count_alloc' */
	} AREA;

static	AREA	*area;

static	long	maxAllocated,	/* maximum # of bytes allocated */
		nowAllocated;	/* current # of bytes allocated */
static	int	nowPending,	/* current end of 'area[]' table */
		maxPending;	/* maximum # of segments allocated */

static void
InitArea(void)
{
	if (area == 0) {
		area = (AREA *)calloc(DOALLOC,sizeof(AREA));
		if (area == 0)
			abort();
		Trace("Initialized doalloc (%d * %d) = %ld\n",
			DOALLOC,
			sizeof(AREA),
			(long)sizeof(AREA) * (long)DOALLOC);
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
				nowPending = j+1;
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

	InitArea();
	if ((j = FindArea(ptr)) >= 0) {
		nowAllocated -= area[j].size;
		area[j].size = 0;
		area[j].text = 0;
		area[j].note = count_freed;
		if ((j+1) == nowPending) {
			register int	k;
			for (k = j; (k >= 0) && !area[k].size; k--)
				nowPending = k;
		}
	}
	return j;
}

static int
record_alloc(char *newp, char *oldp, unsigned len)
{
	register int	j;

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
		if ((j = FindArea((char *)0)) >= 0) {
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
#endif	/* DOALLOC */

#ifdef	DEBUG2
#define	LOG_PTR(msg,num)	Trace("%s %p\n", msg, num);
#define	LOG_LEN(msg,num)	Trace("%s %d\n", msg, num);
#else
#define LOG_PTR(msg,num)
#define	LOG_LEN(msg,num)
#endif

/************************************************************************
 *	public entrypoints						*
 ************************************************************************/
#if DOALLOC
char *
doalloc (char *oldp, unsigned amount)
{
	register char	*newp;

	count_alloc += (oldp == 0);
	LOG_LEN("allocate", amount)
	LOG_PTR("  old = ", oldp)

	newp = (oldp != 0) ? realloc(oldp, amount) : malloc(amount);
	if (!OK_ALLOC(newp,oldp,amount)) {
		perror("doalloc");
		fail_alloc("doalloc", oldp);
		/*NOT REACHED*/
	}

	LOG_PTR("  new = ", newp)
	return (newp);
}

char *
do_calloc (unsigned nmemb, unsigned size)
{
	unsigned len = nmemb * size;
	char	*result = doalloc((char *)0, len);
	memset(result, 0, len);
	return result;
}

/*
 * Entrypoint so we can validate pointers
 */
void
dofree(char *oldp)
{
	count_freed++;
	LOG_PTR("dealloc ", oldp)

	if (OK_FREE(oldp)) {
		free(oldp);
		return;
	}

	fail_alloc("free (not found)", oldp);
}
#endif

#ifndef show_alloc
void
show_alloc(void)
{
#if	DOALLOC
	static	char	*fmt = ".. %-24.24s %10ld\n";

	printf("show_alloc\n"); /* patch */
	Trace("** allocator metrics:\n");
	Trace(fmt, "allocs:", count_alloc);
	Trace(fmt, "frees:",  count_freed);
	{
		register int	j, count = 0;
		register long	total	= 0;

		for (j = 0; j < nowPending; j++) {
			if (area[j].text) {
				if (count++ < 10)
					Trace("...%5d) %5ld bytes in alloc #%-5d @%p\n",
						j,
						area[j].size,
						area[j].note,
						area[j].text);
				total += area[j].size;
			}
		}
		Trace(fmt, "total bytes allocated:",   total);
		Trace(fmt, "current bytes allocated:", nowAllocated);
		Trace(fmt, "maximum bytes allocated:", maxAllocated);
		Trace(fmt, "segment-table length:",    nowPending);
		Trace(fmt, "current # of segments:",   (long)count);
		Trace(fmt, "maximum # of segments:",   maxPending);
	}
#endif
}
#endif	/* show_alloc */
