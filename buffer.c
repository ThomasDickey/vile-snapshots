/*
 * Buffer management.
 * Some of the functions are internal,
 * and some are actually attached to user
 * keys. Like everyone else, they set hints
 * for the display system.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/buffer.c,v 1.162 1997/10/13 13:06:48 kev Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"

/*--------------------------------------------------------------------------*/
#if	OPT_UPBUFF
static	int	show_BufferList ( BUFFER *bp );
#define	update_on_chg(bp) (!b_is_temporary(bp) || show_all)
#endif

#if	OPT_PROCEDURES
static	void	run_buffer_hook (void);
#endif

#if	!OPT_MAJORMODE
static	int	cmode_active ( BUFFER *bp );
#endif

static	BUFFER *find_BufferList (void);
static	BUFFER *find_b_number ( const char *number  );
static	BUFFER *find_latest (void);
static	BUFFER *find_nth_created ( int n );
static	BUFFER *find_nth_used ( int n );
static	int	countBuffers (void);
static	int	hist_show (void);
static	int	lookup_hist ( BUFFER *bp );
static	void	FreeBuffer ( BUFFER *bp );
static	void	MarkDeleted ( BUFFER *bp );
static	void	MarkUnused ( BUFFER *bp );
static	void	TrackAlternate ( BUFFER *bp );
static	void	makebufflist (LIST_ARGS);

#if !SMALLER
static	void	footnote ( int c );
#endif

/*--------------------------------------------------------------------------*/

static	BUFFER	*last_bp,	/* noautobuffer value */
		*this_bp,	/* '%' buffer */
		*that_bp;	/* '#' buffer */
static	int	show_all,	/* true iff we show all buffers */
		updating_list;

/*--------------------------------------------------------------------------*/

/*
 * Returns the buffer-list pointer, if it exists.
 */
static BUFFER *
find_BufferList(void)
{
	return find_b_name(BUFFERLIST_BufName);
}

/*
 * Look for a buffer-pointer in the list, to see if it has been delinked yet.
 */
BUFFER *find_bp(BUFFER *bp1)
{
	register BUFFER *bp;
	for_each_buffer(bp)
		if (bp == bp1)
			return bp;
	return 0;
}

/*
 * Returns the total number of buffers in the list.
 */
static int
countBuffers(void)
{
	register BUFFER *bp;
	register int	count = 0;
	for_each_buffer(bp)
		count++;
	return count;
}

/*
 * Returns the n'th buffer created
 */
static BUFFER *
find_nth_created(int n)
{
	register BUFFER *bp;

	for_each_buffer(bp)
		if (bp->b_created == n)
			return bp;
	return 0;
}

/*
 * Returns the n'th buffer used
 */
static BUFFER *
find_nth_used(int n)
{
	register BUFFER *bp;

	for_each_buffer(bp)
		if (bp->b_last_used == n)
			return bp;
	return 0;
}

/*
 * Returns the buffer with the largest value of 'b_last_used'.
 */
static BUFFER *
find_latest(void)
{
	register BUFFER *bp, *maxbp = 0;

	for_each_buffer(bp) {
		if (maxbp == 0)
			maxbp = bp;
		else if (maxbp->b_last_used < bp->b_last_used)
			maxbp = bp;
	}
	return maxbp;
}

/*
 * Look for a filename in the buffer-list
 */
BUFFER *
find_b_file(const char *fname)
{
	register BUFFER *bp;
	char	nfname[NFILEN];

	(void)lengthen_path(strcpy(nfname, fname));
	for_each_buffer(bp)
		if (same_fname(nfname, bp, FALSE))
			return bp;
	return 0;
}

/*
 * Look for a specific buffer number
 */
BUFFER *
find_b_hist(int number)
{
	register BUFFER *bp;

	if (number >= 0) {
		for_each_buffer(bp)
			if (!b_is_temporary(bp) && (number-- <= 0))
				break;
	} else
		bp = 0;
	return bp;
}

/*
 * Look for a buffer-number (i.e., one from the buffer-list display)
 */
static BUFFER *
find_b_number(const char *number)
{
	register int	c = 0;
	while (isDigit(*number))
		c = (c * 10) + (*number++ - '0');
	if (!*number)
		return find_b_hist(c);
	return 0;
}

/*
 * Find buffer, given (possibly) filename, buffer name or buffer number
 */
BUFFER *
find_any_buffer(const char *name)
{
	register BUFFER *bp;

	if ((bp=find_b_name(name)) == 0		/* Try buffer */
	 && (bp=find_b_file(name)) == 0		/* ...then filename */
	 && (bp=find_b_number(name)) == 0) {	/* ...then number */
		mlforce("[No such buffer] %s", name);
		return 0;
	}

	return bp;
}

/*
 * Delete all instances of window pointer for a given buffer pointer
 */
int
zotwp(BUFFER *bp)
{
	register WINDOW *wp;
	register BUFFER *nbp;
	register BUFFER *obp = 0;
	WINDOW	dummy;
	int s = FALSE;

	TRACE(("zotwp(%s)\n", bp->b_bname))

	/*
	 * Locate buffer to switch to after deleting windows.  It can't be
	 * the same as the buffer which is being deleted and if it's an
	 * invisible or scratch buffer, it must have been already visible.
	 * From the set of allowable buffers, choose the most recently
	 * used.
	 */
	for_each_buffer(nbp) {
		if (nbp != bp && (!b_is_temporary(nbp) || nbp->b_nwnd != 0)) {
			if (obp == 0)
				obp = nbp;
			else if (obp->b_last_used < nbp->b_last_used)
				obp = nbp;
		}
	}

	/* Delete window instances...*/
	for_each_window(wp) {
		if (wp->w_bufp == bp && wheadp->w_wndp != NULL) {
			dummy.w_wndp = wp->w_wndp;
			s = delwp(wp);
			wp = &dummy;
		}
	}
	if (obp != NULL)
		s = swbuffer(obp);

	return s;
}

/*
 * Mark a buffer's created member to 0 (unused), adjusting the other buffers
 * so that there are no gaps.
 */
static void
MarkDeleted(register BUFFER *bp)
{
	int	created = bp->b_created;

	if (created) {
		bp->b_created = 0;
		for_each_buffer(bp) {
			if (bp->b_created > created)
				bp->b_created -= 1;
		}
	}
}

/*
 * Mark a buffer's last-used member to 0 (unused), adjusting the other buffers
 * so that there are no gaps.
 */
static void
MarkUnused(register BUFFER *bp)
{
	int	used = bp->b_last_used;

	if (used) {
		bp->b_last_used = 0;
		for_each_buffer(bp) {
			if (bp->b_last_used > used)
				bp->b_last_used -= 1;
		}
	}
}

/*
 * After 'bclear()', frees remaining storage for a buffer.
 */
