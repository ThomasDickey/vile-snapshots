/*
 * The routines in this file implement commands that work word or a
 * paragraph at a time.  There are all sorts of word mode commands.  If I
 * do any sentence mode commands, they are likely to be put in this file.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/word.c,v 1.81 2006/11/02 01:50:12 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"
#include	"nefunc.h"

/* Word wrap on n-spaces. Back-over whatever precedes the point on the current
 * line and stop on the first word-break or the beginning of the line. If we
 * reach the beginning of the line, jump back to the end of the word and start
 * a new line.	Otherwise, break the line at the word-break, eat it, and jump
 * back to the end of the word.
 * Returns TRUE on success, FALSE on errors.
 */
int
wrapword(int f, int n)
{
    int cnt = 0;		/* size of word wrapped to next line */
    int c;			/* character temporary */
    B_COUNT to_delete;
    C_NUM to_append = 0;

    /* Back up from the <NL> 1 char */
    if (!backchar(FALSE, 1))
	return (FALSE);

    /* If parameter given, delete the trailing white space.  This is done
     * to support a vi-like wrapmargin mode (see insert.c).  Unlike vi,
     * we're deleting all of the trailing whitespace; vi deletes only the
     * portion that was added during the current insertion.
     *
     * If there's no trailing whitespace, and we're not inserting blanks
     * (n!=0), try to split the line at the last embedded whitespace.
     */
    if (f) {
	LINE *lp = DOT.l;
	to_delete = 0L;
	if (DOT.o >= 0 && !n && !isSpace(lgetc(lp, DOT.o))) {
	    for (c = DOT.o; c >= 0; c--) {
		if (isSpace(lgetc(lp, c))) {
		    to_append = n;
		    cnt = (DOT.o - c);
		    DOT.o = c;
		    break;
		}
	    }
	}
	for (c = DOT.o; c >= 0; c--) {
	    if (isSpace(lgetc(lp, c))) {
		to_delete++;
		DOT.o = c;
	    } else {
		break;
	    }
	}
	if (to_delete == 0)
	    DOT.o++;
    } else {
	/* Back up until we aren't in a word, make sure there is a
	 * break in the line
	 */
	while (c = char_at(DOT), !isSpace(c)) {
	    cnt++;
	    if (!backchar(FALSE, 1))
		return (FALSE);
	    /* if we make it to the beginning, start a new line */
	    if (DOT.o == 0) {
		(void) gotoeol(FALSE, 0);
		return (lnewline());
	    }
	}
	to_delete = 1L;
    }

    /* delete the forward white space */
    if (!ldelete(to_delete, FALSE))
	return (FALSE);

    /* put in newline */
    if (!newline(FALSE, 1))
	return FALSE;

    /* and past the first word */
    while (cnt-- > 0) {
	if (forwchar(FALSE, 1) == FALSE)
	    return (FALSE);
    }
    if (to_append > 0)
	linsert(to_append, ' ');
    return (TRUE);
}

/*
 * Implements the vi "w" command.
 *
 * Move the cursor forward by the specified number of words. All of the motion
 * is done by "forwchar". Error if you try and move beyond the buffer's end.
 *
 * Returns of SORTOFTRUE result if we're doing a non-delete operation.
 * Whitespace after a word is always included on deletes (and non-operations,
 * of course), but only on intermediate words for other operations, for
 * example.  The last word of non-delete ops does _not_ include its whitespace.
 */
int
forwviword(int f, int n)
{
    int s;

    if (n < 0)
	return (backword(f, -n));
    setchartype();
    if (forwchar(TRUE, 1) == FALSE)
	return (FALSE);
    while (n--) {
	int any = 0;
	while (((s = isnewviwordf()) == FALSE) ||
	       (s == SORTOFTRUE && n != 0)) {
	    if (forwchar(TRUE, 1) == FALSE)
		return (any != 0);
	    any++;
	}
    }
    return TRUE;
}

/*
 * Implements the vi "W" command.
 *
 * Move the cursor forward by the specified number of words. All of the motion
 * is done by "forwchar". Error if you try and move beyond the buffer's end.
 */
