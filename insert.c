/*
 * Do various types of character insertion, including most of insert mode.
 *
 * Most code probably by Dan Lawrence or Dave Conroy for MicroEMACS
 * Extensions for vile by Paul Fox
 *
 * $Id: insert.c,v 1.193 2025/01/26 14:15:12 tom Exp $
 */

#include	"estruct.h"
#include	"edef.h"

#define	DOT_ARGUMENT	((dotcmdactive == PLAY) && dotcmdarg)

#if OPT_SHELL && SYS_UNIX && defined(SIGTSTP)	/* job control, ^Z */
#define USE_SIGTSTP 1
#else
#define USE_SIGTSTP 0
#endif

#define	BackspaceLimit() (b_val(curbp,MDBACKLIMIT) && autoindented <= 0)\
			? DOT.o\
			: w_left_margin(curwp)

/* Use this to convert from UTF-8 in cases where a character is needed rather
 * than a series of bytes.
 */
#if OPT_MULTIBYTE
#define DecodeUTF8(target, source) \
	    B_COUNT limit; \
	    UINT utf8; \
	    int test; \
 \
	    if ((vl_encoding >= enc_UTF8) \
		&& b_is_utfXX(curbp) \
		&& (limit = (B_COUNT) strlen(source)) > 1 \
		&& (test = vl_check_utf8(source, limit)) > 1 \
		&& vl_conv_to_utf32(&utf8, source, (B_COUNT) test) == test) { \
		target = (int) utf8; \
		source += test; \
	    } else
#else
#define DecodeUTF8(target, source)	/* nothing */
#endif

static int backspace(void);
static int doindent(int ind);
static int indented_newline(void);
static int indented_newline_above(void);
static int ins_anytime(int playback, int cur_count, int max_count, int *splicep);
static int insbrace(int n, int c);
static int inspound(void);
static int nextindent(int *bracefp);
static int openlines(int n);
static int shiftwidth(int f, int n);
static int tab(int f, int n);
#if !SMALLER
static int istring(int f, int n, int mode);
static int inumber(int f, int n, int mode);
#endif

/* value of insertmode maintained through subfuncs */
static int savedmode;

static int allow_aindent = TRUE;
static int skipindent;

/*--------------------------------------------------------------------------*/

/* If the wrapmargin mode is active (i.e., nonzero), and if it's not wider than
 * the screen, return the difference from the current position to the wrap
 * margin.  Otherwise, return negative.
 */
static int
past_wrapmargin(int c)
{
    int n = b_val(curbp, VAL_WRAPMARGIN);

    if (n > 0) {		/* positive, count from the right */
	n = (term.cols - (nu_width(curwp) + n));
    } else if (n == 0) {	/* zero, inactive */
	n = -1;
    } else {			/* negative, count from the left */
	n = -n;
    }

    /* now 'n' is the effective wrapmargin's column */
    if (n >= 0) {
	int list = w_val(curwp, WMDLIST);

	/* compute the effective screen column after adding the
	 * latest character
	 */
	return column_after(c, getccol(list), list) - n;
    }
    return -1;
}

/* Returns true iff wrap-margin or wrap-words is active and we'll wrap at the
 * current column.
 */
static int
wrap_at_col(int c)
{
    int rc;
    int n;

    /* if we'll split the line, there is no point in wrapping */
    if (isreturn(c)
	&& (DOT.o >= llength(DOT.l)
	    || !isSpace(lgetc(DOT.l, DOT.o)))) {
	rc = FALSE;
    } else if (past_wrapmargin(c) >= 0) {
	rc = TRUE;
    } else if (b_val(curbp, MDWRAPWORDS)
	       && (n = getfillcol(curbp)) > 0) {
	rc = (getccol(FALSE) > n);
    } else {
	rc = FALSE;
    }
    return rc;
}

/* advance one character past the current position, for 'append()' */
static void
advance_one_char(void)
{
    if (!is_header_line(DOT, curbp) && !is_at_end_of_line(DOT))
	forwchar(TRUE, 1);	/* END OF LINE HACK */
}

/* common logic for i,I,a,A commands */
static int
ins_n_times(int f, int n, int advance)
{
    int status = TRUE;
    int i;
    int flag = FALSE;

    n = need_at_least(f, n, 1);

    for (i = 0; i < n; i++) {
	if ((status = ins_anytime((i != 0), i, n, &flag)) != TRUE)
	    break;
	if (advance && !flag)
	    advance_one_char();
    }
    return status;
}

/* open lines up before this one */
int
openup(int f, int n)
{
    int s;

    n = need_a_count(f, n, 1);

    if (n < 0)
	return (FALSE);
    if (n == 0)
	return ins();

    (void) gotobol(TRUE, 1);

    /* if we are using C-indents and this is a default <NL> */
    if (allow_aindent && n == 1 &&
	(b_val(curbp, MDCINDENT) || b_val(curbp, MDAIND)) &&
	!is_header_line(DOT, curbp)) {
	s = indented_newline_above();
	if (s != TRUE)
	    return (s);

	return (ins());
    }
    s = lnewline();
    if (s != TRUE)
	return s;

    (void) backline(TRUE, 1);	/* back to the blank line */

    if (n > 1) {
	s = openlines(n - 1);
	if (s != TRUE)
	    return s;
	s = backline(TRUE, 1);	/* backup over the first one */
	if (s != TRUE)
	    return s;
    }

    return (ins());
}

