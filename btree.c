/*
 * $Id: btree.c,v 1.4 1998/04/28 10:15:52 tom Exp $
 * Copyright 1997 by Thomas E. Dickey
 *
 * Maintains a balanced binary tree (aka AVL tree) of unspecified nodes.  The
 * algorithm is taken from "The Art of Computer Programming -- Volume 3 --
 * Sorting and Searching", by Donald Knuth.  Knuth presents the insertion and
 * search algorithms in assembly language for MIX, and gives an overview of the
 * deletion algorithm for the "interested reader".
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if !defined(DEBUG_BTREE) && !defined(NDEBUG)
#define NDEBUG
#endif

#include <assert.h>

#include "btree.h"

#if defined(OPT_TRACE) || defined(DOALLOC)
#include "trace.h"
#endif

#ifdef USE_DBMALLOC
#include <dbmalloc.h>
#endif

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#define castalloc(cast,nbytes) (cast *)malloc(nbytes)

			/* definitions to make this simple, like Knuth */
#define	LLINK(p)	BI_LEFT(p)
#define	RLINK(p)	BI_RIGHT(p)
#define	KEY(p)		BI_KEY(p)
#define	B(p)		(p)->balance

#define COMPARE(a,b)	btree_strcmp(a,b)

#define	LINK(a,p)	(p)->links[(a)>0]

#ifdef DEBUG_BTREE
# ifndef TRACE
#  if DEBUG_BTREE > 0
#   define TRACE(p)	printf p ; fflush(stdout) ;
#  endif
# endif
#else
# undef TRACE
#endif

#if DEBUG_BTREE > 1
#define TRACE_TREE(s,p)	TRACE((s)) btree_printf(p)
#endif

#if DEBUG_BTREE > 2
#define TRACE_SUBTREE(s,h,p)	TRACE(s); dump_nodes(h,p,0)
#endif

#ifndef TRACE
#define TRACE(p)	/*nothing*/
#endif

#ifndef TRACE_TREE
#define TRACE_TREE(s,p)	/*nothing*/
#endif

#ifndef TRACE_SUBTREE
#define TRACE_SUBTREE(s,h,p)	/*nothing*/
#endif

#ifdef DEBUG_BTREE
static int btree_verify(BI_TREE *funcs, BI_NODE *p);
#endif

/*
 * FIXME
 */
static int
btree_strcmp(const char *a, const char *b)
{
	register int cmp = strcmp(a, b);
	if (cmp < 0)
		cmp = -1;
	else if (cmp > 0)
		cmp = 1;
	return cmp;
}

