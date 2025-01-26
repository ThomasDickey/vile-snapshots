/* these routines take care of undo operations
 * code by Paul Fox, original algorithm mostly by Julia Harper May, 89
 *
 * written for vile.  Copyright (c) 1990, 1995-2001 by Paul Fox
 *
 * $Id: undo.c,v 1.104 2025/01/26 14:34:21 tom Exp $
 *
 */

#include "estruct.h"
#include "edef.h"

/*
 * There are two stacks and two "dots" saved for undo operations.
 *
 * The first stack, the "backstack", or the "undo stack", is used to move
 * back in history -- that is, as changes are made, they go on this stack,
 * and as we undo those changes, they come off of this stack.  The second
 * stack, the "forward stack" or "redo stack", is used when going forward
 * in history, i.e., when undoing an undo.
 *
 * The "backdot" is the last value of DOT _before_ a change occurred, and
 * therefore must be restored _after_ undoing that change.  The "forwdot"
 * is the first value of DOT _after_ a change occurred, and therefore must
 * be restored _after_ re-doing that change.
 *
 * Distinct sets of changes (i.e.  single user-operations) are separated on
 * the stacks by "stack separator" (natch) entries.  The lforw and lback
 * pointers of these entries are used to save the values of the forward and
 * backward dots.
 *
 * The undo strategy is this:
 *
 * 1) For any deleted line, push it onto the undo stack.
 *
 * 2) On any change to a line, make a copy of it, push the copy to the undo
 * stack, and mark the original as having been copied.  Do not copy/push
 * lines that are already marked as having been copied.  Push a tag
 * matching up the copy with the original.  Later, when undoing, and the
 * copy is being put into the file, we can go down through the undo stack,
 * find all references to the original line, and make them point at the
 * copy instead.  We could do this immediately, instead of pushing the
 * "patch" and waiting for the undo, but it seems better to pay the price
 * of that stack walk at undo time, rather than at the time of the change.
 * This patching wouldn't be necessary at all if we used line no's as the
 * pointers, instead of real pointers.
 *
 * On the actual undo, we pop these things (lines or tags) one by one.
 * There should be either a) no lines where it goes (it was a deleted line)
 * and we can just put it back, or b) exactly one line where it goes (it
 * was a changed/copied line) and it can be replaced, or if this is a tag
 * we're popping (not a real line), then the line it points at was a
 * fresh insert (and should be deleted now).  That makes it easy to undo
 * the changes one by one.  Of course, we need to build a different,
 * inverse stack (the "forward", or "redo" stack) as we go, so that undo
 * can itself be undone.
 *
 * The "copied" cookie in the LINE structure is unioned with the stack link
 * pointer on the undo stack, since they aren't both needed at once.
 *
 * There are basically three interface routines for code making buffer changes:
 *  toss_to_undo() -- called when deleting a whole line
 *  tag_for_undo() -- called when inserting a whole line
 *  copy_for_undo() -- called when modifying a line
 * These routines should be called _before_ calling chg_buff to mark the
 * buffer as modified, since they want to record the current
 * modified/unmodified state in the undo stack, so it can be restored later.
 *
 * In addition:
 *  freeundostacks() cleans up any undo data structures for a buffer,
 *  nounmodifiable() is called if a change is happening to a
 *	buffer that is not undoable.
 *  mayneedundo() is called before starting an operation that might call
 *	one of the toss/copy/tag routines above.
 *  dumpuline() is called if the whole-line undo (the 'U' command) line
 *	may need to be flushed due to the current change.
 *  redo_ok() and undo_ok() return T if something is on the appropriate stack.
 *
 * The command functions are:
 *  backundo() -- undo changes going back in history  (^X-u)
 *  forwredo() -- redo changes going forward in history  (^X-r)
 *  undo() -- undo changes, toggling backwards and forwards  (u)
 *  lineundo() -- undo all changes to currently-being-modified line (U)
 *	(The lineundo() command is kind of separate, and acts independently
 *	 of the normal undo stacks -- an extra copy of the line is simply
 *	 made when the first change is made.)
 *
 *
 * Notes on how the "copied" marks work:
 *
 * Say you do a change to a line, like you go into insertmode, and type
 * three characters.  The first character causes lins_bytes to be called,
 * which calls copy_for_undo(), which copies the line, and marks it as
 * "copied".  Then, the second and third characters also call
 * copy_for_undo(), but since the line is marked, nothing happens.  Now you
 * hit ESC.  Now enter insertmode again.  So far, nothing has happened,
 * except that the "needundocleanup" flag has been set (by execute()s call
 * to mayneedundo()), since we're in an undo-able command.  Type a
 * character.  lins_bytes() calls copy_for_undo() which calls preundocleanup()
 * (based on the "needundocleanup" flag.  In previous versions of vile,
 * this cleanup required walking the entire buffer, to reset the "copied"
 * flag.  Now, the "copied" flag is actually a word-sized "cookie", which
 * matches the global "current_undo_cookie" when the line has been copied.
 * By incrementing the global "current_undo_cookie" in preundocleanup(), we
 * are effectively resetting all of the lines' "marks", since the cookie is
 * now _guaranteed_ to not match against any of them.  Which is why, when
 * the cookie wraps around to 0, we _do_ need to clean the buffer, because
 * now there's a chance that the current_undo_cookie might match a very old
 * marked line.
 */

