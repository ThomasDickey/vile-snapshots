/*
 * The routines in this file move the cursor around on the screen. They
 * compute a new value for the cursor, then adjust ".". The display code
 * always updates the cursor location, so only moves between lines, or
 * functions that adjust the top line in the window and invalidate the
 * framing, are hard.
 *
 * $Id: basic.c,v 1.182 2025/01/26 20:55:32 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"

#define	RegexpLen(exp) ((exp->mlen) ? (int)(exp->mlen) : 1)

/*
 * Replace "DOT.o++".
 */
#if OPT_MULTIBYTE
#define lp_bytes_at0(lp, off) bytes_at0(lvalue(lp), llength(lp), off)
#define tb_bytes_at0(tb, off) bytes_at0(tb_values(tb), (int) tb_length(tb), off)

static int
bytes_at0(const char *value, int length, int off)
{
    return ((length > off && off >= 0)
	    ? vl_conv_to_utf32((UINT *) 0,
			       value + off,
			       (B_COUNT) (length - off))
	    : 0);
}

int
bytes_at(LINE *lp, int off)
{
    int rc = lp_bytes_at0(lp, off);
    return rc ? rc : 1;
}

/*
 * Replace "DOT.o--", find the number of bytes of a multibyte character before
 * the given offset.
 */
int
bytes_before(LINE *lp, int off)
{
    int rc = 0;
    int first = off;
    int legal = FALSE;

    while (off-- > 0) {
	++rc;
	if (lp_bytes_at0(lp, off) == rc) {
	    legal = TRUE;
	    break;
	}
    }
    if (!legal && first)
	rc = 1;
    return rc;
}

int
tb_bytes_before(TBUFF *tb, int off)
{
    int rc = 0;
    int first = off;
    int legal = FALSE;

    while (off-- > 0) {
	++rc;
	if (tb_bytes_at0(tb, off) == rc) {
	    legal = TRUE;
	    break;
	}
    }
    if (!legal && first)
	rc = 1;
    return rc;
}

/*
 * Count the number of characters from the given offset to the end of the line.
 */
int
chars_to_eol(LINE *lp, int off)
{
    int rc = bytes_to_eol(lp, off);
    if (b_is_utfXX(curbp)) {
	rc = 0;
	while (off < llength(lp)) {
	    off += bytes_at(lp, off);
	    ++rc;
	}
    }
    return rc;
}

/*
 * Count the number of characters from the given offset to the beginning of the
 * line.
 */
int
chars_to_bol(LINE *lp, int off)
{
    int rc = bytes_to_bol(lp, off);
    if (b_is_utfXX(curbp)) {
	int xx = 0;
	rc = 0;
	while (xx < off) {
	    xx += bytes_at(lp, xx);
	    ++rc;
	}
    }
    return rc;
}

/*
 * Count the number of bytes in the given number of characters in the line
 * starting from the given offset.
 */
int
count_bytes(LINE *lp, int off, int chars)
{
    int rc = chars;
    if (b_is_utfXX(curbp)) {
	int xx = 0;
	while (off < llength(lp) && chars-- > 0) {
	    int value = bytes_at(lp, off);
	    off += value;
	    xx += value;
	}
	rc = xx;
    }
    return rc;
}

/*
 * Count the number of characters in the given number of bytes in the line
 * starting from the given offset.
 */
int
count_chars(LINE *lp, int off, int bytes)
{
    int rc = bytes;
    if (b_is_utfXX(curbp)) {
	int xx = 0;
	while (off < llength(lp) && bytes > 0) {
	    int value = bytes_at(lp, off);
	    off += value;
	    bytes -= value;
	    ++xx;
	}
	rc = xx;
    }
    return rc;
}

int
mb_cellwidth(WINDOW *wp, const char *text, int limit)
{
    UINT value;
    int rc = COLS_UTF8;		/* "\uXXXX" */

    vl_conv_to_utf32(&value, text, (B_COUNT) limit);
    if (isPrint(value) && FoldTo8bits((int) value)) {
	rc = 1;
	if (w_val(wp, WMDUNICODE_AS_HEX)) {
	    rc = COLS_UTF8;
	} else if (!isPrint(value)) {
	    rc = COLS_8BIT;
	}
    } else if (term_is_utfXX()) {
	rc = vl_wcwidth((int) value);
	if (rc <= 0)
	    rc = COLS_UTF8;
    }
    return rc;
}
#endif /* OPT_MULTIBYTE */

/* utility routine for 'forwpage()' and 'backpage()' */
static int
full_pages(int f, int n)
{
    if (f == FALSE) {
	n = curwp->w_ntrows - 2;	/* Default scroll.      */
	if (n <= 0)		/* Don't blow up if the */
	    n = 1;		/* window is tiny.  */
    }
#if OPT_CVMVAS
    else if (n > 0)		/* Convert from pages */
	n *= curwp->w_ntrows;	/* to lines.  */
#endif
    return n;
}

/* utility routine for 'forwhpage()' and 'backhpage()' */
static int
half_pages(int f, int n)
{
    if (f == FALSE) {
	n = curwp->w_ntrows / 2;	/* Default scroll.  */
	if (n <= 0)		/* Forget the overlap */
	    n = 1;		/* if tiny window.  */
    }
#if OPT_CVMVAS
    else if (n > 0)		/* Convert from pages */
	n *= curwp->w_ntrows / 2;	/* to lines.  */
#endif
    return n;
}

static void
skipblanksf(void)
{
    while (lforw(DOT.l) != buf_head(curbp) && is_empty_line(DOT)) {
	dot_next_bol();
    }
}

static void
skipblanksb(void)
{
    while (lback(DOT.l) != buf_head(curbp) && is_empty_line(DOT)) {
	dot_prev_bol();
    }
}

/*
 * Implements the vi "0" command.
 *
 * Move the cursor to the beginning of the current line.
 */
/* ARGSUSED */
int
gotobol(int f GCC_UNUSED, int n GCC_UNUSED)
{
    DOT.o = w_left_margin(curwp);
    return mvleftwind(TRUE, w_val(curwp, WVAL_SIDEWAYS));
}

