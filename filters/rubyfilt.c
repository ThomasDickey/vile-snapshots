/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/rubyfilt.c,v 1.9 2003/02/10 01:07:58 tom Exp $
 *
 * Filter to add vile "attribution" sequences to ruby scripts.  This is a
 * translation into C of an earlier version written for LEX/FLEX.
 *
 * Although the documentation says it has simpler syntax, Ruby borrows from
 * Perl the worst of its syntax, i.e., regular expressions which can be split
 * across lines, have embedded comments.
 */

#include <filters.h>

#ifdef DEBUG
DefineOptFilter("ruby", "d");
#else
DefineFilter("ruby");
#endif

#define isIdent(c)   (isalnum(CharOf(c)) || c == '_')

#ifdef DEBUG
#define DPRINTF(params) if(flt_options['d'])printf params
#else
#define DPRINTF(params)		/*nothing */
#endif

typedef enum {
    eCODE
    ,eHERE
    ,ePOD
} States;

static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *String_attr;
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
    case ePOD:
	result = "POD";
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
is_ESCAPED(char *s)
{
    int found = 0;
    if (*s == BACKSLASH) {
	found = 1;
    }
    return found;
}

static int
is_STRINGS(char *s, int *err, int left_delim, int right_delim)
{
    char *base = s;
    int found = 0;
    int escape = 0;
    int level = 0;

    *err = 0;
    if (*s == left_delim) {	/* should always be true */
	s++;
	if (left_delim != right_delim)
	    ++level;

	for (;;) {
	    if (s >= the_last) {
		*err = 1;	/* unterminated string */
		break;
	    }
	    if (!escape && (*s == BACKSLASH)) {
		escape = 1;
	    } else if (escape) {
		escape = 0;
	    } else {
		if (left_delim != right_delim) {
		    if (*s == left_delim) {
			++level;
		    } else if (*s == right_delim) {
			if (--level > 0) {
			    ++s;
			    continue;
			}
			/* otherwise, fallthru to the other right_delim check */
		    }
		}
		if (*s == right_delim) {
		    s++;
		    break;
		}
	    }
	    s++;
	}
	found = s - base;
	DPRINTF(("...found(%d)\n", found));
    }
    return found;
}

/*
 * pattern: ({SSTRING}|{DSTRING}|{KEYWORD}|"`"{KEYWORD}"`")
 */
static int
is_QIDENT(char *s)
{
    char *base = s;
    int ch;

    while (s != the_last) {
	ch = CharOf(*s);
	if (isIdent(ch)
	    || ch == BQUOTE
	    || ch == SQUOTE
	    || ch == DQUOTE)
	    s++;
	else
	    break;
    }
    return s - base;
}

/*
 * pattern: {KEYWORD}
 * returns: length of the token
 */
static int
is_KEYWORD(char *s)
{
    int found = 0;

    if (!isdigit(CharOf(s[0]))) {
	while ((s + found < the_last) && isIdent(s[found])) {
	    ++found;
	}
    }
    return found;
}

/*
 * pattern: [$]([-_.\/,"\\#%=~|\$?&`'+*\[\];!@<>():]|{CONST}|{VAR})
 * returns: length of the token, or zero.
 */
static int
is_GLOBAL(char *s)
{
    int found = 0;

    if (*s == '$' && (++s < the_last)) {
	if (*s != '\0' && strchr("-_./,\"\\#%=~|$?&`'+*[];!@<>():", *s)) {
	    found = 1;
	} else {
	    found = is_KEYWORD(s);
	}
	if (found != 0)
	    ++found;
    }
    return found;
}

/*
 * pattern: [@]+{KEYWORD}
 * returns: length of the token, or zero.
 */
static int
is_INSTANCE(char *s)
{
    char *base = s;
    int found = 0;

    if (*s == '@') {
	while ((*s++ == '@') && (s < the_last)) {
	    ;
	}
	if ((found = is_KEYWORD(s)) != 0)
	    found += (s - base);
    }
    return found;
}

/*
 * pattern: ({INSTANCE}|{GLOBAL})
 * returns: length of the token, or zero.
 */
