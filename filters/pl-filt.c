/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/pl-filt.c,v 1.39 2002/11/01 01:50:27 tom Exp $
 *
 * Filter to add vile "attribution" sequences to perl scripts.  This is a
 * translation into C of an earlier version written for LEX/FLEX.
 */

#include <filters.h>

#ifdef DEBUG
DefineOptFilter("perl", "d");
#else
DefineFilter("perl");
#endif

#define ESC     '\\'
#define SQUOTE  '\''
#define DQUOTE  '"'

#define L_CURLY '{'
#define R_CURLY '}'
#define L_PAREN '('
#define R_PAREN ')'
#define L_BLOCK '['
#define R_BLOCK ']'

#define QUOTE_DELIMS "#:/?|!:%`',{}[]()"
/* from Perl's S_scan_str() */
#define LOOKUP_TERM "([{< )]}> )]}>"

#define isIdent(c)   (isalnum(CharOf(c)) || c == '_')
#define isBlank(c)   ((c) == ' ' || (c) == '\t')
#define isPattern(c) ((c) == '/' || (c) == '?')

#ifdef DEBUG
#define DPRINTF(params) if(flt_options['d'])printf params
#else
#define DPRINTF(params)		/*nothing */
#endif

typedef enum {
    eCODE
    ,eHERE
    ,ePATTERN
    ,ePOD
    ,eIGNORED
} States;

static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *String_attr;
static char *Preproc_attr;
static char *Number_attr;

/*
 * The in-memory copy of the input file.
 */
static char *the_file;
static char *the_last;
static size_t the_size;

#ifdef DEBUG
static char *
stateName(States state)
{
    char *result;
    switch (state) {
    case eCODE:
	result = "CODE";
	break;
    case eHERE:
	result = "HERE";
	break;
    case ePATTERN:
	result = "PATTERN";
	break;
    case ePOD:
	result = "POD";
	break;
    case eIGNORED:
	result = "IGNORED";
	break;
    default:
	result = "?";
    }
    return result;
}
#endif

/******************************************************************************
 * Lexical functions that match a particular token type                       *
 ******************************************************************************/
static int
is_BLANK(char *s)
{
    int found = 0;
    while ((s != the_last) && isBlank(*s)) {
	found++;
	s++;
    }
    return found;
}

static int
is_INTEGER(char *s)
{
    int found = 0;
    while ((s != the_last) && isdigit(CharOf(*s))) {
	found++;
	s++;
    }
    return found;
}

static int
is_ESCAPED(char *s)
{
    int found = 0;
    if (*s == ESC) {
	found = 1;
    }
    return found;
}

static int
is_STRINGS(char *s, int *err, int delim)
{
    char *base = s;
    int found = 0;
    int escape = 0;

    *err = 0;
    if (*s == delim) {
	s++;
	for (;;) {
	    if (s == the_last) {
		*err = 1;	/* unterminated string */
		break;
	    }
	    if (!escape && (*s == ESC)) {
		escape = 1;
	    } else {
		if (!escape && (*s == delim)) {
		    s++;
		    break;
		}
		escape = 0;
	    }
	    s++;
	}
	found = s - base;
    }
    return found;
}

static int
is_KEYWORD(char *s)
{
    char *base = s;
    int ch;
    int quote = 0;

    while (s != the_last) {
	ch = CharOf(*s);
	if (ch == SQUOTE) {	/* leading or embedded - obs */
	    /* This works for "&'name" and "name'two" */
	    if (s == base) {
		if (s == the_file || s[-1] != '&') {
		    return 0;
		}
	    } else {
		if (s + 1 == the_last
		    || !isalpha(CharOf(s[1]))) {
		    return 0;
		}
	    }
	    s++;
	    quote++;
	} else if (isalpha(CharOf(ch))
		   || ch == '_'
		   || (s != base && (isdigit(CharOf(ch)))))
	    s++;
	else
	    break;
    }
    if (quote == (s - base))
	s = base;
    return s - base;
}

static int
is_QIDENT(char *s)
{
    char *base = s;
    int ch;

    while (s != the_last) {
	ch = CharOf(*s);
	if (isalpha(ch)
	    || (s != base && isdigit(CharOf(ch)))
	    || ch == ESC
	    || ch == '_'
	    || ch == SQUOTE
	    || ch == DQUOTE)
	    s++;
	else
	    break;
    }
    return s - base;
}

