/*
 *
 *	fences.c
 *
 * Match up various fenceposts, like (), [], {}, */ /*, #if, #el, #en
 *
 * Most code probably by Dan Lawrence or Dave Conroy for MicroEMACS
 * Extensions for vile by Paul Fox
 * Revised to use regular expressions - T.Dickey
 *
 * $Header: /users/source/archives/vile.vcs/RCS/fences.c,v 1.52 1998/12/14 12:01:46 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"

#if OPT_CFENCE

#define	CPP_UNKNOWN -1
#define	CPP_IF       0
#define	CPP_ELIF     1
#define	CPP_ELSE     2
#define	CPP_ENDIF    3

#define ok_CPP(n) ((n) >= CPP_IF && (n) <= CPP_ENDIF)

#define BLK_UNKNOWN -1
#define BLK_BEGIN    4
#define BLK_END      5

#define ok_BLK(n) ((n) >= BLK_BEGIN && (n) <= BLK_END)

#define COMPLEX_FENCE_CH  -4
#define COMMENT_FENCE_CH  -3
#define PAIRED_FENCE_CH   -2
#define UNKNOWN_FENCE_CH  -1

#define S_COL(exp) (C_NUM)(exp->startp[0] - DOT.l->l_text)
#define E_COL(exp) (C_NUM)(exp->endp[0]   - DOT.l->l_text)

#define BlkBegin b_val_rexp(curbp, VAL_FENCE_BEGIN)->reg
#define BlkEnd   b_val_rexp(curbp, VAL_FENCE_END)->reg

#define CurrentChar() \
 	(is_at_end_of_line(DOT) ? '\n' : char_at(DOT))
#define InDirection(sdir) \
 	((sdir == REVERSE) ? backchar(FALSE, 1) : forwchar(FALSE, 1))

#if OPT_TRACE
static char *typeof_complex(int code)
{
	char *s;
	switch (code) {
	case CPP_IF:		s = "CPP_IF";		break;
	case CPP_ELIF:		s = "CPP_ELIF";		break;
	case CPP_ELSE:		s = "CPP_ELSE";		break;
	case CPP_ENDIF:		s = "CPP_ENDIF";	break;
	default: 		s = "CPP_UNKNOWN";	break;
	}
	return s;
}
#endif

static int
match_complex(LINE *lp)
{
	static int modes[] = { CPP_IF, CPP_ELIF, CPP_ELSE, CPP_ENDIF };
	size_t j, k;
	int code = CPP_UNKNOWN;

	TRACE(("match_complex %d:%s\n", line_no(curbp, lp), lp_visible(lp)))
	for (j = 0; j < TABLESIZE(modes); j++) {
		/* fix for CC_CANNOT_OFFSET_CASES */
		switch (modes[j]) {
		case CPP_IF:		k = VAL_FENCE_IF;	break;
		case CPP_ELIF:		k = VAL_FENCE_ELIF;	break;
		case CPP_ELSE:		k = VAL_FENCE_ELSE;	break;
		case CPP_ENDIF:		k = VAL_FENCE_FI;	break;
		default:					continue;
		}
		if (lregexec(b_val_rexp(curbp, k)->reg, lp, 0, llength(lp))) {
			code = modes[j];
			break;
		}
	}

	TRACE(("...match_complex %s\n", typeof_complex(code)))
	return code;
}

/*
 * Find the match, if any, for a begin/end comment marker.  If we find a
 * match, the regular expression will overlap the given LINE/offset.
 */
static int
match_simple(void)
{
	C_NUM first = 0;
	C_NUM last = llength(DOT.l);

	TRACE(("match_simple %d:%s\n", line_no(curbp, DOT.l), lp_visible(DOT.l)))
	for (first = 0; first < last; first = S_COL(BlkBegin) + 1) {
		if (!lregexec(BlkBegin, DOT.l, first, last))
			break;
		if ((S_COL(BlkBegin) <= DOT.o)
		 && (E_COL(BlkBegin) >  DOT.o)) {
			TRACE(("...match_simple BLK_BEGIN\n"))
			return BLK_BEGIN;
		}
	}

	for (first = 0; first < last && DOT.o <= last; last = E_COL(BlkEnd) - 1) {
		if (!lregexec(BlkEnd, DOT.l, first, last))
			break;
		if ((S_COL(BlkEnd) <= DOT.o)
		 && (E_COL(BlkEnd) >  DOT.o)) {
			TRACE(("...match_simple BLK_END\n"))
			return BLK_END;
		}
		if (last >= E_COL(BlkEnd) - 1)
			break;
	}

	TRACE(("...match_simple BLK_UNKNOWN\n"))
	return BLK_UNKNOWN;
}

static int
complex_fence(int sdir, int key)
{
	int count = 1;
	int that = CPP_UNKNOWN;

	/* patch: this should come from arguments */
	if (key == CPP_ENDIF)
		sdir = REVERSE;
	else
		sdir = FORWARD;

	/* set up for scan */
	if (sdir == REVERSE)
		DOT.l = lback(DOT.l);
	else
		DOT.l = lforw(DOT.l);

	while (count > 0 && !is_header_line(DOT, curbp)) {
		if (((that = match_complex(DOT.l)) != CPP_UNKNOWN)) {
			int	done = FALSE;

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

			if ((count <= 0) || done) {
				(void) firstnonwhite(FALSE,1);
				break;
			}
		}

		if (sdir == REVERSE)
			DOT.l = lback(DOT.l);
		else
			DOT.l = lforw(DOT.l);

		if (is_header_line(DOT,curbp) || interrupted())
			return FALSE;
	}
	if (count == 0) {
		curwp->w_flag |= WFMOVE;
		if (doingopcmd)
			regionshape = FULLLINE;
		return TRUE;
	}
	return FALSE;
}

