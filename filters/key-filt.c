/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/key-filt.c,v 1.6 2000/01/10 00:00:33 tom Exp $
 *
 * Filter to add vile "attribution" sequences to a vile keyword file.  It's
 * done best in C because the delimiters may change as a result of processing
 * the file.
 */

#include <filters.h>

#define QUOTE '\''

char *filter_name = "key";

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
ExecClass(FILE *fp, char *param)
{
    char *t = strmalloc(param);
    parse_keyword(t, 1);
    free(t);
    t = skip_blanks(skip_ident(skip_blanks(param)));
    if (*t == eqls_ch) {
	write_string(fp, param, t - param, Ident2_attr);
	fputc(*t++, fp);
	write_string(fp, t, strlen(t), color_of(t));
    } else {
	write_string(fp, param, strlen(param), Ident2_attr);
    }
}

static void
ExecEquals(FILE *fp, char *param)
{
    eqls_ch = *param;
    write_string(fp, param, strlen(param), Literal_attr);
}

static void
ExecMeta(FILE *fp, char *param)
{
    meta_ch = *param;
    write_string(fp, param, strlen(param), Literal_attr);
}

static void
ExecTable(FILE *fp, char *param)
{
    char *t = skip_ident(param);
    write_string(fp, param, t - param, Literal_attr);
    write_string(fp, t, strlen(t), Error_attr);
}

static int parse_directive(FILE *fp, char *line)
{
    /* *INDENT-OFF* */
    static struct {
	const char *name;
	void (*func) (FILE *, char *);
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
		    write_string(fp, line, s-line, Ident_attr);
		    (*table[n].func) (fp, s);
		    return 1;
		}
	    }
	}
	write_string(fp, line, strlen(line), Error_attr);
    }
    return 0;
}

void
init_filter(int before GCC_UNUSED)
{
}

void
do_filter(FILE * input, FILE * output)
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
	write_string(output, line, s - line, "");
	if (*s == eqls_ch) {
	    write_string(output, s, strlen(s), Comment_attr);
	} else if (!parse_directive(output, s)) {
	    t = skip_ident(s);
	    write_string(output, s, t - s, Ident_attr);
	    if (*t == eqls_ch) {
		fputc(*t++, output);
	    }
	    s = skip_ident(t);
	    if (s != t) {
		int save = *s;
		*s = 0;
		if (is_class(t)) {
		    write_string(output, t, strlen(t), Ident2_attr);
		    t = s;
		}
		*s = save;
	    }
	    write_string(output, t, strlen(t), color_of(t));
	}
    }
}
