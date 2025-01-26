/*
 *	tbuff.c
 *
 *	Manage dynamic temporary buffers.
 *	Note that some temp-buffs are never freed, for speed
 *
 *	To do:	add 'tb_ins()' and 'tb_del()' to support cursor-level command
 *		editing.
 *
 * $Id: tbuff.c,v 1.83 2025/01/26 17:08:26 tom Exp $
 */

#include "estruct.h"
#include "edef.h"

#define	NCHUNK	NLINE

#define ERROR_LEN 6		/* sizeof(ERROR) */

/*******(testing)************************************************************/
#if NO_LEAKS
typedef struct _tb_list {
    struct _tb_list *link;
    TBUFF *buff;
} TB_LIST;

static TB_LIST *all_tbuffs;

#define	AllocatedBuffer(q)	tb_remember(q)
#define	FreedBuffer(q)		tb_forget(q)

static void
tb_remember(TBUFF *p)
{
    TB_LIST *q;

    if ((q = typealloc(TB_LIST)) != NULL) {
	q->buff = p;
	q->link = all_tbuffs;
	all_tbuffs = q;
    }
}

static void
tb_forget(TBUFF *p)
{
    TB_LIST *q, *r;

    beginDisplay();
    for (q = all_tbuffs, r = NULL; q != NULL; r = q, q = q->link) {
	if (q->buff == p) {
	    if (r != NULL)
		r->link = q->link;
	    else
		all_tbuffs = q->link;
	    free(q);
	    break;
	}
    }
    endofDisplay();
}

void
tb_leaks(void)
{
    while (all_tbuffs != NULL) {
	TBUFF *q = all_tbuffs->buff;
	FreedBuffer(q);
	tb_free(&q);
    }
}

#else
#define	AllocatedBuffer(q)
#define	FreedBuffer(q)
#endif

#if OPT_TRACE
static void
ValidateTBUFF(TBUFF *p, int lineno)
{
    int ok = TRUE;

    if (p != NULL) {
	if (p->tb_errs) {
	    if ((p->tb_size != 0 || p->tb_used != 0) || (p->tb_data != NULL))
		ok = FALSE;
	} else {
	    if ((p->tb_size != 0 || p->tb_used != 0) && (p->tb_data == NULL))
		ok = FALSE;
	    if ((p->tb_size == 0 && p->tb_used == 0) && (p->tb_data != NULL))
		ok = FALSE;
	}
	if (!ok) {
	    TRACE(("%s @%d: inconsistent TBUFF %p(errs %d, size %d, used %d, data %p)\n",
		   __FILE__, lineno,
		   (void *) p,
		   p->tb_errs,
		   (int) p->tb_size,
		   (int) p->tb_used,
		   (void *) p->tb_data));
	}
    }
}

#define valid_tbuff(p)		ValidateTBUFF(p, __LINE__)
#else
#define valid_tbuff(p)		/* nothing */
#endif /* OPT_TRACE */

/*******(initialization)*****************************************************/

/*
 * ensure that the given temp-buff has as much space as specified
 */
TBUFF *
tb_alloc(TBUFF **p, size_t n)
{
    TBUFF *q = *p;

    beginDisplay();
    valid_tbuff(q);

    /* any nonzero-size will do, but nonzero it must be */
    if (n < sizeof(TBUFF)) {
	n = sizeof(TBUFF);
    }

    if (q == NULL) {
	if ((q = typecalloc(TBUFF)) != NULL) {
	    if ((q->tb_data = typeallocn(char, q->tb_size = n)) != NULL) {
		q->tb_used = 0;
		q->tb_last = 0;
		q->tb_endc = esc_c;
		q->tb_data[0] = 0;	/* appease Purify */
		q->tb_errs = FALSE;
		AllocatedBuffer(q);
	    } else {
		tb_free(&q);
	    }
	}
    } else if (!isTB_ERRS(q) && (n >= q->tb_size || q->tb_data == NULL)) {
	safe_typereallocn(char, q->tb_data, q->tb_size = (n * 2));
	if (q->tb_data == NULL)
	    tb_free(&q);
    }
    endofDisplay();
    return (*p = q);
}

/*
 * (re)initialize a temp-buff
 */
