/*
 *	tbuff.c
 *
 *	Manage dynamic temporary buffers.
 *	Note that some temp-buffs are never freed, for speed
 *
 *	To do:	add 'tb_ins()' and 'tb_del()' to support cursor-level command
 *		editing.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/tbuff.c,v 1.32 1998/11/30 11:52:18 tom Exp $
 *
 */

#include "estruct.h"
#include "edef.h"

#define	NCHUNK	NLINE

/*******(testing)************************************************************/
#if NO_LEAKS
typedef	struct	_tb_list	{
	struct	_tb_list	*link;
	TBUFF			*buff;
	} TB_LIST;

static	TB_LIST	*all_tbuffs;

#define	AllocatedBuffer(q)	tb_remember(q);
#define	FreedBuffer(q)		tb_forget(q);

static
void
tb_remember(TBUFF *p)
{
	register TB_LIST *q = typealloc(TB_LIST);
	q->buff = p;
	q->link = all_tbuffs;
	all_tbuffs = q;
}

static
void
tb_forget(TBUFF *p)
{
	register TB_LIST *q, *r;

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
tb_leaks(void)
{
	while (all_tbuffs != 0) {
		TBUFF	*q = all_tbuffs->buff;
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
tb_alloc(TBUFF **p, ALLOC_T n)
{
	register TBUFF *q = *p;
	if (q == 0) {
		q = *p = typealloc(TBUFF);
		q->tb_data = typeallocn(char, q->tb_size = n);
		q->tb_used = 0;
		q->tb_last = 0;
		q->tb_endc = abortc;
		q->tb_data[0] = 0;	/* appease Purify */
		AllocatedBuffer(q)
	} else if (n >= q->tb_size) {
		q->tb_data = typereallocn(char, q->tb_data, q->tb_size = (n*2));
	}
	return q;
}

/*
 * (re)initialize a temp-buff
 */
TBUFF *
tb_init(TBUFF **p, int c)
{
	register TBUFF *q = *p;
	if (q == 0)
		q = tb_alloc(p, NCHUNK);
	q->tb_used = 0;
	q->tb_last = 0;
	q->tb_endc = c;		/* code to return if no-more-data */
	return (*p = q);
}

/*
 * deallocate a temp-buff
 */
void
tb_free(TBUFF **p)
{
	register TBUFF *q = *p;

	if (q != 0) {
		FreedBuffer(q)
		free(q->tb_data);
		free((char *)q);
	}
	*p = 0;
}

/*******(storage)************************************************************/

/*
 * put a character c at the nth position of the temp-buff
 */
TBUFF *
tb_put(TBUFF **p, ALLOC_T n, int c)
{
	register TBUFF *q;

	if ((q = tb_alloc(p, n+1)) != 0) {
		q->tb_data[n] = (char)c;
		q->tb_used = n+1;
	}
	return (*p = q);
}

#if NEEDED
/*
 * stuff the nth character into the temp-buff -- assumes space already there
 *  it's sort of the opposite of tb_peek
 */
void
tb_stuff(TBUFF *p, int c)
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
tb_append(TBUFF **p, int c)
{
	register TBUFF *q = *p;
	register ALLOC_T n = (q != 0) ? q->tb_used : 0;

	return tb_put(p, n, c);
}

/*
 * insert a character into the temp-buff
 */
TBUFF *
tb_insert(TBUFF **p, ALLOC_T n, int c)
{
	register ALLOC_T m = tb_length(*p);
	register TBUFF *q = tb_append(p, c);

	if (q != 0 && n < m) {
		while (n < m) {
			q->tb_data[m] = q->tb_data[m-1];
			m--;
		}
		q->tb_data[m] = c;
	}
	return q;
}

/*
 * Copy one temp-buff to another
 */
TBUFF *
tb_copy(TBUFF **d, TBUFF *s)
{
	register TBUFF *p;

	if (s != 0) {
		if ((p = tb_init(d, s->tb_endc)) != 0)
			p = tb_bappend(d, s->tb_data, s->tb_used);
	} else
		p = tb_init(d, abortc);
	return p;
}

/*
 * append a binary data to the temp-buff
 */
TBUFF *
tb_bappend(TBUFF **p, const char *s, ALLOC_T len)
{
	register TBUFF *q = *p;
	register ALLOC_T n = (q != 0) ? q->tb_used : 0;

	if ((q = tb_alloc(p, n+len)) != 0) {
		memcpy(q->tb_data + n, s, len);
		q->tb_used = n+len;
	}
	return *p;
}
/*
 * append a string to the temp-buff
 */
TBUFF *
tb_sappend(TBUFF **p, const char *s)
{
	if (s != 0)
		(void) tb_bappend(p, s, strlen(s));
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
	TBUFF *p = 0;
	(void) tb_init(&p, EOS);
	return tb_bappend(&p, s, strlen(s));
}

/*******(retrieval)************************************************************/

#if DISP_X11
/*
 * get the nth character from the temp-buff
 */
int
tb_get(TBUFF *p, ALLOC_T n)
{
	register int	c = abortc;

	if (p != 0)
		c = (n < p->tb_used) ? p->tb_data[n] : p->tb_endc;

	return char2int(c);
}
#endif

/*
 * undo the last 'tb_put'
 */
void
tb_unput(TBUFF *p)
{
	if (p != 0
	 && p->tb_used != 0)
		p->tb_used -= 1;
}

/*******(iterators)************************************************************/

#if NEEDED
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
	return (p != 0) ? (p->tb_last < p->tb_used) : FALSE;
}

/*
 * get the next character from the temp-buff
 */
int
tb_next(TBUFF *p)
{
	if (p != 0)
		return tb_get(p, p->tb_last++);
	return abortc;
}
#endif

#if NEEDED
/*
 * undo a tb_next
 */
void
tb_unnext(TBUFF *p)
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
tb_peek(TBUFF *p)
{
	if (p != 0)
		return tb_get(p, p->tb_last);
	return abortc;
}
#endif  /* NEEDED */

/*******(bulk-data)************************************************************/

/*
 * returns a pointer to data, assumes it is one long string
 */
char *
tb_values(TBUFF *p)
{
	return (p != 0) ? p->tb_data : 0;
}

/*
 * returns the length of the data
 */
ALLOC_T
tb_length(TBUFF *p)
{
	return (p != 0) ? p->tb_used : 0;
}
