/*
 *	A few functions that used to operate on single whole lines, mostly
 *	here to support the globals() function.  They now work on regions.
 *	Copyright (c) 1990, 1995 by Paul Fox, except for delins(), which is
 *	Copyright (c) 1986 by University of Toronto, as noted below.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/oneliner.c,v 1.89 1998/04/28 10:17:39 tom Exp $
 */

#include	"estruct.h"
#include	"edef.h"

#define PLIST	0x01
#define PNUMS	0x02

static	int	delins(regexp *exp, char *sourc);
static	int	substline(regexp *exp, int nth_occur, int printit, int globally, int *confirmp);
static	int	substreg1(int needpats, int use_opts);

static	int	lines_changed,
		total_changes;

/*
 * put lines in a popup window
 */
static int
pregion(UINT flag)
{
	register WINDOW *wp;
	register BUFFER *bp;
	register int	status;
	REGION		region;
	register LINEPTR linep;
	BUFFER *oldbp = curbp;

	if ((status = get_fl_region(&region)) != TRUE) {
		return (status);
	}

	linep = region.r_orig.l;		 /* Current line.	 */

	/* first check if we are already here */
	bp = bfind(P_LINES_BufName, 0);
	if (bp == NULL) {
		return FALSE;
	}

	if (bp == curbp) {
		mlforce("[Can't do that from this buffer.]");
		return FALSE;
	}

	if (!calledbefore) {		/* fresh start */
		/* bring p-lines up */
		if (popupbuff(bp) != TRUE
		 || bclear(bp) != TRUE) {
			return FALSE;
		}

		calledbefore = TRUE;
	}

	do {
		if (!addline(bp, linep->l_text, llength(linep)))
			break;	/* out of memory */
		if (flag & PNUMS) {
			BUFFER *savebp = curbp;
			WINDOW *savewp = curwp;
			curbp = bp;
			curwp = bp2any_wp(bp);
			DOT.l = lback(buf_head(bp));
			DOT.o = 0;
			bprintf("%s:%d:", oldbp->b_bname, line_no(oldbp, linep));
			DOT.o = 0;
			curbp = savebp;
			curwp = savewp;
		}
		linep = lforw(linep);
	} while (linep != region.r_end.l);

	set_bname(bp, P_LINES_BufName);
	set_rdonly(bp, non_filename(), MDVIEW);
	set_b_val(bp,VAL_TAB,tabstop_val(curbp));

	for_each_visible_window(wp) {
		if (wp->w_bufp == bp) {
			make_local_w_val(wp,WMDLIST);
			set_w_val(wp, WMDLIST, ((flag & PLIST) != 0) );
			wp->w_flag |= WFMODE|WFFORCE;
		}
	}
	return TRUE;
}

int
llineregion(void)
{
	return pregion(PLIST);
}

int
pplineregion(void)
{
	return pregion(PNUMS);
}

int
plineregion(void)
{
	return pregion(0);
}

static regexp *substexp;

int
substregion(void)
{
	return substreg1(TRUE,TRUE);
}

int
subst_again_region(void)
{
	return substreg1(FALSE,TRUE);
}

/* traditional vi & command */
/* ARGSUSED */
int
subst_again(int f GCC_UNUSED, int n GCC_UNUSED)
{
	int s;
	MARK curpos;

	curpos = DOT;

	/* the region spans just the line */
	MK.l = DOT.l;
	DOT.o = 0;
	MK.o = llength(MK.l);
	s = substreg1(FALSE,FALSE);
	if (s != TRUE) {
		mlforce("[No match.]");
		DOT = curpos;
		return s;
	}
	swapmark();
	return TRUE;
}

