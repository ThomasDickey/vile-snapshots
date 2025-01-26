/*
 * $Id: key-filt.c,v 1.54 2025/01/26 10:50:02 tom Exp $
 *
 * Filter to add vile "attribution" sequences to a vile keyword file.  It's
 * done best in C because the delimiters may change as a result of processing
 * the file.
 */

#include <filters.h>

#define QUOTE '\''

DefineOptFilter(key, "c");

#define VERBOSE(level,params)	if (FltOptions('v') >= level) mlforce params

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Literal_attr;

static int
color_code(const char *s, const char **t)
{
    int result = 0;
    const char *base = s;

    if (*s != 0) {
	for (;;) {
	    while ((*s != '\0') && strchr("RUBI", *s) != NULL)
		++s;
	    if (*s == '\0') {
		result = 1;
		break;
	    } else if (*s++ == 'C') {
		if (isxdigit(CharOf(*s))) {
		    if (*(++s) == 0) {
			result = 1;
		    }
		}
	    } else {
		break;
	    }
	}
	if (result && FltOptions('c')) {
	    *t = base;
	}
    }
    return result;
}

static int
is_color(const char *s)
{
    const char *t = NULL;
    return color_code(s, &t);
}

static const char *
color_of(char *s, int arg)
{
    const char *result = "";
    char *t;
    int quoted = 0;
    int save;

    s = skip_blanks(s);
    t = skip_ident(s);
    if ((save = *t) != 0)
	*t = 0;

    if (is_class(s)) {
	if (FltOptions('c')) {
	    result = get_keyword_attr(s);
	    if (result == NULL)
		result = class_attr(s);
	    if (result == NULL)
		result = Ident2_attr;
	} else {
	    result = Ident2_attr;
	}
    } else if (arg && (*s != 0)) {
	char *base = s;

	if (!FltOptions('c'))
	    result = Action_attr;
	while (*s != 0) {
	    if (quoted) {
		if (*s == QUOTE)
		    quoted = 0;
	    } else if (*s == QUOTE) {
		quoted = 1;
		result = Literal_attr;
	    } else if ((s == base) && color_code(s, &result)) {
		break;
	    }
	    s++;
	}
    }
    if (save)
	*t = (char) save;
    return result;
}

/*
 * Skip {BLANK}{EQLS}{BLANK}.
 */
static int
skip_eqls_ch(char **param)
{
    char *s = *param;
    int rc = 0;

    s = skip_blanks(s);
    if (*s == eqls_ch) {
	rc = 1;
	++s;
	s = skip_blanks(s);
    }
    *param = s;
    return rc;
}

/*
 * Skip and color {BLANK}{EQLS}{BLANK}.
 */
static int
parse_eqls_ch(char **param)
{
    char *s = *param;
    int rc = 0;

    s = flt_put_blanks(s);
    if (*s == eqls_ch) {
	rc = 1;
	flt_putc(*s++);
	s = flt_put_blanks(s);
    }
    *param = s;
    return rc;
}

static int
abbr_len(char *s)
{
    char *base = s;

    while (*s != '\0') {
	if (*s == zero_or_more || *s == zero_or_all)
	    break;
	++s;
    }
    return (int) (s - base);
}

static const char *
actual_color(const char *param, int len, int arg, int *theirs)
{
    const char *result;
    char *s = strmalloc(param);

    *theirs = 0;		/* set to 1 to tell caller to free result */
    if (len > 0) {		/* if not null-terminated, set it now */
	s[len] = '\0';
    }

    result = color_of(s, arg);
    if (*result == 0)
	result = get_keyword_attr(s);
    if (result == s)
	*theirs = 1;

    if (result != NULL && *result != 0 && !is_color(result)) {
	result = Literal_attr;
    }

    if (!*theirs)
	free(s);
    return result;
}

static void
ExecAbbrev(char *param)
{
    zero_or_more = *param;
    flt_puts(param, (int) strlen(param), Literal_attr);
}

static void
ExecBrief(char *param)
{
    zero_or_all = *param;
    flt_puts(param, (int) strlen(param), Literal_attr);
}

