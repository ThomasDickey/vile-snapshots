/* 
 * api.c -- (roughly) nvi's api to perl and tcl
 */

#include "estruct.h"
#include "edef.h"

#if OPT_PERL

#include "api.h"

static WINDOW *curwp_after;

/* Maybe this should go in line.c ? */
static int
linsert_chars(char *s, int len)
{
#if 0 
    int nlcount = 0;
    while (len-- > 0) {
	if (*s == '\n') {
	    lnewline();
	    nlcount++;
	}
	else
	    linsert(1, *s);
	s++;
    }
    return nlcount;
#else 
    /* I wrote the code this way both for efficiency reasons and 
       so that the MARK denoting the end of the range wouldn't 
       be moved to one of the newly inserted lines. 
 
       I have no doubt it could be made much more efficient still 
       by moving it into line.c and rewriting it to insert chunks 
       of characters en masse. 
    */ 
    int nlcount = 0; 
    int nlatend = 0; 
 
    if (len <= 0) 
	return 0; 
 
    if (s[len-1] == '\n') { 
	if (!is_empty_buf(curbp)) { 
 
	    /* We implicitly get a newline by inserting anything into 
	       an empty buffer.  If we insert a newline into an empty 
	       buffer, we'll end up getting two of them. */ 
 
	    lnewline(); 
	    nlcount++; 
	    DOT.l = lback(DOT.l);		/* back up DOT to the newly */ 
	    DOT.o = 0;			/* inserted line */ 
	} 
	nlatend = 1; 
	len--; 
    } 
 
    while (len-- > 0) { 
	if (*s == '\n') { 
	    lnewline(); 
	    nlcount++; 
	} 
	else 
	    linsert(1, *s); 
	s++; 
    } 
 
    if (nlatend) { 
	/* Advance DOT to where it's supposed to be */ 
	DOT.l = lforw(DOT.l); 
	DOT.o = 0; 
    } 
 
    return nlcount; 
#endif 
}

/* Another candidate for line.c */
static int
lreplace(char *s, int len)
{
    LINE *lp = DOT.l;
    int i;
    char *t;
    WINDOW *wp;

    copy_for_undo(lp);

    DOT.o = 0;

    if (len > lp->l_size) {
	int nlen;
	char *ntext;

#define roundlenup(n) ((n+NBLOCK-1) & ~(NBLOCK-1))

	nlen = roundlenup(len);
	ntext = castalloc(char, nlen);
	if (ntext == 0)
	    return FALSE;
	if (lp->l_text)
	    ltextfree(lp, curbp);
	lp->l_text = ntext;
	lp->l_size = nlen;
    }

    lp->l_used = len;

    /* FIXME: Handle embedded newlines */
    for (i=len-1, t=lp->l_text; i >=0; i--)
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
    do_mark_iterate(mp,
		    if (mp->l == lp && mp->o > len)
			mp->o = len;
    );

    return TRUE;
}