/*
 * Implements the nvi/vim ex "i!" command.
 */
#if !SMALLER
int
openup_i(int f, int n)
{
    int s;
    int oallow = allow_aindent;

    allow_aindent = !oallow;
    s = openup(f, n);
    allow_aindent = oallow;
    return s;
}

/*
 * Implements the nvi/vim ex "c!" command.
 */
int
operchg_i(int f, int n)
{
    int s;
    int oallow = allow_aindent;

    allow_aindent = !oallow;
    s = operchg(f, n);
    allow_aindent = oallow;
    return s;
}
#endif /* !SMALLER */

/*
 * as above, but override all autoindenting and cmode-ing
 */
int
openup_no_aindent(int f, int n)
{
    int s;
    int oallow = allow_aindent;
    allow_aindent = FALSE;
    s = openup(f, n);
    allow_aindent = oallow;
    return s;
}

/* open lines up after this one */
int
opendown(int f, int n)
{
    int s;

    n = need_a_count(f, n, 1);

    if (n < 0)
	return (FALSE);
    if (n == 0)
	return ins();

    s = openlines(n);
    if (s != TRUE)
	return (s);

    return (ins());
}

/*
 * as above, but override all autoindenting and cmode-ing
 */
int
opendown_no_aindent(int f, int n)
{
    int s;
    int oallow = allow_aindent;
    allow_aindent = FALSE;
    s = opendown(f, n);
    allow_aindent = oallow;
    return s;
}

/*
 * Open up some blank space. The basic plan is to insert a bunch of newlines,
 * and then back up over them.
 *
 * This interprets the repeat-count for the 'o' and 'O' commands.  Unlike vi
 * (which does not use the repeat-count), this specifies the number of blank
 * lines to create before proceeding with inserting the string-argument of the
 * command.
 */
static int
openlines(int n)
{
    int i = n;			/* Insert newlines. */
    int s = TRUE;
    while (i-- && s == TRUE) {
	(void) gotoeol(FALSE, 1);
	s = newline(TRUE, 1);
    }
    if (s == TRUE && n)		/* Then back up over top */
	(void) backline(TRUE, n - 1);	/* of them all.          */

    curgoal = -1;

    return s;
}

/*
 * Implements the vi 'i' command.
 */
int
insert(int f, int n)
{
    return ins_n_times(f, n, TRUE);
}

/*
 * as above, but override all autoindenting and cmode-ing
 */
int
insert_no_aindent(int f, int n)
{
    int s;
    int oallow = allow_aindent;
    allow_aindent = FALSE;
    s = ins_n_times(f, n, TRUE);
    allow_aindent = oallow;
    return s;
}

/*
 * Implements the vi 'I' command.
 */
int
insertbol(int f, int n)
{
    if (!DOT_ARGUMENT || (dotcmdrep == dotcmdcnt))
	(void) firstnonwhite(FALSE, 1);
    return ins_n_times(f, n, TRUE);
}

/*
 * Implements the vi 'a' command.
 */
int
append(int f, int n)
{
    advance_one_char();

    return ins_n_times(f, n, !DOT_ARGUMENT);
}

/*
 * Implements the nvi/vim ex "a!" command.
 */
int
append_i(int f, int n)
{
    int code;
    int mode = allow_aindent;

    allow_aindent = !allow_aindent;
    code = opendown(f, n);
    allow_aindent = mode;

    return code;
}

/*
 * Implements the vi 'A' command.
 */
int
appendeol(int f, int n)
{
    if (!is_header_line(DOT, curbp))
	(void) gotoeol(FALSE, 0);

    return ins_n_times(f, n, TRUE);
}

/*
 * Unlike most flags on the mode-line, we'll only show the insertion-mode on
 * the current window.
 */
static void
set_insertmode(int mode)
{
    insertmode = mode;
    if (b_val(curbp, MDSHOWMODE))
	curwp->w_flag |= WFMODE;
}

/*
 * Function that returns the insertion mode if we're inserting into a given
 * window
 */
int
ins_mode(WINDOW *wp)
{
    return (wp == curwp) ? insertmode : FALSE;
}

/*
 * Implements the vi 'R' command.
 *
 * This takes an optional repeat-count and a string-argument.  The repeat-count
 * (default 1) specifies the number of times that the string argument is
 * inserted.  The length of the string-argument itself determines the number of
 * characters (beginning with the cursor position) to delete before beginning
 * the insertion.
 */
int
overwritechars(int f, int n)
{
    set_insertmode(INSMODE_OVR);
    return ins_n_times(f, n, TRUE);
}

/*
 * Implements the vi 'r' command.
 *
 * This takes an optional repeat-count and a single-character argument.  The
 * repeat-count (default 1) specifies the number of characters beginning with
 * the cursor position that are replaced by the argument.  Newline is treated
 * differently from the other characters (only one newline is inserted).
 *
 * Unlike vi, the number of characters replaced can be longer than a line.
 * Also, vile allows quoted characters.
 */