#define FORW 0
#define BACK 1

/* shorthand for the two stacks and the two dots */
#define BACKSTK(bp) (&(bp->b_udstks[BACK]))
#define FORWSTK(bp) (&(bp->b_udstks[FORW]))
#define BACKDOT(bp) (bp->b_uddot[BACK])
#define FORWDOT(bp) (bp->b_uddot[FORW])

/* these let us refer to the current and other stack in relative terms */
#define STACK(i)	(&(curbp->b_udstks[i]))
#define OTHERSTACK(i)	(&(curbp->b_udstks[1^i]))

static LINE *popline(LINE **stkp, int force);
static LINE *copyline(LINE *lp);
static int linesmatch(LINE *lp1, LINE *lp2);
static int undoworker(int stkindx);
static void freshstack(int stkindx);
static void make_undo_patch(LINE *olp, LINE *nlp);
static void preundocleanup(void);
static void pushline(LINE *lp, LINE **stk);
static void repointstuff(LINE *nlp, LINE *olp);
static void setupuline(LINE *lp);

static short needundocleanup;

/* this could be per-buffer, but i don't think it matters in practice */
static USHORT current_undo_cookie = 1;	/* see L_FLAG.cook */

/* #define UNDOLOG 1 */
#ifdef UNDOLOG
static void
undolog(char *s, LINE *lp)
{
    char *t;
    if (lisreal(lp))
	t = "real";
    else if (lispurestacksep(lp))
	t = "purestacksep";
    else if (lisstacksep(lp))
	t = "stacksep";
    else if (lispatch(lp))
	t = "patch";
    else
	t = "unknown";

    dbgwrite("%s %s lp 0x%x", s, t, lp);
}
#else
# define undolog(s,l)
#endif

/* push a deleted line onto the undo stack. */
void
toss_to_undo(LINE *lp)
{
    LINE *next;
    LINE *prev;
    int fc;

    TRACE2((T_CALLED "toss_to_undo(%p)\n", lp));
    if (needundocleanup)
	preundocleanup();

    pushline(lp, BACKSTK(curbp));

    next = lforw(lp);

    /* need to save a dot -- either the next line or
       the previous one */
    if (next == buf_head(curbp)) {
	prev = lback(lp);
	FORWDOT(curbp).l = prev;
	fc = firstchar(prev);
	if (fc < 0)		/* all white */
	    FORWDOT(curbp).o = llength(prev) - 1;
	else
	    FORWDOT(curbp).o = fc;
    } else {
	FORWDOT(curbp).l = next;
	fc = firstchar(next);
	if (fc < 0)		/* all white */
	    FORWDOT(curbp).o = b_left_margin(curbp);
	else
	    FORWDOT(curbp).o = fc;
    }

    dumpuline(lp);
    return2Void();
}

