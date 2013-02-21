/*
 * $Id: btree.c,v 1.54 2013/02/21 09:53:42 tom Exp $
 * Copyright 1997-2010,2013 by Thomas E. Dickey
 *
 * Maintains a balanced binary tree (aka AVL tree) of unspecified nodes.  The
 * algorithm is taken from "The Art of Computer Programming -- Volume 3 --
 * Sorting and Searching", by Donald Knuth.  Knuth presents the insertion and
 * search algorithms in assembly language for MIX, and gives an overview of the
 * deletion algorithm for the "interested reader".
 *
 * See 6.2.2 for deletion, as well as notes in 6.2.3 for rebalancing.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if !defined(DEBUG_BTREE) && !defined(NDEBUG)
#define NDEBUG
#endif

#if defined(DEBUG_BTREE)
#if defined(OPT_TRACE) || defined(DOALLOC)
#include "trace.h"
#endif

#ifdef USE_DBMALLOC
#include <dbmalloc.h>
#endif

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#define castalloc(cast,nbytes) (cast *)malloc((size_t) nbytes)
#define	for_ever for(;;)
#define beginDisplay()		/* nothing */
#define endofDisplay()		/* nothing */

#define	NonNull(s)	((s == 0) ? "" : s)

#include <time.h>

#else
#include "estruct.h"
#include "edef.h"
#endif

#include <assert.h>

#include "btree.h"

			/* definitions to make this simple, like Knuth */
#define	LLINK(p)	BI_LEFT(p)
#define	RLINK(p)	BI_RIGHT(p)
#define	KEY(p)		BI_KEY(p)
#define	B(p)		(p)->balance

#define COMPARE(a,b)	btree_strcmp(a,b)

#define	LINK(a,p)	(p)->links[(a)>0]

#ifdef DEBUG_BTREE
# ifndef TRACE
#  if DEBUG_BTREE > 1
#   define TRACE(p)	printf p ; fflush(stdout)
#  endif
# endif
#elif 1
# undef TRACE
# undef OPT_TRACE
# define OPT_TRACE 0
# define DEBUG_BTREE -1
#else
# define DEBUG_BTREE -1
#endif

#if DEBUG_BTREE > 1
#define TRACE_TREE(s,p)	TRACE((s)); btree_printf(p)
#endif

#if DEBUG_BTREE > 2
#define TRACE_SUBTREE(s,h,p)	TRACE((s)); dump_nodes(h,p,0)
#endif

#ifndef TRACE
#define TRACE(p)		/*nothing */
#endif

#ifndef TRACE_TREE
#define TRACE_TREE(s,p)		/*nothing */
#endif

#ifndef TRACE_SUBTREE
#define TRACE_SUBTREE(s,h,p)	/*nothing */
#endif

#if DEBUG_BTREE >= 0
static int btree_verify(BI_TREE * funcs, BI_NODE * p);
static void dump_nodes(BI_TREE * funcs, BI_NODE * p, int level);
#endif

/*
 * FIXME - this should compare unsigned chars to be completely consistent.
 */
static int
btree_strcmp(const char *a, const char *b)
{
    int cmp = strcmp(a, b);
    if (cmp < 0)
	cmp = -1;
    else if (cmp > 0)
	cmp = 1;
    return cmp;
}