void 
api_setup_fake_win(SCR *sp) 
{
    if (curwp_after == 0)
	curwp_after = curwp;

    if (sp->fwp) {
	curwp = sp->fwp;
    }
    else {
	(void) push_fake_win(sp->bp);
	sp->fwp = curwp;
	sp->changed = 0;
    }

    /* Should be call make_current() for this? */
    curbp = curwp->w_bufp;
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
       one of our SCR structures still pointer to one of the 
       structures. 
 
    3) Make line.c and undo.c aware of the SCR regions. 
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
    SCR  *sp = bp2sp(bp); 
 
    if (sp != NULL) { 
	switch (*iterp) { 
	    case 0: 
		mp = &sp->region.r_orig; 
		break; 
	    case 1: 
		mp = &sp->region.r_end; 
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
 *
 */
int 
api_gotoline(SCR *sp, int lno) 
{
#if !SMALLER
    int count;
    LINE *lp;
    BUFFER *bp = sp->bp;

    if (!b_is_counted(bp))
	bsizes(bp);

    count = lno - DOT.l->l_number;
    lp = DOT.l;

    while (count < 0 && lp != buf_head(bp)) { 
	lp = lback(lp);
	count++;
    }
    while (count > 0 && lp != buf_head(bp)) { 
	lp = lforw(lp);
	count--;
    }

    DOT.o = 0;

    if (lp != buf_head(bp) && lno == lp->l_number) {
	DOT.l = lp;
	return TRUE;
    }
    else {
	DOT.l = lback(buf_head(bp));
	return FALSE;
    }

#else
    return gotoline(TRUE, lno);
#endif
}

int
api_aline(SCR *sp, int lno, char *line, int len)
{
    api_setup_fake_win(sp); 

    if (lno >= 0 && lno < line_count(sp->bp)) {
	api_gotoline(sp, lno+1); 
	linsert_chars(line, len);
	lnewline();
    }
    else {	/* append to the end */
	gotoline(FALSE, 0);
	gotoeol(FALSE, 0);
	lnewline();
	linsert_chars(line, len);
    }

    return TRUE;
}

int
api_dotinsert(SCR *sp, char *text, int len) { 
    api_setup_fake_win(sp); 
    linsert_chars(text, len); 
    return TRUE; 
} 
 
int 
api_dline(SCR *sp, int lno)
{
    int status = TRUE;

    api_setup_fake_win(sp); 

    if (lno > 0 && lno <= line_count(sp->bp)) {
	api_gotoline(sp, lno); 
	gotobol(TRUE,TRUE);
	ldelete(llength(DOT.l) + 1, FALSE); 
    }
    else
	status = FALSE;

    return status;
}

int
api_gline(SCR *sp, int lno, char **linep, int *lenp)
{
    int status = TRUE;

    api_setup_fake_win(sp); 

    if (lno > 0 && lno <= line_count(sp->bp)) {
	api_gotoline(sp, lno); 
	*linep = DOT.l->l_text;
	*lenp = llength(DOT.l);
	if (*lenp == 0) {
	    *linep = "";	/* Make sure we pass back a zero length,
	                           null terminated value when the length
				   is zero.  Otherwise perl gets confused.
				   (It thinks it should calculate the length
				   when given a zero length.)
				 */
	}
    }
    else
	status = FALSE;

    return status;
}

int
api_dotgline(SCR *sp, char **linep, int *lenp) 
{ 
 
    api_setup_fake_win(sp); 
    if (!sp->dot_inited) {  
	DOT = sp->region.r_orig;	/* set DOT to beginning of region */ 
	sp->dot_inited = 1; 
    } 
 
    /* FIXME: Handle rectangular regions. */ 
 
    if (   is_header_line(DOT, curbp)  
        || (   DOT.l == sp->region.r_end.l  
	    && (   sp->regionshape == FULLLINE 
	        || (   sp->regionshape == EXACT 
		    && DOT.o >= sp->region.r_end.o)))) 
    { 
	return FALSE; 
    } 
 
    *linep = DOT.l->l_text + DOT.o; 
    *lenp = llength(DOT.l) - DOT.o; 
 
    if (sp->regionshape == EXACT && DOT.l == sp->region.r_end.l) { 
	*lenp -= llength(DOT.l) - sp->region.r_end.o; 
    } 
 
    if (*lenp < 0) 
	*lenp = 0;	/* Make sure return length is non-negative */ 
 
    if (*lenp == 0) { 
	*linep = "";	/* Make sure we pass back a zero length, 
			       null terminated value when the length 
			       is zero.  Otherwise perl gets confused. 
			       (It thinks it should calculate the length 
			       when given a zero length.) 
			     */ 
    } 
 
    if (sp->inplace_edit) { 
	if (sp->regionshape == EXACT && DOT.l == sp->region.r_end.l) 
	    ldelete(*lenp, TRUE); 
	else 
	    ldelete(*lenp + 1, TRUE); 
    } 
    else { 
	if (sp->regionshape == EXACT && DOT.l == sp->region.r_end.l) 
	    DOT.o += *lenp; 
	else { 
	    DOT.l = lforw(DOT.l); 
	    DOT.o = 0; 
	} 
    } 
    return TRUE; 
} 
 
int 
api_sline(SCR *sp, int lno, char *line, int len)
{
    int status = TRUE;

    api_setup_fake_win(sp); 

    if (lno > 0 && lno <= line_count(sp->bp)) {
	api_gotoline(sp, lno); 
	if (   DOT.l->l_text != line 
	    && (   llength(DOT.l) != len
	        || memcmp(line, DOT.l->l_text, len) != 0)) {
	    lreplace(line, len);
	    sp->changed = 1;
	}
    }
    else
	status = FALSE;

    return status;
}

int
api_iline(SCR *sp, int lno, char *line, int len)
{
    return api_aline(sp, lno-1, line, len);
}

int
api_lline(SCR *sp, int *lnop)
{
    *lnop = line_count(sp->bp);
    return TRUE;
}

SCR *
api_fscreen(int id, char *name)
{
    BUFFER *bp;

    bp = find_b_file(name);

    if (bp)
	return api_bp2sp(bp);
    else
	return 0;
}

/* FIXME: return SCR * for retbpp */
int
api_edit(SCR *sp, char *fname, SCR **retspp, int newscreen)
{
    BUFFER *bp;
    if (fname == NULL) {
	/* FIXME: This should probably give you a truly anonymous buffer */
	fname = (char *) UNNAMED_BufName;
    }
    bp = getfile2bp(fname, FALSE, FALSE);
    if (bp == 0) {
	*retspp = 0;
	return 1;
    }
    *retspp = api_bp2sp(bp);
    api_setup_fake_win(*retspp); 
    return !swbuffer_lfl(bp, FALSE);
}

int
api_swscreen(SCR *oldsp, SCR *newsp)
{
    api_command_cleanup();		/* pop the fake windows */

    swbuffer(sp2bp(oldsp));
    swbuffer(sp2bp(newsp));
    curwp_after = curwp;
}

/*
 * The following are not in the nvi API. But I needed them in order to
 * do an efficient implementation for vile.
 */

void
api_command_cleanup(void)
{
    BUFFER *bp;
    WINDOW *wp; 

    if (curwp_after == 0)
	curwp_after = curwp;

    /* Propagate DOT for the fake windows that need it */ 
 
    for_each_window(wp) { 
	if (!is_fake_win(wp)) { 
	    /* We happen to know that the fake windows will always 
	       be first in the buffer list.  So we exit the loop 
	       when we hit one that isn't fake. */ 
	    break; 
	} 
	if (bp2sp(wp->w_bufp)->dot_changed) { 
	    if (curwp_after->w_bufp == wp->w_bufp) { 
		curwp_after->w_dot = wp->w_dot; 
		curwp_after->w_flag |= WFHARD; 
	    } 
	    else { 
		int found = 0; 
		WINDOW *pwp; 
		for_each_window(pwp) { 
		    if (wp->w_bufp == pwp->w_bufp) { 
			found = 1; 
			pwp->w_dot = wp->w_dot; 
			pwp->w_flag |= WFHARD; 
			break;		/* out of inner for_each_window */ 
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
 
    /* Pop the fake windows */

    while ((bp = pop_fake_win(curwp_after)) != NULL) {
	if (bp2sp(bp) != NULL)
	    bp2sp(bp)->fwp = 0;
	    bp2sp(bp)->dot_inited = 0;		/* for next time */ 
	    bp2sp(bp)->dot_changed = 0;		/* ditto */ 
	    if (bp2sp(bp)->changed) {
		chg_buff(bp, WFHARD);
		bp2sp(bp)->changed = 0;
	    }
    }

    curwp_after = 0;

    if (curbp != curwp->w_bufp)
	make_current(curwp->w_bufp);
}

void
api_free_private(void *vsp)
{
    SCR *sp = (SCR *) vsp;

    if (sp) {
	sp->bp->b_api_private = 0;
#if OPT_PERL
	perl_free_handle(sp->perl_handle);
#endif
	free(sp);
    }
}

/* Given a buffer pointer, returns a pointer to a SCR structure,
 * creating it if necessary.
 */
SCR *
api_bp2sp(BUFFER *bp)
{
    SCR *sp;

    sp = bp2sp(bp);
    if (sp == 0) {
	sp = typecalloc(SCR);
	if (sp != 0) {
	    bp->b_api_private = sp;
	    sp->bp = bp;
	    sp->regionshape = FULLLINE; 
	    sp->region.r_orig.l = 
	    sp->region.r_end.l  = buf_head(bp); 
	    sp->region.r_orig.o = 
	    sp->region.r_end.o  = 0; 
	    sp->dot_inited = 0; 
	    sp->dot_changed = 0;  
	}
    }
    return sp;
}

#endif