static int
substreg1(int needpats, int use_opts)
{
	int c, status;
	static int printit, globally, nth_occur, confirm;
	REGION region;
	LINEPTR oline;
	int	getopts = FALSE;
	char    tpat[NPAT];

	if ((status = get_fl_region(&region)) != TRUE) {
		return (status);
	}

	if (calledbefore == FALSE && needpats) {
		c = kbd_delimiter();
		if ((status = readpattern("substitute pattern: ", &pat[0],
					&gregexp, c, FALSE)) != TRUE) {
			if (status != ABORT)
				mlforce("[No pattern.]");
			return FALSE;
		}

		if (gregexp) {
			FreeIfNeeded(substexp);
			substexp = castalloc(regexp,(ALLOC_T)(gregexp->size));
			(void)memcpy((char *)substexp, (char *)gregexp,
							(SIZE_T)gregexp->size);
		}

		tpat[0] = 0;
		status = readpattern("replacement string: ",
				     &tpat[0], (regexp **)0, c, FALSE);

		(void)strcpy(rpat, tpat);
		if (status == ABORT)
			/* if false, the pattern is null, which is okay... */
			return FALSE;

		nth_occur = -1;
		confirm = printit = globally = FALSE;
		getopts = (lastkey == c); /* the user may have
						something to add */

	} else {
		if (!use_opts) {
			nth_occur = -1;
			printit = globally = FALSE;
		} else {
			if (more_named_cmd()) {
				unkeystroke(lastkey);
				nth_occur = -1;
				printit = globally = FALSE;
				getopts = TRUE;
			}
		}
	}

	if (getopts) {
		char buf[4];
		char *bp = buf;

		buf[0] = EOS;
		status = mlreply(
"(g)lobally, ([1-9])th occurrence on line, (c)onfirm, and/or (p)rint result: ",
			buf, sizeof buf);
		if (status == ABORT) {
			return FALSE;
		}
		nth_occur = 0;
		while (*bp) {
			if (*bp == 'p' && !printit) {
				printit = TRUE;
			} else if (*bp == 'g' &&
					!globally && !nth_occur) {
				globally = TRUE;
			} else if (isDigit(*bp) &&
					!nth_occur && !globally) {
				nth_occur = *bp - '0';
				globally = TRUE;
			} else if (*bp == 'c' && !confirm) {
				confirm = TRUE;
			} else if (!isSpace(*bp)) {
				mlforce("[Unknown action %s]",buf);
				return FALSE;
			}
			bp++;
		}
		if (!nth_occur)
			nth_occur = -1;
	}

	lines_changed =
	total_changes = 0;
	DOT.l = region.r_orig.l;	    /* Current line.	    */
	do {
		oline = DOT.l;
		if ((status = substline(substexp, nth_occur, printit,
						globally, &confirm)) != TRUE) {
			return status;
		}
		DOT.l = lforw(oline);
	} while (!sameline(DOT, region.r_end));
	calledbefore = TRUE;

	if (do_report(total_changes)) {
		mlforce("[%d change%s on %d line%s]",
			total_changes, PLURAL(total_changes),
			lines_changed, PLURAL(lines_changed));
	}
	return TRUE;
}

/* show the pattern we've matched (at least one space, even for empty-string) */
static void
showpat(regexp *rp, int on)
{
	register LINEPTR	lp;
	int	row;

	for (lp = curwp->w_line.l, row = curwp->w_toprow;
		lp != DOT.l;
			lp = lforw(lp))
		row += line_height(curwp,lp);

	hilite(row,
		offs2col(curwp, lp, DOT.o),
		offs2col(curwp, lp, (C_NUM)(DOT.o + rp->mlen)) + (rp->mlen == 0),
		on);
}