TBUFF *
tb_init(TBUFF **p, int c)
{
    TBUFF *q = *p;
    if (q == NULL)
	q = tb_alloc(p, (size_t) NCHUNK);
    if (q != NULL) {
	q->tb_used = 0;
	q->tb_last = 0;
	q->tb_endc = c;		/* code to return if no-more-data */
	q->tb_errs = FALSE;
    }
    return (*p = q);
}

/*
 * Set a temp-buff to error-state
 */
TBUFF *
tb_error(TBUFF **p)
{
    TBUFF *result;

    beginDisplay();
    if ((result = tb_init(p, EOS)) != NULL) {
	result->tb_size = 0;
	result->tb_used = 0;
	FreeAndNull(result->tb_data);
	result->tb_errs = TRUE;
    }
    endofDisplay();
    return result;
}

/*
 * deallocate a temp-buff
 */
void
tb_free(TBUFF **p)
{
    TBUFF *q = *p;

    beginDisplay();
    valid_tbuff(q);
    if (q != NULL) {
	FreedBuffer(q);
	FreeIfNeeded(q->tb_data);
	free(q);
    }
    *p = NULL;
    endofDisplay();
}

/*******(storage)************************************************************/

/*
 * put a character c at the nth position of the temp-buff
 */
TBUFF *
tb_put(TBUFF **p, size_t n, int c)
{
    TBUFF *q = *p;

    if (!isTB_ERRS(q)) {
	if ((q = tb_alloc(p, n + 1)) != NULL) {
	    q->tb_data[n] = (char) c;
	    q->tb_used = n + 1;
	}
    }
    valid_tbuff(q);
    return (*p = q);
}

#if VILE_NEEDED
/*
 * stuff the nth character into the temp-buff -- assumes space already there
 *  it's sort of the opposite of tb_peek
 */
void
tb_stuff(TBUFF *p, int c)
{
    if (p != 0) {
	if (p->tb_last < p->tb_used)
	    p->tb_data[p->tb_last] = c;
	else
	    p->tb_endc = c;
    }
}
#endif

/*
 * append a character to the temp-buff
 */
TBUFF *
tb_append(TBUFF **p, int c)
{
    TBUFF *q = *p;
    size_t n = (q != NULL) ? q->tb_used : 0;

    return tb_put(p, n, c);
}

/*
 * insert a character into the temp-buff
 */
TBUFF *
tb_insert(TBUFF **p, size_t n, int c)
{
    TBUFF *q = *p;

    if (!isTB_ERRS(q)) {
	size_t m = tb_length(*p);

	q = tb_append(p, c);
	if (q != NULL && n < m) {
	    while (n < m) {
		q->tb_data[m] = q->tb_data[m - 1];
		m--;
	    }
	    q->tb_data[m] = (char) c;
	}
    }
    return (*p = q);
}

/*
 * Copy one temp-buff to another
 */
TBUFF *
tb_copy(TBUFF **d, TBUFF *s)
{
    TBUFF *p = *d;

    if (isTB_ERRS(p)) {
	p = tb_error(d);
    } else {
	if (s != NULL) {
	    if ((p = tb_init(d, s->tb_endc)) != NULL)
		p = tb_bappend(d, tb_values(s), tb_length(s));
	} else {
	    p = tb_init(d, esc_c);
	}
    }
    return (*d = p);
}

/*
 * append a binary data to the temp-buff
 */
TBUFF *
tb_bappend(TBUFF **p, const char *s, size_t len)
{
    TBUFF *q = *p;

    if (!isTB_ERRS(*p) && (s != NULL)) {
	size_t n = (q != NULL) ? q->tb_used : 0;

	if (isErrorVal(s)) {
	    tb_error(&q);
	} else if ((q = tb_alloc(p, n + len)) != NULL) {
	    if (len != 0) {
		if (q->tb_data + n != s)
		    memmove(q->tb_data + n, s, len);
		q->tb_used = n + len;
	    }
	}
    }
    valid_tbuff(q);
    return q;
}

/*
 * append a string to the temp-buff
 */
TBUFF *
tb_sappend(TBUFF **p, const char *s)
{
    if (s != NULL)
	(void) tb_bappend(p, s, strlen(s));
    return *p;
}

/*
 * append a string to the temp-buff, assume there may be a null on the end of
 * target which is removed.
 */