BI_DATA *
btree_insert(BI_TREE * funcs, BI_DATA * data)
{
    /* (A1:Initialize) */

    BI_NODE *t = &(funcs->head);	/* 't' => to the father of 's'  */
    BI_NODE *s = RLINK(t);	/* 's' => to rebalancing point  */
    BI_NODE *p = RLINK(t);	/* 'p' => down the tree         */
    BI_NODE *q;
    BI_NODE *r;

    int a;
    BI_DATA *value = 0;

    TRACE(("btree_insert(%p,%s)\n", funcs, data->bi_key));
    if (p == 0) {
	if ((p = (*funcs->allocat) (data)) == 0)
	    return 0;

	RLINK(t) = p;
	funcs->depth += 1;
	funcs->count += 1;
	return &(p->value);
    }
    /* (A2:Compare) */
    while ((a = COMPARE(data->bi_key, KEY(p))) != 0) {
	/* (A3,A4: move left/right accordingly) */
	if ((q = LINK(a, p)) != 0) {
	    value = &(p->value);
	    if (B(q)) {
		t = p;
		s = q;
	    }
	    p = q;
	    /* ...continue comparing */
	} else {
	    /* (A5:Insert) */
	    if ((q = (*funcs->allocat) (data)) == 0)
		return 0;

	    LINK(a, p) = q;
	    funcs->count += 1;
	    value = &(q->value);

	    /* (A6:Adjust balance factors) */
	    /*
	     * Now the balance factors on nodes between 's' and 'q'
	     * need to be changed from zero to +/- 1.
	     */
	    if (COMPARE(data->bi_key, KEY(s)) < 0)
		r = p = LLINK(s);
	    else
		r = p = RLINK(s);

	    while (p != q) {
		if ((a = COMPARE(data->bi_key, KEY(p))) != 0) {
		    B(p) = (a < 0) ? -1 : 1;
		    p = LINK(a, p);
		}
	    }

	    /* (A7:Balancing act) */
	    a = (COMPARE(data->bi_key, KEY(s)) < 0) ? -1 : 1;

	    if (B(s) == 0) {
		/* ...the tree has grown higher */
		B(s) = a;
		funcs->depth += 1;
	    } else if (B(s) == -a) {
		/* ...the tree has gotten more balanced */
		B(s) = 0;
	    } else {		/* (B(s) == a) */
		assert(B(s) == a);
		/* ...the tree has gotten out of balance */
		TRACE(("...must rebalance\n"));
		if (B(r) == a) {
		    TRACE(("(A8:Single rotation)\n"));
		    p = r;
		    LINK(a, s) = LINK(-a, r);
		    LINK(-a, r) = s;

		    B(s) = B(r) = 0;
		} else {	/* (B(r) == -a) */
		    assert(B(r) == -a);
		    TRACE(("(A9: Double rotation)\n"));
		    p = LINK(-a, r);
		    assert(p != 0);
		    if (p == 0) {
			TRACE(("(BUG: null pointer)\n"));
			break;
		    }
		    LINK(-a, r) = LINK(a, p);
		    LINK(a, p) = r;
		    LINK(a, s) = LINK(-a, p);
		    LINK(-a, p) = s;

		    TRACE(("r=%s\n", KEY(r)));
		    TRACE(("s=%s\n", KEY(s)));
		    TRACE(("B(%s) = %d vs a = %d\n",
			   KEY(p), B(p), a));

		    if (B(p) == a) {
			B(s) = -a;
			B(r) = 0;
		    } else if (B(p) == 0) {
			B(s) = 0;
			B(r) = 0;
		    } else {	/* if (B(p) == -a) */
			B(s) = 0;
			B(r) = a;
		    }

		    B(p) = 0;
		}
		/* A10:Finishing touch */
		t->links[(s == RLINK(t))] = p;
	    }
	    break;
	}
    }
    return (value);
}

#define MAXSTK 100

typedef struct {
    BI_NODE *ptr;
    int dir;
} STACKDATA;

static BI_NODE *
single_rotation(BI_NODE * item, int dir)
{
    BI_NODE *temp = item->links[!dir];

    item->links[!dir] = temp->links[dir];
    temp->links[dir] = item;

    return temp;
}

static BI_NODE *
double_rotation(BI_NODE * item, int dir)
{
    BI_NODE *temp = item->links[!dir]->links[dir];

    item->links[!dir]->links[dir] = temp->links[!dir];
    temp->links[!dir] = item->links[!dir];
    item->links[!dir] = temp;

    temp = item->links[!dir];
    item->links[!dir] = temp->links[dir];
    temp->links[dir] = item;

    return temp;
}

static void
readjust_balance(BI_NODE * item, int dir, int balance)
{
    BI_NODE *next = item->links[dir];
    BI_NODE *last = next->links[!dir];

    if (last->balance == 0) {
	item->balance = next->balance = 0;
    } else if (last->balance == balance) {
	item->balance = -balance;
	next->balance = 0;
    } else {
	/* last->balance == -balance */
	item->balance = 0;
	next->balance = balance;
    }

    last->balance = 0;
}

static BI_NODE *
remove_balance(BI_NODE * item, int dir, int *done)
{
    BI_NODE *next = item->links[!dir];
    int balance = (dir == 0) ? -1 : 1;

    if (next->balance == -balance) {
	item->balance = next->balance = 0;
	item = single_rotation(item, dir);
    } else if (next->balance == balance) {
	readjust_balance(item, !dir, -balance);
	item = double_rotation(item, dir);
    } else {
	/* next->balance == 0 */
	item->balance = -balance;
	next->balance = balance;
	item = single_rotation(item, dir);
	*done = 1;
    }

    return item;
}