/*
 * Move the cursor backwards by "n" characters. If "n" is less than zero call
 * "forwchar" to actually do the move. Otherwise compute the new cursor
 * location. Error if you try and move out of the buffer. Set the flag if the
 * line pointer for dot changes.
 */
int
backchar(int f, int n)
{
    LINE *lp;

    n = need_a_count(f, n, 1);

    if (n < 0)
	return (forwchar(f, -n));
    while (n--) {
	if (DOT.o == w_left_margin(curwp)) {
	    if ((lp = lback(DOT.l)) == buf_head(curbp))
		return (FALSE);
	    DOT.l = lp;
	    DOT.o = llength(lp);
	    curwp->w_flag |= WFMOVE;
	} else {
	    DOT.o -= BytesBefore(DOT.l, DOT.o);
	}
    }
    return (TRUE);
}

/*
 * Implements the vi "h" command.
 *
 * Move the cursor backwards by "n" characters. Stop at beginning of line.
 */
int
backchar_to_bol(int f, int n)
{
    int rc = TRUE;

    n = need_a_count(f, n, 1);

    if (n < 0) {
	rc = forwchar_to_eol(f, -n);
    } else {
	while (n--) {
	    if (DOT.o == w_left_margin(curwp)) {
		rc = doingopcmd;
		break;
	    } else {
		DOT.o -= BytesBefore(DOT.l, DOT.o);
	    }
	}
    }
    return rc;
}

/*
 * Implements the vi "$" command.
 *
 * Move the cursor to the end of the current line.  Trivial.
 */
int
gotoeol(int f, int n)
{
    if (f == TRUE) {
	if (n > 0)
	    --n;
	else if (n < 0)
	    ++n;
	if (forwline(f, n) != TRUE)
	    return FALSE;
    }
    DOT.o = llength(DOT.l);
    curgoal = VL_HUGE;
    return (TRUE);
}

/*
 * Move the cursor forwards by "n" characters. If "n" is less than zero call
 * "backchar" to actually do the move. Otherwise compute the new cursor
 * location, and move ".". Error if you try and move off the end of the
 * buffer. Set the flag if the line pointer for dot changes.
 */
int
forwchar(int f, int n)
{
    int rc = TRUE;

    n = need_a_count(f, n, 1);

    if (n < 0) {
	rc = backchar(f, -n);
    } else {
	while (n--) {
	    /* if an explicit arg was given, allow us to land
	       on the newline, else skip it */
	    if (is_at_end_of_line(DOT) ||
		(f == FALSE && !insertmode &&
		 llength(DOT.l) && DOT.o == llength(DOT.l) - 1)) {
		if (is_header_line(DOT, curbp) ||
		    is_last_line(DOT, curbp)) {
		    rc = FALSE;
		    break;
		}
		DOT.l = lforw(DOT.l);
		DOT.o = w_left_margin(curwp);
		curwp->w_flag |= WFMOVE;
	    } else {
		DOT.o += BytesAt(DOT.l, DOT.o);
	    }
	}
    }
    return (rc);
}

/*
 * Implements the vi "l" command.
 *
 * Move the cursor forwards by "n" characters. Don't go past end-of-line
 *
 * If the flag 'doingopcmd' is set, implements a vi "l"-like motion for
 * internal use.  The end-of-line test is off-by-one from the true "l" command
 * to allow for substitutions at the end of a line.
 */
int
forwchar_to_eol(int f, int n)
{
    int rc = TRUE;
    int nwas = n;
    int lim;

    n = need_a_count(f, n, 1);

    if (n < 0) {
	rc = backchar_to_bol(f, -n);
    } else if (n != 0) {

	/* normally, we're confined to the text on the line itself.  if
	   we're doing an opcmd, then we're allowed to move to the newline
	   as well, to take care of the internal cases:  's', 'x', and '~'. */
	if (doingopcmd || insertmode)
	    lim = llength(DOT.l);
	else
	    lim = llength(DOT.l) - 1;
	do {
	    if (DOT.o >= lim) {
		rc = (n != nwas);	/* return ok if we moved at all */
		break;
	    } else {
		DOT.o += BytesAt(DOT.l, DOT.o);
	    }
	} while (--n != 0);
    }
    return rc;
}

/*
 * Move to a particular line (the argument).  Count from bottom of file if
 * argument is negative.
 */
int
vl_gotoline(int n)
{
    int status = TRUE;		/* status return */

    MARK odot;

    if (n == 0)			/* if a bogus argument...then leave */
	return (FALSE);

    odot = DOT;

    DOT.o = w_left_margin(curwp);
    if (n < 0) {
	DOT.l = lback(buf_head(curbp));
	status = backline(TRUE, -n - 1);
    } else {
	DOT.l = lforw(buf_head(curbp));
	status = forwline(TRUE, n - 1);
    }
    if (status != TRUE) {
	DOT = odot;
	return status;
    }
    (void) firstnonwhite(FALSE, 1);
    curwp->w_flag |= WFMOVE;
    return TRUE;
}

/*
 * Implements the vi "G" command.
 *
 * Move to a particular line (the argument).  Count from bottom of file if
 * argument is negative.
 */
int
gotoline(int f, int n)
{
    int status;			/* status return */

    if (f == FALSE) {
	status = gotoeob(f, n);
    } else {
	status = vl_gotoline(n);
	if (status != TRUE)
	    mlwarn("[Not that many lines in buffer: %ld]", absol((long) n));
    }
    return status;
}

/*
 * Goto the beginning of the buffer. Massive adjustment of dot. This is
 * considered to be hard motion; it really isn't if the original value of dot
 * is the same as the new value of dot.
 */
/* ARGSUSED */
int
gotobob(int f GCC_UNUSED, int n GCC_UNUSED)
{
    DOT.l = lforw(buf_head(curbp));
    DOT.o = w_left_margin(curwp);
    curwp->w_flag |= WFMOVE;
    return (TRUE);
}

/*
 * Move to the end of the buffer. Dot is always put at the end of the file.
 */
/* ARGSUSED */
int
gotoeob(int f GCC_UNUSED, int n GCC_UNUSED)
{
    DOT.l = lback(buf_head(curbp));
    curwp->w_flag |= WFMOVE;
    return firstnonwhite(FALSE, 1);
}

