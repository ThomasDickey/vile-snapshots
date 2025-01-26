/*
 *
 *	fences.c
 *
 * Match up various fenceposts, like (), [], {}, #if, #el, #en as well as
 * comment begin/end.
 *
 * Original version probably by Dan Lawrence or Dave Conroy for MicroEMACS
 * Extensions for vile by Paul Fox
 * Rewrote to use regular expressions - T.Dickey
 *
 * $Id: fences.c,v 1.99 2025/01/26 11:43:27 tom Exp $
 */

#include	"estruct.h"
#include	"edef.h"

#if OPT_CFENCE

#define	CPP_UNKNOWN -1
#define	CPP_IF       0
#define	CPP_ELIF     1
#define	CPP_ELSE     2
#define	CPP_ENDIF    3

#define BLK_UNKNOWN -1
#define BLK_BEGIN    4
#define BLK_END      5

#if OPT_TRACE
#define TRACEARG(p) p,
#else
#define TRACEARG(p)		/*nothing */
#endif

#define ok_BLK(n) ((n) >= BLK_BEGIN && (n) <= BLK_END)

#define COMPLEX_FENCE_CH  -4
#define COMMENT_FENCE_CH  -3
#define PAIRED_FENCE_CH   -2
#define UNKNOWN_FENCE_CH  -1

#define S_COL(exp) (C_NUM)(exp->startp[0] - lvalue(DOT.l))
#define E_COL(exp) (C_NUM)(exp->endp[0]   - lvalue(DOT.l))

#define BlkBegin b_val_rexp(curbp, VAL_FENCE_BEGIN)->reg
#define BlkEnd   b_val_rexp(curbp, VAL_FENCE_END)->reg

#define CurrentChar() \
	(is_at_end_of_line(DOT) ? '\n' : CharAtDot())
#define InDirection(sdir) \
	((sdir == REVERSE) ? backchar(FALSE, 1) : forwchar(FALSE, 1))

#define direction_of(sdir) (((sdir) == FORWARD) ? "forward" : "backward")

#undef  min
#define min(a,b)	((a) < (b) ? (a) : (b))

#if !OPT_MAJORMODE
#define limit_iterations()	/* nothing */
#endif

	/*
	 * Start (or recur) a complex fence.
	 */
#define start_fence_op2(sdir, oldpos, oldpre) \
	TRACE(("...start_fence_op2 %s@%d\n", __FILE__, __LINE__)); \
	oldpos = DOT; \
	oldpre = pre_op_dot

	/*
	 * Start a simple fence.
	 *
	 * This adds a special case, which is handled differently for complex
	 * fences: ops are inclusive of the endpoint; account for this when
	 * saving pre_op_dot
	 */
#define start_fence_op(sdir, oldpos, oldpre) \
	start_fence_op2(sdir, oldpos, oldpre); \
	if (doingopcmd && sdir == REVERSE) { \
		forwchar(TRUE,1); \
		pre_op_dot = DOT; \
		backchar(TRUE,1); \
	}

#define test_fence_op(st, oldpos, oldpre) \
	TRACE(("...test_fence_op, status=%d %s@%d\n", st, __FILE__, __LINE__)); \
	if (st != TRUE) { \
		DOT = oldpos; \
		pre_op_dot = oldpre; \
	}

static int find_complex(int sdir, int *newkey);
static int complex_fence(int sdir, int key, int group, int level, int *newkey);
#if OPT_MAJORMODE
static int find_one_complex(int sdir, int level, int group, int *newkey);
static void limit_iterations(void);
#endif

#if OPT_MAJORMODE
static time_t final_time;
static long iterations;
#endif

static int
next_line(int sdir)
{
    DOT.o = b_left_margin(curbp);
    if (sdir == REVERSE) {
	DOT.l = lback(DOT.l);
    } else {
	DOT.l = lforw(DOT.l);
    }

    if (is_header_line(DOT, curbp)) {
	return FALSE;
    } else if (interrupted()) {
	kbd_alarm();
	return ABORT;
    }
    return TRUE;
}

#if OPT_TRACE
static const char *
typeof_complex(int code)
{
    const char *s;
    switch (code) {
    case CPP_IF:
	s = "CPP_IF";
	break;
    case CPP_ELIF:
	s = "CPP_ELIF";
	break;
    case CPP_ELSE:
	s = "CPP_ELSE";
	break;
    case CPP_ENDIF:
	s = "CPP_ENDIF";
	break;
    default:
	s = "CPP_UNKNOWN";
	break;
    }
    return s;
}
#endif