int
forwword(int f, int n)
{
    int s;

    if (n < 0)
	return (backword(f, -n));
    setchartype();
    if (forwchar(TRUE, 1) == FALSE)
	return (FALSE);
    while (n--) {
	int any = 0;
	while (((s = isnewwordf()) == FALSE) ||
	       (s == SORTOFTRUE && n != 0)) {
	    if (forwchar(TRUE, 1) == FALSE)
		return (any != 0);
	    any++;
	}
    }
    return (TRUE);
}

/*
 * Implements the vi "e" command.
 *
 * Move the cursor forward by the specified number of words. All of the motion
 * is done by "forwchar". Error if you try and move beyond the buffer's end.
 */
int
forwviendw(int f, int n)
{
    int s = FALSE;

    n = need_a_count(f, n, 1);

    if (n <= 0)
	return (FALSE);
    if (forwchar(TRUE, 1) == FALSE)
	return (FALSE);
    setchartype();
    while (n--) {
	int any = 0;
	while ((s = isendviwordf()) == FALSE) {
	    if (forwchar(TRUE, 1) == FALSE)
		return (any != 0);
	    any++;
	}

    }
    if (s == SORTOFTRUE)
	return TRUE;
    else
	return backchar(FALSE, 1);
}

/*
 * Implements the vi "E" command.
 *
 * Move the cursor forward by the specified number of words. All of the motion
 * is done by "forwchar". Error if you try and move beyond the buffer's end.
 */
int
forwendw(int f, int n)
{
    int s = FALSE;

    n = need_a_count(f, n, 1);

    if (n <= 0)
	return (FALSE);
    if (forwchar(TRUE, 1) == FALSE)
	return (FALSE);
    setchartype();
    while (n--) {
	int any = 0;
	while ((s = isendwordf()) == FALSE) {
	    if (forwchar(TRUE, 1) == FALSE)
		return (any != 0);
	    any++;
	}

    }
    if (s == SORTOFTRUE)
	return TRUE;
    else
	return backchar(FALSE, 1);
}

/*
 * Implements the vi "b" command.
 *
 * Move the cursor backward by "n" words. All of the details of motion are
 * performed by the "backchar" and "forwchar" routines. Error if you try to
 * move beyond the buffers.
 */
int
backviword(int f, int n)
{
    if (n < 0)
	return (forwword(f, -n));
    if (backchar(FALSE, 1) == FALSE)
	return (FALSE);
    setchartype();
    while (n--) {
	int any = 0;
	while (isnewviwordb() == FALSE) {
	    any++;
	    if (backchar(FALSE, 1) == FALSE)
		return (any != 0);
	}
    }
    return (forwchar(TRUE, 1));
}

/*
 * Implements the vi "B" command.
 *
 * Move the cursor backward by "n" words. All of the details of motion are
 * performed by the "backchar" and "forwchar" routines. Error if you try to
 * move beyond the buffers.
 */
int
backword(int f, int n)
{
    if (n < 0)
	return (forwword(f, -n));
    if (backchar(FALSE, 1) == FALSE)
	return (FALSE);
    setchartype();
    while (n--) {
	int any = 0;
	while (isnewwordb() == FALSE) {
	    any++;
	    if (backchar(FALSE, 1) == FALSE)
		return (any != 0);
	}
    }
    return (forwchar(TRUE, 1));
}

/*
 * If not 'exact', will adjust whitespace at the join-position.
 */
