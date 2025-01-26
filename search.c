/*
 * The functions in this file implement commands that search in the forward
 * and backward directions.
 *  heavily modified by Paul Fox, 1990
 *
 * $Id: search.c,v 1.160 2025/01/26 17:08:26 tom Exp $
 *
 * original written Aug. 1986 by John M. Gamble, but I (pgf) have since
 * replaced his regex stuff with Henry Spencer's regexp package.
 *
 */

#include	"estruct.h"
#include	"edef.h"

static char const onlyonemsg[] = "Only one occurrence of pattern";
static char const notfoundmsg[] = "Not found";
static char const hitendmsg[] = "Search reached %s without matching pattern";

static int rsearch(int f, int n, int dummy, int fromscreen);
static void movenext(MARK *pdot, int dir);
static void savematch(MARK curpos, size_t matchlen);

static void
not_found_msg(int wrapok, int dir)
{
    if (wrapok || global_b_val(MDTERSE))
	mlforce(notfoundmsg);
    else
	mlforce(hitendmsg, ((dir == FORWARD) ? "bottom" : "top"));
}

int
scrforwsearch(int f, int n)
{
    return fsearch(f, n, FALSE, TRUE);
}

int
scrbacksearch(int f, int n)
{
    return rsearch(f, n, FALSE, TRUE);
}

/* forwsearch:  Search forward.  Prompt for a search string from the
 * user, and search the current buffer for it.
 */

int
forwsearch(int f, int n)
{
    int status;
    hst_init('/');
    status = fsearch(f, n, FALSE, FALSE);
    hst_flush();
    return status;
}

/* extra args -- marking if called from globals, and should mark lines, and
	fromscreen, if the searchpattern is on the screen, so we don't need to
	ask for it.  */
int
fsearch(int f, int n, int marking, int fromscreen)
{
    int status = TRUE;
    int wrapok;
    MARK curpos;
    int didmark = FALSE;
    int didwrap;
    int ic;

    assert(curwp != NULL);

    if (f && n < 0)
	return rsearch(f, -n, FALSE, FALSE);

    if (n == 0)
	n = 1;

    wrapok = marking || window_b_val(curwp, MDWRAPSCAN);

    last_srch_direc = FORWARD;

    /* ask the user for a regular expression to search for.  if
     * "marking", then we were called to do line marking for the
     * global command.
     */

    if (!marking) {
	status = readpattern("Search: ", &searchpat, &gregexp,
			     lastkey, fromscreen);
	if (status != TRUE)
	    return status;
    }

    if (curwp == NULL)
	return FALSE;

    curpos = DOT;
    scanboundry(wrapok, curpos, FORWARD);
    didwrap = FALSE;
    ic = window_b_val(curwp, MDIGNCASE) &&
	!(window_b_val(curwp, MDSMARTCASE) && gregexp->uppercase);

    while (marking || n--) {
	movenext(&(DOT), FORWARD);
	status = scanner(gregexp, FORWARD, wrapok, TRUE, ic, &didwrap);
	if (status == ABORT) {
	    mlwarn("[Aborted]");
	    DOT = curpos;
	    return status;
	}
	/* if found, mark the line */
	if (status && marking) {
	    /* if we were on a match when we started, then
	       scanner returns TRUE, even though it's
	       on a boundary. quit if we find ourselves
	       marking a line twice */
	    if (lismarked(DOT.l))
		break;
	    lsetmarked(DOT.l);
	    /* and, so the next movenext gets to next line */
	    DOT.o = llength(DOT.l);
	    didmark = TRUE;
	}
	if (!marking && didwrap) {
	    mlwrite("[Search wrapped past end of buffer]");
	    didwrap = FALSE;
	}
	if (status != TRUE)
	    break;
    }

    if (!marking && !status)
	movenext(&(DOT), REVERSE);

    if (marking) {		/* restore dot and offset */
	DOT = curpos;
    } else if (status) {
	savematch(DOT, gregexp->mlen);
	if (samepoint(DOT, curpos)) {
	    mlwrite(onlyonemsg);
	}
    }

    /* Complain if not there.  */
    if ((marking && didmark == FALSE) ||
	(!marking && status == FALSE)) {
	not_found_msg(wrapok, FORWARD);
	return FALSE;
    }

    attrib_matches();
    return TRUE;
}