static int
is_NORMALVARS(char *s)
{
    char *base = s;
    int ch;
    int quoted = 0;
    int part1 = 0;
    int part2 = 0;

    while (s != the_last) {
	ch = CharOf(*s);
	if (s == base) {
	    if (strchr("$%@", ch) == 0) {
		break;
	    }
	} else if (quoted) {
	    if (isalnum(ch))
		part2 = 1;
	    else
		break;
	} else {
	    if (ch == SQUOTE) {
		quoted = 1;
	    } else if (isalnum(ch) || ch == '_') {
		part1 = 1;
	    } else {
		break;
	    }
	}
	s++;
    }
    return (part1 && (quoted == part2)) ? (s - base) : 0;
}

static int
is_OTHERVARS(char *s)
{
    char *base = s;
    int ch;
    int part1 = 0;
    int part2 = 0;

    while (s != the_last) {
	ch = CharOf(*s);
	if (s == base) {
	    if (ch != '$') {
		break;
	    }
	} else if (s == base + 1) {
	    if (ch == '^') {
		/*EMPTY */ ;
	    } else if (strchr("-_./,\"\\#%=~|$?&`'+*[];!@<>():", ch) != 0) {
		part1 = 1;
	    } else {
		break;
	    }
	} else if (s == base + 2) {
	    if (part1) {
		break;
	    } else if (ch >= '@' && ch < '\177') {
		part2 = 1;
	    }
	} else {
	    break;
	}
	s++;
    }
    return (part1 || part2) ? (s - base) : 0;
}

static int
is_NUMBER(char *s, int *err)
{
    char *base = s;
    int state = 0;
    int value = 0;
    int radix = 0;
    int dot = 0;

    *err = 0;
    while (s != the_last) {
	int ch = CharOf(*s);

	if ((s == base) && (ch == '+' || ch == '-')) {
	    /* EMPTY */ ;
	} else if ((s == base) && (ch == 'v')) {
	    radix = 11;
	} else if (radix == 11) {
	    if (ch != '_') {
		if (isdigit(ch)) {
		    value = 1;
		} else if (ch != '.' || !isdigit(s[-1])) {
		    break;
		}
	    }
	} else if (ch == '.') {
	    if (the_last - s > 1
		&& s[1] == '.') {
		break;
	    }
	    if (radix == 8)
		radix = 10;
	    if (dot || (radix != 0 && radix != 10) || (state > 1)) {
		*err = 1;
	    }
	    dot = 1;
	    radix = 10;
	    state = 1;
	} else if (ch == '_') {
	    /* EMPTY */ ;
	} else if (radix == 0) {
	    if (ch == '0') {
		if ((s + 1) == the_last) {
		    radix = 10;
		} else if (s[1] == 'x') {
		    radix = 16;
		    s++;
		} else if (s[1] == 'b') {
		    radix = 2;
		    s++;
		} else {
		    radix = 8;
		}
		value = 1;
	    } else if (isdigit(ch)) {
		radix = 10;
		value = 1;
	    } else {
		break;
	    }
	} else if (dot && (ch == 'e' || ch == 'E')) {
	    if (state != 1 || !value)
		*err = 1;
	    state = 2;
	} else {
	    if (((state || (radix == 10)) && isdigit(ch))
		|| ((radix == 16) && isxdigit(ch))
		|| ((radix == 8) && (ch >= '0' && ch < '8'))
		|| ((radix == 2) && (ch >= '0' && ch < '2'))) {
		value = 1;
	    } else {
		if (value) {
		    while (s != the_last) {
			ch = CharOf(*s);
			if (isalnum(ch)) {
			    *err = 1;
			    ++s;
			} else {
			    break;
			}
		    }
		}
		break;
	    }
	}
	s++;
    }

    return value ? (s - base) : 0;
}

static int
is_IDENT(char *s)
{
    int found;

    if ((found = is_NORMALVARS(s)) == 0)
	found = is_OTHERVARS(s);
    return found;
}

static int
is_NAME(char *s)
{
    int found = 0;

    while (isIdent(s[found])) {
	++found;
    }
    return found;
}

/* for compatibility with perlfilt.l, process single-line comments only */
static int
is_COMMENT(char *s)
{
    char *base = s;
    char *t = is_BLANK(s) + s;

    if (*t++ == '#') {
	while (t != the_last) {
	    if (*t == '\n') {
		if ((t + 1) == the_last
		    || t[1] != '#')
		    break;
	    }
	    t++;
	}
	s = t;
    }
    return (s - base);
}

