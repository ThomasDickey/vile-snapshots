/*
 * find the next error in mentioned in the shell output window.
 * written for vile by paul fox.
 * rewritten to use regular expressions by T.Dickey (dickey@clark.net)
 *
 * Copyright (c) 1990-1999 by Paul Fox and Thomas Dickey
 *
 * $Header: /users/source/archives/vile.vcs/RCS/finderr.c,v 1.85 2000/01/07 01:34:42 tom Exp $
 *
 */

#include "estruct.h"
#include "edef.h"

#if OPT_FINDERR

#define W_VERB  0
#define W_FILE  1
#define W_LINE  2
#define W_COLM  3
#define W_TEXT  4
#define W_LAST	5

typedef struct {
    regexp *exp_comp;
    int words[W_LAST];
} ERR_PATTERN;

static LINE *getdot(BUFFER *bp);
static void putdotback(BUFFER *bp, LINE *dotp);

static char febuff[NBUFN];	/* name of buffer to find errors in */
static int newfebuff = TRUE;	/* is the name new since last time? */

static TBUFF *fe_verb;
static TBUFF *fe_file;
static TBUFF *fe_text;
static int fe_colm;
static int fe_line;

/*
 * This is the list of predefined regular expressions for the error
 * finder.  The user can substitute a new list at runtime by loading
 * the buffer [Error Expressions].  Basically, they're normal regular
 * expressions, with embedded stuff that the error finder can parse to
 * find the verb, file, line and text fields that the regular
 * expression may contain.  These fields may be in any order, and all
 * except the file are optional.
 *
 *      %V - verb, for tracking gmake-style Entering/Leaving messages
 *      %F - range of characters to match filename.
 *      %B - range of characters to match scratch-buffer.
 *      %L - line number (this has to be an integer)
 *      %T - text to display in the message line. If no field is given,
 *              the error finder will display the entire line from
 *              the error-buffer.
 *
 * The %V, %F, %T fields may be given in alternate form, using ranges.
 * The default field is a blank-delimited token, which is enough for
 * %V, marginal for %F and useless for %T.  Vile takes each %-marked
 * field and replaces it by a subexpression, to use the subexpression
 * number to obtain the actual field mapping.
 *
 * FIXME:  some lint programs put the file, line-number and text on
 * separate lines.  Maybe we should add another control code that
 * specifies sequences of regular expressions.
 *
 * FIXME:  it might be useful to autoconf for the existing lint
 * program, to select the bsd/sys5 lint regular expressions.
 */
static const
char *const predefined[] =
{
    "^\"%[^\" \t]\", line %L:%T",	/* various C compilers */
    "^%[^: \t]:\\s*%L:\\s*%T",	/* "grep -n" */
#if OPT_MSDOS_PATH
    "^%F:\\s*%L:\\s*%T",	/* "grep -n", handles */
					/* dos drive letter   */
#endif
#if SYS_VMS
    "[ \t]*At line number %L in %[^;].*",	/* crude support for DEC C
						 * compilers. Unfortunately,
						 * these compilers place error
						 * text on previous _lines_.
						 */
#endif
#if SYS_APOLLO
    " Line %L of \"%[^\" \t]\"",	/* C compiler */
#endif
#if SYS_SUNOS && SYSTEM_HAS_LINT_PROG
    "%[^:( \t](%L):%T",		/* bsd lint) */
    "  ::  %[^( \t](%L)",	/* bsd lint) */
    "used[ \t]*([ \t]%[^(](%L)[ \t]*)",		/* bsd lint) */
#endif
    /* ultrix, sgi, osf1 (alpha only?)  use:                            */
    /*      compiler-name: Error: filename, line line-number ...        */
    "[^ ]\\+ [^ ]\\+ \"%[^, \t\"]\", line %L",
    "[^ ]\\+ [^ ]\\+ %[^, \t], line %L",
    "[^ ]\\+ \"%[^\"]\", line %L",	/* HP/UX C compiler */
    "File = %F, Line = %L",	/* SGI MIPSpro 7.3 compilers    */
#if defined(_AIX)
    "^\"%[^\" \t]\", line %L\\.[0-9]\\+:%T",	/* AIX C compilers */
#endif
#if defined(clipper) || defined(__clipper__)
    "^\"%[^\" \t]\", line %L (col. [0-9]\\+):%T",	/* CLIX C compiler */
#endif
    "^%[^(](%L)[ \t]\\+:%T",

#if SYS_UNIX && SYSTEM_HAS_LINT_PROG
    /* sys5 lint */
    "^    [^ \t]\\+[ \t]\\+%[^(](%L)$",
    "^    [^(].*( arg %L ) \t%[^( \t](%L) :: [^(]\\+(%L))",
    "^    .* :: %[^(](%L)",
#endif
#if CC_CSETPP
    "^%[^(](%L:%C) : %T",
#endif
#if CC_TURBO
    "^Error %[^ ] %L:",
    "^Warning %[^ ] %L:",
#endif
#if CC_WATCOM
    "^%[^(](%L): %T",
#endif
    "^%B:%L:%T",		/* "pp" in scratch buf */
    "^[^:]\\+: %V directory `%[^']'",	/* GNU make */
    "%T at %F line %L.*",	/* perl 5 */
    "%F\\[%L\\]:%T",		/* hgrep */
    "^\"%[^\"]\", line %L, col %C, %T",		/* ncurses, atac */
    "^\"%[^\"]\", line %L, %T",	/* ncurses */
};

