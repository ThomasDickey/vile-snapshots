/*
 * $Id: rakufilt.c,v 1.4 2025/01/26 10:56:05 tom Exp $
 *
 * Filter to add vile "attribution" sequences to raku (formerly perl6) scripts.
 * This is a clone of "pl-filt.c", to handle differences from perl5.
 */

#include <filters.h>

#ifdef DEBUG
DefineOptFilter(raku, "d");
#else
DefineFilter(raku);
#endif

/*
 * Punctuation that might be quote-delimiters (it is undocumented whether we
 * should consider '$' and '&', but reading the parser hints that might work
 * too).
 */
#define QUOTE_DELIMS "$+&#:/?|!:%',{}[]()@;=~\""
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

static char *put_embedded(char *, int, const char *);

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

/*
 * Count consecutive blanks at the current position.
 */
static int
count_blanks(char *s)
{
    char *base = s;

    while (MORE(s)) {
	if (!isspace(CharOf(*s))) {
	    break;
	}
	++s;
    }
    return (int) (s - base);
}

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
	found = (int) (s - base);
    }
    return found;
}

static int
is_KEYWORD(const char *s)
{
    const char *base = s;
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
    return (int) (s - base);
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
    return (int) (s - base);
}

/*
 * If double-quoted, look for variables after a "&", "$", "%" or "@".
 * If not double-quoted, look also for syntax such as $foo'bar
 */
static int
is_NORMALVARS(const char *s, int dquoted)
{
    const char *base = s;
    int ch;
    int squoted = 0;
    int part1 = 0;
    int part2 = 0;

    while (MORE(s)) {
	ch = CharOf(*s);
	if (s == base) {
	    if (vl_index(dquoted ? "$" : "&$%@", ch) == NULL) {
		break;
	    }
	} else if (squoted && !dquoted) {
	    if (isalnum(ch)) {
		part2 = 1;
	    } else {
		break;
	    }
	} else {
	    if (ch == SQUOTE && !dquoted) {
		squoted = 1;
	    } else if (isalnum(ch) || ch == '_') {
		part1 = 1;
	    } else if (ch == ':' && ATLEAST(s, 2) && s[1] == ':') {
		part1 = 1;
		s += 2;
	    } else {
		break;
	    }
	}
	s++;
    }
    return (part1 && (dquoted || (squoted == part2))) ? (int) (s - base) : 0;
}

static int
is_OTHERVARS(const char *s)
{
    const char *base = s;
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
	    } else if (strchr("-_./,\"\\#%=~|$?&`'+*[];!@<>():", ch) != NULL) {
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
    return (part1 || part2) ? (int) (s - base) : 0;
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
	} else if (value && (radix != 16) && (ch == 'e' || ch == 'E')) {
	    if (state >= 2 || !value) {
		*err = 1;
	    }
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

    return value ? (int) (s - base) : 0;
}

static char *
put_NUMBER(char *s, int ok, int *err)
{
    if (*err) {
	flt_error("illegal number");
	flt_puts(s, ok, Error_attr);
    } else {
	flt_puts(s, ok, Number_attr);
    }
    s += ok;
    return s;
}

static int
is_ELLIPSIS(char *s)
{
    int dots;
    int ok = 0;

    for (dots = 0; dots < 3; ++dots) {
	if (!MORE(s) || (*s++ != '.')) {
	    break;
	}
	ok = dots + 1;
    }
    return (ok > 1) ? ok : 0;
}

static int
is_IDENT(const char *s, int quoted)
{
    int found;

    if ((found = is_NORMALVARS(s, quoted)) == 0)
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
    return (int) (s - base);
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
    if (strncmp(s, "line", (size_t) 4))
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
    return (int) (s - base);
}

/******************************************************************************
 ******************************************************************************/

static int
end_marker(char *s, const char *marker, int only)
{
    size_t len = strlen(marker);

    return (ATLEAST(s, (int) len)
	    && !strncmp(s, marker, len)
	    && (!only || (s[len] == '\n')));
}

/*
 * Check for one or more blank lines followed by a line beginning with an "="
 * and an alpha-character.  If found, return the length skipped through the
 * "=" (the "=" is painted in a different color).  Otherwise return 0.
 */