/*
 * Push a copy of a line onto the undo stack.  Push a patch so we can
 * later fix up any references to this line that might already be in the
 * stack.  When the undo happens, the later pops (i.e. those lines still
 * in the stack) will point at the _original_ (which will by then be on the
 * redo stack) instead of at the copy, which will have just been popped,
 * unless we fix them by popping and using the patch.
 */
int
copy_for_undo(LINE *lp)
{
    int status = FALSE;
    LINE *nlp;

    TRACE2((T_CALLED "copy_for_undo(%p)\n", lp));
    if (needundocleanup)
	preundocleanup();

    if (liscopied(lp)) {
	status = TRUE;
    } else if ((nlp = copyline(lp)) == NULL) {
	status = ABORT;
    } else {
	/* take care of the normal undo stack */
	pushline(nlp, BACKSTK(curbp));

	make_undo_patch(lp, nlp);

	lsetcopied(lp);

	setupuline(lp);

	FORWDOT(curbp).l = lp;
	FORWDOT(curbp).o = DOT.o;

	status = TRUE;
    }
    return2Code(status);
}

/* push an unreal line onto the undo stack
 * lp should be the new line, _after_ insertion, so
 *	lforw() and lback() are right
 */
int
tag_for_undo(LINE *lp)
{
    int status = FALSE;
    LINE *nlp;

    TRACE2((T_CALLED "tag_for_undo(%p)\n", lp));
    if (needundocleanup)
	preundocleanup();

    if (liscopied(lp)) {
	status = TRUE;
    } else if ((nlp = lalloc(LINENOTREAL, curbp)) == NULL) {
	TRACE(("tag_for_undo: no memory\n"));
	status = ABORT;
    } else {
	set_lforw(nlp, lforw(lp));
	set_lback(nlp, lback(lp));

	pushline(nlp, BACKSTK(curbp));

	lsetcopied(lp);
	FORWDOT(curbp).l = lp;
	FORWDOT(curbp).o = DOT.o;

	status = TRUE;
    }
    return2Code(status);
}

/* Change all PURESTACKSEP's on the stacks to STACKSEP's, so that undo won't
 * reset the BFCHG bit.  This should be called anytime a non-undoable change is
 * made to a buffer.
 */
void
nounmodifiable(BUFFER *bp)
{
    LINE *tlp;
    for (tlp = *BACKSTK(bp); tlp != NULL; tlp = tlp->l_nxtundo) {
	if (lispurestacksep(tlp))
	    llength(tlp) = STACKSEP;
    }
    for (tlp = *FORWSTK(bp); tlp != NULL; tlp = tlp->l_nxtundo) {
	if (lispurestacksep(tlp))
	    llength(tlp) = STACKSEP;
    }
}

/* before any undoable command (except undo itself), clean the undo list */
/* clean the copied flag on the line we're the copy of */
void
freeundostacks(BUFFER *bp, int both)
{
    LINE *lp;

    TRACE((T_CALLED "freeundostacks(%p,%d)\n", (void *) bp, both));
    while ((lp = popline(FORWSTK(bp), TRUE)) != NULL) {
	lfree(lp, bp);
    }
    if (both) {
	while ((lp = popline(BACKSTK(bp), TRUE)) != NULL) {
	    lfree(lp, bp);
	}
	bp->b_udtail = NULL;
	bp->b_udlastsep = NULL;
	bp->b_udcount = 0;
    }
    returnVoid();
}

int
check_editable(BUFFER *bp)
{
    int status = TRUE;
    if (valid_buffer(bp) && b_val(bp, MDVIEW))
	status = rdonly();
    return status;
}

