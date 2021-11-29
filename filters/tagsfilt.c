/*
 * $Id: tagsfilt.c,v 1.13 2013/12/02 01:32:53 tom Exp $
 *
 * Filter to add vile "attribution" sequences to a tags file.  It's done best
 * in C because the file consists only of columns of data that are delimited
 * by whitespace.
 */

#include <filters.h>

DefineFilter(tags);

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
    (void) before;
}

static void
do_filter(FILE *input GCC_UNUSED)
{
    static size_t used;
    static char *line;

    char *s, *t;

    (void) input;

    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr = class_attr(NAME_IDENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Literal_attr = class_attr(NAME_LITERAL);
    Number_attr = class_attr(NAME_NUMBER);

    while (flt_gets(&line, &used) != NULL) {
	s = line;
	if (*s != '!') {
	    t = skip_field(s);
	    flt_puts(s, (int) (t - s), Ident_attr);
	    if (*t == '\t') {
		flt_putc(*t++);
	    }
	    s = t;
	    t = skip_field(s);
	    flt_puts(s, (int) (t - s), Ident2_attr);
	    if (*t == '\t') {
		flt_putc(*t++);
	    }
	    s = t;
	    if (isdigit(CharOf(*s))) {
		while (isdigit(CharOf(*t)))
		    t++;
		flt_puts(s, (int) (t - s), Number_attr);
	    } else if (ispunct(CharOf(*s))) {
		t++;
		while (*t != 0 && *t != *s) {
		    if (*t == '\\')
			t++;
		    if (*t != 0)
			t++;
		}
		flt_puts(s, (int) (t - s), Literal_attr);
	    }
	    s = t;
	}
	flt_puts(s, (int) strlen(s), Comment_attr);
    }
}

#if NO_LEAKS
static void
free_filter(void)
{
}
#endif
