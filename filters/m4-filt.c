/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/m4-filt.c,v 1.19 2000/11/04 20:15:46 tom Exp $
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

#define isIdent(c)  (isalpha(CharOf(c)) || (c) == '_')
#define isNamex(c)  (isalnum(CharOf(c)) || (c) == '_')

static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Literal_attr;
static char *Number_attr;

/* changequote */
typedef struct {
    char *text;
    unsigned used, have;
} Quote;

typedef struct {
    char *name;
    void (*func) (char **);
} Funcs;

#define isQuote(s,n) (n.used && !strncmp(s, n.text, n.used))

#define write_quote(n) flt_puts(n.text, n.used, "")
#define wrong_quote(n) flt_puts(n.text, n.used, Error_attr)

static Quote leftquote, rightquote;
static Quote leftcmt, rightcmt;

static char *
SkipBlanks(char *src)
{
    while (isspace(CharOf(*src)))
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
parse_arglist(char *name, char *s, char ***args, int *parens)
{
    char *r;
    char *v;
    int quoted;
    unsigned count;
    unsigned used;
    char *t;
    int processing;

    t = SkipBlanks(s);
    if (*parens == 0) {
	quoted = 0;
	count = 0;
	used = 0;
	if (*t == L_PAREN) {
	    processing = 1;
	    *args = (char **) do_alloc((char *) 0, sizeof(*args) *
				       (count + 4), &used);
	    (*args)[count++] = strmalloc(name);
	    (*args)[count] = 0;
	    t++;
	} else {
	    processing = 0;
	}
    } else {
	processing = 1;
	quoted = (*parens > 0);
	count = count_of(*args);
	used = sizeof(*args) * (count + 2);	/* guess... */
    }

    if (processing) {
	for (;;) {
	    r = t = SkipBlanks(t);
	    while ((*t != ',') && (*t != R_PAREN) && *t)
		t++;
	    if (*t == ',' || *t == R_PAREN) {
		v = (char *) malloc(1 + t - r);
		if (t != r)
		    strncpy(v, r, t - r);
		v[t - r] = 0;
		*args = (char **) do_alloc((char *) (*args), sizeof(*args) *
					   (count + 2), &used);
		(*args)[count++] = v;
	    }
	    (*args)[count] = 0;
	    if (isQuote(t, leftquote)) {
		t += leftquote.used;
		quoted = 1;
	    } else if (isQuote(t, rightquote)) {
		t += rightquote.used;
		quoted = 0;
	    } else if (*t == R_PAREN && !quoted) {
		t++;
		*parens = 0;
		break;
	    } else if (!*t) {
		*parens = quoted ? 1 : -1;
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

static Funcs *
our_directive(char *name)
{
    static Funcs table[] =
    {
	{
	    "changequote", ChangeQuote
	},
	{
	    "changecom", ChangeComment
	},
    };
    Funcs *result = 0;
    size_t n;

    for (n = 0; n < sizeof(table) / sizeof(table[0]); n++) {
	if (!strcmp(name, table[n].name)) {
	    result = table + n;
	    break;
	}
    }
    return result;
}

static void
handle_directive(char ***args, int *parens)
{
    Funcs *ptr;

    if (*args != 0 && *parens == 0) {
	if ((ptr = our_directive((*args)[0])) != 0) {
	    ptr->func(*args + 1);
	}
	free_arglist(*args);
	*parens = 0;
	*args = 0;
    }
}

/*
 * We really do not need to parse argument lists except for those that modify
 * the delimiters we'll use for subsequent parsing.
 */
static char *
parse_directive(char *name, char *s, char ***args, int *parens)
{
    if (our_directive(name) != 0) {
	s = parse_arglist(name, s, args, parens);
	handle_directive(args, parens);
    }
    return s;
}

static char *
extract_identifier(char *s, char ***args, int *parens)
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
	s = parse_directive(name, s, args, parens);
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
	    done = !isdigit(CharOf(*s)) || (*s == '8') || (*s == '9');
	    break;
	case 10:
	    done = !isdigit(CharOf(*s));
	    break;
	case 16:
	    done = !isxdigit(CharOf(*s));
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
	write_quote(rightquote);
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
    if (before) {
	insert_keyword(NAME_L_QUOTE, L_QUOTE, 1);
	insert_keyword(NAME_R_QUOTE, R_QUOTE, 1);
	insert_keyword(NAME_L_CMT, L_CMT, 1);
	insert_keyword(NAME_R_CMT, R_CMT, 1);
    }
}

static void
do_filter(FILE * input GCC_UNUSED)
{
    static unsigned used;
    static char *line;

    char *s;
    char **args;
    int literal, comment, parens;

    new_quote(&leftquote, class_attr(NAME_L_QUOTE));
    new_quote(&rightquote, class_attr(NAME_R_QUOTE));
    new_quote(&leftcmt, class_attr(NAME_L_CMT));
    new_quote(&rightcmt, class_attr(NAME_R_CMT));

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT);
    Literal_attr = class_attr(NAME_LITERAL);
    Number_attr = class_attr(NAME_NUMBER);

    args = 0;
    literal = 0;
    comment = 0;
    parens = 0;

    while (flt_gets(&line, &used) != NULL) {
	s = line;
	while (*s) {
	    if (parens != 0) {
		s = parse_directive(args[0], s, &args, &parens);
	    } else if (literal) {
		/*
		 * Quoted literals can nest, and override comments
		 */
		s = write_literal(s, &literal);
	    } else if (isQuote(s, leftquote)) {
		write_quote(leftquote);
		s += leftquote.used;
		literal++;
		s = write_literal(s, &literal);
	    } else if (isQuote(s, rightquote)) {
		wrong_quote(rightquote);
		s += rightquote.used;
		if (--literal > 0)
		    s = write_literal(s, &literal);
		else
		    literal = 0;
	    } else if (comment) {
		/*
		 * Comments don't nest
		 */
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
		s = extract_identifier(s, &args, &parens);
	    } else if (isdigit(CharOf(*s))) {
		s = write_number(s);
	    } else {
		flt_putc(*s++);
	    }
	}
    }
}