static int
is_VARIABLE(char *s)
{
    int found = 0;

    if (*s == '$') {
	found = is_GLOBAL(s);
    } else if (*s == '@') {
	found = is_INSTANCE(s);
    }
    return found;
}

/*
 * pattern: \?(\\M-)?(\\C-)?.
 */
static int
is_CHAR(char *s, int *err)
{
    int found = 0;

    if (*s == '?' && (s + 5 < the_last)) {
	if ((*++s == BACKSLASH)
	    && ((*++s == 'M') || (*s == 'C'))
	    && (s[1] == '-')) {
	    *err = 0;
	    found = 5;
	    if (*s == 'M' && (s + 5 < the_last)
		&& s[2] == BACKSLASH
		&& s[3] == 'C'
		&& s[4] == '-') {
		found += 3;
	    }
	}
    }
    return found;
}

/*
 * lex patterns:
 *	SIGN		[-+]
 *	DECIMAL		[0-9_]+
 *	OCTAL		0[0-7_]+
 *	HEXADECIMAL	0x[0-9a-fA-F_]+
 *	REAL		[-+]?([0-9_]*\.[0-9][0-9_]*)([eE][+-]?[0-9_]+)?
 *	NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL})[L]?|{REAL}
 */
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
		|| ((radix == 8) && (ch >= '0' && ch < '8'))) {
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

/*
 * pattern: ^([#].*[\n])+
 * returns: length of the comment, or zero
 */
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

/******************************************************************************
 ******************************************************************************/

/*
 * Match a given 'marker' keyword, optionally checking if it is alone on the
 * line.  Documentation implies otherwise, but parser does check for a blank
 * after the keyword.
 */
static int
end_marker(char *s, char *marker, int only)
{
    int len = strlen(marker);

    return (the_last - s > len
	    && !strncmp(s, marker, len)
	    && isspace(s[len])
	    && (!only || (s[len] == '\n')));
}

/*
 * pattern: "^=begin.*"
 */
static int
begin_POD(char *s)
{
    return end_marker(s, "=begin", 0);
}

/*
 * pattern: "^=end.*"
 */
static int
end_POD(char *s)
{
    return end_marker(s, "=end", 0);
}

/*
 * FIXME: we're only implementing a single here-doc (documentation says they
 * can be stacked), and don't check for unbalanced quotes).
 */
static char *
begin_HERE(char *s, int *quoted, int *strip_here)
{
    char *base;
    int ok;
    int strip = 0;

    if (the_last - s > 3
	&& s[0] == '<'
	&& s[1] == '<'
	&& !isBlank(s[2])) {
	s += 2;
	base = s;
	if (*s == '-') {
	    strip = 1;
	    ++s;
	}
	if ((ok = is_QIDENT(s)) != 0) {
	    unsigned temp = 0;
	    char *marker = do_alloc((char *) 0, ok + 1, &temp);
	    char *d = marker;
	    s += ok;
	    *quoted = 0;
	    *strip_here = 1;
	    while (base != s) {
		if (isIdent(*base))
		    *d++ = *base;
		else
		    *quoted = (*base != DQUOTE);
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
 * Do the real work for is_Regexp().  Documentation is vague, but the parser
 * appears to use the same quoting type for each nesting level.
 */
static int
is_REGEXP(char *s, int left_delim, int right_delim)
{
    char *base = s;
    int found = 0;
    int len;
    int level = 0;

    while (s < the_last) {
	if (left_delim != right_delim) {
	    if (*s == left_delim) {
		++level;
	    } else if (*s == right_delim) {
		if (--level > 0) {
		    ++s;
		    continue;
		}
		/* otherwise, fallthru to the other right_delim check */
	    }
	}
	if (s != base && *s == right_delim) {
	    ++s;
	    while (s < the_last && isalpha(*s)) {
		++s;
	    }
	    found = s - base;
	    break;
	} else if ((len = is_VARIABLE(s)) != 0) {
	    s += len;
	} else if ((len = is_ESCAPED(s)) != 0) {
	    s += len;
	} else {
	    ++s;
	}
    }
    return found;
}

static int
balanced_delimiter(char *s)
{
    int result;

    switch (*s) {
    case L_CURLY:
	result = R_CURLY;
	break;
    case L_PAREN:
	result = R_PAREN;
	break;
    case L_BLOCK:
	result = R_BLOCK;
	break;
    case L_ANGLE:
	result = R_ANGLE;
	break;
    default:
	result = *s;
	break;
    }
    return result;
}

/*
 * Check for a regular expression, returning its length
 */
static int
is_Regexp(char *s, int *delim)
{
    int found = 0;

    if (*s == '/') {
	*delim = balanced_delimiter(s);
	found = is_REGEXP(s, *s, *delim);
    } else if (s + 4 < the_last
	       && s[0] == '%'
	       && s[1] == 'r'
	       && !isalnum(s[2])) {

	*delim = balanced_delimiter(s + 2);
	found = 2 + is_REGEXP(s + 2, s[2], *delim);
    }
    return found;
}

/*
 * Parse the various types of quoted strings.
 */
static int
is_String(char *s, int *err)
{
    int found = 0;

    if (s + 2 < the_last) {
	switch (*s) {
	case '%':
	    if (s + 4 < the_last) {
		switch (s[1]) {
		case 'q':
		case 'Q':
		    found = is_STRINGS(s + 2,
				       err,
				       s[2],
				       balanced_delimiter(s + 2));
		    if (found != 0) {
			found += 2;
		    }
		    break;
		}
	    }
	    break;
	case BQUOTE:
	case SQUOTE:
	case DQUOTE:
	    found = is_STRINGS(s, err, *s, *s);
	    break;
	case BACKSLASH:
	    found = is_ESCAPED(s);
	    break;
	}
    }
    return found;
}

static int
line_size(char *s)
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

/*
 * pattern: #(\{[^}]*\}|{IDENT})
 * returns: length of the token
 */
static int
var_embedded(char *s)
{
    int found = 0;

    if (*s == '#' && (++s < the_last)) {
	if (*s == L_CURLY) {
	    for (found = 1; s + found < the_last; ++found) {
		/* FIXME: this check for %q{xxx} is too naive... */
		if (s[found] == '%' && (strchr("qQ", s[found + 1]) != 0)) {
		    int ignore;
		    found += is_String(s + found, &ignore);
		} else if (s[found] == R_CURLY) {
		    ++found;
		    break;
		}
	    }
	} else {
	    found = is_VARIABLE(s);
	}
    }
    return found ? found + 1 : 0;
}

static char *
put_embedded(char *s, int len, char *attr)
{
    int id;
    int j, k;

    for (j = k = 0; j < len; j++) {
	if ((j == 0 || (s[j - 1] != BACKSLASH))
	    && (id = var_embedded(s + j)) != 0) {
	    if (j != k)
		flt_puts(s + k, j - k, attr);
	    flt_puts(s + j, id, Ident2_attr);
	    k = j + id;
	    j = k - 1;
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

static char *
put_COMMENT(char *s, int ok)
{
    int skip = (skip_BLANKS(s) - s);
    ok -= skip;
    s += skip;
    flt_puts(s, ok, Comment_attr);
    return s + ok;
}

static char *
put_KEYWORD(char *s, int ok, int *had_op)
{
    char *attr = 0;
    char save = s[ok];

    s[ok] = '\0';
    attr = keyword_attr(s);
    s[ok] = save;
    flt_puts(s, ok, attr);
    *had_op = (attr == Keyword_attr);
    return s + ok;
}

static char *
put_VARIABLE(char *s, int ok)
{
    char *attr = 0;
    char save = s[ok];

    s[ok] = '\0';
    attr = keyword_attr(s);
    s[ok] = save;
    flt_puts(s, ok, (attr != 0 && *attr != '\0') ? attr : Ident2_attr);
    return s + ok;
}

static char *
put_REGEXP(char *s, int length, int delim)
{
    char *base = s;
    char *last;
    int len;

    if (*s == '%') {
	flt_puts(s, 2, Keyword_attr);
	s += 2;
    }
    flt_bfr_begin(String_attr);
    while (s < base + length) {
	if (s != base && *s == delim) {
	    flt_bfr_append(s, 1);
	    last = ++s;
	    while (s < the_last && isalpha(*s)) {
		++s;
	    }
	    flt_bfr_embed(s, s - last, Keyword_attr);
	    break;
	} else if ((len = var_embedded(s)) != 0) {
	    flt_bfr_embed(s, len, Ident2_attr);
	    s += len;
	} else if ((len = is_ESCAPED(s)) != 0) {
	    flt_bfr_append(s, len);
	    s += len;
	} else {
	    flt_bfr_append(s, 1);
	    ++s;
	}
    }
    flt_bfr_finish();
    return s;
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

    unsigned actual = 0;
    size_t request = 0;
    States state = eCODE;
    char *s;
    char *marker = 0;
    int in_line = -1;
    int ok;
    int err;
    int delim;
    int strip_here = 0;		/* strip leading blanks in here-document */
    int quote_here = 0;		/* tokens in here-document are quoted */
    int had_op = 0;

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    /*
     * Read the whole file into a single string, in-memory.  Rather than
     * spend time working around the various continuation-line types _and_
     * the regular expresion "syntax", let's just concentrate on the latter.
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
	while (s != the_last) {
	    if (*s == '\n') {
		in_line = -1;
	    } else {
		in_line++;
	    }
	    DPRINTF(("(%s(%.*s) line:%d op:%d)\n",
		     stateName(state), is_KEYWORD(s) + 1, s, in_line, had_op));
	    switch (state) {
	    case eCODE:
		if ((marker = begin_HERE(s, &quote_here, &strip_here)) != 0) {
		    state = eHERE;
		    s = put_remainder(s, String_attr, quote_here);
		} else if ((in_line < 0 && begin_POD(s + 1))
			   || ((s == the_file && begin_POD(s)))) {
		    DPRINTF(("...POD\n"));
		    state = ePOD;
		    flt_putc(*s++);	/* write the newline */
		    s = put_remainder(s, Comment_attr, 1);
		} else if ((ok = is_COMMENT(s)) != 0) {
		    DPRINTF(("...COMMENT: %d\n", ok));
		    s = put_COMMENT(s, ok);
		} else if ((ok = is_BLANK(s)) != 0) {
		    DPRINTF(("...BLANK: %d\n", ok));
		    flt_puts(s, ok, "");
		    s += ok;
		} else if (had_op && (ok = is_Regexp(s, &delim)) != 0) {
		    DPRINTF(("...REGEXP: %d\n", ok));
		    s = put_REGEXP(s, ok, delim);
		} else if ((ok = is_CHAR(s, &err)) != 0) {
		    DPRINTF(("...CHAR: %d\n", ok));
		    had_op = 0;
		    flt_puts(s, ok, err ? Error_attr : Number_attr);
		    s += ok;
		} else if ((ok = is_NUMBER(s, &err)) != 0) {
		    DPRINTF(("...NUMBER: %d\n", ok));
		    had_op = 0;
		    flt_puts(s, ok, err ? Error_attr : Number_attr);
		    s += ok;
		} else if ((ok = is_KEYWORD(s)) != 0) {
		    DPRINTF(("...KEYWORD: %d\n", ok));
		    s = put_KEYWORD(s, ok, &had_op);
		} else if ((ok = is_VARIABLE(s)) != 0) {
		    DPRINTF(("...VARIABLE: %d\n", ok));
		    s = put_VARIABLE(s, ok);
		} else if ((ok = is_String(s, &err)) != 0) {
		    had_op = 0;
		    if (*s == DQUOTE) {
			s = put_embedded(s, ok, err ? Error_attr : String_attr);
		    } else {
			flt_puts(s, ok, err ? Error_attr : String_attr);
			s += ok;
		    }
		} else {
		    if (strchr("()|&=~!,;", *s) != 0)
			had_op = 1;
		    else if (!isspace(*s))
			had_op = 0;
		    flt_putc(*s++);
		}
		break;
	    case eHERE:
		if (end_marker(s + is_BLANK(s), marker, 1)) {
		    state = eCODE;
		    free(marker);
		    marker = 0;
		}
		s = put_remainder(s, String_attr, quote_here);
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