/*
 * Implements the vi "H" command.
 *
 * Move to first (or nth) line in window
 */
int
gotobos(int f, int n)
{
    LINE *last = DOT.l;

    n = need_at_least(f, n, 1);

    DOT.l = curwp->w_line.l;
    while (--n != 0) {
	if (is_last_line(DOT, curbp))
	    break;
	dot_next_bol();
    }

    if (DOT.l != last)
	curwp->w_flag |= WFMOVE;
    return firstnonwhite(FALSE, 1);
}

/*
 * Implements the vi "M" command.
 *
 * Move to the middle of lines displayed in window
 */
/* ARGSUSED */
int
gotomos(int f GCC_UNUSED, int n)
{
    LINE *last = DOT.l;
    LINE *lp, *head;
    int half = (curwp->w_ntrows + 1) / 2;

    head = buf_head(curbp);
    for (n = 0, lp = curwp->w_line.l; lp != head; lp = lforw(lp)) {
	if (n < half)
	    DOT.l = lp;
	if ((n += line_height(curwp, lp)) >= curwp->w_ntrows)
	    break;
    }
    if (n < curwp->w_ntrows) {	/* then we hit eof before eos */
	half = (n + 1) / 2;	/* go back up */
	for (n = 0, lp = curwp->w_line.l; lp != head; lp = lforw(lp)) {
	    DOT.l = lp;
	    if ((n += line_height(curwp, lp)) >= half)
		break;
	}
    }

    if (DOT.l != last)
	curwp->w_flag |= WFMOVE;
    return firstnonwhite(FALSE, 1);
}

/*
 * Implements the vi "L" command.
 *
 * Move to the last (or nth last) line in window
 */
int
gotoeos(int f, int n)
{
    LINE *last = DOT.l;
    int nn;

    n = need_at_least(f, n, 1);

    /* first get to the end */
    DOT.l = curwp->w_line.l;
    nn = curwp->w_ntrows;
    while ((nn -= line_height(curwp, DOT.l)) > 0) {
	if (is_last_line(DOT, curbp))
	    break;
	dot_next_bol();
    }
#ifdef WMDLINEWRAP
    /* adjust if we pointed to a line-fragment */
    if (w_val(curwp, WMDLINEWRAP)
	&& nn < 0
	&& DOT.l != curwp->w_line.l)
	DOT.l = lback(DOT.l);
#endif
    /* and then go back up */
    /* (we're either at eos or eof) */
    while (--n != 0) {
	if (sameline(DOT, curwp->w_line))
	    break;
	DOT.l = lback(DOT.l);
    }
    if (DOT.l != last)
	curwp->w_flag |= WFMOVE;
    return firstnonwhite(FALSE, 1);
}

/*
 * Implements the vi "j" command.
 *
 * Move forward by full lines. If the number of lines to move is less than
 * zero, call the backward line function to actually do it. The last command
 * controls how the goal column is set.
 */
int
forwline(int f, int n)
{
    int rc;
    LINE *dlp;

    n = need_a_count(f, n, 1);

    if (n < 0) {
	rc = backline(f, -n);
    } else if (n == 0) {
	rc = TRUE;
    } else {

	/* set the "goal" column if necessary */
	if (curgoal < 0)
	    curgoal = getccol(FALSE);

	/* loop downwards */
	dlp = DOT.l;
	rc = TRUE;
	do {
	    LINE *nlp = lforw(dlp);
	    if (nlp == buf_head(curbp)) {
		rc = FALSE;
		break;
	    }
	    dlp = nlp;
	} while (--n != 0);

	if (rc) {
	    /* set dot */
	    DOT.l = dlp;
	    DOT.o = getgoal(dlp);
	    curwp->w_flag |= WFMOVE;
	}
    }
    return rc;
}
/*
 * Implements the vi "^" command.
 *
 * Move to the first nonwhite character on the current line.  No errors are
 * returned.
 */
/* ARGSUSED */
int
firstnonwhite(int f GCC_UNUSED, int n GCC_UNUSED)
{
    DOT.o = firstchar(DOT.l);
    if (DOT.o < w_left_margin(curwp)) {
	if (llength(DOT.l) <= w_left_margin(curwp))
	    DOT.o = w_left_margin(curwp);
	else
	    DOT.o = llength(DOT.l) - 1;
    }
    return TRUE;
}

/* ARGSUSED */
#if !SMALLER
int
lastnonwhite(int f GCC_UNUSED, int n GCC_UNUSED)
{
    DOT.o = lastchar(DOT.l);
    if (DOT.o < w_left_margin(curwp))
	DOT.o = w_left_margin(curwp);
    return TRUE;
}
#endif

/* return the offset of the first non-white character on the line,
	or -1 if there are no non-white characters on the line */
int
firstchar(LINE *lp)
{
    int off = w_left_margin(curwp);
    while (off < llength(lp) && isBlank(lgetc(lp, off)))
	off++;
    if (off == llength(lp))
	return -1;
    return off;
}

/* return the offset of the next non-white character on the line,
	or -1 if there are no more non-white characters on the line */
int
nextchar(LINE *lp, int off)
{
    while (off < llength(lp)) {
	if (!isSpace(lgetc(lp, off)))
	    return off;
	off++;
    }
    return -1;
}

/* return the offset of the last non-white character on the line
	or -1 if there are no non-white characters on the line */
int
lastchar(LINE *lp)
{
    int off = llength(lp) - 1;
    while (off >= 0 && isSpace(lgetc(lp, off)))
	off--;
    return off;
}

/*
 * Implements the vi "^M" command.
 *
 * Like 'forwline()', but goes to the first non-white character position.
 */
int
forwbline(int f, int n)
{
    int s;

    n = need_a_count(f, n, 1);

    if ((s = forwline(f, n)) != TRUE)
	return (s);
    return firstnonwhite(FALSE, 1);
}

/*
 * Implements the vi "-" command.
 *
 * Like 'backline()', but goes to the first non-white character position.
 */
int
backbline(int f, int n)
{
    int s;

    n = need_a_count(f, n, 1);

    if ((s = backline(f, n)) != TRUE)
	return (s);
    return firstnonwhite(FALSE, 1);
}

