/*
 *	tbuff.c
 *
 *	Manage dynamic temporary buffers.
 *	Note that some temp-buffs are never freed, for speed
 *
 *	To do:	add 'tb_ins()' and 'tb_del()' to support cursor-level command
 *		editing.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/tbuff.c,v 1.44 2003/07/27 17:04:27 tom Exp $
 *
 */

#include "estruct.h"
#include "edef.h"

#define	NCHUNK	NLINE

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
tb_remember(TBUFF * p)
{
    TB_LIST *q;

    q = typealloc(TB_LIST);
    q->buff = p;
    q->link = all_tbuffs;
    all_tbuffs = q;
}

static void
tb_forget(TBUFF * p)
{
    TB_LIST *q, *r;

    beginDisplay();
    for (q = all_tbuffs, r = 0; q != 0; r = q, q = q->link) {
	if (q->buff == p) {
	    if (r != 0)
		r->link = q->link;
	    else
		all_tbuffs = q->link;
	    free((char *) q);
	    break;
	}
    }
    endofDisplay();
}

void
tb_leaks(void)
{
    while (all_tbuffs != 0) {
	TBUFF *q = all_tbuffs->buff;
	FreedBuffer(q);
	tb_free(&q);
    }
}

#else
#define	AllocatedBuffer(q)
#define	FreedBuffer(q)
#endif

/*******(initialization)*****************************************************/

/*
 * ensure that the given temp-buff has as much space as specified
 */
TBUFF *
tb_alloc(TBUFF ** p, size_t n)
{
    TBUFF *q = *p;

    beginDisplay();
    if (q == 0) {
	q = *p = typealloc(TBUFF);
	q->tb_data = typeallocn(char, q->tb_size = n);
	q->tb_used = 0;
	q->tb_last = 0;
	q->tb_endc = esc_c;
	q->tb_data[0] = 0;	/* appease Purify */
	q->tb_errs = FALSE;
	AllocatedBuffer(q);
    } else if (n >= q->tb_size) {
	q->tb_data = typereallocn(char, q->tb_data, q->tb_size = (n * 2));
    }
    endofDisplay();
    return q;
}

/*
 * (re)initialize a temp-buff
 */
TBUFF *
tb_init(TBUFF ** p, int c)
{
    TBUFF *q = *p;
    if (q == 0)
	q = tb_alloc(p, NCHUNK);
    q->tb_used = 0;
    q->tb_last = 0;
    q->tb_endc = c;		/* code to return if no-more-data */
    q->tb_errs = FALSE;
    return (*p = q);
}

/*
 * deallocate a temp-buff
 */
void
tb_free(TBUFF ** p)
{
    TBUFF *q = *p;

    beginDisplay();
    if (q != 0) {
	FreedBuffer(q);
	free(q->tb_data);
	free((char *) q);
    }
    *p = 0;
    endofDisplay();
}

/*******(storage)************************************************************/

/*
 * put a character c at the nth position of the temp-buff
 */
TBUFF *
tb_put(TBUFF ** p, size_t n, int c)
{
    TBUFF *q;

    if ((q = tb_alloc(p, n + 1)) != 0) {
	q->tb_data[n] = (char) c;
	q->tb_used = n + 1;
    }
    return (*p = q);
}

#if VILE_NEEDED
/*
 * stuff the nth character into the temp-buff -- assumes space already there
 *  it's sort of the opposite of tb_peek
 */
void
tb_stuff(TBUFF * p, int c)
{
    if (p->tb_last < p->tb_used)
	p->tb_data[p->tb_last] = c;
    else
	p->tb_endc = c;
}
#endif

/*
 * append a character to the temp-buff
 */
TBUFF *
tb_append(TBUFF ** p, int c)
{
    TBUFF *q = *p;
    size_t n = (q != 0) ? q->tb_used : 0;

    return tb_put(p, n, c);
}

/*
 * insert a character into the temp-buff
 */