/* preprocessor stuff:
 *
 *	/^#\s*line\s+(\d+)\s*(?:\s"([^"]+)")?\s*$/
 */
static int
is_PREPROC(char *s)
{
    char *base = s;
    int skip;
    int err;

    s += is_BLANK(s);
    if (*s++ != '#')
	return 0;
    s += is_BLANK(s);
    if (strncmp(s, "line", 4))
	return 0;
    if ((skip = is_BLANK(s += 4)) == 0)
	return 0;
    if ((skip = is_INTEGER(s += skip)) == 0)
	return 0;
    s += is_BLANK(s += skip);
    if (*s == DQUOTE) {
	s += is_STRINGS(s, &err, DQUOTE);
	if (err)
	    return 0;
    }
    s += is_BLANK(s);
    return (s - base);
}

/******************************************************************************
 ******************************************************************************/

static int
end_marker(char *s, char *marker, int only)
{
    int len = strlen(marker);

    return (the_last - s > len
	    && !strncmp(s, marker, len)
	    && (!only || (s[len] == '\n')));
}

static int
begin_POD(char *s)
{
    int skip = 0;
    while (skip < 2 && s != the_last) {
	if (*s == '\n')
	    ++skip;
	else if (*s != '\r')
	    return 0;
	++s;
    }
    return (the_last - s > 2
	    && s[0] == '='
	    && isalpha(CharOf(s[1])));
}

static int
end_POD(char *s)
{
    return end_marker(s, "=cut", 0);
}

/*
 * See discussion of "here-doc" in perldata.
 * FIXME: we're only implementing a single here-doc (perldata says they
 * can be stacked), and don't check for unbalanced quotes).
 */
static char *
begin_HERE(char *s, int *quoted)
{
    char *base;
    int ok;

    if (the_last - s > 2
	&& s[0] == '<'
	&& s[1] == '<') {
	s += 2;
	if ((ok = is_BLANK(s)) != 0)
	    s += ok;
	base = s;
	if ((ok = is_QIDENT(s)) != 0) {
	    unsigned temp = 0;
	    char *marker = do_alloc((char *) 0, ok + 1, &temp);
	    char *d = marker;
	    s += ok;
	    *quoted = 0;
	    while (base != s) {
		if (*base != SQUOTE && *base != DQUOTE && *base != ESC)
		    *d++ = *base;
		else
		    *quoted = 1;
		base++;
	    }
	    *d = 0;
	    return marker;
	}
    }
    return 0;
}

static char *
skip_BLANKS(char *s)
{
    char *base = s;

    while (s != the_last) {
	if (!isspace(CharOf(*s))) {
	    break;
	}
	++s;
    }
    if (s != base) {
	flt_puts(base, s - base, "");
    }
    return s;
}

/*
 * Return the first character, skipping optional blanks
 */
static int
after_blanks(char *s)
{
    int result = '\0';

    while (s != the_last) {
	if (!isspace(CharOf(*s))) {
	    result = CharOf(*s);
	    break;
	}
	++s;
    }
    return result;
}

/*
 * FIXME: the only place that perlfilt.l recognizes a PATTERN is after "!~"
 * or "=~".  Doing that in other places gets complicated - the reason for
 * moving to C.
 */
static int
begin_PATTERN(char *s)
{
    if (the_last - s > 2
	&& (s[0] == '!' || s[0] == '=')
	&& s[1] == '~') {
	return 2 + is_BLANK(s + 2);
    }
    return 0;
}

/*
 * If we're pointing to a quote-like operator, return its length.
 * As a side-effect, set the number of delimiters (assuming '/') it needs.
 */
static int
is_QUOTE(char *s, int *delims)
{
    char *base = s;
    size_t len;

    *delims = 0;
    while ((s != the_last) && isIdent(*s)) {
	++s;
    }
    if ((len = (s - base)) != 0) {
	switch (len) {
	case 1:
	    if (*base == 'm' || *base == 'q') {
		*delims = 2;
	    } else if (*base == 's' || *base == 'y') {
		*delims = 3;
	    }
	    break;
	case 2:
	    if (!strncmp(base, "tr", 2)) {
		*delims = 3;
	    } else if (!strncmp(base, "qq", 2) ||
		       !strncmp(base, "qx", 2) ||
		       !strncmp(base, "qw", 2) ||
		       !strncmp(base, "qr", 2)) {
		*delims = 2;
	    }
	    break;
	case 4:
	    if (!strncmp(base, "grep", 2)) {
		*delims = 2;
	    }
	    break;
	}
    }
    if (*delims == 0)
	s = base;

    if (s != base) {
	int test = after_blanks(s);
	DPRINTF(("is_Quote(%.*s:%c)", s - base, base, test));
	if (test == '#' && isspace(CharOf(*s)))
	    test = 0;
	if ((test == 0) || (strchr(QUOTE_DELIMS, test) == 0))
	    s = base;
	DPRINTF(("is_QUOTE(%.*s)",
		 (s != base) ? (s - base) : 1,
		 (s != base) ? base : ""));
    }
    return (s - base);
}

