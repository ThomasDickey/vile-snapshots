/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/rubyfilt.c,v 1.70 2013/04/07 23:32:40 tom Exp $
 *
 * Filter to add vile "attribution" sequences to ruby scripts.  This is a
 * translation into C of an earlier version written for LEX/FLEX.
 *
 * Although the documentation says it has simpler syntax, Ruby borrows from
 * Perl the worst of its syntax, i.e., regular expressions which can be split
 * across lines, have embedded comments.
 *
 * TODO: general delimited strings, e.g., %&this is a string&
 * TODO: %Q is equivalent of double-quoted string
 * TODO: %q is equivalent of single-quoted string
 * TODO: %x is equivalent of back-quoted string
 * TODO: embed quotes using backslashes
 * TODO: embed quotes by using double-quotes inside single
 * TODO: embed quotes by using single-quotes inside double
 * TODO: make var_embedded() parse nested quotes
 */

#include <filters.h>

#ifdef DEBUG
DefineOptFilter("ruby", "d");
#else
DefineFilter("ruby");
#endif

#define isIdent(c)   (isalnum(CharOf(c)) || c == '_')
#define isIdent0(c)  (isalpha(CharOf(c)) || c == '_')

#define MORE(s)	     ((s) < the_last)
#define ATLEAST(s,n) ((the_last - (s)) > (n))

#ifdef DEBUG
#define DPRINTF(params) if(FltOptions('d'))printf params
#else
#define DPRINTF(params)		/*nothing */
#endif

#define Parsed(var,tok) var = tok; \
	DPRINTF(("...%s: %d\n", tokenType(var), ok))

typedef enum {
    eALIAS
    ,eCLASS
    ,eCODE
    ,eDEF
    ,eHERE
    ,ePOD
    ,eEND
} States;

typedef enum {
    tNULL
    ,tBLANK
    ,tCHAR
    ,tCOMMENT
    ,tHERE
    ,tKEYWORD
    ,tNUMBER
    ,tOPERATOR
    ,tREGEXP
    ,tSTRING
    ,tVARIABLE
} TType;

static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *String_attr;
static char *Number_attr;
static char *Type_attr;

static int line_size(char *);
static int var_embedded(char *);

/*
 * The in-memory copy of the input file.
 */
static char *the_file;
static char *the_last;
static size_t the_size;

/*
 * Stack of here-document tags
 */
#define HERE_TAGS struct _here_tags
HERE_TAGS {
    HERE_TAGS *next;
    char *value;
    int strip;
    int quote;
};

static HERE_TAGS *here_tags;

#ifdef DEBUG
static char *
stateName(States state)
{
    char *result = "?";

    switch (state) {
    case eALIAS:
	result = "ALIAS";
	break;
    case eCLASS:
	result = "CLASS";
	break;
    case eCODE:
	result = "CODE";
	break;
    case eDEF:
	result = "DEF";
	break;
    case eHERE:
	result = "HERE";
	break;
    case ePOD:
	result = "POD";
	break;
    case eEND:
	result = "END";
	break;
    }
    return result;
}