static int
begin_POD(char *s, int emit)
{
    int result = 0;
    char *base;
    int skip = 2;
    int maybe = 0;

    while (s > the_file) {
	if (s[0] != '\n' ||
	    s[-1] != '\n' ||
	    --skip <= 0) {
	    break;
	}
	--s;
    }
    skip = 0;
    base = s;

    while (MORE(s)) {
	if (*s == '\n')
	    ++skip;
	else
	    break;
	++s;
    }
    if ((base != the_file) && (skip == 1)) {
	maybe = 1;
	++skip;
    }
    if (((base == the_file) || (skip >= 2))
	&& ATLEAST(s, 2)
	&& s[0] == '='
	&& isalpha(CharOf(s[1]))) {
	result = (int) (s + 1 - base);
	if (result && maybe)
	    flt_error("expected a blank line");
	if (emit) {
	    while (base != s) {
		flt_putc(*base++);	/* skip the newlines */
	    }
	}
    }
    return result;
}

static int
end_POD(char *s, int skip)
{
    if (skip) {
	while (MORE(s)) {
	    if (*s != '\n')
		break;
	    ++s;
	}
    }
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
	s += count_blanks(s);
	base = s;
	if ((ok = is_QIDENT(s)) != 0) {
	    size_t temp = 0;
	    char *marker = do_alloc((char *) 0, (size_t) (ok + 1), &temp);
	    char *d = marker;

	    s += ok;
	    *quoted = 0;

	    if (marker != NULL) {
		while (base != s) {
		    if (*base != SQUOTE
			&& *base != DQUOTE
			&& *base != BACKSLASH)
			*d++ = *base;
		    else if (*base != DQUOTE)
			*quoted = 1;
		    base++;
		}
		*d = 0;
	    }
	    return marker;
	}
    }
    return NULL;
}

static char *
skip_BLANKS(char *s)
{
    int count = count_blanks(s);

    if (count) {
	flt_puts(s, count, "");
    }
    return s + count;
}

/*
 * Return the first character, skipping optional blanks
 */
static int
char_after_blanks(char *s)
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
    return (int) (s - base);
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
 * Check for simple pattern, trying to distinguish '/' from its use for divide,
 * and '$' from its use in variable names.
 */