static void
FreeBuffer(BUFFER *bp)
{
	if (bp->b_fname != out_of_mem)
		FreeIfNeeded(bp->b_fname);

#if !WINMARK
	if (is_header_line(MK, bp)) {
		MK.l = null_ptr;
		MK.o = 0;
	}
#endif
	lfree(buf_head(bp), bp);		/* Release header line. */
	if (delink_bp(bp)) {
		if (curbp == bp)
			curbp = NULL;

#if OPT_HILITEMATCH
		clobber_save_curbp(bp);
#endif
#if OPT_PERL || OPT_TCL
		api_free_private(bp->b_api_private);
#endif
		free((char *) bp);		/* Release buffer block */
	}
}

/*
 * Adjust buffer-list's last-used member to account for a new buffer.
 */
static void
TrackAlternate(BUFFER *newbp)
{
	register BUFFER *bp;

	if (!updating_list) {
		MarkUnused(newbp);
		if ((bp = find_latest()) != 0) {
			newbp->b_last_used = (bp->b_last_used + 1);
		} else {	/* shouldn't happen... */
			newbp->b_last_used = 1;
		}
	}
}

/* c is an index (counting only visible/nonscratch) into buffer list */
char *
hist_lookup(int c)
{
	register BUFFER *bp = find_b_hist(c);

	return (bp != 0) ? bp->b_bname : 0;
}

/* returns the buffer corresponding to the given number in the history */
static int
lookup_hist(BUFFER *bp1)
{
	register BUFFER *bp;
	register int	count = -1;

	for_each_buffer(bp)
		if (!b_is_temporary(bp)) {
			count++;
			if (bp == bp1)
				return count;
		}
	return -1;	/* no match */
}

/*
 * Run the $buffer-hook procedure, if it's defined.  Note that we mustn't do
 * this if curwp->w_bufp != curbp, since that would break the use of DOT and MK
 * throughout the program when performing editing operations.
 */
#if OPT_PROCEDURES
static int bufhooking;
#define DisableBufferHook bufhooking++;
#define EnableBufferHook  bufhooking--;

static void
run_buffer_hook(void)
{
	if (!bufhooking
	 && !reading_msg_line
	 && *bufhook
	 && curwp != 0
	 && curwp->w_bufp == curbp) {
		DisableBufferHook;
		run_procedure(bufhook);
		EnableBufferHook;
	}
}
#else
#define run_buffer_hook() /*EMPTY*/
#define DisableBufferHook /*EMPTY*/
#define EnableBufferHook  /*EMPTY*/
#endif

static int
hist_show(void)
{
	register BUFFER *bp;
	register int i = 0;
	char line[NLINE];
	BUFFER *abp = (BUFFER *)0;

	if (!global_g_val(GMDABUFF))
		abp = find_alt();

	(void)strcpy(line,"");
	for_each_buffer(bp) {
		if (!b_is_temporary(bp)) {
			if (bp != curbp) {	/* don't bother with current */
				(void)lsprintf(line+strlen(line), "  %d%s%s %s",
					i,
					b_is_changed(bp) ? "*" : "",
					(abp && abp == bp) ? "#" : "",
					bp->b_bname);
			}
			if (++i > 9)	/* limit to single-digit */
				break;
		}
	}
	if (strcmp(line,"")) {
		mlforce("%s",line);
		return TRUE;
	} else {
		return FALSE;
	}
}

/*
 * Given a buffer, returns any corresponding WINDOW pointer
 */
WINDOW *
bp2any_wp(BUFFER *bp)
{
	register WINDOW *wp;
	for_each_window(wp)
		if (wp->w_bufp == bp)
			break;
	return wp;
}

/*
 * Lets the user select a given buffer by its number.
 */
int
histbuff(int f, int n)
{
	register int thiskey, c;
	register BUFFER *bp = 0;
	char *bufn;

	if (f == FALSE) {
		if (!hist_show())
			return FALSE;
		thiskey = lastkey;
		c = keystroke8();
		mlerase();
		if (c == thiskey) {
			c = lookup_hist(bp = find_alt());
		} else if (isDigit(c)) {
			c = c - '0';
		} else {
			if (!isreturn(c))
				unkeystroke(c);
			return FALSE;
		}
	} else {
		c = n;
	}

	if (c >= 0) {
		if ((bufn = hist_lookup(c)) == NULL) {
			mlwarn("[No such buffer.]");
			return FALSE;
		}
		/* first assume its a buffer name, then a file name */
		if ((bp = find_b_name(bufn)) == NULL)
			return getfile(bufn,TRUE);

	} else if (bp == 0) {
		mlwarn("[No alternate buffer]");
		return FALSE;
	}

	return swbuffer(bp);
}

/*
 * Returns the alternate-buffer pointer, if any
 */
BUFFER *
find_alt(void)
{
	register BUFFER *bp;

	if (global_g_val(GMDABUFF)) {
		BUFFER *any_bp = 0;
		if ((bp = find_bp(curbp)) == 0)
			bp = bheadp;
		for (; bp; bp = bp->b_bufp) {
			if (bp != curbp) {
				/*
				 * If we allow temporary buffers to be selected as the
				 * alternate, we get into the situation where we try
				 * to load the message buffer, which has a NULL filename.
				 * Note that the 'noautobuffer' case, below, never selects
				 * a temporary buffer as the alternate buffer.
				 */
				if (!b_is_temporary(bp))
					return bp;
			}
		}
		return any_bp;
	} else {
		register BUFFER *last = 0,
				*next = find_latest();

		for_each_buffer(bp) {
			if ((bp != next)
			 && !b_is_temporary(bp)) {
				if (last) {
					if (last->b_last_used < bp->b_last_used)
						last = bp;
				} else
					last = bp;
			}
		}
		return last;
	}
}

/* make '#' buffer in noautobuffer-mode, given filename */
void
imply_alt(
char *	fname,
int	copy,
int	lockfl)
{
	register BUFFER *bp;
	register LINE	*lp;
	BUFFER *savebp;
	char nfname[NFILEN];

	if (interrupted() || fname == 0) /* didn't really have a filename */
		return;

	(void)lengthen_path(strcpy(nfname, fname));
	if (global_g_val(GMDIMPLYBUFF)
	 && curbp != 0
	 && curbp->b_fname != 0
	 && !same_fname(nfname, curbp, FALSE)
	 && !isInternalName(fname)) {
		savebp = curbp;
		if ((bp = find_b_file(nfname)) == 0) {
			L_NUM	top, now;

			if ((bp = make_bp(fname, 0)) == 0) {
				mlforce("[Cannot create buffer]");
				return;
			}

			/* fill the buffer */
			b_clr_flags(bp, BFINVS|BFCHG);
			b_set_flags(bp, BFIMPLY);
			bp->b_active = TRUE;
			ch_fname(bp, nfname);
			make_local_b_val(bp,MDNEWLINE);
			if (curwp != 0 && curwp->w_bufp == curbp) {
				top = line_no(curbp, curwp->w_line.l);
				now = line_no(curbp, DOT.l);
			} else
				top = now = -1;

			if (copy) {
				for_each_line(lp, savebp) {
					if (addline(bp,lp->l_text,lp->l_used) != TRUE) {
						mlforce("[Copy-buffer failed]");
						return;
					}
				}
				set_b_val(bp, MDNEWLINE, b_val(savebp,MDNEWLINE));
				setm_by_suffix(bp);
				setm_by_preamble(bp);
			} else
				readin(fname, lockfl, bp, FALSE);

			/* setup so that buffer-toggle works as in vi (i.e.,
			 * the top/current lines of the screen are the same).
			 */
			if (now >= 0) {
				for_each_line(lp,bp) {
					if (--now == 0) {
						bp->b_dot.l = lp;
						bp->b_dot.o = curbp->b_dot.o;
						break;
					}
					if (--top == 0) {
						bp->b_wline.l = lp;
					}
				}
			}
		}
		DisableBufferHook;
		make_current(bp);
		make_current(savebp);
		EnableBufferHook;
	}
}

