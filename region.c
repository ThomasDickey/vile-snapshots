/*
 * The routines in this file
 * deal with the region, that magic space
 * between "." and mark. Some functions are
 * commands. Some functions are just for
 * internal use.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/region.c,v 1.101 1999/11/15 23:35:00 Ryan.Murray Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"

typedef	int (*CharProcFunc) (int c);

static	int	blankline(void *flagp, int l, int r);
static	int	do_chars_in_line(void *flagp, int ll, int rr);
static	int	do_lines_in_region(int (*linefunc) (REGN_ARGS), void *argp, int convert_cols);
static	int	killrectmaybesave (int save);

static CharProcFunc charprocfunc;

/*
 * Kill the region. Ask "getregion"
 * to figure out the bounds of the region.
 * Move "." to the start, and kill the characters.
 */
int
killregion(void)
{
	if (regionshape == RECTANGLE)
	    return killrectmaybesave(TRUE);
	else
	    return killregionmaybesave(TRUE);
}

int
killregionmaybesave(int save)
{
	register int    status;
	REGION		region;

	if ((status = getregion(&region)) == TRUE) {
		if (save) {
			kregcirculate(TRUE);
			ksetup();		/* command, so do magic */
			if (regionshape == FULLLINE)
				kregflag |= KLINES;
		}
		DOT = region.r_orig;
		status = ldelete(region.r_size, save);
		if (save) {
			kdone();
			ukb = 0;
		}
	}
#if OPT_SELECTIONS
	find_release_attr(curbp, &region);
#endif
	return status;
}

static int
kill_line(void *flagp, int l, int r)
{
	int s;
	int save = *(int *)flagp;
	LINE *lp = DOT.l;

	s = detabline((void *)FALSE, 0, 0);
	if (s != TRUE) return s;

	DOT.o = l;

	if (r > llength(lp))
	    r = llength(lp);

	if (r > l) {
	    s = ldelete((B_COUNT)(r - l), save);
	    if (s != TRUE) return s;
	}

	if (save)
		kinsert('\n');

	if (b_val(curbp,MDTABINSERT))
		s = entabline((void *)TRUE, 0, 0);

	DOT.o = l;
	return s;
}

static int
killrectmaybesave(int save)
{
	register int    s;
	MARK savedot;

	savedot = DOT;

	if (save) {
		kregcirculate(TRUE);
		ksetup();
		if (regionshape == FULLLINE) {
			kregflag |= KLINES;
		} else if (regionshape == RECTANGLE) {
			kregflag |= KRECT;
		}
	}
	s = do_lines_in_region(kill_line, (void *)&save, FALSE);
	restore_dot(savedot);

	if (s && do_report(klines+(kchars!=0))) {
		mlwrite("[%d line%s, %d character%s killed]",
			klines, PLURAL(klines),
			kchars, PLURAL(kchars));
	}
	if (save) {
		kdone();
		ukb = 0;
	}
	return (s);
}

/*
 * open up a region -- shift the "selected" area of each line by its
 * own length.  most useful for rectangular regions.
 * fill character is space, unless a string is passed in, in which case
 * it is used instead.
 */
/*ARGSUSED*/
static int
open_hole_in_line(void *flagp, int l, int r)
{
	char *string = (char *)flagp;
	int len;
	int s;
	int saveo = DOT.o;
	LINE *lp = DOT.l;

	s = detabline((void *)FALSE, 0, 0);
	if (s != TRUE) return s;

	if (llength(lp) <= l) {	/* nothing to do if no string */
		if (!string) {
		    if (b_val(curbp,MDTABINSERT))
			    s = entabline((void *)TRUE, 0, 0);
		    DOT.o = saveo;
		    return s;
		} else {
		    DOT.o = llength(lp);
		    if (l - DOT.o)
			linsert(l - DOT.o, ' ');
		}
	}
	DOT.o = l;
	if (string) {
		len = strlen(string);
		if (len < r - l)
			len = r - l;
	} else {
		len = r - l;
	}
	s =  lstrinsert(string, len );
	if (s != TRUE) return s;

	DOT.o = saveo;
	if (b_val(curbp,MDTABINSERT))
		s = entabline((void *)TRUE, 0, 0);
	return s;
}

/*
 * open up a region
 */
int
openregion(void)
{
	return do_lines_in_region(open_hole_in_line, (void *)NULL, FALSE);
}

