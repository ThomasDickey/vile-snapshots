/*
 * Main program and I/O for external vile syntax/highlighter programs
 *
 * $Header: /users/source/archives/vile.vcs/RCS/builtflt.c,v 1.14 2000/08/09 23:10:00 tom Exp $
 *
 */

#include <estruct.h>

#include <builtflt.h>

#ifdef _builtflt_h

#include <edef.h>
#include <stdarg.h>

static FILTER_DEF *current_filter;
static MARK mark_in;
static MARK mark_out;
static TBUFF *gets_data;
static const char *current_params;
static int need_separator;

/******************************************************************************
 * Private functions                                                          *
 ******************************************************************************/

#if OPT_MAJORMODE
/*
 * Given the 'major_name', verify that it applies to this buffer, and if so,
 * lookup $filtername and see if we can associate the external filtername to
 * one of the built-in filters.
 *
 * FIXME:  This only works for filters constructed with the naming convention
 * that we use for our external filters, i.e., "vile-{majormode}-filt".
 *
 * FIXME:  Should we allow lookup of majormodes that aren't assigned to a
 * buffer?
 */
static int
parse_filtername(const char *major_name, const char **params)
{
    char *temp;

    *params = 0;

    if (curbp != 0
	&& curbp->majr != 0
	&& !strcmp(curbp->majr->name, major_name)
	&& (temp = b_val_ptr(curbp, VAL_FILTERNAME)) != 0) {
	if (!strncmp(temp, "vile-", 5)) {
	    char *base = temp + 5;
	    char *next = base;
	    int n;

	    while (*next) {
		if (!strncmp(next, "-filt", 5)
		    && (isSpace(next[5]) || (next[5] == EOS))) {
		    break;
		}
		next++;
	    }
	    for (n = 0; builtflt[n] != 0; n++) {
		if ((int) strlen(builtflt[n]->filter_name) == (next - base)
		    && !strncmp(base, builtflt[n]->filter_name, next - base)) {
		    *params = skip_cblanks(next + 5);
		    return n;
		}
	    }
	}
    }
    return -1;
}
#endif

/*
 * All we're really interested in are the -k options.  Ignore -v and -q.
 */
static int
process_params(void)
{
    int k_used = 0;
    const char *s = current_params;
    const char *t;

    while (*s != EOS) {
	s = skip_cblanks(s);
	if (*s == '-') {
	    while (*++s != EOS && !isSpace(*s)) {
		if (*s == 'k') {
		    s = skip_cblanks(s + 1);
		    t = skip_ctext(s);
		    if (t != s) {
			char *value = strmalloc(s);
			value[t - s] = EOS;
			flt_read_keywords(value);
			free(value);
			k_used = 1;
			s = t;
		    }
		}
	    }
	} else {
	    s = skip_ctext(s);
	}
    }
    return k_used;
}

/******************************************************************************
 * Public functions                                                           *
 ******************************************************************************/

void
flt_echo(char *string, int length)
{
    while (length-- > 0)
	flt_putc(*string++);
}

void
flt_failed(const char *msg)
{
    mlforce("[Filter: %s]", msg);
}

void
flt_finish(void)
{
    tb_free(&gets_data);
}

char *
flt_gets(char **ptr, unsigned *len)
{
    int need = is_header_line(mark_in, curbp) ? -1 : llength(mark_in.l);

    *len = 0;
    *ptr = 0;

    if (need >= 0
	&& tb_init(&gets_data, 0) != 0
	&& tb_bappend(&gets_data, mark_in.l->l_text, need) != 0
	&& tb_sappend(&gets_data, "\n") != 0
	&& tb_append(&gets_data, EOS) != 0) {
	*ptr = tb_values(gets_data);
	*len = need + len_record_sep(curbp);

	TRACE(("flt_gets %6d:%.*s",
		line_no(curbp, mark_in.l),
		*ptr ? need : 1,
		*ptr ? *ptr : ""));

	mark_in.l = lforw(mark_in.l);
    } else {
	TRACE(("flt_gets - EOF\n"));
    }
    return *ptr;
}

/*
 * This function is used from flex to read chunks from the input file.  We'll
 * make it simple by ending each read at the end of a line; some long lines
 * will be split across successive reads.
 */