static int
match_complex(TRACEARG(int group)
	      LINE *lp, struct VAL *vals, int ic)
{
    static int modes[] =
    {CPP_IF, CPP_ELIF, CPP_ELSE, CPP_ENDIF};
    size_t j, k;
    int code = CPP_UNKNOWN;

#define any_rexp(bv,n) (bv[n].vp->r)
#define any_mode(bv,n) (bv[n].vp->i)

    for (j = 0; j < TABLESIZE(modes); j++) {
	switch (modes[j]) {
	case CPP_IF:
	    k = VAL_FENCE_IF;
	    break;
	case CPP_ELIF:
	    k = VAL_FENCE_ELIF;
	    break;
	case CPP_ELSE:
	    k = VAL_FENCE_ELSE;
	    break;
	case CPP_ENDIF:
	    k = VAL_FENCE_FI;
	    break;
	default:
	    continue;
	}
	if (lregexec(any_rexp(vals, k)->reg, lp, 0, llength(lp), ic)) {
	    code = modes[j];
	    TRACE(("match_complex(%d) %s\n", group, typeof_complex(code)));
	    break;
	}
    }

    return code;
}

/*
 * Find the match, if any, for a begin/end comment marker.  If we find a
 * match, the regular expression will overlap the given LINE/offset.
 */
static int
match_simple(void)
{
    C_NUM first;
    C_NUM last = llength(DOT.l);

    TRACE(("match_simple %d:%s\n", line_no(curbp, DOT.l), lp_visible(DOT.l)));
    for (first = 0; first < last; first = S_COL(BlkBegin) + 1) {
	if (!lregexec(BlkBegin, DOT.l, first, last, FALSE))
	    break;
	if ((S_COL(BlkBegin) <= DOT.o)
	    && (E_COL(BlkBegin) > DOT.o)) {
	    TRACE(("...match_simple BLK_BEGIN\n"));
	    return BLK_BEGIN;
	}
    }

    for (first = 0; first < last && DOT.o <= last; last = E_COL(BlkEnd) - 1) {
	if (!lregexec(BlkEnd, DOT.l, first, last, FALSE))
	    break;
	if ((S_COL(BlkEnd) <= DOT.o)
	    && (E_COL(BlkEnd) > DOT.o)) {
	    TRACE(("...match_simple BLK_END\n"));
	    return BLK_END;
	}
	if (last >= E_COL(BlkEnd) - 1)
	    break;
    }

    TRACE(("...match_simple BLK_UNKNOWN\n"));
    return BLK_UNKNOWN;
}

#define TRACE_COMPLEX TRACE(("complex_fence(%d:%d:%d) @%d\n", level, group, count, line_no(curbp, DOT.l)))