int
btree_delete(BI_TREE * funcs, const char *data)
{
    TRACE(("btree_delete(%p,%s)\n", funcs, NonNull(data)));

    if (funcs->count != 0) {
	STACKDATA stack[MAXSTK + 1];
	BI_NODE *item;
	int top = 0;
	int done = 0;

	/* (A1:Initialize) */
	item = &(funcs->head);
	if (item == 0
	    || (item = LINK(1, item)) == 0) {
	    TRACE(("...not found @%d\n", __LINE__));
	    return 0;
	}

	for (;;) {
	    /* (A2:Compare) */
	    if (item == NULL || KEY(item) == 0) {
		TRACE(("...not found @%d\n", __LINE__));
		return 0;
	    } else {
		if (COMPARE(KEY(item), data) == 0) {
		    break;
		}
	    }

	    /* Push direction and node onto stack */
	    assert(top < (MAXSTK - 1));
	    if (top >= MAXSTK) {
		TRACE(("(BUG: stack overflow)\n"));
		return 0;
	    }
	    stack[top].dir = (COMPARE(KEY(item), data) < 0);
	    stack[top].ptr = item;
	    ++top;

	    /* (A3,A4: move left/right accordingly) */
	    item = item->links[stack[top - 1].dir];
	}

	/* D1: Is RLINK null? */
	/* D1.5: Is LLINK null? */
	if (item->links[0] == NULL || item->links[1] == NULL) {
	    /* Which child is not null? */
	    int dir = item->links[0] == NULL;

	    /* Fix parent */
	    if (top != 0)
		stack[top - 1].ptr->links[stack[top - 1].dir] = item->links[dir];
	    else
		LINK(1, &(funcs->head)) = item->links[dir];

	    TRACE(("...freed %p @%d\n", BI_KEY(item), __LINE__));
	    (*funcs->dealloc) (item);
	    funcs->count -= 1;
	} else {
	    /* D2: Find the inorder successor */
	    BI_NODE *child = item->links[1];

	    /* Save the path */
	    assert(top < (MAXSTK - 1));
	    if (top >= (MAXSTK - 1)) {
		TRACE(("(BUG: stack overflow)\n"));
		return 0;
	    }
	    stack[top].dir = 1;
	    stack[top].ptr = item;
	    ++top;

	    /* D3: Find null LLINK */
	    while (child->links[0] != NULL) {
		stack[top].dir = 0;
		stack[top].ptr = child;
		++top;
		child = child->links[0];
	    }

	    /* Swap data */
	    (*funcs->exchang) (item, child);
	    /* Unlink successor and adjust parent */
	    stack[top - 1].ptr->links[stack[top - 1].ptr == item] =
		child->links[1];

	    /* D4: Free the node */
	    TRACE(("...freed %p @%d\n", BI_KEY(child), __LINE__));
	    (*funcs->dealloc) (child);
	    funcs->count -= 1;
	}

	/* Walk back up the stack */
	while (--top >= 0 && !done) {
	    /* Update balance factors */
	    stack[top].ptr->balance += (stack[top].dir != 0) ? -1 : +1;

	    /* Terminate, or rebalance as necessary */
	    if (abs(stack[top].ptr->balance) == 1) {
		break;
	    } else if (abs(stack[top].ptr->balance) > 1) {
		stack[top].ptr = remove_balance(stack[top].ptr,
						stack[top].dir, &done);

		/* Fix parent */
		if (top != 0) {
		    stack[top - 1].ptr->links[stack[top - 1].dir] =
			stack[top].ptr;
		} else {
		    LINK(1, &(funcs->head)) = stack[0].ptr;
		}
	    }
	}
    }

    TRACE(("...found @%d\n", __LINE__));
    return 1;
}

BI_DATA *
btree_search(BI_TREE * funcs, const char *data)
{
    /* (A1:Initialize) */

    BI_NODE *t = &(funcs->head);	/* 't' => to the father of 's'  */
    BI_NODE *p = RLINK(t);	/* 'p' => down the tree         */
    BI_NODE *q;

    int a;

    if (p == 0) {
	return 0;
    }
    /* (A2:Compare) */
    while ((a = COMPARE(data, KEY(p))) != 0) {
	/* (A3,A4: move left/right accordingly) */
	if ((q = LINK(a, p)) != 0) {
	    p = q;
	    /* ...continue comparing */
	} else {
	    break;
	}
    }
    return a ? 0 : &(p->value);
}