/* switch back to the most recent buffer */
/* ARGSUSED */
int
altbuff(int f GCC_UNUSED, int n GCC_UNUSED)
{
	register BUFFER *bp = find_alt();
	if (bp == 0) {
		mlwarn("[No alternate filename to substitute for #]");
		return FALSE;
	} else {
		return swbuffer(bp);
	}
}

/*
 * Attach a buffer to a window. The
 * values of dot and mark come from the buffer
 * if the use count is 0. Otherwise, they come
 * from some other window.
 */
/* ARGSUSED */
int
usebuffer(int f GCC_UNUSED, int n GCC_UNUSED)
{
	register BUFFER *bp;
	register int	s;
	char		bufn[NBUFN];

	bufn[0] = EOS;
	if ((s=mlreply("Use buffer: ", bufn, sizeof(bufn))) != TRUE)
		return s;
	if ((bp=find_any_buffer(bufn)) == 0)	/* Try buffer */
		return FALSE;
	return swbuffer(bp);
}

/* switch back to the first buffer (i.e., ":rewind") */
/* ARGSUSED */
int
firstbuffer(int f GCC_UNUSED, int n GCC_UNUSED)
{
	int s = histbuff(TRUE,0);
	if (!global_g_val(GMDABUFF))
		last_bp = s ? curbp : 0;
	return s;
}

/* ARGSUSED */
int
nextbuffer(int f GCC_UNUSED, int n GCC_UNUSED)	/* switch to the next buffer in the buffer list */
{
	register BUFFER *bp;	/* eligible buffer to switch to*/
	register BUFFER *stopatbp;	/* eligible buffer to switch to*/

	if (global_g_val(GMDABUFF)) {	/* go backward thru buffer-list */
		stopatbp = NULL;
		while (stopatbp != bheadp) {
			/* get the last buffer in the list */
			bp = bheadp;
			while(bp != 0 && bp->b_bufp != stopatbp)
				bp = bp->b_bufp;
			/* if that one's invisible, back up and try again */
			if (b_is_invisible(bp))
				stopatbp = bp;
			else
				return swbuffer(bp);
		}
	} else {			/* go forward thru args-list */
		if ((stopatbp = curbp) == 0)
			stopatbp = find_nth_created(1);
		if (last_bp == 0)
			last_bp = find_b_hist(0);
		if (last_bp != 0) {
			for (bp = last_bp->b_bufp; bp; bp = bp->b_bufp) {
				if (b_is_argument(bp))
					return swbuffer(last_bp = bp);
			}
		}
		mlforce("[No more files to edit]");
	}
	/* we're back to the top -- they were all invisible */
	return swbuffer(stopatbp);
}

/* bring nbp to the top of the list, where curbp usually lives */
void
make_current(BUFFER *nbp)
{
	register BUFFER *bp;
#if OPT_PROCEDURES
	register BUFFER *ocurbp;

	ocurbp = curbp;
#endif

	TrackAlternate(nbp);

	if (!updating_list && global_g_val(GMDABUFF)) {
		if (nbp != bheadp) {	/* remove nbp from the list */
			bp = bheadp;
			while(bp != 0 && bp->b_bufp != nbp)
				bp = bp->b_bufp;
			bp->b_bufp = nbp->b_bufp;

			/* put it at the head */
			nbp->b_bufp = bheadp;

			bheadp = nbp;
		}
		curbp = bheadp;
	} else
		curbp = nbp;

#if OPT_PROCEDURES
	if (curbp != ocurbp) {
		run_buffer_hook();
	}
#endif
	curtabval = tabstop_val(curbp);
	curswval = shiftwid_val(curbp);

#if OPT_TITLE
	{
		char title[256];
		sprintf(title, "vile - %s", curbp->b_bname);
		TTtitle(title);
	}
#endif
}


int
swbuffer(register BUFFER *bp)	/* make buffer BP current */
{
	return swbuffer_lfl(bp, TRUE);
}

int
swbuffer_lfl(register BUFFER *bp, int lockfl)	/* make buffer BP current */
{
	int s = TRUE;

	if (!bp) {
		mlforce("BUG:  swbuffer passed null bp");
		return FALSE;
	}

	TRACE(("swbuffer(%s) nwnd=%d\n", bp->b_bname, bp->b_nwnd))
	if (curbp == bp
	 && curwp != 0
	 && DOT.l != 0
	 && curwp->w_bufp == bp) {  /* no switching to be done */

		if (!bp->b_active) /* on second thought, yes there is */
			goto suckitin;

		return TRUE;
	 }

	if (curbp) {
		/* if we'll have to take over this window, and it's the last */
		if (bp->b_nwnd == 0 && curbp->b_nwnd != 0 &&
					--(curbp->b_nwnd) == 0) {
			undispbuff(curbp,curwp);
		}
	}

	/* don't let make_current() call the hook -- there's
		more to be done down below */
	DisableBufferHook;
	make_current(bp);	/* sets curbp */
	EnableBufferHook;

	bp = curbp;  /* if running the bufhook caused an error, we may
				be in a different buffer than we thought
				we were going to */

	/* get it already on the screen if possible */
	if (bp->b_nwnd != 0)  { /* then it's on the screen somewhere */
		register WINDOW *wp = bp2any_wp(bp);
		if (!wp)
			mlforce("BUG: swbuffer: wp still NULL");
		curwp = wp;
		upmode();
#ifdef MDCHK_MODTIME
		(void)check_modtime( bp, bp->b_fname );
#endif
#if OPT_UPBUFF
		if (bp != find_BufferList())
			updatelistbuffers();
#endif
		run_buffer_hook();
		return (find_bp(bp) != 0);
	} else if (curwp == 0) {
		return FALSE;	/* we haven't started displaying yet */
	}

	/* oh well, suck it into this window */

	curwp->w_bufp  = bp;
	if (bp->b_nwnd++ == 0) {		/* First use.		*/
		register WINDOW *wp;
    suckitin:
		for_each_window(wp) {
			if (wp->w_bufp == bp)
				copy_traits(&(wp->w_traits), &(bp->b_wtraits));
		}
	}
	curwp->w_flag |= WFMODE|WFHARD;		/* Quite nasty.		*/

	if (bp->b_active != TRUE) {		/* buffer not active yet*/
		s = bp2readin(bp, lockfl);	/* read and activate it */
	}
#ifdef MDCHK_MODTIME
	else
		(void)check_modtime( bp, bp->b_fname );
#endif
	updatelistbuffers();
	run_buffer_hook();
	return s;
}

