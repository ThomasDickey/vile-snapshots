/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/pl-filt.c,v 1.72 2003/12/15 01:42:54 tom Exp $
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

#define QUOTE_DELIMS "#:/?|!:%',{}[]()@;="
/* from Perl's S_scan_str() */
#define LOOKUP_TERM "([{< )]}> )]}>"

#define isIdent(c)   (isalnum(CharOf(c)) || c == '_')
#define isPattern(c) ((c) == '/' || (c) == '?')

#define MORE(s)	     ((s) != the_last)
#define ATLEAST(s,n) (the_last - (s) > (n))

#ifdef DEBUG
#define DPRINTF(params) if(FltOptions('d'))printf params
#else
#define DPRINTF(params)		/*nothing */
#endif

typedef enum {
    eBACKTIC
    ,eCODE
    ,eHERE
    ,ePATTERN
    ,ePOD
    ,eIGNORED
} States;

typedef struct {
    int may_have_pattern;
    int has_no_pattern;
} AfterKey;

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *String_attr;
static char *Preproc_attr;
static char *Number_attr;

static AfterKey nullKey;

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
    case eBACKTIC:
	result = "BACKTIC";
	break;
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
    while (MORE(s) && isBlank(*s)) {
	found++;
	s++;
    }
    return found;
}

static int
is_INTEGER(char *s)
{
    int found = 0;
    while (MORE(s) && isdigit(CharOf(*s))) {
	found++;
	s++;
    }
    return found;
}

/*
 * Actually all we are looking for is escaped quotes.  perl uses backslashes as
 * part of the syntax for referencing variables.  Take that back:  syntax does
 * not really apply to perl...
 */
