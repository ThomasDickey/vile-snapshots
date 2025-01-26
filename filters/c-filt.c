/*
 * Program: A simple comment and keyword attributer for vile
 * Author : Jukka Keto, jketo@cs.joensuu.fi
 * Date   : 30.12.1994
 * Modifications:  kevin buettner and paul fox  2/95
 *		string literal ("Literal") support --  ben stoltz
 *		factor-out hashing and file I/O - tom dickey
 *
 * $Id: c-filt.c,v 1.103 2025/01/26 17:00:27 tom Exp $
 *
 * Usage: refer to vile.hlp and doc/filters.doc .
 *
 * Options:
 *	-j	java special cases
 *	-o	objc has '@' directives
 *	-p	suppress preprocessor support
 *	-s	javascript special cases
 *	-#	C# special cases
 */

#include <filters.h>

DefineOptFilter(c, "jops#");

#define UPPER(c) isalpha(CharOf(c)) ? toupper(CharOf(c)) : c

#define isIdent(c)  (isalpha(CharOf(c)) \
		     || (c) == '_' \
		     || (FltOptions('j') && (c) == '$') \
		     || (FltOptions('o') && (c) == '@'))
#define isNamex(c)  (isIdent(c) || isdigit(CharOf(c)))

#define isQuote(c)  ((c) == DQUOTE || (c) == SQUOTE || (FltOptions('j') && (c) == BQUOTE))

static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Literal_attr;
static char *Number_attr;
static char *Preproc_attr;

static char *
extract_identifier(char *s)
{
    static char *name;
    static size_t have;

    size_t need;
    const char *attr;
    char *base = s;
    int found = 0;

    while (isNamex(*s))
	s++;
    if (base != s) {
	need = (size_t) (s - base);
	if ((name = do_alloc(name, need, &have)) != NULL) {
	    strncpy(name, base, need);
	    name[need] = 0;
	    if ((attr = get_keyword_attr(name)) != NULL) {
		flt_puts(base, (int) need, attr);
		found = 1;
	    }
	}
	if (!found) {
	    flt_puts(base, (int) need, Ident_attr);
	}
#if NO_LEAKS
	free(name);
	name = NULL;
	have = 0;
#endif
    }
    return s;
}

static int
has_endofcomment(char *s)
{
    int i = 0;
    while (*s) {
	if (*s == '*' && *(s + 1) == '/') {
	    return (i + 2);
	}
	i += 1;
	s += 1;
    }
    return (0);
}

static int
has_endofliteral(char *s, int delim, int verbatim, int *escaped)
{				/* points to '"' */
    int i = 0;
    int result = -1;
    char *base = s;

    while (*s) {
	if (*s == delim) {
	    if (verbatim && s[1] == delim) {
		++i;
		++s;
	    } else {
		result = i + 2;	/* include quote chars */
		break;
	    }
	}
	if (!verbatim && s[0] == BACKSLASH && (s[1] == delim || s[1] == BACKSLASH)) {
	    ++i;
	    ++s;
	}
	++i;
	++s;
    }

    if (*escaped
	&& !verbatim
	&& (s > base)
	&& (s[1] == '\0')
	&& isreturn(s[0])
	&& (s[-1] != BACKSLASH)) {
	*escaped = 0;
    }

    return result;
}

static char *
skip_white(char *s)
{
    while (*s && isBlank(*s))
	flt_putc(*s++);
    return s;
}

static int
firstnonblank(char *tst, char *cmp)
{
    while (*cmp && isBlank(*cmp))
	cmp++;
    return (tst == cmp);
}

static void
write_comment(char *s, int len, int begin)
{
    char *t = s;
    char *nested;
    if (begin)
	t += 2;
    while ((nested = strstr(t, "/*")) != NULL && (nested - s) < len) {
	flt_puts(s, (int) (nested - s), Comment_attr);
	flt_error("nested comment");
	flt_puts(nested, 2, Error_attr);
	len -= (int) (nested + 2 - s);
	s = t = nested + 2;
    }
    flt_puts(s, len, Comment_attr);
}

static char *
write_escape(char *s, char *attr)
{
    char *base = s++;
    int want = ((*s == '0' || *s == 'x')
		? 3
		: ((FltOptions('j') && *s == 'u')
		   ? 4
		   : 1));
    while (want && *s != 0) {
	s++;
	want--;
    }
    flt_puts(base, (int) (s - base), attr);
    return s;
}

