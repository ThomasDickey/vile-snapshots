/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sed-filt.c,v 1.6 2000/08/28 02:14:20 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of m4 
 * input text.  This is in C rather than LEX because M4's quoting delimiters
 * change by embedded directives, to simplify inclusion of brackets.
 */

#include <filters.h>

DefineFilter("sed");

#define ESC '\\'

#define L_BRACE '{'
#define R_BRACE '}'

#define isSlash(c) ((c) != 0 && strchr("/@%", c) != 0)

typedef enum {
    LeadingBlanks,
    OptionalRBrace,
    OptionalLabel,
    OptionalComment,
    OptionalAddresses,
    BlanksAfterAddresses,
    CommandChar,
    ExpectSubsParams,
    ExpectTransParams,
    ExpectLabel,
    ExpectText,
    AfterCommandChar,
} States;

static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Literal_attr;
static char *Number_attr;

static char *
SkipBlanks(char *s)
{
    while (isspace(*s)) {
	flt_putc(*s++);
    }
    return s;
}

/* remaining nonblanks on the line are unexpected */
static char *
SkipError(char *s)
{
    char *base = s;
    size_t len = strlen(s);

    while (len > 0 && isspace(s[len - 1]))
	len--;
    flt_puts(base, len, Error_attr);
    return SkipBlanks(base + len);
}

static char *
SkipRemaining(char *s, char *attr)
{
    char *base = s;
    size_t len = strlen(s);

    s += len;
    if (attr != Comment_attr && len > 1 && s[-2] == ESC) {
	flt_puts(base, s - base - 2, attr);
	flt_puts(s - 2, 1, Literal_attr);
    } else {
	flt_puts(base, s - base - 1, attr);
    }
    flt_putc('\n');
    return s;
}

static char *
SkipRBrace(char *s)
{
    if (*s == R_BRACE) {
	flt_puts(s++, 1, Literal_attr);
	s = SkipBlanks(s);
	s = SkipError(s);
    }
    return s;
}

static char *
SkipComment(char *s)
{
    if (*s == '#') {
	s = SkipRemaining(s, Comment_attr);
    }
    return s;
}

static char *
SkipLabel(char *s)
{
    if (*s == ':') {
	char *base;
	char *tail;

	flt_puts(s++, 1, Ident_attr);

	s = SkipBlanks(s);
	base = s;
	while (!isspace(*s))	/* FIXME: can a label have punctuation? */
	    s++;

	tail = s;
	while (isspace(*s))
	    s++;

	if (*s || tail == base) {
	    s = SkipRemaining(base, Error_attr);
	} else {
	    flt_puts(base, tail - base, Ident2_attr);
	    flt_puts(tail, s - tail, "");
	}
    }
    return s;
}

/* the first character is the delimiter we'll search for */
static char *
SkipPattern(char *s, int *done, int *errs)
{
    int delim = *s++;
    int level = 0;
    int ch;

    *done = 0;
    *errs = 0;
    while (*s != 0) {
	if (*s == ESC) {
	    switch (ch = *++s) {
	    case '(':
		level++;
		break;
	    case ')':
		if (--level < 0)
		    *errs = 1;
		break;
	    }
	    if (ch != 0)
		s++;
	} else if (*s++ == delim) {
	    if (level != 0)
		*errs = 1;
	    *done = 1;
	    break;
	}
    }
    return s;
}

static char *
SkipTwoPatterns(char *s, int flags)
{
    char *base = s;
    char *base2;
    int done;
    int errs;

    s = SkipPattern(s, &done, &errs);
    base2 = done ? s - 1 : s;
    flt_puts(base, base2 - base, errs ? Error_attr : Literal_attr);

    s = SkipPattern(base2, &done, &errs);
    flt_puts(base2, s - base2, errs ? Error_attr : Literal_attr);

    if (flags) {
	base = s;
	while (*s != 0 && !isspace(*s)) {
	    if (isdigit(*s)
		|| *s == 'g'
		|| *s == 'p')
		s++;
	    else if (*s == 'w') {
		s += strlen(s);
	    }
	}
	flt_puts(base, s - base, Ident2_attr);
	s = SkipBlanks(s);
	s = SkipError(s);
    }
    return s;
}

