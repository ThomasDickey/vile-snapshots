/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/m4-filt.c,v 1.15 2000/02/28 11:58:59 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of m4 
 * input text.  This is in C rather than LEX because M4's quoting delimiters
 * change by embedded directives, to simplify inclusion of brackets.
 */

#include <filters.h>

DefineFilter("m4");

#define NAME_L_QUOTE "LeftQuote"
#define NAME_R_QUOTE "RightQuote"
#define NAME_L_CMT   "LeftComment"
#define NAME_R_CMT   "RightComment"

#define L_PAREN '('
#define R_PAREN ')'

#define L_QUOTE "`"
#define R_QUOTE "'"

#define L_CMT "#"
#define R_CMT "\n"

#define isIdent(c)  (isalpha(c) || (c) == '_')
#define isNamex(c)  (isalnum(c) || (c) == '_')

static char *Comment_attr;
static char *Ident_attr;
static char *Literal_attr;
static char *Number_attr;

/* changequote */
typedef struct {
    char *text;
    unsigned used, have;
} Quote;

#define isQuote(s,n) (n.used && !strncmp(s, n.text, n.used))

#define write_quote(output,n) flt_puts(n.text, n.used, "")

static Quote leftquote, rightquote;
static Quote leftcmt, rightcmt;

static char *
SkipBlanks(char *src)
{
    while (isspace(*src))
	src++;
    return (src);
}

static void
new_quote(Quote * q, char *s)
{
    q->used = strlen(s);
    q->text = do_alloc(q->text, q->used, &(q->have));
    strcpy(q->text, s);
}

static int
count_of(char **args)
{
    int count = 0;
    if (args != 0) {
	while (*args != 0) {
	    args++;
	    count++;
	}
    }
    return count;
}

static void
ChangeQuote(char **args)
{
    if (args != 0) {
	new_quote(&leftquote, count_of(args) > 0 ? args[0] : L_QUOTE);
	new_quote(&rightquote, count_of(args) > 1 ? args[1] : R_QUOTE);
    }
}

static void
ChangeComment(char **args)
{
    if (args != 0) {
	new_quote(&leftcmt, count_of(args) > 0 ? args[0] : L_CMT);
	new_quote(&rightcmt, count_of(args) > 1 ? args[1] : R_CMT);
    }
}

static char *
parse_arglist(char *s, char ***args)
{
    char *t = SkipBlanks(s);
    char *r;
    char *v;
    unsigned count, used;

    if (*t == L_PAREN) {
	count = 0;		/* always have a null on the end */
	used = 0;
	t++;
	for (;;) {
	    /* FIXME: m4 could accept newlines in an arglist */
	    r = t = SkipBlanks(t);
	    while ((*t != ',') && (*t != R_PAREN) && *t)
		t++;
	    v = (char *)malloc(1 + t - r);
	    if (t != r)
		strncpy(v, r, t - r);
	    v[t - r] = 0;
	    *args = (char **) do_alloc((char *) (*args), sizeof(*args) *
		(count + 2), &used);
	    (*args)[count++] = v;
	    (*args)[count] = 0;
	    if (*t == R_PAREN) {
		t++;
		break;
	    } else if (!*t) {
		break;
	    }
	    t++;
	}
	flt_puts(s, t - s, "");
	return t;
    }
    return s;
}

static void
free_arglist(char **args)
{
    if (args != 0) {
	char **save = args;
	while (*args)
	    free(*args++);
	free(save);
    }
}

static char *
handle_directive(char *name, char *s)
{
    static struct {
	char *name;
	void (*func) (char **);
    } table[] = {
	{
	    "changequote", ChangeQuote
	},
	{
	    "changecom", ChangeComment
	},
    };
    size_t n;

    for (n = 0; n < sizeof(table) / sizeof(table[0]); n++) {
	if (!strcmp(name, table[n].name)) {
	    char **args = 0;
	    s = parse_arglist(s, &args);
	    table[n].func(args);
	    free_arglist(args);
	    break;
	}
    }
    return s;
}

static char *
extract_identifier(char *s)
{
    static char *name;
    static unsigned have;

    unsigned has_cpp = 0;
    unsigned need;
    char *attr;
    char *base;
    char *show = s;

    if (*s == '#') {
	has_cpp = 1;
	s++;
	s = SkipBlanks(s);
    }
    base = s;
    while (isNamex(*s))
	s++;
    if (base != s) {
	need = has_cpp + (s - base);
	name = do_alloc(name, need, &have);
	if (has_cpp) {
	    name[0] = '#';
	}
	strncpy(name + has_cpp, base, s - base);
	name[(s - base) + has_cpp] = 0;
	if (!strcmp(name, "dnl")) {
	    /* FIXME: GNU m4 may accept an argument list here */
	    s += strlen(s);
	    flt_puts(show, s - show, Comment_attr);
	} else if ((attr = keyword_attr(name)) != 0) {
	    flt_puts(show, s - show, attr);
	} else {
	    flt_puts(show, s - show, Ident_attr);
	}
	s = handle_directive(name, s);
    }
    return s;
}

