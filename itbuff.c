/*
 *	tbuff.c
 *
 *	Manage dynamic temporary buffers.
 *	Note that some temp-buffs are never freed, for speed
 *
 *	To do:	add 'itb_ins()' and 'itb_del()' to support cursor-level command
 *		editing.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/itbuff.c,v 1.14 2002/01/09 00:30:55 tom Exp $
 *
 */

#include "estruct.h"
#include "edef.h"

#define	NCHUNK	NLINE

/*******(testing)************************************************************/
#if NO_LEAKS
typedef	struct	_itb_list	{
	struct	_itb_list	*link;
	ITBUFF			*buff;
	} ITB_LIST;

static	ITB_LIST	*all_tbuffs;

#define	AllocatedBuffer(q)	itb_remember(q);
#define	FreedBuffer(q)		itb_forget(q);

static void
itb_remember(ITBUFF *p)
{
	register ITB_LIST *q = typealloc(ITB_LIST);
	q->buff = p;
	q->link = all_tbuffs;
	all_tbuffs = q;
}

static void
itb_forget(ITBUFF *p)
{
	register ITB_LIST *q, *r;

	for (q = all_tbuffs, r = 0; q != 0; r = q, q = q->link)
		if (q->buff == p) {
			if (r != 0)
				r->link = q->link;
			else
				all_tbuffs = q->link;
			free((char *)q);
			break;
		}
}

