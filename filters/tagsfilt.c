/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/tagsfilt.c,v 1.3 2000/02/10 11:30:12 tom Exp $
 *
 * Filter to add vile "attribution" sequences to a tags file.  It's done best
 * in C because the file consists only of columns of data that are delimited
 * by whitespace.
 */

#include <filters.h>

DefineFilter("tags");

static char *Comment_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Literal_attr;
static char *Number_attr;

static char *
skip_field(char *src)
{
    while (*src != 0 && *src != '\t')
	src++;
    return (src);
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input, FILE *output)
{
    static unsigned used;
    static char *line;

    char *s, *t;

    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr   = class_attr(NAME_IDENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Literal_attr = class_attr(NAME_LITERAL);
    Number_attr  = class_attr(NAME_NUMBER);

    while (readline(input, &line, &used) != NULL) {
	s = line;
	if (*s != '!') {
	    t = skip_field(s);
	    write_token(output, s, t-s, Ident_attr);
	    if (*t == '\t') {
		fputc(*t++, output);
	    }
	    s = t;
	    t = skip_field(s);
	    write_token(output, s, t-s, Ident2_attr);
	    if (*t == '\t') {
		fputc(*t++, output);
	    }
	    s = t;
	    if (isdigit(*s)) {
		while (isdigit(*t))
		    t++;
		write_token(output, s, t-s, Number_attr);
	    } else if (ispunct(*s)) {
		t++;
		while (*t != 0 && *t != *s) {
		    if (*t == '\\')
			t++;
		    if (*t != 0)
			t++;
		}
		write_token(output, s, t-s, Literal_attr);
	    }
	    s = t;
	}
	write_token(output, s, strlen(s), Comment_attr);
    }
}
