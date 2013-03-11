/*
 * $Id: blist.c,v 1.16 2013/03/11 00:04:11 tom Exp $
 * Copyright 2007-2008,2013 by Thomas E. Dickey
 *
 * Provide binary-search lookup of arrays of sorted structs.  The beginning of
 * each struct is a pointer to a string, which is the key by which the structs
 * are sorted.  The last entry in the array contains a struct whose key is
 * null.  That makes it simple to initialize the itemCount value when the
 * array contents are determined by ifdef's.
 */

#include	<estruct.h>
#include	<blist.h>

#define	ILLEGAL_NUM	-1
#define ItemOf(data,inx) *(const char * const*)((const void *)((const char *)(data->theList) + ((UINT) (inx) * data->itemSize)))

#define ItemToInx(data, item) \
	((UINT) ((const char *) item - (const char *) (data->theList)) \
	  / data->itemSize)

typedef struct {
    const char *name;
} ToFind;

#ifdef DEBUG_ME
static long total_calls;
static long total_compares;
static long total_linear;
static long total_limits;

#define COUNTER(which,n) which += n
#else
#define COUNTER(which,n)
#endif

/*
 * Returns the number of items in the BLIST, saving the number in the struct
 * for later use.
 */
int
blist_count(BLIST * data)
{
    if (data->itemCount < 0) {
	int n;
	for (n = 0; ItemOf(data, n) != 0; ++n) {
	    ;
	}
	data->itemCount = n;
    }
    return data->itemCount;
}

static int
exact_match(const void *keyobj, const void *arraymember)
{
    COUNTER(total_compares, 1);
    return strcmp(*(const char *const *) keyobj, (*(const char *const *) arraymember));
}

/*
 * Exact matches can use bsearch.
 */
int
blist_match(BLIST * data, const char *name)
{
    int rc = -1;
    int last = blist_count(data);
    void *check;
    ToFind dummy;

    dummy.name = name;
    check = bsearch(&dummy,
		    data->theList,
		    (size_t) last,
		    (size_t) data->itemSize,
		    exact_match);
    if (check != 0) {
	rc = (int) ItemToInx(data, check);
    }
    COUNTER(total_linear, rc + 1);
    COUNTER(total_limits, last);
    COUNTER(total_calls, 1);
    return rc;
}

/*
 * Given what may be part of the name (length in 'len'), look for a match.
 * If the result is ambiguous, do not match unless the given name exactly
 * matches one item.
 */
int
blist_pmatch(BLIST * data, const char *name, int len)
{
    int actual = (int) strlen(name);
    int rc = -1;
    int hi, lo, x0, x1, cmp;
    int last = blist_count(data);
    const char *item;
    const char *test;

    if (len < 0 || len > actual)
	len = actual;

    x1 = -1;
    hi = last - 1;
    lo = 0;
    do {
	x0 = x1;
	x1 = lo + (hi - lo) / 2;
	if (x0 == x1) {
	    if (++x1 > hi)
		break;
	}
	item = ItemOf(data, x1);
	cmp = strncmp(item, name, (size_t) len);
	if (cmp < 0) {
	    lo = (x1 == lo) ? (x1 + 1) : x1;
	} else if (cmp > 0) {
	    hi = (x1 == hi) ? (x1 - 1) : x1;
	} else {
	    rc = x1;

	    /*
	     * Check for an exact match...
	     */
	    COUNTER(total_compares, 1);
	    if (strcmp(item, name)) {
		if (x1 > lo) {
		    ToFind dummy;

		    dummy.name = name;
		    test = (const char *) bsearch(&dummy,
						  &ItemOf(data, lo),
						  (size_t) (x1 + 1 - lo),
						  (size_t) data->itemSize,
						  exact_match);
		    if (test) {
			rc = (int) ItemToInx(data, test);
			break;
		    }
		}
	    }

	    /*
	     * Now - if we have not found an exact match, check for ambiguity.
	     */
	    if (rc >= 0) {
		if (len > (int) strlen(item)) {
		    rc = -1;
		} else if (x1 < last - 1) {
		    COUNTER(total_compares, 2);
		    if (strcmp(item, name)
			&& !strncmp(ItemOf(data, x1 + 1), name, (size_t) len)) {
			rc = -1;
		    }
		}
	    }
	    break;
	}
    } while (hi >= lo);
    COUNTER(total_linear, rc + 2);
    COUNTER(total_limits, last);
    COUNTER(total_calls, 1);
    return rc;
}

/*
 * Reset the count because we have a new or reallocated list.
 */
void
blist_reset(BLIST * data, const void *newList)
{
    data->itemCount = -1;
    data->theList = newList;
}

void
blist_summary(void)
{
#ifdef DEBUG_ME
    if (total_calls) {
	printf("\r\nBLIST:\n");
	printf("   calls %ld, average table size %.1f\n",
	       total_calls,
	       (1.0 * total_limits) / total_calls);
	printf("   compares %6ld (%.1f%%)\n",
	       total_compares,
	       (100.0 * total_compares) / total_limits);
	printf("   linear   %6ld (%.1f%%)\n\n",
	       total_linear,
	       (100.0 * total_linear) / total_limits);
    }
#endif
}