int
replacechar(int f, int n)
{
    int s = TRUE;
    int t = FALSE;
    int c;

    if (!f && is_empty_line(DOT))
	return FALSE;

    if (clexec || isnamedcmd) {
	int status;
	static char cbuf[NLINE];
	char *tp = cbuf;
	if ((status = mlreply("Replace with: ", cbuf, sizeof(cbuf))) != TRUE)
	    return status;
	{
	    DecodeUTF8(c, tp)
		c = *tp;
	}
    } else {
	set_insertmode(INSMODE_RPL);	/* need to fool SPEC prefix code */
	if (dotcmdactive != PLAY)
	    (void) update(FALSE);
	c = keystroke();
	if (ABORTED(c)) {
	    set_insertmode(FALSE);
	    return ABORT;
	}

    }
    c = kcod2key(c);

    n = need_a_count(f, n, 1);

    if (n < 0)
	s = FALSE;
    else {
	int vi_fix = (!DOT_ARGUMENT || (dotcmdrep <= 1));

	set_insertmode(INSMODE_OVR);
	if (c == quotec) {
	    t = s = quote_next(f, n);
	} else {
	    (void) ldel_chars((B_COUNT) n, FALSE);
	    if (isreturn(c)) {
		if (vi_fix)
		    s = lnewline();
	    } else {
		if (isbackspace(c)) {	/* vi beeps here */
		    s = TRUE;	/* replaced with nothing */
		} else {
		    t = s = lins_chars(n, c);
		}
	    }
	}
	if ((t == TRUE) && (DOT.o > w_left_margin(curwp)) && vi_fix)
	    s = backchar(FALSE, 1);
    }
    set_insertmode(FALSE);
    return s;
}

/*
 * Check if a command is safe to execute within insert-mode.
 */
static const CMDFUNC *
can_ins_exec(int c)
{
    const CMDFUNC *cfp = NULL;
    int always = isSpecial(c);
    int okay = FALSE;

    if (always || global_g_val(GMDINSEXEC)) {
	if ((cfp = InsertKeyBinding(c)) != NULL) {
	    okay = always;

	    if (!always) {
		/* filter out things that would be a nuisance */
		if (ABORTED(c) ||
		    isreturn(c) ||
		    isBlank(c) ||
		    isbackspace(c) ||
		    c == tocntrl('D') ||
		    c == tocntrl('T') ||
		    c == startc ||
		    c == stopc ||
#if USE_SIGTSTP			/* job control, ^Z */
		    c == suspc ||
#endif
		    c == killc ||
		    c == wkillc) {
		    /* ...unless they are rebound to user macros */
		    /* FIXME - implement check */
		    okay = FALSE;
		} else {
		    okay = TRUE;
		}
	    }

	    /*
	     * Finally, verify that the character is at least (a) bound to a
	     * goal/motion, making its movement consistent and/or (b) undoable.
	     */
	    if (okay) {
		if ((cfp->c_flags & (GOAL | MOTION)) == 0
		    && (cfp->c_flags & (UNDO | REDO)) != (UNDO | REDO)) {
		    okay = FALSE;
		}
	    }
	}
    }

    return okay ? cfp : NULL;
}

/*
 * Execute a command within the insert-mode.
 */
static int
insertion_exec(const CMDFUNC * cfp)
{
    int savedexecmode = insertmode;
    int backsp_limit = w_left_margin(curwp);

    if (curgoal < 0)
	curgoal = getccol(FALSE);

    (void) execute(cfp, FALSE, 1);
    insertmode = savedexecmode;

    return backsp_limit;
}

/*
 * This routine performs the principal decoding for insert mode (i.e.., the
 * i,I,a,A,R commands).  It is invoked via 'ins_n_times()', which loops over
 * the repeat-count for direct commands.  One complicating factor is that
 * 'ins_n_times()' (actually its callers) is called once for each repetition in
 * a '.' command.  At this level we compute the effective loop counter (the '.'
 * and the direct commands), because we have to handle the vi-compatibilty case
 * of inserting a newline.
 *
 * We stop repeating insert after the first newline in the insertion-string
 * (that's what vi does).  If a user types
 *
 *	3iABC<nl>foo<esc>
 *
 * then we want to insert
 *
 *	ABCABCABC<nl>foo
 */
static int last_insert_char;