/*
 * Implements the vi "k" command.
 *
 * This function is like "forwline", but goes backwards.
 */
int
backline(int f, int n)
{
    int rc;
    LINE *dlp;

    n = need_a_count(f, n, 1);

    if (n < 0) {
	rc = forwline(f, -n);
    } else if (is_first_line(DOT, curbp)) {
	/* cannot move up */
	rc = FALSE;
    } else {
	/* set the "goal" column if necessary */
	if (curgoal < 0)
	    curgoal = getccol(FALSE);

	/* loop upwards */
	dlp = DOT.l;
	while (n-- && lback(dlp) != buf_head(curbp))
	    dlp = lback(dlp);

	/* set dot */
	DOT.l = dlp;
	DOT.o = getgoal(dlp);
	curwp->w_flag |= WFMOVE;
	rc = TRUE;
    }
    return rc;
}

/*
 * Go to the beginning of the current paragraph.
 */
int
gotobop(int f, int n)
{
    MARK odot;
    int was_on_empty;
    int fc;

    n = need_a_count(f, n, 1);

    was_on_empty = is_empty_line(DOT);
    odot = DOT;

    fc = firstchar(DOT.l);
    if (doingopcmd &&
	((fc >= 0 && DOT.o <= fc) || fc < 0) &&
	!is_first_line(DOT, curbp)) {
	backchar(TRUE, DOT.o + 1);
	pre_op_dot = DOT;
    }
    while (n) {
	if (findpat(TRUE, 1, b_val_rexp(curbp, VAL_PARAGRAPHS)->reg,
		    REVERSE) != TRUE) {
	    (void) gotobob(f, n);
	} else if (is_empty_line(DOT)) {
	    /* special case -- if we found an empty line,
	       and it's adjacent to where we started,
	       skip all adjacent empty lines, and try again */
	    if ((was_on_empty && lforw(DOT.l) == odot.l) ||
		(n > 0 && llength(lforw(DOT.l)) == 0)) {
		/* then we haven't really found what we
		   wanted.  keep going */
		skipblanksb();
		continue;
	    }
	}
	n--;
    }
    if (doingopcmd) {
	fc = firstchar(DOT.l);
	if (!sameline(DOT, odot) &&
	    (pre_op_dot.o > lastchar(pre_op_dot.l)) &&
	    ((fc >= 0 && DOT.o <= fc) || fc < 0)) {
	    regionshape = rgn_FULLLINE;
	}
    }
    return TRUE;
}

/*
 * Go to the end of the current paragraph.
 */
int
gotoeop(int f, int n)
{
    MARK odot;
    int was_at_bol;
    int was_on_empty;
    int fc;

    n = need_a_count(f, n, 1);

    fc = firstchar(DOT.l);
    was_on_empty = is_empty_line(DOT);
    was_at_bol = ((fc >= 0 && DOT.o <= fc) || fc < 0);
    odot = DOT;

    while (n) {
	if (findpat(TRUE, 1, b_val_rexp(curbp, VAL_PARAGRAPHS)->reg,
		    FORWARD) != TRUE) {
	    DOT = curbp->b_line;
	} else if (is_empty_line(DOT)) {
	    /* special case -- if we found an empty line. */
	    /* either as the very next line, or at the end of
	       our search */
	    if ((was_on_empty && lback(DOT.l) == odot.l) ||
		(n > 0 && llength(lback(DOT.l)) == 0)) {
		/* then we haven't really found what we
		   wanted.  keep going */
		skipblanksf();
		continue;
	    }
	}
	n--;
    }
    if (doingopcmd) {
	/* if we're now at the beginning of a line and we can back up,
	   do so to avoid eating the newline and leading whitespace */
	fc = firstchar(DOT.l);
	if (((fc >= 0 && DOT.o <= fc) || fc < 0) &&
	    !is_first_line(DOT, curbp) &&
	    !sameline(DOT, odot)) {
	    backchar(TRUE, DOT.o + 1);
	}
	/* if we started at the start of line, eat the whole line */
	if (!sameline(DOT, odot) && was_at_bol)
	    regionshape = rgn_FULLLINE;
    }
    return TRUE;
}

#if OPT_STUTTER_SEC_CMD
getstutter(void)
{
    int thiskey;
    if (!clexec) {
	thiskey = lastkey;
	kbd_seq();
	if (thiskey != lastkey) {
	    return FALSE;
	}
    }
    return TRUE;
}
#endif

/*
 * Go to the beginning of the current section (or paragraph if no section
 * marker found).
 */
int
gotobosec(int f, int n)
{
#if OPT_STUTTER_SEC_CMD
    if (!getstutter())
	return FALSE;
#endif
    if (findpat(f, n, b_val_rexp(curbp, VAL_SECTIONS)->reg,
		REVERSE) != TRUE) {
	(void) gotobob(f, n);
    }
    return TRUE;
}

/*
 * Go to the end of the current section (or paragraph if no section marker
 * found).
 */
int
gotoeosec(int f, int n)
{
#if OPT_STUTTER_SEC_CMD
    if (!getstutter())
	return FALSE;
#endif
    if (findpat(f, n, b_val_rexp(curbp, VAL_SECTIONS)->reg,
		FORWARD) != TRUE) {
	DOT = curbp->b_line;
    }
    return TRUE;
}

/*
 * Go to the beginning of the current sentence. If we skip into an empty line
 * (from a non-empty line), return at that point -- that's what vi does.
 */
int
gotobosent(int f, int n)
{
    MARK savepos;
    int looped = 0;
    int extra;
    int empty = is_empty_line(DOT);
    regexp *exp;
    int s = TRUE;

    savepos = DOT;
    exp = b_val_rexp(curbp, VAL_SENTENCES)->reg;

    while (s && (is_at_end_of_line(DOT) || isSpace(CharAtDot()))) {
	s = backchar(TRUE, 1);
	if (is_empty_line(DOT) && !empty)
	    return TRUE;
    }
  top:
    extra = 0;
    if (findpat(f, n, exp, REVERSE) != TRUE) {
	return gotobob(f, n);
    }
    s = forwchar(TRUE, RegexpLen(exp));
    while (s && (is_at_end_of_line(DOT) || isSpace(CharAtDot()))) {
	s = forwchar(TRUE, 1);
	extra++;
    }
    if (n == 1 && samepoint(savepos, DOT)) {	/* try again */
	if (looped > 10)
	    return FALSE;
	s = backchar(TRUE, RegexpLen(exp) + extra + looped);
	while (s && is_at_end_of_line(DOT)) {
	    if (!empty && is_empty_line(DOT))
		return TRUE;
	    s = backchar(TRUE, 1);
	}
	looped++;
	goto top;

    }
    return TRUE;
}

