/*
 * find the next error in mentioned in the shell output window.
 * written for vile by paul fox.
 * rewritten to use regular expressions by T.Dickey
 *
 * Copyright (c) 1990-2003 by Paul Fox and Thomas Dickey
 *
 * $Header: /users/source/archives/vile.vcs/RCS/finderr.c,v 1.128 2005/01/21 22:37:35 tom Exp $
 *
 */

#include "estruct.h"
#include "edef.h"
#include "nevars.h"

#if OPT_FINDERR

typedef enum {
    W_VERB = 0
    ,W_FILE
    ,W_LINE
    ,W_COLM
    ,W_TEXT
    ,W_LAST
} ErrTokens;

typedef struct {
    char *exp_text;
    regexp *exp_comp;
    int words[W_LAST];
    int lBase;
    int cBase;
} ERR_PATTERN;

static LINE *getdot(BUFFER *bp);
static void putdotback(BUFFER *bp, LINE *dotp);

static char febuff[NBUFN];	/* name of buffer to find errors in */
static int newfebuff = TRUE;	/* is the name new since last time? */

/*
 * These variables are set as a side-effect of decode_exp().
 */
static TBUFF *fe_verb;
static TBUFF *fe_file;
static TBUFF *fe_text;
static int cC_base;
static int lL_base;
static long fe_colm;
static long fe_line;

/*
 * Beginning of line.
 */
#define P_BOL	"^"

/*
 * Text from running "ant", which puts the program name before each message,
 * in square brackets.
 */
#define P_ANT	P_BOL "\\(\\s\\+\\[[^]]\\+\\]\\s\\+\\)\\?"

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
 *      %C - column number (this has to be an integer)
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
    P_ANT "\"%[^\" \t]\", line %L:%T",	/* various C compilers */
    P_ANT "%F:\\s*%L:%C:\\s*%T",	/* antic */
    P_ANT "%F:\\s*%L:\\s*%T",	/* "grep -n" */

#if SYS_VMS
    "[ \t]*At line number %L in %[^;].*",	/* crude support for DEC C
						 * compilers. Unfortunately,
						 * these compilers place error
						 * text on previous _lines_.
						 */
#endif

#if SYS_SUNOS && defined(SYSTEM_HAS_LINT_PROG)
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
    "^\"%F\":line %L:%T",	/* Solaris lex */
#if defined(_AIX)
    "^\"%[^\" \t]\", line %L\\.%C:%T",	/* AIX C compilers */
#endif
#if defined(clipper) || defined(__clipper__)
    "^\"%[^\" \t]\", line %L (col. %C\\+):%T",	/* CLIX C compiler */
#endif

#ifdef _MSC_VER
    "^[ \t]*%F\\s*(%L)\\s*:%T",	/* MSVC++ */
#endif

    "^%[^\"(](%L)[ \t]\\?:%T",	/* weblint */
#if SYS_UNIX && defined(SYSTEM_HAS_LINT_PROG)
    /* sys5 lint */
    "^    [^ \t]\\+[ \t]\\+%[^(](%L)$",
    "^    [^(].*( arg %L ) \t%[^( \t](%L) :: [^(]\\+(%L))",
    "^    .* :: %[^(](%L)",
#endif

#if SYS_OS2 || SYS_OS2_EMX
    "^%[^(](%L:%C) : %T",	/* CSet compiler */
#endif
#if CC_WATCOM
    "^%[^(](%L): %T",
#endif

    /* Borland C++ */
#if CC_NEWDOSCC
    "^Error\\( [^ ]\\+\\)\\? %F %L: %T",
    "^Warning\\( [^ ]\\+\\)\\? %F %L: %T",
#endif

    "^%B:%L:%T",		/* "pp" in scratch buf */
    "^[^:]\\+: %V directory `%[^']'",	/* GNU make */
    "%T at %F line %L.*",	/* perl 5 */
    "^%F\\[%L\\]:%T",		/* hgrep */
    "^\"%[^\"]\", line %L, col %C, %T",		/* ncurses, atac */
    "^\"%[^\"]\", line %L, %T",	/* ncurses */
};

static ERR_PATTERN *exp_table = 0;
static size_t exp_count = 0;

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

