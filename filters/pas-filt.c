/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/pas-filt.c,v 1.11 2000/02/28 11:58:59 tom Exp $
 *
 * Markup a Pascal file, for highlighting with vile.
 */

#include <filters.h>

DefineFilter("pas");

#define isNameBegin(c)   (isalpha(c) || (c) == '_')
#define isNameExtra(c)   (isalnum(c) || (c) == '_')
#define isBlank(c)  ((c) == ' ' || (c) == '\t')

#define L_CURL '{'
#define R_CURL '}'
#define QUOTE  '\''

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
	    name[s-base] = (isalpha(*s) && isupper(*s))
			    ? tolower(*s)
			    : *s;
	}
	name[s - base] = 0;
	flt_puts(base, s - base, keyword_attr(name));
    }
    return s;
}

static int
has_endofcomment(char *s)
{
    int i=0;
    while (*s) {
	if (*s == R_CURL) {
	    return(i+1);
	}
	i += 1;
	s += 1;
    }
    return(0);
}

static int
has_endofliteral(char *s)	/* points past beginning QUOTE */
{
    int i = 0;

    while (s[i]) {
	if (s[i] == QUOTE) {
	    if (s[i+1] == QUOTE) {
		i += 2;
	    } else {
	        return (i);	/* points before ending QUOTE */
	    }
	}
	++i;
    }
    return(0);
}

static char *
skip_white(char *s)
{
    while(*s && isBlank(*s))
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
    if (*s == QUOTE)
    	flt_putc(*s++);
    return s;
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
    static unsigned used;
    static char *line;
    char *s;
    int comment,c_length;

    comment_attr = class_attr(NAME_COMMENT);
    literal_attr = class_attr(NAME_LITERAL);
    comment = 0;

    while (readline(input, &line, &used) != NULL) {
	s = line;
	s = skip_white(s);
	while (*s) {
	    if (!comment && *s == L_CURL) {
		c_length = has_endofcomment(s);
		if (c_length == 0) { /* Comment continues to the next line */
		    c_length = strlen(s);
		    comment = 1;
		}
		flt_puts(s, c_length, comment_attr);
		s = s + c_length ;
	    }
	    if (comment && *s) {
		if ((c_length = has_endofcomment(s)) > 0) {
		    flt_puts(s, c_length, comment_attr);
		    s = s + c_length ;
		    comment = 0;
		} else { /* Whole line belongs to comment */
		    c_length = strlen(s);
		    flt_puts(s, c_length, comment_attr);
		    s = s + c_length;
		}
	    } else if (*s == QUOTE)  {
		flt_putc(*s++);
		s = write_literal(s);
	    } else if (*s) {
		if ( isNameBegin(*s) ) {
		    s = extract_identifier(s);
		} else {
		    flt_putc(*s++);
		}
	    }
	}
    }
}