/*
 * open up a region, filling it with a supplied string
 * this is pretty simplistic -- could be a lot more clever
 */
int
stringrect(void)
{
	int		s;
	static char     buf[NLINE];

	s = mlreply("Rectangle text: ", buf, sizeof(buf) );
	if (s != TRUE)
		return s;

/* i couldn't decide at first whether we should be inserting or
	overwriting... this chooses. */
#ifdef insert_the_string
	return do_lines_in_region(open_hole_in_line, (void *)buf, FALSE);
#else /* overwrite the string */
	return do_lines_in_region(blankline, (void *)buf, FALSE);
#endif
}

/*
 * insert a shiftwidth at the front of the line
 * don't do it if we're in cmode and the line starts with '#'
 */
/*ARGSUSED*/
static int
shift_right_line(void *flagp GCC_UNUSED, int l GCC_UNUSED, int r GCC_UNUSED)
{
	int s, t;

	if (is_empty_line(DOT) || (is_c_mode(curbp) &&
					llength(DOT.l) > 0 &&
					char_at(DOT) == '#')) {
		return TRUE;
	}
	s = shiftwid_val(curbp);
	t = curtabval;
	DOT.o = w_left_margin(curwp);
	if (s) {  /* try to just insert tabs if possible */
		if (b_val(curbp,MDTABINSERT) && s >= t && (s % t == 0)) {
			linsert(s/t, '\t');
		} else {
			detabline((void *)TRUE, 0, 0);
			DOT.o = w_left_margin(curwp);
			linsert(s, ' ');
		}
		if (b_val(curbp,MDTABINSERT))
			entabline((void *)TRUE, 0, 0);
	}
	return firstnonwhite(FALSE,1);
}

/*
 * shift region right by a tab stop
 * if region is rectangular, "open it up"
 */
int
shiftrregion(void)
{
	if (regionshape == RECTANGLE)
	    return do_lines_in_region(open_hole_in_line, (void *)NULL, FALSE);

	regionshape = FULLLINE;
	return do_lines_in_region(shift_right_line, (void *)0, FALSE);
}

/*
 * delete a shiftwidth-equivalent from the front of the line
 */
/*ARGSUSED*/
static int
shift_left_line(void *flagp GCC_UNUSED, int l GCC_UNUSED, int r GCC_UNUSED)
{
	register int	i;
	register int	lim;
	register int	s;
	register LINE *linep = DOT.l;

	if (llength(linep) == 0)
		return TRUE;

	s = shiftwid_val(curbp);

	detabline((void *)TRUE, 0, 0);

	/* examine the line to the end, or the first shiftwidth, whichever
		comes first */
	lim = (s < llength(linep)) ? s : llength(linep);


	i = 0;
	/* count the leading spaces */
	while (lgetc(linep,i) == ' ' && i < lim)
		i++;

	if (i != 0) { /* did we find space/tabs to kill? */
		DOT.o = w_left_margin(curwp);
		if ((s = ldelete((B_COUNT)i,FALSE)) != TRUE)
			return s;
	}

	DOT.o = w_left_margin(curwp);
	if (b_val(curbp,MDTABINSERT))
		entabline((void *)TRUE, 0, 0);
	return TRUE;
}

/*
 * shift region left by a tab stop
 */
int
shiftlregion(void)
{
	if (regionshape == RECTANGLE)
	    return killrectmaybesave(FALSE);

	regionshape = FULLLINE;
	return do_lines_in_region(shift_left_line, (void *)0, FALSE);
}

/*
 * change all tabs in the line to the right number of spaces.
 * leadingonly says only do leading whitespace
 */
/*ARGSUSED*/
int
detabline(void *flagp GCC_UNUSED, int l GCC_UNUSED, int r GCC_UNUSED)
{
	register int	s;
	register int	c;
	int	ocol;
	long leadingonly = (long)flagp;
	LINE *lp = DOT.l;

	if (llength(lp) == 0)
		return TRUE;

	ocol = getccol(FALSE);

	DOT.o = 0;

	/* remove tabs from the entire line */
	while (DOT.o < llength(lp)) {
		c = char_at(DOT);
		if (leadingonly && !isSpace(c))
			break;
		/* if we have found a tab to remove */
		if (c == '\t') {
			if ((s = ldelete(1L, FALSE)) != TRUE)
				return s;
			if ((s = insspace(TRUE,
				curtabval - (DOT.o % curtabval) )) != TRUE)
				return s;
		}
		DOT.o++;
	}
	(void)gocol(ocol);
	return TRUE;
}