static ERR_PATTERN *exp_table = 0;
static ALLOC_T exp_count = 0;

void
set_febuff(const char *name)
{
    (void) strncpy0(febuff, name, NBUFN);
    newfebuff = TRUE;
}

const char *
get_febuff(void)
{
    return febuff;
}

/*
 * Convert a given error-pattern to regular expression
 */

#define	APP_S(S) if (pass == 1) want += sizeof(S); \
		 else dst = lsprintf(dst, "%s", S)
#define APP_C    if (pass != 1) *dst++ = *src

static void
convert_pattern(ERR_PATTERN * errp, LINE *lp)
{
    static const char before[] = "\\(";
    static const char after[] = "\\+\\)";
    static const char number[] = "\\([0-9]\\+\\)";
    static const char normal[] = "\\([^ \t]\\+\\)";
    static const char remain[] = "\\(.\\+\\)";

    char *temp = 0, *src, *dst = 0;
    regexp *exp = 0;
    int pass;
    int word;
    int mark;
    int range;
    size_t want = llength(lp);
    char *first = lp->l_text;
    char *last = first + want;

    (void) memset(errp, 0, sizeof(*errp));

    /* In the first pass, find the number of fields we'll substitute.
     * Then allocate a new string that's a genuine regular expression
     */
    for (pass = 1; pass <= 2; pass++) {
	for (src = first, word = 0, range = FALSE; src < last; src++) {
	    if (*src == BACKSLASH) {
		APP_C;
		if (++src == last)
		    break;
		APP_C;
	    } else if (*src == '%') {
		mark = -1;
		switch (*++src) {
		case 'V':
		    mark = W_VERB;
		    break;
		case 'F':
		    mark = W_FILE;
		    break;
		case 'B':
		    APP_S("\\(\\[[^:]\\+]\\)");
		    errp->words[W_FILE] = ++word;
		    break;
		case 'T':
		    APP_S(remain);
		    errp->words[W_TEXT] = ++word;
		    break;
		case 'C':
		    APP_S(number);
		    errp->words[W_COLM] = ++word;
		    break;
		case 'L':
		    APP_S(number);
		    errp->words[W_LINE] = ++word;
		    break;
		case LBRACK:
		    range = TRUE;
		    APP_S(before);
		    APP_C;
		    if (src[1] == '^') {
			src++;
			APP_C;
		    }
		    if (src[1] == RBRACK) {
			src++;
			APP_C;
		    }
		    break;
		default:
		    src--;
		    break;
		}
		if (mark >= 0) {
		    APP_S(normal);
		    errp->words[mark] = ++word;
		}
	    } else if ((*src == RBRACK) && range) {
		APP_C;
		APP_S(after);
		range = FALSE;
		if (src + 1 < last) {
		    switch (*++src) {
		    case 'V':
			mark = W_VERB;
			break;
		    default:
			src--;
			/* FALLTHRU */
		    case 'F':
			mark = W_FILE;
			break;
		    case 'T':
			mark = W_TEXT;
			break;
		    }
		} else {
		    mark = W_FILE;
		}
		errp->words[mark] = ++word;
	    } else {
		APP_C;
	    }
	}
	if (pass == 1) {
	    dst = temp = typeallocn(char, want);
	    if (dst == 0)
		break;
	} else
	    *dst = EOS;
    }
    if (temp != 0) {
	exp = regcomp(temp, TRUE);
	free(temp);
    }
    errp->exp_comp = exp;
}

