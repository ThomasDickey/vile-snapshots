/*
 * Program: A simple comment and keyword attributer for vile
 * Author : Jukka Keto, jketo@cs.joensuu.fi
 * Date   : 30.12.1994
 * Modifications:  kevin buettner and paul fox  2/95
 *		string literal ("Literal") support --  ben stoltz
 *		factor-out hashing and file I/O - tom dickey
 *
 * $Header: /users/source/archives/vile.vcs/filters/RCS/c-filt.c,v 1.64 2002/12/17 02:02:06 tom Exp $
 *
 * Usage: refer to vile.hlp and doc/filters.doc .
 */

#include <filters.h>

DefineOptFilter("c", "p");

#define UPPER(c) isalpha(CharOf(c)) ? toupper(CharOf(c)) : c

#define isIdent(c)  (isalpha(CharOf(c)) || (c) == '_')
#define isNamex(c)  (isalnum(CharOf(c)) || (c) == '_')

#define isQuote(c)  ((c) == DQUOTE || (c) == SQUOTE)

static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Literal_attr;
static char *Number_attr;
static char *Preproc_attr;

static char *
extract_identifier(char *s)
{
    static char *name;
    static unsigned have;

    unsigned need;
    char *attr;
    char *base = s;

    while (isNamex(*s))
	s++;
    if (base != s) {
	need = s - base;
	name = do_alloc(name, need, &have);
	strncpy(name, base, need);
	name[need] = 0;
	if ((attr = keyword_attr(name)) != 0) {
	    flt_puts(base, need, attr);
	} else {
	    flt_puts(base, need, Ident_attr);
	}
#ifdef NO_LEAKS
	free(name);
	name = 0;
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
has_endofliteral(char *s, int delim)
{				/* points to '"' */
    int i = 0;
    while (*s) {
	if (*s == delim)
	    return (i);
	if (s[0] == BACKSLASH && (s[1] == delim || s[1] == BACKSLASH)) {
	    ++i;
	    ++s;
	}
	++i;
	++s;
    }
    return (-1);
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
    while ((nested = strstr(t, "/*")) != 0 && (nested - s) < len) {
	flt_puts(s, nested - s, Comment_attr);
	flt_puts(nested, 2, Error_attr);
	len -= (nested + 2 - s);
	s = t = nested + 2;
    }
    flt_puts(s, len, Comment_attr);
}

static char *
write_escape(char *s, char *attr)
{
    char *base = s++;
    int want = (*s == '0' || *s == 'x') ? 3 : 1;
    while (want && *s != 0) {
	s++;
	want--;
    }
    flt_puts(base, s - base, attr);
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
    char *attr = Number_attr;
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
	    done = 1;
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
		} else if (ch == 'F') {
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
		} else if (ch == 'F') {
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
		if (ch == 'F') {
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
	    && !strncmp(base, "...", 3)) {
	    attr = "";
	} else {
	    attr = Error_attr;
	}
    } else {
    }
    flt_puts(base, s - base, attr);
    return s;
}

static char *
write_literal(char *s, int *literal, int escaped)
{
    int c_length = has_endofliteral(s, *literal);
    if (c_length < 0)
	c_length = strlen(s);
    else
	*literal = 0;
    flt_puts(s, c_length, escaped ? Literal_attr : Error_attr);
    s += c_length;
    if (!*literal)
	flt_putc(*s++);
    return s;
}

static char *
parse_prepro(char *s, int *literal)
{
    char *attr;
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
    if ((attr = keyword_attr(ss)) == 0) {
	char *dst = 0;
	if (strtol(ss, &dst, 10) != 0 && dst != 0 && *dst == 0) {
	    flt_puts(s, ss - s, Preproc_attr);
	    flt_puts(ss, tt - ss, Number_attr);
	} else {
	    flt_puts(s, tt - s, Error_attr);
	}
	s = tt;
    }
    attr = Preproc_attr;
    *tt = save;

    if (isinclude) {		/* eat filename as well */
	flt_puts(s, tt - s, attr);
	ss = tt;
	while (isBlank(*tt))
	    tt++;
	flt_puts(ss, tt - ss, "");
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
    flt_puts(s, tt - s, attr);
    return tt;
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
    int comment, c_length, literal, escaped, was_esc;
    unsigned len;

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT);
    Literal_attr = class_attr(NAME_LITERAL);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = flt_options['p'] ? Error_attr : class_attr(NAME_PREPROC);

    comment = 0;
    literal = 0;
    was_esc = 0;

    while (flt_gets(&line, &used) != NULL) {
	escaped = was_esc;
	was_esc = 0;
	s = line;
	if (literal)
	    s = write_literal(s, &literal, escaped);
	s = skip_white(s);
	while (*s) {
	    if (!comment && *s == '/' && *(s + 1) == '*') {
		c_length = has_endofcomment(s + 2);
		if (c_length == 0) {	/* Comment continues to the next line */
		    c_length = strlen(s);
		    comment += 1;
		} else {
		    c_length += 2;
		}
		write_comment(s, c_length, 1);
		s = s + c_length;
		continue;
	    } else if (!comment && *s == '/' && *(s + 1) == '/') {
		/* C++ comments */
		c_length = strlen(s);
		write_comment(s, c_length, 1);
		break;
	    } else if (!escaped && !comment && *s == '#' && firstnonblank(s, line)
		       && set_symbol_table("cpre")) {
		s = parse_prepro(s, &literal);
		set_symbol_table(filter_def.filter_name);
	    } else if (comment && *s) {
		if ((c_length = has_endofcomment(s)) > 0) {
		    write_comment(s, c_length, 0);
		    s = s + c_length;
		    comment -= 1;
		    if (comment < 0)
			comment = 0;
		} else {	/* Whole line belongs to comment */
		    c_length = strlen(s);
		    write_comment(s, c_length, 0);
		    s = s + c_length;
		}
	    } else if (*s == BACKSLASH) {
		if (s[1] != '\n') {
		    s = write_escape(s, Error_attr);
		} else {
		    /* escaped newline - ok */
		    flt_putc(*s++);
		}
	    } else if (isQuote(*s)) {
		literal = (literal == 0) ? *s : 0;
		flt_putc(*s++);
		if (literal) {
		    s = write_literal(s, &literal, 1);
		}
	    } else if (isIdent(*s)) {
		s = extract_identifier(s);
	    } else if (isdigit(CharOf(*s))
		       || (*s == '.'
			   && (isdigit(CharOf(s[1])) || s[1] == '.'))) {
		s = write_number(s);
	    } else if (*s == '#') {
		char *t = s;
		while (*s == '#')
		    s++;
		flt_puts(t, s - t, ((s - t) > 2) ?
			 Error_attr : Preproc_attr);
	    } else {
		flt_putc(*s++);
	    }
	}
	len = s - line;
	if (len > 2 && !strcmp(line + len - 2, "\\\n"))
	    was_esc = 1;
    }
}
