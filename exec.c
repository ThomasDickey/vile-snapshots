/*	This file is for functions dealing with execution of
 *	commands, command lines, buffers, files and startup files
 *
 *	original by Daniel Lawrence, but
 *	much modified since then.  assign no blame to him.  -pgf
 *
 * $Id: exec.c,v 1.370 2025/01/26 15:01:34 tom Exp $
 */

#include "estruct.h"
#include "edef.h"
#include "nefunc.h"
#include "nefsms.h"

/* while-loops are described by a list of small structs.  these point
 * at the line on which they were found, and at the line to which one
 * might want to go from there, i.e.:
 *
 *    |-> while ---------
 *    |     .           |
 *    |     .           |
 *    |     .           |
 *    |     break ----  |
 *    |     .        |  |
 *    |     .        |  |
 *    --- endwhile <--<-|
 *
 * the list of structs is created in a pre-processing pass in
 * setup_dobuf() and then used in perform_dobuf().
 */

typedef struct WHLOOP {
    LINE *w_here;		/* line containing this directive */
    LINE *w_there;		/* if endwhile, ptr to while, else to endwhile */
    DIRECTIVE w_type;		/* block type: D_WHILE, D_BREAK, D_ENDWHILE */
    struct WHLOOP *w_next;
} WHLOOP;

static int token_ended_line;	/* did the last token end at end of line? */

#if !SMALLER

typedef struct locals {
    struct locals *next;
    char *name;
    char *m_value;		/* malloc'd value */
    const char *p_value;	/* pure value */
} LOCALS;

typedef struct {
    int disabled;		/* nonzero to disable execution */
    int level;			/* ~if ... ~endif nesting level */
    int fired;			/* at top-level, ~if/... used */
    REGIONSHAPE shape;
    const CMDFUNC *motion;
    LOCALS *locals;
    TBUFF *prefix;
} IFSTK;

#else /* SMALLER */

typedef struct {
    int disabled;		/* nonzero to disable execution */
} IFSTK;			/* just a dummy variable to simplify ifdef's */

#endif

static IFSTK ifstk;

/* while storing away a macro, this is where it goes.
 * if NULL, we're not storing a macro.  */
static BUFFER *macrobuffer = NULL;

/*----------------------------------------------------------------------------*/

#define isSign(c)    ((c) == '-' || (c) == '+')
#define isPattern(c) ((c) == '?' || (c) == '/')

/* on entry, s points to a pattern delimiter */
static const char *
skip_pattern(const char *s, int *done)
{
    int ch = *s++;

    while (*s != ch && *s != EOS) {
	if (*s == BACKSLASH) {
	    if (*++s == EOS) {
		/* FIXME let's not try to escape a null */
		break;
	    }
	}
	++s;
    }
    if (*s == EOS) {
	*done = FALSE;
    } else {
	++s;
    }
    return s;
}

/*
 * Like parse_linespec(), but does not update marks.
 */
static const char *
skip_linespec(const char *s, const char **before, int *done)
{
    /* parse each ;-delimited clause of this linespec */
    do {
	/* skip an initial ';', if any */
	if (*s == ';')
	    s++;

	s = skip_cblanks(s);	/* skip leading spaces */

	*done = TRUE;		/* we usually complete the parse */
	*before = s;		/* remember what we tried to parse */
	if (*s == '.') {	/* dot means current position */
	    s++;
	} else if (*s == '$') {	/* '$' means the last line */
	    s++;
	} else if (isDigit(*s)) {
	    s = skip_cnumber(s);
	} else if (*s == SQUOTE) {
	    /* apostrophe means go to a set mark */
	    s += 2;		/* FIXME: see making_mark() */
	} else if (isSign(*s)) {
	    int ch = *s;
	    s = skip_cnumber(s + 1);
	    while (*s == ch)
		s++;
	} else if (isPattern(*s)) {
	    s = skip_pattern(s, done);
	}

	/* maybe add an offset */
	if (isSign(*s)) {
	    *before = s;
	    s = skip_cnumber(s + 1);
	}
    } while (*s == ';' || isSign(*s));

    return s;
}

/* on entry, s points to a digit */
static const char *
parse_number(const char *s, int *number)
{
    *number = 0;
    while (isDigit(*s)) {
	*number = (*number * 10) + *s - '0';
	++s;
    }
    return s;
}

/* on entry, s points to a sign */
static const char *
parse_signed_number(const char *s, int *number)
{
    int ch = *s++;

    s = parse_number(s, number);
    if (*number == 0)
	*number = 1;
    while (*s == ch) {
	++s;
	*number += 1;
    }
    if (ch == '-')
	*number = -(*number);
    return s;
}

/*
 * parse an ex-style line spec -- code originally from elvis, file ex.c, by
 * Steve Kirkendall
 */
static const char *
parse_linespec(const char *s, LINE **markptr)
{
    const char *base = s;
    int num;
    LINE *lp;			/* where the linespec takes us */
    int status;

    (void) setmark();
    lp = NULL;

    /* parse each ;-delimited clause of this linespec */
    do {
	/* skip an initial ';', if any */
	if (*s == ';')
	    s++;

	s = skip_cblanks(s);	/* skip leading spaces */

	if (*s == '.') {
	    /* dot means current position */
	    s++;
	    lp = DOT.l;
	} else if (*s == '$') {
	    /* '$' means the last line */
	    s++;
	    status = gotoeob(TRUE, 1);
	    if (status)
		lp = DOT.l;
	} else if (isDigit(*s)) {
	    /* digit means an absolute line number */
	    s = parse_number(s, &num);
	    status = gotoline(TRUE, num);
	    if (status)
		lp = DOT.l;
	} else if (*s == SQUOTE) {
	    /* apostrophe means go to a set mark */
	    s++;
	    status = gonmmark(*s);
	    if (status)
		lp = DOT.l;
	    s++;
	} else if (isSign(*s)) {
	    s = parse_signed_number(s, &num);
	    status = forwline(TRUE, num);
	    if (status)
		lp = DOT.l;
	} else if (isPattern(*s)) {
	    int done = 1;
	    int found = FALSE;
	    const char *save = s;
	    MARK save_DOT = DOT;

	    last_srch_direc = (*save == '/') ? FORWARD : REVERSE;
	    s = skip_pattern(save, &done);
	    tb_init(&searchpat, EOS);
	    tb_bappend(&searchpat, save + 1, (size_t) (s - save - 1 - done));
	    tb_append(&searchpat, EOS);

	    if (tb_length(searchpat) > 1 &&
		(gregexp = regcomp(tb_values(searchpat),
				   tb_length(searchpat) - 1,
				   b_val(curbp, MDMAGIC))) != NULL) {

		scanboundry(TRUE, DOT, last_srch_direc);
		if (scanner(gregexp,
			    last_srch_direc,
			    (DOT.o == 0),
			    FALSE,
			    FALSE,
			    (int *) 0)) {
		    lp = DOT.l;
		    found = TRUE;
		}
		DOT = save_DOT;
		if (!found)
		    mlwarn("[Not found: %s]", tb_values(searchpat));
	    } else {
		mlwarn("[Not a legal expression: %s]", tb_values(searchpat));
	    }
	} else if (*s == EOS) {	/* empty string matches '.' */
	    lp = DOT.l;
	}

	/* if linespec was faulty, quit now */
	if (!lp) {
	    break;
	}

	/* maybe add an offset */
	if (isSign(*s)) {
	    s = parse_signed_number(s, &num);
	    if (forwline(TRUE, num) == TRUE)
		lp = DOT.l;
	}
    } while (*s == ';' || isSign(*s));

    *markptr = lp;

    /*
     * If we're processing a script (for example), which has no line-specifiers,
     * we'll get a null pointer here.  But there's no reason to flag a bug in
     * swapmark().
     */
    if (lp == NULL && (s == base)) {
	DOT = MK;
    } else {
	swapmark();
    }
    return s;
}

/*
 * parse an ex-style line range -- code originally from elvis, file ex.c, by
 * Steve Kirkendall
 */
static int
rangespec(const char *specp,
	  LINE **fromlinep,
	  LINE **tolinep,
	  CMDFLAGS * flagp,
	  int testonly)
{
    const char *scan;		/* used to scan thru specp */
    LINE *fromline;		/* first linespec */
    LINE *toline;		/* second linespec */

    *flagp = 0;

    if (specp == NULL)
	return -1;

    /* ignore command lines that start with a double-quote */
    if (*specp == '"') {
	*fromlinep = *tolinep = DOT.l;
	return 1;
    }

    /* permit extra colons at the start of the line */
    while (isBlank(*specp) || *specp == ':') {
	specp++;
    }

    /* parse the line specifier */
    scan = specp;
    if (*scan == '0') {
	fromline = toline = buf_head(curbp);	/* _very_ top of buffer */
	*flagp |= (FROM | ZERO);
	scan++;
    } else if (*scan == '%') {
	/* '%' means all lines */
	fromline = lforw(buf_head(curbp));
	toline = lback(buf_head(curbp));
	scan++;
	*flagp |= FROM_TO | (FROM | TO);
    } else {
	scan = parse_linespec(scan, &toline);
	*flagp |= FROM;
	if (toline == NULL)
	    toline = DOT.l;
	fromline = toline;
	while (*scan == ',') {
	    fromline = toline;
	    scan++;
	    scan = parse_linespec(scan, &toline);
	    *flagp |= TO;
	    if (toline == NULL) {
		TRACE(("...faulty line spec\n"));
		return -1;
	    }
	}
    }

    if (is_empty_buf(curbp))
	fromline = toline = NULL;

    if (scan == specp)
	*flagp |= DFLALL;

    /* skip whitespace */
    scan = skip_cblanks(scan);

    if (*scan && !testonly) {
	TRACE(("...crud at end %s (%s)\n", specp, scan));
	return -1;
    }

    *fromlinep = fromline;
    *tolinep = toline;

    return (int) (scan - specp);
}

/* special test for 'a style mark references */
static int
making_mark(EOL_ARGS)
{
    (void) eolchar;

    return (cpos != 0
	    && buffer[cpos - 1] == SQUOTE
	    && can_set_nmmark(c));
}

const char *
skip_linespecs(const char *buffer, int cpos, int *done)
{
    const char *src = buffer;
    const char *before = NULL;

    TRACE(("skip_linespecs(%d) \"%.*s\"\n", cpos, cpos, buffer));
    if (cpos != 0) {
	/* skip over previous linespec's */
	while ((src - buffer) < cpos) {
	    src = skip_linespec(src, &before, done);
	    TRACE(("after linespec %d(%d):%.*s\n",
		   *done,
		   (int) (cpos - (src - buffer)),
		   (int) (cpos - (src - buffer)),
		   src));
	    if (*src != ',')
		break;
	    ++src;
	}
    }
    return src;
}