/*
 * Free the storage currently used in this module
 */
static void
free_patterns(void)
{
    if (exp_table != 0) {
	while (exp_count != 0)
	    free((char *) (exp_table[--exp_count].exp_comp));
	free((char *) exp_table);
    }
}

#if OPT_UPBUFF
/* ARGSUSED */
static int
update_patterns(BUFFER *bp GCC_UNUSED)
{
    free_patterns();
    return TRUE;
}
#endif

/*
 * Initialize this module.  If the expressions buffer doesn't exist, load it
 * from the internal table. If our cached regexp list doesn't match, recompute
 * that as well.
 */
static int
load_patterns(void)
{
    BUFFER *bp;
    LINE *lp;
    SIZE_T n;

    /* find the error-expressions buffer */
    if ((bp = find_b_name(ERRORS_BufName)) == 0) {
	if ((bp = bfind(ERRORS_BufName, BFINVS)) == NULL)
	    return FALSE;

	for (n = 0; n < TABLESIZE(predefined); n++)
	    addline(bp, predefined[n], -1);
	set_rdonly(bp, bp->b_fname, MDVIEW);
	free_patterns();
    } else if (b_is_changed(bp) || ((L_NUM) exp_count != bp->b_linecount)) {
	free_patterns();
    }
    bsizes(bp);
    if (bp->b_linecount == 0)
	return FALSE;

    /* any change makes the patterns obsolete */
#if OPT_UPBUFF
    update_scratch(ERRORS_BufName, update_patterns);
    bp->b_rmbuff = update_patterns;
#endif

    if (exp_count == 0) {
	exp_count = bp->b_linecount;
	exp_table = typeallocn(ERR_PATTERN, exp_count);
	for (n = 0; n < W_LAST; n++)
	    exp_table->words[n] = -1;

	n = 0;
	for_each_line(lp, bp)
	    convert_pattern(&exp_table[n++], lp);
    }
    return TRUE;
}

/*
 * Initialize this module by converting the error-patterns to regular
 * expressions.  Return the count'th item in the error-patterns list, or null
 * if count is out of range.
 */
static ERR_PATTERN *
next_pattern(ALLOC_T count)
{
    ERR_PATTERN *result = 0;

    if (count < exp_count)
	result = &exp_table[count];
    return (result);
}

/*
 * Decode the matched ERR_PATTERN
 */
static void
decode_exp(ERR_PATTERN * exp)
{
    regexp *p = exp->exp_comp;
    int n;
    TBUFF *temp;

    tb_free(&fe_verb);
    tb_free(&fe_file);
    tb_free(&fe_text);
    fe_colm = 1;
    fe_line = 0;

    n = 0;
    for (n = 1; (n < NSUBEXP) && p->startp[n] && p->endp[n]; n++) {
	temp = 0;
	if (tb_bappend(&temp,
		p->startp[n],
		(ALLOC_T) (p->endp[n] - p->startp[n])) == 0
	    || tb_append(&temp, EOS) == 0)
	    return;

	if (n == exp->words[W_VERB]) {
	    fe_verb = temp;
	} else if (n == exp->words[W_FILE]) {
	    fe_file = temp;
	} else if (n == exp->words[W_TEXT]) {
	    fe_text = temp;
	} else {
	    if (n == exp->words[W_LINE])
		fe_line = atoi(tb_values(temp));
	    else if (n == exp->words[W_COLM])
		fe_colm = atoi(tb_values(temp));
	    tb_free(&temp);
	}
    }
}