static int
ins_anytime(int playback, int cur_count, int max_count, int *splicep)
{
#if OPT_MOUSE || OPT_B_LIMITS
    WINDOW *wp0 = curwp;
#endif
    int status;
    int c;			/* command character */
    int backsp_limit;
    static ITBUFF *insbuff;
    static int nested;
    int osavedmode;
    const CMDFUNC *cfp;
    int changed = FALSE;

    /*
     * Prevent recursion of insert-chars (it's confusing).
     */
    if (nested++
	|| is_delinked_bp(curbp)) {
	kbd_alarm();
	nested--;
	return FALSE;
    }

    if (DOT_ARGUMENT) {
	max_count = cur_count + dotcmdcnt;
	cur_count += dotcmdcnt - dotcmdrep;
    }

    if (playback && (insbuff != NULL))
	itb_first(insbuff);
    else if (!itb_init(&insbuff, esc_c)) {
	nested--;
	return FALSE;
    }

    if (insertmode == FALSE)
	set_insertmode(INSMODE_INS);
    osavedmode = savedmode;
    savedmode = insertmode;

    backsp_limit = BackspaceLimit();

    last_insert_char = EOS;

    for_ever {
	if (curwp->w_flag & WFEDIT)
	    changed++;

	/*
	 * Read another character from the insertion-string.
	 */
	c = esc_c;
	if (playback) {
	    if (*splicep && !itb_more(insbuff))
		playback = FALSE;
	    else
		c = itb_next(insbuff);
	}
	if (!playback) {
	    if (dotcmdactive != PLAY)
		(void) update(FALSE);

	    c = mapped_keystroke();

	    /*
	     * Like kbd_seq(), but we would find it too painful to
	     * also allow '#' to be used as a prefix in insert
	     * mode.
	     */
	    if (global_g_val(GMDINSEXEC)) {
		if (c == cntl_a) {
		    c = (int) (CTLA | (UINT) keystroke());
		} else if (c == cntl_x) {
		    c = (int) (CTLX | (UINT) keystroke());
		}
	    }
#if OPT_MOUSE
	    /*
	     * Prevent user from starting insertion into a
	     * modifiable buffer, then clicking on another
	     * buffer to continue inserting.  This assumes that
	     * 'setcursor()' handles entry into the other
	     * buffer.
	     */
	    if (curwp != wp0) {
		/* end insert mode for window we started in */
		wp0->w_traits.insmode = FALSE;
		if (b_val(wp0->w_bufp, MDSHOWMODE))
		    wp0->w_flag |= WFMODE;
		unkeystroke(c);
		goto leave_ins;
	    }
#endif
	    if (!itb_append(&insbuff, c)) {
		status = FALSE;
		break;
	    }
	}

	/* if we're allowed to honor SPEC bindings,
	   then see if it's bound to something, and
	   execute it */
	if ((cfp = can_ins_exec(c)) != NULL) {
	    backsp_limit = insertion_exec(cfp);
	    continue;
	} else if (isSpecial(c)) {
	    /* ignore SPEC bindings that we cannot use */
	    continue;
	}

	if (!isident(c))
	    abbr_check(&backsp_limit);

	if (isreturn(c)) {
	    if ((cur_count + 1) < max_count) {
		if (DOT_ARGUMENT) {
		    while (itb_more(dotcmd))
			(void) mapped_keystroke();
		}
		*splicep = TRUE;
		status = TRUE;
		break;
	    } else if (DOT_ARGUMENT) {
		*splicep = TRUE;
	    }
	}
	/*
	 * Decode the character
	 */
	if (ABORTED(c)) {
#if OPT_MOUSE
	  leave_ins:
#endif
	    /* an unfortunate Vi-ism that ensures one
	       can always type "ESC a" if you're not sure
	       you're in insert mode. */
	    if (DOT.o > w_left_margin(wp0))
		backchar(TRUE, 1);
	    if (autoindented >= 0) {
		(void) trimline((void *) 0, 0, 0);
		autoindented = -1;
	    }
	    if (cur_count + 1 == max_count)
		*splicep = TRUE;
	    status = TRUE;
	    break;
	} else if ((c & HIGHBIT) && b_val(curbp, MDMETAINSBIND)) {
	    /* if we're allowed to honor meta-character bindings,
	       then see if it's bound to something, and
	       insert it if not */
	    if ((cfp = can_ins_exec(c)) != NULL) {
		backsp_limit = insertion_exec(cfp);
		continue;
	    }
	}

	if (c == startc || c == stopc) {	/* ^Q and ^S */
	    continue;
	}
#if USE_SIGTSTP			/* job control, ^Z */
	else if (c == suspc) {
	    status = bktoshell(FALSE, 1);
	}
#endif
	else {
	    status = inschar(c, &backsp_limit);
	    curgoal = -1;
	}

	if (status != TRUE)
	    break;

#if OPT_CFENCE
	/* check for CMODE fence matching */
	if (b_val(curbp, MDSHOWMAT))
	    fmatch(c);
#endif

	/* do we need to do an auto-save? */
	if (b_val(curbp, MDASAVE)
	    && !b_val(curbp, MDREADONLY)) {
	    curbp->b_acount--;
	    if (curbp->b_acount <= 0) {
		(void) update(TRUE);
		filesave(FALSE, 0);
		curbp->b_acount = (short) b_val(curbp, VAL_ASAVECNT);
	    }
	}
    }

    set_insertmode(FALSE);
    savedmode = osavedmode;
    nested--;

    /*
     * If we did changes for an insert, ensure that we do some tidying up to
     * undo temporary changes to the display.
     */
    if (changed) {
	chg_buff(curbp, WFEDIT);
	update(TRUE);
    }

    return (status);
}

/* grunt routine for insert mode */
int
ins(void)
{
    int flag;
    return ins_anytime(FALSE, 1, 1, &flag);
}

static int
isallspace(LINE *ln, int lb, int ub)
{
    while (lb <= ub) {
	if (!isSpace(lgetc(ln, ub)))
	    return FALSE;
	ub--;
    }
    return TRUE;
}

/*
 * This function is used when testing for wrapping, to see if there are any
 * blanks already on the line.  We explicitly exclude blanks before the
 * autoindent margin, if any, to avoid inserting unnecessary blank lines.
 */