static int
is_PATTERN(char *s)
{
    char *base = s;
    int delim = *s++;
    int result = 0;
    int escaped = 0;
    char *content = s;

    while (MORE(s)) {
	int ch = *s++;
	if (escaped) {
	    escaped = 0;
	} else if (ch == BACKSLASH) {
	    escaped = 1;
	} else if (ch == delim) {
	    if (((s - 1) - content) != 0) {
		/* content is nonempty */
		char *next = NULL;
		(void) strtol(content, &next, 0);
		if (next == NULL || next == content) {
		    /* content was not a number */
		    result = (int) (s - base);
		}
	    }
	    break;
	} else if (isspace(ch)) {
	    break;
	} else if (ch == '$') {
	    if (*s != delim)
		break;
	}
    }
    return result;
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
    int result = 0;

    *delims = 0;
    while (MORE(s) && isIdent(*s)) {
	++s;
    }
    if ((len = (size_t) (s - base)) != 0) {
	switch (len) {
	case 1:
	    if (*base == 'm' || *base == 'q') {
		*delims = 2;
	    } else if (*base == 's' || *base == 'y') {
		*delims = 3;
	    }
	    break;
	case 2:
	    if (!strncmp(base, "tr", (size_t) 2)) {
		*delims = 3;
	    } else if (!strncmp(base, "qq", (size_t) 2) ||
		       !strncmp(base, "qx", (size_t) 2) ||
		       !strncmp(base, "qw", (size_t) 2) ||
		       !strncmp(base, "qr", (size_t) 2)) {
		*delims = 2;
	    }
	    break;
	}
    }
    if (*delims == 0)
	s = base;

    if ((result = (int) (s - base)) != 0) {
	int test = char_after_blanks(s);
	DPRINTF(("is_Quote(%.*s:%c)", result, base, test));
	if (test == '#' && isspace(CharOf(*s)))
	    test = 0;
	if ((test == 0) || (strchr(QUOTE_DELIMS "<>", test) == NULL)) {
	    s = base;
	    result = 0;
	}
	DPRINTF(("is_QUOTE(%.*s)\n",
		 result ? result : 1,
		 result ? base : ""));
    }
    return result;
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
	int in_blocks = 0;
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
	    if (delim != 0 && (next = strchr(LOOKUP_TERM, delim)) != NULL)
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
			ignored = 0;

			switch (*s) {
			case L_BLOCK:
			    if (in_blocks == 0)
				in_blocks = 1;
			    break;
			case R_BLOCK:
			    if (in_blocks != 0) {
				in_blocks = 0;
				ignored = 1;
			    }
			    break;
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

			if (delim == L_BLOCK && in_blocks)
			    ignored = 1;
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
				&& !strncmp(s + 1, "...", (size_t) 3))
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
	return (int) (s - base);
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
    int interp = 0;
    int len_flags = 0;
    int e_modifier = 0;

    DPRINTF(("\n*write_PATTERN(%.*s)\n", len, s));
    if (skip) {
	flt_puts(s, skip, Keyword_attr);
	s += skip;
	len -= skip;
    }

    skip = (int) (skip_BLANKS(s) - s);
    if (skip) {
	s += skip;
	len -= skip;
    }
    delimiter = *s;

    for (len_flags = 0; len_flags < (len - 2); ++len_flags) {
	int ch = CharOf(s[len - len_flags - 1]);
	if (ch == 'e') {
	    e_modifier = 1;
	} else if (!isalpha(ch))
	    break;
    }
    interp = (delimiter == '"');

    for (n = first = 0; n < len; n++) {
	if (!escaped
	    && !interp
	    && e_modifier
	    && (n > 0)
	    && (s[n] == delimiter)) {
	    if (comment) {
		flt_puts(s + first, (n - first + 1), Comment_attr);
		comment = 0;
	    } else {
		flt_puts(s + first, (n - first + 1), String_attr);
	    }
	    first = n + 1;
	    interp = 1;
	    continue;
	}

	if (escaped) {
	    escaped = 0;
	} else if (s[n] == BACKSLASH) {
	    escaped = 1;
	} else if (isBlank(s[n]) && !escaped && !comment &&
		   (leading || char_after_blanks(s + n) == '#')) {
	    if (interp) {
		put_embedded(s + first, (n - first - 0), String_attr);
	    } else {
		flt_puts(s + first, (n - first - 0), String_attr);
	    }
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
		    if (interp) {
			put_embedded(s + first, (n - first - 1), String_attr);
		    } else {
			flt_puts(s + first, (n - first - 1), String_attr);
		    }
		    comment = 1;
		    first = n + 0;
		}
	    }
	}
    }
    if (comment)
	first += (int) (skip_BLANKS(s) - s);
    if (comment) {
	flt_puts(s + first, (len - first), Comment_attr);
    } else {
	if (interp) {
	    put_embedded(s + first, (len - first - len_flags), String_attr);
	} else {
	    flt_puts(s + first, (len - first - len_flags), String_attr);
	}
	flt_puts(s + len - len_flags, len_flags, Keyword_attr);
    }
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

/*
 * FIXME: this only handles subscripts that consist of a single identifier,
 * with no expressions.
 */
static int
is_Subscript(char *s, int len, char *delimp)
{
    if (delimp != NULL) {
	int delim = *delimp;
	int delim2;
	char *next;

	if (delim != 0 && (next = strchr(LOOKUP_TERM, delim)) != NULL) {
	    delim2 = next[5];

	    if (s[len] == delim2)
		return 1;
	}
    }
    return 0;
}

static char *
put_newline(char *s)
{
    if (MORE(s))
	flt_putc(*s++);
    return s;
}