#define SkipDigit(radix,s) \
		(radix == 10 && isdigit(CharOf(*s))) || \
		(radix == 16 && isxdigit(CharOf(*s)))

#define BeginExponent(radix,ch) \
		(radix == 10 && ch == 'E') || \
		(radix == 16 && ch == 'P')

#define IsDigitX(c) (isNamex(c) || (c) == '.')

static char *
write_number(char *s)
{
    char *base = s;
    const char *attr = Number_attr;
    int radix = (*s == '0')
    ? ((s[1] == 'x' || s[1] == 'X') ? 16
       : (!isdigit(CharOf(s[1]))) ? 10 : 8) : 10;
    int state = 0;
    int done = 0;
    int found = isdigit(CharOf(*s)) && (radix != 16);
    int num_f = 0;
    int num_u = 0;
    int num_l = 0;
    int dot = (*s == '.');

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
	if ((radix == 10 || radix == 16) && *s == '.') {
	    dot++;
	    break;
	}
	found += !done;
    }
    if (radix == 10 || radix == 16) {
	while (state >= 0) {
	    int ch = UPPER(*s) ? toupper(CharOf(*s)) : *s;
	    switch (state) {
	    case 0:
		if (ch == '.') {
		    state = 1;
		} else if (BeginExponent(radix, ch)) {
		    state = 2;
		} else if (ch == 'F' || (FltOptions('j') && ch == 'D')) {
		    num_f++;
		    state = 3;
		} else if (ch == 'L') {
		    num_l++;
		    state = 4;
		} else if (ch == 'U') {
		    num_u++;
		    state = 4;
		} else {
		    state = -1;
		}
		break;
	    case 1:		/* after decimal point */
		while (SkipDigit(radix, s)) {
		    s++;
		    found++;
		}
		ch = UPPER(*s);
		state = -1;
		if (BeginExponent(radix, ch)) {
		    state = 2;
		} else if (ch == 'F' || (FltOptions('j') && ch == 'D')) {
		    num_f++;
		    state = 3;
		} else if (ch == 'L') {
		    num_l++;
		    state = 3;
		}
		break;
	    case 2:		/* after exponent letter */
		if (ch == '+' || ch == '-')
		    s++;
		while (SkipDigit(radix, s))
		    s++;
		ch = UPPER(*s);
		/* FALLTHRU */
	    case 3:
		if (ch == 'F' || (FltOptions('j') && ch == 'D')) {
		    if (++num_f > 1)
			state = -1;
		} else if (ch == 'L') {
		    if (++num_l > 1)
			state = -1;
		} else {
		    state = -1;
		}
		break;
	    case 4:
		if (ch == 'L') {
		    if (++num_l > 2)
			state = -1;
		} else if (ch == 'U') {
		    if (++num_u > 1)
			state = -1;
		} else {
		    state = -1;
		}
		break;
	    }
	    if (state >= 0)
		s++;
	}
    } else {
	for (;;) {
	    int ch = UPPER(*s);
	    if (ch == 'L') {
		if (++num_l > 2)
		    break;
	    } else if (ch == 'U') {
		if (++num_u > 1)
		    break;
	    } else {
		break;
	    }
	    s++;
	}
    }
    if (!found || IsDigitX(*s) || dot > 1) {	/* something is run-on to a number */
	while (IsDigitX(*s))
	    s++;
	if (dot > 0
	    && (s - base) == 3
	    && !strncmp(base, "...", (size_t) 3)) {
	    attr = "";
	} else {
	    flt_error("illegal number");
	    attr = Error_attr;
	}
    } else {
    }
    flt_puts(base, (int) (s - base), attr);
    return s;
}