#if NEEDED
/* check to ensure any buffer that thinks it's displayed _is_ displayed */
void
buf_win_sanity(void)
{
	register BUFFER *bp;
	for_each_buffer(bp) {
	    if (bp->b_nwnd != 0)  { /* then it's on the screen somewhere */
		register WINDOW *wp = bp2any_wp(bp);
		if (!wp) {
		    dbgwrite("BUG: swbuffer 1: wp is NULL");
		}
	    }
	}
}
#endif

void
undispbuff(
register BUFFER *bp,
register WINDOW *wp)
{
	/* get rid of it completely if it's a scratch buffer,
		or it's empty and unmodified */
	if (b_is_scratch(bp)) {
		/* Save the location within the help-file */
		if (eql_bname(bp, HELP_BufName))
			help_at = line_no(bp, wp->w_dot.l);
		(void)zotbuf(bp);
	} else if ( global_g_val(GMDABUFF) && !b_is_changed(bp) &&
		    is_empty_buf(bp) && !ffexists(bp->b_fname)) {
		(void)zotbuf(bp);
	} else {  /* otherwise just adjust it off the screen */
		copy_traits(&(bp->b_wtraits), &(wp->w_traits));
	}
}

#if	!OPT_MAJORMODE
/* return true iff c-mode is active for this buffer */
static int
cmode_active(register BUFFER *bp)
{
	if (is_local_b_val(bp,MDCMOD))
		return is_c_mode(bp);
	else
		return (is_c_mode(bp) && has_C_suffix(bp));
}
#endif

/* return the correct tabstop setting for this buffer */
int
tabstop_val(register BUFFER *bp)
{
#if	OPT_MAJORMODE
	int i = b_val(bp, VAL_TAB);
#else
	int i = b_val(bp, (cmode_active(bp) ? VAL_C_TAB : VAL_TAB));
#endif
	if (i == 0) return 1;
	return i;
}

/* return the correct shiftwidth setting for this buffer */
int
shiftwid_val(register BUFFER *bp)
{
#if	OPT_MAJORMODE
	int i = b_val(bp, VAL_SWIDTH);
#else
	int i = b_val(bp, (cmode_active(bp) ? VAL_C_SWIDTH : VAL_SWIDTH));
#endif
	if (i == 0) return 1;
	return i;
}

#if	!OPT_MAJORMODE
int
has_C_suffix(register BUFFER *bp)
{
	int s;
	int save = ignorecase;
#if OPT_CASELESS
	ignorecase = TRUE;
#else
	ignorecase = FALSE;
#endif
	s =  regexec(global_g_val_rexp(GVAL_CSUFFIXES)->reg,
			bp->b_fname, (char *)0, 0, -1);
	ignorecase = save;
	return s;
}
#endif

/*
 * Dispose of a buffer, by name.  If this is a screen-command, pick the name up
 * from the screen.  Otherwise, ask for the name.  Look it up (don't get too
 * upset if it isn't there at all!).  Get quite upset if the buffer is being
 * displayed.  Clear the buffer (ask if the buffer has been changed).  Then
 * free the header line and the buffer header.
 *
 * If we are given a repeat-count, try to kill that many buffers.  Killing from
 * names selected from the buffer-list is a special case, because the cursor
 * may be pointing to the buffer-number column.  In that case, we must
 * recompute the buffer-contents.  Otherwise, move the cursor down one line
 * (staying in the same column), so we can pick up the names from successive
 * lines.
 */
int
killbuffer(int f, int n)
{
	register BUFFER *bp;
	register int	s;
	char bufn[NFILEN];

#if OPT_UPBUFF
	C_NUM	save_COL;
	MARK	save_DOT;
	MARK	save_TOP;
	int	animated = f
			&& (n > 1)
			&& (curbp != 0)
			&& (curbp == find_BufferList());
	int	special  = animated && (DOT.o == 2);

	if (animated && !special) {
		save_COL = getccol(FALSE);
		save_DOT = DOT;
		save_TOP = curwp->w_line;
	} else
		save_COL = 0;	/* appease gcc */
#endif

	if (!f)
		n = 1;
	for_ever {
		bufn[0] = EOS;
		if (clexec || isnamedcmd) {
			if ((s=mlreply("Kill buffer: ", bufn, sizeof(bufn))) != TRUE)
				break;
		} else if ((s = screen_to_bname(bufn)) != TRUE) {
			mlforce("[Nothing selected]");
			break;
		}

		if ((bp=find_any_buffer(bufn)) == 0) {	/* Try buffer */
			s = FALSE;
			break;
		}

		if ((curbp == bp) && (find_alt() == 0)) {
			mlforce("[Can't kill that buffer]");
			s = FALSE;
			break;
		}

		(void)strcpy(bufn, bp->b_bname); /* ...for info-message */
		if (bp->b_nwnd != 0)  { /* then it's on the screen somewhere */
			(void)zotwp(bp);
			if (find_bp(bp) == 0) { /* delwp must have zotted us */
				s = FALSE;
				break;
			}
		}
		if ((s = zotbuf(bp)) != TRUE)
			break;
		mlwrite("Buffer %s gone", bufn);
		if (--n > 0) {
#if OPT_UPBUFF
			if (special)
				(void)update(TRUE);
			else
#endif
			 if (!forwline(FALSE,1))
				break;
		} else
			break;
	}

#if OPT_UPBUFF
	if (animated && !special) {
		curgoal = save_COL;
		DOT     = save_DOT;
		DOT.o   = getgoal(DOT.l);
		curwp->w_line = save_TOP;
		curwp->w_flag &= (USHORT) ~WFMOVE;
	}
#endif
	return s;
}

/*
 * Unlink a buffer from the list, adjusting last-used and created counts.
 */
int
delink_bp(BUFFER *bp)
{
	register BUFFER *bp1, *bp2;

	bp1 = NULL;				/* Find the header.	*/
	bp2 = bheadp;
	while (bp2 != bp) {
		bp1 = bp2;
		bp2 = bp2->b_bufp;
		if (bp2 == 0)
			return FALSE;
	}
	bp2 = bp2->b_bufp;			/* Next one in chain.	*/
	if (bp1 == NULL)			/* Unlink it.		*/
		bheadp = bp2;
	else
		bp1->b_bufp = bp2;
	MarkUnused(bp);
	MarkDeleted(bp);
	if (bp == last_bp)
		last_bp = 0;
	return TRUE;
}