TBUFF *
tb_sappend0(TBUFF **p, const char *s)
{
    if (s != NULL) {
	TBUFF *q = *p;
	if (q != NULL
	    && !isTB_ERRS(q)
	    && q->tb_used > 0
	    && q->tb_data[q->tb_used - 1] == EOS) {
	    q->tb_used--;
	}
	tb_bappend(p, s, strlen(s));
	tb_append(p, EOS);
    }
    return *p;
}

/*
 * copy a string to the temp-buff, including a null
 */
TBUFF *
tb_scopy(TBUFF **p, const char *s)
{
    (void) tb_init(p, EOS);
    (void) tb_sappend(p, s);
    return tb_append(p, EOS);
}

/*
 * Construct a TBUFF from a null-terminated string, omitting its null.
 */
TBUFF *
tb_string(const char *s)
{
    TBUFF *p = NULL;
    (void) tb_init(&p, EOS);
    return tb_bappend(&p, s, strlen(s));
}

/*******(retrieval)************************************************************/

/*
 * get the nth character from the temp-buff
 */
int
tb_get(TBUFF *p, size_t n)
{
    int c = esc_c;

    if (p != NULL
	&& !isTB_ERRS(p))
	c = (n < p->tb_used) ? p->tb_data[n] : p->tb_endc;

    return CharOf(c);
}

/*
 * undo the last 'tb_put'
 */
void
tb_unput(TBUFF *p)
{
    if (p != NULL
	&& !isTB_ERRS(p)
	&& p->tb_used != 0)
	p->tb_used -= 1;
    valid_tbuff(p);
}

/*******(iterators)************************************************************/

#if VILE_NEEDED
/*
 * Reset the iteration-count
 */
void
tb_first(TBUFF *p)
{
    if (p != 0)
	p->tb_last = 0;
}
#endif

#if DISP_X11
/*
 * Returns true iff the iteration-count has not gone past the end of temp-buff.
 */
int
tb_more(TBUFF *p)
{
    return (p != NULL && !isTB_ERRS(p)) ? (p->tb_last < p->tb_used) : FALSE;
}

/*
 * get the next character from the temp-buff
 */
int
tb_next(TBUFF *p)
{
    if (p != NULL && !isTB_ERRS(p))
	return tb_get(p, p->tb_last++);
    return esc_c;
}
#endif

#if VILE_NEEDED
/*
 * undo a tb_next
 */
void
tb_unnext(TBUFF *p)
{
    if (p == 0 || isTB_ERRS(p))
	return;
    if (p->tb_last > 0)
	p->tb_last--;
    valid_tbuff(p);
}

/*
 * get the next character from the temp-buff w/o incrementing index
 */
int
tb_peek(TBUFF *p)
{
    if (p != 0 && !isTB_ERRS(p))
	return tb_get(p, p->tb_last);
    return esc_c;
}
#endif /* NEEDED */

/*******(evaluation)***********************************************************/
/*
 * Strip single- or double-quotes from the content, adjusting the size.
 */
TBUFF *
tb_dequote(TBUFF **p)
{
    char *value = tb_values(*p);

    TRACE2(("tb_dequote %s\n", tb_visible(*p)));
    if (value == NULL) {
	TRACE2(("...empty\n"));
    } else if (!isErrorVal(value)) {
	int escaped = FALSE;
	UINT delim = CharOf(value[0]);
	UINT j, k, ch;
	size_t have = tb_length(*p);
	size_t used = have;

	if (delim == SQUOTE) {
	    for (j = k = 0; k < have; ++k) {
		value[j] = value[k];
		if (CharOf(value[j]) == delim) {
		    --used;
		} else {
		    ++j;
		}
	    }
	    (*p)->tb_used = used;
	    TRACE2(("...tb_dequote 1: %s\n", tb_visible(*p)));
	} else if (delim == DQUOTE) {
	    for (j = 0, k = 1; k < have; ++k) {
		ch = CharOf(value[j] = value[k]);
		if (escaped) {
		    escaped = FALSE;
		    ++j;
		} else if (ch == BACKSLASH) {
		    escaped = TRUE;
		    --used;
		} else if (ch == delim) {
		    --used;
		} else {
		    ++j;
		}
	    }
	    (*p)->tb_used = used;
	    TRACE2(("...tb_dequote 2: %s\n", tb_visible(*p)));
	} else {
	    TRACE2(("...tb_dequote OOPS: %s\n", tb_visible(*p)));
	}
    }
    valid_tbuff(*p);
    return *p;
}

