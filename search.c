/*
 * The functions in this file implement commands that search in the forward
 * and backward directions.
 *  heavily modified by Paul Fox, 1990
 *
 * $Header: /users/source/archives/vile.vcs/RCS/search.c,v 1.109 1998/04/26 20:12:56 tom Exp $
 *
 * original written Aug. 1986 by John M. Gamble, but I (pgf) have since
 * replaced his regex stuff with Henry Spencer's regexp package.
 *
 */

#include	"estruct.h"
#include        "edef.h"

static char const onlyonemsg[] = "Only one occurrence of pattern";
static char const notfoundmsg[] = "Not found";
static char const hitendmsg[] = "Search reached %s without matching pattern";

static	int	rsearch(int f, int n, int dummy, int fromscreen);
static	void	nextch(MARK *pdot, int dir);
static	void	savematch(MARK curpos, SIZE_T matchlen);

static void
not_found_msg(int wrapok, int dir)
{
	if (wrapok || global_b_val(MDTERSE))
		mlforce (notfoundmsg);
	else
		mlforce (hitendmsg, dir == FORWARD ? "bottom":"top");
}

int
scrforwsearch(int f, int n)
{
	return fsearch(f,n,FALSE,TRUE);
}

int
scrbacksearch(int f, int n)
{
	return rsearch(f,n,FALSE,TRUE);
}

/*
 * forwsearch -- Search forward.  Get a search string from the user, and
 *	search for the string.  If found, reset the "." to be just after
 *	the match string, and (perhaps) repaint the display.
 */

int
forwsearch(int f, int n)
{
	register int status;
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
	register int status;
	int wrapok;
	MARK curpos;
	int didmark = FALSE;
	int didwrap;

	if (f && n < 0)
		return rsearch(f, -n, FALSE, FALSE);

	wrapok = marking || window_b_val(curwp, MDSWRAP);

	last_srch_direc = FORWARD;

	/* Ask the user for the text of a pattern.  If the
	 * response is TRUE (responses other than FALSE are
	 * possible), search for the pattern for as long as
	 * n is positive (n == 0 will go through once, which
	 * is just fine).
	 *
	 * If "marking", then we were called to do line marking for the
	 *  global command.
	 */
	if (!marking && (status = readpattern("Search: ", &pat[0],
				&gregexp, lastkey, fromscreen)) != TRUE) {
		return status;
	}

	ignorecase = window_b_val(curwp, MDIGNCASE);

	curpos = DOT;
	scanboundry(wrapok,curpos,FORWARD);
	didwrap = FALSE;
	do {
		nextch(&(DOT), FORWARD);
		status = scanner(gregexp, FORWARD, wrapok, &didwrap);
		if (status == ABORT) {
			mlforce("[Aborted]");
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
			/* and, so the next nextch gets to next line */
			DOT.o = llength(DOT.l);
			didmark = TRUE;
		}
		if (!marking && didwrap) {
			mlwrite("[Search wrapped past end of buffer]");
			didwrap = FALSE;
		}
	} while ((marking || --n > 0) && status == TRUE);

	if (!marking && !status)
		nextch(&(DOT),REVERSE);

	if (marking) {  /* restore dot and offset */
		DOT = curpos;
	} else if (status) {
		savematch(DOT,gregexp->mlen);
		if (samepoint(DOT,curpos)) {
			mlwrite(onlyonemsg);
		}
	}

	/* Complain if not there.  */
	if ((marking && didmark == FALSE) ||
				(!marking && status == FALSE)) {
		not_found_msg(wrapok,FORWARD);
		return FALSE;
	}

	attrib_matches();
	return TRUE;
}

/*
 * forwhunt -- Search forward for a previously acquired search string.
 *	If found, reset the "." to be just after the match string,
 *	and (perhaps) repaint the display.
 */