/*
 * Check if we've started accepting a /pattern/ or ?pattern?, and not completed
 * it.  In those cases, we accept further characters in eol_range().
 */
static int
making_pattern(EOL_ARGS)
{
    int done = FALSE;
    int in_regexp = FALSE;

    (void) eolchar;

    TRACE(("making_pattern(buffer=%.*s, cpos=%d, c=%#x, eolchar=%#x)\n",
	   (int) cpos, buffer, (int) cpos, c, eolchar));
    if (cpos != 0) {
	(void) skip_linespecs(buffer, (int) cpos, &done);
	in_regexp = !done || isPattern(c);
    } else if (isPattern(c)) {
	in_regexp = TRUE;
    }
    TRACE(("...making_pattern:%d\n", in_regexp));
    return in_regexp;
}

/*ARGSUSED*/
static int
eol_range(EOL_ARGS)
{
    int result = FALSE;

    (void) eolchar;

    if (is_edit_char(c)) {
	result = FALSE;
    } else if (isSpecial(c) || isCntrl(c)) {
	result = TRUE;
    } else if (islinespec(c)
	       || (c == ':' && (cpos == 0 || buffer[cpos - 1] == c))
	       || making_mark(buffer, cpos, c, eolchar)
	       || making_pattern(buffer, cpos, c, eolchar)) {
	result = FALSE;
    } else {
	result = TRUE;
    }
    return result;
}

/*
 * Returns true iff the user ended the last prompt with a carriage-return.
 */
static int
end_of_cmd(void)
{
    int c = end_string();
    return isreturn(c);
}

/* returns true iff we are in a named-command and if the user ended the last
 * prompt with a carriage-return.
 */
int
end_named_cmd(void)
{
    int result = FALSE;

    if (clexec) {
	result = token_ended_line;
    } else if (isnamedcmd) {
	result = end_of_cmd();
    }
    return result;
}

/* returns true iff we are in a named-command and if the user did not end the
 * last prompt with a carriage-return.
 */
int
more_named_cmd(void)
{
    int result = FALSE;

    if (clexec) {
	result = !token_ended_line;
    } else if (isnamedcmd) {
	result = !end_of_cmd();
    }
    return result;
}

/*
 * Scrolling through the history buffer may bring up a selection containing
 * a line/range specifier and arguments past the command-verb.  Check for,
 * and adjust for these cases:
 * a) no change is needed
 * b) strip arguments
 * c) back up to put the command-verb into the fifo, returning TRUE.
 * If no back-up is needed, this function returns FALSE.
 */
static int
resplice_command(TBUFF **lspec, const char *cspec)
{
    int result = FALSE;
    int end_lspec = 0;
    int end_input = (int) strlen(cspec);
    LINE *fromline = NULL;
    LINE *toline = NULL;
    CMDFLAGS lflag = 0;

    if (tb_length(*lspec) == 1 && *tb_values(*lspec) == EOS) {
	end_lspec = rangespec(cspec, &fromline, &toline, &lflag, TRUE);
	if (end_lspec > 0) {
	    while (end_input-- > 0) {
		unkeystroke(cspec[end_input]);
	    }
	    result = TRUE;
	}
    }
    return result;
}

/*
 * namedcmd:	execute a named command even if it is not bound
 */

#if SMALLER
#define execute_named_command namedcmd
#else
static				/* next procedure is local */
#endif

#ifndef DEBUG_KPOS
#define DEBUG_KPOS 0
#endif

#if DEBUG_KPOS
#define TRACE_KPOS(s) TRACE(s)
#else
#define TRACE_KPOS(s)		/* nothing */
#endif

int
execute_named_command(int f, int n)
{
    static char empty[] = "";

    int status;
    const CMDFUNC *cfp;		/* function to execute */
    char *fnp;			/* ptr to the name of the cmd to exec */

    LINE *fromline;		/* first linespec */
    LINE *toline;		/* second linespec */
    MARK save_DOT;
    MARK last_DOT;
    static TBUFF *lspec;	/* text of line spec */
    char cspec[NLINE];		/* text of command spec */
    int cmode = 0;
    int c;
    char repeat_cmd = EOS;
    int maybe_reg, maybe_num;
    CMDFLAGS lflag, flags;

    tb_scopy(&lspec, "");
    last_DOT = DOT;

    TRACE_KPOS(("kpos before %d.%d\n", line_no(curbp, DOT.l), DOT.o));

    /* prompt the user to type a named command */
#ifdef SMALLER
    mlprompt(": ");
#else
    mlprompt(tb_values(prompt_string));
#endif

    /* and now get the function name to execute */
    for_ever {
	if (cmode == 0) {	/* looking for range-spec, if any */
	    status = kbd_reply((char *) 0,	/* no-prompt => splice */
			       &lspec,	/* in/out buffer */
			       eol_range,
			       EOS,	/* may be a conflict */
			       0,	/* no expansion, etc. */
			       no_completion);
	    c = end_string();
	    if (status != TRUE && status != FALSE) {
		if (status == SORTOFTRUE) {
		    (void) keystroke();		/* cancel ungetc */
		    return FALSE;
		}
		return status;
	    } else if (isreturn(c) && (status == FALSE)) {
		return FALSE;
	    } else {
		if (!clexec)	/* if there's an I/O vs a buffer */
		    unkeystroke(c);	/* ...so we can splice */
	    }
	    cmode = 1;
	    repeat_cmd = EOS;
	} else {		/* looking for command-name */
	    fnp = NULL;
	    status = kbd_engl_stat((char *) 0, cspec, 0, KBD_STATED);
	    if (status == TRUE) {
		fnp = cspec;
#if !SMALLER
		if (isRepeatable(*fnp) && clexec) {
		    repeat_cmd = *fnp;
		    cmode++;
		    hst_glue(EOS);
		} else
#endif
		if (isRepeatable(*fnp) && (*fnp == end_string())) {
		    unkeystroke(*fnp);
		    repeat_cmd = *fnp;
		    cmode++;
		    hst_glue(EOS);
		} else {
		    c = fnp[strlen(fnp) - 1];
		    if (isPunct(c) && (c != '&')) {
			c = end_string();
			if (c != NAMEC
			    && c != ' '
			    && !isreturn(c)) {
			    unkeystroke(c);
			    /* e.g., !-command */
			}
		    } else {	/* e.g., ":e#" */
			c = end_string();
			if (isPunct(c)
			    && vl_index(global_g_val_ptr(GVAL_EXPAND_CHARS),
					c) != NULL) {
			    unkeystroke(c);
			}
		    }
		    if (resplice_command(&lspec, cspec)) {
			cmode = 0;
			hst_reset();
			hst_init(EOS);
			continue;
		    }
		    break;
		}
	    } else if (status == SORTOFTRUE) {
		hst_remove((cmode > 1) ? "*" : "");
		if (cmode > 1)
		    (void) keystroke();		/* eat the delete */
		if (--cmode <= 1) {
		    cmode = 0;
		    hst_remove(tb_values(lspec));
		}
	    } else if ((status == ABORT) || (lastkey == killc)) {
		return status;
	    } else {		/* status == FALSE (killc/wkillc) */
		if (cmode > 1) {
		    fnp = cspec;
		    cmode--;
		    break;
		} else {
		    if (tb_length(lspec) <= 1) {
			return status;
		    } else {
			break;	/* range-only */
		    }
		}
	    }
	}
    }

    /* reconstruct command if user edited it */
    if (repeat_cmd != EOS && fnp != NULL && *fnp == EOS) {
	fnp[0] = repeat_cmd;
	fnp[1] = EOS;
    }

    /* infer repeat count from repeated-command */
    if (cmode > 1) {
	if (!f) {
	    f = TRUE;
	    n = cmode;
	} else {
	    mlforce("[Redundant repeat-count]");
	    return FALSE;
	}
    }

    /* parse the accumulated lspec */
    if (rangespec(tb_values(lspec), &fromline, &toline, &lflag, FALSE) < 0) {
	mlforce("[Improper line range '%s']", tb_values(lspec));
	return FALSE;
    }

    /* if range given, and it wasn't "0" and the buffer's empty */
    if (!(lflag & (DFLALL | ZERO)) && is_empty_buf(curbp)) {
	mlforce("[No range possible in empty buffer '%s']", fnp);
	return FALSE;
    }

    /* did we get a name? */
    if (fnp == NULL) {
	if ((lflag & DFLALL)) {	/* no range, no function */
	    return FALSE;
	} else {		/* range, no function */
	    cfp = &f_gomark;
	    fnp = empty;
	}
    } else if ((cfp = engl2fnc(fnp)) == NULL) {		/* bad function */
	return no_such_function(fnp);
    }

    ev_end_of_cmd = end_of_cmd();
    flags = cfp->c_flags;

    /* bad arguments? */
#ifdef EXRC_FILES
    /*
     * seems like we need one more check here-- is it from a.exrc file ?
     * cmd not ok in.exrc empty file
     */
    if (!(flags & EXRCOK) && is_empty_buf(curbp)) {
	mlforce("[Can't use the \"%s\" command in a %s file.]",
		cmdnames[cmdidx].name, EXRC);
	return FALSE;
    }
#endif

    /* was: if (!(flags & (ZERO | EXRCOK)) && fromline == NULL ) */
    if ((lflag & ZERO)) {
	if (!(flags & ZERO)) {
	    mlforce("[Can't use address 0 with \"%s\" command]", fnp);
	    return FALSE;
	}
	if (fromline == NULL)
	    fromline = buf_head(curbp);		/* buffer is empty */

	/*  we're positioned at fromline == curbp->b_linep, so commands
	   must be willing to go _down_ from there.  Seems easiest
	   to special case the commands that prefer going up */
	if (cfp == &f_insfile) {
	    /* EMPTY */ ;
	} else if (cfp == &f_lineputafter) {
	    cfp = &f_lineputbefore;
	    fromline = lforw(fromline);
	} else if (cfp == &f_opendown) {
	    cfp = &f_openup;
	    fromline = lforw(fromline);
	} else if (cfp == &f_gomark) {
	    fromline = lforw(fromline);
	} else {
	    mlforce("[Configuration error: ZERO]");
	    return FALSE;
	}
	flags = cfp->c_flags;
	toline = fromline;
    }

    /* if we're not supposed to have a line no., and the line no. isn't
       the current line, and there's more than one line */
    if (!(flags & FROM) && (fromline != DOT.l) &&
	!is_empty_buf(curbp) &&
	(lforw(lforw(buf_head(curbp))) != buf_head(curbp))) {
	mlforce("[Can't use address with \"%s\" command.]", fnp);
	return FALSE;
    }
    /* if we're not supposed to have a second line no., and the line no.
       isn't the same as the first line no., and there's more than
       one line */
    if (!(flags & TO) && (toline != fromline) &&
	!is_empty_buf(curbp) &&
	(lforw(lforw(buf_head(curbp))) != buf_head(curbp))) {
	mlforce("[Can't use a range with \"%s\" command.]", fnp);
	return FALSE;
    }
    TRACE_KPOS(("kpos from %d\n", line_no(curbp, fromline)));
    TRACE_KPOS(("kpos to   %d\n", line_no(curbp, toline)));

    /* some commands have special default ranges */
    if (lflag & DFLALL) {
	if (flags & DFLALL) {
	    if (cfp == &f_showlength) {
		fromline = lforw(buf_head(curbp));
		toline = lback(buf_head(curbp));
	    } else if (cfp == &f_operwrite) {
		cfp = &f_filewrite;
	    } else if (cfp == &f_operglobals) {
		cfp = &f_globals;
	    } else if (cfp == &f_opervglobals) {
		cfp = &f_vglobals;
	    } else {
		mlforce("[Configuration error: DFLALL]");
		return FALSE;
	    }
	    lflag |= (FROM | TO);	/* avoid prompt for line-count */
	} else if (flags & DFLNONE) {
#if OPT_SHELL
	    if (cfp == &f_operfilter) {
		cfp = &f_vl_spawn;
		(void) setmark();	/* not that it matters */
	    } else
#endif
#if OPT_PERL
	    if (cfp == &f_perl) {
		perl_default_region();
	    } else
#endif
	    {
		mlforce("[Configuration error: DFLNONE]");
		return FALSE;
	    }
	    fromline = toline = NULL;
	    lflag |= (FROM | TO);	/* avoid prompt for line-count */
	} else
	    lflag &= ~DFLALL;
    }

    /* Process argument(s) for named-buffer & line-count, if present.  The
     * line-count is expected only if no "to" address specification was
     * given.
     */
    if ((*fnp != EOS) && !end_of_cmd() && !isPunct(end_string())) {
	maybe_reg = ((flags & OPTREG) == OPTREG);
	maybe_num = ((flags & TO) == TO)
	    && !((lflag & TO) == TO);
	if (maybe_num && (lflag & FROM)) {
	    toline = fromline;
	    maybe_num = FALSE;
	}
    } else {
	maybe_reg = FALSE;
	maybe_num = FALSE;
    }

    if (maybe_reg || maybe_num) {
	int num;
	int that = (maybe_num && maybe_reg) ? 0 : (maybe_num ? 1 : -1);
	int last = maybe_num ? 2 : 1;

	while (!end_of_cmd() && (that < last)) {
	    status = mlreply_reg_count(fnc2engl(cfp), that, &num, &that);
	    if (status == ABORT)
		return status;
	    else if (status != TRUE)
		break;
	    if (that == 2) {
		if (is_empty_buf(curbp)) {
		    mlforce("[No line count possible in empty buffer '%s']", fnp);
		    return FALSE;
		}
		swapmark();
		DOT.l = fromline;
		(void) forwline(TRUE, num - 1);
		toline = DOT.l;
		swapmark();
	    } else {
		ukb = (short) num;
		kregflag = (USHORT) ((that == 1) ? KAPPEND : 0);
		that = 1;
		/* patch: need to handle recursion */
	    }
	}
    }

    save_DOT = DOT;
    TRACE_KPOS(("kpos save %d.%d\n", line_no(curbp, DOT.l), DOT.o));

    if (!(flags & NOMOVE)
	&& ((toline != NULL) || (fromline != NULL))) {
	/* assume it's an absolute motion */
	/* we could probably do better */
	curwp->w_lastdot = DOT;
    }
    if ((toline != NULL) && (flags & TO)) {
	DOT.l = toline;
	(void) firstnonwhite(FALSE, 1);
	(void) setmark();
    }
    if ((fromline != NULL) && (flags & FROM)) {
	DOT.l = fromline;
	(void) firstnonwhite(FALSE, 1);
	if (toline == NULL)
	    (void) setmark();
    }
    if (!sameline(last_DOT, DOT))
	curwp->w_flag |= WFMOVE;

    /* and then execute the command */
    isnamedcmd = TRUE;
    havemotion = &f_gomark;
    regionshape = rgn_FULLLINE;

    /* if the command ended with a bang, let the function know
       that so it can take special action */
    if ((flags & BANG) && (fnp[strlen(fnp) - 1] == '!')) {
	f = TRUE;
	n = SPECIAL_BANG_ARG;
    }

    /*
     * Ensure that we've completed the command properly.  (We could make
     * the same check after 'execute()', but that would tend to obscure
     * informational/warning messages...).
     */
    if (flags == NONE
	&& end_string() != ' '
	&& !end_of_cmd()) {
	status = FALSE;
	if (!mapped_c_avail()) {
	    unkeystroke(end_string());
	}
	(void) kbd_seq();
	mlforce("[Extra characters after \"%s\" command]", fnp);
    } else {
	status = execute(cfp, f, n);
    }

    havemotion = NULL;
    isnamedcmd = FALSE;
    regionshape = rgn_EXACT;

    /* don't use this if the command modifies! */
    if (flags & NOMOVE)
	restore_dot(save_DOT);
    else if (status == ABORT)
	restore_dot(last_DOT);
#ifdef GVAL_KEEP_POS
    else {
	TRACE_KPOS(("kpos restoring\n"));
	switch (global_g_val(GVAL_KEEP_POS)) {
	case KPOS_VILE:
	    break;
	case KPOS_NVI:
	    if ((lflag & FROM_TO) &&
		(cfp == &f_operrshift || cfp == &f_operlshift)) {
		flags |= NOMOVE;
		TRACE_KPOS(("kpos restore bot\n"));
	    } else if (toline != NULL &&
		       fromline != NULL) {
		int dotline = line_no(curbp, save_DOT.l);
		if (line_no(curbp, toline) < dotline) {
		    save_DOT.l = toline;
		    save_DOT.o = 0;
		    flags |= NOMOVE;
		    TRACE_KPOS(("kpos restore to %d\n", line_no(curbp, toline)));
		} else if (line_no(curbp, fromline) > dotline) {
		    save_DOT.l = toline;
		    save_DOT.o = 0;
		    flags |= NOMOVE;
		    TRACE_KPOS(("kpos restore from %d\n", line_no(curbp, fromline)));
		} else {
		    flags |= NOMOVE;
		    TRACE_KPOS(("kpos restore ignored to/from\n"));
		}
	    } else {
		TRACE_KPOS(("kpos restore failed\n"));
	    }
	    break;
	case KPOS_VI:
	    save_DOT.l = toline;
	    save_DOT.o = 0;
	    flags |= NOMOVE;
	    break;
	}
	if (flags & NOMOVE) {
	    restore_dot(save_DOT);
	}
    }
#endif

    TRACE_KPOS(("kpos after %d.%d\n", line_no(curbp, DOT.l), DOT.o));

    return status;
}