static int
add_to_PATTERN(char *s)
{
    char *base = s;
    char *next;
    int need;
    int skip = is_QUOTE(s, &need);

    if (skip == 0)
	need = 2;

    DPRINTF(("before(%d:%s)", skip, s));
    if (the_last - s > need + skip) {
	int delim = 0;
	int escaped = 0;
	int comment = 0;
	int bracketed = 0;

	s += skip;
	while ((s != the_last) && isspace(CharOf(*s))) {
	    s++;
	}
	if (s != the_last) {
	    delim = *s++;
	}
	if (delim == 0)
	    return 0;
	/* from Perl's S_scan_str() */
	if (delim != 0 && (next = strchr(LOOKUP_TERM, delim)) != 0)
	    delim = next[5];
	DPRINTF(("need(%c:%d)", delim, need));
	need--;
	next = s;
	while (s != the_last) {
	    if (comment) {
		if (*s == '\n')
		    comment = 0;
	    } else if (!escaped && (*s == ESC)) {
		escaped = 1;
	    } else {
		if (!escaped) {
		    if (*s == L_CURLY)
			++bracketed;
		    if (*s == R_CURLY)
			if (--bracketed < 0)
			    bracketed = 0;	/* oops */
		    if (*s == '#' && delim == R_CURLY && !bracketed)
			comment = 1;
		    if (*s == delim) {
			DPRINTF(("DELIM%d%c(%.*s)", need, delim,
				 the_last - s, s));
			if (--need <= 0) {
			    ++s;
			    break;
			}
		    }
		}
		escaped = 0;
	    }
	    s++;
	}
	while (s != the_last) {
	    if (!isalpha(CharOf(*s))
		|| *s == ';')
		break;
	    s++;
	}
	DPRINTF(("after(%s)", s));
	return (s - base);
    }
    return 0;
}

/*
 * FIXME: revisit 9.2e's test-case and determine how to handle /x modifier.
 */
static char *
write_PATTERN(char *s, int len)
{
    int delimiter = 0;
    int delims;
    int skip = is_QUOTE(s, &delims);
    int n;
    int first;
    int leading = 0;
    int comment = 0;
    int escaped = 0;
    int range = 0;

    DPRINTF(("write(%.*s)", len, s));
    if (skip) {
	flt_puts(s, skip, Keyword_attr);
	s += skip;
	len -= skip;
    }

    skip = skip_BLANKS(s) - s;
    if (skip) {
	s += skip;
	len -= skip;
    }
    delimiter = *s;

    for (n = first = 0; n < len; n++) {
	if (escaped) {
	    escaped = 0;
	} else if (s[n] == ESC) {
	    escaped = 1;
	} else if (isBlank(s[n]) && !escaped && !comment &&
		   (leading || after_blanks(s + n) == '#')) {
	    flt_puts(s + first, (n - first - 0), String_attr);
	    flt_putc(s[n]);
	    first = n + 1;
	} else if (s[n] == '\n') {
	    leading = 1;
	    if (comment) {
		flt_puts(s + first, (n - first + 1), Comment_attr);
		comment = 0;
		first = n + 1;
	    }
	} else {
	    leading = 0;
	    if ((s[n] == L_BLOCK) && !comment) {
		range = 1;
	    } else if ((s[n] == R_BLOCK) && range) {
		range = 0;
	    } else if ((s[n] == '#')
		       && (delimiter == L_CURLY || (n > 0 && isBlank(s[n - 1])))
		       && !range) {
		if (!comment) {
		    flt_puts(s + first, (n - first - 1), String_attr);
		    comment = 1;
		    first = n + 0;
		}
	    }
	}
    }
    if (comment)
	first += (skip_BLANKS(s) - s);
    flt_puts(s + first, (len - first), comment ? Comment_attr : String_attr);
    s += len;
    return s;
}

