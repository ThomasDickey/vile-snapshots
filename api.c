/*
 * api.c -- (roughly) nvi's api to perl and tcl
 *
 * Status of this file:  Some of the functions in this file are unused.
 * Early on, I was trying for compatibility with nvi's perl interface.
 * Now that I'm not, I've gotten lazy and have occasionally been skipping
 * this layer.
 *
 * Some of the code in this file is still used and is, in fact, very
 * important.  There's other code which may simply be removed.
 *
 * OTOH, if someone ever does a TCL interface (it probably won't
 * be me -- I really like Perl and would probably lack motivation to
 * do a proper job of it), some of the as now unused stuff might come
 * in handy.
 *				- kev 4/7/1998
 *
 * $Id: api.c,v 1.56 2025/01/26 16:31:43 tom Exp $
 */

#include "estruct.h"

#if OPT_PERL || OPT_PLUGIN

#include "edef.h"
#include "api.h"

WINDOW *curwp_visible;

static void propagate_dot(void);

/* Maybe this should go in line.c ? */
static int
linsert_chars(char *s, int len)
{
    /* I wrote the code this way both for efficiency reasons and
     * so that the MARK denoting the end of the range wouldn't
     * be moved to one of the newly inserted lines.
     */
    int nlcount = 0;
    int nlatend = 0;
    int nlsuppress = 0;
    int mdnewline = b_val(curbp, MDNEWLINE);

    if (len <= 0)
	return 0;

    if (DOT.l == buf_head(curbp)) {
	if (!mdnewline) {
	    DOT.l = lback(DOT.l);
	    DOT.o = llength(DOT.l);
	}
	if (s[len - 1] == '\n') {
	    if (len > 1)
		nlsuppress = TRUE;
	    if (!mdnewline) {
		set_local_b_val(curbp, MDNEWLINE, TRUE);
	    }
	} else {
	    if (mdnewline) {
		set_local_b_val(curbp, MDNEWLINE, FALSE);
	    }
	}
    }

    if (s[len - 1] == '\n') {
	if (!mdnewline
	    && lforw(DOT.l) == buf_head(curbp)
	    && DOT.o == llength(DOT.l)) {
	    nlsuppress = TRUE;
	    set_local_b_val(curbp, MDNEWLINE, TRUE);
	}
	if (!nlsuppress) {

	    /* We implicitly get a newline by inserting anything at
	     * the head of a buffer (which includes the empty buffer
	     * case).  So if we actually insert a newline, we'll end
	     * up getting two of them.  (Which is something we don't
	     * want unless MDNEWLINE is TRUE.)
	     */
	    lnewline();
	    nlcount++;
	    DOT.l = lback(DOT.l);	/* back up DOT to the newly */
	    DOT.o = llength(DOT.l);	/* inserted line */
	}
	nlatend = 1;
	len--;
    }

    while (len > 0) {
	if (*s == '\n') {
	    lnewline();
	    nlcount++;
	    s++;
	    len--;
	} else {
	    int i;
	    /* Find next newline so we can do block insert; We
	     * start at 1, because we know the first character
	     * can't be a newline.
	     */
	    for (i = 1; i < len && s[i] != '\n'; i++) ;
	    lins_bytes(i, *s);
	    if (i > 1) {
		memcpy(lvalue(DOT.l) + DOT.o - i + 1, s + 1, (size_t) (i - 1));
	    }
	    len -= i;
	    s += i;
	}
    }

    if (nlatend) {
	/* Advance DOT to where it's supposed to be */
	dot_next_bol();
    }

    return nlcount;
}