static int
substline(regexp *exp, int nth_occur, int printit, int globally, int *confirmp)
{
	int foundit;
	int again = 0;
	register int s;
	register int which_occur = 0;
	int matched_at_eol = FALSE;
	int yes, c, skipped;

	/* if the "magic number" hasn't been set yet... */
	if (!exp || UCHAR_AT(exp->program) != REGEXP_MAGIC) {
		mlforce("[No pattern set yet]");
		return FALSE;
	}

	ignorecase = window_b_val(curwp, MDIGNCASE);

	foundit = FALSE;
	scanboundpos.l = DOT.l;
	scanbound_is_header = FALSE;
	DOT.o = 0;
	do {
		scanboundpos.o = llength(DOT.l);
		s = scanner(exp, FORWARD, FALSE, (int *)0);
		if (s != TRUE)
			break;

		/* found the pattern */
		foundit = TRUE;
		which_occur++;
		if (nth_occur == -1 || which_occur == nth_occur) {
			(void)setmark();
			/* only allow one match at the end of line, to
				prevent loop with s/$/x/g  */
			if (MK.o == llength(DOT.l)) {
				if (matched_at_eol)
					break;
				matched_at_eol = TRUE;
			}

			/* if we need confirmation, get it */
			skipped = FALSE;
			if (*confirmp) {

				/* force the pattern onto the screen */
				(void)gomark(FALSE, 0);
				if (update(TRUE) == TRUE)
					showpat(exp, TRUE);
				s = mlquickask("Make change [y/n/q/a]?",
					"ynqa\r",&c);

				showpat(exp, FALSE);

				if (s != TRUE)
					c = 'q';

				switch(c) {
				case 'y' :
					yes = TRUE;
					break;
				default:
				case 'n' :
					yes = FALSE;
					(void)update(TRUE);
					skipped = TRUE;
					/* so we don't match this again */
					DOT.o += exp->mlen;
					break;
				case 'q' :
					mlerase();
					return(FALSE);
				case 'a' :
					yes = TRUE;
					*confirmp = FALSE;
					mlerase();
					break;
				}
			} else {
				yes = TRUE;
			}
			if (yes) {
				s = delins(exp, &rpat[0]);
				if (s != TRUE)
					return s;
				if (!again++)
					lines_changed++;
				total_changes++;
			}
			if (*confirmp && !skipped) /* force a screen update */
				(void)update(TRUE);

			if (exp->mlen == 0 && forwchar(TRUE,1) == FALSE)
				break;
			if (nth_occur > 0)
				break;
		} else { /* non-overlapping matches */
			s = forwchar(TRUE, (int)(exp->mlen));
			if (s != TRUE)
				return s;
		}
	} while (globally && sameline(scanboundpos,DOT));
	if (foundit && printit) {
		register WINDOW *wp = curwp;
		(void)setmark();
		s = plineregion();
		if (s != TRUE) return s;
		/* back to our buffer */
		swbuffer(wp->w_bufp);
	}
	if (*confirmp)
		mlerase();
	return TRUE;
}

/*
 * delins, modified by pgf from regsub
 *
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to redistribute it freely,
 *	subject to the following restrictions:
 *
 *	1. The author is not responsible for the consequences of use of
 *		this software, no matter how awful, even if they arise
 *		from defects in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *		by explicit claim or by omission.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *		be misrepresented as being the original software.
 */

static	char	*buf_delins;
static	UINT	len_delins;

/*
 - delins - perform substitutions after a regexp match
 */