#if !SMALLER
/* intercept calls on 'namedcmd()' to allow logging of all commands, even
 * those that have errors in them.
 */
int
namedcmd(int f, int n)
{
    int status;

    hst_init(EOS);
    status = execute_named_command(f, n);
    hst_flush();

    if (clexec)
	map_drain();		/* don't let stray characters go too far */

    return status;
}
#endif

/*
 * take a passed string as a command line and translate it to be executed as a
 * command.  This function will be used by execute-command-line and by all
 * source and startup files.
 *
 * format of the command line is:
 *
 *	{# arg} <command-name> {<argument string(s)>}
 */
int
docmd(char *cline, int execflag, int f, int n)
{
    int status;			/* return status of function */
    int oldcle;			/* old contents of clexec flag */
    char *oldestr;		/* original exec string */
    TBUFF *tok = NULL;
    char *token;		/* next token off of command line */
    const CMDFUNC *cfp;

    TRACE((T_CALLED "docmd(cline=%s, execflag=%d, f=%d, n=%d)\n",
	   str_visible(cline), execflag, f, n));

    set_end_string(EOS);
    oldestr = execstr;		/* save ptr to starting command string to execute */
    execstr = cline;		/* and change execstr to the new one */

    do {
	if ((token = mac_unquotedarg(&tok)) == NULL) {	/* grab first token */
	    execstr = oldestr;
	    returnCode(FALSE);
	}
	if (*token == ':') {	/* allow leading ':' on line */
	    int j;
	    for (j = 0; (token[j] = token[j + 1]) != EOS; j++) ;
	}
    } while (!*token);

    /*
     * If it doesn't look like a command (and command names can't
     * be hidden in variables), then it must be a leading argument, e.g., a
     * repeat-count (CNT).
     */
    if (toktyp(token) != TOK_LITSTR || isDigit(token[0])) {
	f = TRUE;
	n = (int) strtol(tokval(token), NULL, 0);

	/* and now get the command to execute */
	if ((token = mac_unquotedarg(&tok)) == NULL) {
	    execstr = oldestr;
	    returnCode(FALSE);
	}
    }

    /* and match the token to see if it exists */
    if ((cfp = engl2fnc(token)) == NULL) {
	status = no_such_function(token);
    } else {
	/* save the arguments and go execute the command */
	oldcle = clexec;	/* save old clexec flag */
	clexec = execflag;	/* in cline execution */
	/*
	 * Flag the first time through for some commands -- e.g.  subst
	 * must know to not prompt for strings again, and pregion must
	 * only restart the p-lines buffer once for each command.
	 */
	calledbefore = FALSE;
	status = execute(cfp, f, n);
	setcmdstatus(status);
	clexec = oldcle;
    }

    execstr = oldestr;
    tb_free(&tok);
    returnCode(status);
}

/*
 *  Call the appropriate action for a given CMDFUNC
 */
int
call_cmdfunc(const CMDFUNC * p, int f, int n)
{
    int status = FALSE;

    switch (p->c_flags & CMD_TYPE) {
    case CMD_FUNC:		/* normal CmdFunc */
	status = (CMD_U_FUNC(p)) (f, n);
	break;

#if OPT_NAMEBST
#if OPT_PROCEDURES
    case CMD_PROC:		/* named procedure */
	status = dobuf(CMD_U_BUFF(p), f ? n : 1, -1);
	break;

    case CMD_OPER:		/* user-defined operator */
	regionshape = rgn_EXACT;	/* FIXME */
	opcmd = OPOTHER;
	status = vile_op(f, n, user_operator, CMD_U_BUFF(p)->b_bname);
	break;
#endif

#if OPT_PERL
    case CMD_PERL:		/* perl subroutine */
	status = perl_call_sub(CMD_U_PERL(p), (int) (p->c_flags & OPER), f, n);
	break;
#endif
#endif /* OPT_NAMEBST */
    default:
	mlforce("BUG: invalid CMDFUNC type");
	status = FALSE;
	break;
    }
    return status;
}

/*
 * This is the general command execution routine. It takes care of checking
 * flags, globals, etc, to be sure we're not doing something dumb.
 * Return the status of command.
 */