/* Another candidate for line.c */
static int
lreplace(char *s, int len)
{
    LINE *lp = DOT.l;
    int i;
    char *t;
    WINDOW *wp;

    CopyForUndo(lp);

    DOT.o = b_left_margin(curbp);

    if (len > (int) lp->l_size) {
	size_t nlen;
	char *ntext;

#define roundlenup(n) ((n+NBLOCK-1) & ~(NBLOCK-1))

	nlen = (size_t) roundlenup(len);
	ntext = castalloc(char, nlen);
	if (ntext == NULL)
	    return FALSE;
	if (lvalue(lp))
	    ltextfree(lp, curbp);
	lvalue(lp) = ntext;
	lp->l_size = nlen;
#if OPT_LINE_ATTRS
	FreeAndNull(lp->l_attrs);
#endif
    }

    llength(lp) = len;

    /* FIXME: Handle embedded newlines */
    for (i = len - 1, t = lvalue(lp); i >= 0; i--)
	t[i] = s[i];

    /* We'd normally need a call to chg_buff here, but I don't want
       to pay the price.

       BUT...

       We do call chg_buff before returning to vile.  See
       api_command_cleanup below.
     */

    for_each_window(wp) {
	if (wp->w_dot.l == lp && wp->w_dot.o > len)
	    wp->w_dot.o = len;
	if (wp->w_lastdot.l == lp && wp->w_lastdot.o > len)
	    wp->w_lastdot.o = len;
    }
    do_mark_iterate(mp, {
	if (mp->l == lp && mp->o > len)
	    mp->o = len;
    });
#if OPT_LINE_ATTRS
    FreeAndNull(lp->l_attrs);
#endif

    return TRUE;
}

void
api_setup_fake_win(VileBuf * vbp, int do_delete)
{
    if (vbp != NULL) {
	if (curwp_visible == NULL)
	    curwp_visible = curwp;

	if (vbp->fwp) {
	    curwp = vbp->fwp;
	    curbp = curwp->w_bufp;
	} else {
	    (void) push_fake_win(vbp->bp);
	    vbp->fwp = curwp;
	    vbp->changed = 0;
	}

	if (vbp->ndel != 0 && do_delete) {
	    int status;
	    /* Do lazy delete; FALSE means don't put text in kill buffer */
	    status = ldel_bytes(vbp->ndel, FALSE);
	    vbp->ndel = 0;
	    if (status == FALSE
		|| (lforw(DOT.l) == buf_head(curbp) && DOT.o >= llength(DOT.l))) {
		set_local_b_val(curbp, MDNEWLINE, FALSE);
	    }
	}
    }
}

/* I considered three possible solutions for preventing stale
   regions from being used.  Here they are:

    1) Validate the region in question.  I.e, make sure that
       the region start and end lines are still in the buffer.
       Set the region to reasonable defaults if not.

       The problem with this approach is that we need something
       else to hold the end marker of the region while we're
       working on it.  One possible solution is to use the
       per-buffer w_lastdot field.  But I'm not entirely sure
       this is safe.

    2) Make the region and attributed region and put it on
       the attribute list.  The marks representing the
       ends of the region will certainly be updated correctly
       if this is done.  The downside is that we'd have to
       beef up the code which deals with attributes to allow
       external pointers to the attributes.  It wouldn't
       do for one of the attribute lists to be freed up with
       one of our VileBuf structures still pointer to one of the
       structures.

    3) Make line.c and undo.c aware of the VileBuf regions.
       This solution has no serious downside aside from
       further bulking up the files in question.

    I chose option 3 since I was able to bury the work
    inside do_mark_iterate (see estruct.h).  The following
    function (api_mark_iterator) is the helper function
    found in do_mark_iterate.
*/

MARK *
api_mark_iterator(BUFFER *bp, int *iterp)
{
    MARK *mp = NULL;
    VileBuf *vbp = bp2vbp(bp);

    if (vbp != NULL) {
	switch (*iterp) {
	case 0:
	    mp = &vbp->region.r_orig;
	    break;
	case 1:
	    mp = &vbp->region.r_end;
	    break;
	default:
	    break;
	}
	(*iterp)++;
    }

    return mp;
}

/*
 * This is a variant of gotoline in basic.c.  It differs in that
 * it attempts to use the line number information to more efficiently
 * find the line in question.  It will also position DOT at the beginning
 * of the line.
 */