/*
 * Go to the end of the current sentence.  Like gotobosent(), if we skip into
 * an empty line, return at that point.
 */
int
gotoeosent(int f, int n)
{
    regexp *exp;
    int s;
    int empty = is_empty_line(DOT);

    exp = b_val_rexp(curbp, VAL_SENTENCES)->reg;
    /* if we're on the end of a sentence now, don't bother scanning
       further, or we'll miss the immediately following sentence */
    if (!(lregexec(exp, DOT.l, DOT.o, llength(DOT.l), FALSE) &&
	  exp->startp[0] - lvalue(DOT.l) == DOT.o)) {
	if (findpat(f, n, exp, FORWARD) != TRUE) {
	    DOT = curbp->b_line;
	} else if (empty || !is_at_end_of_line(DOT)) {
	    s = forwchar(TRUE, RegexpLen(exp));
	    while (s && (is_at_end_of_line(DOT) || isSpace(CharAtDot()))) {
		LINE *lp = DOT.l;
		s = forwchar(TRUE, 1);
		if (lp != DOT.l)
		    break;
	    }
	}
    } else {
	s = forwchar(TRUE, RegexpLen(exp));
	while (s && (is_at_end_of_line(DOT) || isSpace(CharAtDot())))
	    s = forwchar(TRUE, 1);
    }
    return TRUE;
}

/*
 * This routine, given a pointer to a LINE, and the current cursor goal
 * column, return the best choice for the offset. The offset is returned.
 * Used by "C-N" and "C-P".
 */
int
getgoal(LINE *dlp)
{
    int col;
    int newcol;
    int dbo;

    col = 0;
    dbo = w_left_margin(curwp);
    while (dbo < llength(dlp)) {
	newcol = next_column(dlp, dbo, col);
	if (newcol > curgoal)
	    break;
	col = newcol;
	dbo += BytesAt(dlp, dbo);
    }
    return (dbo);
}

int
next_sw(int col)
{
    int width = shiftwid_val(curbp);
    return (((col / width) + 1) * width);
}

int
next_tabcol(int col)
{
    int t = tabstop_val(curbp);
    return (((col / t) + 1) * t);
}

#define NonPrintingCols(c) (((c) & HIGHBIT) ? COLS_8BIT : COLS_CTRL)

/*
 * Return the next column index, given the current char and column.
 */
int
next_column(LINE *lp, int off, int col)
{
    int rc = 1;
    int c;

    if (off < llength(lp)) {
	c = lgetc(lp, off);

	if (c == '\t') {
	    rc = next_tabcol(col) - col;
	}
#if OPT_MULTIBYTE
	else if (b_is_utfXX(curbp)) {
	    if (bytes_at(lp, off) > 1) {
		rc = mb_cellwidth(curwp, lvalue(lp) + off, llength(lp) - off);
	    }
	}
#endif
	else if (!isPrint(CharOf(c))) {
	    rc = NonPrintingCols(c);
	}
    } else {
	rc = 0;
    }
    return col + rc;
}

/*
 * Given a char to add, and the current column, return the next column index,
 */
int
column_after(int c, int col, int list)
{
    int rc = (col + 1);

    if (!list && (c == '\t')) {
	rc = next_tabcol(col);
    }
#if OPT_MULTIBYTE
    else if (b_is_utfXX(curbp)) {
	if (vl_conv_to_utf8((UCHAR *) 0, (UINT) c, (B_COUNT) MAX_UTF8) > 1)
	    rc = col + COLS_UTF8;	/* "\uXXXX" */
    }
#endif
    else if (!isPrint(CharOf(c))) {
	rc = col + NonPrintingCols(c);
    }
    return rc;
}

/*
 * Given a pointer to a LINE's text where we have a "nonprinting" character,
 * and the limit on remaining chars to display, return the number of columns
 * which are needed to display it, e.g., in hex or octal.  As a side-effect,
 * set the *used parameter to the number of chars needed for a multibyte
 * character if we have one.
 */
int
column_sizes(WINDOW *wp, const char *text, unsigned limit, int *used)
{
    int rc = NonPrintingCols(CharOf(*text));

    *used = 1;
#if OPT_MULTIBYTE
    if (b_is_utfXX(wp->w_bufp)) {
	*used = vl_conv_to_utf32((UINT *) 0, text, (B_COUNT) limit);
	if (*used > 1) {
	    rc = COLS_UTF8;	/* "\uXXXX" */
	} else if (*used < 1) {
	    *used = 1;		/* probably a broken character... */
	} else if (isPrint(CharOf(*text))) {
	    rc = 1;
	}
    } else
#else
    (void) wp;
    (void) limit;
#endif
    if (isPrint(CharOf(*text))) {
	rc = 1;
    }
    return rc;
}

/*
 * Implements the vi "^F" command.
 *
 * Scroll forward by a specified number of lines, or by a full page if no
 * argument.
 */
int
forwpage(int f, int n)
{
    LINE *lp;
    int status;

    if ((n = full_pages(f, n)) < 0)
	return backpage(f, -n);

    if ((status = (lforw(DOT.l) != buf_head(curbp))) == TRUE) {
	lp = curwp->w_line.l;
	n -= line_height(curwp, lp);
	while (lp != buf_head(curbp)) {
	    lp = lforw(lp);
	    if ((n -= line_height(curwp, lp)) < 0)
		break;
	}
	if (n < 0)
	    curwp->w_line.l = lp;
	DOT.l = lp;
	(void) firstnonwhite(FALSE, 1);
	curwp->w_flag |= WFHARD | WFMODE;
    }
    return status;
}