static int
var_embedded(const char *s)
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
put_embedded(char *s, int len, const char *attr)
{
    int id;
    int j, k;

    for (j = k = 0; j < len; j++) {
	if ((j == 0 || (s[j - 1] != BACKSLASH))
	    && (id = is_IDENT(s + j, 1)) != 0) {
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
 * Write the line for perldoc (see perlpodspec).
 */
static char *
put_document(char *s)
{
    char *attr = Comment_attr;
    int ok = line_size(s);
    int j, k;

    j = 0;
    if (s[j] == '=') {
	flt_puts(s, j, attr);
	for (k = j; k < ok; ++k) {
	    if (isspace(CharOf(s[k])))
		break;
	}
	flt_puts(s + j, k - j, Preproc_attr);
	flt_puts(s + k, ok - k, String_attr);
	j = ok;
    }
    flt_puts(s + j, ok - j, attr);
    return put_newline(s + ok);
}

/*
 * Check for special cases of keywords after which we may expect a pattern
 * that does not have to be inside parentheses.
 */
static void
check_keyword(char *s, int ok, AfterKey * state)
{
    size_t len = (size_t) ok;

    state->may_have_pattern = 0;
    state->has_no_pattern = 0;

    switch (ok) {
    case 2:
	state->may_have_pattern = (!strncmp(s, "if", len)
				   || !strncmp(s, "eq", len)
				   || !strncmp(s, "ge", len)
				   || !strncmp(s, "gt", len)
				   || !strncmp(s, "le", len)
				   || !strncmp(s, "lt", len)
				   || !strncmp(s, "ne", len)
				   || !strncmp(s, "or", len));
	break;
    case 3:
	state->has_no_pattern = !strncmp(s, "sub", len);
	state->may_have_pattern = (!strncmp(s, "and", len)
				   || !strncmp(s, "cmp", len)
				   || !strncmp(s, "not", len)
				   || !strncmp(s, "xor", len));
	break;
    case 4:
	state->may_have_pattern = !strncmp(s, "grep", len);
	break;
    case 5:
	state->may_have_pattern = (!strncmp(s, "split", len)
				   || !strncmp(s, "until", len)
				   || !strncmp(s, "while", len));
	break;
    case 6:
	state->may_have_pattern = !strncmp(s, "unless", len);
	break;
    }
}

/*
 * Identifier may be a keyword, or a user identifier.
 */
static char *
put_IDENT(char *s, int ok, AfterKey * if_wrd)
{
    const char *attr = NULL;
    char save = s[ok];

    s[ok] = '\0';
    attr = get_keyword_attr(s);
    s[ok] = save;
    flt_puts(s, ok, (attr != NULL && *attr != '\0') ? attr : Ident2_attr);
    check_keyword(s, ok, if_wrd);
    return s + ok;
}

/*
 * Identifier must be a user identifier.
 */
static char *
put_NOKEYWORD(char *s, int ok, AfterKey * if_wrd)
{
    const char *attr = NULL;
    char save = s[ok];

    s[ok] = '\0';
    attr = get_keyword_attr(s);
    s[ok] = save;
    flt_puts(s, ok, attr);
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
    return found ? (int) (s - base) : 0;
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
    return (wild && both) ? (int) (s - base) : 0;
}

/*
 * Check for a "format =" or "format KEYWORD =" line, which begins a special
 * type of here-document.
 */
static int
is_FORMAT(char *s, int len)
{
    if (len == 6 && !strncmp(s, "format", (size_t) len)) {
	s += len;
	s += is_BLANK(s);
	s += is_NAME(s);
	s += is_BLANK(s);
	if (*s == '=' && !ispunct(CharOf(s[1])))
	    return 1;
    }
    return 0;
}

/******************************************************************************
 ******************************************************************************/

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
}

#define opRightArrow() (had_op != NULL && old_op == had_op - 1 && *old_op == '-' && *had_op == '>')
#define opBeforePattern() (had_op != NULL && strchr("{(|&=~!", *had_op) != NULL)

#define saveOp(p) { DPRINTF(("\nsaveOp('%c') @%d\n", *p, __LINE__)); old_op = had_op; had_op = p; }
#define clearOp() { DPRINTF(("\nclearOp @%d\n", __LINE__)); old_op = had_op = NULL; }

static void
do_filter(FILE *input GCC_UNUSED)
{
    static size_t used;
    static char *line;

    AfterKey if_old;
    AfterKey if_wrd;
    States state = eCODE;
    char *marker = NULL;
    char *s;
    int err;
    char *had_op = NULL;
    char *old_op = NULL;
    int ignore;
    int in_line = -1;
    int in_stmt = 0;
    int ok;
    int parens = 0;
    int quoted = 0;
    int save;
    size_t request = 0;
    size_t actual = 0;
    size_t mark_len = 0;

    (void) input;

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
    the_file = NULL;
    while (flt_gets(&line, &used) != NULL) {
	size_t len = strlen(line);	/* FIXME: nulls? */
	if (len != 0 && line[len - 1] == '\r')	/* FIXME: move this to readline */
	    line[--len] = '\0';
	if ((request = the_size + len + 1) > actual)
	    request = 1024 + (request * 2);
	the_file = do_alloc(the_file, request, &actual);
	if (the_file == NULL)
	    break;
	memcpy(the_file + the_size, line, len + 1);
	the_size += len;
    }

    if (the_file != NULL) {
	the_last = the_file + the_size;

	s = the_file;
	if_wrd = nullKey;
	while (MORE(s)) {
	    if (*s == '\n' || s == the_file) {
		in_line = -1;
	    } else {
		in_line++;
	    }
	    if_old = if_wrd;
	    DPRINTF(("(%s(%c) line:%d.%d(%d) if:%d.%d op:%c%c)%s\n",
		     stateName(state), *s, in_line, in_stmt, parens,
		     if_wrd.may_have_pattern,
		     if_wrd.has_no_pattern,
		     old_op ? *old_op : ' ',
		     had_op ? *had_op : ' ',
		     opRightArrow()? " arrow" : ""));
	    switch (state) {
	    case eBACKTIC:
		if ((ok = end_BACKTIC(s)) != 0) {
		    s = put_embedded(s, ok, "");
		} else {
		    flt_error("missing backtic");
		    s = put_remainder(s, Error_attr, 0);
		}
		if (*s == BQUOTE) {
		    flt_puts(s, 1, Action_attr);
		    state = eCODE;
		    ++s;
		}
		if_wrd = nullKey;
		break;

	    case eCODE:
		if (*s == '\004' || *s == '\032') {	/* ^D or ^Z */
		    state = eIGNORED;
		    flt_puts(s, 1, Preproc_attr);
		    break;
		} else if (!isspace(CharOf(*s))) {
		    ++in_stmt;
		}

		if (*s == BACKSLASH && ATLEAST(s, 2) && s[1] == BQUOTE) {
		    flt_puts(s, 2, String_attr);
		    s += 2;
		    if_wrd = nullKey;
		} else if (*s == BQUOTE) {
		    flt_puts(s, 1, Action_attr);
		    ++s;
		    state = eBACKTIC;
		    if_wrd = nullKey;
		} else if ((ok = is_GLOB(s)) != 0) {
		    s = put_embedded(s, ok, String_attr);
		    if_wrd = nullKey;
		} else if ((marker = begin_HERE(s, &quoted)) != NULL) {
		    state = eHERE;
		    s = put_remainder(s, String_attr, quoted);
		    if_wrd = nullKey;
		} else if ((ok = begin_PATTERN(s)) != 0) {
		    flt_puts(s, ok, "");
		    s += ok;
		    DPRINTF(("\nePATTERN:%d\n", __LINE__));
		    state = ePATTERN;
		    if_wrd = nullKey;
		} else if (in_line <= 0
			   && (ok = begin_POD(s, 0))) {
		    char *base = s;
		    state = ePOD;
		    while (*s != '=')
			flt_putc(*s++);
		    s = put_document(s);
		    if (end_POD(base, 1))
			state = eCODE;
		} else if (in_line == 0
			   && (ok = is_PREPROC(s)) != 0) {
		    flt_puts(s, ok, Preproc_attr);
		    s += ok;
		} else if ((ok = is_COMMENT(s)) != 0) {
		    int skip = (int) (skip_BLANKS(s) - s);
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
			    || opBeforePattern()
			    || (in_stmt == 1))
			   && isPattern(*s)) {
		    DPRINTF(("\nePATTERN:%d\n", __LINE__));
		    state = ePATTERN;
		} else if (*s == L_CURLY) {
		    saveOp(s);
		    flt_putc(*s++);
		    if_wrd = nullKey;
		} else if (*s == L_PAREN) {
		    parens++;
		    saveOp(s);
		    flt_putc(*s++);
		    if (isPattern(*s)) {
			DPRINTF(("\nePATTERN:%d\n", __LINE__));
			state = ePATTERN;
		    }
		    if_wrd = nullKey;
		} else if (*s == R_PAREN) {
		    if (--parens < 0)
			parens = 0;
		    flt_putc(*s++);
		    clearOp();
		    if_wrd = nullKey;
		} else if ((ok = is_ELLIPSIS(s)) != 0) {
		    flt_puts(s, ok, "");
		    s += ok;
		    if_wrd = nullKey;
		} else if ((ok = is_NUMBER(s, &err)) != 0) {
		    clearOp();
		    s = put_NUMBER(s, ok, &err);
		    if_wrd = nullKey;
		} else if ((ok = is_KEYWORD(s)) != 0) {
		    if ((s != the_file)
			&& (s[-1] == '*')) {	/* typeglob */
			s = put_IDENT(s, ok, &if_wrd);
			clearOp();
		    } else if ((s != the_file)
			       && (if_old.has_no_pattern)) {
			s = put_NOKEYWORD(s, ok, &if_wrd);
			clearOp();
		    } else if (!opRightArrow()
			       && !is_Subscript(s, ok, had_op)
			       && is_QUOTE(s, &ignore)) {
			DPRINTF(("\nePATTERN:%d\n", __LINE__));
			state = ePATTERN;
		    } else {
			if (is_FORMAT(s, ok)) {
			    char *try_mark;
			    quoted = 0;
			    state = eHERE;
			    mark_len = 0;
			    try_mark = do_alloc(NULL, (size_t) 2, &mark_len);
			    if (try_mark != NULL) {
				free(marker);
				marker = strcpy(try_mark, ".");
			    }
			}
			save = s[ok];
			s[ok] = 0;
			if (!strcmp(s, "__END__") || !strcmp(s, "__DATA__"))
			    state = eIGNORED;
			clearOp();
			flt_puts(s, ok, get_keyword_attr(s));
			check_keyword(s, ok, &if_wrd);
			s[ok] = (char) save;
			s += ok;
		    }
		} else if ((ok = is_Option(s)) != 0) {
		    clearOp();
		    flt_puts(s, ok, Keyword_attr);
		    check_keyword(s, ok, &if_wrd);
		    s += ok;
		} else if ((ok = is_IDENT(s, 0)) != 0) {
		    s = put_IDENT(s, ok, &if_wrd);
		    clearOp();
		} else if ((ok = is_String(s, &err)) != 0) {
		    clearOp();
		    if (*s == DQUOTE) {
			if (err) {
			    flt_error("unexpected quote");
			    s = put_embedded(s, ok, Error_attr);
			} else {
			    s = put_embedded(s, ok, String_attr);
			}
		    } else {
			if (err) {
			    flt_error("missing quote");
			    flt_puts(s, ok, Error_attr);
			} else {
			    flt_puts(s, ok, String_attr);
			}
			s += ok;
		    }
		    if_wrd = nullKey;
		} else if (isPattern(*s) && (ok = is_PATTERN(s))) {
		    s = write_PATTERN(s, ok);
		    if_wrd = nullKey;
		} else {
		    if (!isspace(CharOf(*s))) {
			if_wrd = nullKey;
		    }
		    if (*s == ';') {
			in_stmt = 0;
		    }
		    if (parens) {
			if (strchr("|&=~!->", *s) != NULL) {
			    saveOp(s);
			} else if (!isspace(CharOf(*s))) {
			    clearOp();
			}
		    } else {
			if (strchr("|&=~!->", *s) != NULL) {
			    saveOp(s);
			}
		    }
		    flt_putc(*s++);
		}
		break;

	    case eHERE:
		if (end_marker(s, marker, 1)) {
		    state = eCODE;
		    free(marker);
		    marker = NULL;
		}
		s = put_remainder(s, String_attr, quoted);
		if_wrd = nullKey;
		break;

	    case eIGNORED:
		s = put_document(s);
		break;

	    case ePATTERN:
		s = skip_BLANKS(s);
		if (MORE(s)) {
		    if ((ok = is_NAME(s)) != 0 && !is_QUOTE(s, &ignore)) {
			s = put_IDENT(s, ok, &if_wrd);
			clearOp();
		    } else if ((ok = is_IDENT(s, 0)) != 0 && !is_QUOTE(s, &ignore)) {
			s = put_IDENT(s, ok, &if_wrd);
			clearOp();
		    } else if ((ok = add_to_PATTERN(s)) != 0) {
			s = write_PATTERN(s, ok);
		    } else {
			flt_putc(*s++);
		    }
		    clearOp();
		    state = eCODE;
		}
		if_wrd = nullKey;
		break;

	    case ePOD:
		if (end_POD(s, 0))
		    state = eCODE;
		s = put_document(s);
		if (begin_POD(s - 1, 0)) {
		    state = ePOD;
		}
		break;
	    }
	}
	free(the_file);
    }
    free(marker);
}

#if NO_LEAKS
static void
free_filter(void)
{
}
#endif