static int
blanks_on_line(void)
{
    int code = FALSE;
    int indentwas = b_val(curbp, MDAIND) ? previndent((int *) 0) : 0;
    int save = DOT.o;
    int list = w_val(curwp, WMDLIST);

    for (DOT.o = b_left_margin(curbp); DOT.o < llength(DOT.l); DOT.o +=
	 BytesAt(DOT.l, DOT.o)) {
	if (isSpace(CharAtDot())
	    && getccol(list) >= indentwas) {
	    code = TRUE;
	    break;
	}
    }
    DOT.o = save;
    return code;
}

/*
 * Check if we're to interpret the given character ch for C-style indenting.
 */
int
is_cindent_char(BUFFER *bp, int ch)
{
    return valid_buffer(bp)
	&& b_val(bp, MDCINDENT)
	&& (b_val_ptr(bp, VAL_CINDENT_CHARS) != NULL)
	&& (vl_index(b_val_ptr(bp, VAL_CINDENT_CHARS), ch) != NULL);
}

/*
 * Check if we're replacing text in a UTF-8 buffer.  We'll want to only delete
 * the old characters at the beginning of each UTF-8 sequence.
 */
#if OPT_MULTIBYTE
static int
is_utf8_continuation(BUFFER *bp, int ch)
{
    int result = FALSE;
    if (b_is_utfXX(bp) && (ch >= 0x80)) {
	if ((ch & 0xc0) != 0x80) {
	    result = TRUE;
	}
    }
    return result;
}
#else
#define is_utf8_continuation(bp, ch) FALSE
#endif

int
inschar(int c, int *backsp_limit_p)
{
    int rc = FALSE;
    CmdFunc execfunc;		/* ptr to function to execute */

    TRACE((T_CALLED "inschar U+%04X\n", c));
    execfunc = NULL;
    if (c == quotec) {
	execfunc = quote_next;
    } else {
	/*
	 * If a space was typed, fill column is defined, the argument
	 * is non-negative, wrap mode is enabled, and we are now past
	 * fill column, perform word wrap.
	 */
	if (wrap_at_col(c)) {
	    int offset = past_wrapmargin(c);
	    int is_print = (!isSpecial(c) && isPrint(c));
	    int is_space = (!isSpecial(c) && isSpace(c));
	    int at_end = DOT.o >= llength(DOT.l);
	    int wm_flag = (offset >= 0) || (is_space && !at_end);

	    if (is_space
		|| (is_print && (offset >= 1) && blanks_on_line())) {
		int status = wrapword(wm_flag, is_space);
		*backsp_limit_p = w_left_margin(curwp);
		if (wm_flag && is_space) {
		    returnCode(status);
		}
	    } else if (wm_flag
		       && !blanks_on_line()
		       && (c == '\t' || is_print)) {
		kbd_alarm();	/* vi beeps past the margin */
	    }
	}

	if (c == '\t') {	/* tab */
	    execfunc = tab;
	    autoindented = -1;
	} else if (isreturn(c)) {
	    execfunc = newline;
	    if (autoindented >= 0) {
		(void) trimline((void *) 0, 0, 0);
		autoindented = -1;
	    }
	    *backsp_limit_p = w_left_margin(curwp);
	} else if (isbackspace(c) ||
		   c == tocntrl('D') ||
		   c == killc ||
		   c == wkillc) {	/* ^U and ^W */
	    execfunc = nullproc;
	    /* all this says -- "is this a regular ^D for
	       backing up a shiftwidth?".  otherwise,
	       we treat it as ^U, below */
	    if (c == tocntrl('D')
		&& !(DOT.o > *backsp_limit_p
		     && ((lgetc(DOT.l, DOT.o - 1) == '0'
			  && last_insert_char == '0')
			 || (lgetc(DOT.l, DOT.o - 1) == '^'
			     && last_insert_char == '^'))
		     && isallspace(DOT.l, w_left_margin(curwp),
				   DOT.o - 2))) {
		int goal, col, sw;

		sw = shiftwid_val(curbp);
		if (autoindented >= 0)
		    *backsp_limit_p = w_left_margin(curwp);
		col = getccol(FALSE);
		if (col > 0)
		    goal = ((col - 1) / sw) * sw;
		else
		    goal = 0;
		while (col > goal &&
		       DOT.o > *backsp_limit_p) {
		    backspace();
		    col = getccol(FALSE);
		}
		if (col < goal)
		    lins_bytes(goal - col, ' ');
	    } else if (isbackspace(c)
		       && !b_val(curbp, MDBACKLIMIT)
		       && (DOT.o <= *backsp_limit_p)) {
		backspace();
	    } else {
		/* have we backed thru a "word" yet? */
		int saw_word = FALSE;

		/* was it '^^D'?  then set the flag
		   that tells us to skip a line
		   when calculating the autoindent
		   on the next newline */
		if (c == tocntrl('D') &&
		    last_insert_char == '^')
		    skipindent = 1;

		while (DOT.o > *backsp_limit_p) {
		    if (c == wkillc) {
			if (isSpace(lgetc(DOT.l,
					  DOT.o - 1))) {
			    if (saw_word)
				break;
			} else {
			    saw_word = TRUE;
			}
		    }
		    backspace();
		    autoindented--;
		    if (c != wkillc && c != killc
			&& c != tocntrl('D'))
			break;
		}
	    }
	} else if (c == tocntrl('T')) {		/* ^T */
	    execfunc = shiftwidth;
	}

	last_insert_char = c;
    }

    if (execfunc != NULL) {
	rc = (*execfunc) (FALSE, 1);
    } else {
	/* make it a real character again */
	c = kcod2key(c);

	/* if we are in overwrite mode, not at eol,
	   and next char is not a tab or we are at a tab stop,
	   delete a char forword                        */
	if ((insertmode == INSMODE_OVR)
	    && !is_utf8_continuation(curbp, c)
	    && (!DOT_ARGUMENT || (dotcmdrep <= 1))
	    && (DOT.o < llength(DOT.l))
	    && (CharAtDot() != '\t'
		|| DOT.o % tabstop_val(curbp) == tabstop_val(curbp) - 1)) {
	    autoindented = -1;
	    (void) ldel_chars((B_COUNT) 1, FALSE);
	}

	/* do the appropriate insertion */
	if (allow_aindent && b_val(curbp, MDCINDENT)) {
	    int dir;
	    if (is_cindent_char(curbp, c)
		&& is_user_fence(c, &dir)
		&& dir == REVERSE) {
		rc = insbrace(1, c);
	    } else if (c == '#' && is_cindent_char(curbp, '#')) {
		rc = inspound();
	    } else {
		autoindented = -1;
		rc = lins_chars(1, c);
	    }
	} else {
	    autoindented = -1;
	    rc = lins_chars(1, c);
	}
    }
    returnCode(rc);
}