int
zotbuf(register BUFFER *bp)	/* kill the buffer pointed to by bp */
{
	register int	s;
	register int	didswitch = FALSE;

	if (find_bp(bp) == 0) 	/* delwp may have zotted us, pointer obsolete */
		return TRUE;

	TRACE(("zotbuf(%s)\n", bp->b_bname))

#define no_del
#ifdef no_del
	if (bp->b_nwnd != 0) {			/* Error if on screen.	*/
		mlforce("[Buffer is being displayed]");
		return (FALSE);
	}
#else
	if (curbp == bp) {
		didswitch = TRUE;
		if (find_alt() == 0) {
			mlforce("[Can't kill that buffer]");
			return FALSE;
		}
	}
	if (bp->b_nwnd != 0)  { /* then it's on the screen somewhere */
		(void)zotwp(bp);
		if (find_bp(bp) == 0) /* delwp must have zotted us */
			return TRUE;
	}

#endif
#if OPT_LCKFILES
	/* If Buffer is killed and not locked by other then release own lock */
	if ( global_g_val(GMDUSEFILELOCK) ) {
		if ( bp->b_active )
			 if (!b_val (curbp, MDLOCKED) && !b_val (curbp, MDVIEW))
				release_lock(bp->b_fname);
	}
#endif
	/* Blow text away.	*/
	if ((s=bclear(bp)) != TRUE) {
		/* the user must have answered no */
		if (didswitch)
			(void)swbuffer(bp);
	} else {
		FreeBuffer(bp);
		updatelistbuffers();
	}
	return (s);
}

/* ARGSUSED */
int
namebuffer(int f GCC_UNUSED, int n GCC_UNUSED)	/*	Rename the current buffer	*/
{
	register BUFFER *bp;	/* pointer to scan through all buffers */
	static char bufn[NBUFN];	/* buffer to hold buffer name */
	const char *prompt = "New name for buffer: ";

	/* prompt for and get the new buffer name */
	do {
		if (mlreply(prompt, bufn, sizeof(bufn)) != TRUE)
			return(FALSE);
		if (*mktrimmed(bufn) == EOS)
			return(FALSE);
		prompt = "That name's been used.  New name: ";
		bp = find_b_name(bufn);
		if (bp == curbp)	/* no change */
			return(FALSE);
	} while (bp != 0);

	set_bname(curbp, bufn);		/* copy buffer name to structure */
	curwp->w_flag |= WFMODE;	/* make mode line replot */
	b_clr_scratch(curbp);		/* if renamed, we must want it */
	mlerase();
	updatelistbuffers();
	return(TRUE);
}

/* create or find a window, and stick this buffer in it.  when
	done, we own the window and the buffer, but they are _not_
	necessarily curwp and curbp */
int
popupbuff(BUFFER *bp)
{
	register WINDOW *wp;

	if (!curbp) {
		curbp = bp;  /* possibly at startup time */
		curwp->w_bufp = curbp;
		++curbp->b_nwnd;
	} else if (bp->b_nwnd == 0) {	/* Not on screen yet. */
		if ((wp = wpopup()) == NULL)
			return FALSE;
		if (--wp->w_bufp->b_nwnd == 0)
			undispbuff(wp->w_bufp,wp);
		wp->w_bufp  = bp;
		++bp->b_nwnd;
	}

	for_each_window(wp) {
		if (wp->w_bufp == bp) {
			wp->w_line.l = lforw(buf_head(bp));
			wp->w_dot.l  = lforw(buf_head(bp));
			wp->w_dot.o  = 0;
#if WINMARK
			wp->w_mark = nullmark;
#endif
			wp->w_lastdot = nullmark;
			wp->w_values = global_w_values;
			wp->w_flag |= WFMODE|WFHARD;
		}
	}
	return swbuffer(bp);
}

/*
 * Invoked after changing mode 'autobuffer', this relinks the list of buffers
 * sorted according to the mode: by creation or last-used order.
 */
void
sortlistbuffers(void)
{
	register BUFFER *bp, *newhead = 0;
	register int	c;

	if (global_g_val(GMDABUFF)) {
		c = 1;
		while ((bp = find_nth_used(c++)) != 0) {
			bp->b_relink = newhead;
			newhead = bp;
		}
	} else {
		c = countBuffers();
		while ((bp = find_nth_created(c--)) != 0) {
			bp->b_relink = newhead;
			newhead = bp;
		}
	}

	for (bp = newhead; bp; bp = bp->b_relink)
		bp->b_bufp = bp->b_relink;
	bheadp = newhead;

	updatelistbuffers();
}

/*
 * List all of the active buffers.  First update the special buffer that holds
 * the list.  Next make sure at least 1 window is displaying the buffer list,
 * splitting the screen if this is what it takes.  Lastly, repaint all of the
 * windows that are displaying the list.  A numeric argument forces it to
 * toggle the listing invisible buffers as well (a subsequent argument forces
 * it to a normal listing).
 */
int
togglelistbuffers(int f, int n)
{
	int status;
	register BUFFER *bp;

	/* if it doesn't exist, create it */
	if ((bp = find_BufferList()) == NULL) {
		status = listbuffers(f,n);
	/* if user supplied argument, re-create */
	} else if (f) {
		status = listbuffers(!show_all,n);
	} else {
		/* if it does exist, delete the window, which in turn
		   will kill the BFSCRTCH buffer */
		status = zotwp(bp);
	}

	return status;
}

/*
 * Track/emit footnotes for 'makebufflist()', showing only the ones we use.
 */
#if !SMALLER
static void
footnote(int c)
{
	static	struct	{
		const char *name;
		int	flag;
	} table[] = {
		{"automatic",	0},
		{"invisible",	0},
		{"modified",	0},
		{"scratch",	0},
		{"unread",	0},
		};
	register SIZE_T	j, next;

	for (j = next = 0; j < TABLESIZE(table); j++) {
		if (c != 0) {
			if (table[j].name[0] == c) {
				bputc(c);
				table[j].flag = TRUE;
				break;
			}
		} else if (table[j].flag) {
			bprintf("%s '%c'%s %s",
				next ? "," : "notes:",	table[j].name[0],
				next ? ""  : " is",	table[j].name);
			next++;
			table[j].flag = 0;
		}
	}
	if (next)
		bputc('\n');
}
#define	MakeNote(c)	footnote(c)
#define	ShowNotes()	footnote(0)
#else
#define	MakeNote(c)	bputc(c)
#define	ShowNotes()
#endif

