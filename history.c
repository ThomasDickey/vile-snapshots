/*
 *	history.c
 *
 *	Manage command-history buffer
 *
 * Notes:
 *	This module manages an invisible, non-volatile buffer "[History]".
 *	Each keyboard command to vile is logged in this buffer.  The leading
 *	':' is not included, but all other characters are preserved so that the
 *	command can be replayed.
 *
 *	Each interactive call on 'kbd_reply()' logs the resulting string, and
 *	the "glue" character (nominally, the end-of-line character), so that
 *	successive calls can be spliced together. On completion of the command,
 *	the composed command (in 'MyText') is flushed into the history-buffer.
 *
 *	The procedure 'edithistory()' is invoked from 'kbd_reply()' to provide
 *	access to the history.  In particular, it presents a scrollable list of
 *	strings based on a match of the command to that point.  For example, if
 *	the commands
 *
 *		:set ts=8
 *		:set ab
 *		:set ai
 *
 *	were entered, then in response to ":set", the user would see the
 *	strings "ai", "ab" and "ts".
 *
 *	Scrolling is accomplished by either arrow keys, or by an escaped set of
 *	commands (a la 'ksh').
 *
 *	Note that this implementation is a compromise.  Ideally, the command
 *	interpreter for ':' would be able to scroll through the entire list of
 *	commands from any point, moving forward and backward through its
 *	internal state of range, command, arguments.  As implemented, it is not
 *	so general.  The user can backspace from the command state into the
 *	range state, but not from the arguments.  Also, the history scrolling
 *	is not really useful in the range state, so it is disabled there.  If
 *	the command interpreter were able to easily go back and forth in its
 *	state, then it would be simple to implement an 'expert' mode, in which
 *	prompting would be suppressed.
 *
 * To do:
 *	Add logic to quote arguments that should be strings, to make them
 *	easier to parse back for scrolling, etc.
 *
 *	Integrate this with the "!!" response to the ^X-! and !-commands.
 *
 *	Modify the matching logic so that file commands (i.e., ":e", ":w",
 *	etc.) are equivalent when matching for the argument.  Currently, the
 *	history scrolling will show only arguments for identical command names.
 *
 *	Modify the matching logic so that search commands (i.e., "/" and "?")
 *	are equivalent when matching for the argument.  Note also that these do
 *	not (yet) correspond to :-commands.  Before implementing, probably will
 *	have to make TESTC a settable mode.
 *
 * $Id: history.c,v 1.96 2025/01/26 15:01:35 tom Exp $
 *
 */

#include "estruct.h"
#include "edef.h"

#if	OPT_HISTORY

#define HST_QUOTES 1

#define	tb_args(p)	tb_values(p), (int)tb_length(p)
#define	tb_args2(p)	tb_values(p), tb_length(p)
#define	lp_args(p)	lvalue(p), llength(p)

typedef struct {
    TBUFF **buffer;
    size_t *position;
    int (*endfunc) (EOL_ARGS);
    int eolchar;
    UINT options;
} HST;

/*--------------------------------------------------------------------------*/

static TBUFF *MyText;		/* current command to display */
static int MyGlue;		/* most recent eolchar */
static int MyLevel;		/* logging iff level is 1 */

/*--------------------------------------------------------------------------*/

static void
stopMyBuff(void)
{
    BUFFER *bp;

    if ((bp = find_b_name(HISTORY_BufName)) != NULL)
	(void) zotbuf(bp);

    tb_free(&MyText);
}

static BUFFER *
makeMyBuff(void)
{
    BUFFER *bp;

    if (!global_g_val(GMDHISTORY)) {
	bp = NULL;
    } else if ((bp = make_ro_bp(HISTORY_BufName, BFINVS)) == NULL) {
	stopMyBuff();
    } else {
	set_vilemode(bp);
    }
    return bp;
}

/*
 * Returns 0 or 1 according to whether we will add the glue-character in the
 * next call on 'hst_append()'.
 */
static int
willGlue(void)
{
    if ((tb_length(MyText) != 0) && (isPrint(MyGlue) || MyGlue == '\r')) {
	int c = tb_values(MyText)[0];
	if ((c != SHPIPE_LEFT[0]) || isRepeatable(c))
	    return 1;
    }
    return 0;
}

