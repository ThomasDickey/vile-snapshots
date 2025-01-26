/*
 * msgs.c
 *
 * Support functions for "popup-msgs" mode.
 * Written by T.E.Dickey for vile (august 1994).
 *
 * $Id: msgs.c,v 1.34 2025/01/26 17:08:26 tom Exp $
 */
#include "estruct.h"

#if OPT_POPUP_MSGS

#include "edef.h"

/*
 * Create the message-buffer if it doesn't already exist
 */
static BUFFER *
create_msgs(void)
{
    BUFFER *bp = NULL;

    if (wheadp != NULL) {	/* we need windows before creating buffers */
	bp = bfind(MESSAGES_BufName, BFINVS);

	if (valid_buffer(bp)) {
	    b_set_invisible(bp);
	    bp->b_active = TRUE;
	}
    }
    return bp;
}

/*
 * This is invoked as a wrapper for 'kbd_putc()'.  It writes to the Messages
 * scratch buffer, and also to the message line.  If the Messages buffer isn't
 * visible, it is automatically popped up when a new message line is begun.
 * Since it's a scratch buffer, popping it down destroys it.
 */
int
msg_putc(int c)
{
    BUFFER *savebp = curbp;
    WINDOW *savewp = curwp;
    MARK savemk;
    int saverow = ttrow;
    int savecol = ttcol;
    register BUFFER *bp;
    register WINDOW *wp;

    if ((bp = create_msgs()) == NULL)
	return TRUE;

    savemk = DOT;
    beginDisplay();
    /*
     * Modify the current-buffer state as unobtrusively as possible (i.e.,
     * don't modify the buffer order, and don't make the buffer visible if
     * it isn't already!).  To use the 'bputc()' logic, though, we've got
     * to have a window, even if it's not real.
     */
    curbp = bp;
    if ((wp = bp2any_wp(bp)) == NULL) {
	static WINDOW dummy;
	wp = &dummy;
	wp->w_bufp = bp;
    }
    curwp = wp;
    DOT.l = lback(buf_head(bp));
    DOT.o = llength(DOT.l);

    /*
     * Write into the [Messages]-buffer
     */
#if OPT_TRACE
    if (c == '\n') {
	static TBUFF *ss;
	int len = (DOT.o > 0) ? DOT.o : 1;
	if (tb_init(&ss, EOS) != NULL
	    && tb_bappend(&ss,
			  (DOT.o > 0) ? lvalue(DOT.l) : "?",
			  (size_t) len) != NULL
	    && tb_append(&ss, EOS) != NULL) {
	    TRACE(("msg:%s\n",
		   visible_buff(tb_values(ss),
				(int) tb_length(ss) - 1, TRUE)));
	}
    }
#endif
    if ((c != '\n') || (DOT.o > 0)) {
	bputc(c);
	b_clr_changed(bp);
    }

    /* Finally, restore the original current-buffer and write the character
     * to the message line.
     */
    curbp = savebp;
    curwp = savewp;
    if (savewp)
	DOT = savemk;
    movecursor(saverow, savecol);
    if (c != '\n') {
	if (sgarbf) {
	    mlsavec(c);
	} else {
	    kbd_putc(c);
	}
    }
    endofDisplay();

    return TRUE;
}

void
popup_msgs(void)
{
    BUFFER *savebp = curbp;
    WINDOW *savewp = curwp;
    MARK savemk;
    register BUFFER *bp;
    WINDOW *wp;

    if ((bp = create_msgs()) == NULL)
	return;

    savemk = DOT;
    if (!is_empty_buf(bp)) {
	if ((curwp == NULL) || sgarbf || global_g_val(GMDPOPUP_MSGS) == -TRUE) {
	    return;		/* CAN'T popup yet */
	}
	if (popupbuff(bp) == FALSE) {
	    (void) zotbuf(bp);
	    return;
	}

	if ((wp = bp2any_wp(bp)) != NULL) {
	    make_local_w_val(wp, WMDNUMBER);
	    set_w_val(wp, WMDNUMBER, FALSE);
	}
	set_rdonly(bp, non_filename(), MDVIEW);
	curbp = savebp;
	curwp = savewp;
	if (savewp)
	    DOT = savemk;
    }
}

/*
 * If no warning messages were encountered during startup, and the popup-msgs
 * mode wasn't enabled, discard the informational messages that are there
 * already.
 */
void
purge_msgs(void)
{
    TRACE(("purge_msgs mode:%d, warnings:%d\n",
	   global_g_val(GMDPOPUP_MSGS), warnings));

    if ((global_g_val(GMDPOPUP_MSGS) == -TRUE)
	&& (warnings == 0)) {
	BUFFER *bp = find_b_name(MESSAGES_BufName);
	if (valid_buffer(bp)
	    && bp->b_nwnd == 0) {
	    (void) zotbuf(bp);
	}
	set_global_g_val(GMDPOPUP_MSGS, FALSE);
    }
}
#endif
