/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sed-filt.c,v 1.14 2001/12/06 01:55:46 tom Exp $
 *
 * Filter to add vile "attribution" sequences to sed scripts.
 */

#include <filters.h>

DefineFilter("sed");

#define ESC '\\'

#define L_CURLY '{'
#define R_CURLY '}'

#define L_ROUND '('
#define R_ROUND ')'

#define L_SQUARE '['
#define R_SQUARE ']'

#define isSlash(c) ((c) == '/' || (c) == ESC)

typedef enum {
    LeadingBlanks
    ,OptionalRCurly
    ,OptionalLabel
    ,OptionalComment
    ,OptionalAddresses
    ,BlanksAfterAddresses
    ,CommandChar
    ,ExpectSubsParams
    ,ExpectTransParams
    ,ExpectLabel
    ,ExpectText
    ,AfterCommandChar
} States;

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Literal_attr;
static char *Number_attr;

static char *
SkipBlanks(char *s)
{
    while (isspace(CharOf(*s))) {
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

    while (len > 0 && isspace(CharOf(s[len - 1])))
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
SkipRCurly(char *s)
{
    if (*s == R_CURLY) {
	flt_puts(s++, 1, Action_attr);
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
	while (!isspace(CharOf(*s)))	/* FIXME: can a label have punctuation? */
	    s++;

	tail = s;
	while (isspace(CharOf(*s)))
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

#define RE_NEST(var, left, right) \
	switch (ch) { \
	case left: var++; break; \
	case right: if (--var < 0) error = 1; break; \
	}

/* the first character is the delimiter we'll search for */
static char *
SkipPattern(char *s, int *done, int join)
{
    char *base = s;
    int error = 0;
    int delim = *s++;
    int curly = 0;
    int round = 0;
    int square = 0;
    int ch;

    *done = 0;
    error = 0;
    while (*s != 0) {
	if (*s == ESC) {
	    ch = *++s;
	    RE_NEST(round, L_ROUND, R_ROUND);
	    RE_NEST(curly, L_CURLY, R_CURLY);
	    if (ch != 0)
		s++;
	} else {
	    ch = *s++;
	    if (!square && ch == delim) {
		if (curly || round || square)
		    error = 1;
		*done = 1;
		break;
	    }
	    if (ch == L_SQUARE) {
		square++;
	    } else if (ch == R_SQUARE) {
		if (square)
		    square--;
	    }
	}
    }

    if (s != base) {
	if (error) {
	    flt_puts(base, s - base, Error_attr);
	} else {
	    if (!join)
		flt_puts(base, 1, Action_attr);
	    base++;
	    if (done)
		s--;
	    flt_puts(base, s - base, Literal_attr);
	    if (done)
		flt_puts(s++, 1, Action_attr);
	}
    }
    return s;
}

static char *
SkipTwoPatterns(char *s, int flags)
{
    char *base = s;
    int done;

    s = SkipPattern(s, &done, 0);
    s = SkipPattern(done ? s - 1 : s, &done, 1);

    if (flags) {
	base = s;
	while (*s != 0 && !isspace(CharOf(*s))) {
	    if (isdigit(CharOf(*s))
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
    return (isdigit(CharOf(*s)) || *s == '$' || isSlash(*s) || *s == ',');
}

static char *
SkipAddress(char *s, int *count)
{
    char *base = s;
    int done;

    if (*count) {
	if (*s == ',') {
	    flt_putc(*s++);
	} else {
	    return s;
	}
    }

    if (isdigit(CharOf(*s))) {
	while (isdigit(CharOf(*s)))
	    s++;
	flt_puts(base, s - base, Number_attr);
    } else if (*s == '$') {
	flt_puts(s++, 1, Literal_attr);
    } else if (isSlash(*s)) {
	if (*s == ESC) {
	    flt_puts(s++, 1, Action_attr);
	}
	s = SkipPattern(s, &done, 0);
    } else {
	s = SkipError(s);
    }
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
    int addresses;
    int escaped_newline;
    States state = LeadingBlanks;

    Action_attr = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Literal_attr = class_attr(NAME_LITERAL);
    Number_attr = class_attr(NAME_NUMBER);

    while (flt_gets(&line, &used) != NULL) {
	size_t len = strlen(s = line);

	escaped_newline = (len > 1 && s[len - 2] == ESC);

	while (*s) {
	    States next = LeadingBlanks;

	    /* we use a state-driven case to handle continuation-lines */
	    switch (state) {
	    case LeadingBlanks:
		if (*(s = SkipBlanks(s)) != 0)
		    next = OptionalRCurly;
		break;
	    case OptionalRCurly:
		if (*(s = SkipRCurly(s)) != 0)
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
		    if (*s == ',') {
			next = state;
		    } else {
			s = SkipBlanks(s);
			if (*s == '!')
			    flt_puts(s++, 1, Literal_attr);
		    }
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
		if (isalpha(CharOf(*s)) || *s == '=') {
		    switch (*s) {
		    case 's':
			next = ExpectSubsParams;
			break;
		    case 'y':
			next = ExpectTransParams;
			break;
		    case 'b':
		    case 't':
			next = ExpectLabel;
			break;
		    }
		    flt_puts(s++, 1, Ident_attr);
		} else if (*s == L_CURLY) {
		    flt_puts(s++, 1, Action_attr);
		    next = LeadingBlanks;
		} else {
		    flt_puts(s++, 1, Error_attr);
		}
		if (*s == 0)
		    next = LeadingBlanks;
		break;
	    case ExpectSubsParams:
	    case ExpectTransParams:
		s = SkipTwoPatterns(s, state == ExpectSubsParams);
		next = (*s != '\0') ? AfterCommandChar : LeadingBlanks;
		break;
	    case ExpectLabel:
		s = SkipRemaining(s, Ident2_attr);
		if (escaped_newline)
		    next = state;
		break;
	    case ExpectText:
		s = SkipRemaining(s, "");
		if (escaped_newline)
		    next = state;
		break;
	    case AfterCommandChar:
		if (*s == ';') {
		    flt_puts(s++, 1, Action_attr);
		    next = LeadingBlanks;
		} else {
		    s = SkipRemaining(s, "");
		    if (escaped_newline)
			next = state;
		}
		break;
	    }
	    state = next;
	}
    }
}