/* ARGSUSED */
int
undo(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int status;
    L_NUM before;

    TRACE((T_CALLED "undo(%d,%d)\n", f, n));

    if ((status = check_editable(curbp)) == TRUE) {
	before = vl_line_count(curbp);
	if ((status = undoworker(curbp->b_udstkindx)) == TRUE) {
	    if (!line_report(before)) {
		mlwrite("[change %sdone]",
			curbp->b_udstkindx == BACK ? "un" : "re");
	    }
	    curbp->b_udstkindx ^= 1;	/* flip to other stack */
	} else {
	    mlwarn("[No changes to undo]");
	}
    }
    returnCode(status);
}

int
inf_undo(int f, int n)
{
    int status = TRUE;

    TRACE((T_CALLED "undo(%d,%d)\n", f, n));

    n = need_at_least(f, n, 1);

    if ((status = check_editable(curbp)) == TRUE) {
	curbp->b_udstkindx ^= 1;	/* flip to other stack */
	while ((status == TRUE) && n--) {
	    if ((status = undoworker(curbp->b_udstkindx)) == TRUE) {
		mlwrite("[change %sdone]",
			curbp->b_udstkindx == BACK ? "un" : "re");
	    } else {
		mlwarn("[No more changes to %s]",
		       curbp->b_udstkindx == BACK ? "undo" : "redo");
	    }
	}
	curbp->b_udstkindx ^= 1;	/* flip to other stack */
    }
    returnCode(status);
}

int
backundo(int f, int n)
{
    int status = TRUE;

    TRACE((T_CALLED "backundo(%d,%d)\n", f, n));

    n = need_at_least(f, n, 1);

    if ((status = check_editable(curbp)) == TRUE) {
	while ((status == TRUE) && n--) {
	    status = undoworker(BACK);
	    if (status) {
		mlwrite("[change undone]");
	    } else {
		mlwarn("[No more changes to undo]");
	    }
	}

	curbp->b_udstkindx = FORW;	/* flip to other stack */
    }
    returnCode(status);
}

int
forwredo(int f, int n)
{
    int status = TRUE;

    TRACE((T_CALLED "forwundo(%d,%d)\n", f, n));

    n = need_at_least(f, n, 1);

    if ((status = check_editable(curbp)) == TRUE) {
	while ((status == TRUE) && n--) {
	    status = undoworker(FORW);
	    if (status) {
		mlwrite("[change redone]");
	    } else {
		mlwarn("[No more changes to redo]");
	    }
	}

	curbp->b_udstkindx = BACK;	/* flip to other stack */
    }
    return status;
}

void
mayneedundo(void)
{
    if (in_autocolor)
	return;
    needundocleanup = TRUE;
}

static void
preundocleanup(void)
{
    LINE *lp;

    freeundostacks(curbp, FALSE);

    /* clear the flags in the buffer */
    /* there may be a way to clean these less drastically, by
       using the information on the stacks above, but I
       couldn't figure it out.  -pgf  */
    if (++current_undo_cookie == 0) {
	current_undo_cookie++;	/* never let it be zero */
	for_each_line(lp, curbp) {	/* once in while, feel the pain */
	    lsetnotcopied(lp);
	}
    }

    curbp->b_udstkindx = BACK;

    if (doingopcmd)
	BACKDOT(curbp) = pre_op_dot;
    else
	BACKDOT(curbp) = DOT;

    /* be sure FORWDOT has _some_ value (may be null the first time)
       if (sameline(FORWDOT(curbp), nullmark))
       FORWDOT(curbp) = BACKDOT(curbp);
     */
    freshstack(BACK);
    FORWDOT(curbp) = BACKDOT(curbp);

    needundocleanup = FALSE;
}

static void
pushline(LINE *lp, LINE **stk)
{
    lp->l_nxtundo = *stk;
    *stk = lp;
    undolog("pushing", lp);
}

/* get a line from the specified stack.  unless force'ing, don't
	go past a false bottom stack-separator */
static LINE *
popline(LINE **stkp, int force)
{
    LINE *lp;

    lp = *stkp;

    if (lp == NULL || (!force && lisstacksep(lp))) {
	undolog("popping null", lp);
	return NULL;
    }

    *stkp = lp->l_nxtundo;
    lp->l_nxtundo = NULL;
    undolog("popped", lp);
    return (lp);
}