static int
delins(regexp *exp, char *sourc)
{
	register char *src;
	register ALLOC_T dlength;
	register char c;
	register int no;
	int s;
#define NO_CASE	0
#define UPPER_CASE 1
#define LOWER_CASE 2
	int case_next, case_all;

	if (exp == NULL || sourc == NULL) {
		mlforce("BUG: NULL parm to delins");
		return FALSE;
	}
	if (UCHAR_AT(exp->program) != REGEXP_MAGIC) {
		regerror("damaged regexp fed to delins");
		return FALSE;
	}

	dlength = exp->mlen;

	if (buf_delins == NULL || dlength + 1 > len_delins) {
		if (buf_delins)
			free(buf_delins);
		if ((buf_delins = castalloc(char,dlength+1)) == NULL) {
			mlforce("[Out of memory in delins]");
			return FALSE;
		}
		len_delins = dlength + 1;
	}

	(void)memcpy(buf_delins, exp->startp[0], (SIZE_T)dlength);
	buf_delins[dlength] = EOS;

	if (ldelete((long) dlength, FALSE) != TRUE) {
		mlforce("[Error while deleting]");
		return FALSE;
	}
	src = sourc;
	case_next = case_all = NO_CASE;
	while ((c = *src++) != EOS) {
	    no = 0;
	    s = TRUE;
	    switch(c) {
	    case '\\':
		    c = *src++;
		    if (c == EOS)
			return TRUE;
		    if (!isDigit(c)) {
			    /* here's where the \U \E \u \l \t etc.
			    special escapes should be implemented */
			    switch (c) {
			    case 'U':
				    case_all = UPPER_CASE;
				    break;
			    case 'L':
				    case_all = LOWER_CASE;
				    break;
			    case 'u':
				    case_next = UPPER_CASE;
				    break;
			    case 'l':
				    case_next = LOWER_CASE;
				    break;
			    case 'E':
			    case 'e':
				    case_all = NO_CASE;
				    break;
			    case 'b':
				    s = linsert(1,'\b');
				    break;
			    case 'f':
				    s = linsert(1,'\f');
				    break;
			    case 'r':
				    s = linsert(1,'\r');
				    break;
			    case 't':
				    s = linsert(1,'\t');
				    break;
			    case 'n':
				    s = lnewline();
				    break;
			    default:
				    s = linsert(1,c);
				    break;
			    }
			    break;
		    }
		    /* else it's a digit --
		    	get pattern number, and fall through */
		    no = c - '0';
		    /* FALLTHROUGH */
	    case '&':
		    if (exp->startp[no] != NULL && exp->endp[no] != NULL) {
			    char *cp = buf_delins;
			    long len = exp->endp[no] - exp->startp[no];
			    long adj = exp->startp[no] - exp->startp[0];
			    cp += (size_t)adj;
			    while ((s == TRUE) && len--) {
				    c = *cp++;
				    if (c == EOS) {
					    mlforce( "BUG: mangled replace");
					    return FALSE;
				    }
				    if (c == '\n')
					    s = lnewline();
				    else if (case_next == NO_CASE && case_all == NO_CASE)
					    s = linsert(1,c);
				    else {
					    int direction = case_next != NO_CASE ? case_next : case_all;
					    case_next = NO_CASE;
					    /* Somewhat convoluted to handle
					       \u\L correctly (upper case first
					       char, lower case remainder).
					       This is the perl model, not the vi model. */
					    if (isUpper(c) && (direction == LOWER_CASE))
						    c = toLower(c);
					    if (isLower(c) && (direction == UPPER_CASE))
						    c = toUpper(c);
					    s = linsert(1,c);
				    }
			    }
		    }
		    break;

	    case '\n':
	    case '\r':
		    s = lnewline();
		    break;

	    default:
		    if (case_next || case_all) {
			    int direction = case_next != NO_CASE ? case_next : case_all;
			    case_next = NO_CASE;
			    /* Somewhat convoluted to handle
			       \u\L correctly (upper case first
			       char, lower case remainder).
			       This is the perl model, not the vi model. */
			    if (isUpper(c) && (direction == LOWER_CASE))
				    c = toLower(c);
			    if (isLower(c) && (direction == UPPER_CASE))
				    c = toUpper(c);
		    }
		    s = linsert(1,c);
		    break;
	    }
	    if (s != TRUE) {
		    mlforce("[Out of memory while inserting]");
		    return FALSE;
	    }
	}
	return TRUE;
}

#if NO_LEAKS
void
onel_leaks(void)
{
	FreeIfNeeded(substexp);
	FreeIfNeeded(buf_delins);
}
#endif