/*
 * Returns true iff we display the complete, rather than the immediate portion
 * of the history line.  We do this for !-commands so that the user can see the
 * entire command when scrolling.
 *
 * The shift-commands also are a (similar) special case.
 */
static int
willExtend(const char *src, int srclen)
{
    if ((tb_length(MyText) == 0)
	&& src != NULL
	&& (srclen > 0)) {
	return (src[0] == SHPIPE_LEFT[0]) || isRepeatable(src[0]);
    }
    return FALSE;
}

/*
 * Returns a positive number if the length of the given LINE is at least as
 * long as the given string in 'src' and if it matches to the length in
 * 'srclen'.
 */
static int
sameLine(LINE *lp, char *src, int srclen)
{
    if (lp == NULL || !lisreal(lp) || src == NULL || srclen <= 0)
	return 0;
    else {
	int dstlen = llength(lp);

	if (dstlen >= srclen) {
	    if (!memcmp(lvalue(lp), src, (size_t) srclen)) {
		if (isRepeatable(*src)
		    && isRepeatable(lvalue(lp)[0])
		    && dstlen != srclen)
		    return -1;
		return (dstlen - srclen);
	    }
	}
    }
    return -1;
}

/*
 * Given a starting offset and index limit for src, find the index of the
 * character that ends the parameter.
 */
static int
endOfParm(HST * parm, char *src, int offset, int limit)
{
    int n;

    if (limit > 0) {
	if (willExtend(src, limit)) {
	    n = limit;
	} else {
#if HST_QUOTES
	    int quoted = (src[offset] == DQUOTE);
	    int escaped = FALSE;
#endif
	    for (n = offset; n < limit; n++) {
#if HST_QUOTES
		if (quoted) {
		    if (escaped) {
			escaped = FALSE;
		    } else if (src[n] == BACKSLASH) {
			escaped = TRUE;
		    } else if (n != offset && (src[n] == src[offset])) {
			n++;
			break;
		    }
		} else
#endif
		if ((parm->endfunc) (src, (size_t) n, src[n], parm->eolchar))
		    break;
	    }
	}
    } else {
	n = 0;
    }
    return n;
}

/*
 * Returns the index of the character that ends the current argument on the
 * given line.
 */
static int
parseArg(HST * parm, LINE *lp)
{
    return endOfParm(parm,
		     lvalue(lp),
		     willGlue() + (int) tb_length(MyText),
		     llength(lp));
}

/*
 * Returns true if the buffer is not the first token on the current history
 * line, and if it contains blanks or quotes which would confuse us when
 * parsing.
 */
static int
needQuotes(TBUFF *src)
{
#if HST_QUOTES
    if (tb_length(MyText)
	&& must_quote_token(tb_values(src), tb_length(src)))
	return TRUE;
#endif
    return FALSE;
}

#if HST_QUOTES
/*
 * Reverses append_quoted_token() by stripping the quotes and returning an
 * allocated buffer with the text.  The associated length is returned via the
 * 'actual' parameter.
 */
static char *
stripQuotes(char *src, int len, int eolchar, int *actual)
{
    char *dst = NULL;

    if (len > 0) {
	TRACE(("stripQuotes(%.*s)\n", len, src));
	if ((dst = typeallocn(char, (size_t) len + 1)) != NULL) {
	    int j, k;
	    int quoted = FALSE;
	    int escaped = FALSE;

	    for (j = k = 0; j < len; j++) {
		if (escaped) {
		    escaped = FALSE;
		    switch (src[j]) {
		    case DQUOTE:
		    case BACKSLASH:
		    default:
			dst[k++] = src[j];
			break;
		    case 'b':
			dst[k++] = '\b';
			break;
		    case 't':
			dst[k++] = '\t';
			break;
		    case 'r':
			dst[k++] = '\r';
			break;
		    case 'n':
			dst[k++] = '\n';
			break;
		    }
		} else if (src[j] == BACKSLASH) {
		    escaped = TRUE;
		} else if (src[j] == DQUOTE && src[0] == DQUOTE) {
		    quoted = !quoted;
		} else if (!quoted && src[j] == eolchar) {
		    break;
		} else {
		    dst[k++] = src[j];
		}
	    }
	    dst[k] = EOS;
	    *actual = k;
	}
    }
    return dst;
}
#endif