static LINE *
peekline(LINE **stkp)
{
    return *stkp;
}

static void
freshstack(int stkindx)
{
    LINE *plp;
    /* push on a stack delimiter, so we know where this undo ends */
    if (b_is_changed(curbp)) {
	plp = lalloc(STACKSEP, curbp);
    } else {			/* if the buffer is unmodified, use special separator */
	plp = lalloc(PURESTACKSEP, curbp);

	/* and make sure there are no _other_ special separators */
	nounmodifiable(curbp);
    }
    if (plp == NULL) {
	TRACE(("freshstack: no memory\n"));
	return;
    }
    set_lback(plp, BACKDOT(curbp).l);
    plp->l_back_offs = BACKDOT(curbp).o;
    set_lforw(plp, FORWDOT(curbp).l);
    plp->l_forw_offs = FORWDOT(curbp).o;
    pushline(plp, STACK(stkindx));
    if (stkindx == BACK) {
	plp->l_nextsep = NULL;
	if (curbp->b_udtail == NULL) {
	    if (curbp->b_udcount != 0) {
		mlforce("BUG: null tail with non-0 undo count");
		curbp->b_udcount = 0;
	    }
	    curbp->b_udtail = plp;
	    curbp->b_udlastsep = plp;
	} else {
	    if (curbp->b_udlastsep == NULL) {
		/* then we need to find lastsep */
		int i;
		curbp->b_udlastsep = curbp->b_udtail;
		for (i = curbp->b_udcount - 1; i > 0; i--) {
		    curbp->b_udlastsep =
			curbp->b_udlastsep->l_nextsep;
		}
	    }
	    curbp->b_udlastsep->l_nextsep = plp;
	    curbp->b_udlastsep = plp;
	}
	/* enforce stack growth limit */
	curbp->b_udcount++;
	/* dbgwrite("bumped undocount %d", curbp->b_udcount); */
	if (b_val(curbp, VAL_UNDOLIM) != 0 &&
	    curbp->b_udcount > b_val(curbp, VAL_UNDOLIM)) {
	    LINE *newtail;
	    LINE *lp;

	    if ((newtail = curbp->b_udtail) != NULL) {
		while (curbp->b_udcount > b_val(curbp, VAL_UNDOLIM)) {
		    if ((newtail = newtail->l_nextsep) == NULL)
			break;
		    curbp->b_udcount--;
		}

		if ((curbp->b_udtail = newtail) != NULL) {
		    newtail = newtail->l_nxtundo;
		    if (newtail != NULL) {
			do {
			    lp = newtail;
			    if (newtail == curbp->b_udlastsep)
				mlforce("BUG: tail passed lastsep");
			    newtail = newtail->l_nxtundo;
			    lfree(lp, curbp);
			} while (newtail != NULL);
		    }
		    curbp->b_udtail->l_nxtundo = NULL;
		}
	    }
	}
    }
}

static void
make_undo_patch(LINE *olp, LINE *nlp)
{
    LINE *plp;
    /* push on a tag that matches up the copy with the original */
    plp = lalloc(LINEUNDOPATCH, curbp);
    if (plp == NULL) {
	TRACE(("make_undo_patch: no memory\n"));
	return;
    }
    set_lforw(plp, olp);	/* lforw() is the original line */
    set_lback(plp, nlp);	/* lback() is the copy */
    pushline(plp, BACKSTK(curbp));
}

static void
applypatch(LINE *newlp, LINE *oldlp)
{
    LINE *tlp;
    for (tlp = *BACKSTK(curbp); tlp != NULL;
	 tlp = tlp->l_nxtundo) {
	if (!lispatch(tlp)) {
	    if (lforw(tlp) == oldlp)
		set_lforw(tlp, newlp);
	    if (lback(tlp) == oldlp)
		set_lback(tlp, newlp);
	} else {		/* it's a patch */
	    if (lforw(tlp) == oldlp) {
		set_lforw(tlp, newlp);
	    }
	    if (lback(tlp) == oldlp) {
		mlforce("BUG? copy is an old line");
		break;
	    }
	}
    }
}

