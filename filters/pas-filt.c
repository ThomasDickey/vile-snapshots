/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/pas-filt.c,v 1.15 2002/12/15 20:02:01 tom Exp $
 *
 * Markup a Pascal file, for highlighting with vile.
 */

#include <filters.h>

DefineFilter("pas");

#define isNameBegin(c)   (isalpha(CharOf(c)) || (c) == '_')
#define isNameExtra(c)   (isalnum(CharOf(c)) || (c) == '_')

static char *comment_attr;
static char *literal_attr;

static char *
extract_identifier(char *s)
{
    static char *name;
    static unsigned have;

    unsigned need;
    char *base;

    base = s;
    while (isNameExtra(*s))
	s++;

    if (base != s) {
	need = (s - base);
	name = do_alloc(name, need, &have);
	for (s = base; isNameExtra(*s); s++) {
	    name[s - base] = ((isalpha(CharOf(*s)) && isupper(CharOf(*s)))
			      ? tolower(*s)
			      : *s);
	}
	name[s - base] = 0;
	flt_puts(base, s - base, keyword_attr(name));
    }
    return s;
}

static int
has_endofcomment(char *s)
{
    int i = 0;
    while (*s) {
	if (*s == R_CURLY) {
	    return (i + 1);
	}
	i += 1;
	s += 1;
    }
    return (0);
}

static int
has_endofliteral(char *s)	/* points past beginning SQUOTE */
{
    int i = 0;

    while (s[i]) {
	if (s[i] == SQUOTE) {
	    if (s[i + 1] == SQUOTE) {
		i += 2;
	    } else {
		return (i);	/* points before ending SQUOTE */
	    }
	}
	++i;
    }
    return (0);
}

static char *
skip_white(char *s)
{
    while (*s && isBlank(*s))
	flt_putc(*s++);
    return s;
}

static char *
write_literal(char *s)
{
    int c_length = has_endofliteral(s);
    if (c_length == 0)
	c_length = strlen(s);
    flt_puts(s, c_length, literal_attr);
    s += c_length;
    if (*s == SQUOTE)
	flt_putc(*s++);
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
    int comment, c_length;

    comment_attr = class_attr(NAME_COMMENT);
    literal_attr = class_attr(NAME_LITERAL);
    comment = 0;

    while (flt_gets(&line, &used) != NULL) {
	s = line;
	s = skip_white(s);
	while (*s) {
	    if (!comment && *s == L_CURLY) {
		c_length = has_endofcomment(s);
		if (c_length == 0) {	/* Comment continues to the next line */
		    c_length = strlen(s);
		    comment = 1;
		}
		flt_puts(s, c_length, comment_attr);
		s = s + c_length;
	    }
	    if (comment && *s) {
		if ((c_length = has_endofcomment(s)) > 0) {
		    flt_puts(s, c_length, comment_attr);
		    s = s + c_length;
		    comment = 0;
		} else {	/* Whole line belongs to comment */
		    c_length = strlen(s);
		    flt_puts(s, c_length, comment_attr);
		    s = s + c_length;
		}
	    } else if (*s == SQUOTE) {
		flt_putc(*s++);
		s = write_literal(s);
	    } else if (*s) {
		if (isNameBegin(*s)) {
		    s = extract_identifier(s);
		} else {
		    flt_putc(*s++);
		}
	    }
	}
    }
}