/*
 * forwhunt -- repeat previous forward search
 */
int
forwhunt(int f, int n)
{
    int status = TRUE;
    int wrapok;
    MARK curpos;
    int didwrap;
    int ic;

    assert(curwp != NULL);

    wrapok = window_b_val(curwp, MDWRAPSCAN);

    if (f && n < 0)		/* search backwards */
	return (backhunt(f, -n));

    if (n == 0)
	n = 1;

    /* Make sure a pattern exists */
    if (tb_length(searchpat) == 0 || gregexp == NULL) {
	mlforce("[No pattern set]");
	return FALSE;
    }

    if (curwp == NULL)
	return FALSE;

    /* find n'th occurrence of pattern
     */
    curpos = DOT;
    scanboundry(wrapok, DOT, FORWARD);
    didwrap = FALSE;
    ic = window_b_val(curwp, MDIGNCASE) &&
	!(window_b_val(curwp, MDSMARTCASE) && gregexp->uppercase);

    while (n--) {
	movenext(&(DOT), FORWARD);
	status = scanner(gregexp, FORWARD, wrapok, TRUE, ic, &didwrap);
	if (didwrap) {
	    mlwrite("[Search wrapped past end of buffer]");
	    didwrap = FALSE;
	}
	if (status != TRUE)
	    break;
    }

    if (status == TRUE) {
	savematch(DOT, gregexp->mlen);
	if (samepoint(DOT, curpos)) {
	    mlwrite(onlyonemsg);
	} else if (gregexp->mlen == 0
		   && is_header_line(DOT, curbp)) {
	    movenext(&(DOT), FORWARD);
	}
    } else if (status == FALSE) {
	movenext(&(DOT), REVERSE);
	not_found_msg(wrapok, FORWARD);
    } else if (status == ABORT) {
	mlwarn("[Aborted]");
	DOT = curpos;
	return status;
    }
    attrib_matches();
    return status;
}

/*
 * Search backward.  Get a search string from the user, and
 *	search for it.
 */
int
backsearch(int f, int n)
{
    int status;
    hst_init('?');
    status = rsearch(f, n, FALSE, FALSE);
    hst_flush();
    return status;
}

/* ARGSUSED */
static int
rsearch(int f, int n, int dummy GCC_UNUSED, int fromscreen)
{
    int status;
    int wrapok;
    MARK curpos;
    int didwrap;
    int ic;

    assert(curwp != NULL);

    if (f && n < 0)		/* reverse direction */
	return fsearch(f, -n, FALSE, fromscreen);

    if (n == 0)
	n = 1;

    wrapok = window_b_val(curwp, MDWRAPSCAN);

    last_srch_direc = REVERSE;

    /* ask the user for the regular expression to search for, and
     * find n'th occurrence.
     */
    status = readpattern("Reverse search: ", &searchpat, &gregexp,
			 EOS, fromscreen);
    if (status != TRUE)
	return status;

    if (curwp == NULL)
	return FALSE;

    curpos = DOT;
    scanboundry(wrapok, DOT, REVERSE);
    didwrap = FALSE;
    ic = window_b_val(curwp, MDIGNCASE) &&
	!(window_b_val(curwp, MDSMARTCASE) && gregexp->uppercase);

    while (n--) {
	movenext(&(DOT), REVERSE);
	status = scanner(gregexp, REVERSE, wrapok, TRUE, ic, &didwrap);
	if (didwrap) {
	    mlwrite(
		       "[Search wrapped past start of buffer]");
	    didwrap = FALSE;
	}
	if (status != TRUE)
	    break;
    }

    if (status == TRUE) {
	savematch(DOT, gregexp->mlen);
	if (samepoint(DOT, curpos)) {
	    mlwrite(onlyonemsg);
	}
    } else if (status == FALSE) {
	movenext(&(DOT), FORWARD);
	not_found_msg(wrapok, REVERSE);
    } else if (status == ABORT) {
	mlwarn("[Aborted]");
	DOT = curpos;
	return status;
    }
    attrib_matches();
    return status;
}

/*
 * backhunt -- repeat previous forward search
 */