static int
join_region(int exact)
{
    int status;
    int doto, c;
    LINE *end;
    REGION region;
    int done = FALSE;

    if ((status = getregion(&region)) == TRUE
	&& (status = !is_last_line(region.r_orig, curbp)) == TRUE) {

	DOT = region.r_orig;
	end = region.r_end.l;
	if (DOT.l == end) {
	    /* Degenerate case -- nothing to join */

	    return (TRUE);
	}
	regionshape = EXACT;

	while (!done && status == TRUE) {
	    c = EOS;
	    (void) gotoeol(FALSE, 1);
	    if (DOT.o > 0)
		c = lgetc(DOT.l, DOT.o - 1);
	    (void) setmark();
	    if (forwline(FALSE, 1) != TRUE)
		break;
	    if (exact) {
		(void) gotobol(FALSE, 1);
	    } else {
		(void) firstnonwhite(FALSE, 1);
	    }

	    done = ((DOT.l == end) || (lforw(DOT.l) == end));
	    if (killregionmaybesave(FALSE) != TRUE)
		break;

	    doto = DOT.o;
	    if (doto == 0 || exact) {
		/*EMPTY */ ;
	    }
	    /* join at column 0 to empty line */
	    else if (doto < llength(DOT.l)) {
		if (lgetc(DOT.l, doto) == ')') {
		    /*EMPTY */ ;
		}
		/* join after parentheses */
		else if (lgetc(DOT.l, doto - 1) == '.') {
		    status = linsert(2, ' ');
		} else if (!isSpace(c)) {
		    status = linsert(1, ' ');
		}
	    }
	}
    }
    return status;
}

int
joinregion(void)
{
    return join_region(FALSE);
}

int
joinregion_x(void)
{
    return join_region(TRUE);
}

int
joinlines(int f, int n)
{
    havemotion = &f_godotplus;
    return (operjoin(f, n));
}

#if OPT_FORMAT
static int
dot_at_section_break(void)
{
    regexp *expP = b_val_rexp(curbp, VAL_PARAGRAPHS)->reg;
    regexp *expC = b_val_rexp(curbp, VAL_COMMENTS)->reg;

    return (lregexec(expP, DOT.l, 0, llength(DOT.l)) ||
	    lregexec(expC, DOT.l, 0, llength(DOT.l)));
}

/* returns the length of the comment-prefix, if it matches, otherwise -1 */
static int
comment_prefix(void)
{
    regexp *expP = b_val_rexp(curbp, VAL_CMT_PREFIX)->reg;
    int result = -1;
    if (lregexec(expP, DOT.l, 0, llength(DOT.l))) {
	result = (int) (expP->endp[0] - DOT.l->l_text);
    }
    return result;
}

#define cplus_comment_start(c) \
		((c == '/') \
		&& DOT.o+1 < llength(DOT.l) \
		&& lgetc(DOT.l,DOT.o+1) == '/')

#define c_comment_start(c) \
		((c == '/') \
		&& DOT.o+1 < llength(DOT.l) \
		&& lgetc(DOT.l,DOT.o+1) == '*')

#define fmt_insert(count,chr) \
		if ((s = linsert(count,chr)) != TRUE) \
			return s; \
		else \
			clength += count

#define fmt_c_preprocessor(cp) \
		(tb_length(*cp) == 1 \
		&& *tb_values(*cp) == '#' \
		&& is_c_mode(curbp))

#define word_finishes_c_comment(wp) \
		(tb_length(*wp) >= 2 \
		&& memcmp("/*", tb_values(*wp), 2) == 0)