static int
complex_fence(int sdir, int key, int group, int level, int *newkey)
{
    int count = 1;
    int that = CPP_UNKNOWN;
    int result = -1;
    struct VAL *vals;

    (void) group;
    (void) level;
    (void) newkey;

    TRACE(("ComplexFence(%d:%d) %d: %s %s\n",
	   level,
	   group,
	   line_no(curbp, DOT.l),
	   direction_of(sdir),
	   typeof_complex(key)));
#if OPT_MAJORMODE
    if (iterations < 0) {
	return ABORT;
    } else if ((++iterations % 200) == 0) {	/* roughly once/sec, slow PC */
	if (time((time_t *) 0) > final_time) {
	    if (iterations >= 0) {
		mlforce("[Too many iterations: %ld]", iterations);
		iterations = -1;
	    }
	    return ABORT;
	}
    }
#endif

    while (count > 0) {

	MARK savedot;
	int savecount;
	int status = next_line(sdir);

	if (status != TRUE)
	    return status;

	savedot = DOT;
	savecount = count;

	TRACE(("complex_fence(%d:%d:%d) %4d:%s\n",
	       level, group, count,
	       line_no(curbp, DOT.l), lp_visible(DOT.l)));

	for_each_modegroup(curbp, result, group, vals) {
	    DOT = savedot;
	    count = savecount;
	    if (((that = match_complex(TRACEARG(result) DOT.l, vals, FALSE))
		 != CPP_UNKNOWN)) {
		int done = FALSE;

		TRACE(("for_each_modegroup:%d:%d (line %d, count %d)\n",
		       result, group,
		       line_no(curbp, DOT.l), count));
#if OPT_MAJORMODE
		if (result != group) {
		    int find = (sdir == FORWARD) ? CPP_IF : CPP_ENDIF;
		    if (that == find) {
			MARK save;
			find = (sdir == FORWARD) ? CPP_ENDIF : CPP_IF;
			do {
			    TRACE_COMPLEX;
			    TRACE(("calling find_one_complex(%s)\n",
				   get_submode_name(curbp, result)));
			    if ((status = find_one_complex(sdir,
							   level + 1,
							   result,
							   &that)) != TRUE) {
				TRACE_COMPLEX;
				TRACE(("done calling find_one_complex (%s)\n",
				       (status == ABORT
					? "abort"
					: "fail")));
				that = CPP_UNKNOWN;
				if (status == ABORT)
				    return status;
				break;
			    }
			    TRACE_COMPLEX;
			    TRACE(("done calling find_one_complex(%s) (ok)\n",
				   get_submode_name(curbp, result)));
			} while (that != find);

			save = DOT;
			if (that != CPP_UNKNOWN) {
			    TRACE_COMPLEX;
			    TRACE(("recurring to finish group '%s'\n",
				   get_submode_name(curbp, result)));
			    if ((status = complex_fence(sdir,
							key,
							group,
							level + 1,
							newkey)) != FALSE) {
				TRACE_COMPLEX;
				TRACE(("done recurring (%s)\n",
				       (status == TRUE
					? "ok"
					: "abort")));
				return status;
			    }
			    DOT = save;
			}
		    }
		    /* improperly nesting? - we missed something */
		    TRACE(("...complex_match mismatch\n"));
		    continue;
		}
		*newkey = that;
#else
		(void) result;
#endif
		TRACE(("...before %s, count=%d, done=%d\n",
		       typeof_complex(that),
		       count, done));

		switch (that) {
		case CPP_IF:
		    if (sdir == FORWARD) {
			count++;
		    } else {
			done = ((count-- == 1) &&
				(key != that));
			if (done)
			    count = 0;
		    }
		    break;

		case CPP_ELIF:
		case CPP_ELSE:
		    done = ((sdir == FORWARD) && (count == 1));
		    if (done)
			count = 0;
		    break;

		case CPP_ENDIF:
		    if (sdir == FORWARD) {
			done = (--count == 0);
		    } else {
			count++;
		    }
		}

		TRACE(("...after %s, count=%d, done=%d, level=%d\n",
		       typeof_complex(that),
		       count, done, level));

		savecount = count;
		if ((count <= 0) || done) {
		    (void) firstnonwhite(FALSE, 1);
		    goto finish;
		}
	    }
	}
    }
  finish:
    if (count == 0) {
	curwp->w_flag |= WFMOVE;
	if (doingopcmd)
	    regionshape = rgn_FULLLINE;
	return TRUE;
    }
    return FALSE;
}

/*
 * Look for a complex fence beginning on the current line.  If we find it,
 * see if we can find the next part.
 */
static int
find_complex(int sdir, int *newkey)
{
    int rc = FALSE;
    int key;
    int group = -1;
    MARK oldpos, oldpre;
    struct VAL *vals;

    /*
     * Iterate over the complex fence groups
     */
    TRACE(("find_complex %4d:%s\n", line_no(curbp, DOT.l), lp_visible(DOT.l)));
    limit_iterations();
    for_each_modegroup(curbp, group, 0, vals) {
	int ic = any_mode(vals, MDIGNCASE);
	if ((key = match_complex(TRACEARG(group) DOT.l, vals, ic)) != CPP_UNKNOWN) {
	    start_fence_op2(sdir, oldpos, oldpre);
	    sdir = ((key == CPP_ENDIF)
		    ? REVERSE
		    : FORWARD);
	    rc = complex_fence(sdir, key, group, 0, newkey);
	    test_fence_op(rc, oldpos, oldpre);
#if OPT_MAJORMODE
	    if (rc) {
		if (iterations < 0)
		    rc = FALSE;
		break;
	    }
#endif
	}
    }
#if OPT_MAJORMODE
    TRACE(("...find_complex %d (iterations %ld)\n", rc, iterations));
#endif
    return rc;
}

/*
 * Look for a complex fence beginning on the current line.  If we find it,
 * see if we can find the next part.
 */