int
forwhunt(int f, int n)
{
	register int status;
	int wrapok;
	MARK curpos;
	int didwrap;

	wrapok = window_b_val(curwp, MDSWRAP);

	if (n < 0)		/* search backwards */
		return(backhunt(f, -n));

	/* Make sure a pattern exists */
	if (pat[0] == EOS)
	{
		mlforce("[No pattern set]");
		return FALSE;
	}

	ignorecase = window_b_val(curwp, MDIGNCASE);

	/* Search for the pattern for as long as
	 * n is positive (n == 0 will go through once, which
	 * is just fine).
	 */
	curpos = DOT;
	scanboundry(wrapok,DOT,FORWARD);
	didwrap = FALSE;
	do {
		nextch(&(DOT),FORWARD);
		status = scanner(gregexp, FORWARD, wrapok, &didwrap);
		if (didwrap) {
			mlwrite("[Search wrapped past end of buffer]");
			didwrap = FALSE;
		}
	} while ((--n > 0) && status == TRUE);

	/* Save away the match, or complain if not there.  */
	if (status == TRUE) {
		savematch(DOT,gregexp->mlen);
		if (samepoint(DOT,curpos)) {
			mlwrite(onlyonemsg);
		}
	} else if (status == FALSE) {
		nextch(&(DOT),REVERSE);
		not_found_msg(wrapok,FORWARD);
	} else if (status == ABORT) {
		mlforce("[Aborted]");
		DOT = curpos;
		return status;
	}

	attrib_matches();
	return status;
}

/*
 * backsearch -- Reverse search.  Get a search string from the user, and
 *	search, starting at "." and proceeding toward the front of the buffer.
 *	If found "." is left pointing at the first character of the pattern
 *	(the last character that was matched).
 */
int
backsearch(int f, int n)
{
	register int status;
	hst_init('?');
	status = rsearch(f, n, FALSE, FALSE);
	hst_flush();
	return status;
}

/* ARGSUSED */
static int
rsearch(int f, int n, int dummy GCC_UNUSED, int fromscreen)
{
	register int status;
	int wrapok;
	MARK curpos;
	int didwrap;

	if (n < 0)
		return fsearch(f, -n, FALSE, fromscreen);

	wrapok = window_b_val(curwp, MDSWRAP);

	last_srch_direc = REVERSE;

	/* Ask the user for the text of a pattern.  If the
	 * response is TRUE (responses other than FALSE are
	 * possible), search for the pattern for as long as
	 * n is positive (n == 0 will go through once, which
	 * is just fine).
	 */
	if ((status = readpattern("Reverse search: ", &pat[0],
				&gregexp, EOS, fromscreen)) == TRUE) {
		ignorecase = window_b_val(curwp, MDIGNCASE);

		curpos = DOT;
		scanboundry(wrapok,DOT,REVERSE);
		didwrap = FALSE;
		do {
			nextch(&(DOT),REVERSE);
			status = scanner(gregexp, REVERSE, wrapok, &didwrap);
			if (didwrap) {
				mlwrite(
				  "[Search wrapped past start of buffer]");
				didwrap = FALSE;
			}
		} while ((--n > 0) && status == TRUE);

		/* Save away the match, or complain if not there.  */
		if (status == TRUE)
			savematch(DOT,gregexp->mlen);
			if (samepoint(DOT,curpos)) {
				mlwrite(onlyonemsg);
			}
		else if (status == FALSE) {
			nextch(&(DOT),FORWARD);
			not_found_msg(wrapok,REVERSE);
		} else if (status == ABORT) {
			mlforce("[Aborted]");
			DOT = curpos;
			return status;
		}
	}
	attrib_matches();
	return status;
}

/*
 * backhunt -- Reverse search for a previously acquired search string,
 *	starting at "." and proceeding toward the front of the buffer.
 *	If found "." is left pointing at the first character of the pattern
 *	(the last character that was matched).
 */