/*
 * Quote the content.  We prefer single-quotes since they're simpler to read.
 */
TBUFF *
tb_enquote(TBUFF **p)
{
    char *value = tb_values(*p);

    TRACE2(("tb_enquote %s\n", tb_visible(*p)));
    if (value == NULL) {
	TRACE2(("...empty\n"));
    } else if (!isErrorVal(value) && tb_length(*p)) {
	UINT j;
	size_t have = tb_length(*p) - 1;
	size_t need = 2 + have;
	int delim;

	/* decide which delimiter we can use */
	delim = SQUOTE;
	for (j = 0; j < have; ++j) {
	    if (value[j] == SQUOTE) {
		delim = DQUOTE;
		break;
	    }
	}

	/* if we can use single-quotes, there is no need for backslashes */
	if (delim == DQUOTE) {
	    for (j = 0; j < have; ++j) {
		if (value[j] == BACKSLASH || value[j] == DQUOTE)
		    ++need;
	    }
	}

	tb_alloc(p, need + 1);
	if (*p)
	    (*p)->tb_used = need + 1;

	if ((value = tb_values(*p)) != NULL) {
	    value[need] = EOS;
	    value[need - 1] = (char) delim;
	    for (j = 0; j < have; ++j) {
		size_t i = have - j - 1;
		size_t k = need - j - 2;
		UINT ch = CharOf(value[k] = value[i]);
		if (delim == DQUOTE && (ch == DQUOTE || ch == BACKSLASH)) {
		    --need;
		    value[k - 1] = BACKSLASH;
		}
	    }
	    value[0] = (char) delim;
	}
	TRACE2(("...tb_enquote %s\n", tb_visible(*p)));
    }
    valid_tbuff(*p);
    return *p;
}

void
tb_prequote(TBUFF **p)
{
    char *value;

    TRACE2(("tb_prequote %s\n", tb_visible(*p)));
    if (tb_length(*p) && (value = tb_values(*p)) != NULL) {
	if (*value == DQUOTE || *value == SQUOTE) {
	    tb_append(p, EOS);
	    {
		size_t len = tb_length(*p);
		if ((value = tb_values(*p)) != NULL) {
		    do {
			value[len] = value[len - 1];
		    } while (--len != 0);
		    value[0] = SQUOTE;
		}
		TRACE2(("...tb_prequote %s\n", tb_visible(*p)));
	    }
	}
    }
}

/*******(bulk-data)************************************************************/

/*
 * returns a pointer to data, assumes it is one long string
 */
char *
tb_values(TBUFF *p)
{
    char *result = NULL;

    if (p != NULL) {
	if (isTB_ERRS(p)) {
	    result = error_val;
	} else {
	    result = p->tb_data;
	}
    }
    valid_tbuff(p);
    return result;
}

/*
 * returns the length of the data
 */
size_t
tb_length(TBUFF *p)
{
    size_t result = 0;

    if (p != NULL) {
	if (isTB_ERRS(p)) {
	    result = ERROR_LEN;
	} else if (p->tb_data != NULL) {
	    result = p->tb_used;
	}
    }
    valid_tbuff(p);
    return result;
}

/*
 * returns the length of the data, assuming it is terminated by a trailing null
 * which we do not want to count.
 */
int
tb_length0(TBUFF *p)
{
    int result = (int) tb_length(p);

    /* FIXME: we should have an assertion here */
    if (result)
	--result;
    return result;
}

/*
 * Set the length of the buffer, increasing it if needed, and accounting for
 * errors.
 */
void
tb_setlen(TBUFF **p, int n)
{
    size_t len;

    if (p != NULL) {
	if (!isTB_ERRS(*p)) {
	    if (n < 0) {
		char *value = tb_values(*p);
		if (value != NULL)
		    len = strlen(value) + 1;
		else
		    len = 0;
	    } else {
		len = (size_t) n;
		tb_alloc(p, len);
	    }
	    if (*p)
		(*p)->tb_used = len;
	}
	valid_tbuff(*p);
    }
}