static char *
write_literal(char *s, int *literal, int *verbatim, int escaped, int skip)
{
    int c_length = has_endofliteral(s + skip, *literal, *verbatim, &escaped);
    if (c_length < 0) {
	c_length = (int) strlen(s);
    } else {
	c_length -= (1 - skip);
	*literal = 0;
    }
    if (escaped || *verbatim) {
	if (FltOptions('j')) {
	    int l = 0;
	    char *p = s;
	    while (l < c_length) {
		if (*(s + l) == '$' && *(s + l + 1) == '{') {
		    if (l > 0) {
			flt_puts(p, l - (int) (p - s), Literal_attr);
			p = s + l;
		    }
		    while (++l < c_length && *(s + l) != '}' && *(s + l) != '$') ;
		    if (*(s + l) == '}') {
			flt_puts(p, ++l - (int) (p - s), Ident2_attr);
		    } else {
			flt_error("expected a '}'");
			flt_puts(p, l - (int) (p - s), Error_attr);
		    }
		    p = s + l;
		} else {
		    l++;
		}
	    }
	    flt_puts(p, l - (int) (p - s), Literal_attr);
	} else {
	    flt_puts(s, c_length, Literal_attr);
	}
    } else {
	flt_error("expected an escape");
	flt_puts(s, c_length, Error_attr);
	*literal = 0;
    }
    s += c_length;
    if (!*literal) {
	*verbatim = 0;
    }
    return s;
}

/* Java and JavaScript */
static char *
write_wchar(char *s)
{
    int n;
    for (n = 2; n <= 6; ++n) {
	if (!isxdigit(CharOf(s[n])))
	    break;
    }
    if (n == 6) {
	flt_puts(s, n, Literal_attr);
    } else {
	flt_error("expected 6 chars after escape");
	flt_puts(s, n, Error_attr);
    }
    return s + n;
}

/* JavaScript */
static char *
write_regexp(char *s)
{
    char *base = s;
    int escape = 0;
    int adjust = 0;
    int ranges = 0;

    do {
	int ch = *s;
	if (escape) {
	    ++s;
	    escape = 0;
	} else {
	    if (ch == L_BLOCK)
		ranges = 1;
	    if (ch == R_BLOCK)
		ranges = 0;
	    if (ch == BACKSLASH)
		escape = 1;
	    ++s;
	}
    } while ((*s != '\0') && (escape || ranges || (!ranges && (*s != '/'))));
    if (*s == '/') {
	++s;
	adjust = 1;
    }
    /*
     * g = global
     * i = case-insensitive
     * m = multiline
     * s = dot matches newline
     * u = unicode
     * y = sticky search, matches starting point.
     */
    while (*s != '\0' && strchr("gimsuy", *s) != NULL) {
	++s;
	adjust = 1;
    }
    if (!adjust)
	++s;
    flt_puts(base, (int) (s - base), Literal_attr);
    return s;
}