/* Edits the file and goes to the line pointed at by the next compiler error in
 * the "[output]" window.  It unfortunately doesn't mark the lines for you, so
 * adding lines to the file throws off the later numbering.  Solutions to this
 * seem messy at the moment
 */

/* ARGSUSED */
int
finderr(int f GCC_UNUSED, int n GCC_UNUSED)
{
    register BUFFER *sbp;
    register int s;
    LINE *dotp;
    int moveddot = FALSE;
    ERR_PATTERN *exp;

    char *errverb;
    char *errfile;
    char *errtext;
    char ferrfile[NFILEN];
    ALLOC_T len;

    static int oerrline = -1;
    static TBUFF *oerrfile;
    static TBUFF *oerrtext;

    static LINE *odotp = 0;

#define DIRLEVELS 20
    static int l = 0;
    static char *dirs[DIRLEVELS];

    if (!comp_err_exps(FALSE, 1))
	return (FALSE);

    /* look up the right buffer */
    if ((sbp = find_b_name(febuff)) == NULL) {
	mlforce("[No buffer to search for errors.]");
	return (FALSE);
    }
    if (newfebuff) {
	oerrline = -1;
	oerrfile = tb_init(&oerrfile, EOS);
	oerrtext = tb_init(&oerrtext, EOS);
	while (l)
	    free(dirs[l--]);
	odotp = 0;
    }
    dotp = getdot(sbp);

    if (newfebuff || dotp != odotp) {

	LINE *tdotp;

	while (l)
	    free(dirs[l--]);

	tdotp = lforw(buf_head(sbp));

	/* check for Entering/Leaving lines from top of buffer
	 * to dot.  if we'd gotten here in sequence, then we'd
	 * already have tracked them, but since we seem to
	 * have jumped out of sequence we need to recalibrate
	 * the directory stack against our current position.
	 */
	while (tdotp != dotp) {

	    if (lisreal(tdotp)) {
		ALLOC_T count = 0;

		while ((exp = next_pattern(count++)) != 0) {
		    if (exp->words[W_VERB] > 0)
			if (lregexec(exp->exp_comp, tdotp, 0, llength(tdotp)))
			    break;
		}

		if (exp != 0) {
		    decode_exp(exp);

		    errverb = tb_values(fe_verb);
		    errfile = tb_values(fe_file);

		    if (errverb != 0 && errfile != 0) {
			if (!strcmp("Entering", errverb)) {
			    if (l < DIRLEVELS) {
				dirs[++l] = strmalloc(errfile);
			    }
			} else if (!strcmp("Leaving", errverb)) {
			    if (l > 0)
				free(dirs[l--]);
			}
		    }
		} else if (interrupted()) {
		    kbd_alarm();
		    return ABORT;
		}
	    }
	    tdotp = lforw(tdotp);
	}
    }
    newfebuff = FALSE;

    for_ever {
	/* To use this line, we need both the filename and the line
	 * number in the expected places, and a different line than
	 * last time.
	 */
	if (lisreal(dotp)) {
	    ALLOC_T count = 0;

	    while ((exp = next_pattern(count++)) != 0
		&& !lregexec(exp->exp_comp, dotp, 0, llength(dotp))) ;

	    if (exp != 0) {
		decode_exp(exp);

		errverb = tb_values(fe_verb);
		errfile = tb_values(fe_file);
		errtext = tb_values(fe_text);

		if (errfile != 0
		    && fe_line > 0) {
		    if (oerrline != fe_line
			|| strcmp(tb_values(oerrfile), errfile))
			break;
		    if (oerrline == fe_line
			&& errtext != 0
			&& strcmp(tb_values(oerrtext), errtext))
			break;
		} else if (errverb != 0
		    && errfile != 0) {
		    if (!strcmp("Entering", errverb)) {
			if (l < DIRLEVELS) {
			    dirs[++l] = strmalloc(errfile);
			}
		    } else if (!strcmp("Leaving", errverb)) {
			if (l > 0)
			    free(dirs[l--]);
		    }
		}
	    }
	}
	if (interrupted()) {
	    kbd_alarm();
	    return ABORT;
	} else if (lforw(dotp) == buf_head(sbp)) {
	    mlwarn("[No more errors in %s buffer]", febuff);
	    /* start over at the top of file */
	    putdotback(sbp, lforw(buf_head(sbp)));
	    while (l)
		free(dirs[l--]);
	    return FALSE;
	}
	dotp = lforw(dotp);
	moveddot = TRUE;
    }
    /* put the new dot back, before possible changes to contents
       of current window from getfile() */
    if (moveddot)
	putdotback(sbp, dotp);

    odotp = dotp;

    (void) lengthen_path(pathcat(ferrfile, dirs[l], errfile));

    if (strcmp(ferrfile, curbp->b_fname)) {
	/* if we must change windows */
	WINDOW *wp;
	for_each_visible_window(wp) {
	    if (!strcmp(wp->w_bufp->b_fname, ferrfile))
		break;
	}
	if (wp) {
	    curwp = wp;
	    make_current(curwp->w_bufp);
	    upmode();
	} else {
	    s = getfile(ferrfile, TRUE);
	    if (s != TRUE)
		return s;
	}
    }
    if (errtext) {
	mlforce("%s", errtext);
	len = strlen(errtext);
    } else {
	mlforce("Error: %*S", dotp->l_used, dotp->l_text);
	errtext = dotp->l_text;
	len = dotp->l_used;
    }
    if ((oerrtext = tb_init(&oerrtext, EOS)) != 0) {
	tb_bappend(&oerrtext, errtext, len);
	tb_append(&oerrtext, EOS);
    }
    /* it's an absolute move */
    curwp->w_lastdot = DOT;
    s = gotoline(TRUE, -(curbp->b_lines_on_disk - fe_line + 1));
    DOT.o = fe_colm ? fe_colm - 1 : 0;

    oerrline = fe_line;
    (void) tb_scopy(&oerrfile, errfile);

    return s;
}