/*
 * This routine rebuilds the text in the buffer that holds the buffer list.  It
 * is called by the list buffers command.  Return TRUE if everything works.
 * Return FALSE if there is an error (if there is no memory).  The variable
 * 'show_all' (set if the command had a repeat-count) indicates whether to list
 * hidden buffers.
 */
/* ARGSUSED */
static void
makebufflist(
	int iflag GCC_UNUSED,	/* list hidden buffer flag */
	void *dummy GCC_UNUSED)
{
	register BUFFER *bp;
	LINEPTR curlp;		/* entry corresponding to buffer-list */
	int nbuf = 0;		/* no. of buffers */
	int this_or_that;

	curlp = null_ptr;

	if (this_bp == 0)
		this_bp = curbp;
	if (this_bp == that_bp)
		that_bp = find_alt();

	bprintf("      %7s %*s %s\n", "Size",NBUFN-1,"Buffer name","Contents");
	bprintf("      %7p %*p %30p\n", '-',NBUFN-1,'-','-');

	/* output the list of buffers */
	for_each_buffer(bp) {
		/* skip those buffers which don't get updated when changed */
#if	OPT_UPBUFF
		if (!update_on_chg(bp)) {
			continue;
		}
#endif

		/* output status flag (e.g., has the file been read in?) */
		if (b_is_scratch(bp))
			MakeNote('s');
		else if (!(bp->b_active))
			MakeNote('u');
		else if (b_is_implied(bp))
			MakeNote('a');
		else if (b_is_invisible(bp))
			MakeNote('i');
		else if (b_is_changed(bp))
			MakeNote('m');
		else
			bputc(' ');

		this_or_that = (bp == this_bp)
			? EXPC_THIS
			: (bp == that_bp)
				? EXPC_THAT
				: ' ';

		if (b_is_temporary(bp))
			bprintf("   %c ", this_or_that);
		else
			bprintf(" %2d%c ", nbuf++, this_or_that);

		(void)bsizes(bp);
		bprintf("%7ld %*S ", bp->b_bytecount, NBUFN-1, bp->b_bname );
		{
			char	temp[NFILEN];
			char	*p;

			if ((p = bp->b_fname) != 0)
				p = shorten_path(strcpy(temp, p), TRUE);

			if (p != 0)
				bprintf("%s",p);
		}
		bprintf("\n");
		if (bp == curbp)
			curlp = lback(lback(buf_head(curbp)));
	}
	ShowNotes();
	bprintf("             %*s %s", NBUFN-1, "Current dir:",
		current_directory(FALSE));

	/* show the actual size of the buffer-list */
	if (curlp != null_ptr) {
		char	temp[20];
		(void)bsizes(curbp);
		(void)lsprintf(temp, "%7ld", curbp->b_bytecount);
		(void)memcpy(curlp->l_text + 6, temp,
		             (SIZE_T)strlen(temp));
	}
}

#if	OPT_UPBUFF
/*
 * (Re)compute the contents of the buffer-list.  Use the flag 'updating_list'
 * as a semaphore to avoid adjusting the last used/created indices while
 * cycling over the list of buffers.
 */
/*ARGSUSED*/
static int
show_BufferList(BUFFER *bp GCC_UNUSED)
{
	register int	status;
	if ((status = (!updating_list++ != 0)) != FALSE) {
		this_bp = curbp;
		that_bp = find_alt();
		status = liststuff(BUFFERLIST_BufName, FALSE, 
				makebufflist, 0, (void *)0);
	}
	updating_list--;
	return status;
}

/*
 * If the list-buffers window is visible, update it after operations that
 * would modify the list.
 */
void
updatelistbuffers(void)
{
	update_scratch(BUFFERLIST_BufName, show_BufferList);
}

/* mark a scratch/temporary buffer for update */
void
update_scratch(const char *name, int (*func)(BUFFER *))
{
	register BUFFER *bp = find_b_name(name);

	if (bp != 0) {
		bp->b_upbuff = func;
		b_set_obsolete(bp);
	}
}
#endif	/* OPT_UPBUFF */

/* ARGSUSED */
int
listbuffers(int f GCC_UNUSED, int n GCC_UNUSED)
{
#if	OPT_UPBUFF
	register int status;

	show_all = f;	/* save this to use in automatic updating */
	if (find_BufferList() != 0) {
		updatelistbuffers();
		return TRUE;
	}
	this_bp = 0;
	that_bp = curbp;
	status  = liststuff(BUFFERLIST_BufName, FALSE, 
				makebufflist, 0, (void *)0);
	b_clr_obsolete(curbp);
	return status;
#else
	show_all = f;
	this_bp = 0;
	that_bp = curbp;
	return liststuff(BUFFERLIST_BufName, FALSE, 
				makebufflist, 0, (void *)0);
#endif
}

/*
 * The argument "text" points to
 * a string. Append this line to the
 * buffer. Handcraft the EOL
 * on the end. Return TRUE if it worked and
 * FALSE if you ran out of room.
 */
int
addline(register BUFFER *bp, const char *text, int len)
{
	if (add_line_at (bp, lback(buf_head(bp)), text, len) == TRUE) {
		/* If "." is at the end, move it to new line  */
		if (sameline(bp->b_dot, bp->b_line))
			bp->b_dot.l = lback(buf_head(bp));
		return TRUE;
	}
	return FALSE;
}

/*
 * Add a LINE filled with the given text after the specified LINE.
 */
int
add_line_at (
register BUFFER	*bp,
LINEPTR	prevp,
const char *text,
int	len)
{
	register LINEPTR newlp;
	register LINEPTR nextp;
	register LINE *	lp;
	register int	ntext;

	nextp = lforw(prevp);
	ntext = (len < 0) ? strlen(text) : len;
	newlp = lalloc(ntext, bp);
	if (newlp == null_ptr)
		return (FALSE);

	lp = newlp;
	if (ntext > 0)
		(void)memcpy(lp->l_text, text, (SIZE_T)ntext);

	/* try to maintain byte/line counts? */
	if (b_is_counted(bp)) {
		if (nextp == buf_head(bp)) {
			make_local_b_val(bp,MDNEWLINE);
			set_b_val(bp, MDNEWLINE, TRUE);
			bp->b_bytecount += (ntext+1);
			bp->b_linecount += 1;
#if !SMALLER		/* tradeoff between codesize & data */
			lp->l_number = bp->b_linecount;
#endif
		} else
			b_clr_counted(bp);
	}

	set_lforw(prevp, newlp);	/* link into the buffer */
	set_lback(newlp, prevp);
	set_lback(nextp, newlp);
	set_lforw(newlp, nextp);

	return (TRUE);
}

/*
 * Look through the list of
 * buffers. Return TRUE if there
 * are any changed buffers. Buffers
 * that hold magic internal stuff are
 * not considered; who cares if the
 * list of buffer names is hacked.
 * Return FALSE if no buffers
 * have been changed.
 * Return a pointer to the first changed buffer,
 * in case there's only one -- it's useful info
 * then.
 */