#if OPT_MAJORMODE
static int
find_one_complex(int sdir, int level, int group, int *newkey)
{
    int s = FALSE;
    int key;
    MARK oldpos, oldpre;
    struct VAL *vals = get_submode_vals(curbp, group);

    /*
     * Iterate over the complex fence groups
     */
    TRACE(("find_one_complex %4d:%s\n", line_no(curbp, DOT.l), lp_visible(DOT.l)));
    if ((key = match_complex(TRACEARG(group) DOT.l, vals, FALSE)) != CPP_UNKNOWN) {
	start_fence_op2(sdir, oldpos, oldpre);
	if (level == 0)
	    sdir = ((key == CPP_ENDIF)
		    ? REVERSE
		    : FORWARD);
	s = complex_fence(sdir, key, group, level, newkey);
	test_fence_op(s, oldpos, oldpre);
    }
    return s;
}

/*
 * Provide an absolute limit on the number of iterations in a recursive search
 * for fences.  This is needed when the fences are not properly nested;
 * otherwise the complex fence logic will iterate up to linecount raised to the
 * modegroup power (worse case ;-).
 */
static void
limit_iterations(void)
{
    bsizes(curbp);
    final_time = time((time_t *) 0) + b_val(curbp, VAL_FENCE_LIMIT);
    iterations = 0;
}
#endif

int
is_user_fence(int ch, int *sdirp)
{
    char *fences = b_val_ptr(curbp, VAL_FENCES);
    char *chp, och;
    if (!ch)
	return 0;
    chp = vl_index(fences, ch);
    if (!chp)
	return 0;
    if ((chp - fences) & 1) {
	/* look for the left fence */
	och = chp[-1];
	if (sdirp)
	    *sdirp = REVERSE;
    } else {
	/* look for the right fence */
	och = chp[1];
	if (sdirp)
	    *sdirp = FORWARD;
    }
    return och;
}

static int
simple_fence(int sdir, int ch, int ofence)
{
    int count = 1;		/* Assume that we're sitting at one end of the fence */
    int c;

    /* scan for fence */
    while (InDirection(sdir) && !interrupted()) {
	c = CurrentChar();
	if (c == ch) {
	    ++count;
	} else if (c == ofence) {
	    if (--count <= 0)
		break;
	}
    }

    /* if count is zero, we have a match, move the sucker */
    if (count <= 0) {
	if (!doingopcmd || doingsweep)
	    sweephack = TRUE;
	else if (sdir == FORWARD)
	    forwchar(TRUE, 1);
	curwp->w_flag |= WFMOVE;
	return TRUE;
    }
    return FALSE;
}

static int
comment_fence(int sdir)
{
    /* avoid overlapping match between begin/end patterns */
    if (sdir == FORWARD) {
	size_t off = (size_t) DOT.o - (size_t) S_COL(BlkBegin);
	if (BlkEnd->mlen > off)
	    forwchar(TRUE, (int) (BlkEnd->mlen - off));
    }

    scanboundry(FALSE, DOT, sdir);
    if (scanner((sdir == FORWARD) ? BlkEnd : BlkBegin,
		sdir, (DOT.o == 0), FALSE, FALSE, (int *) 0)) {
	if (!doingopcmd || doingsweep) {
	    sweephack = TRUE;
	    if (sdir == FORWARD && (BlkEnd->mlen > 1))
		forwchar(TRUE, (int) (BlkEnd->mlen - 1));
	} else if (sdir == FORWARD && (BlkEnd->mlen > 1)) {
	    forwchar(TRUE, (int) (BlkEnd->mlen - 0));
	}
	curwp->w_flag |= WFMOVE;
	return TRUE;
    }
    return FALSE;
}