int
backhunt(int f, int n)
{
    int status = TRUE;
    int wrapok;
    MARK curpos;
    int didwrap;
    int ic;

    assert(curwp != NULL);

    wrapok = window_b_val(curwp, MDWRAPSCAN);

    if (f && n < 0)		/* search forwards */
	return (forwhunt(f, -n));

    if (n == 0)
	n = 1;

    /* Make sure a pattern exists */
    if (tb_length(searchpat) == 0 || gregexp == NULL) {
	mlforce("[No pattern set]");
	return FALSE;
    }

    if (curwp == NULL)
	return FALSE;

    /* find n'th occurrence of pattern
     */
    curpos = DOT;
    scanboundry(wrapok, DOT, REVERSE);
    didwrap = FALSE;
    ic = window_b_val(curwp, MDIGNCASE) &&
	!(window_b_val(curwp, MDSMARTCASE) && gregexp->uppercase);

    while (n--) {
	movenext(&(DOT), REVERSE);
	status = scanner(gregexp, REVERSE, wrapok, TRUE, ic, &didwrap);
	if (didwrap) {
	    mlwrite("[Search wrapped past start of buffer]");
	    didwrap = FALSE;
	}
	if (status != TRUE)
	    break;
    }

    if (status == TRUE) {
	savematch(DOT, gregexp->mlen);
	if (samepoint(DOT, curpos)) {
	    mlwrite(onlyonemsg);
	}
    } else if (status == FALSE) {
	movenext(&(DOT), FORWARD);
	not_found_msg(wrapok, REVERSE);
    } else if (status == ABORT) {
	mlwarn("[Aborted]");
	DOT = curpos;
	return status;
    }

    attrib_matches();
    return status;
}

/* continue searching in the same direction as previous */
int
consearch(int f, int n)
{
    if (last_srch_direc == FORWARD)
	return (forwhunt(f, n));
    else
	return (backhunt(f, n));
}

/* reverse the search direction */
int
revsearch(int f, int n)
{
    if (last_srch_direc == FORWARD)
	return (backhunt(f, n));
    else
	return (forwhunt(f, n));
}

static int
testit(LINE *lp, regexp * exp, int *end, int srchlim, int ic)
{
    char *txt = lvalue(lp);
    C_NUM col = (C_NUM) (exp->startp[0] - txt) + 1;

    if (col > llength(lp))
	col = llength(lp);
    if (lregexec(exp, lp, col, srchlim, ic)) {
	col = (C_NUM) (exp->startp[0] - txt) + 1;
	if (col > llength(lp) && !*end) {
	    col = llength(lp);
	    *end = TRUE;
	}
	if (col <= srchlim)
	    return TRUE;
    }
    return FALSE;
}

/*
 * scanner -- Search for a pattern in either direction.  can optionally
 * wrap around end of buffer.
 */