int
execute(const CMDFUNC * execfunc, int f, int n)
{
    int status;
    CMDFLAGS flags;

    if (execfunc == NULL) {
#if OPT_REBIND
	mlwarn("[Key not bound]");	/* complain             */
#else
	mlwarn("[Not a command]");	/* complain             */
#endif
	return (FALSE);
    }

    TRACE((T_CALLED "execute(execfunc=%p(%s:%s), f=%d, n=%d)\n",
	   (const void *) execfunc,
	   TRACE_CMDFUNC(execfunc),
	   f, n));

    flags = execfunc->c_flags;

    /* commands following operators can't be redone or undone */
    if (!doingopcmd && !doingsweep) {
	/* don't record non-redoable cmds, */
	/* but if we're in insertmode, it's okay, since we must
	   be executing a function key, like an arrow key,
	   that the user will want to have replayed later */
	if ((!insertmode) && (flags & REDO) == 0)
	    dotcmdstop();

	if (flags & UNDO) {
	    /* undoable command can't be permitted when read-only */
	    if (!(flags & VIEWOK)) {
		if (b_val(curbp, MDVIEW)) {
		    returnCode(rdonly());
		}
#ifdef MDCHK_MODTIME
		if (!b_is_changed(curbp) &&
		    !ask_shouldchange(curbp)) {
		    returnCode(FALSE);
		}
#endif
	    }
	    if (!kbd_replaying(FALSE))
		mayneedundo();
	}
    }

    if (dotcmdactive != PLAY) {
	if (execfunc != &f_dotcmdplay) {
	    /* reset dotcmdkreg on any command where ukb is
	     * unspecified.  usekreg() does it on the one's
	     * where it is specified.  */
	    if (ukb == 0)
		dotcmdkreg = 0;

	    /* override the saved dot-cmd argument, if this
	       is a new redoable command */
	    if (flags & REDO)
		dotcmdarg = FALSE;
	}
    } else {
	/* if we _are_ playing, re-use the previously kreg */
	if (dotcmdkreg != 0)
	    ukb = dotcmdkreg;
    }

    if (curwp->w_tentative_lastdot.l == NULL)
	curwp->w_tentative_lastdot = DOT;

    status = call_cmdfunc(execfunc, f, n);
    if ((flags & GOAL) == 0) {	/* goal should not be retained */
	curgoal = -1;
    }

    /* if motion was absolute, and it wasn't just on behalf of an
       operator, and we moved, update the "last dot" mark */
    if ((flags & ABSM) && !doingopcmd &&
	!sameline(DOT, curwp->w_tentative_lastdot)) {
	curwp->w_lastdot = curwp->w_tentative_lastdot;
    }

    curwp->w_tentative_lastdot = DOT;

    returnCode(status);
}

/*
 * Chop a token off a string 'src' return a pointer past the token in 'tok'.
 * 'eolchar' is the nominal delimiter we expect (perhaps '=' for the "set"
 * command), and 'actual' returns the first character past the token that we
 * actually found.
 */
char *
get_token(char *src, TBUFF **tok, int (*endfunc) (EOL_ARGS), int eolchar, int *actual)
{
    tb_init(tok, EOS);
    return get_token2(src, tok, endfunc, eolchar, actual);
}

/*
 * This is used for the "map" command, which doesn't honor anything except ^V.
 */
char *
get_token1(char *src, TBUFF **tok, int (*endfunc) (EOL_ARGS), int eolchar, int *actual)
{
    int c, chr;

    if (actual != NULL)
	*actual = EOS;
    if (src == NULL)
	return src;

    /* first scan past any whitespace in the source string */
    src = skip_space_tab(src);

    /* scan through the source string, which may be quoted */
    while ((c = *src) != EOS) {
	if (c == quotec) {
	    if (*++src == EOS)
		break;
	}
#if OPT_MODELINE
	/*
	 * Multiple settings on the line may be separated by colons.
	 */
	else if (c == ':' && in_modeline) {
	    if (actual != NULL)
		*actual = ' ';
	    src++;
	    break;
	} else
#endif
	if (c == eolchar) {
	    if (actual != NULL)
		*actual = *src;
	    if (!isBlank(c))
		src++;
	    break;
	} else if (endfunc(tb_values(*tok), tb_length(*tok), c, eolchar)) {
	    if (actual != NULL)
		*actual = *src;
	    break;
	} else if (isBlank(c)) {
	    if (actual != NULL)
		*actual = *src;
	    break;
	}

	chr = *src++;		/* record the character */
	tb_append(tok, chr);
    }

    /* scan past any whitespace remaining in the source string */
    src = skip_space_tab(src);
    token_ended_line = isreturn(*src) || *src == EOS;

    tb_append(tok, EOS);

    return src;
}

/*
 * This is split-out from get_token() so we can handle the special case of
 * re-gluing a "!" to a quoted string from kbd_reply().
 */
char *
get_token2(char *src, TBUFF **tok, int (*endfunc) (EOL_ARGS), int eolchar, int *actual)
{
    int quotef = EOS;		/* nonzero iff the current string quoted */
    int quotes = FALSE;
    int c, i, d, chr;
    int shell = tb_length(*tok) && isShellOrPipe(tb_values(*tok));
    int first = TRUE;

    if (actual != NULL)
	*actual = EOS;
    if (src == NULL)
	return src;

    /* first scan past any whitespace in the source string */
    src = skip_space_tab(src);

    /* scan through the source string, which may be quoted */
    while ((c = *src) != EOS) {
	/* single-quotes override backslashes */
	if (quotef == SQUOTE) {
	    if (c == SQUOTE) {
		if (*++src == SQUOTE) {
		    /* double a single-quote to insert one */
		    chr = *src++;
		} else {
		    quotef = EOS;
		    continue;
		}
	    } else {
		chr = *src++;
	    }
	} else if (quotef == EOS && c == SQUOTE) {
	    quotef = SQUOTE;
	    if (first)
		quotes = TRUE;
	    chr = *src++;
	} else if (c == BACKSLASH) {	/* process special characters */
	    /*
	     * FIXME - if quotef is EOS, we should implicitly double-quote.
	     */
	    src++;
	    if (*src == EOS) {
		/* FIXME - if leading but no trailing quote, is error */
		break;
	    }
	    switch (c = *src++) {
	    case 'r':
		chr = '\r';
		break;
	    case 'n':
		chr = '\n';
		break;
	    case 't':
		chr = '\t';
		break;
	    case 'b':
		chr = '\b';
		break;
	    case 'f':
		chr = '\f';
		break;
	    case 'a':
		chr = '\a';
		break;
	    case 's':
		chr = ' ';
		break;
	    case 'e':
		chr = ESC;
		break;

	    case 'x':
	    case 'X':
		i = 2;		/* allow \xNN hex */
		c = 0;
		while (isAlnum(*src) && i--) {
		    if (isDigit(*src)) {
			d = *src - '0';
		    } else if (isLower(*src)) {
			d = *src - 'a' + 10;
		    } else {
			d = *src - 'A' + 10;
		    }
		    if (d > 15)
			break;
		    c = (c * 16) + d;
		    src++;
		}
		chr = (char) c;
		break;

	    default:
		if (c >= '0' && c <= '7') {
		    i = 2;	/* allow \NNN octal */
		    c -= '0';
		    while (isDigit(*src)
			   && *src < '8'
			   && i--) {
			c = (c * 8) + (*src++ - '0');
		    }
		}
		chr = (char) c;
	    }
	} else {
	    /* check for the end of the token */
	    if (quotef != EOS) {
		if (c == quotef) {
		    src++;
		    if (actual != NULL)
			*actual = *src;
		    break;
		}
	    } else {
#if OPT_MODELINE
		/*
		 * Multiple settings on the line may be separated by colons.
		 */
		if (c == ':' && in_modeline) {
		    if (actual != NULL)
			*actual = ' ';
		    src++;
		    break;
		} else
#endif
		if (c == eolchar) {
		    if (actual != NULL)
			*actual = *src;
		    if (!isBlank(c))
			src++;
		    break;
		} else if (endfunc(tb_values(*tok), tb_length(*tok), c, eolchar)) {
		    if (actual != NULL)
			*actual = *src;
		    break;
		} else if (c == DQUOTE) {
		    quotef = c;
		    if (first)
			quotes = TRUE;
		    /*
		     * Note that a leading quote is included in the result from
		     * this function.
		     */
		} else if (isBlank(c)) {
		    if (actual != NULL)
			*actual = *src;
		    break;
		}
	    }

	    chr = *src++;	/* record the character */
	}
	if (first && shell && chr == DQUOTE) {
	    /* eat the leading quote if we're re-gluing a shell command */
	    /* EMPTY */ ;
	} else {
	    tb_append(tok, chr);
	}
	first = FALSE;
    }

    /* scan past any whitespace remaining in the source string */
    src = skip_space_tab(src);
    token_ended_line = isreturn(*src) || *src == EOS;

    tb_append(tok, EOS);
    /*
     * Check for case where the input may have had a leading backslash to
     * introduce a quote.
     *
     * FIXME: we could have several adjacent strings such as
     *   \'xx\"yy\'
     * It may be better to process those in chunks.
     */
    if (tb_length(*tok) && !quotes)
	tb_prequote(tok);

    /* both double and single-quote strings are reduced to a single-quote
     * string with no trailing quote, to simplify use in the caller.
     */
    if (tb_length(*tok) && *tb_values(*tok) == DQUOTE)
	*tb_values(*tok) = SQUOTE;
    return src;
}

/*
 * Convert the string 'src' into a string that we can read back with 'token()'.
 * If it is a shell-command, this will be a single-token.  Repeated shift
 * commands are multiple tokens.
 */
int
macroize(TBUFF **p, TBUFF *src, int skip)
{
    int c;
    char *ref;
    char *txt;

    if ((ref = tb_values(src)) != NULL) {	/* FIXME */
	int multi = !isShellOrPipe(ref);	/* shift command? */
	int count = 0;

	if (tb_init(p, esc_c) != NULL) {
	    size_t n, len = tb_length(src);

	    if ((txt = tb_values(src)) != NULL) {

		TRACE(("macroizing %s\n", tb_visible(src)));
		(void) tb_append(p, DQUOTE);
		for (n = (size_t) skip; n < len; n++) {
		    c = txt[n];
		    if (multi && count++)
			(void) tb_sappend(p, "\" \"");
		    if (c == BACKSLASH || c == DQUOTE)
			(void) tb_append(p, BACKSLASH);
		    (void) tb_append(p, c);
		}
		(void) tb_append(p, DQUOTE);
		TRACE(("macroized %s\n", tb_visible(*p)));
		return (tb_append(p, EOS) != NULL);
	    }
	}
    }
    return FALSE;
}