static void
ExecClass(char *param)
{
    char *t = strmalloc(param);
    char *s;
    const char *attr = "";
    int ours = 0;

    parse_keyword(t, 1);
    free(t);
    t = flt_put_blanks(param);
    s = skip_ident(t);
    if (FltOptions('c')) {
	attr = actual_color(param, (int) (s - param), 1, &ours);
    } else {
	attr = Ident2_attr;
    }
    flt_puts(param, (int) (s - param), attr);
    if (ours)
	free(TYPECAST(void *, attr));
    if (parse_eqls_ch(&s)) {
	t = s;
	s = skip_ident(t);
	if (FltOptions('c')) {
	    attr = actual_color(t, (int) (s - t), 1, &ours);
	} else {
	    if (*(attr = color_of(t, 0)) == '\0')
		attr = Action_attr;
	}
	flt_puts(t, (int) (s - t), attr);
	if (ours)
	    free(TYPECAST(void *, attr));
	if (parse_eqls_ch(&s)) {
	    flt_puts(s, (int) strlen(s), Literal_attr);
	} else if (*s) {
	    flt_puts(s, (int) strlen(s), Error_attr);
	}
    } else if (*s) {
	flt_puts(s, (int) strlen(s), Error_attr);
    }
}

static void
ExecDefault(char *param)
{
    char *s = skip_ident(param);
    const char *t = param;
    const char *attr = Literal_attr;
    int save = *s;
    int ours = 0;

    VERBOSE(1, ("ExecDefault(%s)", param));
    *s = 0;
    if (!*t)
	t = NAME_KEYWORD;
    if (is_class(t)) {
	free(default_attr);
	default_attr = strmalloc(t);
    }
    if (FltOptions('c')) {
	attr = actual_color(t, -1, 1, &ours);
	VERBOSE(2, ("actual_color(%s) = %s", t, attr));
    }
    *s = (char) save;
    flt_puts(param, (int) strlen(param), attr);
    if (ours)
	free(TYPECAST(void *, attr));
}

static void
ExecEquals(char *param)
{
    eqls_ch = *param;
    flt_puts(param, (int) strlen(param), Literal_attr);
}

static void
ExecInclude(char *param)
{
    flt_read_keywords(param);
    flt_puts(param, (int) strlen(param), Literal_attr);
}

static void
ExecMeta(char *param)
{
    meta_ch = *param;
    flt_puts(param, (int) strlen(param), Literal_attr);
}

/*
 * Include a symbol table from another key-file.
 */
static void
ExecSource(char *param)
{
    int save_meta = meta_ch;
    int save_eqls = eqls_ch;

    flt_make_symtab(param);
    flt_read_keywords(MY_NAME);
    flt_read_keywords(param);
    set_symbol_table(flt_name());

    meta_ch = save_meta;
    eqls_ch = save_eqls;

    flt_puts(param, (int) strlen(param), Literal_attr);
}

static void
ExecTable(char *param)
{
    char *t;

    VERBOSE(1, ("ExecTable(%s)", param));
    if (FltOptions('c')) {
	t = skip_ident(param);
	if (*skip_blanks(t) == '\0') {
	    int save = *t;

	    *t = 0;
	    if (*param) {
		flt_make_symtab(param);
		flt_read_keywords(MY_NAME);
	    } else {
		set_symbol_table(default_table);
	    }
	    *t = (char) save;
	}
    }

    t = skip_ident(param);
    flt_puts(param, (int) (t - param), Literal_attr);
    if (*skip_blanks(t) == '\0') {
	flt_puts(t, (int) strlen(t), "");
    } else {
	flt_error("unexpected tokens");
	flt_puts(t, (int) strlen(t), Error_attr);
    }
}