/*
 * change all tabs in the region to the right number of spaces
 */
int
detab_region(void)
{
	regionshape = FULLLINE;
	return do_lines_in_region(detabline,(void *)FALSE, FALSE);
}

/*
 * convert all appropriate spaces in the line to tab characters.
 * leadingonly says only do leading whitespace
 */
/*ARGSUSED*/
int
entabline(void *flagp GCC_UNUSED, int l GCC_UNUSED, int r GCC_UNUSED)
{
	int	savecol;
	long leadingonly = (long)flagp;
	LINE *lp = DOT.l;
	C_NUM ocol, ncol;
	C_NUM ooff, noff;
	int ch;

	if (llength(lp) == 0)
		return TRUE;

	savecol = getccol(FALSE);

	ooff = noff = 0;
	ocol = ncol = 0;
	for_ever {

	    if (ooff == llength(lp))
		ch = '\n';
	    else
		ch = lgetc(lp, ooff);

	    switch ( ch ) {
		case ' ':
		    ocol++;
		    break;
		case '\t':
		    ocol = nexttabcol(ocol,curtabval);
		    break;
		default:
			while (nexttabcol(ncol,curtabval) <= ocol) {
				if (ncol + 1 == ocol)
					break;
				(void)lreplc(lp, noff++, '\t');
				ncol = nexttabcol(ncol,curtabval);
			}
			while (ncol < ocol) {
				(void)lreplc(lp, noff++, ' ');
				ncol++;
			}
			if (ch == '\n' || leadingonly) {
				if (noff != ooff) {
				    while (ooff < llength(lp))
					(void)lreplc(lp, noff++,
						    lgetc(lp, ooff++));
				    DOT.o = noff;
				    (void)ldelete(ooff - noff, FALSE);
				}
				(void)gocol(savecol);
				return TRUE;
			}
			(void)lreplc(lp, noff++, ch);
			ncol++, ocol++;
	    }
	    ooff++;
	}
}

/*
 * convert all appropriate spaces in the region to tab characters
 */
int
entab_region(void)
{
	regionshape = FULLLINE;
	return do_lines_in_region(entabline,(void *)FALSE, FALSE);
}

/* trim trailing whitespace from a line.  dot is preserved if possible. */
/* (dot is even preserved if it was sitting on the newline) */
/*ARGSUSED*/
int
trimline(void *flag GCC_UNUSED, int l GCC_UNUSED, int r GCC_UNUSED)
{
	register int off;
	register LINE *lp;
	int odoto, s;
	int delcnt, was_at_eol;

	lp = DOT.l;

	if (llength(lp) == 0)
		return TRUE;

	/* may return -1 if line is all whitespace.  but
		that's okay, since the math still works. */
	off = lastchar(lp);

	delcnt = llength(lp) - (off + 1);
	if (!delcnt)
		return TRUE;

	odoto = DOT.o;
	was_at_eol = (odoto == llength(lp));

	DOT.o = off + 1;
	s = ldelete((B_COUNT)delcnt,FALSE);

	if (odoto > off) {	/* do we need to back up? */
		odoto = llength(lp);
		if (!was_at_eol)
			odoto--; /* usually we want the last char on line */
	}

	if (odoto < 0)
		DOT.o = 0;
	else
		DOT.o = odoto;
	return s;
}

/*
 * trim trailing whitespace from a region
 */
int
trim_region(void)
{
	regionshape = FULLLINE;
	return do_lines_in_region(trimline, (void *)0, FALSE);
}