/* estimate maximum number of macro tokens, for allocating arrays */
unsigned
mac_tokens(void)
{
    const char *s = execstr;
    unsigned result = 0;

    while (s != NULL && *s != EOS) {
	if (isSpace(*s)) {
	    s = skip_cblanks(s);
	} else if (*s != EOS) {
	    result++;
	    s = skip_ctext(s);
	}
    }
    return result;
}

/* fetch and isolate the next token from execstr */
int
mac_token(TBUFF **tok)
{
    int result;
    int savcle;
    const char *oldstr = execstr;

    savcle = clexec;
    clexec = TRUE;

    /* get and advance past token */
    execstr = get_token(execstr, tok, eol_null, EOS, (int *) 0);
    result = (execstr != oldstr);

    if (clexec && result) {
	if (execstr == NULL || *skip_blanks(execstr) == EOS)
	    set_end_string('\n');
    }

    clexec = savcle;
    return result;
}

/* fetch and isolate and evaluate the next token from execstr */
char *
mac_tokval(TBUFF **tok)
{
    if (mac_token(tok) != 0) {
	char *previous = tb_values(*tok);
	const char *newvalue = tokval(previous);
	int old_type = toktyp(previous);
	/* evaluate the token */
	if (previous != NULL
	    && newvalue != NULL
	    && old_type != TOK_QUOTSTR
	    && (const char *) previous != newvalue) {
	    /*
	     * Check for the special case where we're just shifting the result
	     * down by one since we're stripping a leading quote.
	     */
	    if (((const char *) previous) + 1 == newvalue) {
		TBUFF *fix = NULL;
		tb_scopy(&fix, newvalue);
		tb_free(tok);
		*tok = fix;
	    } else {
		(void) tb_scopy(tok, newvalue);
	    }
	    tb_dequote(tok);
	}
	return (tb_values(*tok));
    }
    tb_free(tok);
    return (NULL);
}

/*
 * The result of mac_tokval() could be a literal string with quotes.
 * Dequote that level.
 */
char *
mac_unquotedarg(TBUFF **tok)
{
    char *result = mac_tokval(tok);
    if (toktyp(result) == TOK_QUOTSTR) {
	tb_dequote(tok);
	result = tb_values(*tok);
    }
    return result;
}

/*
 * get a macro line argument
 */
int
mac_literalarg(TBUFF **tok)
{				/* buffer to place argument */
    /* grab everything on this line, literally */
    (void) tb_scopy(tok, execstr);
    execstr += strlen(execstr);
    token_ended_line = TRUE;
    return TRUE;
}

#if OPT_MACRO_ARGS
/*
 * Parameter info is given as a keyword, optionally followed by a prompt string.
 */
static int
decode_parameter_info(TBUFF *tok, PARAM_INFO * result)
{
    char name[NSTRING];
    char text[NSTRING];
    char *s = vl_strncpy(name, tb_values(tok), sizeof(name));
    int code;

    if ((s = strchr(s, '=')) != NULL) {
	vl_strncpy(text, tokval(s + 1), sizeof(text));
	*s = EOS;
    } else {
	text[0] = EOS;
    }
    if ((s = strchr(name, ':')) != NULL)
	*s++ = EOS;

    code = choice_to_code(&fsm_paramtypes_blist, name, strlen(name));
    /* split-up to work around gcc bug */
    result->pi_type = (PARAM_TYPES) code;
    if (result->pi_type >= 0) {
	result->pi_text = *text ? strmalloc(text) : NULL;
#if OPT_ENUM_MODES
	if (s != NULL)
	    result->pi_choice = name_to_choices(s);
	else
	    result->pi_choice = NULL;
#endif
	return TRUE;
    }
    return FALSE;
}
#endif

static int
setup_macro_buffer(TBUFF *name, int bufnum, UINT flags)
{
#if OPT_MACRO_ARGS || OPT_ONLINEHELP
    static TBUFF *temp;
    unsigned limit = mac_tokens();
    unsigned count = 0;
#endif
#if OPT_ONLINEHELP
    TBUFF *helpstring = NULL;	/* optional help string */
#endif
    char bname[NBUFN];		/* name of buffer to use */
    BUFFER *bp;

    (void) flags;

    /* construct the buffer name for the macro */
    if (bufnum < 0)
	(void) add_brackets(bname, tb_values(name));
    else
	(void) lsprintf(bname, MACRO_N_BufName, bufnum);

    /* set up the buffer for the new macro */
    if ((bp = bfind(bname, BFINVS)) == NULL) {
	mlforce("[Cannot create procedure]");
	return FALSE;
    }
    if (buffer_in_use(bp)) {
	return FALSE;
    }

    /* and make sure it is empty */
    if (!bclear(bp))
	return FALSE;

    set_rdonly(bp, bp->b_fname, MDVIEW);

    /* save this into the list of : names */
#if OPT_NAMEBST
    if (bufnum < 0) {
	CMDFUNC *cf = typecalloc(CMDFUNC);

	if (!cf)
	    return no_memory("registering procedure name");

#ifdef CC_CANNOT_INIT_UNIONS
	cf->c_union = (void *) bp;
#else
	cf->cu.c_buff = bp;
#endif
	cf->c_flags = UNDO | REDO | VIEWOK | flags;

#if OPT_MACRO_ARGS || OPT_ONLINEHELP
	if (limit != 0) {
	    while (mac_token(&temp) == TRUE) {
		switch (toktyp(tb_values(temp))) {
#if OPT_ONLINEHELP
		case TOK_QUOTSTR:
		    tb_copy(&helpstring, temp);
		    break;
#endif
#if OPT_MACRO_ARGS
		case TOK_LITSTR:
		    if (cf->c_args == NULL) {
			cf->c_args = typeallocn(PARAM_INFO, (size_t) limit + 1);
			if (cf->c_args == NULL) {
			    free(cf);
			    return no_memory("Allocating parameter info");
			}
		    }
		    if (decode_parameter_info(temp, &(cf->c_args[count]))) {
			cf->c_args[++count].pi_type = PT_UNKNOWN;
			cf->c_args[count].pi_text = NULL;
			break;
		    }
#endif
		    /* FALLTHRU */
		default:
		    free(cf);
		    mlforce("[Unexpected token '%s']", tb_values(temp));
		    return FALSE;
		}
	    }
	}
#endif

#if OPT_ONLINEHELP
	if (helpstring == NULL)
	    cf->c_help = strmalloc("User-defined procedure");
	else
	    cf->c_help = strmalloc(tb_values(helpstring));
	tb_free(&helpstring);
#endif

	if (insert_namebst(tb_values(name), cf, FALSE, 0) != TRUE)
	    return FALSE;
	tb_copy(&(bp->b_procname), name);
    }
#endif /* OPT_NAMEBST */

    set_vilemode(bp);

    /* and set the macro store pointers to it */
    macrobuffer = bp;
    return TRUE;
}

/* can't store macros interactively */
static int
can_store_macro(const char *name)
{
    int status = (clexec != 0);

    if (!status) {
	mlforce("[Cannot store %s interactively]", name);
    }
    return status;
}

/*
 * Set up a macro buffer and flag to store all executed command lines there.
 * 'n' is the macro number to use.
 */
int
store_macro(int f, int n)
{
    int status;

    if (!can_store_macro("macros")) {
	status = FALSE;
    } else if (!f) {
	/* the numeric arg is our only way of distinguishing macros */
	mlforce("[No macro specified]");
	status = FALSE;
    } else if (n < 1 || n > OPT_EXEC_MACROS) {
	/* range check the macro number */
	mlforce("[Macro number out of range]");
	status = FALSE;
    } else {
	status = setup_macro_buffer((TBUFF *) 0, n, (UINT) CMD_PROC);
    }
    return status;
}

#if OPT_PROCEDURES
/*
 * Set up a procedure buffer and flag to store all executed command lines
 * there.  'n' is the macro number to use.
 */
int
store_proc(int f, int n)
{
    static TBUFF *name;		/* procedure name */
    int status;			/* return status */

    if (f) {
	/* if a number was given, then they must mean macro N */
	status = store_macro(f, n);
    } else if (!can_store_macro("procedures")) {
	status = FALSE;
    } else {

	/* get the name of the procedure */
	tb_scopy(&name, "");
	if ((status = kbd_reply("Procedure name: ", &name,
				eol_command, ' ',
				KBD_NORMAL, no_completion)) == TRUE) {
	    status = setup_macro_buffer(name, -1, (UINT) CMD_PROC);
	}
    }
    return status;
}

int
store_op(int f, int n)
{
    static TBUFF *name;		/* procedure name */
    int status;			/* return status */

    (void) f;
    (void) n;

    if (!can_store_macro("operators")) {
	status = FALSE;
    } else {

	/* get the name of the procedure */
	tb_scopy(&name, "");
	if ((status = kbd_reply("Operator name: ", &name,
				eol_command, ' ',
				KBD_NORMAL, no_completion)) == TRUE) {
	    status = setup_macro_buffer(name, -1, (UINT) CMD_OPER);
	}
    }
    return status;
}

/*
 * Prompt for a procedure name and execute the named procedure
 */
int
execproc(int f, int n)
{
    static char name[NBUFN];

    int status;
    BUFFER *bp;			/* ptr to buffer to execute */
    char bufn[NBUFN];		/* name of buffer to execute */

    if ((status = mlreply("Execute procedure: ",
			  name, (int) sizeof(name))) != TRUE) {
	return status;
    }

    /* construct a name for the procedure buffer */
    (void) add_brackets(bufn, name);

    /* find the buffer with that name */
    if ((bp = find_b_name(bufn)) == NULL) {
	return FALSE;
    }
    return dobuf(bp, need_a_count(f, n, 1), -1);

}

#endif

#if ! SMALLER
/*
 * Execute the contents of a buffer of commands
 */
int
execbuf(int f, int n)
{
    static char bufn[NBUFN];	/* name of buffer to execute */

    BUFFER *bp;			/* ptr to buffer to execute */
    int status;			/* status return */

    /* find out what buffer the user wants to execute */
    if ((status = ask_for_bname("Execute buffer: ", bufn, sizeof(bufn))) != TRUE)
	return status;

    /* find the pointer to that buffer */
    if ((bp = find_any_buffer(bufn)) == NULL) {
	return FALSE;
    }

    return dobuf(bp, need_a_count(f, n, 1), -1);
}
#endif

/*
 * free a while block pointer list, given a pointer 'wp' to the head of the list.
 */
static void
free_all_whiles(WHLOOP * wp)
{
    WHLOOP *tp;

    while (wp) {
	tp = wp->w_next;
	free((char *) wp);
	wp = tp;
    }
}

#define DIRECTIVE_CHAR '~'

#if ! SMALLER
#define DDIR_FAILED     -1
#define DDIR_COMPLETE    0
#define DDIR_INCOMPLETE  1
#define DDIR_FORCE       2
#define DDIR_HIDDEN      3
#define DDIR_QUIET	 4