/******************************************************************************/

#ifndef BTREE_VERIFY
#define BTREE_VERIFY 1
#endif

#ifndef BTREE_DRIVER
#define BTREE_DRIVER 1
#endif

#if DEBUG_BTREE >= BTREE_VERIFY
static int
btree_count(BI_NODE * p)
{
    int count = 0;
    if (p != 0) {
	count = 1;
	count += btree_count(LLINK(p));
	count += btree_count(RLINK(p));
    }
    return count;
}
#endif

#if DEBUG_BTREE >= BTREE_VERIFY
static int
btree_depth(BI_NODE * p)
{
    int depth = 0;
    if (p != 0) {
	int l, r;
	assert(LLINK(p) != p);
	assert(RLINK(p) != p);
	l = btree_depth(LLINK(p));
	r = btree_depth(RLINK(p));
	depth = 1;
	if (l > r)
	    depth += l;
	else
	    depth += r;
    }
    return depth;
}
#endif

static void
dump_nodes(BI_TREE * funcs, BI_NODE * p, int level)
{
    if (p) {
	dump_nodes(funcs, LLINK(p), level + 1);
	(*funcs->display) (p, level);
#if DEBUG_BTREE > 1
	if (LLINK(p) != 0
	    && COMPARE(KEY(LLINK(p)), KEY(p)) > 0)
	    TRACE((" OOPS:L"));
	if (RLINK(p) != 0
	    && COMPARE(KEY(RLINK(p)), KEY(p)) < 0)
	    TRACE((" OOPS:R"));
	TRACE((" %d", btree_depth(p)));
#endif
#if DEBUG_BTREE >= BTREE_DRIVER
	printf("\n");
#endif
	dump_nodes(funcs, RLINK(p), level + 1);
    }
}

/*
 * Invoke display function for each node
 */
void
btree_printf(BI_TREE * funcs)
{
    TRACE(("TREE, depth %d, count %d\n", funcs->depth, funcs->count));
    dump_nodes(funcs, RLINK(&(funcs->head)), 0);
}

/******************************************************************************/

/*
 * Find the the matching entry given a name in the tree.  Match according to
 * the 'mode' parameter:
 *
 *	If it's FALSE then only the first len characters must match and this
 *	will return the parent of the subtree that contains these entries (so
 *	that an inorder walk can find the other matches).
 *
 *	Use TRUE to force this to return the first of the ordered list of
 *	partial matches; we need this behavior for interactive name completion.
 */
BI_NODE *
btree_pmatch(BI_NODE * n, const int mode, const char *name)
{
    BI_NODE *m = 0;
    size_t len = strlen(name);

    while (n != 0) {
	int cmp;

	cmp = strncmp(name, BI_KEY(n), len);

	if (cmp == 0) {
	    if (mode
		&& (m = btree_pmatch(BI_LEFT(n), mode, name)) != 0)
		n = m;
	    return n;
	} else if (cmp < 0) {
	    n = BI_LEFT(n);
	} else {
	    n = BI_RIGHT(n);
	}
    }
    return 0;
}

/*
 * Find the size of the binary-subtree with the matching name.
 */
static int
btree_pcount(BI_NODE * node, char *matchname, size_t len)
{
    int left = 0;
    int right = 0;
    int me = 0;

    if (BI_LEFT(node) != 0) {
	left = btree_pcount(BI_LEFT(node), matchname, len);
    }

    if ((len == 0)
	|| (strncmp(BI_KEY(node), matchname, len) == 0))
	me = 1;

    if (BI_RIGHT(node) != 0) {
	right = btree_pcount(BI_RIGHT(node), matchname, len);
    }

    return me + left + right;
}

static void
build_parray(BI_NODE * head, char *matchname, size_t len, const char **nptr, int *i)
{
    if (BI_LEFT(head) != 0) {
	build_parray(BI_LEFT(head), matchname, len, nptr, i);
    }

    if ((len == 0)
	|| (strncmp(BI_KEY(head), matchname, len) == 0)) {
	nptr[*i] = BI_KEY(head);
	(*i)++;
    }

    if (BI_RIGHT(head) != 0) {
	build_parray(BI_RIGHT(head), matchname, len, nptr, i);
    }
}