static int
HaveAddress(char *s)
{
    return (isdigit(*s) || *s == '$' || isSlash(*s) || *s == ',');
}

static char *
SkipAddress(char *s, int *count)
{
    char *base = s;
    char *attr = "";
    int done;
    int errs;

    if (*count) {
	if (*s == ',') {
	    flt_putc(*s++);
	} else {
	    return s;
	}
    }

    if (isdigit(*s)) {
	attr = Number_attr;
	while (isdigit(*s))
	    s++;
    } else if (*s == '$') {
	attr = Literal_attr;
	s++;
    } else if (isSlash(*s)) {
	attr = Literal_attr;
	s = SkipPattern(s, &done, &errs);
	if (errs)
	    attr = Error_attr;
    } else {
	attr = Error_attr;
	s += strlen(s);
    }
    flt_puts(base, s - base, attr);
    *count += 1;
    return s;
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE * input GCC_UNUSED)
{
    static unsigned used;
    static char *line;

    char *s;
    int literal;
    int addresses;
    int escaped_newline;
    States state = LeadingBlanks;

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Literal_attr = class_attr(NAME_LITERAL);
    Number_attr = class_attr(NAME_NUMBER);

    literal = 0;

    while (flt_gets(&line, &used) != NULL) {
	size_t len = strlen(s = line);

	escaped_newline = (len > 1 && s[len - 2] == ESC);

	while (*s) {
	    States next = LeadingBlanks;

	    /* we use a state-driven case to handle continuation-lines */
	    switch (state) {
	    case LeadingBlanks:
		if (*(s = SkipBlanks(s)) != 0)
		    next = OptionalRBrace;
		break;
	    case OptionalRBrace:
		if (*(s = SkipRBrace(s)) != 0)
		    next = OptionalLabel;
		break;
	    case OptionalLabel:
		if (*(s = SkipLabel(s)) != 0)
		    next = OptionalComment;
		break;
	    case OptionalComment:
		if (*(s = SkipComment(s)) != 0) {
		    next = OptionalAddresses;
		    addresses = 0;
		}
		break;
	    case OptionalAddresses:
		if (HaveAddress(s)) {
		    s = SkipAddress(s, &addresses);
		    if (*s == ',')
			next = state;
		    if (*s == '!')
			flt_puts(s++, 1, Literal_attr);
		} else {
		    next = BlanksAfterAddresses;
		}
		break;
	    case BlanksAfterAddresses:
		if (*(s = SkipBlanks(s)) != 0)
		    next = CommandChar;
		break;
	    case CommandChar:
		next = AfterCommandChar;
		if (isalpha(*s) || *s == '=') {
		    switch (*s) {
		    case 's':
			next = ExpectSubsParams;
		    case 'y':
			next = ExpectTransParams;
			break;
		    case 'b':
		    case 't':
			next = ExpectLabel;
			break;
		    }
		    flt_puts(s++, 1, Ident_attr);
		} else if (*s == L_BRACE) {
		    flt_puts(s++, 1, Literal_attr);
		} else {
		    flt_puts(s++, 1, Error_attr);
		}
		if (*s == 0)
		    next = LeadingBlanks;
		break;
	    case ExpectSubsParams:
	    case ExpectTransParams:
		s = SkipTwoPatterns(s, state == ExpectSubsParams);
		next = AfterCommandChar;
		break;
	    case ExpectLabel:
		s = SkipRemaining(s, Ident2_attr);
		if (escaped_newline)
		    next = state;
		break;
	    case ExpectText:
	    case AfterCommandChar:
		s = SkipRemaining(s, "");
		if (escaped_newline)
		    next = state;
		break;
	    }
	    state = next;
	}
    }
}