/*
 * When we get a "~local", we save the variable's name and its value.  Save the
 * value only the first time we encounter the variable.  We will restore the
 * variables when exiting from the current buffer macro.
 *
 * FIXME:  It would be nice (but more complicated) to implement local variables
 * within while/endwhile blocks.
 */
static int
push_variable(char *name)
{
    LOCALS *p;
    VALARGS args;

    TPRINTF(("...push_variable: {%s}%s\n", name, execstr));

    switch (toktyp(name)) {
    case TOK_STATEVAR:
    case TOK_TEMPVAR:
	break;
    default:
	return FALSE;
    }

    /* Check if we've saved this before */
    for (p = ifstk.locals; p != NULL; p = p->next) {
	if (!strcmp(name, p->name))
	    return TRUE;
    }

    /* We've not saved it - do it now */
    if ((p = typealloc(LOCALS)) == NULL) {
	no_memory("push_variable");
	return FALSE;
    }
    p->next = ifstk.locals;
    p->name = strmalloc(name);
    p->m_value = NULL;
    p->p_value = tokval(name);
    if (isErrorVal(p->p_value)) {
	/* special case: so we can delete vars */
	if (toktyp(name) != TOK_TEMPVAR) {
	    free(p->name);
	    free((char *) p);
	    return FALSE;
	}
    } else {
	/* FIXME: this won't handle embedded nulls */
	if (find_mode(curbp, (*name == '$') ? (name + 1) : name, -TRUE, &args)
	    && must_quote_token(p->p_value, strlen(p->p_value))) {
	    TBUFF *tmp = NULL;
	    append_quoted_token(&tmp, p->p_value, strlen(p->p_value));
	    tb_append(&tmp, EOS);
	    if (tb_values(tmp) != NULL)
		p->p_value = tb_values(tmp);
	    free(tmp);
	} else {
	    p->m_value = strmalloc(p->p_value);
	    p->p_value = NULL;
	}
    }

    ifstk.locals = p;

    return TRUE;
}

/*
 * On exit from the buffer, restore variables declared local, in reverse order.
 */
static void
pop_variable(void)
{
    LOCALS *p = ifstk.locals;
    const char *value = (p->p_value != NULL) ? p->p_value : p->m_value;

    TRACE((T_CALLED "pop_variable()\n"));

    ifstk.locals = p->next;
    TPRINTF(("...pop_variable(%s) %s\n", p->name, value));
    if (!strcmp(value, error_val)) {
	rmv_tempvar(p->name);
    } else {
	set_state_variable(p->name, value);
	if (p->m_value != NULL)
	    free(p->m_value);
    }
    free(p->name);
    free((char *) p);

    returnVoid();
}

static void
push_buffer(IFSTK * save)
{
    static const IFSTK new_ifstk =
    {0, 0, FALSE, rgn_EXACT, NULL, NULL, NULL};		/* all 0's */

    *save = ifstk;
    save->shape = regionshape;
    save->motion = havemotion;
    save->prefix = with_prefix;

    ifstk = new_ifstk;
    havemotion = NULL;
    regionshape = rgn_EXACT;
    with_prefix = NULL;
}

static void
pop_buffer(IFSTK * save)
{
    while (ifstk.locals)
	pop_variable();

    tb_free(&with_prefix);

    ifstk = *save;
    havemotion = ifstk.motion;
    regionshape = ifstk.shape;
    with_prefix = ifstk.prefix;
}

/*
 * Lookup a directory name.
 *	*cmdp points to a command
 *	length is the nominal maximum length (*cmdp is null-terminated)
 */
DIRECTIVE
dname_to_dirnum(char **cmdpp, size_t length)
{
    DIRECTIVE dirnum = D_UNKNOWN;
    size_t n;
    int code;
    const char *ptr = (*cmdpp);

    if ((length-- > 1)
	&& (*ptr++ == DIRECTIVE_CHAR)) {
	for (n = 0; n < length; n++) {
	    if (!isAlnum(ptr[n]))
		length = n;
	}
	code = choice_to_code(&fsm_directive_blist, ptr, length);
	/* split-up to work around gcc bug */
	dirnum = (DIRECTIVE) code;
	if (dirnum < 0)
	    dirnum = D_UNKNOWN;
	else
	    *cmdpp += (1 + length);
    }

    return dirnum;
}

#define directive_name(code) choice_to_name(&fsm_directive_blist, code)

static int
unbalanced_directive(DIRECTIVE dirnum)
{
    mlforce("[Unexpected directive: %s]", directive_name(dirnum));
    return DDIR_FAILED;
}

static int
navigate_while(LINE **lpp, WHLOOP * whlist)
{
    WHLOOP *wht;

    /* find the while control block we're on.  do what it says.
     *  (i.e. if we're at a break or while, we're sent to the
     *  bottom, if we're at an endwhile, we're sent to the top.)
     */
    for (wht = whlist; wht != NULL; wht = wht->w_next) {
	if (wht->w_here == *lpp) {
	    *lpp = wht->w_there;
	    return TRUE;
	}
    }
    return FALSE;
}

/*
 * Do the bookkeeping to begin a directive.
 *	*cmdp points to the first token after the directive name
 *	dirnum is the number of the directive
 *	whlish is state/data for while/loops
 */
static int
begin_directive(char **const cmdpp,
		DIRECTIVE dirnum,
		WHLOOP * whlist,
		BUFFER *bp,
		LINE **lpp)
{
    int status = DDIR_COMPLETE;	/* assume directive is self-contained */
    TBUFF *argtkn = NULL;
    char *value;
    char *old_execstr = execstr;

    execstr = *cmdpp;

    switch (dirnum) {
    case D_LOCAL:
	if (!ifstk.disabled) {
	    while (mac_token(&argtkn) == TRUE) {
		value = tb_values(argtkn);
		if (push_variable(value) != TRUE) {
		    mlforce("[cannot save '%s']", value);
		    status = DDIR_FAILED;
		    break;
		}
	    }
	}
	break;

    case D_WHILE:
	if (!ifstk.disabled) {
	    if ((value = mac_unquotedarg(&argtkn)) == NULL) {
		status = DDIR_INCOMPLETE;
		break;
	    } else if (scan_bool(value) != TRUE) {
		if (navigate_while(lpp, whlist) != TRUE)
		    status = unbalanced_directive(dirnum);
	    }
	}
	break;

    case D_BREAK:
	if (!ifstk.disabled) {
	    if (navigate_while(lpp, whlist) != TRUE)
		status = unbalanced_directive(dirnum);
	}
	break;

    case D_ENDWHILE:
	if (!ifstk.disabled) {
	    if (navigate_while(lpp, whlist) != TRUE)
		status = unbalanced_directive(dirnum);
	    else
		*lpp = lback(*lpp);
	}
	break;

    case D_IF:
	ifstk.level++;
	if (!ifstk.disabled) {
	    ifstk.fired = FALSE;
	    ifstk.disabled = ifstk.level;
	    if ((value = mac_unquotedarg(&argtkn)) == NULL)
		status = DDIR_INCOMPLETE;
	    else if (scan_bool(value) == TRUE) {
		ifstk.disabled = 0;
		ifstk.fired = TRUE;
	    }
	}
	break;

    case D_ELSEIF:
	if (ifstk.level == 0) {
	    status = unbalanced_directive(dirnum);
	} else {
	    if (ifstk.fired) {
		if (!ifstk.disabled)
		    ifstk.disabled = ifstk.level;
	    } else if ((value = mac_unquotedarg(&argtkn)) == NULL) {
		status = DDIR_INCOMPLETE;
	    } else if (!ifstk.fired
		       && ifstk.disabled == ifstk.level
		       && (scan_bool(value) == TRUE)) {
		ifstk.disabled = 0;
		ifstk.fired = TRUE;
	    }
	}
	break;

    case D_ELSE:
	if (ifstk.level == 0) {
	    status = unbalanced_directive(dirnum);
	} else {
	    if (ifstk.fired) {
		if (!ifstk.disabled)
		    ifstk.disabled = ifstk.level;
	    } else if (ifstk.disabled == ifstk.level) {
		ifstk.disabled = 0;
		ifstk.fired = TRUE;
	    }
	}
	break;

    case D_ENDIF:
	if (ifstk.level == 0) {
	    status = unbalanced_directive(dirnum);
	} else {
	    ifstk.level--;
	    if (ifstk.disabled > ifstk.level) {
		ifstk.disabled = 0;
		ifstk.fired = TRUE;
	    }
	}
	break;

    case D_GOTO:
	if (!ifstk.disabled) {
	    TBUFF *label = NULL;
	    LINE *glp;		/* line to goto */

	    /* grab label to jump to */
	    *cmdpp = (char *) get_token(*cmdpp,
					&label,
					eol_null,
					EOS,
					(int *) 0);
#if VILE_MAYBE
	    /* i think a simple re-eval would get us variant
	     * targets, i.e. ~goto %somelabel.  */
	    tb_scopy(label, tokval(tb_values(label)));
#endif
	    value = tb_values(label);
	    glp = label2lp(bp, value);
	    if (glp == NULL) {
		mlforce("[No such label \"%s\"]", value);
		status = DDIR_FAILED;
	    } else {
		*lpp = glp;
	    }
	    tb_free(&label);
	}
	break;

    case D_RETURN:
	if (!ifstk.disabled)
	    status = DDIR_INCOMPLETE;
	break;

    case D_FORCE:
	status = DDIR_FORCE;
	break;

    case D_HIDDEN:
	status = DDIR_HIDDEN;
	break;

    case D_QUIET:
	status = DDIR_QUIET;
	break;

    case D_ELSEWITH:
	if (!ifstk.disabled) {
	    if (!tb_length(with_prefix)) {
		status = unbalanced_directive(dirnum);
		break;
	    }
	}
	/* FALLTHRU */
    case D_WITH:
	if (!ifstk.disabled) {
	    execstr = skip_space_tab(execstr);
	    tb_scopy(&with_prefix, execstr);
	    status = DDIR_COMPLETE;
	}
	break;

    case D_ENDWITH:
	if (!ifstk.disabled) {
	    tb_free(&with_prefix);
	    status = DDIR_COMPLETE;
	}
	break;

    case D_UNKNOWN:
    case D_ENDM:
	break;

    case D_TRACE:
	if (!ifstk.disabled) {
	    if ((value = mac_unquotedarg(&argtkn)) == NULL) {
		mlforce("[Trace is %s]", tracemacros ? "on" : "off");
	    } else {
		tracemacros = scan_bool(value);
	    }
	}
	break;
    }
    execstr = old_execstr;
    tb_free(&argtkn);
    return status;
}