int
api_gotoline(VileBuf * vbp, int lno)
{
#if !SMALLER
    int count;
    LINE *lp;
    BUFFER *bp = vbp->bp;

    if (!b_is_counted(bp))
	bsizes(bp);

    count = lno - DOT.l->l_number;
    lp = DOT.l;

    while (count < 0 && lp != buf_head(bp)) {
	lp = lback(lp);
	count++;
    }
    while (count > 0) {
	lp = lforw(lp);
	count--;
	if (lp == buf_head(bp))
	    break;
    }

    DOT.o = b_left_margin(curbp);

    if (lp != buf_head(bp) && lno == lp->l_number) {
	DOT.l = lp;
	return TRUE;
    } else {
	DOT.l = buf_head(bp);
	return FALSE;
    }

#else
    return vl_gotoline(lno);
#endif
}

int
api_aline(VileBuf * vbp, int lno, char *line, int len)
{
    api_setup_fake_win(vbp, TRUE);

    if (lno >= 0 && lno < vl_line_count(vbp->bp)) {
	api_gotoline(vbp, lno + 1);
	linsert_chars(line, len);
	lnewline();
    } else {			/* append to the end */
	gotoeob(FALSE, 0);
	gotoeol(FALSE, 0);
	lnewline();
	linsert_chars(line, len);
    }

    return TRUE;
}

int
api_dotinsert(VileBuf * vbp, char *text, int len)
{

    /* Set up the fake window; but we'll do any pending deletes
       ourselves. */
    api_setup_fake_win(vbp, FALSE);

    /* FIXME: Check to see if the buffer needs to be modified at all.
       We'll save our undo space better this way.  We'll also be able
       to better preserve the user's marks. */

    linsert_chars(text, len);
    if (vbp->ndel) {
	int status;
	status = ldel_bytes(vbp->ndel, FALSE);
	vbp->ndel = 0;
	if (status == FALSE
	    || (lforw(DOT.l) == buf_head(curbp) && DOT.o >= llength(DOT.l))) {
	    set_local_b_val(curbp, MDNEWLINE, FALSE);
	}
    }
    vbp->changed = 1;
    return TRUE;
}

int
api_dline(VileBuf * vbp, int lno)
{
    int status = TRUE;

    api_setup_fake_win(vbp, TRUE);

    if (lno > 0 && lno <= vl_line_count(vbp->bp)) {
	api_gotoline(vbp, lno);
	gotobol(TRUE, TRUE);
	status = ldel_bytes((B_COUNT) (llength(DOT.l) + 1), FALSE);
    } else
	status = FALSE;

    return status;
}

int
api_gline(VileBuf * vbp, int lno, char **linep, int *lenp)
{
    static char empty[1];
    int status = TRUE;

    api_setup_fake_win(vbp, TRUE);

    if (lno > 0 && lno <= vl_line_count(vbp->bp)) {
	api_gotoline(vbp, lno);
	*linep = lvalue(DOT.l);
	*lenp = llength(DOT.l);
	if (*lenp == 0) {
	    *linep = empty;	/* Make sure we pass back a zero length,
				   null terminated value when the length
				   is zero.  Otherwise perl gets confused.
				   (It thinks it should calculate the length
				   when given a zero length.)
				 */
	}
    } else
	status = FALSE;

    return status;
}