/*
 * Implements the vi "^B" command.
 *
 * This command is like "forwpage", but it goes backwards.
 */
int
backpage(int f, int n)
{
    LINE *lp;
    int status;

    if ((n = full_pages(f, n)) < 0)
	return forwpage(f, -n);

    lp = curwp->w_line.l;
    if (lback(lp) != buf_head(curbp)) {
	while ((n -= line_height(curwp, lp)) >= 0
	       && lback(lp) != buf_head(curbp))
	    lp = lback(lp);
	curwp->w_line.l = lp;
	(void) gotoeos(FALSE, 1);
	curwp->w_flag |= WFHARD | WFMODE;
	status = TRUE;
    } else if (DOT.l != lp) {
	DOT.l = lp;
	curwp->w_flag |= WFHARD | WFMODE;
	status = TRUE;
    } else {
	status = FALSE;
    }
    return status;
}

/*
 * Implements the vi "^D" command.
 *
 * Scroll forward by a half-page.  If a repeat count is given, interpret that
 * as the number of half-pages to scroll.
 *
 * Unlike vi, the OPT_CVMVAS option causes the repeat-count to be interpreted as
 * half-page, rather than lines.
 */
int
forwhpage(int f, int n)
{
    LINE *llp, *dlp;
    int status;

    if ((n = half_pages(f, n)) < 0)
	return backhpage(f, -n);

    llp = curwp->w_line.l;
    dlp = DOT.l;
    if ((status = (lforw(dlp) != buf_head(curbp))) == TRUE) {
	n -= line_height(curwp, dlp);
	while (lforw(dlp) != buf_head(curbp)) {
	    llp = lforw(llp);
	    dlp = lforw(dlp);
	    if ((n -= line_height(curwp, dlp)) < 0)
		break;
	}
	curwp->w_line.l = llp;
	DOT.l = dlp;
	curwp->w_flag |= WFHARD | WFKILLS;
    }
    (void) firstnonwhite(FALSE, 1);
    return status;
}

/*
 * Implements the vi "^U" command.
 *
 * This command is like "forwpage", but it goes backwards.  It returns false
 * only if the cursor is on the first line of the buffer.
 *
 * Unlike vi, the OPT_CVMVAS option causes the repeat-count to be interpreted as
 * half-pages, rather than lines.
 */
int
backhpage(int f, int n)
{
    LINE *llp, *dlp;
    int status;

    if ((n = half_pages(f, n)) < 0)
	return forwhpage(f, -n);

    llp = curwp->w_line.l;
    dlp = DOT.l;
    if ((status = (lback(dlp) != buf_head(curbp))) == TRUE) {
	n -= line_height(curwp, dlp);
	while (lback(dlp) != buf_head(curbp)) {
	    llp = lback(llp);
	    dlp = lback(dlp);
	    if ((n -= line_height(curwp, dlp)) < 0)
		break;
	}
	curwp->w_line.l = llp;
	DOT.l = dlp;
	curwp->w_flag |= WFHARD | WFINS;
    }
    (void) firstnonwhite(FALSE, 1);
    return status;
}

/*
 * Move forward n rows on the screen, staying in the same column.  It's ok to
 * scroll, too.
 */
int
forw_row(int f, int n)
{
    int code = TRUE;
    int col, next;

    n = need_a_count(f, n, 1);

    if (n < 0) {
	code = back_row(f, -n);
    } else if (n > 0) {

	/* set the "goal" column if necessary */
	if (curgoal < 0)
	    curgoal = getccol(FALSE);

	col = curgoal;
	next = col;

	while ((n-- > 0) && (code == TRUE)) {
	    int save = next;
	    next += term.cols;
	    if (gotocol(TRUE, next + 1) == FALSE) {
		curgoal %= term.cols;
		gotocol(TRUE, save);
		code = forwline(TRUE, 1);
	    } else {
		curgoal = next;
	    }
	}
    }
    return code;
}

/*
 * Move back n rows on the screen, staying in the same column.  It's ok to
 * scroll, too.
 */
int
back_row(int f, int n)
{
    int code = TRUE;
    int col, next;

    n = need_a_count(f, n, 1);

    if (n < 0) {
	code = forw_row(f, -n);
    } else if (n > 0) {

	/* set the "goal" column if necessary */
	if (curgoal < 0)
	    curgoal = getccol(FALSE);

	col = curgoal;
	next = col;

	while ((n-- > 0) && (code == TRUE)) {
	    next -= term.cols;
	    if (next < 0) {
		if ((code = backline(TRUE, 1)) == TRUE
		    && llength(DOT.l) >= curgoal) {
		    next = llength(DOT.l) / term.cols;
		    next = (next * term.cols) + curgoal;
		    curgoal = next;
		}
	    } else {
		if ((code = gotocol(TRUE, next + 1)) == TRUE)
		    curgoal = next;
	    }
	}
    }
    return code;
}

#if NMARKS > 26
static int
nmark2inx(int c)
{
    if (isDigit(c)) {
	return c - '0' + 26;
    } else if (isLower(c)) {
	return c - 'a';
    }
    return -1;
}

static int
inx2nmark(int c)
{
    if (c > 36 || c < 0) {
	return '?';
    } else if (c >= 26) {
	return c - 26 + '0';
    } else {
	return c + 'a';
    }
}
#else
#define nmark2inx(c) ((c) - 'a')
#define inx2nmark(c) ((c) + 'a')
#endif

#if OPT_SHOW_MARKS
static int
show_mark(int count, BUFFER *bp, MARK mark, int name)
{
    static int tab = 8;		/* no b_val(bp,VAL_TAB) -- set_rdonly uses 8 */
    static int stop;

    if (!samepoint(mark, nullmark)) {

	if (!count) {
	    bprintf("\nMark  Line     Column");
	    if (stop == 0) {
		stop = ((DOT.o + tab - 1) / tab) * tab;
	    }
	    bpadc(' ', stop - DOT.o);
	    bprintf("Text");
	    bprintf("\n----  -------- ------");
	    bpadc(' ', stop - DOT.o);
	    bprintf("----");
	}

	bprintf("\n%c     %8d %8d",
		name,
		line_no(bp, mark.l),
		mk_to_vcol(curwp, mark, FALSE, 0, 0) + 1);
	if (llength(mark.l) > 0) {
	    bpadc(' ', stop - DOT.o);
	    bputsn(lvalue(mark.l), llength(mark.l));
	}
	return 1;
    }
    return 0;
}