void
itb_leaks(void)
{
	while (all_tbuffs != 0) {
		ITBUFF	*q = all_tbuffs->buff;
		FreedBuffer(q);
		itb_free(&q);
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
ITBUFF *
itb_alloc(ITBUFF **p, size_t n)
{
	register ITBUFF *q = *p;
	if (q == 0) {
		q = *p = typealloc(ITBUFF);
		q->itb_data = typeallocn(int, q->itb_size = n);
		q->itb_used = 0;
		q->itb_last = 0;
		q->itb_endc = esc_c;
		AllocatedBuffer(q)
	} else if (n >= q->itb_size) {
		q->itb_data = typereallocn(int, q->itb_data,
						q->itb_size = (n*2));
	}
	return q;
}

/*
 * (re)initialize a temp-buff
 */
ITBUFF *
itb_init(ITBUFF **p, int c)
{
	register ITBUFF *q = *p;
	if (q == 0)
		q = itb_alloc(p, NCHUNK);
	q->itb_used = 0;
	q->itb_last = 0;
	q->itb_endc = c;	/* code to return if no-more-data */
	return (*p = q);
}

/*
 * deallocate a temp-buff
 */
void
itb_free(ITBUFF **p)
{
	register ITBUFF *q = *p;

	if (q != 0) {
		FreedBuffer(q)
		free((char *)(q->itb_data));
		free((char *)q);
	}
	*p = 0;
}

/*******(storage)************************************************************/

/*
 * put a character c at the nth position of the temp-buff.  make it the last.
 */
static ITBUFF *
itb_put(ITBUFF **p, size_t n, int c)
{
	register ITBUFF *q;

	if ((q = itb_alloc(p, n+1)) != 0) {
		q->itb_data[n] = c;
		q->itb_used = n+1;
	}
	return q;
}

/*
 * stuff the nth character into the temp-buff -- assumes space already there
 *  it's sort of the opposite of itb_peek
 */
void
itb_stuff(ITBUFF *p, int c)
{
	if (p->itb_last < p->itb_used)
		p->itb_data[p->itb_last] = c;
	else
		p->itb_endc = c;
}
/*
 * append a character to the temp-buff
 */
ITBUFF *
itb_append(ITBUFF **p, int c)
{
	register ITBUFF *q = *p;
	register size_t n = (q != 0) ? q->itb_used : 0;

	return itb_put(p, n, c);
}

/*
 * Copy one temp-buff to another
 */
ITBUFF *
itb_copy(ITBUFF **d, ITBUFF *s)
{
	ITBUFF *p;

	if (s != 0) {
		if ((p = itb_init(d, s->itb_endc)) != 0) {
			int	*ptr = s->itb_data;
			size_t len = s->itb_used;
			while ((len-- != 0) && itb_append(&p, *ptr++) != 0)
				;
		}
	} else
		p = itb_init(d, esc_c);
	return p;
}

/*
 * append a binary data to the temp-buff
 */
ITBUFF *
itb_bappend(ITBUFF **p, const char *s, size_t len)
{
	while ((len-- != 0) && itb_append(p, (int)(*s++)) != 0)
		;
	return *p;
}

#if NEEDED
/*
 * append a string to the temp-buff
 */
ITBUFF *
itb_sappend( ITBUFF **p, const char *s)
{
	if (!s)
		return *p;
	while (*s && itb_append(p, (int)(*s++)) != 0)
		;
	return *p;
}

void
itb_delete(ITBUFF *p, size_t cnt)
{
	int *from, *to, *used;

	to = &p->itb_data[p->itb_last];
	from = to + cnt;

	used = &p->itb_data[p->itb_used];

	if (from >= used) {
		p->itb_used = p->itb_last;
		return;
	}

	while (from < used) {
		*to++ = *from++;
	}

	p->itb_used -= cnt;

}

ITBUFF *
itb_insert(ITBUFF **p, int c)
{
	register ITBUFF *q = *p;
	int *last, *to;

	/* force enough room for another character */
	itb_put(p, q->itb_used, 0 /* any value */ );

	/* copy up */
	to = &q->itb_data[q->itb_used-1];
	last = &q->itb_data[q->itb_last];
	while (to > last) {
		*to = *(to-1);
		to--;
	}

	/* put in the new one */
	itb_stuff(q, c);

	return *p;
}
#endif  /* NEEDED */


/*******(retrieval)************************************************************/

/*
 * get the nth character from the temp-buff
 */
int
itb_get(ITBUFF *p, size_t n)
{
	register int	c = esc_c;

	if (p != 0)
		c = (n < p->itb_used) ? p->itb_data[n] : p->itb_endc;

	return c;
}

#if NEEDED
/*
 * undo the last 'itb_put'
 */
void
itb_unput(ITBUFF *p)
{
	if (p != 0
	 && p->itb_used != 0)
		p->itb_used -= 1;
}
#endif

/*******(iterators)************************************************************/

/*
 * Reset the iteration-count
 */
static
size_t
itb_seek(ITBUFF *p, size_t seekto, int whence)
{
	size_t olast;

	if (p == 0)
		return 0;

	olast = p->itb_last;

	if (whence == 0)
		p->itb_last = seekto;
	else if (whence == 1)
		p->itb_last += seekto;
	else if (whence == 2)
		p->itb_last = p->itb_used - seekto;
	return olast;
}

void
itb_first(ITBUFF *p)
{
	(void)itb_seek(p, 0, 0);
}

/*
 * Returns true iff the iteration-count has not gone past the end of temp-buff.
 */
int
itb_more(ITBUFF *p)
{
	return (p != 0) ? (p->itb_last < p->itb_used) : FALSE;
}

/*
 * get the next character from the temp-buff
 */
int
itb_next(ITBUFF *p)
{
	if (p != 0)
		return itb_get(p, p->itb_last++);
	return esc_c;
}

/*
 * get the last character from the temp-buff, and shorten
 * (opposite of itb_append)
 */
int
itb_last(ITBUFF *p)
{
	int c;
	if (p != 0 && p->itb_used > 0) {
		c = itb_get(p, p->itb_used-1);
		p->itb_used--;
		return c;
	}
	return esc_c;
}

#if NEEDED
/*
 * undo a itb_next
 */
void
itb_unnext(ITBUFF *p)
{
	if (p == 0)
		return;
	if (p->itb_last > 0)
		p->itb_last--;
}
#endif

/*
 * get the next character from the temp-buff w/o incrementing index
 */
int
itb_peek(ITBUFF *p)
{
	if (p != 0)
		return itb_get(p, p->itb_last);
	return esc_c;
}

/*******(bulk-data)************************************************************/

/*
 * returns a pointer to data, assumes it is one long string
 */
int *
itb_values(ITBUFF *p)
{
	return (p != 0) ? p->itb_data : 0;
}

/*
 * returns the length of the data
 */
size_t
itb_length(ITBUFF *p)
{
	return (p != 0) ? p->itb_used : 0;
}