static int
do_formatting(TBUFF **wp, TBUFF **cp)
{
    int c;			/* current char during scan     */
    int clength;		/* position on line during fill */
    int plength;		/* prefix to ignore during fill */
    size_t i;			/* index during word copy       */
    int newlen;			/* tentative new line length    */
    int finished;		/* Are we at the End-Of-Paragraph? */
    int firstflag;		/* first word? (needs no space) */
    int is_comment;		/* doing a comment block?       */
    int at_nl = TRUE;		/* just saw a newline?          */
    LINE *pastline;		/* pointer to line just past EOP */
    int sentence;		/* was the last char a period?  */
    int secondindent;
    int s;
    int fillcolumn = getfillcol(curbp);

    if (!sameline(MK, DOT)) {
	REGION region;

	if (getregion(&region) != TRUE)
	    return FALSE;
	if (sameline(region.r_orig, MK))
	    swapmark();
    }
    pastline = MK.l;
    if (pastline != buf_head(curbp))
	pastline = lforw(pastline);

    finished = FALSE;
    while (finished != TRUE) {	/* i.e. is FALSE or SORTOFTRUE */
	if (DOT.l == pastline)	/* FIXME */
	    return setmark();
	while (dot_at_section_break()) {
	    DOT.l = lforw(DOT.l);
	    if (DOT.l == pastline)
		return setmark();
	}

	secondindent = indentlen(DOT.l);

	/* go forward to get the indent for the second
	   and following lines */
	DOT.l = lforw(DOT.l);

	if (DOT.l != pastline) {
	    secondindent = indentlen(DOT.l);
	}

	/* and back where we should be */
	DOT.l = lback(DOT.l);
	(void) firstnonwhite(FALSE, 1);

	clength = indentlen(DOT.l);
	tb_init(wp, EOS);
	tb_init(cp, EOS);
	sentence = FALSE;

	c = char_at(DOT);
	is_comment = FALSE;
	if ((plength = comment_prefix()) >= 0) {
	    is_comment = TRUE;
	    tb_bappend(cp,
		       DOT.l->l_text + DOT.o,
		       (size_t) (plength - DOT.o));
	} else if (cplus_comment_start(c)) {
	    is_comment = TRUE;
	    tb_bappend(cp, "//", 2);
	} else if (c_comment_start(c)) {
	    is_comment = TRUE;
	    tb_bappend(cp, "*", 1);
	}

	/* scan through lines, filling words */
	firstflag = TRUE;
	finished = FALSE;
	plength -= DOT.o;
	while (finished == FALSE) {	/* i.e. is not TRUE  */
	    /* or SORTOFTRUE */
	    if (interrupted())
		return ABORT;

	    /* get the next character */
	    if (is_at_end_of_line(DOT)) {
		c = ' ';
		DOT.l = lforw(DOT.l);
		if (DOT.l == pastline) {
		    finished = TRUE;
		} else if (dot_at_section_break()) {
		    finished = SORTOFTRUE;
		}

		if ((s = comment_prefix()) >= 0) {
		    int save = DOT.o;

		    (void) firstnonwhite(FALSE, 1);
		    s -= DOT.o;

		    if (s != (int) tb_length(*cp)
			|| (s > 0
			    && memcmp(tb_values(*cp),
				      DOT.l->l_text + DOT.o,
				      s))) {
			finished = SORTOFTRUE;
		    }

		    if (finished == FALSE) {
			plength = s;
			tb_init(cp, EOS);
			if (plength > 0) {
			    tb_bappend(cp,
				       DOT.l->l_text + DOT.o,
				       (size_t) (plength));
			}
			if (DOT.l != pastline
			    && !dot_at_section_break()) {
			    B_COUNT spcs = DOT.o;
			    DOT.o = 0;
			    s = ldelete(spcs, FALSE);
			    if (s != TRUE)
				return s;
			}
		    }
		    DOT.o = save;
		}
		DOT.l = lback(DOT.l);
		at_nl = TRUE;
	    } else {
		c = char_at(DOT);
		if (at_nl && ((plength-- > 0) || isSpace(c)))
		    c = ' ';
		else
		    at_nl = FALSE;
	    }

	    /* and then delete it */
	    if (finished == FALSE) {
		s = ldelete(1L, FALSE);
		if (s != TRUE)
		    return s;
	    }

	    /* if not a separator, just add it in */
	    if (!isBlank(c)) {
		tb_append(wp, c);
		sentence = FALSE;
		if (global_g_val(GMDSPACESENT)) {
		    /* was it the end of a "sentence"? */
		    if (c == '?' || c == ':' || c == '!') {
			sentence = TRUE;
		    } else if (c == '.') {
			size_t len = tb_length(*wp);
			if (len == 4) {
			    char *ptr = tb_values(*wp);
			    /*
			     * These "never" end a sentence.  Some Europeans
			     * drop the comma which Americans add, which makes
			     * formatting sentences a little odd.  I would
			     * add "etc.", but that is a valid sentence ending,
			     * and would be unclear without a comma otherwise.
			     */
			    sentence = memcmp(ptr, "e.g.", 4)
				&& memcmp(ptr, "i.e.", 4);
			} else {
			    sentence = TRUE;
			}
		    }
		}
	    } else if (tb_length(*wp)) {
		/* at a word break with a word waiting */
		/* calculate tentative new length
		   with word added */
		newlen = (int) (clength + 1 + tb_length(*wp));
		if (newlen <= fillcolumn) {
		    /* add word to current line */
		    if (!firstflag) {
			fmt_insert(1, ' ');
		    }
		} else {
		    /* fix the leading indent now, if
		       some spaces should be tabs */
		    if (b_val(curbp, MDTABINSERT))
			entabline((void *) TRUE, 0, 0);
		    if (lnewline() == FALSE)
			return FALSE;
		    clength = 0;
		    fmt_insert(secondindent, ' ');
		    firstflag = TRUE;
		}

		if (firstflag
		    && is_comment
		    && !word_finishes_c_comment(wp)) {
		    for (i = 0; i < tb_length(*cp); i++) {
			fmt_insert(1, tb_values(*cp)[i]);
		    }
		    if (!fmt_c_preprocessor(cp)) {
			fmt_insert(1, ' ');
		    }
		}
		firstflag = FALSE;

		/* and add the word in in either case */
		for (i = 0; i < tb_length(*wp); i++) {
		    fmt_insert(1, tb_values(*wp)[i]);
		}
		if (finished == FALSE && sentence) {
		    fmt_insert(1, ' ');
		}
		tb_init(wp, EOS);
	    }
	}
	/* catch the case where we're done with a line not because
	   there's no more room, but because we're done processing a
	   section or the region */
	if (b_val(curbp, MDTABINSERT))
	    entabline((void *) TRUE, 0, 0);
	DOT.l = lforw(DOT.l);
    }
    return setmark();
}