int
scanner(
	   regexp * exp,	/* the compiled expression */
	   int direct,		/* up or down */
	   int wrapok,		/* ok to wrap around end of buffer? */
	   int at_bol,		/* ok to match "^" ? */
	   int ic,		/* ignore case */
	   int *wrappedp)	/* in/out: tells if we wrapped around */
{
    MARK curpos;
    int found;
    int wrapped = FALSE;
    int leftmargin = b_left_margin(curbp);

    TRACE((T_CALLED "scanner %s%s %s\n",
	   (at_bol ? "bol " : ""),
	   ((direct == FORWARD) ? "forward" : "backward"),
	   (wrapok ? "wrapok" : "nowrapok")));

    if (!exp) {
	mlforce("BUG: null exp");
	returnCode(FALSE);
    }

    /* Set starting search position to current position
     */
    curpos = DOT;
    if (curpos.o < leftmargin)
	curpos.o = leftmargin;

    /* Scan each character until we hit the scan boundary */
    for_ever {
	int startoff, srchlim;

	if (interrupted()) {
	    if (wrappedp)
		*wrappedp = wrapped;
	    returnCode(ABORT);
	}

	if (sameline(curpos, scanboundpos)) {
	    if (scanbound_is_header) {
		/* if we're on the header, nothing can match */
		found = FALSE;
		srchlim = leftmargin;
	    } else {
		if (direct == FORWARD) {
		    if (wrapped) {
			startoff = curpos.o;
			srchlim = scanboundpos.o;
		    } else {
			startoff = curpos.o;
			srchlim = ((scanboundpos.o > startoff)
				   ? scanboundpos.o
				   : llength(curpos.l));
		    }
		} else {
		    if (wrapped) {
			startoff = scanboundpos.o;
			srchlim = llength(curpos.l);
		    } else {
			startoff = leftmargin;
			srchlim = scanboundpos.o + 1;
		    }
		}
		found = cregexec(exp, curpos.l, startoff, srchlim, at_bol, ic);
	    }
	} else {
	    if (direct == FORWARD) {
		startoff = curpos.o;
		srchlim = llength(curpos.l);
	    } else {
		startoff = leftmargin;
		srchlim = curpos.o + 1;
		if (srchlim > llength(curpos.l))
		    srchlim = llength(curpos.l);
	    }
	    found = cregexec(exp, curpos.l, startoff, srchlim, at_bol, ic);
	}
	if (found) {
	    char *txt = lvalue(curpos.l);
	    char *got = exp->startp[0];
	    C_NUM next;
	    C_NUM last = curpos.o;

	    if (direct == REVERSE) {	/* find the last one */
		int end = FALSE;
		char *tst = NULL;

		last++;
		while (testit(curpos.l, exp, &end, srchlim, ic)) {
		    got = exp->startp[0];
		    /* guard against infinite loop:  "?$"
		     * or "?.*"
		     */
		    if (tst == got)
			break;
		    tst = got;
		}
		if (end)
		    last++;
		if (!cregexec(exp, curpos.l, (int) (got - txt), srchlim,
			      at_bol, ic)) {
		    mlforce("BUG: prev. match no good");
		    returnCode(FALSE);
		}
	    } else if (llength(curpos.l) <= leftmargin
		       || last < llength(curpos.l)) {
		last--;
	    }
	    next = (C_NUM) (got - txt);
	    if (next != last) {
		DOT.l = curpos.l;
		DOT.o = next;
		curwp->w_flag |= WFMOVE;	/* flag that we have moved */
		if (wrappedp)
		    *wrappedp = wrapped;
#if OPT_TRACE
		trace_mark("...scanner", &DOT, curbp);
#endif
		returnCode(TRUE);
	    }
	} else {
	    if (sameline(curpos, scanboundpos) &&
		(!wrapok || wrapped))
		break;
	}
	if (direct == FORWARD) {
	    curpos.l = lforw(curpos.l);
	} else {
	    curpos.l = lback(curpos.l);
	}
	if (is_header_line(curpos, curbp)) {
	    wrapped = TRUE;
	    if (sameline(curpos, scanboundpos) &&
		(!wrapok || wrapped))
		break;
	    if (direct == FORWARD)
		curpos.l = lforw(curpos.l);
	    else
		curpos.l = lback(curpos.l);
	}
	if (direct == FORWARD) {
	    curpos.o = leftmargin;
	} else {
	    if ((curpos.o = llength(curpos.l) - 1) < leftmargin)
		curpos.o = leftmargin;
	}
	at_bol = TRUE;

    }

    if (wrappedp)
	*wrappedp = wrapped;
    returnCode(FALSE);		/* We could not find a match. */
}

#if OPT_HILITEMATCH
static int hilite_suppressed;
static TBUFF *savepat = NULL;
static int save_igncase;
static int save_magic;
static BUFFER *save_curbp;
static VIDEO_ATTR save_vattr;

void
clobber_save_curbp(BUFFER *bp)
{
    if (save_curbp == bp)
	save_curbp = NULL;
}

/* keep track of enough state to give us a hint as to whether
	we need to redo the visual matches */
static int
need_to_rehilite(int ic)
{
    /* save static copies of state that affects the search */

    if ((curbp->b_highlight & (HILITE_ON | HILITE_DIRTY)) ==
	(HILITE_ON | HILITE_DIRTY) ||
	tb_length(searchpat) != tb_length(savepat) ||
	(tb_length(searchpat) != 0 &&
	 tb_length(savepat) != 0 &&
	 memcmp(tb_values(searchpat),
		tb_values(savepat),
		tb_length(savepat))) ||
	save_igncase != ic ||
	save_vattr != b_val(curbp, VAL_HILITEMATCH) ||
	save_magic != b_val(curbp, MDMAGIC) ||
	(!hilite_suppressed && save_curbp != curbp)) {
	tb_copy(&savepat, searchpat);
	save_igncase = ic;
	save_vattr = (VIDEO_ATTR) b_val(curbp, VAL_HILITEMATCH);
	save_magic = b_val(curbp, MDMAGIC);
	save_curbp = curbp;
	return TRUE;
    }
    return FALSE;
}
#endif