#if 0				/* Not used. */
int
api_dotgline(VileBuf * vbp, char **linep, B_COUNT * lenp, int *neednewline)
{

    api_setup_fake_win(vbp, TRUE);
    if (!vbp->dot_inited) {
	DOT = vbp->region.r_orig;	/* set DOT to beginning of region */
	vbp->dot_inited = 1;
    }

    /* FIXME: Handle rectangular regions. */

    if (is_header_line(DOT, curbp)
	|| !lisreal(DOT.l)
	|| DOT.o < 0) {
	return FALSE;
    }

    if ((DOT.l == vbp->region.r_end.l
	 && (vbp->regionshape == rgn_FULLLINE
	     || (vbp->regionshape == rgn_EXACT
		 && DOT.o >= vbp->region.r_end.o)))) {
	return FALSE;
    }

    *linep = lvalue(DOT.l) + DOT.o;

    if (llength(DOT.l) >= DOT.o) {
	*lenp = llength(DOT.l) - DOT.o;

	if (vbp->regionshape == rgn_EXACT && DOT.l == vbp->region.r_end.l) {
	    B_COUNT next = *lenp + vbp->region.r_end.o;
	    if (next >= (B_COUNT) llength(DOT.l)) {
		*lenp = next - llength(DOT.l);
	    } else {
		*lenp = 0;
	    }
	}
    } else {
	*lenp = 0;
    }

    if (*lenp == 0) {
	*linep = "";		/* Make sure we pass back a zero length,
				   null terminated value when the length
				   is zero.  Otherwise perl gets confused.
				   (It thinks it should calculate the length
				   when given a zero length.)
				 */
    }

    if (vbp->inplace_edit) {
	if (vbp->regionshape == rgn_EXACT && DOT.l == vbp->region.r_end.l) {
	    vbp->ndel = *lenp;
	    *neednewline = 0;
	} else {
	    vbp->ndel = *lenp + 1;
	    *neednewline = 1;
	}
    } else {
	if (vbp->regionshape == rgn_EXACT && DOT.l == vbp->region.r_end.l) {
	    DOT.o += *lenp;
	    *neednewline = 0;
	} else {
	    dot_next_bol();
	    *neednewline = 1;
	}
    }
    return TRUE;
}
#endif /* Not used */

int
api_sline(VileBuf * vbp, int lno, char *line, int len)
{
    int status = TRUE;

    api_setup_fake_win(vbp, TRUE);

    if (lno > 0 && lno <= vl_line_count(vbp->bp)) {
	api_gotoline(vbp, lno);
	if (lvalue(DOT.l) != line
	    && (llength(DOT.l) != len
		|| memcmp(line, lvalue(DOT.l), (size_t) len) != 0)) {
	    lreplace(line, len);
	    vbp->changed = 1;
	}
    } else
	status = FALSE;

    return status;
}

int
api_iline(VileBuf * vbp, int lno, char *line, int len)
{
    return api_aline(vbp, lno - 1, line, len);
}

int
api_lline(VileBuf * vbp, int *lnop)
{
    *lnop = vl_line_count(vbp->bp);
    return TRUE;
}

VileBuf *
api_fscreen(char *name)
{
    BUFFER *bp;

    bp = find_b_file(name);

    if (bp)
	return api_bp2vbp(bp);
    else
	return NULL;
}

int
api_delregion(VileBuf * vbp)
{

    api_setup_fake_win(vbp, TRUE);

    haveregion = NULL;
    DOT = vbp->region.r_orig;
    MK = vbp->region.r_end;
    regionshape = vbp->regionshape;

    if (vbp->regionshape == rgn_FULLLINE) {
	MK.l = lback(MK.l);
    }

    return killregion();
}

int
api_motion(VileBuf * vbp, char *mstr)
{
    const CMDFUNC *cfp;
    char *mp;
    int c, f, n, s;
    int status;
    int saved_clexec, saved_vl_echo, saved_vl_msgs, saved_isnamedcmd;

    if (mstr == NULL)
	return FALSE;

    status = TRUE;

    saved_clexec = clexec;
    saved_vl_msgs = vl_msgs;
    saved_vl_echo = vl_echo;
    saved_isnamedcmd = isnamedcmd;

    clexec = FALSE;		/* Not executing a command line */
    vl_msgs = FALSE;		/* Don't display commands / arg counts */
    vl_echo = FALSE;		/* Don't display input */
    isnamedcmd = FALSE;		/* Not a named command */

    api_setup_fake_win(vbp, TRUE);

    mp = mstr + strlen(mstr);

    mapungetc((int) ((unsigned char) esc_c | NOREMAP));
    /* Should we allow remapping?  Seems to me like it introduces too
       many variables. */
    while (mp-- > mstr) {
	mapungetc((int) ((unsigned char) *mp | NOREMAP));
    }

    while (mapped_ungotc_avail()) {

	/* Get the character */
	c = kbd_seq();

	if (ABORTED(c)) {
	    if (mapped_ungotc_avail()) {
		/* Not our esc_c */
		while (mapped_ungotc_avail())
		    (void) kbd_seq();
		status = FALSE;
		break;
	    } else
		break;		/* okay, it's ours */
	}

	f = FALSE;
	n = 1;

	do_repeats(&c, &f, &n);

	/* and execute the command */
	cfp = DefaultKeyBinding(c);
	if ((cfp != NULL) && ((cfp->c_flags & (GOAL | MOTION)) != 0))
	    s = execute(cfp, f, n);
	else {
	    while (mapped_ungotc_avail())
		(void) kbd_seq();
	    status = FALSE;
	    break;
	}
    }

    clexec = saved_clexec;
    vl_msgs = saved_vl_msgs;
    vl_echo = saved_vl_echo;
    isnamedcmd = saved_isnamedcmd;

    return status;
}