int
backhunt(int f, int n)
{
	register int status;
	int wrapok;
	MARK curpos;
	int didwrap;

	wrapok = window_b_val(curwp, MDSWRAP);

	if (n < 0)		/* search forwards */
		return(forwhunt(f, -n));

	/* Make sure a pattern exists */
	if (pat[0] == EOS) {
		mlforce("[No pattern set]");
		return FALSE;
	}

	/* Go search for it for as long as
	 * n is positive (n == 0 will go through once, which
	 * is just fine).
	 */

	ignorecase = window_b_val(curwp, MDIGNCASE);

	curpos = DOT;
	scanboundry(wrapok,DOT,REVERSE);
	didwrap = FALSE;
	do {
		nextch(&(DOT),REVERSE);
		status = scanner(gregexp, REVERSE, wrapok, &didwrap);
		if (didwrap) {
			mlwrite("[Search wrapped past start of buffer]");
			didwrap = FALSE;
		}
	} while ((--n > 0) && status == TRUE);

	/* Save away the match, or complain
	 * if not there.
	 */
	if (status == TRUE) {
		savematch(DOT,gregexp->mlen);
		if (samepoint(DOT, curpos)) {
			mlwrite(onlyonemsg);
		}
	} else if (status == FALSE) {
		nextch(&(DOT),FORWARD);
		not_found_msg(wrapok,REVERSE);
	} else if (status == ABORT) {
		mlforce("[Aborted]");
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
		return(forwhunt(f,n));
	else
		return(backhunt(f,n));
}

/* reverse the search direction */
int
revsearch(int f, int n)
{
	if (last_srch_direc == FORWARD)
		return(backhunt(f,n));
	else
		return(forwhunt(f,n));
}

static int
testit(LINE *lp, regexp *exp, int *end, int srchlim)
{
	char *txt = lp->l_text;
	C_NUM col = (C_NUM)(exp->startp[0] - txt) + 1;

	if (col > llength(lp))
		col = llength(lp);
	if (lregexec(exp, lp, col, srchlim)) {
		col = (C_NUM)(exp->startp[0] - txt) + 1;
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
 * scanner -- Search for a pattern in either direction.  If found,
 *	reset the "." to be at the start or just after the match string
 */
int
scanner(
regexp	*exp,	/* the compiled expression */
int	direct,	/* which way to go.*/
int	wrapok,	/* ok to wrap around bottom of buffer? */
int	*wrappedp)
{
	MARK curpos;
	int found;
	int wrapped = FALSE;

	if (!exp) {
		mlforce("BUG: null exp");
		return FALSE;
	}

	/* Setup local scan pointers to global ".".
	 */
	curpos = DOT;

	/* Scan each character until we hit the scan boundary */
	for_ever {
		register int startoff, srchlim;

		if (interrupted()) {
			if (wrappedp)
				*wrappedp = wrapped;
			return ABORT;
		}

		if (sameline(curpos, scanboundpos)) {
			if (scanbound_is_header) {
				/* if we're on the header, nothing can match */
				found = FALSE;
				srchlim = 0;
			} else {
				if (direct == FORWARD) {
					if (wrapped) {
						startoff = curpos.o;
						srchlim = scanboundpos.o;
					} else {
						startoff = curpos.o;
						srchlim = 
						 (scanboundpos.o > startoff) ?
							scanboundpos.o : 
							llength(curpos.l);
					}
				} else {
					if (wrapped) {
						startoff = scanboundpos.o;
						srchlim = llength(curpos.l);
					} else {
						startoff = 0;
						srchlim = scanboundpos.o + 1;
					}
				}
				found = lregexec(exp, curpos.l,
							startoff, srchlim);
			}
		} else {
			if (direct == FORWARD) {
				startoff = curpos.o;
				srchlim = llength(curpos.l);
			} else {
				startoff = 0;
				srchlim = curpos.o+1;
				if (srchlim > llength(curpos.l))
					srchlim = llength(curpos.l);
			}
			found = lregexec(exp, curpos.l,
						startoff, srchlim);
		}
		if (found) {
			char *txt = curpos.l->l_text;
			char *got = exp->startp[0];
			C_NUM next;
			C_NUM last = curpos.o;

			if (direct == REVERSE) { /* find the last one */
				int end = FALSE;
				char *tst = 0;

				last++;
				while (testit(curpos.l, exp, &end, srchlim)) {
					got = exp->startp[0];
					/* guard against infinite loop: "?$" */
					if (tst == got)
						break;
					else if (tst == 0)
						tst = got;
				}
				if (end)
					last++;
				if (!lregexec(exp, curpos.l,
					    (int)(got - txt), srchlim)) {
					mlforce("BUG: prev. match no good");
					return FALSE;
				}
			} else if (llength(curpos.l) <= 0
			   || last < llength(curpos.l))
				last--;
			next = (C_NUM)(got - txt);
			if (next != last) {
				DOT.l = curpos.l;
				DOT.o = next;
				curwp->w_flag |= WFMOVE; /* flag that we have moved */
				if (wrappedp)
					*wrappedp = wrapped;
				return TRUE;
			}
		} else {
			if (sameline(curpos,scanboundpos) &&
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
			if (sameline(curpos,scanboundpos) &&
						(!wrapok || wrapped) )
				break;
			if (direct == FORWARD)
				curpos.l = lforw(curpos.l);
			else
				curpos.l = lback(curpos.l);
		}
		if (direct == FORWARD) {
			curpos.o = 0;
		} else {
			if ((curpos.o = llength(curpos.l)-1) < 0)
				curpos.o = 0;
		}

	}

	if (wrappedp)
		*wrappedp = wrapped;
	return FALSE;	/* We could not find a match.*/
}

#if OPT_HILITEMATCH
static int hilite_suppressed;
static char savepat[NPAT];
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
need_to_rehilite(void)
{
	/* save static copies of state that affects the search */

	if ((curbp->b_highlight & (HILITE_ON|HILITE_DIRTY)) == 
				  (HILITE_ON|HILITE_DIRTY) ||
			strcmp(pat, savepat) != 0 ||
			save_igncase != ignorecase ||
			save_vattr != b_val(curbp,VAL_HILITEMATCH) ||
			save_magic != b_val(curbp, MDMAGIC) ||
			(!hilite_suppressed && save_curbp != curbp)) {
		(void)strcpy(savepat, pat);
		save_igncase = ignorecase;
		save_vattr = b_val(curbp,VAL_HILITEMATCH); 
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
	DOT.o = 0;
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
	int status = TRUE;
	REGIONSHAPE oregionshape = regionshape;
	VIDEO_ATTR vattr;

	ignorecase = window_b_val(curwp, MDIGNCASE);

	if (!need_to_rehilite())
		return;

	if (pat[0] == EOS || gregexp == NULL)
		return;

/* #define track_hilite 1 */
#ifdef track_hilite
	mlwrite("rehighlighting");
#endif

	vattr = b_val(curbp,VAL_HILITEMATCH); 
	if (vattr == 0)
		return;

	(void)clear_match_attrs(TRUE,1);

	origdot = DOT;
	DOT.l = buf_head(curbp);
	DOT.o = 0;

	scanboundry(FALSE,DOT,FORWARD);
	do {
		nextch(&(DOT),FORWARD);
		status = scanner(gregexp, FORWARD, FALSE, (int *)0);
		if (status != TRUE) 
			break;
		if (vattr != VACOLOR)
			videoattribute = vattr;
		else {
			int c;
			for (c = NSUBEXP-1; c > 0; c--)
				if ( gregexp->startp[c] == gregexp->startp[0]
				  && gregexp->endp[c] == gregexp->endp[0] )
					break;
			if (c > NCOLORS-1)
				videoattribute = VCOLORATTR(NCOLORS-1);
			else
				videoattribute = VCOLORATTR(c+1);
		}
		MK.l = DOT.l;
		MK.o = DOT.o + gregexp->mlen;
		regionshape = EXACT;
		videoattribute |= VOWN_MATCHES;
		status = attributeregion();
	} while (status == TRUE);

	DOT = origdot;
	regionshape = oregionshape;
	curbp->b_highlight = HILITE_ON;  /* & ~HILITE_DIRTY */
	hilite_suppressed = FALSE;
#endif /* OPT_HILITEMATCH */
}

void
regerror(const char *s)
{
	mlforce("[Bad pattern: %s ]",s);
}


/*
 * eq -- Compare two characters.  The "bc" comes from the buffer, "pc"
 *	from the pattern.  If we are in IGNCASE mode, fold out the case.
 */
#if OPT_EVAL || UNUSED
int
eq(register int bc, register int pc)
{
	if (bc == pc)
		return TRUE;
	if (!window_b_val(curwp, MDIGNCASE))
		return FALSE;
	return nocase_eq(bc,pc);
}
#endif

/* ARGSUSED */
int
scrsearchpat(int f GCC_UNUSED, int n GCC_UNUSED)
{
	int s;
	s =  readpattern("", pat, &gregexp, EOS, TRUE);
	mlwrite("Search pattern is now %s", pat);
	last_srch_direc = FORWARD;
	return s;
}

/*
 * readpattern -- Read a pattern.  Stash it in apat.  If it is the
 *	search string, re_comp() it.
 *	Apat is not updated if the user types in an empty line.  If
 *	the user typed an empty line, and there is no old pattern, it is
 *	an error.  Display the old pattern, in the style of Jeff Lomicka.
 *	There is some do-it-yourself control expansion.
 *	An alternate termination character is passed in.
 */
int
readpattern(
const char *prompt,
char	*apat,
regexp	**srchexpp,
int	c,
int	fromscreen)
{
	int status;

	/* Read a pattern.  Either we get one,
	 * or we don't, and use the previous pattern.
	 * Then, if it's the search string, compile it.
	 */
	if (fromscreen) {
		status = screen_string(apat, NPAT, _ident);
		if (status != TRUE)
			return status;
	} else {
		/* don't expand #, %, :, and never process backslashes
			since they're handled by regexp directly for the
			search pattern, and in delins() for the replacement
			pattern */
		hst_glue(c);
		status = kbd_string(prompt, apat, NPAT, c, KBD_EXPPAT|KBD_0CHAR,
				    no_completion);
	}
 	if (status == TRUE) {
		if (srchexpp) {	/* If doing the search string, compile it */
			FreeIfNeeded(*srchexpp);
			*srchexpp = regcomp(pat, b_val(curbp, MDMAGIC));
			if (!*srchexpp)
				return FALSE;
		}
	} else if (status == FALSE && *apat != EOS) { /* Old one */
		status = TRUE;
	}

	return status;
}

/*
 * savematch -- We found the pattern?  Let's save it away.
 */

static void
savematch(MARK curpos, SIZE_T matchlen)
{
	static	ALLOC_T	patlen = 0;	/* length of last malloc */

	/* free any existing match string */
	if (patmatch == NULL || patlen < matchlen) {
		FreeIfNeeded(patmatch);
		/* attempt to allocate a new one */
		patmatch = castalloc(char, patlen = matchlen + 20);
		if (patmatch == NULL)
			return;
	}

	(void)strncpy(patmatch, &(curpos.l->l_text[curpos.o]), matchlen);

	/* null terminate the match string */
	patmatch[matchlen] = EOS;
}

void
scanboundry(int wrapok, MARK dot, int dir)
{
	if (wrapok) {
		nextch(&dot,dir);
		scanboundpos = dot;
		scanbound_is_header = FALSE;
	} else {
		scanboundpos = curbp->b_line;
		scanbound_is_header = TRUE;
	}
}

/*
 * nextch -- advance/retreat the given mark
 *  will wrap, and doesn't set motion flags
 */
static void
nextch(MARK *pdot, int dir)
{
	register LINE	*curline;
	register int	curoff;

	curline = pdot->l;
	curoff = pdot->o;
	if (dir == FORWARD) {
		if (curoff == llength(curline)) {
			curline = lforw(curline);	/* skip to next line */
			curoff = 0;
		} else {
			curoff++;
		}
	} else {
		if (curoff == 0) {
			curline = lback(curline);
			curoff = llength(curline);
		} else {
			curoff--;
		}
	}
	pdot->l = curline;
	pdot->o = curoff;
}


/* simple finder -- give it a compiled regex, a direction, and it takes you
	there if it can.  no wrapping allowed  */
int
findpat(int f, int n, regexp *exp, int direc)
{
	int s;
	MARK savepos;

	if (!exp)
		return FALSE;

	if (!f) n = 1;

	s = TRUE;
	scanboundpos = curbp->b_line; /* was scanboundry(FALSE,savepos,0); */
	scanbound_is_header = TRUE;
	while (s == TRUE && n--) {
		savepos = DOT;
		s = (direc == FORWARD) ? forwchar(TRUE,1) : backchar(TRUE,1);
		if (s == TRUE)
			s = scanner(exp, direc, FALSE, (int *)0);
	}
	if (s != TRUE)
		DOT = savepos;

	return s;
}