BI_DATA *
btree_insert(BI_TREE *funcs, BI_DATA *data)
{
				/* (A1:Initialize) */
register
	BI_NODE	*t = &(funcs->head),	/* 't' => to the father of 's'	*/
		*s = RLINK(t),		/* 's' => to rebalancing point	*/
		*p = RLINK(t),		/* 'p' => down the tree		*/
		*q,
		*r;
register
	int	a;
	BI_DATA	*value = 0;

	TRACE(("inserting '%s'\n", data->bi_key))
	if (p == 0) {
		RLINK(t) = p = (*funcs->allocat)(data);
		funcs->depth += 1;
		funcs->count += 1;
		return &(p->value);
	}
				/* (A2:Compare) */
	while ((a = COMPARE(data->bi_key, KEY(p))) != 0) {
				/* (A3,A4: move left/right accordingly)	*/
		if ((q = LINK(a,p)) != 0) {
			value = &(p->value);
			if (B(q)) {
				t = p;
				s = q;
			}
			p = q;
			/* ...continue comparing */
		} else {
			/* (A5:Insert) */
			LINK(a,p) = q = (*funcs->allocat)(data);
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
					p = LINK(a,p);
				}
			}

				/* (A7:Balancing act) */
			a = (COMPARE(data->bi_key, KEY(s)) < 0) ? -1 : 1;

			if (B(s) == 0) {
				/* ...the tree has grown higher	*/
				B(s) = a;
				funcs->depth += 1;
			} else if (B(s) == -a) {
				/* ...the tree has gotten more balanced	*/
				B(s) = 0;
			} else /* (B(s) == a) */ {
				assert(B(s) == a);
				/* ...the tree has gotten out of balance */
				TRACE(("...must rebalance\n"))
				if (B(r) == a) {
					TRACE(("(A8:Single rotation)\n"))
					p          = r;
					LINK(a,s)  = LINK(-a,r);
					LINK(-a,r) = s;

					B(s) = B(r) = 0;
				} else /* (B(r) == -a) */ {
					assert(B(r) == -a);
					TRACE(("(A9: Double rotation)\n"))
					p          = LINK(-a,r);
					LINK(-a,r) = LINK(a,p);
					LINK(a,p)  = r;
					LINK(a,s)  = LINK(-a,p);
					LINK(-a,p) = s;

					TRACE(("r=%s\n", KEY(r)))
					TRACE(("s=%s\n", KEY(s)))
					TRACE(("B(%s) = %d vs a = %d\n",
						KEY(p), B(p), a))

					if (B(p) == a)
						{ B(s) = -a; B(r) = 0;	}
					else if (B(p) == 0)
						{ B(s) =     B(r) = 0;  }
					else /* if (B(p) == -a) */
						{ B(s) = 0;  B(r) = a;  }

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

static BI_NODE *
parent_of(BI_TREE *funcs, BI_NODE *s)
{
	BI_NODE	*r = &(funcs->head),
		*p = RLINK(r);	 /* 'p' => down the tree	*/
	int a;

	while ((a = COMPARE(KEY(s), KEY(p))) != 0) {
		assert(LINK(a,p) != 0);
		r = p;
		p = LINK(a,p);
	}
	TRACE(("parent of '%s' is '%s'\n", KEY(s), KEY(r)))
	return r;
}

#define MAXSTK 20
#define PUSH(A,P) { \
	TRACE(("@%d:push #%d a=%2d, B=%2d, p='%s' -> '%s'\n", __LINE__, \
		k, A, B(P), P?KEY(P):"", LINK(A,P)?KEY(LINK(A,P)):"")) \
	stack[k].a = A;\
	stack[k].p = P;\
	k++; }

int
btree_delete(BI_TREE *funcs, const char *data)
{
	struct {
	BI_NODE	*p;
	int	a;
	} stack[MAXSTK];
	int	k = 0;
				/* (A1:Initialize) */
register
	BI_NODE	*t = &(funcs->head),
		*p = RLINK(t),
		*q, *r, *s;
register
	int	a, b;
	char	*value;

	if ((p = t) == 0
	 || (p = LINK(a=1,p)) == 0) {
		return 0;
	}
				/* (A2:Compare) */
	while ((a = COMPARE(data, value = KEY(p))) != 0) {
		if ((q = LINK(a,p)) == 0) {
			value = 0;
			break;
		}
				/* (A3,A4: move left/right accordingly)	*/
		t = p;
		p = LINK(a,p);
		/* ...continue comparing */
	}

	if (value != 0) {	/* we found the node to delete, in p */
		TRACE(("deleting node '%s'\n", value))
		q = p;
		p = t;
		a = (q == RLINK(p)) ? 1 : -1;

		TRACE(("@%d, p='%s'\n", __LINE__, p?KEY(p):""))
		TRACE(("@%d, q='%s'\n", __LINE__, q?KEY(q):""))
		TRACE(("@%d, a=%d\n",   __LINE__, a))

				/* D1: Is RLINK null? */
		if (RLINK(q) == 0) {
			TRACE(("D1\n"))

			LINK(a,p) = LLINK(q);
			s = p;
#if LATER
		} else if (LLINK(q) == 0) { /* D1.5 */
			TRACE(("D1.5\n"))
			LINK(a,p) = RLINK(q);
			s = RLINK(q);
#endif
		} else {	/* D2: Find successor */
			TRACE_SUBTREE("SUBTREE(p)-before\n", funcs, p);
			r = RLINK(q);
			if (LLINK(r) == 0) {
				TRACE(("D2, r='%s', q='%s'\n", KEY(r), KEY(q)))
				TRACE(("DIFF: %d\n", B(r) - B(q)))

				LLINK(r) = LLINK(q);
				LINK(a,p) = r;
				s = r;
				TRACE(("(D2) replace balance %2d with %2d, a=%2d\n", B(s), B(q), a))
				B(s) = B(q);
				a = 1;	/* for RLINK(q) */

				TRACE_SUBTREE("SUBTREE(p)-after\n", funcs, p);
				TRACE(("@%d, p='%s'\n", __LINE__, KEY(p)))
				TRACE(("@%d, r='%s'\n", __LINE__, KEY(r)))
			} else { /* D3: Find null LLINK */
				TRACE(("D3: find null LLINK\n"))

				TRACE(("@%d, r='%s'\n", __LINE__, r?KEY(r):""))
				s = r;
				do {
					r = s;
					s = LLINK(r);
				} while (LLINK(s) != 0);

				LLINK(s) = LLINK(q);
				LLINK(r) = RLINK(s);
				RLINK(s) = RLINK(q);
				TRACE(("@%d, p='%s', a=%d\n", __LINE__, KEY(p), a))
				TRACE(("@%d, r='%s'\n", __LINE__, KEY(r)))
				TRACE(("@%d, s='%s'\n", __LINE__, KEY(s)))
				LINK(a,p) = s;
				TRACE(("(D3) replace balance %2d with %2d, a=%2d\n", B(s), B(q), a))
				B(s) = B(q);
				s = r;
				a = -1;	/* ...since we followed left */
			}
		}
		b = a;
				/* D4: Free the node */
		assert(q != 0);
		(*funcs->dealloc)(q);
		funcs->count -= 1;
		TRACE_TREE("after delinking:",funcs);

				/* Construct the auxiliary stack */
		a = 1;
		q = &(funcs->head);
		if (s != 0
		 && s != q
		 && q != 0) {
			TRACE(("Construct stack from '%s' down to '%s', final a=%d\n",
				q?KEY(q):"",
				s?KEY(s):"",
				b))
			while (s != q) {
				PUSH(a,q);
				q = LINK(a,q);
				a = COMPARE(KEY(s), KEY(q));
			}
			PUSH(b,q);
			TRACE(("...done building stack\n"))
		}

				/* Rebalance the tree */
		for (;;) {
			if (--k <= 0) {
				TRACE(("shorten the whole tree\n"))
				funcs->depth -= 1;
				break;
			}
			p = stack[k].p;
			a = stack[k].a;
			TRACE(("processing #%d '%s' B = %d, a = %d (%p)\n",
				k, KEY(p), B(p), a, LINK(a,p)))
			if (B(p) == a) {
				TRACE(("Case (i)\n"))
				B(p) = 0;
			} else if (B(p) == 0) {
				TRACE(("Case (ii)\n"))
				B(p) = -a;
				break;
			} else {
				TRACE(("Case (iii): Rebalancing needed!\n"))

				q = LINK(-a,p);
				assert(q != 0);
				assert(k >  0);
				assert(B(p) == -a);

				t = stack[k-1].p;
				TRACE(("t:%s, balance:%d\n", KEY(t), B(t)))
				TRACE(("A:p:%s, balance:%d\n", KEY(p), B(p)))
				TRACE(("B:q:%s, balance:%d\n", KEY(q), B(q)))
				TRACE_TREE("before rebalancing:", funcs);

				if (B(q) == -a) {
					TRACE(("CASE 1: single rotation\n"))

					r          = q;

					TRACE(("link LINK(-a,p) -> LINK( a,q) = '%s'\n", LINK(a,q)?KEY(LINK(a,q)):""))
					TRACE(("link LINK( a,q) -> p          = '%s'\n", p?KEY(p):""))

					LINK(-a,p) = LINK(a,q);
					LINK(a,q)  = p;

					B(p) = B(q) = 0;

					t = parent_of(funcs, p);

					TRACE(("Finish by linking '%s' to %sLINK(%s)\n",
						KEY(r),
						(p == RLINK(t))
							? "R"
							: (p == LLINK(t))
								? "L" : "?",
						KEY(t)))

					t->links[(p == RLINK(t))] = r;

				} else if (B(q) == a) {
					TRACE(("CASE 2: double rotation\n"))

					r          = LINK(a,q);
#if DEBUG_BTREE > 1
					TRACE(("a = '%d'\n", a))
					TRACE(("p = '%s'\n", p?KEY(p):""))
					TRACE(("q = '%s'\n", q?KEY(q):""))
					TRACE(("r = '%s'\n", r?KEY(r):""))
					TRACE(("link LINK( a,q) -> LINK(-a,r) = '%s'\n", LINK(-a,r)?KEY(LINK(-a,r)):""))
					TRACE(("link LINK(-a,r) -> q          = '%s'\n", KEY(q)))
					TRACE(("link LINK(-a,p) -> LINK(a,r)  = '%s'\n", LINK(a,r)?KEY(LINK(a,r)):""))
					TRACE(("link LINK( a,r) -> p          = '%s'\n", KEY(p)))
#endif
					LINK(a,q)  = LINK(-a,r);
					LINK(-a,r) = q;
					LINK(-a,p) = LINK(a,r);
					LINK(a,r)  = p;

					TRACE(("compute balance for '%s', %d vs a=%d\n",
						KEY(r), B(r), a))

					if (B(r) == -a)
						{ B(p) = a;  B(q) = 0;	}
					else if (B(r) == 0)
						{ B(p) =     B(q) = 0;  }
					else /* (B(r) == a) */
						{ B(p) = 0;  B(q) = -a;  }

					B(r) = 0;

					a = stack[k-1].a;
					t = stack[k-1].p;

					TRACE(("Finish by linking '%s' to %sLINK(%s)\n",
						KEY(r),
						(a>0) ? "R" : "L",
						KEY(t)))

					LINK(a,t) = r;

				} else {
					TRACE(("CASE 3: single rotation, end\n"))

					r          = q;
					LINK(-a,p) = LINK(a,q);
					LINK(a,q)  = p;

					B(q) = -B(p);

					LINK(stack[k-1].a,stack[k-1].p) = q;
					break;
				}

				TRACE_TREE("after rebalancing:", funcs);

			}
		}
	}
	return (value != 0);
}

BI_DATA *
btree_search(BI_TREE *funcs, const char *data)
{
				/* (A1:Initialize) */
register
	BI_NODE	*t = &(funcs->head),	/* 't' => to the father of 's'	*/
		*p = RLINK(t),		/* 'p' => down the tree		*/
		*q;
register
	int	a;

	if (p == 0) {
		return 0;
	}
				/* (A2:Compare) */
	while ((a = COMPARE(data, KEY(p))) != 0) {
				/* (A3,A4: move left/right accordingly)	*/
		if ((q = LINK(a,p)) != 0) {
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
btree_count(BI_NODE *p)
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
btree_depth(BI_NODE *p)
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
dump_nodes(BI_TREE *funcs, BI_NODE * p, int level)
{
	if (p) {
		dump_nodes(funcs, LLINK(p),  level+1);
		(*funcs->display)(p, level);
#if DEBUG_BTREE > 0
		if (LLINK(p) != 0
		 && COMPARE(KEY(LLINK(p)), KEY(p)) > 0)
			TRACE((" OOPS:L"))
		if (RLINK(p) != 0
		 && COMPARE(KEY(RLINK(p)), KEY(p)) < 0)
			TRACE((" OOPS:R"))
		TRACE((" %d", btree_depth(p)))
#endif
#if DEBUG_BTREE >= BTREE_DRIVER
		printf("\n");
#endif
		dump_nodes(funcs, RLINK(p), level+1);
	}
}

/*
 * Invoke display function for each node
 */
void
btree_printf(BI_TREE * funcs)
{
	TRACE(("TREE, depth %d, count %d\n", funcs->depth, funcs->count))
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
btree_pmatch(BI_NODE *n, const int mode, const char *name)
{
	BI_NODE *m;
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
btree_pcount(BI_NODE *node, char *matchname, size_t len)
{
	int left = 0, right = 0, me = 0;

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
build_parray(BI_NODE *head, char *matchname, size_t len, const char **nptr, int *i)
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
btree_parray(BI_TREE *tree, char *name, unsigned len)
{
	BI_NODE *top;
	const char **nptr = 0;
	top = btree_pmatch(BI_RIGHT(&(tree->head)), 0, name);

	if (top != 0) {
		int i = 0;
		size_t cnt = btree_pcount(top, name, len);
		nptr = castalloc(const char *, sizeof(const char *) * (cnt + 1));
		if (nptr != 0) {
			build_parray(top, name, len, nptr, &i);
			nptr[i] = 0;
		}
	}
	return nptr;
}

int btree_freeup(BI_TREE *funcs)
{
	TRACE(("Deleting all nodes...\n"))
	TRACE_TREE("INITIAL-", funcs);
	while (RLINK(&(funcs->head))) {
		TRACE(("Try to delete '%s'\n", KEY(RLINK(&(funcs->head)))))
		if (!btree_delete(funcs, KEY(RLINK(&(funcs->head))))) {
			TRACE(("Try-delete failed\n"))
			return 0;
		}
#ifdef DEBUG_BTREE
		TRACE_TREE("AFTER-DELETE, ", funcs);
		if (!btree_verify(funcs, RLINK(&(funcs->head)))) {
			TRACE(("Try-verify failed\n"))
			return 0;
		}
#endif
	}
	return 1;
}

/******************************************************************************/

#ifdef DEBUG_BTREE

static int
btree_verify(BI_TREE *funcs, BI_NODE *p)
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
					count, funcs->count))
			}
		}
		/* verify the balance factor */
		if ((balance = btree_depth(r) - btree_depth(l)) != 0) {
			if (balance > 0) {
				if (balance > 1) {
					ok = 0;
					TRACE(("OOPS: '%s' is unbalanced\n", KEY(p)))
				}
				balance = 1;
			} else {
				if (balance < -1) {
					ok = 0;
					TRACE(("OOPS: '%s' is unbalanced\n", KEY(p)))
				}
				balance = -1;
			}
		}
		if (B(p) != balance) {
			ok = 0;
			TRACE(("OOPS: Balance '%s' have %d vs %d\n",
				KEY(p), B(p), balance))
		}

		/* verify that the nodes are in correct order */
		compare = COMPARE(
				(l != 0) ? KEY(l) : "",
				(r != 0) ? KEY(r) : "\377");
		if (compare >= 0) {
			ok = 0;
			TRACE(("OOPS: Compare %s, have %d vs -1\n",
				KEY(p), compare))
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
#define typecalloc(cast) (cast *)calloc(sizeof(cast),1)

static BI_NODE *
new_node (BI_DATA * data)
{
	BI_NODE *result = typecalloc(BI_NODE);
	result->value = *data;
	result->value.bi_key = strdup(data->bi_key); /* DEBUG-only */
	return result;
}

static void
old_node (BI_NODE *node)
{
	free(node);
}

static void
dpy_node (BI_NODE *a, int level)
{
	while (level-- > 0)
		printf(". ");
	printf("%s (%d)", KEY(a), B(a));
	fflush(stdout);
}

static	BI_TREE	text_tree = {
	new_node,
	old_node,
	dpy_node
	};

/******************************************************************************/

#define MAX_VEC 10000

int main(int argc, char *argv[])
{
	BI_DATA temp;
	BI_NODE *top;
	int n, m;
	int done = 0;
	char buffer[BUFSIZ];
	int vector[MAX_VEC];
	const char **list;

	memset(&temp, 0, sizeof(temp));
	for (n = 1; n < argc; n++) {
		temp.bi_key = argv[n];
		btree_insert(&text_tree, &temp);
	}

	btree_printf(&text_tree);
	btree_verify(&text_tree, RLINK(&(text_tree.head)));

	while (!done && fgets(buffer, sizeof(buffer), stdin) != 0) {
		char *t = buffer;
		char *s = t + strlen(t);

		if (s != buffer && *--s == '\n')
			*s = 0;

		switch (*t++) {
		default:
			printf("Commands are f(ind) i(nsert), d(elete), l(ist), p(rint), r(andom)\n");
			break;
		case 'f':
			n = (btree_search(&text_tree, t) != 0);
			printf("** found(%s) %d\n", t, n);
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
				free(list);
			}
			else
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
			for (n = 0; n < MAX_VEC; n++) {
				vector[n] = random();
				sprintf(buffer, "%d", vector[n]);
				temp.bi_key = buffer;
				(void) btree_insert(&text_tree, &temp);
				printf("** inserted(%s)\n", buffer);
			}
			for (m = 0; m < 2; m++)
			for (n = 0; n < MAX_VEC; n++) {
				unsigned delete = random() & 1;
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
					return (1);
				}
				printf("** result: %d\n", ok);
			}
			break;
		case 'q':
			done = 1;
			break;
		case '#':
			break;
		}
		btree_verify(&text_tree, RLINK(&(text_tree.head)));
	}
	btree_freeup(&text_tree);

	printf("done!\n\n");
	return 0;
}
#endif /* DEBUG_BTREE >= BTREE_DRIVER */

#endif /* DEBUG_BTREE */