static WHLOOP *
alloc_WHLOOP(WHLOOP * whp, DIRECTIVE dirnum, LINE *lp)
{
    WHLOOP *nwhp;

    if ((nwhp = typealloc(WHLOOP)) == NULL) {
	mlforce("[Out of memory during '%s' scan]",
		directive_name(dirnum));
	free_all_whiles(whp);
	return NULL;
    }
    nwhp->w_here = lp;
    nwhp->w_there = NULL;
    nwhp->w_type = dirnum;
    nwhp->w_next = whp;
    return nwhp;
}

static int
setup_dobuf(BUFFER *bp, WHLOOP ** result)
{
    int status = TRUE;
    LINE *lp;
    char *cmdp;
    WHLOOP *twhp, *whp = NULL;
    DIRECTIVE dirnum;

    /* create the WHILE blocks */
    bp->b_dot.o = 0;
    *result = NULL;
    for_each_line(lp, bp) {
	int i;

	bp->b_dot.l = lp;

	/* find the first printable character */
	cmdp = lvalue(lp);
	i = llength(lp);
	while (i > 0 && isBlank(*cmdp)) {
	    cmdp++;
	    i--;
	}

	/* blank line */
	if (i <= 0)
	    continue;

	dirnum = dname_to_dirnum(&cmdp, (size_t) i);
	if (dirnum == D_WHILE ||
	    dirnum == D_BREAK ||
	    dirnum == D_ENDWHILE) {

	    if ((whp = alloc_WHLOOP(whp, dirnum, lp)) == NULL) {
		status = FALSE;
		break;
	    }

	    twhp = NULL;
	    if (dirnum == D_ENDWHILE) {
		/* walk back, filling in unfilled
		 * 'there' pointers on the breaks,
		 * until we come to a while with an
		 * unfilled 'there'.  fill it in, and
		 * use its 'here' to fill in the
		 * 'there' pointer on the endwhile.
		 */
		twhp = whp->w_next;
		while (twhp) {
		    if (!twhp->w_there) {
			twhp->w_there = whp->w_here;
			if (twhp->w_type == D_WHILE) {
			    whp->w_there = twhp->w_here;
			    break;
			}
		    }
		    twhp = twhp->w_next;
		}
	    } else if (dirnum == D_BREAK) {
		/*
		 * Do the same search for break (looking for while), but do not
		 * modify the pointers.
		 */
		twhp = whp->w_next;
		while (twhp) {
		    if (!twhp->w_there) {
			if (twhp->w_type == D_WHILE) {
			    break;
			}
		    }
		    twhp = twhp->w_next;
		}
	    }
	    if (dirnum != D_WHILE) {
		if (!twhp) {
		    /* no while for an endwhile */
		    mlforce("[%s doesn't follow %s in '%s']",
			    directive_name(dirnum),
			    directive_name(D_WHILE),
			    bp->b_bname);
		    status = FALSE;
		    break;
		}
	    }
	}
    }

    /* no endwhile for a while or break */
    if (status == TRUE && whp && whp->w_type != D_ENDWHILE) {
	mlforce("[%s with no matching %s in '%s']",
		directive_name(whp->w_type),
		directive_name(D_ENDWHILE),
		bp->b_bname);
	status = FALSE;
    }

    *result = whp;
    return status;
}
#else
#define dname_to_dirnum(cmdpp,length) \
		((*cmdpp)[0] == DIRECTIVE_CHAR && !strcmp((*cmdpp)+1, "endm") \
		? D_ENDM \
		: D_UNKNOWN)
#define push_buffer(save)	/* nothing */
#define pop_buffer(save)	/* nothing */
#endif

#if OPT_TRACE && !SMALLER
static const char *
TraceIndent(int level, char *cmdp, size_t length)
{
    static const char indent[] = ".  .  .  .  .  .  .  .  ";
    switch (dname_to_dirnum(&cmdp, length)) {
    case D_ELSE:		/* FALLTHRU */
    case D_ELSEIF:		/* FALLTHRU */
    case D_ENDIF:
	if (level > 0)
	    level--;
	break;
    default:
	break;
    }
    level = (int) strlen(indent) - (3 * level);
    if (level < 0)
	level = 0;
    return &indent[level];
}
#define TRACE_INDENT(level, cmdp) TraceIndent(level, cmdp, linlen)
#else
#define TRACE_INDENT(level, cmdp) ""	/* nothing */
#endif

static int
line_in_buffer(BUFFER *bp, LINE *test_lp)
{
    LINE *lp;
    for_each_line(lp, bp) {
	if (lp == test_lp)
	    return TRUE;
    }
    return FALSE;
}

#if ! SMALLER
static void
compute_indent(char *cmd, size_t length, int *indent, int *indstate)
{
    static BUFFER *save;

    /* reset caller's indent-state if we switched to a new macro */
    if (macrobuffer == NULL
	|| macrobuffer != save) {
	save = macrobuffer;
	if (indent != NULL) {
	    *indent = 0;
	    *indstate = 0;
	}
    }
    if (cmd != NULL
	&& length != 0) {
	DIRECTIVE dirnum;
	char *base = cmd;
	int here = *indstate;
	int next = 0;

	cmd = skip_blanks(cmd);
	dirnum = dname_to_dirnum(&cmd, (length - (size_t) (cmd - base)));
	switch (dirnum) {
	case D_IF:		/* FALLTHRU */
	case D_WHILE:		/* FALLTHRU */
	case D_WITH:
	    next = 1;
	    break;
	case D_ELSE:		/* FALLTHRU */
	case D_ELSEIF:		/* FALLTHRU */
	case D_ELSEWITH:
	    --here;
	    next = 1;
	    break;
	case D_ENDIF:		/* FALLTHRU */
	case D_ENDM:		/* FALLTHRU */
	case D_ENDWHILE:	/* FALLTHRU */
	case D_ENDWITH:
	    --here;
	    break;
	default:
	    break;
	}
	*indstate = next;
	if (indent != NULL) {
	    if ((*indent += here) < 0)
		*indent = 0;
	}
    }
}

static char *
add_indent(char *dst, int indent)
{
    while (indent-- > 0) {
	*dst++ = '\t';
    }
    return dst;
}
#endif

static void
handle_endm(void)
{
    if (macrobuffer != NULL) {
	macrobuffer->b_dot.l = lforw(buf_head(macrobuffer));
	macrobuffer->b_dot.o = 0;
	macrobuffer = NULL;
#if !SMALLER
	compute_indent((char *) 0, (size_t) 0, (int *) 0, (int *) 0);
#endif
    }
}

static int
perform_dobuf(BUFFER *bp, WHLOOP * whlist)
{
    int status = TRUE;
    int glue = 0;		/* nonzero to append lines */
    LINE *lp;
    size_t linlen;
    DIRECTIVE dirnum;
    WINDOW *wp;
    char *linebuf = NULL;	/* buffer holding copy of line executing */
    char *cmdp;			/* text to execute */
    int save_clhide = clhide;
    int save_no_errs = no_errs;
    int save_quiet = quiet;
    int indent = 0;
#if !SMALLER
    int indstate = 0;
#endif

    static BUFFER *dobuferrbp = NULL;

    (void) whlist;

    TRACE((T_CALLED "perform_dobuf(bp=%p, whlist=%p) buffer '%s'\n",
	   (void *) bp, (void *) whlist, bp->b_bname));

    bp->b_inuse++;

    /* starting at the beginning of the buffer */
    for_each_line(lp, bp) {
	bp->b_dot.l = lp;

	/* calculate the line length and make a local copy */

	if (llength(lp) <= 0)
	    linlen = 0;
	else
	    linlen = (size_t) llength(lp);

	if (glue) {
	    safe_castrealloc(char, linebuf, (size_t) glue + linlen + 1);
	    if (linebuf == NULL) {
		status = no_memory("during macro execution");
		break;
	    }
	    cmdp = linebuf + glue;
	} else {
	    FreeIfNeeded(linebuf);

#if !SMALLER
	    compute_indent(lvalue(lp), linlen, &indent, &indstate);
#endif
	    linebuf = cmdp = castalloc(char, (size_t) indent + linlen + 1);
	    if (linebuf == NULL) {
		status = no_memory("during macro execution");
		break;
	    }
	}

	if (linlen == 0) {
	    cmdp[0] = EOS;	/* make sure it ends */
	} else {
	    char *src;
	    char *dst;
#if !SMALLER
	    if (!glue) {
		cmdp = add_indent(cmdp, indent);
	    }
#endif
	    (void) memcpy(cmdp, lvalue(lp), linlen);
	    cmdp[linlen] = EOS;	/* make sure it ends */

	    src = skip_space_tab(cmdp);
	    dst = cmdp;
	    /* compress out extra leading whitespace */
	    while ((*dst++ = *src++) != EOS) {
		;
	    }
	    linlen -= (size_t) (src - dst);
	}
	glue = 0;

	TPRINTF(("%s:%d (%d/%d):%s\n", bp->b_bname,
		 line_no(bp, lp),
		 ifstk.level, ifstk.disabled,
		 cmdp));

	/*
	 * If the last character on the line is a backslash, glue the
	 * following line to the one we're processing.
	 */
	if (lforw(lp) != buf_head(bp)
	    && linlen != 0
	    && isEscaped(cmdp + linlen)) {
	    glue = (int) (linlen + (size_t) (cmdp - linebuf) - 1);
	    continue;
	}
	cmdp = skip_space_tab(linebuf);

	/* Skip comments and blank lines.
	 * ';' for uemacs backward compatibility, and
	 * '"' for vi compatibility
	 */
	if (*cmdp == ';'
	    || *cmdp == '"'
	    || *cmdp == EOS) {
	    continue;
	}
#if OPT_DEBUGMACROS
	/* echo lines and get user confirmation when debugging */
	if (tracemacros) {
	    mlforce("<<<%s:%d/%d:%s>>>", bp->b_bname,
		    ifstk.level, ifstk.disabled,
		    str_visible(cmdp));
	    (void) update(TRUE);

	    /* and get the keystroke */
	    if (ABORTED(keystroke())) {
		mlforce("[Macro aborted]");
		status = FALSE;
		break;
	    }
	}
#endif
#if !SMALLER
	TRACE(("<<<%s%s:%d/%d%c%s%s>>>\n",
	       (bp == curbp) ? "*" : "",
	       bp->b_bname, ifstk.level, ifstk.disabled,
	       ifstk.fired ? '+' : ' ',
	       TRACE_INDENT(ifstk.level, cmdp),
	       str_visible(cmdp)));
#endif

	if (*cmdp == DIRECTIVE_CHAR) {

	    dirnum = dname_to_dirnum(&cmdp, linlen);
	    if (dirnum == D_UNKNOWN) {
		mlforce("[Unknown directive \"%s\"]", cmdp);
		status = FALSE;
		break;
	    }

	    /* service the ~endm directive here */
	    if (dirnum == D_ENDM && !ifstk.disabled) {
		if (macrobuffer == NULL) {
		    mlforce("[No macro definition in progress]");
		    status = FALSE;
		    break;
		}
		handle_endm();
		continue;
	    }
	} else {
	    dirnum = D_UNKNOWN;
	}

	/* if macro store is on, just salt this away */
	if (macrobuffer != NULL) {
	    /* allocate the space for the line */
	    if (addline(macrobuffer, linebuf, -1) == FALSE) {
		mlforce("[Out of memory while storing macro]");
		status = FALSE;
		break;
	    }
	    continue;
	}

	/* goto labels are no-ops */
	if (*cmdp == '*') {
	    continue;
	}

	no_errs = save_no_errs;

#if ! SMALLER
	/* deal with directives */
	if (dirnum != D_UNKNOWN) {
	    int code;

	  next_directive:
	    /* move past directive */
	    cmdp = skip_space_tab(cmdp);

	    code = begin_directive(&cmdp, dirnum, whlist, bp, &lp);
	    if (code == DDIR_FAILED) {
		status = FALSE;
		break;
	    } else if (code == DDIR_COMPLETE) {
		continue;
	    } else if (code == DDIR_INCOMPLETE) {
		status = TRUE;	/* not exactly an error */
		break;
	    } else if (code == DDIR_FORCE) {
		TRACE(("~force\n"));
		no_errs = TRUE;
	    } else if (code == DDIR_HIDDEN) {
		TRACE(("~hidden\n"));
		clhide = TRUE;
	    } else if (code == DDIR_QUIET) {
		TRACE(("~quiet\n"));
		quiet = TRUE;
	    }

	    /* check if next word is also a directive */
	    cmdp = skip_space_tab(cmdp);
	    if (*cmdp == DIRECTIVE_CHAR) {
		dirnum = dname_to_dirnum(&cmdp, linlen);
		if (dirnum == D_UNKNOWN) {
		    mlforce("[Unknown directive \"%s\"]", cmdp);
		    status = FALSE;
		    break;
		}
		goto next_directive;
	    }
	} else if (*cmdp != DIRECTIVE_CHAR) {
	    /* prefix lines with "WITH" value, if any */
	    if (tb_length(with_prefix)) {
		char *temp = typeallocn(char, tb_length(with_prefix)
					+ strlen(cmdp) + 2);
		if (temp == NULL) {
		    status = no_memory("performing ~with");
		    break;
		}
		(void) lsprintf(temp, "%s %s", tb_values(with_prefix), cmdp);
		free(linebuf);
		cmdp = linebuf = temp;
	    }
	}
#endif

	/* if we are only scanning, come back here */
	if (ifstk.disabled)
	    status = TRUE;
	else
	    status = docmd(cmdp, TRUE, FALSE, 1);

	if (no_errs)
	    status = TRUE;

	clhide = save_clhide;
	no_errs = save_no_errs;
	quiet = save_quiet;

	if (status == TRUE) {
	    dobuferrbp = NULL;
	} else if (line_in_buffer(bp, lp)) {
	    /* update window if visible */
	    for_each_visible_window(wp) {
		if (wp->w_bufp == bp) {
		    wp->w_dot.l = lp;
		    wp->w_dot.o = 0;
		    wp->w_flag |= WFHARD;
		}
	    }
	    /* set the buffer's dot */
	    bp->b_dot.l = lp;
	    bp->b_dot.o = 0;
	    bp->b_wline.l = lforw(buf_head(bp));
	    if (dobuferrbp == NULL) {
		dobuferrbp = bp;
		(void) swbuffer(bp);
		kbd_alarm();
	    }
	    break;
	} else {
	    break;
	}
    }

    FreeIfNeeded(linebuf);

    if (--(bp->b_inuse) < 0)
	bp->b_inuse = 0;
    returnCode(status);
}