#if ! SMALLER
int
appstring(int f, int n)
{
    TRACE((T_CALLED "appstring(f=%d, n=%d)\n", f, n));
    advance_one_char();
    returnCode(istring(f, n, INSMODE_INS));
}

int
insstring(int f, int n)
{
    TRACE((T_CALLED "insstring(f=%d, n=%d)\n", f, n));
    returnCode(istring(f, n, INSMODE_INS));
}

int
overwstring(int f, int n)
{
    TRACE((T_CALLED "overwstring(f=%d, n=%d)\n", f, n));
    returnCode(istring(f, n, INSMODE_OVR));
}

/* ask for and insert or overwrite a string into the current */
/* buffer at the current point */
static int
istring(int f, int n, int mode)
{
    char *tp;			/* pointer into string to add */
    int status;			/* status return code */
    int backsp_limit;
    static char tstring[NPAT + 1];	/* string to add */

    /* ask for string to insert */
    if ((status = mlreply("String to insert: ", tstring, NPAT)) == TRUE) {
	n = need_a_count(f, n, 1);

	if (n < 0)
	    n = -n;

	set_insertmode(mode);

	backsp_limit = BackspaceLimit();

	/* insert it */
	while (n-- > 0) {
	    tp = tstring;
	    while (*tp) {
		int c;
		DecodeUTF8(c, tp)
		    c = CharOf(*tp++);
		if ((status = inschar(c, &backsp_limit)) != TRUE) {
		    n = 0;
		    break;
		}
	    }
	}

	set_insertmode(FALSE);
    }
    return (status);
}
#endif

#if ! SMALLER
int
appnumber(int f, int n)
{
    TRACE((T_CALLED "appnumber(f=%d, n=%d)\n", f, n));
    advance_one_char();
    returnCode(inumber(f, n, INSMODE_INS));
}

int
insnumber(int f, int n)
{
    TRACE((T_CALLED "insnumber(f=%d, n=%d)\n", f, n));
    returnCode(inumber(f, n, INSMODE_INS));
}

int
overwnumber(int f, int n)
{
    TRACE((T_CALLED "overwnumber(f=%d, n=%d)\n", f, n));
    returnCode(inumber(f, n, INSMODE_OVR));
}

/* ask for and insert or overwrite a character-number into the current */
/* buffer at the current point */
static int
inumber(int f, int n, int mode)
{
    int status;			/* status return code */
    int backsp_limit;
    int value;
    static char tnumber[NPAT + 1];	/* number to add */

    /* ask for number to insert */
    if ((status = mlreply("Number to insert: ", tnumber, NPAT)) == TRUE) {
	if ((status = string_to_number(tnumber, &value)) == TRUE) {

	    n = need_a_count(f, n, 1);

	    if (n < 0)
		n = -n;

	    set_insertmode(mode);

	    backsp_limit = BackspaceLimit();

	    /* insert it */
	    while (n--) {
		if ((status = inschar(value, &backsp_limit)) != TRUE)
		    break;
	    }

	    set_insertmode(FALSE);
	}
    }
    return (status);
}
#endif

static int
backspace(void)
{
    int s;

    if ((s = backchar(TRUE, 1)) == TRUE && insertmode != INSMODE_OVR)
	s = ldel_chars((B_COUNT) 1, FALSE);
    return (s);
}

/*
 * Insert a newline. If we are in CMODE, do automatic
 * indentation as specified.
 */