static const char *
get_token_name(ErrTokens n)
{
    char *result;

    switch (n) {
    case W_VERB:
	result = "verb";
	break;
    case W_FILE:
	result = "filename";
	break;
    case W_LINE:
	result = "line";
	break;
    case W_COLM:
	result = "column";
	break;
    case W_TEXT:
	result = "text";
	break;
    default:
	result = "unknown";
	break;
    }
    return result;
}

static int
marks_in(const char *expr)
{
    int result = 0;
    int escaped = FALSE;

    while (*expr != EOS) {
	if (escaped) {
	    if (*expr == L_PAREN)
		result++;
	    escaped = FALSE;
	} else if (*expr == BACKSLASH) {
	    escaped = TRUE;
	}
	expr++;
    }
    return result;
}

/*
 * Convert a given error-pattern to regular expression
 */

#define	APP_T(S) if (pass == 1) want += strlen(S); \
		 else dst = lsprintf(dst, "%s", S)
#define	APP_S(S) if (pass == 1) want += sizeof(S); \
		 else dst = lsprintf(dst, "%s", S)
#define APP_C    if (pass != 1) *dst++ = *src

/*
 * Convert the pattern; returns false if malloc's fail.
 */
static int
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
    int status = TRUE;
    size_t want = llength(lp);
    char *first = lp->l_text;
    char *last = first + want;

    (void) memset(errp, 0, sizeof(*errp));
    TPRINTF(("error-pattern %.*s\n", (int) want, first));

    /* In the first pass, find the number of fields we'll substitute.
     * Then allocate a new string that's a genuine regular expression
     */
    for (pass = 1; pass <= 2; pass++) {
	for (src = first, word = 0, range = FALSE; src < last; src++) {
	    if (*src == BACKSLASH) {
		APP_C;
		if (++src == last)
		    break;
		if (*src == L_PAREN)	/* a group we don't own... */
		    word++;
		APP_C;
	    } else if (*src == '%') {
		mark = -1;
		switch (*++src) {
		case 'V':
		    mark = W_VERB;
		    break;
		case 'F':
		    if (tb_values(filename_expr) != 0) {
			APP_S(before);
			APP_T(tb_values(filename_expr));
			APP_S(after);
			errp->words[W_FILE] = ++word;
			word += marks_in(tb_values(filename_expr));
		    }
		    break;
		case 'B':
		    APP_S("\\(\\[[^:]\\+]\\)");
		    errp->words[W_FILE] = ++word;
		    break;
		case 'T':
		    APP_S(remain);
		    errp->words[W_TEXT] = ++word;
		    break;
		case 'c':
		case 'C':
		    errp->cBase = ((*src) == 'C');
		    APP_S(number);
		    errp->words[W_COLM] = ++word;
		    break;
		case 'l':
		case 'L':
		    errp->lBase = ((*src) == 'L');
		    APP_S(number);
		    errp->words[W_LINE] = ++word;
		    break;
		case L_BLOCK:
		    range = TRUE;
		    APP_S(before);
		    APP_C;
		    if (src[1] == '^') {
			src++;
			APP_C;
		    }
		    if (src[1] == R_BLOCK) {
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
	    } else if ((*src == R_BLOCK) && range) {
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
	    beginDisplay();
	    dst = temp = typeallocn(char, want + 1);
	    endofDisplay();
	    if (dst == 0) {
		status = no_memory("convert_pattern");
		break;
	    }
	} else {
	    *dst = EOS;
	}
    }
    if (temp != 0) {
#if OPT_TRACE
	TRACE(("COMPILE %s\n", temp));
	for (word = 0; word < W_LAST; word++)
	    TRACE(("word[%d] = %d (%s)\n", word, errp->words[word],
		   get_token_name((ErrTokens) word)));
#endif
	TPRINTF(("-> %s\n", temp));
	exp = regcomp(temp, strlen(temp), TRUE);
	/* FIXME:  this might be null if the pattern was incorrect, or if we
	 * ran out of memory.  We only want the latter condition.
	 */
    }
    errp->exp_text = temp;
    errp->exp_comp = exp;

    return status;
}

/*
 * Free the storage currently used in this module
 */
static void
free_patterns(void)
{
    if (exp_table != 0) {
	beginDisplay();
	while (exp_count-- != 0) {
	    free(exp_table[exp_count].exp_text);
	    free((char *) (exp_table[exp_count].exp_comp));
	}
	free((char *) exp_table);
	exp_table = 0;
	exp_count = 0;
	endofDisplay();
    }
}

#if OPT_UPBUFF
/* ARGSUSED */
int
free_err_exps(BUFFER *bp GCC_UNUSED)
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
    size_t n;
    int status = TRUE;

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
    update_scratch(ERRORS_BufName, free_err_exps);
    bp->b_rmbuff = free_err_exps;
#endif

    if (exp_count == 0) {
	beginDisplay();
	exp_count = bp->b_linecount;
	exp_table = typeallocn(ERR_PATTERN, exp_count);
	endofDisplay();

	if (exp_table != 0) {
	    for (n = 0; n < W_LAST; n++)
		exp_table->words[n] = -1;
	    n = 0;
	    for_each_line(lp, bp) {
		if (!convert_pattern(&exp_table[n++], lp)) {
		    status = FALSE;
		    break;
		}
	    }
	} else {
	    status = no_memory("load_patterns");
	}
    }
    return status;
}

/*
 * Initialize this module by converting the error-patterns to regular
 * expressions.  Return the count'th item in the error-patterns list, or null
 * if count is out of range.
 */
static ERR_PATTERN *
next_pattern(size_t count)
{
    ERR_PATTERN *result = 0;

    if (count < exp_count)
	result = &exp_table[count];
    return (result);
}

/*
 * Decode the matched ERR_PATTERN
 */
static int
decode_exp(ERR_PATTERN * exp)
{
    /* *INDENT-OFF* */
    static struct {
	ErrTokens code;
	TBUFF **buffer;
	long *number;
    } lookup[] = {
	{ W_VERB, &fe_verb, 0 },
	{ W_FILE, &fe_file, 0 },
	{ W_LINE, 0, &fe_line },
	{ W_COLM, 0, &fe_colm },
	{ W_TEXT, &fe_text, 0 },
    };
    /* *INDENT-ON* */

    regexp *p = exp->exp_comp;
    int j, n;
    int failed = FALSE;
    TBUFF *temp;

    TRACE(("decode_exp{%s}\n", exp->exp_text));

    for (j = 0; j < W_LAST; j++) {
	if (lookup[j].buffer != 0) {
	    tb_free(lookup[j].buffer);
	} else {
	    *(lookup[j].number) = 0;
	}
    }
    cC_base = exp->cBase;
    lL_base = exp->lBase;
    fe_colm = cC_base;

    /*
     * Count the atoms separately from the loop indices because when
     * we do
     *          setv $filename-expr='\([a-zA-Z]:\)\?[^ ^I\[:]'
     * the resulting array will have an extra entry with null pointers
     * for the nested atom in the %F expression:
     *          \([a-zA-Z]:\)
     */
    for (n = 1; !failed && (n < NSUBEXP); n++) {
	if (p->startp[n] == 0 || p->endp[n] == 0)
	    continue;		/* discount nested atom */
	if (p->startp[n] >= p->endp[n])
	    continue;		/* discount empty atom */
	temp = 0;
	if (tb_bappend(&temp,
		       p->startp[n],
		       (size_t) (p->endp[n] - p->startp[n])) == 0
	    || tb_append(&temp, EOS) == 0) {
	    (void) no_memory("finderr");
	    failed = TRUE;
	} else if (tb_length(temp) == 0) {
	    mlforce("BUG: marker %d is empty string", n);
	    failed = TRUE;
	} else {
	    for (j = 0; j < W_LAST; j++) {
		ErrTokens code = lookup[j].code;
		if (n == exp->words[code]) {
		    if (lookup[j].buffer) {
			*(lookup[j].buffer) = temp;
			TRACE(("matched %s:%s\n", get_token_name(code),
			       tb_values(temp)));
		    } else {
			*(lookup[j].number) = vl_atol(tb_values(temp), 10, &failed);
			TRACE(("matched %s:%s(%ld)\n", get_token_name(code),
			       tb_values(temp), *(lookup[j].number)));
			if (failed) {
			    mlforce("BUG: \"%s\" (marker %d) is not a number: %s",
				    get_token_name(code),
				    n,
				    tb_values(temp));
			}
			tb_free(&temp);
		    }
		}
	    }
	}
    }

    if (!failed
	&& (fe_colm != cC_base && fe_line == 0)) {
	mlforce("BUG: found column %ld but no line", fe_colm);
	failed = TRUE;
    }

    /*
     * There's not enough room on the message line to show which expression
     * failed - but update $error-expr anyway.
     */
    if (failed)
	var_ERROR_EXPR((TBUFF **) 0, exp->exp_text);

    return failed;
}

#define set_tabstop_val(bp,value) set_b_val(bp, VAL_TAB, value)

static void
goto_column(void)
{
    int saved_tabstop = tabstop_val(curbp);

    if (error_tabstop > 0)
	set_tabstop_val(curbp, error_tabstop);

    gocol(fe_colm ? fe_colm - cC_base : 0);

    if (error_tabstop > 0)
	set_tabstop_val(curbp, saved_tabstop);
}

#define DIRLEVELS 20
static int dir_level = 0;
static char *dir_stack[DIRLEVELS];

static void
freeDirs(void)
{
    beginDisplay();
    while (dir_level) {
	free(dir_stack[dir_level]);
	--dir_level;
    }
    endofDisplay();
}

static void
updateDirs(const char *errverb, const char *errfile)
{
    if (!strcmp("Entering", errverb)) {
	if (dir_level < DIRLEVELS) {
	    beginDisplay();
	    ++dir_level;
	    dir_stack[dir_level] = strmalloc(errfile);
	    endofDisplay();
	}
    } else if (!strcmp("Leaving", errverb)) {
	if (dir_level > 0) {
	    beginDisplay();
	    if (dir_stack[dir_level] != 0)
		free(dir_stack[dir_level]);
	    --dir_level;
	    endofDisplay();
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
    BUFFER *sbp;
    int status;
    LINE *dotp;
    int moveddot = FALSE;
    ERR_PATTERN *exp;
    size_t count;

    char *errverb;
    char *errfile;
    char *errtext;
    char ferrfile[NFILEN];
    size_t len;

    static int oerrline = -1;
    static TBUFF *oerrfile;
    static TBUFF *oerrtext;

    static LINE *odotp = 0;

    if (!comp_err_exps(FALSE, 1))
	return (FALSE);

    /* look up the right buffer */
    if ((sbp = find_b_name(febuff)) == NULL) {
	mlforce("[No buffer to search for errors.]");
	return (FALSE);
    }
    if (newfebuff) {
	beginDisplay();
	oerrline = -1;
	oerrfile = tb_init(&oerrfile, EOS);
	oerrtext = tb_init(&oerrtext, EOS);
	freeDirs();
	odotp = 0;
	endofDisplay();
    }
    dotp = getdot(sbp);

    if (newfebuff || dotp != odotp) {

	LINE *tdotp;

	freeDirs();

	tdotp = lforw(buf_head(sbp));

	/* check for Entering/Leaving lines from top of buffer
	 * to dot.  if we'd gotten here in sequence, then we'd
	 * already have tracked them, but since we seem to
	 * have jumped out of sequence we need to recalibrate
	 * the directory stack against our current position.
	 */
	TRACE(("check for Entering/Leaving lines\n"));
	while (tdotp != dotp) {

	    if (lisreal(tdotp)) {
		count = 0;
		while ((exp = next_pattern(count++)) != 0) {
		    if (exp->words[W_VERB] > 0)
			if (lregexec(exp->exp_comp, tdotp, 0, llength(tdotp)))
			    break;
		}

		if (exp != 0) {
		    if (decode_exp(exp))
			return ABORT;

		    errverb = tb_values(fe_verb);
		    errfile = tb_values(fe_file);

		    if (errverb != 0 && errfile != 0) {
			updateDirs(errverb, errfile);
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

    TRACE(("look for matching line\n"));
    for_ever {
	/* To use this line, we need both the filename and the line
	 * number in the expected places, and a different line than
	 * last time.
	 */
	if (lisreal(dotp)) {
	    count = 0;
	    while ((exp = next_pattern(count++)) != 0
		   && !lregexec(exp->exp_comp, dotp, 0, llength(dotp))) ;

	    if (exp != 0) {
		TRACE(("matched TEXT:%.*s\n", llength(dotp), dotp->l_text));
		if (decode_exp(exp))
		    return ABORT;

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
		} else if (errverb != 0 && errfile != 0) {
		    updateDirs(errverb, errfile);
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
	    freeDirs();
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

    (void) lengthen_path(pathcat(ferrfile, dir_stack[dir_level], errfile));

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
	    status = getfile(ferrfile, TRUE);
	    if (status != TRUE)
		return status;
	}
    }
    if (errtext) {
	mlforce("%s", errtext);
	len = strlen(errtext);
    } else {
	mlforce("Error: %.*s", dotp->l_used, dotp->l_text);
	errtext = dotp->l_text;
	len = dotp->l_used;
    }
    if ((oerrtext = tb_init(&oerrtext, EOS)) != 0) {
	tb_bappend(&oerrtext, errtext, len);
	tb_append(&oerrtext, EOS);
    }
    /* it's an absolute move */
    curwp->w_lastdot = DOT;
    if (fe_line <= lL_base) {
	status = gotobob(f, n);
    } else if ((fe_line - lL_base) >= curbp->b_lines_on_disk) {
	status = gotoeob(f, n);
    } else {
	status = gotoline(TRUE, -(curbp->b_lines_on_disk - fe_line + lL_base));
    }
    goto_column();

    oerrline = fe_line;
    (void) tb_scopy(&oerrfile, errfile);
    if (status == TRUE) {
	TBUFF *match = 0;
	var_ERROR_EXPR((TBUFF **) 0, exp_table[count - 1].exp_text);
	if (tb_bappend(&match, dotp->l_text, dotp->l_used)
	    && tb_append(&match, EOS) != 0) {
	    var_ERROR_MATCH((TBUFF **) 0, tb_values(match));
	    tb_free(&match);
	}
	updatelistvariables();
    }

    return status;
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

#define ERR_PREFIX 8

static void
make_err_regex_list(int dum1 GCC_UNUSED, void *ptr GCC_UNUSED)
{
    BUFFER *bp;
    LINE *lp;

    bprintf("--- Error Meta-Expressions and Resulting Regular Expressions %*P",
	    term.cols - 1, '-');

    if (exp_table == 0)
	load_patterns();

    if (exp_table != 0
	&& (bp = find_b_name(ERRORS_BufName)) != 0) {
	size_t j = 0;
	b_set_left_margin(curbp, ERR_PREFIX);
	for_each_line(lp, bp) {
	    int k, first = TRUE;
	    if (j >= exp_count)
		break;
	    bprintf("\n%7d ", (int) j);
	    bprintf("%.*s\n", llength(lp), lp->l_text);
	    bprintf("%.*s%s", ERR_PREFIX, " ", exp_table[j].exp_text);
	    for (k = 0; k < W_LAST; k++) {
		if (exp_table[j].words[k] != 0) {
		    if (first) {
			bprintf("\n%.*s", ERR_PREFIX, " ");
			first = FALSE;
		    } else {
			bprintf(", ");
		    }
		    bprintf("%s=\\%d", get_token_name((ErrTokens) k),
			    exp_table[j].words[k]);
		}
	    }
	    ++j;
	}
    }
}

/* ARGSUSED */
static int
show_ErrRegex(BUFFER *bp GCC_UNUSED)
{
    return liststuff(ERR_REGEX_BufName,
		     FALSE, make_err_regex_list, 0, (void *) 0);
}

int
show_err_regex(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return show_ErrRegex(curbp);
}

#if OPT_UPBUFF
void
update_err_regex(void)
{
    update_scratch(ERR_REGEX_BufName, show_ErrRegex);
}

#else
#define update_err_regex()	/*nothing */
#endif

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
    update_err_regex();
    return TRUE;
}
#endif