/* turn line, or part, to whitespace */
/*ARGSUSED*/
static int
blankline(void *flagp, int l, int r)
{
	char *string = (char *)flagp;
	int len;
	int s = TRUE;
	int saveo;
	LINE *lp = DOT.l;

	saveo = l;

	/* if the shape is rectangular, then l and r are columns, not
		offsets */
	if (regionshape == RECTANGLE) {
		s = detabline((void *)FALSE, 0, 0);
		if (s != TRUE)
			return s;
	}

	if (llength(lp) <= l) {	/* nothing to do if no string */
		if (!string) {
		    if (regionshape == RECTANGLE && b_val(curbp,MDTABINSERT))
			    s = entabline((void *)TRUE, 0, 0);
		    DOT.o = saveo;
		    return s;
		} else {
		    DOT.o = llength(lp);
		    if (l - DOT.o)
			linsert(l - DOT.o, ' ');
		}
	}

	DOT.o = l;

	if (llength(lp) <= r) {
		/* then the rect doesn't extend to the end of line */
		ldelete((B_COUNT)(llength(lp) - l), FALSE);

		/* so there's nothing beyond the rect, so insert at
			most r-l chars of the string, or nothing */
		if (string) {
		    len = strlen(string);
		    if (len > r - l)
			    len = r - l;
		} else {
		    len = 0;
		}
	} else {
		/* the line goes on, so delete and reinsert exactly */
		ldelete((B_COUNT)(r - l), FALSE);
		len = r - l;
	}

	s = lstrinsert(string, len);
	if (s != TRUE) return s;

	if (regionshape == RECTANGLE && b_val(curbp,MDTABINSERT))
		s = entabline((void *)TRUE, 0, 0);

	return s;
}

/*
 * Copy all of the characters in the
 * region to the kill buffer. Don't move dot
 * at all. This is a bit like a kill region followed
 * by a yank.
 */
static int
_yankchar(int c)
{
	kinsert(c);
	/* FIXME check return value, longjmp back to yank_line */
	return -1;
}

/*ARGSUSED*/
static int
yank_line(void *flagp GCC_UNUSED, int l, int r)
{
	int s;
	charprocfunc = _yankchar;
	s = do_chars_in_line((void  *)NULL, l, r);
	if (s) {
	    if (r == llength(DOT.l) || regionshape == RECTANGLE) {
		/* we don't necessarily want to insert the last newline
			in a region, so we delay it */
		s = kinsertlater('\n');
	    }
	    else if (r > llength(DOT.l)) {
		/* This can happen when selecting with the mouse. */
		kinsert('\n');
	    }
	}
	return s;
}

int
yankregion(void)
{
	register int    s;

	kregcirculate(TRUE);
	ksetup();
	if (regionshape == FULLLINE) {
		kregflag |= KLINES|KYANK;
	} else if (regionshape == RECTANGLE) {
		kregflag |= KRECT|KYANK;
	}
	s = do_lines_in_region(yank_line, (void *)0, TRUE);
	if (s && do_report(klines+(kchars!=0))) {
		mlwrite("[%d line%s, %d character%s yanked]",
			klines, PLURAL(klines),
			kchars, PLURAL(kchars));
	}
	kdone();
	ukb = 0;
	return (s);
}


#if NEEDED
int
_blankchar(int c)
{
	if (!isSpace(c))
		return ' ';
	return -1;
}
#endif

static int
_to_lower(int c)
{
	if (isUpper(c))
		return c ^ DIFCASE;
	return -1;
}

static int
_to_upper(int c)
{
	if (isLower(c))
		return c ^ DIFCASE;
	return -1;
}

static int
_to_caseflip(int c)
{
	if (isAlpha(c))
		return c ^ DIFCASE;
	return -1;
}

/*
 * turn region to whitespace
 */
int
blank_region(void)
{
	return do_lines_in_region(blankline, (void *)NULL, FALSE);
}

int
flipregion(void)
{
	charprocfunc = _to_caseflip;
	return do_lines_in_region(do_chars_in_line,(void *)NULL, TRUE);
}

int
lowerregion(void)
{
	charprocfunc = _to_lower;
	return do_lines_in_region(do_chars_in_line,(void *)NULL, TRUE);
}

int
upperregion(void)
{
	charprocfunc = _to_upper;
	return do_lines_in_region(do_chars_in_line,(void *)NULL, TRUE);
}

/* finish filling in the left/right column info for a rectangular region */
static void
set_rect_columns(register REGION *rp)
{
	if (regionshape == RECTANGLE) {
		MARK	lc, rc;

		lc = (rp->r_orig.o < rp->r_end.o) ? rp->r_orig : rp->r_end;
		rc = (rp->r_orig.o > rp->r_end.o) ? rp->r_orig : rp->r_end;

		/* convert to columns */
		rp->r_leftcol = getcol(lc, FALSE);
		rp->r_rightcol = getcol(rc, FALSE) + 1;
	}
}


/*
 * This routine figures out the
 * bounds of the region in the current window, and
 * fills in the fields of the "REGION" structure pointed
 * to by "rp". Because the dot and mark are usually very
 * close together, we scan outward from dot looking for
 * mark. This should save time. Return a standard code.
 */