int
formatregion(void)
{
    int s = FALSE;
    TBUFF *wp = 0;		/* word buffer */
    TBUFF *cp = 0;		/* comment-prefix buffer */

    if ((wp = tb_init(&wp, EOS)) != 0
	&& (wp = tb_init(&wp, EOS)) != 0) {
	s = do_formatting(&wp, &cp);
	tb_free(&wp);
	tb_free(&cp);
    }
    return s;
}
#endif /* OPT_FORMAT */

#if OPT_WORDCOUNT
/*	wordcount:	count the # of words in the marked region,
			along with average word sizes, # of chars, etc,
			and report on them.			*/
/*ARGSUSED*/
int
wordcount(int f GCC_UNUSED, int n GCC_UNUSED)
{
    LINE *lp;			/* current line to scan */
    int offset;			/* current char to scan */
    long size;			/* size of region left to count */
    int ch;			/* current character to scan */
    int wordflag;		/* are we in a word now? */
    int lastflag;		/* were we just in a word? */
    long nwhite;		/* total # of blanks */
    long nwords;		/* total # of words */
    long nchars;		/* total number of chars */
    int nlines;			/* total number of lines in region */
    double avgch;		/* average number of chars/word */
    int status;			/* status return code */
    REGION region;		/* region to look at */
    const char *ending = get_record_sep(curbp);
    int len_rs = len_record_sep(curbp);

    /* make sure we have a region to count */
    if ((status = getregion(&region)) != TRUE) {
	return (status);
    }
    lp = region.r_orig.l;
    offset = region.r_orig.o;
    size = region.r_size;

    /* count up things */
    lastflag = FALSE;
    nwhite = 0L;
    nchars = 0L;
    nwords = 0L;
    nlines = 0;
    while (size--) {

	/* get the current character */
	if (offset == llength(lp)) {	/* end of line */
	    ch = *ending;
	    lp = lforw(lp);
	    offset = 0;
	    ++nlines;
	    if (len_rs > 1) {
		nwhite += (len_rs - 1);
		size -= (len_rs - 1);
	    }
	} else {
	    ch = lgetc(lp, offset);
	    ++offset;
	}

	/* and tabulate it */
	if (((wordflag = isident(ch)) != 0) && !lastflag)
	    ++nwords;
	lastflag = wordflag;
	if (isSpace(ch))
	    ++nwhite;
	else
	    ++nchars;
    }

    /* and report on the info */
    if (nwords > 0L)
	avgch = ((1.0 * nchars) / nwords);
    else
	avgch = 0.0;

    mlforce("lines %d, words %ld, chars %ld, avg chars/word %f",
	    nlines, nwords, nchars + nwhite, avgch);

    return (TRUE);
}
#endif /* OPT_WORDCOUNT */