TBUFF *
tb_insert(TBUFF ** p, size_t n, int c)
{
    size_t m = tb_length(*p);
    TBUFF *q = tb_append(p, c);

    if (q != 0 && n < m) {
	while (n < m) {
	    q->tb_data[m] = q->tb_data[m - 1];
	    m--;
	}
	q->tb_data[m] = (char) c;
    }
    return q;
}

/*
 * Copy one temp-buff to another
 */
TBUFF *
tb_copy(TBUFF ** d, TBUFF * s)
{
    TBUFF *p;

    if (s != 0) {
	if ((p = tb_init(d, s->tb_endc)) != 0)
	    p = tb_bappend(d, s->tb_data, s->tb_used);
    } else
	p = tb_init(d, esc_c);
    return p;
}

/*
 * append a binary data to the temp-buff
 */
TBUFF *
tb_bappend(TBUFF ** p, const char *s, size_t len)
{
    TBUFF *q = *p;
    size_t n = (q != 0) ? q->tb_used : 0;

    if ((q = tb_alloc(p, n + len)) != 0) {
	if (len != 0) {
	    memcpy(q->tb_data + n, s, len);
	    q->tb_used = n + len;
	    q->tb_errs += (s == error_val);
	}
    }
    return *p;
}

/*
 * append a string to the temp-buff
 */
TBUFF *
tb_sappend(TBUFF ** p, const char *s)
{
    if (s != 0)
	(void) tb_bappend(p, s, strlen(s));
    return *p;
}

/*
 * append a string to the temp-buff, assume there may be a null on the end of
 * target which is removed.
 */
TBUFF *
tb_sappend0(TBUFF ** p, const char *s)
{
    if (s != 0) {
	TBUFF *q = *p;
	if (q != 0
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
tb_scopy(TBUFF ** p, const char *s)
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
    TBUFF *p = 0;
    (void) tb_init(&p, EOS);
    return tb_bappend(&p, s, strlen(s));
}

/*******(retrieval)************************************************************/

/*
 * get the nth character from the temp-buff
 */
int
tb_get(TBUFF * p, size_t n)
{
    int c = esc_c;

    if (p != 0)
	c = (n < p->tb_used) ? p->tb_data[n] : p->tb_endc;

    return char2int(c);
}

/*
 * undo the last 'tb_put'
 */
void
tb_unput(TBUFF * p)
{
    if (p != 0
	&& p->tb_used != 0)
	p->tb_used -= 1;
}

/*******(iterators)************************************************************/

#if VILE_NEEDED
/*
 * Reset the iteration-count
 */
void
tb_first(TBUFF * p)
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
tb_more(TBUFF * p)
{
    return (p != 0) ? (p->tb_last < p->tb_used) : FALSE;
}

/*
 * get the next character from the temp-buff
 */
int
tb_next(TBUFF * p)
{
    if (p != 0)
	return tb_get(p, p->tb_last++);
    return esc_c;
}
#endif

#if VILE_NEEDED
/*
 * undo a tb_next
 */
void
tb_unnext(TBUFF * p)
{
    if (p == 0)
	return;
    if (p->tb_last > 0)
	p->tb_last--;
}

/*
 * get the next character from the temp-buff w/o incrementing index
 */
int
tb_peek(TBUFF * p)
{
    if (p != 0)
	return tb_get(p, p->tb_last);
    return esc_c;
}
#endif /* NEEDED */

/*******(bulk-data)************************************************************/

/*
 * returns a pointer to data, assumes it is one long string
 */
char *
tb_values(TBUFF * p)
{
    char *result = 0;

    if (p != 0) {
	if (p->tb_errs) {
	    result = error_val;
	} else {
	    result = p->tb_data;
	}
    }
    return result;
}

/*
 * returns the length of the data
 */
size_t
tb_length(TBUFF * p)
{
    size_t result = 0;

    if (p != 0) {
	if (p->tb_errs) {
	    result = 6;		/* sizeof(ERROR) */
	} else {
	    result = p->tb_used;
	}
    }
    return result;
}