/*
 * Build an array of the keys that partially-match the given name to at least
 * the given length 'len'.
 */
const char **
btree_parray(BI_TREE * tree, char *name, size_t len)
{
    BI_NODE *top;
    const char **nptr = 0;
    top = btree_pmatch(BI_RIGHT(&(tree->head)), 0, name);

    if (top != 0) {
	int i = 0;
	size_t cnt = (size_t) btree_pcount(top, name, len);
	beginDisplay();
	nptr = castalloc(const char *, sizeof(const char *) * (cnt + 1));
	endofDisplay();
	if (nptr != 0) {
	    build_parray(top, name, len, nptr, &i);
	    nptr[i] = 0;
	}
    }
    return nptr;
}

int
btree_freeup(BI_TREE * funcs)
{
    TRACE(("Deleting all nodes...\n"));
    TRACE_TREE("INITIAL-", funcs);
    while (RLINK(&(funcs->head))) {
	TRACE(("Try to delete '%s'\n", KEY(RLINK(&(funcs->head)))));
	if (!btree_delete(funcs, KEY(RLINK(&(funcs->head))))) {
	    TRACE(("Try-delete failed\n"));
	    return 0;
	}
#if DEBUG_BTREE >= 0
	TRACE_TREE("AFTER-DELETE, ", funcs);
	if (!btree_verify(funcs, RLINK(&(funcs->head)))) {
	    TRACE(("Try-verify failed\n"));
	    return 0;
	}
#endif
    }
    return 1;
}

/******************************************************************************/

#if DEBUG_BTREE >= 0

static int
btree_verify(BI_TREE * funcs, BI_NODE * p)
{
#if DEBUG_BTREE >= BTREE_VERIFY
    int ok = 1;

    if (p != 0) {
	int root = (p == &(funcs->head));
	BI_NODE *l = root ? 0 : LLINK(p);
	BI_NODE *r = RLINK(p);
	int balance = 0;
	int compare = 0;

	if (p == RLINK(&(funcs->head))) {
	    int count = btree_count(p);
	    if (count != funcs->count) {
		ok = 0;
		TRACE(("OOPS: Counted %d vs %d in header\n",
		       count, funcs->count));
	    }
	}
	/* verify the balance factor */
	if ((balance = btree_depth(r) - btree_depth(l)) != 0) {
	    if (balance > 0) {
		if (balance > 1) {
		    ok = 0;
		    TRACE(("OOPS: '%s' is unbalanced: %d\n", KEY(p), balance));
		}
		balance = 1;
	    } else {
		if (balance < -1) {
		    ok = 0;
		    TRACE(("OOPS: '%s' is unbalanced: %d\n", KEY(p), balance));
		}
		balance = -1;
	    }
	}
	if (B(p) != balance) {
	    ok = 0;
	    TRACE(("OOPS: Balance '%s' have %d vs %d\n",
		   KEY(p), B(p), balance));
	}

	/* verify that the nodes are in correct order */
	compare = COMPARE(
			     (l != 0) ? KEY(l) : "",
			     (r != 0) ? KEY(r) : "\377");
	if (compare >= 0) {
	    ok = 0;
	    TRACE(("OOPS: Compare %s, have %d vs -1\n",
		   KEY(p), compare));
	}

	/* recur as needed */
	ok &= btree_verify(funcs, l);
	ok &= btree_verify(funcs, r);
    }
    return ok;
#else
    return 1;
#endif
}

#if DEBUG_BTREE >= BTREE_DRIVER

/******************************************************************************/
#undef typecalloc
#define typecalloc(cast) (cast *)calloc(sizeof(cast), (size_t) 1)

static BI_NODE *
new_node(BI_DATA * data)
{
    BI_NODE *result = typecalloc(BI_NODE);
    result->value = *data;
    result->value.bi_key = strdup(data->bi_key);	/* DEBUG-only */
    return result;
}

static void
old_node(BI_NODE * node)
{
    beginDisplay();
    free(BI_KEY(node));
    free(node);
    endofDisplay();
}

static void
dpy_node(BI_NODE * a, int level)
{
    while (level-- > 0)
	printf(". ");
    printf("%s (%d)", KEY(a), B(a));
    fflush(stdout);
}