static int
parse_directive(char *line)
{
    /* *INDENT-OFF* */
    static struct {
	const char *name;
	void (*func) (char *);
    } table[] = {
	{ "abbrev",  ExecAbbrev   },
	{ "brief",   ExecBrief    },
	{ "class",   ExecClass    },
	{ "default", ExecDefault  },
	{ "equals",  ExecEquals   },
	{ "include", ExecInclude  },
	{ "merge",   ExecSource   },
	{ "meta",    ExecMeta     },
	{ "source",  ExecSource   },
	{ "table",   ExecTable    },
    };
    /* *INDENT-ON* */

    size_t n, len;
    char *s;

    VERBOSE(1, ("parse_directive(%s)", line));
    if (*(s = skip_blanks(line)) == meta_ch) {
	s = skip_blanks(s + 1);
	if ((len = (size_t) (skip_ident(s) - s)) != 0) {
	    for (n = 0; n < sizeof(table) / sizeof(table[0]); n++) {
		if (!strncmp(s, table[n].name, len)) {
		    flt_puts(line, (int) (s + len - line), Ident_attr);
		    s = flt_put_blanks(s + len);
		    (*table[n].func) (s);
		    return 1;
		}
	    }
	}
	flt_error("unknown directive");
	flt_puts(line, (int) strlen(line), Error_attr);
    }
    return 0;
}

static void
parse_nondirective(char *s)
{
    char *base = s;
    char *t;
    const char *attr0 = Ident_attr;
    const char *attr1 = Ident2_attr;
    int our_0 = 0;
    int our_1 = 0;

    if (FltOptions('c')) {
	t = skip_ident(s = base);
	if (t != s) {
	    int save = *t;

	    /* this parses one of
	     * "name"
	     * "name:class"
	     * "name:color"
	     */
	    parse_keyword(s, 0);

	    *t = 0;
	    attr0 = actual_color(s, abbr_len(s), 0, &our_0);
	    *t = (char) save;
	}
	if (skip_eqls_ch(&t)) {
	    s = skip_ident(t);
	    if (s != t) {
		attr1 = actual_color(t, (int) (s - t), 1, &our_1);
	    }
	}
    }

    t = skip_ident(s = base);
    flt_puts(s, (int) (t - s), attr0);
    if (our_0)
	free(TYPECAST(void *, attr0));
    if (parse_eqls_ch(&t)) {
	s = skip_ident(t);
	if (s != t) {
	    int save = *s;
	    *s = 0;
	    if (!FltOptions('c')) {
		if (our_1) {
		    free(TYPECAST(void *, attr1));
		    our_1 = 0;
		}
		if (*(attr1 = color_of(t, 0)) == '\0')
		    attr1 = Action_attr;
	    }
	    flt_puts(t, (int) strlen(t), attr1);
	    *s = (char) save;
	}
	if (parse_eqls_ch(&s)) {
	    flt_puts(s, (int) strlen(s), Literal_attr);
	} else if (*s) {
	    flt_puts(s, (int) strlen(s), Error_attr);
	}
    } else if (*t) {
	flt_puts(t, (int) strlen(t), Error_attr);
    }
    if (our_1)
	free(TYPECAST(void *, attr1));
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

    (void) input;

    /*
     * Unlike most filters, we make a copy of the attributes, since we will be
     * manipulating the class symbol table.
     */
    Action_attr = strmalloc(class_attr(NAME_ACTION));
    Comment_attr = strmalloc(class_attr(NAME_COMMENT));
    Error_attr = strmalloc(class_attr(NAME_ERROR));
    Ident_attr = strmalloc(class_attr(NAME_IDENT));
    Ident2_attr = strmalloc(class_attr(NAME_IDENT2));
    Literal_attr = strmalloc(class_attr(NAME_LITERAL));

    zero_or_more = '*';
    zero_or_all = '?';
    meta_ch = '.';
    eqls_ch = ':';

    while (flt_gets(&line, &used) != NULL) {
	int ending = chop_newline(line);

	s = flt_put_blanks(line);
	if (*s == '\0') {
	    ;
	} else if (*s == eqls_ch) {
	    flt_puts(s, (int) strlen(s), Comment_attr);
	} else if (!parse_directive(s)) {
	    parse_nondirective(s);
	}
	if (ending)
	    flt_putc('\n');
    }
}

#if NO_LEAKS
static void
free_filter(void)
{
    FreeAndNull(Action_attr);
    FreeAndNull(Comment_attr);
    FreeAndNull(Error_attr);
    FreeAndNull(Ident_attr);
    FreeAndNull(Ident2_attr);
    FreeAndNull(Literal_attr);
}
#endif