#if OPT_HILITEMATCH
/* ARGSUSED */
int
clear_match_attrs(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int status;
    MARK origdot, origmark;

    if ((curbp->b_highlight & HILITE_ON) == 0)
	return TRUE;

    origdot = DOT;
    origmark = MK;

    DOT.l = lforw(buf_head(curbp));
    DOT.o = b_left_margin(curbp);
    MK.l = lback(buf_head(curbp));
    MK.o = llength(MK.l) - 1;
    videoattribute = VOWN_MATCHES;
    if ((status = attributeregion()) == TRUE) {
	DOT = origdot;
	MK = origmark;
	curbp->b_highlight = 0;
#if OPT_HILITEMATCH
	hilite_suppressed = TRUE;
#endif
    }
    return status;
}
#endif

void
attrib_matches(void)
{
#if OPT_HILITEMATCH
    MARK origdot;
    MARK nextdot;
    int status;
    REGIONSHAPE oregionshape = regionshape;
    VIDEO_ATTR vattr;
    int ic;

    assert(curwp != NULL);

    if (tb_length(searchpat) == 0 || gregexp == NULL)
	return;

    ic = window_b_val(curwp, MDIGNCASE) &&
	!(window_b_val(curwp, MDSMARTCASE) && gregexp->uppercase);

    if (!need_to_rehilite(ic))
	return;

/* #define track_hilite 1 */
#ifdef track_hilite
    mlwrite("rehighlighting");
#endif

    vattr = (VIDEO_ATTR) b_val(curbp, VAL_HILITEMATCH);
    if (vattr == 0)
	return;

    (void) clear_match_attrs(TRUE, 1);

    if (curwp == NULL)
	return;

    origdot = DOT;
    DOT.l = buf_head(curbp);
    DOT.o = b_left_margin(curbp);
    nextdot = DOT;

    scanboundry(FALSE, DOT, FORWARD);
    do {
	if (b_val(curbp, MDHILITEOVERLAP)) {
	    movenext(&(DOT), FORWARD);
	} else {
	    movenext(&nextdot, FORWARD);
	    DOT = nextdot;
	}
	status = scanner(gregexp, FORWARD, FALSE, TRUE, ic, (int *) 0);
	if (status != TRUE)
	    break;
	if (vattr != VACOLOR)
	    videoattribute = vattr;
	else {
	    int c;
	    for (c = NSUBEXP - 1; c > 0; c--)
		if (gregexp->startp[c] == gregexp->startp[0]
		    && gregexp->endp[c] == gregexp->endp[0])
		    break;
	    if (c > NCOLORS - 1)
		videoattribute = VCOLORATTR(NCOLORS - 1);
	    else
		videoattribute = VCOLORATTR(c + 1);
	}
	MK.l = DOT.l;
	MK.o = DOT.o + (C_NUM) gregexp->mlen;

	/* provide a location for the next non-overlapping match */
	nextdot = MK;
	if (gregexp->mlen > 0)
	    nextdot.o -= BytesBefore(nextdot.l, nextdot.o);

	/* show highlighting from DOT to MK */
	regionshape = rgn_EXACT;
	videoattribute |= VOWN_MATCHES;
	status = attributeregion();
    } while (status == TRUE);

    DOT = origdot;
    regionshape = oregionshape;
    curbp->b_highlight = HILITE_ON;	/* & ~HILITE_DIRTY */
    hilite_suppressed = FALSE;
#endif /* OPT_HILITEMATCH */
}

void
regerror(const char *s)
{
    mlforce("[Bad pattern: %s ]", s);
}

/* ARGSUSED */
int
scrsearchpat(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int status;
    TBUFF *temp = NULL;

    status = readpattern("", &searchpat, &gregexp, EOS, TRUE);
    temp = tb_visbuf(tb_values(searchpat), tb_length(searchpat));
    mlwrite("Search pattern is now %s", temp ? tb_values(temp) : "null");
    tb_free(&temp);
    last_srch_direc = FORWARD;
    return status;
}