static int
is_Option(char *s)
{
    int found = 0;

    if (*s == '-'
	&& ((s + 1) != the_last)
	&& isalpha(CharOf(s[1]))
	&& !isIdent(CharOf(s[2])))
	found = 2;
    return found;
}

static int
is_String(char *s, int *err)
{
    int found = is_STRINGS(s, err, SQUOTE);
    if (!found)
	found = is_STRINGS(s, err, DQUOTE);
    if (!found)
	found = is_ESCAPED(s);
    return found;
}

static int
line_length(char *s)
{
    char *base = s;

    while (s != the_last) {
	if (*s == '\n')
	    break;
	s++;
    }
    return s - base;
}

static char *
put_newline(char *s)
{
    if (s != the_last)
	flt_putc(*s++);
    return s;
}

static int
var_embedded(char *s)
{
    if (*s == '$') {
	if (s[1] == L_PAREN
	    || s[1] == '$') {
	    if (isIdent(s[2]))
		return 0;
	}
    }
    return 1;
}

static char *
put_embedded(char *s, int len, char *attr)
{
    int id;
    int j, k;

    for (j = k = 0; j < len; j++) {
	if ((j == 0 || (s[j - 1] != ESC))
	    && (id = is_IDENT(s + j)) != 0) {
	    if (var_embedded(s + j)) {
		if (j != k)
		    flt_puts(s + k, j - k, attr);
		flt_puts(s + j, id, Ident2_attr);
		k = j + id;
		j = k - 1;
	    } else {
		j += id - 1;
	    }
	}
    }
    if (k < len)
	flt_puts(s + k, len - k, attr);
    return s + len;
}

/*
 * Write the remainder of the line with the given attribute.  If not quoted,
 * highlight identifiers which are embedded in the line.
 */
static char *
put_remainder(char *s, char *attr, int quoted)
{
    int ok = line_length(s);

    if (quoted) {
	flt_puts(s, ok, attr);
	s += ok;
    } else {
	s = put_embedded(s, ok, attr);
    }
    return put_newline(s);
}

/*
 * Check for special cases of keywords after which we may expect a pattern
 * that does not have to be inside parentheses.
 */
static int
check_keyword(char *s, int ok)
{
    switch (ok) {
    case 2:
	return !strncmp(s, "if", ok);
    case 5:
	return !strncmp(s, "split", ok);
    }
    return 0;
}

static char *
put_IDENT(char *s, int ok, int *had_op, int *if_wrd)
{
    char *attr = 0;
    char save = s[ok];

    *had_op = 0;
    s[ok] = '\0';
    attr = keyword_attr(s);
    s[ok] = save;
    flt_puts(s, ok, (attr != 0 && *attr != '\0') ? attr : Ident2_attr);
    *if_wrd = check_keyword(s, ok);
    return s + ok;
}