static void
glueBufferToResult(TBUFF **dst, TBUFF *src)
{
    int shell_cmd = ((tb_length(*dst) != 0 && isShellOrPipe(tb_values(*dst)))
		     || (tb_length(src) != 0 && isShellOrPipe(tb_values(src))));

    if (willGlue())
	(void) tb_append(dst, MyGlue);

    if (!shell_cmd
	&& needQuotes(src)) {
	append_quoted_token(dst, tb_values(src), tb_length(src));
    } else {
	(void) tb_bappend(dst, tb_args2(src));
    }
}

/******************************************************************************/

void
hst_reset(void)
{
    MyLevel = 0;
    (void) tb_init(&MyText, esc_c);
}

void
hst_init(int c)
{
    if (++MyLevel == 1) {
	(void) tb_init(&MyText, esc_c);
	MyGlue = EOS;
	if (c != EOS)
	    (void) tb_append(&MyText, c);
    }
}

void
hst_glue(int c)
{
    /* ensure we don't repeat '/' delimiter */
    if (tb_length(MyText) == 0
	|| tb_values(MyText)[0] != c)
	MyGlue = c;
}

void
hst_append(TBUFF *cmd, int glue, int can_extend)
{
    static int skip = 1;	/* e.g., after "!" */

    if (clexec || !vl_echo || qpasswd) {
	/* noninteractive, $disinp=FALSE, or querying for password */

	return;
    }
    if (isreturn(glue))
	glue = ' ';

    TRACE(("hst_append(cmd=%d:%d:%s) glue='%c'\n",
	   willExtend(tb_args(cmd)),
	   (int) tb_length(cmd),
	   tb_visible(cmd),
	   glue));
    TRACE(("...MyText        :%d:%s\n", (int) tb_length(MyText), tb_visible(MyText)));
    if (can_extend && willExtend(tb_args(cmd))
	&& tb_length(cmd) > (size_t) skip) {
	kbd_pushback(cmd, skip);
    }

    glueBufferToResult(&MyText, cmd);
    TRACE(("...MyText        :%d:%s\n", (int) tb_length(MyText), tb_visible(MyText)));
    MyGlue = glue;
}

void
hst_append_s(char *cmd, int glue, int can_extend)
{
    TBUFF *p = tb_string(cmd);
    hst_append(p, glue, can_extend);
    tb_free(&p);
}

void
hst_remove(const char *cmd)
{
    if (MyLevel == 1 && cmd != NULL && *cmd != EOS) {
	TBUFF *temp = NULL;
	size_t len = tb_length(tb_scopy(&temp, cmd)) - 1;

	TRACE(("hst_remove(%s)\n", cmd));
	while (*cmd++)
	    tb_unput(MyText);
	if ((temp = kbd_kill_response(temp, &len, killc)) != NULL)
	    tb_free(&temp);
    }
}

void
hst_flush(void)
{
    BUFFER *bp;
    WINDOW *wp;
    LINE *lp;

    TRACE(("hst_flush %d:%s\n",
	   MyLevel,
	   visible_buff(tb_values(MyText),
			(int) tb_length(MyText),
			FALSE)));

    if (MyLevel <= 0)
	return;
    if (MyLevel-- != 1)
	return;

    if ((tb_length(MyText) != 0)
	&& ((bp = makeMyBuff()) != NULL)) {

	/* suppress if this is the same as previous line */
	if (((lp = lback(buf_head(bp))) != NULL)
	    && (lp != buf_head(bp))
	    && (sameLine(lp, tb_args(MyText)) == 0)) {
	    (void) tb_init(&MyText, esc_c);
	    return;
	}

	if (!addline(bp, tb_args(MyText))) {
	    stopMyBuff();
	    return;
	}

	/* patch: reuse logic from slowreadf()? */
	for_each_visible_window(wp) {
	    if (wp->w_bufp == bp) {
		wp->w_flag |= WFFORCE;
		if (wp == curwp)
		    continue;
		/* force dot to the beginning of last-line */
		wp->w_force = -1;
		if (wp->w_dot.l != lback(buf_head(bp))) {
		    wp->w_dot.l = lback(buf_head(bp));
		    wp->w_dot.o = 0;
		    wp->w_flag |= WFMOVE;
		}
	    }
	}
	updatelistbuffers();	/* force it to show current sizes */
	(void) tb_init(&MyText, esc_c);
    }
}