static int
getfence(int ch,		/* fence type to match against */
	 int sdir)		/* direction to scan if we're not on a fence to begin with */
{
    MARK oldpos;		/* original pointer */
    MARK oldpre;
    int ofence = 0;
    int s, i;
    int key = CPP_UNKNOWN;
    int fch;

    /* save position */
    oldpos = DOT;

    TRACE(("getfence, starting at %d.%d\n", line_no(curbp, DOT.l), DOT.o));

    if (ch == UNKNOWN_FENCE_CH) {
	if ((i = firstchar(DOT.l)) < 0)		/* offset of first nonblank */
	    return FALSE;	/* line is entirely blank */

	if (DOT.o <= i
	    && ((s = find_complex(sdir, &ch)) != FALSE)) {
	    return s;
	} else if ((key = match_simple()) != BLK_UNKNOWN) {
	    ch = COMMENT_FENCE_CH;
	    sdir = (key == BLK_BEGIN) ? FORWARD : REVERSE;
	} else if (sdir == FORWARD) {
	    if (oldpos.o < llength(oldpos.l)) {
		do {
		    ch = char_at_mark(oldpos);
		} while (!is_user_fence(ch, (int *) 0) &&
			 ++oldpos.o < llength(oldpos.l));
	    }
	    if (is_at_end_of_line(oldpos)) {
		return FALSE;
	    }
	} else {
	    if (oldpos.o >= 0) {
		do {
		    ch = char_at_mark(oldpos);
		} while (!is_user_fence(ch, (int *) 0) &&
			 --oldpos.o >= 0);
	    }

	    if (oldpos.o < 0) {
		return FALSE;
	    }
	}

	/* we've at least found a fence -- move us that far */
	DOT.o = oldpos.o;
    }

    fch = ch;

    if (ch >= 0) {
	ofence = is_user_fence(ch, &sdir);
	if (ofence)
	    fch = PAIRED_FENCE_CH;
    }

    if (fch >= 0) {
	if ((key = match_simple()) == BLK_UNKNOWN)
	    return (FALSE);
    }

    start_fence_op(sdir, oldpos, oldpre);

    if (ok_BLK(key)) {
	s = comment_fence(sdir);
    } else if (ch == '/') {
	s = comment_fence(sdir);
    } else {
	s = simple_fence(sdir, ch, ofence);
    }

    test_fence_op(s, oldpos, oldpre);
    TRACE(("...getfence, end at %d.%d (status=%d)\n",
	   line_no(curbp, DOT.l), DOT.o, s));
    return (s);
}

/*	the cursor is moved to a matching fence */
int
matchfence(int f, int n)
{
    int s = getfence(UNKNOWN_FENCE_CH, (!f || n > 0) ? FORWARD : REVERSE);
    if (s == FALSE)
	kbd_alarm();
    return s;
}

int
matchfenceback(int f, int n)
{
    int s = getfence(UNKNOWN_FENCE_CH, (!f || n > 0) ? REVERSE : FORWARD);
    if (s == FALSE)
	kbd_alarm();
    return s;
}

/* get the indent of the line containing the matching brace/paren. */
int
fmatchindent(int c)
{
    int ind;

    MK = DOT;

    if (getfence(c, REVERSE) == FALSE) {
	(void) gomark(FALSE, 1);
	return previndent((int *) 0);
    }

    ind = indentlen(DOT.l);

    (void) gomark(FALSE, 1);

    return ind;
}

/*	Close fences are matched against their partners, and if
	on screen the cursor briefly lights there		*/
void
fmatch(int rch)
{
    MARK oldpos;		/* original position */
    register LINE *toplp;	/* top line in current window */
    register int count;		/* current fence level count */
    register int c;		/* current character in scan */
    int dir, lch;
    int backcharfailed = FALSE;

    /* get the matching left-fence char, if it exists */
    lch = is_user_fence(rch, &dir);
    if (lch == 0 || dir != REVERSE)
	return;

    /* first get the display update out there */
    (void) update(FALSE);

    /* save the original cursor position */
    oldpos = DOT;

    /* find the top line and set up for scan */
    toplp = lback(curwp->w_line.l);
    count = 1;
    backchar(TRUE, 2);

    /* scan back until we find it, or reach past the top of the window */
    while (count > 0 && DOT.l != toplp) {
	c = CurrentChar();
	if (c == rch)
	    ++count;
	if (c == lch)
	    --count;
	if (backchar(FALSE, 1) != TRUE) {
	    backcharfailed = TRUE;
	    break;
	}
    }

    /* if count is zero, we have a match, display the sucker */
    if (count == 0) {
	if (!backcharfailed)
	    forwchar(FALSE, 1);
	if (update(SORTOFTRUE) == TRUE) {
#if DISP_NTWIN
	    /*
	     * For all flavors of the editor except winvile,
	     * update() has made the cursor visible.  Winvile,
	     * otoh, uses its own custom logic to enable and
	     * disable the cursor.  At the moment, the cursor
	     * is invisible.  Turn it on.
	     */
	    (void) winvile_cursor_state(TRUE, FALSE);
#endif
	    /*
	     * The idea is to leave the cursor there for about a
	     * quarter of a second.
	     */

	    catnap(300, FALSE);
	}
    }

    /* restore the current position */
    DOT = oldpos;
}

#endif /* OPT_CFENCE */