REGION *haveregion;

int
getregion(register REGION *rp)
{
	register LINE   *flp;
	register LINE   *blp;
	B_COUNT fsize;
	B_COUNT bsize;
	int	len_rs = len_record_sep(curbp);

	if (haveregion) {
		*rp = *haveregion;
		haveregion = NULL;
		return TRUE;
	}

#if OPT_SELECTIONS
	rp->r_attr_id = (unsigned short) assign_attr_id();
#endif

	if (MK.l == NULL) {
		mlforce("BUG: getregion: no mark set in this window");
		return FALSE;
	}

	if (sameline(DOT, MK)) {
		rp->r_orig =
		rp->r_end  = DOT;
		if (regionshape == FULLLINE) {
			rp->r_orig.o =
			rp->r_end.o  = w_left_margin(curwp);
			rp->r_end.l  = lforw(DOT.l);
			rp->r_size   = (B_COUNT)(line_length(DOT.l) - w_left_margin(curwp));
		} else {
			if (DOT.o < MK.o) {
				rp->r_orig.o = rp->r_leftcol = DOT.o;
				rp->r_end.o  = rp->r_rightcol = MK.o;
			} else {
				rp->r_orig.o = rp->r_leftcol = MK.o;
				rp->r_end.o  = rp->r_rightcol = DOT.o;
			}
			rp->r_size = rp->r_end.o - rp->r_orig.o;
			set_rect_columns(rp);
		}
		return TRUE;
	}

#if !SMALLER
	if (b_is_counted(curbp)) { /* we have valid line numbers */
		register LINE   *flp_start;
		L_NUM dno, mno;
		dno = line_no(curbp, DOT.l);
		mno = line_no(curbp, MK.l);
		if (mno > dno) {
			flp = DOT.l;
			blp = MK.l;
			rp->r_orig = DOT;
			rp->r_end  = MK;
		} else {
			flp = MK.l;
			blp = DOT.l;
			rp->r_orig = MK;
			rp->r_end  = DOT;
		}
		fsize = (B_COUNT)(line_length(flp) -
				    ((regionshape == FULLLINE) ?
					w_left_margin(curwp) : rp->r_orig.o));
		flp_start = flp;
		while (flp != blp) {
			flp = lforw(flp);
			if (flp != buf_head(curbp) && flp != flp_start) {
			    fsize += line_length(flp) - w_left_margin(curwp);
			} else if (flp != blp) {
			    mlforce("BUG: hit buf end in getregion");
			    return FALSE;
			}

			if (flp == blp) {
				if (regionshape == FULLLINE) {
					rp->r_orig.o =
					rp->r_end.o  = w_left_margin(curwp);
					rp->r_end.l  = lforw(rp->r_end.l);
				} else {
					fsize -=
					    (line_length(flp) - rp->r_end.o);
					set_rect_columns(rp);
				}
				rp->r_size = fsize;
				return TRUE;
			}
		}
	} else
#endif /* !SMALLER */
	{
		blp = DOT.l;
		flp = DOT.l;
		if (regionshape == FULLLINE) {
			bsize = fsize =
			(B_COUNT)(line_length(blp) - w_left_margin(curwp));
		} else {
			bsize = (B_COUNT)(DOT.o - w_left_margin(curwp));
			fsize = (B_COUNT)(line_length(flp) - DOT.o);
		}
		while ((flp != buf_head(curbp)) ||
				(lback(blp) != buf_head(curbp))) {
		    if (flp != buf_head(curbp)) {
			flp = lforw(flp);
			if (flp != buf_head(curbp))
				fsize += line_length(flp) - w_left_margin(curwp);
			if (flp == MK.l) {
				rp->r_orig = DOT;
				rp->r_end  = MK;
				if (regionshape == FULLLINE) {
					rp->r_orig.o =
					rp->r_end.o  = w_left_margin(curwp);
					rp->r_end.l  = lforw(rp->r_end.l);
				} else {
					fsize -= (line_length(flp) - MK.o);
					set_rect_columns(rp);
				}
				rp->r_size = fsize;
				return TRUE;
			}
		    }
		    if (lback(blp) != buf_head(curbp)) {
			blp = lback(blp);
			bsize += line_length(blp) - w_left_margin(curwp);
			if (blp == MK.l) {
				rp->r_orig = MK;
				rp->r_end  = DOT;
				if (regionshape == FULLLINE) {
					rp->r_orig.o =
					rp->r_end.o  = w_left_margin(curwp);
					rp->r_end.l  = lforw(rp->r_end.l);
				} else {
					bsize -= (MK.o - w_left_margin(curwp));
					set_rect_columns(rp);
				}
				rp->r_size = bsize;
				return TRUE;
			}
		    }
		}
	}
	mlforce("BUG: lost mark");
	return FALSE;
}