int
api_edit(char *fname, VileBuf ** retvbpp)
{
    BUFFER *bp;
    if (fname == NULL) {
	char bufname[NBUFN];
	static int unnamed_cnt = 0;
	sprintf(bufname, "[unnamed-%d]", (++unnamed_cnt % 1000));
	bp = bfind(bufname, 0);
	bp->b_active = TRUE;
    } else {
	bp = getfile2bp(fname, FALSE, FALSE);
	if (bp == NULL) {
	    *retvbpp = NULL;
	    return 1;
	}
    }

    *retvbpp = api_bp2vbp(bp);
    api_setup_fake_win(*retvbpp, TRUE);
    return !swbuffer_lfl(bp, FALSE, FALSE);
}

int
api_swscreen(VileBuf * oldsp, VileBuf * newsp)
{
    /*
     * Calling api_command_cleanup nukes various state, like DOT
     * Now if DOT got propagated, all is (likely) well.  But if it didn't,
     * then DOT will likely be in the wrong place if the executing perl
     * script expects to access a buffer on either side of a switchscreen
     * call.
     *
     * I see two different solutions for this.  1) Maintain a copy of
     * dot in the VileBuf structure. 2) Don't destroy the fake windows by
     * popping them off.  Which means that we either teach the rest
     * of vile about fake windows (scary) or we temporarily unlink the
     * fake windows from the buffer list.
     *
     * I'm in favor of temporarily unlinking the fake windows from
     * the buffer list.  The only problem with this is that the unlinked
     * window's version of DOT is not iterated over in do_mark_iterate.
     * (Probably won't be a problem that often, but we need to account
     * for it.)
     *
     * So... I guess we could maintain a separate structure which
     * has the unlinked window pointers.  And then api_mark_iterate
     * could walk these.  (Yuck.)
     *
     * Anyhow, there's a lot of issues here requiring careful
     * consideration.  Which is why I haven't already done it.
     *          - kev 4/3/1998
     *
     * Okay, I put in the stuff for detaching and reattaching fake
     * windows, but the above noted caveats still apply.
     *          - kev 4/18/1998
     *
     * I decided that the fake window stuff was too much of a hack
     * So I blew it away.  The rest of vile has limited knowledge
     * of fake windows now where it needs it.  This knowledge is
     * buried in the macro "for_each_visible_window".
     *          - kev 4/20/1998
     */
    TRACE((T_CALLED "api_swscreen(oldsp=%p, newsp=%p)\n",
	   (void *) oldsp,
	   (void *) newsp));

    curwp = curwp_visible ? curwp_visible : curwp;
    curbp = curwp->w_bufp;

    if (oldsp)
	swbuffer(vbp2bp(oldsp));
    swbuffer(vbp2bp(newsp));

    curwp_visible = curwp;

    returnCode(TRUE);
}

/* Causes the screen(s) to be updated */
void
api_update(void)
{
    TRACE((T_CALLED "api_update\n"));

    propagate_dot();

    curwp = curwp_visible ? curwp_visible : curwp;
    curbp = curwp->w_bufp;

    update(TRUE);

    curwp_visible = curwp;

    returnVoid();
}