int
newline(int f, int n)
{
    int s;

    n = need_a_count(f, n, 1);

    if (n < 0)
	return (FALSE);

    /* if we are in C or auto-indent modes and this is a default <NL> */
    if (allow_aindent
	&& (n == 1)
	&& (b_val(curbp, MDCINDENT) || b_val(curbp, MDAIND))
	&& !is_header_line(DOT, curbp))
	return indented_newline();

    /* insert some lines */
    while (n--) {
	if ((s = lnewline()) != TRUE)
	    return (s);
	curwp->w_flag |= WFINS;
    }
    return (TRUE);
}

/* insert a newline and indentation for C */
static int
indented_newline(void)
{
    int cmode = allow_aindent && b_val(curbp, MDCINDENT);
    int indentwas;		/* indent to reproduce */
    int bracef;			/* was there a brace at the end of line? */

    if (lnewline() == FALSE)
	return FALSE;

    indentwas = previndent(&bracef);
    skipindent = 0;

    if (cmode && bracef)
	indentwas = next_sw(indentwas);

    return doindent(indentwas);
}

/* insert a newline and indentation for autoindent */
static int
indented_newline_above(void)
{
    int cmode = allow_aindent && b_val(curbp, MDCINDENT);
    int indentwas;		/* indent to reproduce */
    int bracef;			/* was there a brace at the beginning of line? */

    indentwas = nextindent(&bracef);
    if (lnewline() == FALSE)
	return FALSE;
    if (backline(TRUE, 1) == FALSE)
	return FALSE;
    if (cmode && bracef)
	indentwas = next_sw(indentwas);

    return doindent(indentwas);
}

/*
 * Get the indent of the last previous non-blank line.  Also, if arg is
 * non-null, check if line ended in a brace.
 */
int
previndent(int *bracefp)
{
    int ind;
    int cmode = allow_aindent && is_cindent_char(curbp, '#');

    if (bracefp)
	*bracefp = FALSE;

    MK = DOT;

    /* backword() will leave us either on this line, if there's something
       non-blank here, or on the nearest previous non-blank line. */
    /* (at start of buffer, may leave us on empty line) */
    do {
	if (backword(FALSE, 1) == FALSE || is_empty_line(DOT)) {
	    (void) gomark(FALSE, 1);
	    return 0;
	}
	DOT.o = b_left_margin(curbp);
	/* if the line starts with a #, then don't copy its indent */
    } while ((skipindent-- > 0) || (cmode && lgetc(DOT.l, 0) == '#'));

    ind = indentlen(DOT.l);
    if (bracefp) {
	int lc = lastchar(DOT.l);
	int c = lgetc(DOT.l, lc);
	int dir;
	*bracefp = (lc >= 0 && ((c == ':' && is_cindent_char(curbp, ':')) ||
				(is_user_fence(c, &dir) && dir == FORWARD)));

    }

    (void) gomark(FALSE, 1);

    return ind;
}

/*
 * Get the indent of the next non-blank line.  Also, if arg is non-null, check
 * if line starts in a brace.
 */
static int
nextindent(int *bracefp)
{
    int ind;
    int fc;

    MK = DOT;

    /* we want the indent of this line if it's non-blank, or the indent
       of the next non-blank line otherwise */
    fc = firstchar(DOT.l);
    if (fc < 0 && (forwword(FALSE, 1) == FALSE
		   || (fc = firstchar(DOT.l)) < 0)) {
	if (bracefp)
	    *bracefp = FALSE;
	DOT = MK;
	return 0;
    }
    ind = indentlen(DOT.l);
    if (bracefp) {
	*bracefp = ((lgetc(DOT.l, fc) == R_CURLY) ||
		    (lgetc(DOT.l, fc) == R_PAREN) ||
		    (lgetc(DOT.l, fc) == R_BLOCK));
    }

    DOT = MK;

    return ind;
}

static int
doindent(int ind)
{
    int i, j;

    /* first clean up existing leading whitespace */
    if ((i = firstchar(DOT.l)) >= 0) {
	j = DOT.o - i;
    } else {
	j = 0;
    }
    if (j < 0)
	j = 0;
    DOT.o = w_left_margin(curwp);
    if (i > 0)
	(void) ldel_bytes((B_COUNT) i, FALSE);

    autoindented = 0;
    /* if no indent was asked for, we're done */
    if (ind > 0) {
	int tabs = tabstop_val(curbp);

	i = ind / tabs;		/* how many tabs? */
	if (i && b_val(curbp, MDTABINSERT)) {
	    autoindented += i;
	    if (tab(TRUE, i) == FALSE)
		return FALSE;
	    ind %= tabs;	/* how many spaces remaining */
	}
	if (ind > 0) {		/* only spaces now */
	    autoindented += ind;
	    if (lins_bytes(ind, ' ') == FALSE)
		return FALSE;
	}
    }
    if (!autoindented)
	autoindented = -1;

    DOT.o += j;			/* put dot back pointing to the same text as before */
    return TRUE;
}

/* return the column indent of the specified line */
int
indentlen(LINE *lp)
{
    int ind, i, c;
    ind = 0;
    for (i = 0; i < llength(lp); ++i) {
	c = lgetc(lp, i);
	if (!isSpace(c))
	    break;
	if (c == '\t')
	    ind = next_tabcol(ind);
	else
	    ++ind;
    }
    return ind;
}

/*
 * Insert a brace or paren into the text here... we are in CMODE
 *
 * n - repeat count
 * c - brace/paren to insert (always '}' or ')' for now)
 */