int
is_user_fence(int ch, int *sdirp)
{
	char *fences = b_val_ptr(curbp,VAL_FENCES);
	char *chp, och;
	if (!ch)
		return 0;
	chp = strchr(fences, ch);
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
	int count = 1;	/* Assmue that we're sitting at one end of the fence */
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
			forwchar(TRUE,1);
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
		SIZE_T off = (SIZE_T)(DOT.o - S_COL(BlkBegin));
		if (BlkEnd->mlen > off)
			forwchar(TRUE, BlkEnd->mlen - off);
	}

	scanboundry(FALSE,DOT,sdir);
	if (scanner((sdir == FORWARD) ? BlkEnd : BlkBegin,
			sdir, FALSE, (int *)0)) {
		if (!doingopcmd || doingsweep)
		{
			sweephack = TRUE;
			if (sdir == FORWARD && (BlkEnd->mlen > 1))
				forwchar(TRUE, BlkEnd->mlen - 1);
		}
		else if (sdir == FORWARD && (BlkEnd->mlen > 1))
		{
			forwchar(TRUE, BlkEnd->mlen - 0);
		}
		curwp->w_flag |= WFMOVE;
		return TRUE;
	}
	return FALSE;
}

static int
getfence(
int ch, /* fence type to match against */
int sdir) /* direction to scan if we're not on a fence to begin with */
{
	MARK	oldpos; 		/* original pointer */
	register int ofence = 0;	/* open fence */
	int s, i;
	int key = CPP_UNKNOWN;
	int fch;

	/* save the original cursor position */
	oldpos = DOT;

	TRACE(("getfence, starting at %d.%d\n", line_no(curbp, DOT.l), DOT.o))

	/* ch may have been passed, if being used internally */
	if (ch < 0) {
		if ((i = firstchar(DOT.l)) < 0)	/* offset of first nonblank */
			return FALSE;		/* line is entirely blank */

		if (DOT.o <= i
		 && ((key = match_complex(DOT.l)) != CPP_UNKNOWN)) {
			ch = COMPLEX_FENCE_CH;
		} else if ((key = match_simple()) != BLK_UNKNOWN) {
			ch = COMMENT_FENCE_CH;
			sdir = (key == BLK_BEGIN) ? FORWARD : REVERSE;
		} else if (sdir == FORWARD) {
			/* get the current character */
			if (oldpos.o < llength(oldpos.l)) {
				do {
					ch = char_at(oldpos);
				} while(!is_user_fence(ch, (int *)0) &&
					++oldpos.o < llength(oldpos.l));
			}
			if (is_at_end_of_line(oldpos)) {
				return FALSE;
			}
		} else {
			/* get the current character */
			if (oldpos.o >= 0) {
				do {
					ch = char_at(oldpos);
				} while(!is_user_fence(ch, (int *)0) &&
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

	/* setup proper matching fence */
	if (fch >= 0) {
		if ((key = match_complex(DOT.l)) == CPP_UNKNOWN
		 || (key = match_simple()) == BLK_UNKNOWN)
			return(FALSE);
	}

	/* ops are inclusive of the endpoint */
	if (doingopcmd && sdir == REVERSE) {
		forwchar(TRUE,1);
		pre_op_dot = DOT;
		backchar(TRUE,1);
	}

	if (ok_CPP(key)) {  /* we're searching for a cpp keyword */
		s = complex_fence(sdir, key);
	} else if (ok_BLK(key)) {
		s = comment_fence(sdir);
	} else if (ch == '/') {
		s = comment_fence(sdir);
	} else {
		s = simple_fence(sdir, ch, ofence);
	}

	if (s != TRUE) {
		s = FALSE;
		DOT = oldpos; /* restore the current position */
	}
	TRACE(("...getfence, end at %d.%d (status=%d)\n", line_no(curbp, DOT.l), DOT.o, s))
	return(s);
}

/*	the cursor is moved to a matching fence */
int
matchfence(int f, int n)
{
	int s = getfence(UNKNOWN_FENCE_CH, (!f || n > 0) ? FORWARD:REVERSE);
	if (s == FALSE)
		kbd_alarm();
	return s;
}

int
matchfenceback(int f, int n)
{
	int s = getfence(UNKNOWN_FENCE_CH, (!f || n > 0) ? REVERSE:FORWARD);
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

	if (getfence(c,REVERSE) == FALSE) {
		(void)gomark(FALSE,1);
		return previndent((int *)0);
	}

	ind = indentlen(DOT.l);

	(void)gomark(FALSE,1);

	return ind;
}



/*	Close fences are matched against their partners, and if
	on screen the cursor briefly lights there		*/
void
fmatch(int rch)
{
	MARK	oldpos; 	/* original position */
	register LINE *toplp;	/* top line in current window */
	register int count;	/* current fence level count */
	register char c;	/* current character in scan */
	int dir, lch;
	int backcharfailed = FALSE;

	/* get the matching left-fence char, if it exists */
	lch = is_user_fence(rch, &dir);
	if (lch == 0 || dir != REVERSE)
		return;

	/* first get the display update out there */
	(void)update(FALSE);

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
		if (update(SORTOFTRUE) == TRUE)
		/* the idea is to leave the cursor there for about a
			quarter of a second */
			catnap(300, FALSE);
	}

	/* restore the current position */
	DOT = oldpos;
}

#endif /* OPT_CFENCE */
