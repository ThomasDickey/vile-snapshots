/*
 *	A few functions that used to operate on single whole lines, mostly
 *	here to support the globals() function.  They now work on regions.
 *	Copyright (c) 1996-2019,2025 by Thomas E. Dickey
 *	Copyright (c) 1990, 1995-1999 by Paul Fox, except for delins(), which is
 *	Copyright (c) 1986 by University of Toronto, as noted below.
 *
 * $Id: oneliner.c,v 1.129 2025/01/26 17:16:19 tom Exp $
 */

#include	"estruct.h"
#include	"edef.h"

#define PNONE	0x00
#define PLIST	0x01		/* shows line in list-mode */
#define PNUMS	0x02		/* shows line in number-mode */
#define PGREP	0x04		/* like "grep -n": buffer name, line number */

static int delins(regexp * exp, char *sourc, int lensrc);
static int substline(regexp * exp, int nth_occur, int printit, int globally, int *confirmp);
static int substreg1(int needpats, int use_opts, int is_globalsub);

static int lines_changed, total_changes;

/*
 * put lines in a popup window
 */
static int
pregion(UINT flag)
{
    WINDOW *wp;
    BUFFER *bp;
    int status;
    REGION region;
    LINE *linep;
    BUFFER *oldbp = curbp;

    if ((status = get_fl_region(oldbp, &region)) != TRUE) {
	return (status);
    }

    linep = region.r_orig.l;	/* Current line.        */

    /* first check if we are already here */
    bp = bfind(P_LINES_BufName, 0);
    if (bp == NULL) {
	return FALSE;
    }

    if (bp == curbp) {
	mlforce("[Can't do that from this buffer.]");
	return FALSE;
    }

    if (!calledbefore) {	/* fresh start */
	/* bring p-lines up */
	if (popupbuff(bp) != TRUE
	    || bclear(bp) != TRUE) {
	    return FALSE;
	}

	calledbefore = TRUE;
    }

    do {
	if (!addline(bp, lvalue(linep), llength(linep)))
	    break;		/* out of memory */
	if (flag & (PNUMS | PGREP)) {
	    BUFFER *savebp = curbp;
	    WINDOW *savewp = curwp;
	    curbp = bp;
	    curwp = bp2any_wp(bp);
	    if (flag & PGREP) {
		DOT.l = lback(buf_head(bp));
		DOT.o = b_left_margin(bp);
		bprintf("%s:%d:", oldbp->b_bname, line_no(oldbp, linep));
		DOT.o = b_left_margin(bp);
	    } else {
		make_local_w_val(curwp, WMDNUMBER);
		set_w_val(curwp, WMDNUMBER, TRUE);
		flag &= (UINT) (~PNUMS);
	    }
	    curbp = savebp;
	    curwp = savewp;
	}
	linep = lforw(linep);
    } while (linep != region.r_end.l);

    set_bname(bp, P_LINES_BufName);
    set_rdonly(bp, non_filename(), MDVIEW);
    set_b_val(bp, VAL_TAB, tabstop_val(curbp));
    b_clr_counted(bp);

    for_each_visible_window(wp) {
	if (wp->w_bufp == bp) {
	    make_local_w_val(wp, WMDLIST);
	    set_w_val(wp, WMDLIST, ((flag & PLIST) != 0));
	    wp->w_flag |= WFMODE | WFFORCE;
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
nlineregion(void)
{
    return pregion(PNUMS);
}

int
pplineregion(void)
{
    return pregion(PGREP);
}

int
plineregion(void)
{
    return pregion(PNONE);
}

static regexp *substexp;

int
substregion(void)
{
    return substreg1(TRUE, TRUE, FALSE);
}

int
subst_again_region(void)
{
    return substreg1(FALSE, TRUE, FALSE);
}

int
subst_all_region(void)
{
    return substreg1(TRUE, TRUE, TRUE);
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
    DOT.o = b_left_margin(curbp);
    MK.o = llength(MK.l);
    s = substreg1(FALSE, FALSE, FALSE);
    if (s != TRUE) {
	mlforce("[No match.]");
	DOT = curpos;
	return s;
    }
    swapmark();
    return TRUE;
}

static int
substreg1(int needpats, int use_opts, int is_globalsub)
{
    int c, status;
    static int printit, globally, nth_occur, confirm;
    REGION region;
    LINE *oline;
    int getopts = FALSE;
    TBUFF *newpattern;

    if ((status = get_fl_region(curbp, &region)) != TRUE) {
	return (status);
    }

    if (calledbefore == FALSE && needpats) {
	c = kbd_delimiter();
	if ((status = readpattern("substitute pattern: ", &searchpat,
				  &gregexp, c, FALSE)) != TRUE) {
	    if (status != ABORT)
		mlforce("[No pattern.]");
	    return status;
	}

	if (gregexp) {
	    FreeIfNeeded(substexp);
	    if ((substexp = castalloc(regexp, (size_t) (gregexp->size))) == NULL) {
		no_memory("substreg1");
		return FALSE;
	    }
	    (void) memcpy((char *) substexp, (char *) gregexp,
			  (size_t) gregexp->size);
	}

	newpattern = NULL;
	tb_init(&newpattern, EOS);
	status = readpattern("replacement string: ",
			     &newpattern, (regexp **) 0, c, FALSE);
	if (status == ABORT) {
	    /* if false, the pattern is null, which is okay... */
	    tb_free(&newpattern);
	    return status;
	}
	tb_free(&replacepat);
	replacepat = newpattern;

	nth_occur = -1;
	confirm = printit = globally = FALSE;
	getopts = (lastkey == c);	/* the user may have
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
	status =
	    mlreply("(g)lobally, ([1-9])th occurrence on line, (c)onfirm, and/or (p)rint result: ",
		    buf, (UINT) sizeof buf);
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
		mlforce("[Unknown action %s]", buf);
		return FALSE;
	    }
	    bp++;
	}
	if (!nth_occur)
	    nth_occur = -1;
    }

    if (is_globalsub)
	globally = TRUE;

    lines_changed =
	total_changes = 0;
    DOT.l = region.r_orig.l;	/* Current line.        */
    do {
	oline = DOT.l;
	if ((status = substline(substexp, nth_occur, printit,
				globally, &confirm)) != TRUE) {
	    return status;
	}
	DOT.l = lforw(oline);
	DOT.o = b_left_margin(curbp);
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
showpat(regexp * rp, int on)
{
    LINE *lp;
    int row;

    for (lp = curwp->w_line.l, row = curwp->w_toprow;
	 lp != DOT.l;
	 lp = lforw(lp))
	row += line_height(curwp, lp);

    hilite(row,
	   offs2col(curwp, lp, DOT.o),
	   offs2col(curwp, lp, (DOT.o + (C_NUM) rp->mlen)) + (rp->mlen == 0),
	   on);
}

static int
substline(regexp * exp, int nth_occur, int printit, int globally, int *confirmp)
{
    int foundit;
    int again = 0;
    int s;
    int which_occur = 0;
    int at_bol = (DOT.o <= b_left_margin(curbp));
    int ic;
    int matched_at_eol = FALSE;
    int yes, c, skipped;

    assert(curwp != NULL);

    TRACE((T_CALLED
	   "substline(exp=%p, nth_occur=%d, printit=%d, globally=%d, confirmp=%p) bol:%d\n",
	   (void *) exp, nth_occur, printit, globally, (void *) confirmp, at_bol));

    /* if the "magic number" hasn't been set yet... */
    if (!exp || UCHAR_AT(exp->program) != REGEXP_MAGIC) {
	mlforce("[No pattern set yet]");
	returnCode(FALSE);
    }

    if (curwp == NULL)
	returnCode(FALSE);

    ic = window_b_val(curwp, MDIGNCASE);

    foundit = FALSE;
    scanboundpos.l = DOT.l;
    scanbound_is_header = FALSE;
    DOT.o = b_left_margin(curbp);
    do {
	scanboundpos.o = llength(DOT.l);
	s = scanner(exp, FORWARD, FALSE, at_bol, ic, (int *) 0);
	if (s != TRUE)
	    break;

	/* found the pattern */
	foundit = TRUE;
	which_occur++;
	at_bol = FALSE;		/* at most, match "^" once */
	if (nth_occur == -1 || which_occur == nth_occur) {
	    (void) setmark();
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
		(void) gomark(FALSE, 0);
		if (update(TRUE) == TRUE)
		    showpat(exp, TRUE);
		s = mlquickask("Make change [y/n/q/a]?",
			       "ynqa\r", &c);

		showpat(exp, FALSE);

		if (s != TRUE)
		    c = 'q';

		switch (c) {
		case 'y':
		    yes = TRUE;
		    break;
		default:
		case 'n':
		    yes = FALSE;
		    (void) update(TRUE);
		    skipped = TRUE;
		    /* so we don't match this again */
		    DOT.o += (C_NUM) exp->mlen;
		    break;
		case 'q':
		    mlerase();
		    returnCode(FALSE);
		case 'a':
		    yes = TRUE;
		    *confirmp = FALSE;
		    mlerase();
		    break;
		}
	    } else {
		yes = TRUE;
	    }
	    if (yes) {
		s = delins(exp, tb_values(replacepat), (int) tb_length(replacepat));
		if (s != TRUE)
		    returnCode(s);
		if (!again++)
		    lines_changed++;
		total_changes++;
	    }
	    if (*confirmp && !skipped)	/* force a screen update */
		(void) update(TRUE);

	    if (exp->mlen == 0 && forwchar(TRUE, 1) == FALSE)
		break;
	    if (nth_occur > 0)
		break;
	} else {		/* non-overlapping matches */
	    s = forwchar(TRUE, (int) (exp->mlen));
	    if (s != TRUE)
		returnCode(s);
	}
    } while (globally && sameline(scanboundpos, DOT));
    if (foundit && printit) {
	WINDOW *wp = curwp;
	(void) setmark();
	s = plineregion();
	if (s != TRUE)
	    returnCode(s);
	/* back to our buffer */
	swbuffer(wp->w_bufp);
    }
    if (*confirmp)
	mlerase();
    returnCode(TRUE);
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

static char *buf_delins;
static size_t len_delins;

/*
 - delins - perform substitutions after a regexp match
 */
static int
delins(regexp * exp, char *sourc, int lensrc)
{
    size_t dlength;
    int c;
    int no;
    int j;
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
	if ((buf_delins = castalloc(char, dlength + 1)) == NULL) {
	    mlforce("[Out of memory in delins]");
	    return FALSE;
	}
	len_delins = dlength + 1;
    }

    (void) memcpy(buf_delins, exp->startp[0], dlength);
    buf_delins[dlength] = EOS;

    if (ldel_bytes((B_COUNT) dlength, FALSE) != TRUE) {
	mlforce("[Error while deleting]");
	return FALSE;
    }
    case_next = case_all = NO_CASE;
    for (j = 0; j < lensrc; ++j) {
	c = sourc[j];
	no = 0;
	s = TRUE;
	switch (c) {
	case BACKSLASH:
	    if (j + 1 >= lensrc)
		return TRUE;
	    c = sourc[++j];
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
		    s = lins_bytes(1, '\b');
		    break;
		case 'f':
		    s = lins_bytes(1, '\f');
		    break;
		case 'r':
		    s = lins_bytes(1, '\r');
		    break;
		case 't':
		    s = lins_bytes(1, '\t');
		    break;
		case 'n':
		    s = lnewline();
		    break;
		default:
		    s = lins_bytes(1, c);
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
		long len = (long) (exp->endp[no] - exp->startp[no]);
		long adj = (long) (exp->startp[no] - exp->startp[0]);
		cp += (size_t) adj;
		while ((s == TRUE) && len--) {
		    c = *cp++;
		    if (c == EOS) {
			mlforce("BUG: mangled replace");
			return FALSE;
		    }
		    if (c == '\n')
			s = lnewline();
		    else if (case_next == NO_CASE && case_all == NO_CASE)
			s = lins_bytes(1, c);
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
			s = lins_bytes(1, c);
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
	    s = lins_bytes(1, c);
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
