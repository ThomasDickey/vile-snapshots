/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/key-filt.c,v 1.9 2000/02/28 11:59:48 tom Exp $
 *
 * Filter to add vile "attribution" sequences to a vile keyword file.  It's
 * done best in C because the delimiters may change as a result of processing
 * the file.
 */

#include <filters.h>

#define QUOTE '\''

DefineFilter("key");

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Literal_attr;

static char *
color_of(char *s)
{
    char *result = Error_attr;
    char *t;
    int quoted = 0;
    int save;

    s = skip_blanks(s);
    t = skip_ident(s);
    save = *t;
    *t = 0;

    if (is_class(s)) {
	result = Ident2_attr;
    } else if (*s != 0) {
	result = Action_attr;
	while (*s != 0) {
	    if (quoted) {
		if (*s == QUOTE)
		    quoted = 0;
	    } else if (*s == QUOTE) {
		quoted = 1;
		result = Literal_attr;
	    } else if (strchr("RUBI", *s) == 0) {
		if (*s++ != 'C' || !isxdigit(*s)) {
		    result = Error_attr;
		    break;
		}
	    }
	    s++;
	}
    }
    *t = save;
    return result;
}

static void
ExecClass(char *param)
{
    char *t = strmalloc(param);
    parse_keyword(t, 1);
    free(t);
    t = skip_blanks(skip_ident(skip_blanks(param)));
    if (*t == eqls_ch) {
	flt_puts(param, t - param, Ident2_attr);
	flt_putc(*t++);
	flt_puts(t, strlen(t), color_of(t));
    } else {
	flt_puts(param, strlen(param), Ident2_attr);
    }
}

static void
ExecEquals(char *param)
{
    eqls_ch = *param;
    flt_puts(param, strlen(param), Literal_attr);
}

static void
ExecMeta(char *param)
{
    meta_ch = *param;
    flt_puts(param, strlen(param), Literal_attr);
}

static void
ExecTable(char *param)
{
    char *t = skip_ident(param);
    flt_puts(param, t - param, Literal_attr);
    flt_puts(t, strlen(t), Error_attr);
}

static int parse_directive(char *line)
{
    /* *INDENT-OFF* */
    static struct {
	const char *name;
	void (*func) (char *);
    } table[] = {
	{ "class",   ExecClass    },
	{ "default", ExecTable    },
	{ "equals",  ExecEquals   },
	{ "include", ExecTable    },
	{ "merge",   ExecTable    },
	{ "meta",    ExecMeta     },
	{ "source",  ExecTable    },
	{ "table",   ExecTable    },
    };
    /* *INDENT-ON* */
    unsigned n, len;
    char *s;

    if (*(s = skip_blanks(line)) == meta_ch) {
	s = skip_blanks(s + 1);
	if ((len = (skip_ident(s) - s)) != 0) {
	    for (n = 0; n < sizeof(table) / sizeof(table[0]); n++) {
		if (!strncmp(s, table[n].name, len)) {
		    s = skip_blanks(s + len);
		    flt_puts(line, s-line, Ident_attr);
		    (*table[n].func) (s);
		    return 1;
		}
	    }
	}
	flt_puts(line, strlen(line), Error_attr);
    }
    return 0;
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE * input)
{
    static unsigned used;
    static char *line;

    char *s, *t;

    /*
     * Unlike most filters, we make a copy of the attributes, since we will be
     * manipulating the class symbol table.
     */
    Action_attr  = strmalloc(class_attr(NAME_ACTION));
    Comment_attr = strmalloc(class_attr(NAME_COMMENT));
    Error_attr   = strmalloc(class_attr(NAME_ERROR));
    Ident_attr   = strmalloc(class_attr(NAME_IDENT));
    Ident2_attr  = strmalloc(class_attr(NAME_IDENT2));
    Literal_attr = strmalloc(class_attr(NAME_LITERAL));

    meta_ch = '.';
    eqls_ch = ':';

    while (readline(input, &line, &used) != NULL) {
	s = skip_blanks(line);
	flt_puts(line, s - line, "");
	if (*s == eqls_ch) {
	    flt_puts(s, strlen(s), Comment_attr);
	} else if (!parse_directive(s)) {
	    t = skip_ident(s);
	    flt_puts(s, t - s, Ident_attr);
	    if (*t == eqls_ch) {
		flt_putc(*t++);
	    }
	    s = skip_ident(t);
	    if (s != t) {
		int save = *s;
		*s = 0;
		if (is_class(t)) {
		    flt_puts(t, strlen(t), Ident2_attr);
		    t = s;
		}
		*s = save;
	    }
	    flt_puts(t, strlen(t), color_of(t));
	}
    }
}
