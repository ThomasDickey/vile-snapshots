/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/pl-filt.c,v 1.16 2001/05/19 00:00:30 tom Exp $
 *
 * Filter to add vile "attribution" sequences to perl scripts.  This is a
 * translation into C of an earlier version written for LEX/FLEX.
 */

#include <filters.h>

DefineFilter("perl");

#define ESC     '\\'
#define SQUOTE  '\''
#define DQUOTE  '"'

#define L_CURLY '{'
#define R_CURLY '}'
#define L_PAREN '('
#define R_PAREN ')'

#define PAREN_SLASH "(/"	/* FIXME: (/pattern/) could be composite */

#define isIdent(c) (isalnum(CharOf(c)) || c == '_')

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

/******************************************************************************
 * Lexical functions that match a particular token type                       *
 ******************************************************************************/
static int
is_BLANK(char *s)
{
    int found = 0;
    while ((*s == ' ') || (*s == '\t')) {
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
	} else if (isalpha(ch)
		   || ch == '_'
		   || (s != base && (isdigit(ch))))
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
	    || (s != base && isdigit(ch))
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
		|| ((radix == 8) && (ch >= '0' && ch < '8')))
		value = 1;
	    else
		break;
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

/* for compatibility with perlfilt.l, process single-line comments only */
static int
is_COMMENT(char *s)
{
    char *base = s;
    char *t = is_BLANK(s) + s;

    if (*t++ == '#') {
	while (t != the_last) {
	    if (*t == '\n')
		break;
	    t++;
	}
	s = t;
    }
    return (s - base);
}

/* preprocessor stuff is a comment with '!' in the second position */
static int
is_PREPROC(char *s)
{
    char *base = s;
    char *t = is_BLANK(s) + s;

    if (*t++ == '#') {
	if (*t == '!') {
	    while (t != the_last) {
		if (*t == '\n')
		    break;
		t++;
	    }
	    s = t;
	}
    }
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
    return (the_last - s > 3
	    && s[0] == '\n'
	    && s[1] == '\n'
	    && s[2] == '='
	    && isalpha(CharOf(s[3])));
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
		if (*base != SQUOTE && *base != DQUOTE)
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
	if (!isspace(*s)) {
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
 * FIXME: the only place that vileperl.l recognizes a PATTERN is after "!~"
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

static int
add_to_PATTERN(char *s)
{
    char *base = s;
    char *next;
    int first = CharOf(*s);
    int need = (first == 's' || first == 'y' || first == 't') ? 3 : 2;	/* number of delims we'll see */
    /* FIXME: 't' for 'tr' */

    if (the_last - s > need) {
	int delim = *s;
	int escaped = 0;

	while (isalpha(delim)) {
	    if (s != the_last) {
		delim = *++s;
	    }
	}
	if (delim == L_CURLY) {
	    delim = R_CURLY;
	    need = 1;
	}
	next = s;
	while (s != the_last) {
	    if (!escaped && (*s == ESC)) {
		escaped = 1;
	    } else {
		if (!escaped) {
		    if (*s == delim) {
			if (--need == 0) {
			    s++;
			    break;
			}
		    } else if (s != next
			       && delim == R_CURLY
			       && *s == L_CURLY) {
			++need;
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
    int x_modifier = 0;
    int n;

    for (n = len - 1; n > 0; n--) {
	if (isalpha(s[n])) {
	    if (s[n] == 'x') {
		x_modifier = 1;
		break;
	    }
	} else {
	    break;
	}
    }

    if (x_modifier) {		/* handle whitespace and comments */
	int first;
	int comment = 0;
	int escaped = 0;

	for (n = first = 0; n < len; n++) {
	    if (escaped) {
		escaped = 0;
	    } else if (s[n] == ESC) {
		escaped = 1;
	    } else if ((s[n] == ' ' || s[n] == '\t') && !comment) {
		flt_puts(s + first, (n - first - 0), String_attr);
		flt_putc(s[n]);
		first = n + 1;
	    } else if (s[n] == '\n') {
		if (comment) {
		    flt_puts(s + first, (n - first + 1), Comment_attr);
		    comment = 0;
		    first = n + 1;
		}
	    } else if (s[n] == '#') {
		if (!comment) {
		    flt_puts(s + first, (n - first - 1), String_attr);
		    comment = 1;
		    first = n + 0;
		}
	    }
	}
	flt_puts(s, (len - first), comment ? Comment_attr : String_attr);
    } else {
	flt_puts(s, len, String_attr);
    }
    s += len;
    return s;
}

static int
is_Option(char *s)
{
    int found = 0;

    if (*s == '-'
	&& ((s + 1) != the_last)
	&& isalpha(CharOf(s[1])))
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
    int ok;
    int err;
    int save;
    int quoted = 0;
    int parens = 0;

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
	while (s != the_last) {
	    if (*s == '\n') {
		in_line = -1;
	    } else {
		in_line++;
	    }
	    switch (state) {
	    case eCODE:
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
		} else if (in_line == 0
			   && (ok = is_PREPROC(s)) != 0) {
		    flt_puts(s, ok, Preproc_attr);
		    s += ok;
		} else if ((ok = is_COMMENT(s)) != 0) {
		    flt_puts(s, ok, Comment_attr);
		    s += ok;
		} else if ((ok = is_BLANK(s)) != 0) {
		    flt_puts(s, ok, "");
		    s += ok;
		} else if (*s == L_PAREN) {
		    parens++;
		    flt_putc(*s++);
		    if (*s == '/') {
			state = ePATTERN;
		    }
		} else if (*s == R_PAREN) {
		    if (--parens < 0)
			parens = 0;
		    flt_putc(*s++);
		} else if ((ok = is_KEYWORD(s)) != 0) {
		    save = s[ok];
		    s[ok] = 0;
		    if (!strcmp(s, "__END__"))
			state = eIGNORED;
		    if (ispunct(save)
			&& strchr("[]<>()", save) == 0
			&& (!strcmp(s, "s")
			    || !strcmp(s, "q")
			    || !strcmp(s, "qq")
			    || !strcmp(s, "y")
			    || !strcmp(s, "m")
			    || !strcmp(s, "tr"))) {
			state = ePATTERN;
			s[ok] = save;
			break;
		    }
		    flt_puts(s, ok, keyword_attr(s));
		    s[ok] = save;
		    s += ok;
		} else if ((ok = is_Option(s)) != 0) {
		    flt_puts(s, ok, Keyword_attr);
		    s += ok;
		} else if ((ok = is_IDENT(s)) != 0) {
		    flt_puts(s, ok, Ident2_attr);
		    s += ok;
		} else if ((ok = is_String(s, &err)) != 0) {
		    s = put_embedded(s, ok, err ? Error_attr : String_attr);
		} else if ((ok = is_NUMBER(s, &err)) != 0) {
		    flt_puts(s, ok, err ? Error_attr : Number_attr);
		    s += ok;
		} else {
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
		if ((ok = add_to_PATTERN(s)) != 0) {
		    s = write_PATTERN(s, ok);
		} else {
		    flt_putc(*s++);
		}
		state = eCODE;
		break;
	    case ePOD:
		if (end_POD(s))
		    state = eCODE;
		s = put_remainder(s, Comment_attr, 1);
		break;
	    default:
		flt_putc(*s++);
		break;
	    }
	}
	free(the_file);
    }
}