static char *
parse_prepro(char *s, int *literal)
{
    const char *attr;
    char *ss = s + 1;
    char *tt;
    int isinclude = 0;
    int save;

    while (isBlank(*ss))
	ss++;
    for (tt = ss; isNamex(*tt); tt++)
	/*LOOP */ ;
    save = *tt;
    *tt = 0;
    isinclude = !strcmp(ss, "include");
    if (get_keyword_attr(ss) == NULL) {
	char *dst = NULL;
	if (strtol(ss, &dst, 10) != 0 && dst != NULL && *dst == 0) {
	    flt_puts(s, (int) (ss - s), Preproc_attr);
	    flt_puts(ss, (int) (tt - ss), Number_attr);
	} else {
	    flt_error("unknown preprocessor directive");
	    flt_puts(s, (int) (tt - s), Error_attr);
	}
	s = tt;
    }
    attr = Preproc_attr;
    *tt = (char) save;

    if (isinclude) {		/* eat filename as well */
	flt_puts(s, (int) (tt - s), attr);
	ss = tt;
	while (isBlank(*tt))
	    tt++;
	flt_puts(ss, (int) (tt - ss), "");
	s = ss = tt;
	attr = Literal_attr;
	while (*tt != 0) {
	    int ch = *tt++;
	    if (*ss == '<') {
		*literal = 1;
		if (ch == '>') {
		    *literal = 0;
		    break;
		}
	    } else if (*ss == '"') {
		*literal = 1;
		if (ch == '"' && (tt != ss + 1)) {
		    *literal = 0;
		    break;
		}
	    } else if (isspace(ch)) {
		break;
	    } else {
		if (!isNamex(ch)) {	/* FIXME: should allow any macro */
		    tt--;
		    break;
		}
	    }
	}
    }
    flt_puts(s, (int) (tt - s), attr);
    return tt;
}

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

    char *s;
    int c_length;
    int comment;
    int escaped;
    int literal;
    int verbatim;
    int maybeRX;
    int was_esc;
    unsigned len;

    (void) input;

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Literal_attr = class_attr(NAME_LITERAL);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = FltOptions('p') ? Error_attr : class_attr(NAME_PREPROC);

    comment = 0;
    literal = 0;
    verbatim = 0;
    maybeRX = 0;
    was_esc = 0;

    while (flt_gets(&line, &used) != NULL) {
	escaped = was_esc;
	was_esc = 0;
	s = line;
	if (literal) {
	    s = write_literal(s, &literal, &verbatim, escaped, 0);
	    was_esc = (literal == BQUOTE);
	}
	s = skip_white(s);
	while (*s) {
	    if (!comment && *s == '/' && *(s + 1) == '*') {
		c_length = has_endofcomment(s + 2);
		if (c_length == 0) {	/* Comment continues to the next line */
		    c_length = (int) strlen(s);
		    comment += 1;
		} else {
		    c_length += 2;
		}
		write_comment(s, c_length, 1);
		s = s + c_length;
		continue;
	    } else if (!comment && *s == '/' && *(s + 1) == '/') {
		/* C++ comments */
		c_length = (int) strlen(s);
		write_comment(s, c_length, 1);
		break;
	    } else if (!escaped && !comment && *s == '#' && firstnonblank(s, line)
		       && set_symbol_table("cpre")) {
		s = parse_prepro(s, &literal);
		set_symbol_table(default_table);
		maybeRX = 0;
	    } else if (comment && *s) {
		if ((c_length = has_endofcomment(s)) > 0) {
		    write_comment(s, c_length, 0);
		    s = s + c_length;
		    comment -= 1;
		    if (comment < 0)
			comment = 0;
		} else {	/* Whole line belongs to comment */
		    c_length = (int) strlen(s);
		    write_comment(s, c_length, 0);
		    s = s + c_length;
		}
	    } else if (*s == BACKSLASH) {
		if (s[1] == '\n') {
		    flt_putc(*s++);	/* escaped newline - ok */
		} else if (FltOptions('j') && (s[1] == 'u')) {
		    /* FIXME: a Unicode value is potentially part of an
		     * identifier.  For now, simply color it like a string.
		     */
		    s = write_wchar(s);
		} else {
		    flt_error("illegal escape");
		    s = write_escape(s, Error_attr);
		}
		maybeRX = 0;
	    } else if (FltOptions('#') && *s == '@' && s[1] == DQUOTE) {
		verbatim = 1;
		flt_putc(*s++);
	    } else if (isQuote(*s)) {
		literal = (literal == 0) ? *s : 0;
		if (literal) {
		    s = write_literal(s, &literal, &verbatim, 1, 1);
		    was_esc = (literal == BQUOTE);
		}
		maybeRX = 0;
		if (FltOptions('j'))
		    was_esc = (literal == BQUOTE);
	    } else if (isIdent(*s)) {
		s = extract_identifier(s);
		maybeRX = 0;
	    } else if (isdigit(CharOf(*s))
		       || (*s == '.'
			   && (isdigit(CharOf(s[1])) || s[1] == '.'))) {
		s = write_number(s);
		maybeRX = 0;
	    } else if (*s == '#') {
		char *t = s;
		while (*s == '#')
		    s++;
		flt_puts(t, (int) (s - t), ((s - t) > 2) ?
			 Error_attr : Preproc_attr);
		maybeRX = 0;
	    } else if (ispunct(CharOf(*s))) {
		if (FltOptions('s') && maybeRX && *s == '/') {
		    s = write_regexp(s);
		} else {
		    /*
		     * A JavaScript regular expression can appear to the
		     * right of an assignment, or as a parameter to new()
		     * or match().
		     */
		    maybeRX = (*s == '=' ||
			       *s == ':' ||
			       *s == L_PAREN ||
			       *s == ',');
		    flt_putc(*s++);
		}
	    } else {
		if (!isspace(CharOf(*s)))
		    maybeRX = 0;
		flt_putc(*s++);
	    }
	}
	len = (unsigned) (s - line);
	if (len > 2 && !strcmp(line + len - 2, "\\\n"))
	    was_esc = 1;
    }
}

#if NO_LEAKS
static void
free_filter(void)
{
}
#endif