int
flt_input(char *buffer, int max_size)
{
    char *separator = "\n";
    int need = strlen(separator);
    int used = 0;

    if (need_separator) {
	strcpy(buffer, separator);
	used = need;
	need_separator = FALSE;
    }
    if (!is_header_line(mark_in, curbp)) {
	while (used < max_size) {
	    if (mark_in.o < llength(mark_in.l)) {
		buffer[used++] = lgetc(mark_in.l, mark_in.o++);
	    } else {
		mark_in.l = lforw(mark_in.l);
		mark_in.o = w_left_margin(curwp);
		need_separator = TRUE;
		break;
	    }
	}
	if (need_separator && (used + need) < max_size) {
	    strcpy(buffer + used, separator);
	    used += need;
	    need_separator = FALSE;
	}
    }
    return used;
}

char *
flt_name(void)
{
    return current_filter ? current_filter->filter_name : "";
}

void
flt_putc(int ch)
{
    if (ch == '\n' || mark_out.o++ >= llength(mark_out.l)) {
	mark_out.l = lforw(mark_out.l);
	mark_out.o = w_left_margin(curwp);
    }
}

void
flt_puts(char *string, int length, char *marker)
{
    char bfr1[NSTRING];
    char bfr2[NSTRING];
    char *last;
    int count;

    if (length > 0) {
	DOT = mark_out;
	if (marker != 0 && *marker != 0 && *marker != 'N') {
	    vl_strncpy(bfr2, marker, sizeof(bfr1) - 10);
	    last = lsprintf(bfr1, "%c%d%s:", CTL_A, length, bfr2);
	    parse_attribute(bfr1, last - bfr1, 0, &count);
	}
	flt_echo(string, length);
	MK = mark_out;
	if (apply_attribute()) {
	    int save_shape = regionshape;
	    regionshape = EXACT;
	    (void) attributeregion();
	    videoattribute = 0;
	    regionshape = save_shape;
	}
    }
}

/*
 * For the given majormode 'name', check if we have a builtin filter by that
 * name, or if it has an associated filtername, check if that is a builtin.
 *
 * Sets current_filter and current_params as a side-effect.
 */
int
flt_lookup(char *name)
{
    int n;

    TRACE(("flt_lookup(%s)\n", name));
    for (n = 0; builtflt[n] != 0; n++) {
	if (!strcmp(name, builtflt[n]->filter_name)) {
	    current_filter = builtflt[n];
	    current_params = "";
	    TRACE(("...%s\n", builtflt[n]->filter_name));
	    return TRUE;
	}
    }
#if OPT_MAJORMODE
    if ((n = parse_filtername(name, &current_params)) >= 0) {
	TRACE(("...%s(%s)\n", builtflt[n]->filter_name, current_params));
	current_filter = builtflt[n];
	return TRUE;
    }
#endif
    TRACE(("...not found\n"));
    return FALSE;
}

int
flt_start(char *name)
{
    TRACE(("flt_start(%s)\n", name));
    if (flt_lookup(name)) {
	MARK save_dot;
	MARK save_mk;

	save_dot = DOT;
	save_mk = MK;

	need_separator = FALSE;
	mark_in.l = lforw(buf_head(curbp));
	mark_in.o = w_left_margin(curwp);
	mark_out = mark_in;
	tb_init(&gets_data, 0);

	flt_initialize();
	flt_make_symtab(current_filter->filter_name);

	current_filter->InitFilter(1);

	flt_read_keywords(MY_NAME);
	if (!process_params()) {
	    if (strcmp(MY_NAME, current_filter->filter_name)) {
		flt_read_keywords(current_filter->filter_name);
	    }
	}
	set_symbol_table(current_filter->filter_name);
	current_filter->InitFilter(0);
	current_filter->DoFilter(stdin);

	DOT = save_dot;
	MK = save_mk;

	return TRUE;
    }
    return FALSE;
}

#if NO_LEAKS
void
flt_leaks(void)
{
    flt_free_symtab();
    FreeIfNeeded(default_attr);
}
#endif

#else

extern void builtflt(void);
void
builtflt(void)
{
}

#endif /* _builtflt_h */