static LINE *
getdot(BUFFER *bp)
{
    register WINDOW *wp;
    if (bp->b_nwnd) {
	/* scan for windows holding that buffer,
	   pull dot from the first */
	for_each_visible_window(wp) {
	    if (wp->w_bufp == bp) {
		return wp->w_dot.l;
	    }
	}
    }
    return bp->b_dot.l;
}

static void
putdotback(BUFFER *bp, LINE *dotp)
{
    register WINDOW *wp;

    if (bp->b_nwnd) {
	for_each_visible_window(wp) {
	    if (wp->w_bufp == bp) {
		wp->w_dot.l = dotp;
		wp->w_dot.o = 0;
		wp->w_flag |= WFMOVE;
	    }
	}
	return;
    }
    /* then the buffer isn't displayed */
    bp->b_dot.l = dotp;
    bp->b_dot.o = 0;
}

/*
 * Ask for a new finderr buffer name
 */
/* ARGSUSED */
int
finderrbuf(int f GCC_UNUSED, int n GCC_UNUSED)
{
    register int s;
    char name[NFILEN + 1];
    BUFFER *bp;

    (void) strcpy(name, febuff);
    if ((s = ask_for_bname("Buffer to scan for \"errors\": ",
		name, sizeof(name))) == ABORT)
	return s;
    if (s == FALSE) {
	set_febuff(OUTPUT_BufName);
    } else {
	if ((bp = find_any_buffer(name)) == 0)
	    return FALSE;
	set_febuff(bp->b_bname);
    }
    return TRUE;
}

/*
 * (Re)compile the error-expressions buffer.  This is needed as an entrypoint
 * so that macros can manipulate the set of expressions (including reading it
 * from a file).
 */
/* ARGSUSED */
int
comp_err_exps(int f GCC_UNUSED, int n GCC_UNUSED)
{
    if (!load_patterns()) {
	mlforce("[No error-expressions are defined.]");
	return (FALSE);
    }
    return TRUE;
}
#endif