/******************************************************************************
 ******************************************************************************/

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE * input GCC_UNUSED)
{
    static unsigned used;
    static char *line;

    unsigned actual = 0;
    size_t request = 0;
    States state = eCODE;
    char *s;
    char *marker = 0;
    int in_line = -1;
    int in_stmt = 0;
    int ok;
    int err;
    int save;
    int quoted = 0;
    int parens = 0;
    int had_op = 0;
    int if_wrd = 0;
    int if_old = 0;
    int ignore;

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    /*
     * Read the whole file into a single string, in-memory.  Rather than
     * spend time working around the various continuation-line types _and_
     * Perl's "syntax", let's just concentrate on the latter.
     */
    the_size = 0;
    the_file = 0;
    while (flt_gets(&line, &used) != NULL) {
	size_t len = strlen(line);	/* FIXME: nulls? */
	if (len != 0 && line[len - 1] == '\r')	/* FIXME: move this to readline */
	    len--;
	if ((request = the_size + len) > actual)
	    request = 1024 + (request * 2);
	the_file = do_alloc(the_file, request, &actual);
	if (the_file == 0)
	    break;
	memcpy(the_file + the_size, line, len);
	the_size += len;
    }

    if (the_file != 0) {
	the_last = the_file + the_size;

	s = the_file;
	if_old = if_wrd = 0;
	while (s != the_last) {
	    if (*s == '\n') {
		in_line = -1;
	    } else {
		in_line++;
	    }
	    if_old = if_wrd;
	    if_wrd = 0;
	    DPRINTF(("(%s(%c) line:%d stmt:%d if:%d op:%d)\n",
		     stateName(state), *s, in_line, in_stmt, if_wrd, had_op));
	    switch (state) {
	    case eCODE:
		if (!isspace(*s))
		    ++in_stmt;
		if ((marker = begin_HERE(s, &quoted)) != 0) {
		    state = eHERE;
		    s = put_remainder(s, String_attr, quoted);
		} else if ((ok = begin_PATTERN(s)) != 0) {
		    flt_puts(s, ok, "");
		    s += ok;
		    state = ePATTERN;
		} else if (in_line < 0
			   && begin_POD(s)) {
		    state = ePOD;
		    flt_putc(*s++);
		    flt_putc(*s++);
		    s = put_remainder(s, Comment_attr, 1);
		} else if (in_line == 0
			   && (ok = is_PREPROC(s)) != 0) {
		    flt_puts(s, ok, Preproc_attr);
		    s += ok;
		} else if ((ok = is_COMMENT(s)) != 0) {
		    int skip = (skip_BLANKS(s) - s);
		    ok -= skip;
		    s += skip;
		    flt_puts(s, ok, Comment_attr);
		    s += ok;
		    if_wrd = if_old;
		} else if ((ok = is_BLANK(s)) != 0) {
		    flt_puts(s, ok, "");
		    s += ok;
		    if_wrd = if_old;
		} else if ((if_old || had_op || (in_stmt == 1)) && isPattern(*s)) {
		    state = ePATTERN;
		} else if (*s == L_PAREN) {
		    parens++;
		    had_op = 1;
		    flt_putc(*s++);
		    if (isPattern(*s)) {
			state = ePATTERN;
		    }
		} else if (*s == R_PAREN) {
		    if (--parens < 0)
			parens = 0;
		    flt_putc(*s++);
		    had_op = 0;
		} else if ((ok = is_NUMBER(s, &err)) != 0) {
		    had_op = 0;
		    flt_puts(s, ok, err ? Error_attr : Number_attr);
		    s += ok;
		} else if ((ok = is_KEYWORD(s)) != 0) {
		    if (is_QUOTE(s, &ignore)) {
			state = ePATTERN;
			break;
		    }
		    save = s[ok];
		    s[ok] = 0;
		    if (!strcmp(s, "__END__"))
			state = eIGNORED;
		    had_op = 0;
		    flt_puts(s, ok, keyword_attr(s));
		    if_wrd = check_keyword(s, ok);
		    s[ok] = save;
		    s += ok;
		} else if ((ok = is_Option(s)) != 0) {
		    had_op = 0;
		    flt_puts(s, ok, Keyword_attr);
		    if_wrd = check_keyword(s, ok);
		    s += ok;
		} else if ((ok = is_IDENT(s)) != 0) {
		    s = put_IDENT(s, ok, &had_op, &if_wrd);
		} else if ((ok = is_String(s, &err)) != 0) {
		    had_op = 0;
		    s = put_embedded(s, ok, err ? Error_attr : String_attr);
		} else {
		    if (parens) {
			if (strchr("|&=~!", *s) != 0)
			    had_op = 1;
			else if (!isspace(CharOf(*s)))
			    had_op = 0;
		    } else {
			if (*s == ';')
			    in_stmt = 0;
			if (strchr("=~", *s) != 0)
			    had_op = 1;
		    }
		    flt_putc(*s++);
		}
		break;
	    case eHERE:
		if (end_marker(s, marker, 1)) {
		    state = eCODE;
		    free(marker);
		    marker = 0;
		}
		s = put_remainder(s, String_attr, quoted);
		break;
	    case eIGNORED:
		s = put_remainder(s, Comment_attr, 1);
		break;
	    case ePATTERN:
		s = skip_BLANKS(s);
		if ((ok = is_NAME(s)) != 0 && !is_QUOTE(s, &ignore)) {
		    s = put_IDENT(s, ok, &had_op, &if_wrd);
		} else if ((ok = is_IDENT(s)) != 0 && !is_QUOTE(s, &ignore)) {
		    s = put_IDENT(s, ok, &had_op, &if_wrd);
		} else if ((ok = add_to_PATTERN(s)) != 0) {
		    s = write_PATTERN(s, ok);
		} else {
		    flt_putc(*s++);
		}
		had_op = 0;
		state = eCODE;
		break;
	    case ePOD:
		if (end_POD(s))
		    state = eCODE;
		s = put_remainder(s, Comment_attr, 1);
		break;
	    }
	}
	free(the_file);
    }
}