static void
makemarkslist(int value GCC_UNUSED, void *dummy GCC_UNUSED)
{
    BUFFER *bp = (BUFFER *) dummy;
    WINDOW *wp;
    int n;
    int done = 0;

    bprintf("Named marks for %s\n", bp->b_bname);
    set_b_val(curbp, VAL_TAB, b_val(bp, VAL_TAB));

    for_each_window(wp) {
	if (wp->w_bufp == bp) {
	    done += show_mark(done, bp, wp->w_lastdot, SQUOTE);
	}
    }
    if (bp->b_nmmarks != NULL) {
	for (n = 0; n < NMARKS; n++) {
	    done += show_mark(done, bp, bp->b_nmmarks[n], inx2nmark(n));
	}
    }
#if OPT_SELECTIONS
    if (bp == sel_buffer()) {
	MARK left, right;
	if (sel_get_leftmark(&left)
	    && sel_get_rightmark(&right)
	    && (left.l != right.l
		|| left.o != right.o)) {
	    done += show_mark(done, bp, left, '<');
	    done += show_mark(done, bp, right, '>');
	}
    }
#endif
    if (done)
	bprintf("\n\n%d mark%s", done, PLURAL(done));
    else
	bprintf("\n(none)");
}

int
showmarks(int f, int n GCC_UNUSED)
{
    WINDOW *wp = curwp;
    int s;

    TRACE((T_CALLED "showmarks(f=%d)\n", f));

    s = liststuff(MARKS_BufName, FALSE, makemarkslist, f, (void *) curbp);
    /* back to the buffer whose modes we just listed */
    if (swbuffer(wp->w_bufp))
	curwp = wp;

    returnCode(s);
}

#if OPT_UPBUFF
/* ARGSUSED */
static int
update_marklist(BUFFER *bp GCC_UNUSED)
{
    return showmarks(FALSE, 1);
}
#endif
#endif /* OPT_SHOW_MARKS */

static int
invalid_nmmark(void)
{
    mlforce("[Invalid mark name]");
    return FALSE;
}

int
can_set_nmmark(int c)
{
    if (isLower(c) || c == SQUOTE)
	return TRUE;
#if !SMALLER
    if (isDigit(c))
	return TRUE;
#endif
#if OPT_SELECTIONS
    if (c == '<' || c == '>')
	return TRUE;
#endif
    return FALSE;
}

static int
get_nmmark(int c, MARK *markp)
{
    int result = TRUE;

    *markp = nullmark;
    if (c == SQUOTE) {		/* use the 'last dot' mark */
	*markp = curwp->w_lastdot;
	if (markp->l == NULL) {
	    markp->l = lforw(buf_head(curbp));
	    markp->o = 0;
	}
    } else {
#if OPT_SELECTIONS
	if (c == '<') {
	    result = sel_get_leftmark(markp);
	} else if (c == '>') {
	    result = sel_get_rightmark(markp);
	} else
#endif
	if (curbp->b_nmmarks != NULL) {
	    *markp = curbp->b_nmmarks[nmark2inx(c)];
	} else {
	    result = FALSE;
	}
    }
    TRACE(("get_nmmark(%c) ->%d.%d\n",
	   c, result ? line_no(curbp, markp->l) : 0, markp->o));
    return result;
}

int
show_mark_is_set(int c)
{
    int rc = TRUE;
#if OPT_SHOW_MARKS
    int ignore = (curbp != NULL && eql_bname(curbp, MARKS_BufName));
    if (isLower(c)) {
	mlwrite("[Mark '%c' %s]", c, ignore ? "ignored" : "set");
    }
    if (ignore) {
	rc = FALSE;
    } else {
	update_scratch(MARKS_BufName, update_marklist);
    }
#else
    (void) c;
#endif
    return rc;
}

/*
 * Implements the vi "m" command.
 *
 * Set the named mark in the current window to the value of "." in the window.
 */
/* ARGSUSED */
int
setnmmark(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int c, i;

    if (clexec || isnamedcmd) {
	int status;
	static char cbuf[2];
	if ((status = mlreply("Set mark: ", cbuf, 2)) != TRUE)
	    return status;
	c = cbuf[0];
    } else {
	c = keystroke();
	if (ABORTED(c))
	    return ABORT;
    }

    if (nmark2inx(c) < 0) {
	return invalid_nmmark();
    }

    if (curbp->b_nmmarks == NULL) {

	beginDisplay();
	curbp->b_nmmarks = typeallocn(struct MARK, NMARKS);
	endofDisplay();

	if (curbp->b_nmmarks == NULL)
	    return no_memory("named-marks");
	for (i = 0; i < NMARKS; i++) {
	    curbp->b_nmmarks[i] = nullmark;
	}
    }

    curbp->b_nmmarks[nmark2inx(c)] = DOT;
    return show_mark_is_set(c);
}

/*
 * Get the name of the mark to use.  interactively, "last dot" is represented
 * by stuttering the goto-mark command.  From the command line, it's always
 * named ' or `.  I suppose this is questionable.
 */
static int
getnmmarkname(int *cp)
{
    int c;
    int thiskey;
    int useldmark;

    if (clexec || isnamedcmd) {
	int status;
	static char cbuf[2];
	if ((status = mlreply("Goto mark: ", cbuf, 2)) != TRUE)
	    return status;
	c = cbuf[0];
	useldmark = (c == SQUOTE || c == BQUOTE);
    } else {
	thiskey = lastkey;
	c = keystroke();
	if (ABORTED(c))
	    return ABORT;
	useldmark = (lastkey == thiskey);	/* usually '' or `` */
    }

    if (useldmark)
	c = SQUOTE;

    *cp = c;
    TRACE(("getnmmarkname(%c)\n", c));
    return TRUE;
}