/*ARGSUSED*/
int
showhistory(int f GCC_UNUSED, int n GCC_UNUSED)
{
    BUFFER *bp = makeMyBuff();

    if (!global_g_val(GMDHISTORY)) {
	mlforce("history mode is not set");
	return FALSE;
    } else if (bp == NULL || popupbuff(bp) == FALSE) {
	return no_memory("show-history");
    }
    return TRUE;
}

/*
 * Find the last line in the history buffer that matches the portion of
 * the command that has been input to this point.  The substrings to the
 * right (up to eolchar) will form the set of history strings that the
 * user may scroll through.
 */
static LINE *
hst_find(HST * parm, BUFFER *bp, LINE *lp, int direction)
{
    LINE *base = buf_head(bp);
    LINE *lp0 = lp;

    if ((lp0 == NULL)
	|| ((lp == base) && (direction > 0))) {
	return NULL;
    }

    for_ever {
	if (direction > 0) {
	    if (lp == lback(base))	/* cannot wrap-around */
		return NULL;
	    lp = lforw(lp);
	} else
	    lp = lback(lp);
	if (lp == base)
	    return NULL;	/* empty or no matches */

	if (!lisreal(lp)
	    || ((size_t) llength(lp) <= (tb_length(MyText)
					 + (size_t) willGlue()))
	    || (sameLine(lp, tb_args(MyText)) < 0))
	    continue;		/* prefix mismatches */

	if (willGlue()) {	/* avoid conflicts setall/set */
	    int len = (int) tb_length(MyText);
	    if (len > 0
		&& (len > 1 || !isPunct(tb_values(MyText)[0]))
		&& llength(lp) > len
		&& lvalue(lp)[len] != MyGlue)
		continue;
	}

	/* '/' and '?' are not (yet) :-commands.  Don't display them
	 * in the command-name scrolling.
	 */
	if (tb_length(MyText) == 0) {
	    if (lvalue(lp)[0] == '/'
		|| lvalue(lp)[0] == '?')
		continue;
	}

	/* compare the argument that will be shown for the original
	 * and current lines.
	 */
	if (lisreal(lp0)) {
	    int n0 = parseArg(parm, lp0);
	    int n1 = parseArg(parm, lp);
	    if (n0 != 0
		&& n1 != 0
		&& n0 == n1
		&& sameLine(lp, lvalue(lp0), n0) >= 0)
		continue;
	}

	return lp;
    }
}

/*
 * Update the display of the currently-scrollable buffer on the prompt-line.
 */
static void
hst_display(HST * parm, char *src, int srclen)
{
    TRACE((T_CALLED "hst_display(%.*s) eolchar='%c'\n",
	   srclen, src,
	   isreturn(parm->eolchar) ? ' ' : parm->eolchar));

    /* kill the whole buffer */
    *(parm->position) = tb_length(*(parm->buffer));
    wminip->w_dot.o = llength(wminip->w_dot.l);
    kbd_kill_response(*(parm->buffer), parm->position, killc);

    if (src != NULL && srclen != 0) {
	char *stripped;
	int offset = (int) tb_length(MyText) + willGlue();
	int n = endOfParm(parm, src, offset, srclen) - offset;

	src += offset;
	srclen -= offset;
	stripped = src;
#if HST_QUOTES
	TRACE(("...offset=%d, n=%d, MyText=%s\n", offset, n, tb_visible(MyText)));
	if (tb_length(MyText) != 0 &&
	    isShellOrPipe(tb_values(MyText))) {
	    TRACE(("...MyText is a shell command\n"));
	} else if (srclen == 0) {
	    TRACE(("...OOPS - no source?\n"));
	} else if (tb_length(MyText) == 0 && offset == 0 && isShellOrPipe(src)) {
	    TRACE(("...src is a shell command\n"));
	} else if (tb_length(MyText) == 0 && offset == 0 && *src == SQUOTE) {
	    TRACE(("...src is a position command\n"));
	    n = srclen;
	} else if (*src == DQUOTE || isSpace(parm->eolchar)) {
	    stripped = stripQuotes(src, n,
				   isSpace(parm->eolchar) ? ' ' : parm->eolchar,
				   &n);
	}
#endif
	TRACE(("...hst_display offset=%d, string='%.*s'\n", offset, n, stripped));
	*parm->position = (size_t) kbd_show_response(parm->buffer,
						     stripped,
						     (size_t) n,
						     parm->eolchar, parm->options);
#if HST_QUOTES
	if (stripped != NULL && stripped != src)
	    free(stripped);
#endif
    }
    returnVoid();
}