static LINE *
copyline(LINE *lp)
{
    LINE *nlp;

    nlp = lalloc(llength(lp), curbp);
    if (nlp == NULL) {
	TRACE(("copyline: no memory\n"));
	return NULL;
    }
    /* copy the text and forward and back pointers.  everything else
       matches already */
    set_lforw(nlp, lforw(lp));
    set_lback(nlp, lback(lp));
    /* copy the rest */
    if (lvalue(lp) && lvalue(nlp))
	(void) memcpy(lvalue(nlp), lvalue(lp), (size_t) llength(lp));
    return nlp;
}

static int
undoworker(int stkindx)
{
    LINE *lp;
    LINE *alp;
    int nopops = TRUE;

    while ((lp = popline(STACK(stkindx), FALSE)) != NULL) {
	if (nopops)		/* first pop -- establish a new stack base */
	    freshstack(1 ^ stkindx);
	nopops = FALSE;
	if (lislinepatch(lp)) {
	    applypatch(lback(lp), lforw(lp));
	    lfree(lp, curbp);
	    continue;
	}
	if (lforw(lback(lp)) != lforw(lp)) {	/* there's something there */
	    if (lforw(lforw(lback(lp))) == lforw(lp)) {
		/* then there is exactly one line there */
		/* alp is the line to remove */
		/* lp is the line we're putting in */
		alp = lforw(lback(lp));
		repointstuff(lp, alp);
		/* remove it */
		set_lforw(lback(lp), lforw(alp));
		set_lback(lforw(alp), lback(alp));
	    } else {		/* there is more than one line there */
		mlforce("BUG: no stacked line for an insert");
#if OPT_TRACE
		trace_line(lp, curbp);
		trace_line(lback(lp), curbp);
		trace_line(lforw(lback(lp)), curbp);
		trace_line(lforw(lforw(lback(lp))), curbp);
		trace_line(lforw(lp), curbp);
#endif
		/* cleanup ? naw, a bugs a bug */
		return (FALSE);
	    }
	} else {		/* there is no line where we're going */
	    /* create an "unreal" tag line to push */
	    alp = lalloc(LINENOTREAL, curbp);
	    if (alp == NULL) {
		TRACE(("undoworker: no memory\n"));
		return (FALSE);
	    }
	    set_lforw(alp, lforw(lp));
	    set_lback(alp, lback(lp));
	}

	/* insert real lines into the buffer
	   throw away the markers */
	if (lisreal(lp)) {
	    set_lforw(lback(lp), lp);
	    set_lback(lforw(lp), lp);
	} else {
	    lfree(lp, curbp);
	}

	pushline(alp, OTHERSTACK(stkindx));
    }

    if (nopops) {
	if (stkindx == BACK && curbp->b_udcount != 0) {
	    mlforce("BUG: nopop, non-0 undo count");
	}
	return (FALSE);
    }
#define bug_checks 1
#ifdef bug_checks
    if ((lp = peekline(STACK(stkindx))) == NULL) {
	mlforce("BUG: found null after undo/redo");
	return FALSE;
    }

    if (!lisstacksep(lp)) {
	mlforce("BUG: found non-sep after undo/redo");
	return FALSE;
    }
#endif

    lp = popline(STACK(stkindx), TRUE);
    FORWDOT(curbp).l = lforw(lp);
    FORWDOT(curbp).o = lp->l_forw_offs;
    BACKDOT(curbp).l = lback(lp);
    BACKDOT(curbp).o = lp->l_back_offs;
    if (stkindx == FORW) {
	/* if we moved, update the "last dot" mark */
	if (!sameline(DOT, FORWDOT(curbp)))
	    curwp->w_lastdot = DOT;
	DOT = FORWDOT(curbp);
    } else {
	/* if we moved, update the "last dot" mark */
	if (!sameline(DOT, BACKDOT(curbp)))
	    curwp->w_lastdot = DOT;
	DOT = BACKDOT(curbp);
	/* dbgwrite("about to decr undocount %d", curbp->b_udcount); */
	curbp->b_udcount--;
	curbp->b_udlastsep = NULL;	/* it's only a hint */
	if (curbp->b_udtail == lp) {
	    if (curbp->b_udcount != 0) {
		mlforce("BUG: popped tail; non-0 undo count");
		curbp->b_udcount = 0;
	    }
	    /* dbgwrite("clearing tail 0x%x and lastsep 0x%x", curbp->b_udtail,
	       curbp->b_udlastsep); */
	    curbp->b_udtail = NULL;
	    curbp->b_udlastsep = NULL;
	}
    }

    b_clr_counted(curbp);	/* don't know the size! */
    if (lispurestacksep(lp))
	unchg_buff(curbp, WFHARD);
    else
	chg_buff(curbp, WFHARD | WFINS | WFKILLS);

    lfree(lp, curbp);

    return TRUE;
}