int
get_fl_region(REGION *rp)
{
	int	status;

	regionshape = FULLLINE;
	status = getregion(rp);
	regionshape = EXACT;

	return status;
}


/* the (*linefunc)() routine that this calls _must_ be prepared to
	to get empty lines passed to it from this routine. */
static int
do_lines_in_region(
int (*linefunc) (REGN_ARGS),
void *argp,
int convert_cols)	/* if rectangle, convert columns to offsets */
{
	register LINE   *linep;
	register int    status;
	REGION		region;
	C_NUM		l, r;

	if ((status=getregion(&region)) == TRUE) {

		/* for each line in the region, ... */
		linep = region.r_orig.l;
		for_ever {
			/* move through the region... */
			/* it's important that the linefunc get called
				for every line, even if blank, since it
				may want to keep track of newlines, for
				instance */
			DOT.l = linep;
			DOT.o = w_left_margin(curwp);
			if (regionshape == RECTANGLE) {
			    if (convert_cols) {
				C_NUM reached;
				l = getoff(region.r_leftcol, &reached);
				if (l < 0) l = -l + llength(linep);
				r = getoff(region.r_rightcol, &reached);
				if (r < 0) r = -r + llength(linep);
				if (reached > region.r_rightcol) /* a tab? */
					reached = region.r_rightcol;
			    } else {
				l = region.r_leftcol;
				r = region.r_rightcol;
			    }
			} else {
				l = w_left_margin(curwp);
				r = llength(DOT.l);
				if (sameline(region.r_orig, DOT))
					l = region.r_orig.o;
				if (sameline(region.r_end, DOT)) {
					r = region.r_end.o;
					/* if we're on the end-of-
					 * region, in col 0, we're
					 * done. we don't want to
					 * call the line function
					 * for the empty case
					 */
					if (r == 0)
						break;
				}
			}

			/* ...and process each line */
			if ((status = (*linefunc)(argp,l,r)) != TRUE) {
				return status;
			}

			if (linep == region.r_end.l)
				break;

			linep = lforw(linep);

			/*
			 * If this happened to be a full-line region, then
			 * we've bumped the end-region pointer to the next line
			 * so we won't run into problems with the width.  But
			 * we don't really want to _process_ the end-region
			 * line, so check here also. (Leave the above check
			 * alone just in case the buffer is empty).
			 */
			if (regionshape == FULLLINE
			 && linep == region.r_end.l)
				break;

		}
		if (regionshape == FULLLINE) {
		    (void)kinsertlater(-1);
		    (void)firstnonwhite(FALSE,1);
		}
	}
#if OPT_SELECTIONS
	if (linefunc == kill_line) /* yuck.  it's the only one that deletes */
		find_release_attr(curbp, &region);
#endif
	return status;
}


/* walk through the characters in a line, applying a function.  the
	line is marked changed if the function returns other than -1.
	if it returns other than -1, then the char is replaced with
	that value.
    the ll and rr args are OFFSETS, so if you use this routine with
	do_lines_in_region, tell it to CONVERT columns to offsets. */
/*ARGSUSED*/
static int
do_chars_in_line(
void	*flagp GCC_UNUSED,
int	ll, int	rr)		/* offsets of of chars to be processed */
{
	register int    c,nc;
	int		changed = 0;
	register LINE *lp;
	int i;

	lp = DOT.l;


	if (llength(lp) < ll)
		return TRUE;

	DOT.o = ll;
	if (rr > llength(lp))
		rr = llength(lp);

	for (i = ll; i < rr; i++) {
		c = lgetc(lp,i);
		nc = (charprocfunc)(c);
		if (nc != -1) {
			copy_for_undo(lp);
			lputc(lp,i,(char)nc);
			changed++;
		}
	}
	if (changed)
	    chg_buff(curbp, WFHARD);
	return TRUE;
}


/* Helper function for vile commands that must access do_lines_in_region(). */
DORGNLINES
get_do_lines_rgn(void)
{
    return (DORGNLINES)(do_lines_in_region);
}