/*
 * The following are not in the nvi API. But I needed them in order to
 * do an efficient implementation for vile.
 */

/* Propagate DOT for the fake windows that need it;
   Also do any outstanding (deferred) deletes. */
static void
propagate_dot(void)
{
    WINDOW *wp;

    for_each_window(wp) {
	if (!is_fake_window(wp)) {
	    /* We happen to know that the fake windows will always
	       be first in the buffer list.  So we exit the loop
	       when we hit one that isn't fake. */
	    break;
	}

	/* Do outstanding delete (if any) */
	api_setup_fake_win(bp2vbp(wp->w_bufp), TRUE);

	if (bp2vbp(wp->w_bufp)->dot_changed) {
	    if (curwp_visible && curwp_visible->w_bufp == wp->w_bufp) {
		curwp_visible->w_dot = wp->w_dot;
		curwp_visible->w_flag |= WFHARD;
	    } else {
		int found = 0;
		WINDOW *pwp;
		for_each_window(pwp) {
		    if (wp->w_bufp == pwp->w_bufp
			&& !is_fake_window(pwp)) {
			found = 1;
			pwp->w_dot = wp->w_dot;
			pwp->w_flag |= WFHARD;
			break;	/* out of inner for_each_window */
		    }
		}
		if (!found) {
		    /* No window to propagate DOT in, so just use the
		       buffer's traits */
		    wp->w_bufp->b_dot = wp->w_dot;
		}
	    }
	}
    }
}

void
api_command_cleanup(void)
{
    BUFFER *bp;

    if (curwp_visible == NULL)
	curwp_visible = curwp;

    /* Propgate dot to the visible windows and do any deferred deletes */
    propagate_dot();

    /* Pop the fake windows */

    while ((bp = pop_fake_win(curwp_visible, (BUFFER *) 0)) != NULL) {
	if (bp2vbp(bp) != NULL) {
	    bp2vbp(bp)->fwp = NULL;
	    bp2vbp(bp)->dot_inited = 0;		/* for next time */
	    bp2vbp(bp)->dot_changed = 0;	/* ditto */
	    if (bp2vbp(bp)->changed) {
		int buf_flag_says_changed = b_is_changed(bp);
		chg_buff(bp, WFHARD);
		bp2vbp(bp)->changed = 0;
		/* Make sure results of an unmark get propagated back */
		if (buf_flag_says_changed)
		    b_set_changed(bp);
		else
		    b_clr_changed(bp);
	    }
	}
    }

    curwp_visible = NULL;

    /* Not needed? */
    if (curbp != curwp->w_bufp)
	make_current(curwp->w_bufp);

    MK = DOT;			/* make sure MK is in same buffer as DOT */

#if OPT_PERL
    perl_free_deferred();
#endif
}

void
api_free_private(void *vsp)
{
    VileBuf *vbp = (VileBuf *) vsp;

    if (vbp) {
	vbp->bp->b_api_private = NULL;
#if OPT_PERL && !NO_LEAKS
	perl_free_handle(vbp->perl_handle);
#endif
	free(vbp);
    }
}

/* Given a buffer pointer, returns a pointer to a VileBuf structure,
 * creating it if necessary.
 */
VileBuf *
api_bp2vbp(BUFFER *bp)
{
    VileBuf *vbp;

    vbp = bp2vbp(bp);
    if (vbp == NULL) {
	vbp = typecalloc(VileBuf);
	if (vbp != NULL) {
	    bp->b_api_private = vbp;
	    vbp->bp = bp;
	    vbp->regionshape = rgn_FULLLINE;
	    vbp->region.r_orig.l =
		vbp->region.r_end.l = buf_head(bp);
	    vbp->region.r_orig.o =
		vbp->region.r_end.o = 0;
	    vbp->dot_inited = 0;
	    vbp->dot_changed = 0;
	    vbp->ndel = 0;
	}
    }
    return vbp;
}

#endif
