/*
 * The routines in this file implement commands that work word or a
 * paragraph at a time.  There are all sorts of word mode commands.  If I
 * do any sentence mode commands, they are likely to be put in this file. 
 *
 * $Header: /users/source/archives/vile.vcs/RCS/word.c,v 1.54 1996/10/03 01:02:51 tom Exp $
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
	register int cnt = 0;	/* size of word wrapped to next line */
	register int c;		/* character temporary */
	B_COUNT	to_delete;
	C_NUM to_append = 0;

	/* Back up from the <NL> 1 char */
	if (!backchar(FALSE, 1))
		return(FALSE);

	/* If parameter given, delete the trailing white space.  This is done
	 * to support a vi-like wrapmargin mode (see insert.c).  Unlike vi,
	 * we're deleting all of the trailing whitespace; vi deletes only the
	 * portion that was added during the current insertion.
	 *
	 * If there's no trailing whitespace, and we're not inserting blanks
	 * (n!=0), try to split the line at the last embedded whitespace.
	 */
	if (f) {
		register LINE *lp = DOT.l;
		to_delete = 0L;
		if (DOT.o >= 0 && !n && !isspace(lgetc(lp,DOT.o))) {
			for (c = DOT.o; c >= 0; c--) {
				if (isspace(lgetc(lp,c))) {
					to_append = n;
					cnt = (DOT.o - c);
					DOT.o = c;
					break;
				}
			}
		}
		for (c = DOT.o; c >= 0; c--) {
			if (isspace(lgetc(lp,c))) {
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
		while (c = char_at(DOT), !isspace(c)) {
			cnt++;
			if (!backchar(FALSE, 1))
				return(FALSE);
			/* if we make it to the beginning, start a new line */
			if (DOT.o == 0) {
				(void)gotoeol(FALSE, 0);
				return(lnewline());
			}
		}
		to_delete = 1L;
	}

	/* delete the forward white space */
	if (!ldelete(to_delete, FALSE))
		return(FALSE);

	/* put in a end of line */
	if (!newline(FALSE,1))
		return FALSE;

	/* and past the first word */
	while (cnt-- > 0) {
		if (forwchar(FALSE, 1) == FALSE)
			return(FALSE);
	}
	if (to_append > 0)
		linsert(to_append, ' ');
	return(TRUE);
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
	return(TRUE);
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
	if (!f)
		n = 1;
	else if (n <= 0)
		return (FALSE);
	if (forwchar(TRUE, 1) == FALSE)
		return (FALSE);
	setchartype();
	while (n--) {
		int	any = 0;
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
	if (!f)
		n = 1;
	else if (n <= 0)
		return (FALSE);
	if (forwchar(TRUE, 1) == FALSE)
		return (FALSE);
	setchartype();
	while (n--) {
		int	any = 0;
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
		int	any = 0;
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
		int	any = 0;
		while (isnewwordb() == FALSE) {
			any++;
			if (backchar(FALSE, 1) == FALSE)
				return (any != 0);
		}
	}
	return (forwchar(TRUE, 1));
}

int
joinregion(void)
{
	register int status;
	register int doto, c;
	LINE	*end;
	REGION	region;
	int	done = FALSE;

	if ((status = getregion(&region)) == TRUE
	 && (status = !is_last_line(region.r_orig, curbp)) == TRUE) {

		DOT = region.r_orig;
		end = region.r_end.l;
		regionshape = EXACT;

		while (!done && status == TRUE) {
			c = EOS;
			(void)gotoeol(FALSE,1);
			if (DOT.o > 0)
				c = lgetc(DOT.l, DOT.o-1);
			(void)setmark();
			if (forwline(FALSE,1) != TRUE)
				break;
			(void)firstnonwhite(FALSE,1);

			done = ((DOT.l == end) || (lforw(DOT.l) == end));
			if (killregionmaybesave(FALSE) != TRUE)
				break;

			doto = DOT.o;
			if (doto == 0)
				/*EMPTY*/; /* join at column 0 to empty line */
			else if (doto < llength(DOT.l)) {
				if (lgetc(DOT.l, doto) == ')')
					/*EMPTY*/; /* join after parentheses */
				else if (lgetc(DOT.l, doto-1) == '.')
					status = linsert(2,' ');
				else if (!isspace(c))
					status = linsert(1,' ');
			}
		}
	}
	return status;
}

int
joinlines(int f, int n)
{
	havemotion = &f_godotplus;
	return(operjoin(f,n));
}

#if OPT_FORMAT
int
formatregion(void)
{
	register int c;			/* current char during scan	*/
	register int wordlen;		/* length of current word	*/
	register int clength;		/* position on line during fill	*/
	register int i;			/* index during word copy	*/
	register int newlen;		/* tentative new line length	*/
	register int finished;		/* Are we at the End-Of-Paragraph? */
	register int firstflag;		/* first word? (needs no space)	*/
	register int is_comment;	/* doing a comment block?	*/
	register int comment_char = -1;	/* # or *, for shell or C	*/
	register int at_nl = TRUE;	/* just saw a newline?		*/
	register LINEPTR pastline;	/* pointer to line just past EOP */
	register int sentence;		/* was the last char a period?	*/
	char wbuf[NSTRING];		/* buffer for current word	*/
	int secondindent;
	regexp *expP, *expC;
	int s;
	
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

	expP = b_val_rexp(curbp,VAL_PARAGRAPHS)->reg;
	expC = b_val_rexp(curbp,VAL_COMMENTS)->reg;
 	finished = FALSE;
 	while (finished != TRUE) {  /* i.e. is FALSE or SORTOFTRUE */
		while (lregexec(expP, DOT.l, 0, llength(DOT.l)) ||
			lregexec(expC, DOT.l, 0, llength(DOT.l)) ) {
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
		(void)firstnonwhite(FALSE,1);
		
		clength = indentlen(DOT.l);
		wordlen = 0;
		sentence = FALSE;

		is_comment = ( ((c = char_at(DOT)) == '#') ||
				(c == '>') ||
				(c == '*') ||
				((c == '/') &&
				DOT.o+1 < llength(DOT.l) &&
				 lgetc(DOT.l,DOT.o+1) == '*'));

		if (is_comment)
			comment_char = (c == '#' || c == '>') ? c :'*';

		/* scan through lines, filling words */
		firstflag = TRUE;
		finished = FALSE;
		while (finished == FALSE) { /* i.e. is not TRUE  */
					    /* or SORTOFTRUE */
			if (interrupted()) return ABORT;

			/* get the next character */
			if (is_at_end_of_line(DOT)) {
				c = ' ';
				DOT.l = lforw(DOT.l);
				if (DOT.l == pastline) {
					finished = TRUE;
				} else if (
				lregexec(expP, DOT.l, 0, llength(DOT.l)) ||
				lregexec(expC, DOT.l, 0, llength(DOT.l))) {
					/* we're at a section break */
					finished = SORTOFTRUE;
				}
				DOT.l = lback(DOT.l);
				at_nl = TRUE;
			} else {
				c = char_at(DOT);
				if (at_nl && ( isspace(c) ||
					(is_comment && c == comment_char)))
					c = ' ';
				else
					at_nl = FALSE;
			}
			/* and then delete it */
			if (finished == FALSE) {
				s = ldelete(1L, FALSE);
				if (s != TRUE) return s;
			}

			/* if not a separator, just add it in */
			if (!isblank(c)) {
				/* was it the end of a "sentence"? */
				sentence = (c == '.' || c == '?' ||
						c == ':' || c == '!');
				if (wordlen < NSTRING - 1)
					wbuf[wordlen++] = (char)c;
			} else if (wordlen) {
				/* at a word break with a word waiting */
				/* calculate tentative new length
							with word added */
				newlen = clength + 1 + wordlen;
				if (newlen <= b_val(curbp,VAL_FILL)) {
					/* add word to current line */
					if (!firstflag) {
						/* the space */
						s = linsert(1, ' ');
						if (s != TRUE) return s;
						++clength;
					} 
				} else {
					/* fix the leading indent now, if
						some spaces should be tabs */
					if (b_val(curbp,MDTABINSERT))
						entabline((void *)TRUE, 0, 0);
			                if (lnewline() == FALSE)
						return FALSE;
				        if (linsert(secondindent,' ') == FALSE)
						return FALSE;
					clength = secondindent;
					firstflag = TRUE;
				}
				if (firstflag && is_comment &&
						strncmp("/*",wbuf,2)) {
					s = linsert(1, comment_char);
					if (s != TRUE) return s;
					s = linsert(1, ' ');
					if (s != TRUE) return s;
					clength += 2;
				}
				firstflag = FALSE;

				/* and add the word in in either case */
				for (i=0; i<wordlen; i++) {
					s = linsert(1, wbuf[i]);
					if (s != TRUE) return s;
					++clength;
				}
				if (finished == FALSE && sentence) {
					s = linsert(1, ' ');
					if (s != TRUE) return s;
					++clength;
				}
				wordlen = 0;
			}
		}
		/* catch the case where we're done with a line not because
		  there's no more room, but because we're done processing a
		  section or the region */
		if (b_val(curbp,MDTABINSERT))
			entabline((void *)TRUE, 0, 0);
		DOT.l = lforw(DOT.l);
	}
	return setmark();
}
#endif /* OPT_FORMAT */


#if	OPT_WORDCOUNT
/*	wordcount:	count the # of words in the marked region,
			along with average word sizes, # of chars, etc,
			and report on them.			*/
/*ARGSUSED*/
int
wordcount(int f, int n)
{
	register LINE *lp;	/* current line to scan */
	register int offset;	/* current char to scan */
	long size;		/* size of region left to count */
	register int ch;	/* current character to scan */
	register int wordflag;	/* are we in a word now? */
	register int lastword;	/* were we just in a word? */
	long nwords;		/* total # of words */
	long nchars;		/* total number of chars */
	int nlines;		/* total number of lines in region */
	int avgch;		/* average number of chars/word */
	int status;		/* status return code */
	REGION region;		/* region to look at */

	/* make sure we have a region to count */
	if ((status = getregion(&region)) != TRUE) {
		return(status);
	}
	lp     = region.r_orig.l;
	offset = region.r_orig.o;
	size   = region.r_size;

	/* count up things */
	lastword = FALSE;
	nchars = 0L;
	nwords = 0L;
	nlines = 0;
	while (size--) {

		/* get the current character */
		if (offset == llength(lp)) {	/* end of line */
			ch = '\n';
			lp = lforw(lp);
			offset = 0;
			++nlines;
		} else {
			ch = lgetc(lp, offset);
			++offset;
		}

		/* and tabulate it */
		if (((wordflag = isident(ch)) != 0) && !lastword)
			++nwords;
		lastword = wordflag;
		++nchars;
	}

	/* and report on the info */
	if (nwords > 0L)
		avgch = (int)((100L * nchars) / nwords);
	else
		avgch = 0;

	mlforce("lines %d, words %D, chars %D, avg chars/word %f",
		nlines, nwords, nchars, avgch);

	return(TRUE);
}
#endif