static int
has_endofcomment(char *s, int *level)
{
    char *base = s;
    while (*s) {
	if (isQuote(s, rightcmt)) {
	    s += rightcmt.used;
	    *level = 0;
	    break;
	} else {
	    ++s;
	}
    }
    return (s - base);
}

static int
has_endofliteral(char *s, int *level)
{
    char *base = s;
    while (*s) {
	if (isQuote(s, leftquote)) {
	    s += leftquote.used;
	    *level += 1;
	} else if (isQuote(s, rightquote)) {
	    s += rightquote.used;
	    *level -= 1;
	    if (*level == 0)
		break;
	} else {
	    ++s;
	}
    }
    return (s - base);
}

static char *
write_number(char *s)
{
    char *base = s;
    int radix = (*s == '0') ? ((s[1] == 'x' || s[1] == 'X') ? 16 : 8) : 10;
    int done = 0;

    if (radix == 16)
	s++;
    while (!done) {
	s++;
	switch (radix) {
	case 8:
	    done = !isdigit(*s) || (*s == '8') || (*s == '9');
	    break;
	case 10:
	    done = !isdigit(*s);
	    break;
	case 16:
	    done = !isxdigit(*s);
	    break;
	}
    }
    flt_puts(base, s - base, Number_attr);
    return s;
}

static char *
write_literal(char *s, int *literal)
{
    static char *buffer;
    static unsigned have;
    static unsigned used;

    unsigned c_length = has_endofliteral(s, literal);
    unsigned need = c_length;

    if (*literal == 0) {
	if (need > rightquote.used) {
	    need -= rightquote.used;
	    buffer = do_alloc(buffer, used + need + 1, &have);
	    strncpy(buffer + used, s, need);
	    used += need;
	}
	if (used) {
	    flt_puts(buffer, used, Literal_attr);
	    used = 0;
	}
	write_quote(fp, rightquote);
    } else {
	buffer = do_alloc(buffer, used + need + 1, &have);
	strncpy(buffer + used, s, need);
	used += need;
    }
    return s + c_length;
}

static char *
write_comment(char *s, int *level)
{
    int c_length = has_endofcomment(s, level);
    flt_puts(s, c_length, Comment_attr);
    return s + c_length;
}

static void
init_filter(int before)
{
    /* FIXME: make these settable by class definitions */
    if (before) {
	insert_keyword(NAME_L_QUOTE, L_QUOTE, 1);
	insert_keyword(NAME_R_QUOTE, R_QUOTE, 1);
	insert_keyword(NAME_L_CMT, L_CMT, 1);
	insert_keyword(NAME_R_CMT, R_CMT, 1);
    }
}

static void
do_filter(FILE * input)
{
    static unsigned used;
    static char *line;

    char *s;
    int literal, comment;

    new_quote(&leftquote, class_attr(NAME_L_QUOTE));
    new_quote(&rightquote, class_attr(NAME_R_QUOTE));
    new_quote(&leftcmt, class_attr(NAME_L_CMT));
    new_quote(&rightcmt, class_attr(NAME_R_CMT));

    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr = class_attr(NAME_IDENT);
    Literal_attr = class_attr(NAME_LITERAL);
    Number_attr = class_attr(NAME_NUMBER);

    literal = 0;
    comment = 0;

    while (readline(input, &line, &used) != NULL) {
	s = line;
	while (*s) {
	    /*
	     * Quoted literals can nest, and override comments
	     */
	    if (literal) {
		s = write_literal(s, &literal);
	    } else if (isQuote(s, leftquote)) {
		write_quote(output, leftquote);
		s += leftquote.used;
		literal++;
		s = write_literal(s, &literal);
	    } else if (isQuote(s, rightquote)) {
		write_quote(output, rightquote);
		s += rightquote.used;
		if (--literal > 0)
		    s = write_literal(s, &literal);
		else
		    literal = 0;
		/*
		 * Comments don't nest
		 */
	    } else if (comment) {
		s = write_comment(s, &comment);
		comment = 0;
	    } else if (isQuote(s, leftcmt)) {
		flt_puts(s, leftcmt.used, Comment_attr);
		s += leftcmt.used;
		comment = 1;
	    } else if (isQuote(s, rightcmt)) {
		flt_puts(s, rightcmt.used, Comment_attr);
		s += rightcmt.used;
		comment = 0;
	    } else if (isIdent(*s)) {
		s = extract_identifier(s);
	    } else if (isdigit(*s)) {
		s = write_number(s);
	    } else {
		flt_putc(*s++);
	    }
	}
    }
}