static int
is_ESCAPED(char *s)
{
    int found = 0;
    if (*s == BACKSLASH) {
	if (ATLEAST(s, 2) && strchr("'`\\", s[1]))
	    found = 2;
	else
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
	    if (!MORE(s)) {
		*err = 1;	/* unterminated string */
		break;
	    }
	    if (!escape && (*s == BACKSLASH)) {
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

    while (MORE(s)) {
	ch = CharOf(*s);
	if (ch == SQUOTE) {	/* leading or embedded - obs */
	    /* This works for "&'name" and "name'two" */
	    if (s == base) {
		if (s == the_file || s[-1] != '&') {
		    return 0;
		}
	    } else {
		if (!ATLEAST(s, 1)
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

/*
 * Test for an identifier or quoted string, which can be used as a
 * here-document marker.
 */
static int
is_QIDENT(char *s)
{
    char *base = s;
    int ch;
    int ok;
    int err;

    if ((ok = is_STRINGS(s, &err, SQUOTE)) != 0) {
	s += ok;
    } else if ((ok = is_STRINGS(s, &err, DQUOTE)) != 0) {
	s += ok;
    } else {
	while (MORE(s)) {
	    ch = CharOf(*s);
	    if (isalpha(ch)
		|| (s != base && isdigit(CharOf(ch)))
		|| ch == BACKSLASH
		|| ch == '_'
		|| ch == SQUOTE
		|| ch == DQUOTE)
		s++;
	    else
		break;
	}
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

    while (MORE(s)) {
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

    while (MORE(s)) {
	ch = CharOf(*s);
	if (s == base) {
	    if (ch != '$') {
		break;
	    }
	} else if (s == base + 1) {
	    if (ch == '^') {
		/*EMPTY */ ;
	    } else if (strchr("-_./,\"\\#%=~|$?&`'+*[];!@<>():", ch) != 0) {
		part1 = ch;
	    } else {
		break;
	    }
	} else if (s == base + 2) {
	    if (part1) {
		if (part1 == '#') {	/* "$#" is followed by an identifier */
		    s += is_KEYWORD(s);
		}
		break;
	    } else if (ch >= '@' && ch < '\177') {
		part2 = ch;
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
    while (MORE(s)) {
	int ch = CharOf(*s);

	if ((s == base) && (ch == '+' || ch == '-')) {
	    /* EMPTY */ ;
	} else if ((s == base) && (ch == 'v')) {
	    radix = 11;
	} else if (radix == 11) {
	    if (ch != '_') {
		if (isdigit(ch)) {
		    value = 1;
		} else if (ch != '.' || !isdigit(CharOf(s[-1]))) {
		    break;
		}
	    }
	} else if (ch == '.') {
	    if (ATLEAST(s, 1)
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
		if (!ATLEAST(s, 1)) {
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
	} else if (value && (ch == 'e' || ch == 'E')) {
	    if (state >= 2 || !value)
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
		    while (MORE(s)) {
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
	while (MORE(t)) {
	    if (*t == '\n') {
		if (!ATLEAST(t, 1)
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
    s += skip;
    s += is_BLANK(s);
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

    return (ATLEAST(s, len)
	    && !strncmp(s, marker, len)
	    && (!only || (s[len] == '\n')));
}

/*
 * Check for one or more blank lines followed by a line beginning with an "="
 * and an alpha-character.  If found, return the length skipped through the
 * "=" (the "=" is painted in a different color).  Otherwise return 0.
 */
static int
begin_POD(char *s)
{
    int result = 0;
    char *base = s;
    int skip = 0;

    while (MORE(s)) {
	if (*s == '\n')
	    ++skip;
	else
	    break;
	++s;
    }
    if (((base == the_file) || (skip >= 2))
	&& ATLEAST(s, 2)
	&& s[0] == '='
	&& isalpha(CharOf(s[1]))) {
	result = s + 1 - base;
	while (base != s) {
	    flt_putc(*base++);	/* skip the newlines */
	}
    }
    return result;
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

    if (ATLEAST(s, 2)
	&& s[0] == '<'
	&& s[1] == '<') {
	s += 2;
	base = s;
	if ((ok = is_QIDENT(s)) != 0) {
	    unsigned temp = 0;
	    char *marker = do_alloc((char *) 0, ok + 1, &temp);
	    char *d = marker;
	    s += ok;
	    *quoted = 0;
	    while (base != s) {
		if (*base != SQUOTE && *base != DQUOTE && *base != BACKSLASH)
		    *d++ = *base;
		else if (*base != DQUOTE)
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

    while (MORE(s)) {
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

    while (MORE(s)) {
	if (!isspace(CharOf(*s))) {
	    result = CharOf(*s);
	    break;
	}
	++s;
    }
    return result;
}

static int
line_size(char *s)
{
    char *base = s;

    while (MORE(s)) {
	if (*s == '\n')
	    break;
	s++;
    }
    return s - base;
}

/*
 * The only place that perlfilt.l recognizes a PATTERN is after "!~" or "=~". 
 * Doing that in other places gets complicated - the reason for moving to C.
 */
static int
begin_PATTERN(char *s)
{
    if (ATLEAST(s, 2)
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
    while (MORE(s) && isIdent(*s)) {
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

    DPRINTF(("\n*add_to_PATTERN(skip=%d, text=%.*s)\n", skip,
	     line_size(s), s));
    if (ATLEAST(s, need + skip)) {
	int delim = 0;
	int delim2 = 0;
	int ignored = 0;
	int escaped = 0;
	int comment = 0;
	int in_curlys = 0;
	int in_parens = 0;

	s += skip;
	--need;
	do {
	    while (MORE(s) && isspace(CharOf(*s))) {
		s++;
	    }
	    if (MORE(s)) {
		delim = *s++;
	    }
	    if ((delim2 = delim) == 0)
		return 0;
	    /* from Perl's S_scan_str() */
	    if (delim != 0 && (next = strchr(LOOKUP_TERM, delim)) != 0)
		delim2 = next[5];

	    if (delim == L_CURLY)
		in_curlys = 1;

	    if (delim == L_PAREN)
		in_parens = 1;

	    DPRINTF(("*start%d  %c%c\n->%.*s\n",
		     need, delim, delim2,
		     line_size(s), s));

	    next = s;
	    while (MORE(s)) {
		if (comment) {
		    if (*s == '\n')
			comment = 0;
		} else if (!escaped && (*s == BACKSLASH)) {
		    escaped = 1;
		} else {
		    if (!escaped) {

			switch (*s) {
			case L_CURLY:
			    ++in_curlys;
			    break;
			case R_CURLY:
			    if (--in_curlys < 0)
				in_curlys = 0;	/* oops */
			    break;
			case L_PAREN:
			    ++in_parens;
			    break;
			case R_PAREN:
			    if (--in_parens < 0)
				in_parens = 0;	/* oops */
			    break;
			}

			if (*s == '#' && delim == R_CURLY && !in_curlys)
			    comment = 1;

			ignored = 0;
			if (delim == L_CURLY && in_curlys)
			    ignored = 1;
			if (delim == L_PAREN && in_parens)
			    ignored = 1;

			if (!ignored && (*s == delim2)) {
			    DPRINTF(("*finish%d %c%c\n->%.*s\n",
				     need, delim, delim2,
				     line_size(s), s));
			    /*
			     * check for /pattern/.../pattern/
			     */
			    if (ATLEAST(s, 4)
				&& need == 1
				&& !strncmp(s + 1, "...", 3))
				need += 2;
			    /*
			     * check for /pattern/ {pattern}
			     */
			    if (delim != delim2 || need == 1)
				++s;
			    break;
			}
		    }
		    escaped = 0;
		}
		s++;
	    }
	} while (--need > 0);
	while (MORE(s)) {
	    if (!isalpha(CharOf(*s))
		|| *s == ';')
		break;
	    s++;
	}
	DPRINTF(("*finally\n->%.*s\n", line_size(s), s));
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

    DPRINTF(("\n*write_PATTERN(%.*s)\n", len, s));
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
	} else if (s[n] == BACKSLASH) {
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
	&& ATLEAST(s, 1)
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

static char *
put_newline(char *s)
{
    if (MORE(s))
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
	if ((j == 0 || (s[j - 1] != BACKSLASH))
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
    int ok = line_size(s);

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
static void
check_keyword(char *s, int ok, AfterKey * state)
{
    state->may_have_pattern = 0;
    state->has_no_pattern = 0;

    switch (ok) {
    case 2:
	state->may_have_pattern = !strncmp(s, "if", ok);
	break;
    case 3:
	state->has_no_pattern = !strncmp(s, "sub", ok);
	break;
    case 4:
	state->may_have_pattern = !strncmp(s, "grep", ok);
	break;
    case 5:
	state->may_have_pattern = (!strncmp(s, "split", ok)
				   || !strncmp(s, "until", ok)
				   || !strncmp(s, "while", ok));
	break;
    case 6:
	state->may_have_pattern = !strncmp(s, "unless", ok);
	break;
    }
}

/*
 * Identifier may be a keyword, or a user identifier.
 */
static char *
put_IDENT(char *s, int ok, int *had_op, AfterKey * if_wrd)
{
    char *attr = 0;
    char save = s[ok];

    *had_op = 0;
    s[ok] = '\0';
    attr = keyword_attr(s);
    s[ok] = save;
    flt_puts(s, ok, (attr != 0 && *attr != '\0') ? attr : Ident2_attr);
    check_keyword(s, ok, if_wrd);
    return s + ok;
}

/*
 * Identifier must be a user identifier.
 */
static char *
put_NOKEYWORD(char *s, int ok, int *had_op, AfterKey * if_wrd)
{
    char *attr = 0;
    char save = s[ok];

    *had_op = 0;
    s[ok] = '\0';
    attr = keyword_attr(s);
    s[ok] = save;
    flt_puts(s, ok, "");
    check_keyword(s, ok, if_wrd);
    return s + ok;
}

/*
 * Check for a matching backtic, return the number of characters we'll skip.
 */
static int
end_BACKTIC(char *s)
{
    char *base = s;
    int found = 0;

    while (MORE(s)) {
	if (*s == BQUOTE) {
	    found = 1;
	    break;
	}
	++s;
    }
    return found ? (s - base) : 0;
}

/*
 * Check for a file glob expression delimited by angle-brackets, e.g.,
 * 	<*>
 *	<*.c>
 */
static int
is_GLOB(char *s)
{
    char *base = s;
    int wild = 0;
    int both = 0;

    if (*s++ == L_ANGLE) {
	while (MORE(s)) {
	    int ch = *s++;
	    if (ch == '\n' || ch == '=' || ch == L_ANGLE) {
		break;
	    } else if (ch == '*') {
		++wild;
	    } else if (ch == R_ANGLE) {
		both = 1;
		break;
	    }
	}
    }
    return (wild && both) ? (s - base) : 0;
}

/*
 * Check for a "format =" or "format KEYWORD =" line, which begins a special
 * type of here-document.
 */
static int
is_FORMAT(char *s, int len)
{
    if (len == 6 && !strncmp(s, "format", len)) {
	s += len;
	s += is_BLANK(s);
	s += is_NAME(s);
	s += is_BLANK(s);
	if (*s == '=')
	    return 1;
    }
    return 0;
}

/******************************************************************************
 ******************************************************************************/

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input GCC_UNUSED)
{
    static unsigned used;
    static char *line;

    AfterKey if_old;
    AfterKey if_wrd;
    States state = eCODE;
    char *marker = 0;
    char *s;
    int err;
    int had_op = 0;
    int ignore;
    int in_line = -1;
    int in_stmt = 0;
    int ok;
    int parens = 0;
    int quoted = 0;
    int save;
    size_t request = 0;
    unsigned actual = 0;
    unsigned mark_len = 0;

    Action_attr = class_attr(NAME_ACTION);
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
	if_old = nullKey;
	if_wrd = nullKey;
	while (MORE(s)) {
	    if (*s == '\n' || s == the_file) {
		in_line = -1;
	    } else {
		in_line++;
	    }
	    if_old = if_wrd;
	    if_wrd = nullKey;
	    DPRINTF(("(%s(%c) line:%d.%d(%d) if:%d op:%d)\n",
		     stateName(state), *s, in_line, in_stmt, parens, if_wrd, had_op));
	    switch (state) {
	    case eBACKTIC:
		if ((ok = end_BACKTIC(s)) != 0) {
		    s = put_embedded(s, ok, "");
		} else {
		    s = put_remainder(s, Error_attr, 0);
		}
		if (*s == BQUOTE) {
		    flt_puts(s, 1, Action_attr);
		    state = eCODE;
		    ++s;
		}
		break;

	    case eCODE:
		if (*s == '\004' || *s == '\032') {	/* ^D or ^Z */
		    state = eIGNORED;
		    flt_puts(s, 1, Preproc_attr);
		    break;
		} else if (!isspace(CharOf(*s)))
		    ++in_stmt;

		if (*s == BACKSLASH && ATLEAST(s, 2) && s[1] == BQUOTE) {
		    flt_puts(s, 2, String_attr);
		    s += 2;
		} else if (*s == BQUOTE) {
		    flt_puts(s, 1, Action_attr);
		    ++s;
		    state = eBACKTIC;
		} else if ((ok = is_GLOB(s)) != 0) {
		    s = put_embedded(s, ok, String_attr);
		} else if ((marker = begin_HERE(s, &quoted)) != 0) {
		    state = eHERE;
		    s = put_remainder(s, String_attr, quoted);
		} else if ((ok = begin_PATTERN(s)) != 0) {
		    flt_puts(s, ok, "");
		    s += ok;
		    state = ePATTERN;
		} else if (in_line < 0
			   && (ok = begin_POD(s))) {
		    state = ePOD;
		    s = put_remainder(s + ok - 1, Comment_attr, 1);
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
		} else if ((if_old.may_have_pattern
			    || had_op
			    || (in_stmt == 1))
			   && isPattern(*s)) {
		    state = ePATTERN;
		} else if (*s == L_CURLY) {
		    had_op = 1;
		    flt_putc(*s++);
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
		    if ((s != the_file)
			&& (s[-1] == '*')) {	/* typeglob */
			s = put_IDENT(s, ok, &had_op, &if_wrd);
			break;
		    }
		    if ((s != the_file)
			&& (if_old.has_no_pattern)) {
			s = put_NOKEYWORD(s, ok, &had_op, &if_wrd);
			break;
		    }
		    if (is_QUOTE(s, &ignore)) {
			state = ePATTERN;
			break;
		    }
		    if (is_FORMAT(s, ok)) {
			quoted = 0;
			state = eHERE;
			mark_len = 0;
			marker = strcpy(do_alloc(0, 2, &mark_len), ".");
		    }
		    save = s[ok];
		    s[ok] = 0;
		    if (!strcmp(s, "__END__") || !strcmp(s, "__DATA__"))
			state = eIGNORED;
		    had_op = 0;
		    flt_puts(s, ok, keyword_attr(s));
		    check_keyword(s, ok, &if_wrd);
		    s[ok] = save;
		    s += ok;
		} else if ((ok = is_Option(s)) != 0) {
		    had_op = 0;
		    flt_puts(s, ok, Keyword_attr);
		    check_keyword(s, ok, &if_wrd);
		    s += ok;
		} else if ((ok = is_IDENT(s)) != 0) {
		    s = put_IDENT(s, ok, &had_op, &if_wrd);
		} else if ((ok = is_String(s, &err)) != 0) {
		    had_op = 0;
		    if (*s == DQUOTE) {
			s = put_embedded(s, ok, err ? Error_attr : String_attr);
		    } else {
			flt_puts(s, ok, err ? Error_attr : String_attr);
			s += ok;
		    }
		} else {
		    if (parens) {
			if (strchr("|&=~!", *s) != 0)
			    had_op = 1;
			else if (!isspace(CharOf(*s)))
			    had_op = 0;
		    } else {
			if (*s == ';')
			    in_stmt = 0;
			if (strchr("|&=~!", *s) != 0)
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
		if (MORE(s)) {
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
		}
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