static void
setupuline(LINE *lp)
{
    LINE *ulp;
    /* take care of the U line */
    if ((ulp = curbp->b_ulinep) == NULL
	|| (ulp->l_nxtundo != lp)) {
	if (ulp != NULL)
	    lfree(curbp->b_ulinep, curbp);
	ulp = curbp->b_ulinep = copyline(lp);
	if (ulp != NULL)
	    ulp->l_nxtundo = lp;
    }
}

void
dumpuline(LINE *lp)
{
    LINE *ulp = curbp->b_ulinep;

    if ((ulp != NULL) && (ulp->l_nxtundo == lp)) {
	lfree(curbp->b_ulinep, curbp);
	curbp->b_ulinep = NULL;
    }
}

/* ARGSUSED */
int
lineundo(int f GCC_UNUSED, int n GCC_UNUSED)
{
    LINE *ulp;			/* the Undo line */
    LINE *lp;			/* the line we may replace */
    WINDOW *wp;
    char *ntext;

    ulp = curbp->b_ulinep;
    if (ulp == NULL) {
	kbd_alarm();
	return FALSE;
    }

    lp = ulp->l_nxtundo;

    if (lforw(ulp) != lforw(lp) ||
	lback(ulp) != lback(lp)) {
	/* then the last change affected more than one line,
	   and we can't use the saved U-line */
	dumpuline(curbp->b_ulinep);
	kbd_alarm();
	return FALSE;
    }

    /* avoid losing our undo stacks needlessly */
    if (linesmatch(ulp, lp) == TRUE)
	return TRUE;

    DOT.l = lp;

    preundocleanup();

    ntext = NULL;
    if (ulp->l_size && (ntext = typeallocn(char, ulp->l_size)) == NULL)
	  return (FALSE);

    CopyForUndo(lp);

    if (ntext && lvalue(lp)) {
	(void) memcpy(ntext, lvalue(ulp), (size_t) llength(ulp));
	ltextfree(lp, curbp);
    }

    lvalue(lp) = ntext;
    llength(lp) = llength(ulp);
    lp->l_size = ulp->l_size;

#if ! WINMARK
    if (MK.l == lp)
	MK.o = b_left_margin(curbp);
#endif
    /* let's be defensive about this */
    for_each_window(wp) {
	if (wp->w_dot.l == lp)
	    wp->w_dot.o = b_left_margin(curbp);
#if WINMARK
	if (wp->w_mark.l == lp)
	    wp->w_mark.o = b_left_margin(curbp);
#endif
	if (wp->w_lastdot.l == lp)
	    wp->w_lastdot.o = b_left_margin(curbp);
    }
    do_mark_iterate(mp, {
	if (mp->l == lp)
	    mp->o = b_left_margin(curbp);
    });
#if OPT_LINE_ATTRS
    FreeAndNull(lp->l_attrs);
#endif

    chg_buff(curbp, WFEDIT | WFKILLS | WFINS);

    return TRUE;

}

#define _min(a,b) ((a) < (b)) ? (a) : (b)