/*
 * readpattern -- read a pattern.  if it is the
 *	search string, recompile it.
 *	pattern not updated if the user types in an empty line.
 */
int
readpattern(const char *prompt,
	    TBUFF **apat,
	    regexp ** srchexpp,
	    int c,
	    int fromscreen)
{
    char temp[NPAT];
    int status;

    TRACE((T_CALLED "readpattern(%s, %s, %p, %d, %d)\n",
	   prompt ? prompt : "",
	   tb_visible(*apat),
	   (void *) srchexpp,
	   c,
	   fromscreen));

    if (fromscreen) {
	if ((status = screen_to_ident(temp, sizeof(temp))) == TRUE) {
	    if (tb_init(apat, EOS) == NULL
		|| tb_bappend(apat, temp, strlen(temp)) == NULL) {
		status = FALSE;
	    }
	}
	if (status != TRUE)
	    returnCode(status);
    } else {
	/* don't expand #, %, :, and never process backslashes
	   since they're handled by regexp directly for the
	   search pattern, and in delins() for the replacement
	   pattern */
	hst_glue(c);
	/*
	 * kbd_reply() expects a trailing null, to simplify calls from
	 * kbd_string().
	 */
	if (tb_values(*apat) != NULL)
	    tb_append(apat, EOS);
	status = kbd_reply(prompt, apat,
			   eol_history, c,
			   KBD_EXPPAT | KBD_0CHAR,
			   no_completion);
	if (tb_length(*apat) != 0)
	    tb_unput(*apat);	/* trim the trailing null */
    }
    if (status == TRUE) {
	if (srchexpp) {		/* compile it */
	    beginDisplay();
	    FreeIfNeeded(*srchexpp);
	    endofDisplay();
	    *srchexpp = regcomp(tb_values(*apat),
				tb_length(*apat),
				b_val(curbp, MDMAGIC));
	    if (!*srchexpp)
		returnCode(FALSE);
	}
    } else if (status == FALSE && tb_length(*apat) != 0) {	/* Old one */
	status = TRUE;
    }

    returnCode(status);
}

/*
 * savematch -- We found what we are looking for, so save it.
 */

static void
savematch(MARK curpos, size_t matchlen)
{
    tb_init(&tb_matched_pat, EOS);
    tb_bappend(&tb_matched_pat, &(lvalue(curpos.l)[curpos.o]), matchlen);
    tb_append(&tb_matched_pat, EOS);
}

void
scanboundry(int wrapok, MARK dot, int dir)
{
    if (wrapok) {
	movenext(&dot, dir);
	scanboundpos = dot;
	scanbound_is_header = FALSE;
    } else {
	scanboundpos = curbp->b_line;
	scanbound_is_header = TRUE;
    }
}

/*
 * movenext -- move to next character in either direction, but
 *  don't return it.  will wrap, and doesn't set motion flags.
 */
static void
movenext(MARK *pdot, int dir)
{
    LINE *curline;
    int curoff;

    curline = pdot->l;
    curoff = pdot->o;
    if (dir == FORWARD) {
	if (curoff == llength(curline)) {
	    curline = lforw(curline);	/* skip to next line */
	    curoff = 0;
	} else {
	    curoff += BytesAt(curline, curoff);
	}
    } else {
	if (curoff == 0) {
	    curline = lback(curline);
	    curoff = llength(curline);
	} else {
	    curoff -= BytesBefore(curline, curoff);
	}
    }
    pdot->l = curline;
    pdot->o = curoff;
}

/* simple finder -- give it a compiled regex, a direction, and it takes you
	there if it can.  no wrapping allowed  */
int
findpat(int f, int n, regexp * exp, int direc)
{
    int s;
    MARK savepos;

    if (!exp)
	return FALSE;

    n = need_a_count(f, n, 1);

    s = TRUE;
    scanboundpos = curbp->b_line;	/* was scanboundry(FALSE,savepos,0); */
    scanbound_is_header = TRUE;
    savepos = DOT;
    while (s == TRUE && n--) {
	savepos = DOT;
	s = ((direc == FORWARD)
	     ? forwchar(TRUE, 1)
	     : backchar(TRUE, 1));
	if (s == TRUE)
	    s = scanner(exp, direc, FALSE, TRUE, FALSE, (int *) 0);
    }
    if (s != TRUE)
	DOT = savepos;

    return s;
}