int
any_changed_buf(BUFFER **bpp)
{
	register BUFFER *bp;
	register int cnt = 0;
	if (bpp) *bpp = NULL;

	for_each_buffer(bp) {
		if (!b_is_invisible(bp) && b_is_changed(bp)) {
		    	if (bpp && !*bpp)
				*bpp = bp;
			cnt++;
		}
	}
	return (cnt);
}

/* similar to above, for buffers that have not been visited */
int
any_unread_buf(BUFFER **bpp)
{
	register BUFFER *bp;
	register int cnt = 0;
	if (bpp) *bpp = NULL;

	for_each_buffer(bp) {
		if (!b_is_invisible(bp) && !bp->b_active) {
		    	if (bpp && !*bpp)
				*bpp = bp;
			cnt++;
		}
	}
	return (cnt);
}

/*
 * Copies string to a buffer-name, trimming trailing blanks for consistency.
 */
void
set_bname(BUFFER *bp, const char *name)
{
	register int j, k;
	register char *d;

	(void)strncpy0(bp->b_bname, name, NBUFN);

	d = bp->b_bname;
	for (j = 0, k = -1; d[j]; j++) {
		if (!isSpace(d[j]))
			k = -1;
		else if (k < 0)
			k = j;
	}
	if (k >= 0)
		d[k] = EOS;
}

#if BEFORE
/*
 * Copies buffer-name to a null-terminated buffer (for use in code that
 * cannot conveniently use the name without a null-termination).
 */
char *
XXX still needed XXX get_bname(BUFFER *bp)
{
	static	char	bname[NBUFN];
	if (bp) {
	    (void)strncpy0(bname, bp->b_bname, NBUFN);
	    bname[NBUFN] = EOS;
	} else {
	    *bname = EOS;
	}
	return bname;
}
#endif

/*
 * Look for a buffer-name in the buffer-list.  This assumes that the argument
 * is null-terminated.  If the length is longer than will fit in a b_bname
 * field, then it is probably a filename.
 */
BUFFER *
find_b_name(const char *bname)
{
	register BUFFER *bp;
	BUFFER	temp;

	set_bname(&temp, bname); /* make a canonical buffer-name */

	for_each_buffer(bp)
		if (eql_bname(bp, temp.b_bname))
			return bp;
	return 0;
}

/*
 * Find a buffer, by name. Return a pointer
 * to the BUFFER structure associated with it.
 * If the buffer is not found, create it. The "bflag" is
 * the settings for the flags in in buffer.
 */
BUFFER	*
bfind(const char *bname, int bflag)
{
	register BUFFER *bp;
	register LINEPTR lp;
	register BUFFER *lastb = NULL;	/* buffer to insert after */
	register BUFFER *bp2;

	for_each_buffer(bp) {
		if (eql_bname(bp, bname))
			return (bp);
		lastb = bp;
	}

	/* set everything to 0's unless we want nonzero */
	if ((bp = typecalloc(BUFFER)) == NULL) {
		(void)no_memory("BUFFER");
		return (NULL);
	}

	/* set this first, to make it simple to trace */
	set_bname(bp, bname);

	lp = lalloc(0, bp);
	if (lp == null_ptr) {
		free((char *) bp);
		(void)no_memory("BUFFER head");
		return (NULL);
	}

	/* and set up the other buffer fields */
	bp->b_values = global_b_values;
	bp->b_wtraits.w_vals = global_w_values;
	bp->b_active = FALSE;
	bp->b_dot.l  = lp;
	bp->b_dot.o  = 0;
	bp->b_wline  = bp->b_dot;
	bp->b_line   = bp->b_dot;
#if WINMARK
	bp->b_mark = nullmark;
#endif
	bp->b_lastdot = nullmark;
#if OPT_VIDEO_ATTRS
#endif
	bp->b_flag  = bflag;
	bp->b_acount = b_val(bp, VAL_ASAVECNT);
	bp->b_fname = NULL;
	ch_fname(bp, "");
#if	OPT_ENCRYPT
	if (!b_is_temporary(bp)
	 && cryptkey != 0 && *cryptkey != EOS) {
		(void)strcpy(bp->b_key, cryptkey);
		make_local_b_val(bp, MDCRYPT);
		set_b_val(bp, MDCRYPT, TRUE);
	} else
		bp->b_key[0] = EOS;
#endif
	bp->b_udstks[0] = bp->b_udstks[1] = null_ptr;
	bp->b_ulinep = null_ptr;
	bp->b_udtail = null_ptr;
	bp->b_udlastsep = null_ptr;

	b_set_counted(bp);	/* buffer is empty */
	set_lforw(lp, lp);
	set_lback(lp, lp);

	/* append at the end */
	if (lastb)
		lastb->b_bufp = bp;
	else
		bheadp = bp;
	bp->b_bufp = NULL;
	bp->b_created = countBuffers();

	for_each_buffer(bp2)
		bp2->b_last_used += 1;
	bp->b_last_used = 1;

#if OPT_PERL || OPT_TCL
	bp->b_api_private = 0;
#endif

	return (bp);
}

/*
 * Given a filename, set up a buffer pointer to correspond
 */
BUFFER *
make_bp (const char *fname, int flags)
{
	BUFFER *bp;
	char bname[NBUFN];

	makename(bname, fname);
	unqname(bname);

	if ((bp = bfind(bname, flags)) != 0)
		ch_fname(bp, fname);
	return bp;
}

/*
 * This routine blows away all of the text
 * in a buffer. If the buffer is marked as changed
 * then we ask if it is ok to blow it away; this is
 * to save the user the grief of losing text. The
 * window chain is nearly always wrong if this gets
 * called; the caller must arrange for the updates
 * that are required. Return TRUE if everything
 * looks good.
 */
int
bclear(register BUFFER *bp)
{
	register LINEPTR lp;

	if (!b_is_temporary(bp)		/* Not invisible or scratch */
	 &&  b_is_changed(bp)) {	/* Something changed	*/
		char ques[30+NBUFN];
		(void)strcat(strcpy(ques,"Discard changes to "), bp->b_bname);
		if (mlyesno(ques) != TRUE)
			return FALSE;
	}
#if OPT_UPBUFF
	if (bp->b_rmbuff != 0)
		(bp->b_rmbuff)(bp);
#endif
	b_clr_changed(bp);		/* Not changed		*/

	freeundostacks(bp,TRUE);	/* do this before removing lines */
	FreeAndNull(bp->b_nmmarks);     /* free the named marks */
#if OPT_SELECTIONS
	free_attribs(bp);
#endif

	while ((lp=lforw(buf_head(bp))) != buf_head(bp)) {
		lremove(bp,lp);
		lfree(lp,bp);
	}

	if (bp->b_ulinep != null_ptr) {
		lfree(bp->b_ulinep, bp);
		bp->b_ulinep = null_ptr;
	}
	FreeAndNull(bp->b_ltext);
	bp->b_ltext_end = NULL;

	FreeAndNull(bp->b_LINEs);
	bp->b_LINEs_end = NULL;

	bp->b_freeLINEs = NULL;

	bp->b_dot  = bp->b_line;	/* Fix "."		*/
#if WINMARK
	bp->b_mark = nullmark;		/* Invalidate "mark"	*/
#endif
	bp->b_lastdot = nullmark;	/* Invalidate "mark"	*/

	b_set_counted(bp);
	bp->b_bytecount = 0;
	bp->b_linecount = 0;

	free_local_vals(b_valnames, global_b_values.bv, bp->b_values.bv);

	return (TRUE);
}