static int
insbrace(int n, int c)
{
#if ! OPT_CFENCE
    /* wouldn't want to back up from here, but fences might take us
       forward */
    /* if we are at the beginning of the line, no go */
    if (DOT.o <= w_left_margin(curwp))
	return (lins_bytes(n, c));
#endif

    if (autoindented < 0) {
	return lins_bytes(n, c);
    } else {
	(void) trimline((void *) 0, 0, 0);
	skipindent = 0;
#if ! OPT_CFENCE		/* no fences?  then put brace one tab in from previous line */
	doindent(((previndent(NULL) - 1) / tabstop_val(curbp)) * tabstop_val(curbp));
#else /* line up brace with the line containing its match */
	doindent(fmatchindent(c));
#endif
	autoindented = -1;

	/* and insert the required brace(s) */
	return (lins_bytes(n, c));
    }
}

/* insert a # into the text here...we are in CMODE */
static int
inspound(void)
{
    /* if we are at the beginning of the line, no go */
    if (DOT.o <= w_left_margin(curwp))
	return (lins_bytes(1, '#'));

    if (autoindented > 0) {	/* must all be whitespace before us */
	if (autoindented > llength(DOT.l))
	    autoindented = llength(DOT.l);
	DOT.o = w_left_margin(curwp);
	if (autoindented > 0)
	    (void) ldel_bytes((B_COUNT) autoindented, FALSE);
    }
    autoindented = -1;

    /* and insert the required pound */
    return (lins_bytes(1, '#'));
}

/* insert a tab into the file */
static int
tab(int f, int n)
{
    int ccol;

    n = need_a_count(f, n, 1);

    if (n <= 0)
	return FALSE;

    if (b_val(curbp, MDTABINSERT))
	return lins_bytes(n, '\t');

    ccol = getccol(FALSE);
    return lins_bytes((next_tabcol(ccol) - ccol)
		      + (n - 1) * tabstop_val(curbp), ' ');
}

/*ARGSUSED*/
static int
shiftwidth(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int logical_col;
    int char_index;
    int space_count;
    int all_white;
    int add_spaces;
    int tabs = tabstop_val(curbp);
    int c;
    int s;

    char_index = DOT.o;

    /*
     * Compute the "logical" column; i.e. the column the cursor is in on the
     * screen.
     *
     * While we're at it, compute the spaces just before the insert point.
     */

    (void) gocol(0);
    logical_col = 0;
    space_count = 0;
    all_white = TRUE;
    while (DOT.o < char_index) {
	c = CharAtDot();
	if (c == ' ') {
	    space_count++;
	} else {
	    space_count = 0;
	}
	if (!isSpace(c)) {
	    all_white = FALSE;
	}

	if (c == '\t') {
	    logical_col += tabs - (logical_col % tabs);
	} else {
	    logical_col++;
	}

	DOT.o++;
    }

    DOT.o = char_index;

    /*
     * Now we can compute the destination column. If this is the same
     * as the tab column, delete the spaces before the insert point
     * & insert a tab; otherwise, insert spaces as required.
     */
    add_spaces = next_sw(logical_col) - logical_col;
    if (space_count + add_spaces > tabs) {
	space_count = tabs - add_spaces;
    }
    if (b_val(curbp, MDTABINSERT) &&
	((add_spaces + logical_col) % tabs == 0)) {
	if (space_count > 0) {
	    DOT.o -= space_count;
	    s = ldel_bytes((B_COUNT) space_count, FALSE);
	} else {
	    space_count = 0;
	    s = TRUE;
	}
	if (s) {
	    space_count += add_spaces;
	    s = lins_bytes((space_count + tabs - 1) / tabs, '\t');
	}
    } else {
	s = lins_bytes(add_spaces, ' ');
    }

    if (all_white && s) {
	if (autoindented >= 0) {
	    int fc = firstchar(DOT.l);
	    if (fc >= 0)
		autoindented = fc;
	    else		/* all white */
		autoindented = llength(DOT.l);
	}
    }
    return s;
}

/*
 * Quote the next character, and insert it into the buffer.  That character may
 * be literal, or composed of decimal or hexadecimal digits:
 *
 * a) the newline, which always has its line splitting meaning, and
 * b) the digits are accumulated (up to a radix-based limit).
 *
 * The resulting value is inserted into the buffer.
 *
 * A character is always read, even if it is inserted 0 times, for regularity.
 */
int
quote_next(int f, int n)
{
    int c, s;

    n = need_a_count(f, n, 1);
    c = read_quoted(n, TRUE);

    if (c < 0) {
	s = ABORT;
    } else {
	if (insertmode == INSMODE_OVR)
	    (void) ldel_chars((B_COUNT) n, FALSE);
	if (c == '\n') {
	    do {
		s = lnewline();
	    } while ((s == TRUE) && (--n != 0));
	} else {
	    s = lins_chars(n, c);
	}
    }
    return s;
}

#if OPT_EVAL
const char *
current_modename(void)
{
    switch (savedmode) {
    default:
	return "command";
    case INSMODE_INS:
	return "insert";
    case INSMODE_OVR:
	return "overwrite";
    case INSMODE_RPL:
	return "replace";
    }
}
#endif