static char *
tokenType(TType type)
{
    char *result = "?";

    switch (type) {
    case tNULL:
	result = "No Token";
	break;
    case tBLANK:
	result = "BLANK";
	break;
    case tCHAR:
	result = "CHAR";
	break;
    case tCOMMENT:
	result = "COMMENT";
	break;
    case tHERE:
	result = "HERE";
	break;
    case tKEYWORD:
	result = "KEYWORD";
	break;
    case tNUMBER:
	result = "NUMBER";
	break;
    case tOPERATOR:
	result = "OPERATOR";
	break;
    case tREGEXP:
	result = "REGEXP";
	break;
    case tSTRING:
	result = "STRING";
	break;
    case tVARIABLE:
	result = "VARIABLE";
	break;
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
is_ESCAPED(char *s)
{
    int found = 0;
    if (*s == BACKSLASH) {
	found = 2;
    }
    return found;
}

static int
is_STRINGS(char *s, int *err, int left_delim, int right_delim, int single)
{
    char *base = s;
    int found = 0;
    int escape = 0;
    int level = 0;
    int len;

    *err = 0;
    if (*s == left_delim) {	/* should always be true */
	s++;
	if (left_delim != right_delim)
	    ++level;

	for (;;) {
	    if (!MORE(s)) {
		*err = 1;	/* unterminated string */
		break;
	    }
	    if (!escape && (*s == BACKSLASH)) {
		escape = 1;
	    } else if (escape) {
		escape = 0;
	    } else if (!single && (len = var_embedded(s)) != 0) {
		s += len - 1;
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
	found = (int) (s - base);
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
    int delim = 0;
    int leading = 1;

    while (MORE(s)) {
	ch = CharOf(*s);
	if (leading && isIdent0(ch)) {
	    leading = 0;
	    s++;
	} else if (!leading && isIdent(ch)) {
	    s++;
	} else if (ch == BQUOTE
		   || ch == SQUOTE
		   || ch == DQUOTE) {
	    s++;
	    if (delim) {
		if (delim == ch)
		    break;
	    } else {
		delim = ch;
	    }
	} else if (!delim) {
	    break;
	} else {
	    ++s;
	}
    }
    return (int) (s - base);
}

/*
 * pattern: {KEYWORD}
 * returns: length of the token
 */
static int
is_KEYWORD(char *s)
{
    int found = 0;

    if (isIdent0(CharOf(s[0]))) {
	while (ATLEAST(s, found) && isIdent(s[found])) {
	    ++found;
	}
    }
    return found;
}

/*
 * Context-specific keyword: Match a method-style keyword,
 * ie. a KEYWORD with a possible suffix of !, ?, or =.
 * pattern: {KEYWORD}[!?=]?
 * returns: length of the token
 */
static int
is_MKEYWORD(char *s, int use_eql)
{
    int found = 0;

    if ((found = is_KEYWORD(s)) != 0) {
	if (ATLEAST(s, found)
	    && (s[found] == '!'
		|| s[found] == '?'	/* FIXME - should look for '.' */
		|| (use_eql == 0 && s[found] == '='))) {
	    ++found;
	}
    }
    return found;
}

/*
 * pattern: [$]([-_.\/,"\\=~\$?&`'+*;!@<>:]|{CONST}|{VAR})
 * returns: length of the token, or zero.
 */
static int
is_GLOBAL(char *s)
{
    int found = 0;

    if (*s == '$' && MORE(++s)) {
	if (*s != '\0' && strchr("-_./,\"\\=~$?&`'+*;!@<>:", *s)) {
	    found = 1;
	} else if (isdigit(CharOf(*s))) {
	    while (ATLEAST(s, found)
		   && isdigit(CharOf(s[found])))
		++found;
	} else {
	    found = is_KEYWORD(s);
	}
	if (found != 0)
	    ++found;
    }
    return found;
}

/*
 * pattern: [@][@]?{KEYWORD}
 * returns: length of the token, or zero.
 */
static int
is_INSTANCE(char *s)
{
    char *base = s;
    int found = 0;

    if (*s == '@' && MORE(++s)) {
	if (*s == '@')
	    s++;
	if ((found = is_KEYWORD(s)) != 0)
	    found += (int) (s - base);
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
 * pattern: \?(\\M-a)?(\\C-a)?
 */
static int
is_CHAR(char *s, int *err)
{
    int found = 0;

    if (*s == '?' && ATLEAST(s, 5)) {
	if (*++s == BACKSLASH) {
	    ++s;
	    if (((*s == 'M') || (*s == 'C'))
		&& (s[1] == '-')) {
		*err = 0;
		found = 5;
		if (*s == 'M' && ATLEAST(s, 5)
		    && s[2] == BACKSLASH
		    && s[3] == 'C'
		    && s[4] == '-'
		    && isgraph(s[5])) {
		    s += 5;
		    found += 3;
		} else if (*s == 'C') {
		    s += 2;
		}
	    } else {
		found = 3;
	    }
	} else {
	    found = 2;
	}
	if (found) {
	    /* s now points to the character, but it may be escaped */
	    if (!isgraph(*s)) {
		found = 0;
	    } else if (*s == BACKSLASH) {
		++found;
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
 *
 * But note that ruby allows a function immediately after a number separated
 * by a ".", e.g.,
 *	1.upto(n)
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
		&& (s[1] == '.' || isalpha(CharOf(s[1])))) {
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
		} else if (s[1] == 'b') {
		    radix = 2;
		    s++;
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
	    if (state != 1 || !value) {
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

/******************************************************************************
 ******************************************************************************/

/*
 * Match a given 'marker' keyword, optionally checking if it is alone on the
 * line.  Documentation implies otherwise, but parser does check for a blank
 * after the keyword.
 */
static int
end_marker(char *s, const char *marker, int only)
{
    int len = (int) strlen(marker);

    return (ATLEAST(s, len)
	    && !strncmp(s, marker, (size_t) len)
	    && isspace(CharOf(s[len]))
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
 * Ruby allows more than one here-document to begin on a line.  They "stack",
 * and are processed in succession.
 */
static void
make_here_tag(char *value, int quote, int strip)
{
    size_t size = 0;
    HERE_TAGS *data = type_alloc(HERE_TAGS, (char *) 0, (size_t) 1, &size);

    if (data != 0) {
	HERE_TAGS *p = here_tags;
	HERE_TAGS *q = 0;

	while (p != 0) {
	    q = p;
	    p = p->next;
	}
	if (q != 0)
	    q->next = data;
	else
	    here_tags = data;
	data->next = p;
	data->value = value;
	data->quote = quote;
	data->strip = strip;
    }
}

static char *
free_here_tag(void)
{
    HERE_TAGS *next = here_tags->next;
    char *result = next ? next->value : 0;

    free(here_tags->value);
    free(here_tags);
    here_tags = next;

    return result;
}

/*
 * FIXME: we don't check for unbalanced quotes.
 */
static int
begin_HERE(char *s)
{
    char *base = s;
    char *first;
    char *marker = 0;
    int ok;
    int strip = 0;

    if (ATLEAST(s, 3)
	&& s[0] == '<'
	&& s[1] == '<'
	&& !isBlank(s[2])) {
	s += 2;
	first = s;
	if (*s == '-') {
	    strip = 1;
	    ++s;
	}
	if ((ok = is_QIDENT(s)) != 0) {
	    size_t temp = 0;
	    int delim = (*s == SQUOTE || *s == DQUOTE) ? *s : 0;
	    int quote = (*s == SQUOTE);

	    s += ok;

	    if ((marker = do_alloc((char *) 0, (size_t) (ok + 1), &temp)) != 0) {
		char *d = marker;
		if (delim)
		    ++first;
		while (first != s) {
		    if (delim) {
			if (*first == delim)
			    break;
			*d++ = *first;
		    } else {
			if (isIdent(*first))
			    *d++ = *first;
		    }
		    first++;
		}
		*d = 0;
		make_here_tag(marker, quote, strip);
	    }
	}
    }
    return (marker ? (int) (s - base) : 0);
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
	flt_puts(base, (int) (s - base), "");
    }
    return s;
}

/*
 * Return the number of characters if an operator is found, otherwise zero.
 */
static int
is_OPERATOR(char *s)
{
    int found = 0;
#define OPS(s) { s, sizeof(s) - 1 }
    static const struct {
	const char *ops;
	int len;
    } table[] = {
	/* 3 */
	OPS("&&="),
	    OPS("**="),
	    OPS("..."),
	    OPS("<<="),
	    OPS("<=>"),
	    OPS("==="),
	    OPS(">>="),
	    OPS("[]="),
	    OPS("||="),
	/* 2 */
	    OPS("!="),
	    OPS("!~"),
	    OPS("%="),
	    OPS("&&"),
	    OPS("&="),
	    OPS("**"),
	    OPS("*="),
	    OPS("+="),
	    OPS("-="),
	    OPS(".."),
	    OPS("/="),
	    OPS("::"),
	    OPS("<<"),
	    OPS("<="),
	    OPS("=="),
	    OPS("=>"),
	    OPS("=~"),
	    OPS(">="),
	    OPS(">>"),
	    OPS("[]"),
	    OPS("^="),
	    OPS("|="),
	    OPS("||"),
	/* 1 */
	    OPS("!"),
	    OPS("&"),
	    OPS("("),
	    OPS(")"),
	    OPS("*"),
	    OPS("+"),
	    OPS(","),
	    OPS("-"),
	    OPS(";"),
	    OPS("="),
	    OPS("["),
	    OPS("]"),
	    OPS("^"),
	    OPS("{"),
	    OPS("|"),
	    OPS("}"),
	    OPS("~"),
    };

    if (ispunct(CharOf(*s))) {
	unsigned n;
	for (n = 0; n < TABLESIZE(table); ++n) {
	    if (ATLEAST(s, table[n].len)
		&& table[n].ops[0] == *s
		&& !memcmp(s, table[n].ops, (size_t) table[n].len)) {
		found = table[n].len;
		break;
	    }
	}
    }
    return found;
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
    int range = 0;
    int block = (left_delim != L_BLOCK);

    while (MORE(s)) {
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
	if ((len = is_ESCAPED(s)) != 0) {
	    s += len;
	} else if (block && (*s == R_BLOCK) && range) {
	    range = 0;
	    ++s;
	} else if (block && (*s == L_BLOCK) && !range) {
	    range = 1;
	    ++s;
	} else if ((len = var_embedded(s)) != 0) {
	    s += len;
	} else if (range) {
	    ++s;
	} else if (s != base && *s == right_delim) {
	    ++s;
	    while (MORE(s) && isalpha(CharOf(*s))) {
		++s;
	    }
	    found = (int) (s - base);
	    break;
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
    } else if (ATLEAST(s, 4)
	       && s[0] == '%'
	       && s[1] == 'r'
	       && !isalnum(CharOf(s[2]))) {

	*delim = balanced_delimiter(s + 2);
	found = 2 + is_REGEXP(s + 2, s[2], *delim);
    }
    return found;
}

/*
 * Parse both the unquoted and quoted symbol types.
 * This gets treated as a string literal anyway, so
 * we only call it from is_String.
 */
static int
is_Symbol(char *s, int *delim, int *err)
{
    int found = 0;

    if (*s++ == ':') {
	switch (*s) {
	case SQUOTE:
	    if ((found = is_STRINGS(s, err, *s, *s, 1)) != 0)
		*delim = SQUOTE;
	    break;
	case DQUOTE:
	    if ((found = is_STRINGS(s, err, *s, *s, 0)) != 0)
		*delim = DQUOTE;
	    break;
	case BQUOTE:
	    found = 1;
	    break;
	default:
	    found = is_MKEYWORD(s, 0);
	    break;
	}
    }

    if (found != 0)
	found += 1;

    return found;
}

/*
 * Parse the various types of quoted strings.
 */
static int
is_String(char *s, int *delim, int *err)
{
    int found = 0;

    *delim = 0;
    if (ATLEAST(s, 2) && (found = is_Symbol(s, delim, err)) == 0) {
	switch (*s) {
	case '%':
	    if (ATLEAST(s, 4)) {
		switch (s[1]) {
		case '(':
		case '[':
		case '{':
		    found = is_STRINGS(s + 1,
				       err,
				       s[1],
				       balanced_delimiter(s + 1),
				       1);
		    if (found != 0) {
			found += 2;
			*delim = SQUOTE;
		    }
		    break;
		case 'q':
		case 'w':
		    found = is_STRINGS(s + 2,
				       err,
				       s[2],
				       balanced_delimiter(s + 2),
				       1);
		    if (found != 0) {
			found += 2;
			*delim = SQUOTE;
		    }
		    break;
		case 'x':
		case 'Q':
		    found = is_STRINGS(s + 2,
				       err,
				       s[2],
				       balanced_delimiter(s + 2),
				       0);
		    if (found != 0) {
			found += 2;
			*delim = DQUOTE;
		    }
		    break;
		}
	    }
	    break;
	case SQUOTE:
	    if ((found = is_STRINGS(s, err, *s, *s, 1)) != 0)
		*delim = SQUOTE;
	    break;
	case BQUOTE:
	case DQUOTE:
	    if ((found = is_STRINGS(s, err, *s, *s, 0)) != 0)
		*delim = DQUOTE;
	    break;
	case BACKSLASH:
	    found = is_ESCAPED(s);
	    *delim = SQUOTE;
	    break;
	}
    }
    return found;
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

static char *
put_newline(char *s)
{
    if (MORE(s))
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
    int delim;
    int level = 0;

    if (*s == '#' && MORE(++s)) {
	if (*s == L_CURLY) {
	    ++level;
	    for (found = 1; ATLEAST(s, found); ++found) {
		/* FIXME: this check for %q{xxx} is too naive... */
		if (s[found] == '%' && (strchr("wqQ", s[found + 1]) != 0)) {
		    int ignore;
		    found += is_String(s + found, &delim, &ignore);
		} else if (s[found] == L_CURLY) {
		    ++level;
		} else if (s[found] == R_CURLY) {
		    if (--level <= 0) {
			++found;
			break;
		    }
		}
	    }
	} else {
	    found = is_VARIABLE(s);
	}
    }
    return found ? found + 1 : 0;
}

/*
 * FIXME: var_embedded() can recognize a multi-line embedded variable, but
 * this function is called for each line.  Probably should redo the output
 * so this is called at the end of the here-document, etc.
 */
static char *
put_embedded(char *s, int len, char *attr)
{
    int id;
    int j, k;

    for (j = k = 0; j < len; j++) {
	if ((j == 0 || (s[j - 1] != BACKSLASH))
	    && (id = var_embedded(s + j)) != 0
	    && (id + j) < len) {
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
    int skip = (int) (skip_BLANKS(s) - s);
    ok -= skip;
    s += skip;
    flt_puts(s, ok, Comment_attr);
    return s + ok;
}

static char *
put_KEYWORD(char *s, int ok, int *had_op)
{
    const char *attr = 0;
    char save = s[ok];

    s[ok] = '\0';
    attr = get_keyword_attr(s);
    s[ok] = save;
    if (isEmpty(attr) && isupper(CharOf(s[0])))
	attr = Type_attr;
    flt_puts(s, ok, attr);
    *had_op = (attr == Keyword_attr);
    return s + ok;
}

static char *
put_OPERATOR(char *s, int ok, int *had_op)
{
    flt_puts(s, ok, "");
    if (strchr("[(|&=~!,;", *s) != 0)
	*had_op = 1;
    return s + ok;
}

static char *
put_VARIABLE(char *s, int ok)
{
    const char *attr = 0;
    char save = s[ok];

    s[ok] = '\0';
    attr = get_keyword_attr(s);
    s[ok] = save;
    flt_puts(s, ok, (attr != 0 && *attr != '\0') ? attr : Ident2_attr);
    return s + ok;
}

static char *
put_REGEXP(char *s, int length, int delim)
{
    char *base = s;
    char *first = s;
    char *last;
    int len;
    int range = 0;
    int on_end = 0;
    int block = (delim != R_BLOCK);
    int level = 0;
    int extended = 0;

    for (last = s + length - 1; last != s && isalpha(*last); --last) {
	if (*last == 'x') {
	    extended = 1;
	    break;
	}
    }
    if (*s == '%') {
	flt_puts(s, 3, Keyword_attr);
	s += 3;
	first = s;
	on_end = 1;
    }
    flt_bfr_begin(String_attr);
    while (s < base + length) {
	if ((len = is_ESCAPED(s)) != 0) {
	    flt_bfr_append(s, len);
	    s += len;
	} else if (block && (*s == R_BLOCK) && range) {
	    range = 0;
	    flt_bfr_append(s++, 1);
	} else if (block && (*s == L_BLOCK) && !range) {
	    range = 1;
	    flt_bfr_append(s++, 1);
	} else if ((len = var_embedded(s)) != 0) {
	    flt_bfr_embed(s, len, Ident2_attr);
	    s += len;
	} else if (range) {
	    flt_bfr_append(s++, 1);
	} else if (*s == L_PAREN && delim != R_PAREN) {
	    ++level;
	    flt_bfr_append(s++, 1);
	} else if (*s == R_PAREN && delim != R_PAREN) {
	    --level;
	    flt_bfr_append(s++, 1);
	} else if (extended && (len = is_BLANK(s)) != 0) {
	    last = s;
	    flt_bfr_embed(last, len, "");
	    s += len;
	} else if (extended && (*s == '#')) {
	    last = s;
	    while (MORE(s) && !isreturn(CharOf(*s))) {
		++s;
	    }
	    flt_bfr_embed(last, (int) (s - last), Comment_attr);
	} else if (s != first && level == 0 && *s == delim) {
	    if (!on_end)
		flt_bfr_append(s++, 1);
	    last = s;
	    if (on_end)
		++s;
	    while (MORE(s) && isalpha(CharOf(*s))) {
		++s;
	    }
	    flt_bfr_embed(last, (int) (s - last), Keyword_attr);
	    break;
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
    (void) before;
}

static void
do_filter(FILE *input GCC_UNUSED)
{
    static size_t used;
    static char *line;

    size_t actual = 0;
    size_t request = 0;
    States state = eCODE;
    TType this_tok = tNULL;
    TType last_tok = tNULL;
    char *s;
    char *marker = 0;
    int in_line = -1;
    int ok;
    int err;
    int delim;
    int had_op = 1;		/* true to allow regex */

    (void) input;

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);
    Type_attr = class_attr(NAME_TYPES);

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
	while (MORE(s)) {
	    if (*s == '\n') {
		if (marker != 0)
		    state = eHERE;
		in_line = -1;
		if (state == eCODE)
		    had_op = 1;
	    } else {
		in_line++;
	    }
	    DPRINTF(("(%s(%.*s) line:%d op:%d)\n",
		     stateName(state), is_KEYWORD(s) + 1, s, in_line, had_op));
	    switch (state) {
		/*
		 * alias method-name method-name
		 * alias global-variable-name global-variable-name
		 */
	    case eALIAS:
	    case eDEF:
		if ((ok = is_COMMENT(s)) != 0) {
		    Parsed(this_tok, tCOMMENT);
		    s = put_COMMENT(s, ok);
		} else if ((ok = is_BLANK(s)) != 0) {
		    Parsed(this_tok, tBLANK);
		    flt_puts(s, ok, "");
		    s += ok;
		} else if ((ok = is_MKEYWORD(s, 0)) != 0) {
		    Parsed(this_tok, tKEYWORD);
		    s = put_KEYWORD(s, ok, &had_op);
		    state = eCODE;
		} else if ((ok = is_OPERATOR(s)) != 0) {
		    Parsed(this_tok, tOPERATOR);
		    s = put_OPERATOR(s, ok, &had_op);
		    state = eCODE;
		} else {
		    flt_putc(*s++);
		    state = eCODE;
		}
		had_op = 1;	/* kludge - in case a "def" precedes regex */
		break;
		/*
		 * Class definitions use '<' specially, like a reverse arrow:
		 *      class ClassName < SuperClass
		 *      class << Object
		 *
		 * The first case is not a syntax problem, but the
		 * singleton-class definition conflicts with here-documents.
		 */
	    case eCLASS:
		if ((ok = is_COMMENT(s)) != 0) {
		    Parsed(this_tok, tCOMMENT);
		    s = put_COMMENT(s, ok);
		} else if ((ok = is_BLANK(s)) != 0) {
		    Parsed(this_tok, tBLANK);
		    flt_puts(s, ok, "");
		    s += ok;
		} else if ((ok = is_KEYWORD(s)) != 0) {
		    Parsed(this_tok, tKEYWORD);
		    s = put_KEYWORD(s, ok, &had_op);
		    state = eCODE;
		} else if ((ok = is_OPERATOR(s)) != 0) {
		    Parsed(this_tok, tOPERATOR);
		    s = put_OPERATOR(s, ok, &had_op);
		    state = eCODE;
		} else {
		    flt_putc(*s++);
		    state = eCODE;
		}
		had_op = 1;	/* kludge - in case a "class" precedes regex */
		break;
	    case eCODE:
		if ((last_tok == tKEYWORD || last_tok == tOPERATOR)
		    && (ok = begin_HERE(s)) != 0) {
		    Parsed(this_tok, tHERE);
		    flt_puts(s, ok, String_attr);
		    s += ok;
		    marker = here_tags->value;
		} else if ((in_line < 0 && begin_POD(s + 1))
			   || ((s == the_file && begin_POD(s)))) {
		    DPRINTF(("...POD\n"));
		    state = ePOD;
		    flt_putc(*s++);	/* write the newline */
		    s = put_remainder(s, Comment_attr, 1);
		} else if ((ok = is_COMMENT(s)) != 0) {
		    Parsed(this_tok, tCOMMENT);
		    s = put_COMMENT(s, ok);
		} else if ((ok = is_BLANK(s)) != 0) {
		    Parsed(this_tok, tBLANK);
		    flt_puts(s, ok, "");
		    s += ok;
		} else if ((*s == '%' || had_op)
			   && (ok = is_Regexp(s, &delim)) != 0) {
		    Parsed(this_tok, tREGEXP);
		    s = put_REGEXP(s, ok, delim);
		} else if ((ok = is_CHAR(s, &err)) != 0) {
		    Parsed(this_tok, tCHAR);
		    had_op = 0;
		    if (err) {
			flt_error("not a number");
			flt_puts(s, ok, Error_attr);
		    } else {
			flt_puts(s, ok, Number_attr);
		    }
		    s += ok;
		} else if ((ok = is_NUMBER(s, &err)) != 0) {
		    Parsed(this_tok, tNUMBER);
		    had_op = 0;
		    if (err) {
			flt_error("not a number");
			flt_puts(s, ok, Error_attr);
		    } else {
			flt_puts(s, ok, Number_attr);
		    }
		    s += ok;
		} else if ((ok = is_MKEYWORD(s, 1)) != 0) {
		    Parsed(this_tok, tKEYWORD);
		    if (ok == 5 && !strncmp(s, "alias", (size_t) ok))
			state = eALIAS;
		    else if (ok == 5 && !strncmp(s, "class", (size_t) ok))
			state = eCLASS;
		    else if (ok == 3 && !strncmp(s, "def", (size_t) ok))
			state = eDEF;
		    else if (ok == 7 && !strncmp(s, "__END__", (size_t) ok))
			state = eEND;
		    s = put_KEYWORD(s, ok, &had_op);
		} else if ((ok = is_VARIABLE(s)) != 0) {
		    Parsed(this_tok, tVARIABLE);
		    s = put_VARIABLE(s, ok);
		    had_op = 0;
		} else if (ATLEAST(s, (ok = 2))
			   && !strncmp(s, "?\"", (size_t) ok)) {
		    Parsed(this_tok, tVARIABLE);
		    s = put_VARIABLE(s, ok);	/* csv.rb uses it, undocumented */
		    had_op = 0;
		} else if ((ok = is_String(s, &delim, &err)) != 0) {
		    int on_end = 0;
		    Parsed(this_tok, tSTRING);
		    had_op = 0;
		    if (*s == '%') {
			flt_puts(s, 2, Keyword_attr);
			if (!err) {
			    --ok;
			    if (strchr("[({", s[1]))	/* })] */
				--ok;
			    on_end = 1;
			}
			s += 2;
			ok -= 2;
		    }
		    if (delim == DQUOTE) {
			if (err) {
			    flt_error("unexpected quote");
			    s = put_embedded(s, ok, Error_attr);
			} else {
			    s = put_embedded(s, ok, String_attr);
			}
		    } else {
			if (err) {
			    flt_error("unterminated string");
			    flt_puts(s, ok, Error_attr);
			} else {
			    flt_puts(s, ok, String_attr);
			}
			s += ok;
		    }
		    if (on_end) {
			flt_puts(s, 1, Keyword_attr);
			++s;
		    }
		} else if ((ok = is_OPERATOR(s)) != 0) {
		    Parsed(this_tok, tOPERATOR);
		    s = put_OPERATOR(s, ok, &had_op);
		} else {
		    if (!isspace(CharOf(*s)))
			had_op = 0;
		    flt_putc(*s++);
		}
		break;
	    case eHERE:
		if (here_tags == 0) {
		    state = eCODE;
		} else if (end_marker(s + (here_tags->strip
					   ? is_BLANK(s)
					   : 0),
				      marker, 1)) {
		    if ((marker = free_here_tag()) == 0)
			state = eCODE;
		}
		s = put_remainder(s, String_attr,
				  (here_tags
				   ? here_tags->quote
				   : 0));
		break;
	    case ePOD:
		if (end_POD(s))
		    state = eCODE;
		s = put_remainder(s, Comment_attr, 1);
		break;
	    case eEND:
		s = put_remainder(s, Comment_attr, 1);
		break;
	    }

	    switch (this_tok) {
	    case tNULL:
	    case tBLANK:
	    case tCOMMENT:
		continue;
	    case tCHAR:
	    case tHERE:
	    case tNUMBER:
	    case tREGEXP:
	    case tSTRING:
		last_tok = this_tok;
		break;
	    case tKEYWORD:
		last_tok = this_tok;
		break;
	    case tOPERATOR:
		last_tok = this_tok;
		break;
	    case tVARIABLE:
		last_tok = this_tok;
		break;
	    }
	}
	free(the_file);
    }
    while (here_tags != 0) {
	flt_error("expected tag:%s", here_tags->value);
	(void) free_here_tag();
    }
}

#if NO_LEAKS
static void
free_filter(void)
{
}
#endif