/*
 * Update the counts for the # of bytes and lines, to use in various display
 * commands.
 */
int
bsizes(BUFFER *bp)
{
	register LINE	*lp;		/* current line */
	register B_COUNT numchars = 0;	/* # of chars in file */
	register L_NUM   numlines = 0;	/* # of lines in file */

	if (b_is_counted(bp))
		return FALSE;

	/* count chars and lines */
	for_each_line(lp,bp) {
		++numlines;
		numchars += llength(lp) + 1;
#if !SMALLER	/* tradeoff between codesize & data */
		lp->l_number = numlines;
#endif
	}
	if (!b_val(bp,MDNEWLINE))
		numchars--;

	bp->b_bytecount = numchars;
	bp->b_linecount = numlines;
	b_set_counted(bp);
	return TRUE;
}

/*
 * Mark a buffer 'changed'
 */
void
chg_buff(register BUFFER *bp, register USHORT flag)
{
	register WINDOW *wp;

	b_clr_counted(bp);
	b_match_attrs_dirty(bp);
	if (bp->b_nwnd != 1)		/* Ensure hard.		*/
		flag |= WFHARD;
	if (!b_is_changed(bp)) {	/* First change, so	*/
		flag |= WFMODE;		/* update mode lines.	*/
		b_set_changed(bp);
	}
#if	OPT_UPBUFF
	if (update_on_chg(bp))
		updatelistbuffers();
#endif
	for_each_window(wp)
		if (wp->w_bufp == bp) {
			wp->w_flag |= flag;
#ifdef WMDLINEWRAP
			/* The change may affect the line-height displayed
			 * on the screen.  Assume the worst-case.
			 */
			if ((flag & WFEDIT) && w_val(wp,WMDLINEWRAP))
				wp->w_flag |= WFHARD;
#endif
		}
}

/*
 * Mark a buffer 'unchanged'
 */
void
unchg_buff(register BUFFER *bp, register USHORT flag)
{
	register WINDOW *wp;

	if (b_is_changed(bp)) {
		if (bp->b_nwnd != 1)		/* Ensure hard.		*/
			flag |= WFHARD;
		flag |= WFMODE;			/* update mode lines.	*/
		b_clr_changed(bp);
		b_clr_counted(bp);
		b_match_attrs_dirty(bp);

		for_each_window(wp) {
			if (wp->w_bufp == bp)
				wp->w_flag |= flag;
		}
#if	OPT_UPBUFF
		if (update_on_chg(bp))
			updatelistbuffers();
#endif
	}
}

/* ARGSUSED */
int
unmark(int f GCC_UNUSED, int n GCC_UNUSED)	/* unmark the current buffers change flag */
{
	unchg_buff(curbp, 0);
	return (TRUE);
}

/* write all _changed_ buffers */
/* if you get the urge to tinker in here, bear in mind that you need
   to test:  :ww, 1:ww, :wwq, 1:wwq, all with 1 buffer, both modified and
   unmodified, with 2 buffers (0, 1, or both modified), and with errors,
   like either buffer not writeable.  oh yes -- you also need to turn
   on "autowrite", and try ^Z and :!cmd.

   by default, we should have scrolling messages, a pressreturn call, and
   a screen update.

   any numeric argument will suppress the pressreturn call, and therefore
   also the scrolling of the messages and screen update.

   if you're leaving (quitting, or suspending to the shell, you want the
   scrolling messages, but no press-return, and no update.

   if there's a failure, you want a pressreturn, and an update, and you
   need to return non-TRUE so you won't try to quit.  we also switch to
   the buffer that failed in that case.
*/
int
writeallchanged(int f, int n)
{
	return writeall(f,n,!f,FALSE,FALSE);
}

int
writeall(int f, int n, int promptuser, int leaving, int autowriting)
{
	register BUFFER *bp;	/* scanning pointer to buffers */
	register BUFFER *oldbp; /* original current buffer */
	register int status = TRUE;
	int count = 0;
	int failure = FALSE;
	int dirtymsgline = FALSE;

	oldbp = curbp;				/* save in case we fail */

	for_each_buffer(bp) {
		if (autowriting && !b_val(bp,MDAUTOWRITE))
			continue;
		if (b_is_changed(bp) && !b_is_invisible(bp)) {
			make_current(bp);
			if (dirtymsgline && (promptuser || leaving)) {
				mlforce("\n");
				dirtymsgline = FALSE;
			}
			status = filesave(f, n);
			dirtymsgline = TRUE;
			failure = (status != TRUE);
			if (failure) {
				mlforce("\n");
				mlforce("[Save of %s failed]",bp->b_fname);
				/* dirtymsgline still TRUE */
				break;
			}
			count++;
		}
	}

	/* shortcut out */
	if (autowriting && !failure && !count)
		return TRUE;
	
	/* do we want a press-return message?  */
	if (failure || ((promptuser && !leaving) && count)) {
		if (dirtymsgline)
			mlforce("\n");
		dirtymsgline = FALSE;
		pressreturn();
	}

	/* get our old buffer back */
	make_current(oldbp);

	/* and then switch to the failed buffer */
	if (failure && !insertmode /* can happen from ^Z and autowrite */ )
		swbuffer(bp);

	/* if we asked "press-return", then we certainly need an update */
	if (failure || ((promptuser && !leaving) && count)) {
		sgarbf = TRUE;
		(void)update(TRUE);
	} else if (dirtymsgline && (promptuser || leaving)) {
		mlforce("\n");
	}

	/* final message -- appears on the message line after the update,
	  or as the last line before the post-quit prompt */
	if (count)
		mlforce("[Wrote %d buffer%s]", count, PLURAL(count));
	else
		mlforce("[No buffers written]");

	if (dirtymsgline) 
		sgarbf = TRUE;

	return status;
}

/* For memory-leak testing (only!), releases all buffer storage. */
#if	NO_LEAKS
void	bp_leaks(void)
{
	register BUFFER *bp;

	if (bminip != 0)
		FreeBuffer(bminip);
	while ((bp = bheadp) != 0) {
		b_clr_changed(bp);	/* discard any changes */
		bclear(bheadp);
		FreeBuffer(bheadp);
	}
}
#endif