/* ARGSUSED */
int
golinenmmark(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int c;
    int s;

    s = getnmmarkname(&c);
    if (s != TRUE)
	return s;
    s = gonmmark(c);
    if (s != TRUE)
	return s;

    return firstnonwhite(FALSE, 1);
}

/* ARGSUSED */
int
goexactnmmark(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int c;
    int s;

    s = getnmmarkname(&c);
    if (s != TRUE)
	return s;

    return gonmmark(c);
}

/* ARGSUSED */
int
gorectnmmark(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int c;
    int s;

    s = getnmmarkname(&c);
    if (s != TRUE)
	return s;

    regionshape = rgn_RECTANGLE;
    return gonmmark(c);
}

int
gonmmark(int c)
{
    MARK my_mark;
    MARK tmark;

    TRACE(("gonmmark(%c)\n", c));

    if (!can_set_nmmark(c)) {
	return invalid_nmmark();
    }

    /* save current dot */
    tmark = DOT;

    /* if we have any named marks, and the one we want isn't null */
    if (!get_nmmark(c, &my_mark) || !restore_dot(my_mark)) {
	mlforce("[Mark not set]");
	return (FALSE);
    }

    if (!doingopcmd)		/* reset last-dot-mark to old dot */
	curwp->w_lastdot = tmark;

    show_mark_is_set(0);
    return (TRUE);
}

/*
 * Set the mark in the current window to the value of "." in the window. No
 * errors are possible.
 */
int
setmark(void)
{
    MK = DOT;
    return (TRUE);
}

/* ARGSUSED */
int
gomark(int f GCC_UNUSED, int n GCC_UNUSED)
{
    DOT = MK;
    curwp->w_flag |= WFMOVE;
    return (TRUE);
}

/* this odd routine puts us at the internal mark, plus an offset of lines */
/*  n == 1 leaves us at mark, n == 2 one line down, etc. */
/*  this is for the use of stuttered commands, and line oriented regions */
int
godotplus(int f, int n)
{
    int s;

    if (!f || n == 1) {
	s = firstnonwhite(FALSE, 1);
    } else if (n < 1) {
	s = FALSE;
    } else {
	s = forwline(TRUE, n - 1);
	if (s && is_header_line(DOT, curbp)) {
	    s = backline(FALSE, 1);
	}
	if (s == TRUE) {
	    (void) firstnonwhite(FALSE, 1);
	}
    }
    return s;
}

/*
 * Swap the values of "." and "mark" in the current window. This is pretty
 * easy, because all of the hard work gets done by the standard routine
 * that moves the mark about. The only possible error is "no mark".
 */
void
swapmark(void)
{
    MARK odot;

    if (samepoint(MK, nullmark)) {
	mlforce("BUG: No mark ");
    } else {
	odot = DOT;
	DOT = MK;
	MK = odot;
	curwp->w_flag |= WFMOVE;
    }
    return;
}

#if OPT_MOUSE
/*
 * Given row & column from the screen, set the MK value.
 * The resulting position will not be past end-of-buffer unless the buffer
 * is empty.
 */
int
setwmark(int row, int col)
{
    MARK save;
    LINE *dlp;

    save = DOT;
    if (row == mode_row(curwp)) {
	(void) gotoeos(FALSE, 1);
	DOT.l = lforw(DOT.l);
	DOT.o = w_left_margin(curwp);
    } else {			/* move to the right row */
	row -= curwp->w_toprow + curwp->w_line.o;
	/*
	 * In the statement above, wp->w_line.o will
	 * normally be zero.  But when the top line of the
	 * window is wrapped and the beginning of the line
	 * is not visible, it will have the number of
	 * lines that would appear before the top line
	 * negated.  (E.g, if the wrapped line occupied
	 * 2 lines before the top window line, then wp->w_line.o
	 * will have the value -2.)
	 */

	dlp = curwp->w_line.l;	/* get pointer to 1st line */
	while ((row -= line_height(curwp, dlp)) >= 0
	       && dlp != buf_head(curbp))
	    dlp = lforw(dlp);
	DOT.l = dlp;		/* set dot line pointer */

	/* now move the dot over until near the requested column */
#ifdef WMDLINEWRAP
	if (w_val(curwp, WMDLINEWRAP))
	    col += term.cols * (row + line_height(curwp, dlp));
#endif
	DOT.o = col2offs(curwp, dlp, col);

#ifdef OPT_MOUSE_NEWLINE
	/* don't allow the cursor to be set past end of line unless we
	 * are in insert mode
	 */
	if (DOT.o >= llength(dlp) && DOT.o > w_left_margin(curwp) &&
	    !insertmode) {
	    DOT.o -= BytesBefore(DOT.l, DOT.o);
	}
#endif
    }
    if (is_header_line(DOT, curwp->w_bufp)) {
	DOT.l = lback(DOT.l);
	DOT.o = llength(DOT.l);
    }
    MK = DOT;
    DOT = save;

    TRACE(("setwmark -> line %d, col %d\n", line_no(curwp->w_bufp, MK.l), MK.o));
    return TRUE;
}

/*
 * Given row & column from the screen, set the curwp and DOT values.
 */
int
setcursor(int row, int col)
{
    int code = FALSE;
    WINDOW *wp0 = curwp;
    WINDOW *wp1;
    MARK saveMK;

    TRACE((T_CALLED "setcursor(%d,%d)\n", row, col));
    if ((wp1 = row2window(row)) != NULL) {
	if (!(doingsweep && curwp != wp1)) {
	    saveMK = MK;
	    if (set_curwp(wp1)
		&& setwmark(row, col)) {
		if (insertmode != FALSE
		    && b_val(wp1->w_bufp, MDVIEW)
		    && b_val(wp1->w_bufp, MDSHOWMODE)) {
		    if (b_val(wp0->w_bufp, MDSHOWMODE))
			wp0->w_flag |= WFMODE;
		    if (b_val(wp1->w_bufp, MDSHOWMODE))
			wp1->w_flag |= WFMODE;
		    insertmode = FALSE;
		}
		DOT = MK;
		if (wp0 == wp1)
		    MK = saveMK;
		curwp->w_flag |= WFMOVE;
		code = TRUE;
	    }
	}
    }
    returnCode(code);
}
#endif