/*
 * Update the display using a LINE as source
 */
static void
display_LINE(HST * parm, LINE *lp)
{
    hst_display(parm, lp_args(lp));
}

/*
 * Update the display using a TBUFF as source
 */
static void
display_TBUFF(HST * parm, TBUFF *tp)
{
    hst_display(parm, tb_args(tp));
}

/*
 * Perform common scrolling functions for arrow-keys and ESC-mode.
 */
static TBUFF *h_original;	/* save 'buffer' on first-scroll */
static int h_was_edited;	/* true iff any edit happened */
static int h_direction;		/* current scrolling +/- */
static int h_distance;		/* distance from original entry */

static LINE *
hst_scroll(LINE *lp1, HST * parm)
{
    BUFFER *bp = makeMyBuff();
    LINE *result = NULL;

    if (bp != NULL) {
	LINE *lp0 = buf_head(bp);
	LINE *lp2 = hst_find(parm, bp, lp1, h_direction);

	if (lp1 != lp2) {
	    if (lp2 == NULL) {
		if (h_direction + h_distance == 0) {
		    lp1 = lp0;
		    h_distance = 0;
		    display_TBUFF(parm, h_original);
		} else {
		    if (lp1 == lp0)	/* nothing to scroll for */
			h_distance = 0;
		    kbd_alarm();
		}
		result = lp1;
	    } else {
		h_distance += h_direction;
		display_LINE(parm, lp2);
		h_was_edited++;
		result = lp2;
	    }
	}
    }
    return result;
}

/*
 * Invoked on an escape-character, this processes history-editing until another
 * escape-character is entered.
 */
int
edithistory(TBUFF **buffer,
	    size_t *position,
	    int *given,
	    UINT options,
	    int (*endfunc) (EOL_ARGS),
	    int eolchar)
{
    HST param;
    BUFFER *bp;
    LINE *lp1, *lp2;
    int c = *given;

    if (!isSpecial(c)) {
	if (is_edit_char(c)
	    || ABORTED(c)
	    || (c == quotec)
	    || isSpace(c)
	    || !isCntrl(c))
	    return FALSE;
    }

    if ((bp = makeMyBuff()) == NULL)	/* something is very wrong */
	return FALSE;

    if ((lp1 = buf_head(bp)) == NULL)
	return FALSE;

    /* slightly better than global data... */
    param.buffer = buffer;
    param.position = position;
    param.endfunc = endfunc;
    param.eolchar = (eolchar == '\n') ? '\r' : eolchar;
    param.options = options;

    h_was_edited = 0;
    h_distance = 0;

    /* save the original buffer, since we expect to scroll it */
    if (tb_copy(&h_original, MyText)) {
	/* make 'original' look just like a complete command... */
	glueBufferToResult(&h_original, *buffer);
    }

    /* process char-commands */
    for_ever {
	const CMDFUNC *p;

	/* If the character is bound to up/down scrolling, scroll the
	 * history.
	 */
	h_direction = 0;	/* ...unless we find scrolling-command */
	if ((p = DefaultKeyBinding(c)) != NULL) {
	    if (CMD_U_FUNC(p) == backline)
		h_direction = -1;
	    else if (CMD_U_FUNC(p) == forwline)
		h_direction = 1;
	}
	if (ABORTED(c)) {
	    *given = c;
	    return FALSE;

	} else if ((h_direction != 0) && !isGraph(c)) {

	    if ((lp2 = hst_scroll(lp1, &param)) != NULL)
		lp1 = lp2;
	    else		/* cannot scroll */
		kbd_alarm();
	} else {
	    *given = c;
	    if (h_was_edited)
		unkeystroke(c);
	    return h_was_edited;

	}

	c = keystroke();
    }
}
#endif /* OPT_HISTORY */