static void
repointstuff(LINE *nlp, LINE *olp)
{
    WINDOW *wp;
    int usenew = lisreal(nlp);
    LINE *point;
    LINE *ulp;

    point = usenew ? nlp : lforw(olp);
#if ! WINMARK
    if (MK.l == olp) {
	MK.l = point;
	MK.o = _min(MK.o, llength(point));
    }
#endif
    /* fix anything important that points to it */
    for_each_window(wp) {
	if (wp->w_dot.l == olp) {
	    wp->w_dot.l = point;
	    wp->w_dot.o = _min(wp->w_dot.o, llength(point));
	}
	if (wp->w_line.l == olp)
	    wp->w_line.l = point;
#if WINMARK
	if (wp->w_mark.l == olp) {
	    wp->w_mark.l = point;
	    wp->w_mark.o = _min(wp->w_mark.o, llength(point));
	}
#endif
	if (wp->w_lastdot.l == olp) {
	    wp->w_lastdot.l = point;
	    wp->w_lastdot.o = _min(wp->w_lastdot.o, llength(point));
	}
    }
    do_mark_iterate(mp, {
	if (mp->l == olp) {
	    mp->l = point;
	    mp->o = _min(mp->o, llength(point));
	}
    });
#if OPT_LINE_ATTRS
    FreeAndNull(olp->l_attrs);
#endif

    /* reset the uline */
    if ((ulp = curbp->b_ulinep) != NULL
	&& (ulp->l_nxtundo == olp)) {
	if (usenew) {
	    ulp->l_nxtundo = point;
	} else {
	    /* we lose the ability to undo all changes
	       to this line, since it's going away */
	    curbp->b_ulinep = NULL;
	}
    }

}

static int
linesmatch(LINE *lp1, LINE *lp2)
{
    if (llength(lp1) != llength(lp2))
	return FALSE;
    if (llength(lp1) == 0)
	return TRUE;
    return !memcmp(lvalue(lp1), lvalue(lp2), (size_t) llength(lp1));
}

int
undo_ok(void)
{
    LINE **lp = STACK(BACK);

    return (*lp != NULL);
}

int
redo_ok(void)
{
    LINE **lp = STACK(FORW);

    return (*lp != NULL);
}

#if !SMALLER
static void
list_undos(LINE *lp)
{
    int limit = 1000;

    for (; lp != NULL; lp = lp->l_nxtundo) {
	int len = llength(lp);

	bputc('\n');
	switch (len) {
	case LINENOTREAL:
	    bprintf("*LINENOTREAL");
	    break;
	case LINEUNDOPATCH:
	    bprintf("*LINEUNDOPATCH");
	    break;
	case STACKSEP:
	    bprintf("*STACKSEP");
	    break;
	case PURESTACKSEP:
	    bprintf("*PURESTACKSEP");
	    break;
	default:
	    if (len > 0) {
		bputsn(lvalue(lp), llength(lp));
	    }
	    break;
	}
	if (--limit < 0)
	    break;
    }
    bputc('\n');
}

static void
make_undostk(int unused GCC_UNUSED, void *dummy)
{
    BUFFER *bp = (BUFFER *) dummy;

    bprintf("Undo-stack for %s\n", bp->b_bname);
    bprintf("current stack:%s\n", bp->b_udstkindx ? "FORW" : "BACK");

    bputc('\n');
    bprintf("BACKSTK:");
    list_undos(*BACKSTK(bp));

    bputc('\n');
    bprintf("FORWSTK:");
    list_undos(*FORWSTK(bp));

    bputc('\n');
    bprintf("BACKDOT = %d.%d", line_no(bp, BACKDOT(bp).l), BACKDOT(bp).o);

    bputc('\n');
    bprintf("FORWDOT = %d.%d", line_no(bp, FORWDOT(bp).l), FORWDOT(bp).o);
}

int
show_undostk(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return liststuff(UNDOSTK_BufName, FALSE, make_undostk, 0, (void *) curbp);
}
#endif