static void
xcg_node(BI_NODE * a, BI_NODE * b)
{
    BI_DATA value = a->value;
    a->value = b->value;
    b->value = value;
}

static BI_TREE text_tree =
{
    new_node,
    old_node,
    dpy_node,
    xcg_node,
    0,
    0,
    {
	{0, 0}, 0,
	{0, 0}
    }
};

/******************************************************************************/

#define MAX_VEC 10000

static void
test_btree(FILE *fp)
{
    BI_DATA temp;
    BI_NODE *top;
    int n, m;
    int done = 0;
    char buffer[BUFSIZ];
    int vector[MAX_VEC];
    const char **list;

    memset(&temp, 0, sizeof(temp));

    while (!done && fgets(buffer, sizeof(buffer), fp) != 0) {
	char *t = buffer;
	char *s = t + strlen(t);

	if (s != buffer && *--s == '\n')
	    *s = 0;

	if (isalpha(*buffer))
	    printf("%s\n", buffer);

	switch (*t++) {
	case '[':
	case ']':
	case '!':
	case '#':
	case '*':
	    break;
	default:
	    printf("Commands are f(ind) i(nsert), d(elete), l(ist), p(rint), r(andom)\n");
	    break;
	case 'f':
	    n = (btree_search(&text_tree, t) != 0);
	    printf("** find(%s) %d\n", t, n);
	    break;
	case 'i':
	    temp.bi_key = t;
	    n = (btree_insert(&text_tree, &temp) != 0);
	    printf("** insert(%s) %d\n", t, n);
	    break;
	case 'd':
	    n = btree_delete(&text_tree, t);
	    printf("** delete(%s) %d\n", t, n);
	    break;
	case 'l':
	    if ((list = btree_parray(&text_tree, t, strlen(t))) != 0) {
		printf("** list(%s)\n", t);
		for (n = 0; list[n] != 0; n++)
		    printf("[%d] '%s'\n", n, list[n]);
		free((char *) list);
	    } else
		printf("** list(%s) fail\n", t);
	    break;
	case 'L':
	    if ((top = btree_pmatch(RLINK(&text_tree.head), 1, t)) != 0)
		printf("** List(%s) -> '%s'\n", t, KEY(top));
	    else
		printf("** List(%s) fail\n", t);
	    break;
	case 'p':
	    btree_printf(&text_tree);
	    break;
	case 'r':
	    srand((unsigned) time(NULL));
	    for (n = 0; n < MAX_VEC; n++) {
		vector[n] = rand();
		sprintf(buffer, "%d", vector[n]);
		temp.bi_key = buffer;
		(void) btree_insert(&text_tree, &temp);
		printf("** inserted(%s)\n", buffer);
	    }
	    for (m = 0; m < 2; m++)
		for (n = 0; n < MAX_VEC; n++) {
		    unsigned delete = rand() & 1;
		    char *name = delete ? "delete" : "insert";
		    int ok;

		    sprintf(buffer, "%d", vector[n]);
		    printf("** random %s (%s)\n", name, buffer);
		    temp.bi_key = buffer;
		    if (delete)
			ok = btree_delete(&text_tree, buffer);
		    else
			ok = btree_insert(&text_tree, &temp) != 0;
		    if (!btree_verify(&text_tree, RLINK(&(text_tree.head)))) {
			printf("OOPS: Random %s-verify '%s' failed\n", name, buffer);
			btree_printf(&text_tree);
			return;
		    }
		    printf("** result: %d\n", ok);
		}
	    break;
	case 'q':
	    done = 1;
	    break;
	}
	btree_verify(&text_tree, RLINK(&(text_tree.head)));
    }
}

int
main(int argc, char *argv[])
{
    if (argc > 1) {
	int n;

	for (n = 1; n < argc; ++n) {
	    FILE *fp = fopen(argv[n], "r");
	    if (fp != 0) {
		printf("** read commands from \"%s\"\n", argv[n]);
		test_btree(fp);
		fclose(fp);
	    } else {
		perror(argv[n]);
	    }
	}
    } else {
	printf("** read commands from <stdin>\n");
	test_btree(stdin);
    }

    btree_freeup(&text_tree);

    return 0;
}
#endif /* DEBUG_BTREE >= BTREE_DRIVER */

#endif /* DEBUG_BTREE */