/*
 * execute the contents of a buffer
 */
int
dobuf(BUFFER *bp, int limit, int real_cmd_count)
{
    TBUFF *macro_result = NULL;
    int status = FALSE;
    WHLOOP *whlist;
    int save_vl_msgs;
    int save_cmd_count;
    int counter;

    static int dobufnesting;	/* flag to prevent runaway recursion */

    if (bp != NULL) {
	TRACE((T_CALLED "dobuf(%s, %d)\n", bp->b_bname, limit));
	beginDisplay();
	if (++dobufnesting < 9) {

	    save_vl_msgs = vl_msgs;
	    save_cmd_count = cmd_count;

	    /* macro arguments are readonly, so we do this once */
	    if ((status = save_arguments(bp)) != ABORT) {
		vl_msgs = FALSE;

		for (counter = 1; counter <= limit; counter++) {
		    if (dobufnesting == 1)
			cmd_count = ((real_cmd_count >= 0)
				     ? real_cmd_count
				     : counter);

#if ! SMALLER
		    if (setup_dobuf(bp, &whlist) != TRUE) {
			status = FALSE;
		    } else
#else
		    whlist = NULL;
#endif
		    {
			IFSTK save_ifstk;
			push_buffer(&save_ifstk);
			status = perform_dobuf(bp, whlist);
			pop_buffer(&save_ifstk);
		    }

		    handle_endm();
		    free_all_whiles(whlist);

		    if (status != TRUE)
			break;
		}

		/*
		 * If the caller set $return, use that value.
		 */
#if OPT_EVAL
		if (this_macro_result != NULL)
		    tb_copy(&macro_result, this_macro_result);
#endif

		restore_arguments(bp);
		vl_msgs = save_vl_msgs;
		cmd_count = save_cmd_count;
	    } else {
	    }
	} else {
	    mlwarn("[Too many levels of files]");
	    tb_error(&macro_result);
	}
	dobufnesting--;

	/*
	 * Set $_ from our TBUFF (preferred), or a readable form of the status
	 * codes.  In the latter case, this is not the same as $status, since
	 * we try to show the ABORT and SORTOFTRUE cases as well.
	 */
#if OPT_EVAL
	tb_free(&last_macro_result);
	if (macro_result != NULL) {
	    last_macro_result = macro_result;
	} else {
	    switch (status) {
	    case FALSE:
		/* FALLTHRU */
	    case TRUE:
		/* FALLTHRU */
	    case ABORT:
		/* FALLTHRU */
	    case SORTOFTRUE:
		tb_scopy(&last_macro_result, status2s(status));
		break;
	    default:
		tb_error(&last_macro_result);
		break;
	    }
	}
#endif /* OPT_EVAL */

	endofDisplay();
    }

    returnCode(status);
}

/*
 * Dummy function used to tell vile_op() that we are passing a user-defined
 * operator's name in the last parameter.
 */
int
user_operator(void)
{
    return TRUE;
}

/*
 * Common function for startup-file, and for :so command.
 */
int
do_source(char *fname, int n, int optional)
{
    int status = TRUE;		/* return status of name query */
    char *fspec;		/* full file spec */

    TRACE((T_CALLED "do_source(%s, %d, %d)\n", TRACE_NULL(fname), n, optional));

    /* look for the file in the configuration paths */
    fspec = cfg_locate(fname, LOCATE_SOURCE);

    /* nonexistent */
    if (fspec == NULL) {
	if (!optional)
	    status = no_such_file(fname);
    } else {

	/* do it */
	while (n-- > 0)
	    if ((status = dofile(fspec)) != TRUE)
		break;
    }

    returnCode(status);
}

/*
 * execute a series of commands in a file
 */
/* ARGSUSED */
int
execfile(int f GCC_UNUSED, int n)
{
    int status;
    char fname[NFILEN];		/* name of file to execute */
    static TBUFF *last;

    if ((status = mlreply_file("File to execute: ", &last, FILEC_READ,
			       fname)) != TRUE)
	return status;

    return do_source(fname, n, FALSE);
}

static L_NUM
get_b_lineno(BUFFER *bp)
{
    L_NUM result;
    WINDOW *wp;

    /* try to save the original location, so we can restore it */
    if (curwp->w_bufp == bp)
	result = line_no(bp, curwp->w_dot.l);
    else if ((wp = bp2any_wp(bp)) != NULL)
	result = line_no(bp, wp->w_dot.l);
    else
	result = line_no(bp, bp->b_dot.l);
    return result;
}

static void
set_b_lineno(BUFFER *bp, L_NUM n)
{
    WINDOW *wp;
    LINE *lp;

    for_each_line(lp, bp) {
	if (--n <= 0) {
	    bp->b_dot.l = lp;
	    bp->b_dot.o = 0;
	    for_each_visible_window(wp) {
		if (wp->w_bufp == bp) {
		    wp->w_dot = bp->b_dot;
		    wp->w_flag |= WFMOVE;
		}
	    }
	    break;
	}
    }
}

/*
 * Yank a file named 'fname' into a buffer and execute it.
 * If there are no errors, delete the buffer on exit.
 */
int
dofile(char *fname)
{
    BUFFER *bp;			/* buffer to place file to execute */
    int status;			/* results of various calls */
    int clobber = FALSE;
    int original;

    TRACE((T_CALLED "dofile(%s)\n", TRACE_NULL(fname)));

    if (isEmpty(fname))
	returnCode(no_file_found());

    /*
     * Check first for the name, assuming it's a filename.  If we don't
     * find an existing buffer with that filename, create a buffer.
     */
    if ((bp = find_b_file(fname)) == NULL) {
	if ((bp = make_bp(fname, 0)) == NULL)
	    returnCode(FALSE);
	clobber = TRUE;
    }

    bp->b_flag |= BFEXEC;

    /* try to save the original location, so we can restore it */
    original = get_b_lineno(bp);

    /* and try to read in the file to execute */
    if ((status = readin(fname, FALSE, bp, TRUE)) == TRUE) {

	/* go execute it! */
	bp->b_refcount += 1;
	status = dobuf(bp, 1, -1);
	bp->b_refcount -= 1;

	/*
	 * If no errors occurred, and if the buffer isn't displayed,
	 * remove it, unless it was loaded before we entered this
	 * function.  In that case, (try to) jump back to the original
	 * location.
	 */
	if (status != TRUE)
	    (void) swbuffer(bp);
	else if ((bp->b_nwnd == 0) && (bp->b_refcount == 0) && clobber)
	    (void) zotbuf(bp);
	else
	    set_b_lineno(bp, original);
    } else if (clobber) {
	(void) zotbuf(bp);
    }
    returnCode(status);
}

/*
 * Execute the contents of a numbered buffer 'bufnum'
 */
static int
cbuf(int f, int n, int bufnum)
{
    BUFFER *bp;			/* ptr to buffer to execute */
    char bufname[NBUFN];

    /* make the buffer name */
    (void) lsprintf(bufname, MACRO_N_BufName, bufnum);

    /* find the pointer to that buffer */
    if ((bp = find_b_name(bufname)) == NULL) {
	mlforce("[Macro %d not defined]", bufnum);
	return FALSE;
    }

    return dobuf(bp, need_a_count(f, n, 1), -1);
}

#include "neexec.h"
