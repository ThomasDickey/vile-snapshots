/*
 * Main program and I/O for external vile syntax/highlighter programs
 *
 * $Header: /users/source/archives/vile.vcs/RCS/builtflt.c,v 1.65 2008/10/19 15:09:57 tom Exp $
 *
 */

#include "estruct.h"
#include "builtflt.h"

#ifdef _builtflt_h

#include "edef.h"
#include "nefunc.h"
#include "nevars.h"
#include <stdarg.h>

#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif

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
    static const char prefix[] = "vile-";
    static const char suffix[] = "-filt";

    char *temp;

    *params = 0;

    if (valid_buffer(curbp)
	&& curbp->majr != 0
	&& !strcmp(curbp->majr->shortname, major_name)
	&& (temp = b_val_ptr(curbp, VAL_FILTERNAME)) != 0) {
	if (!strncmp(temp, prefix, sizeof(prefix) - 1)) {
	    char *base = temp + sizeof(prefix) - 1;
	    char *next = base;
	    int n;

	    while (*next) {
		if (!strncmp(next, suffix, sizeof(suffix) - 1)
		    && (isSpace(next[sizeof(suffix) - 1])
			|| (next[sizeof(suffix) - 1] == EOS))) {
		    size_t len = next - base;
		    for (n = 0; builtflt[n] != 0; n++) {
			char *name = builtflt[n]->filter_name;
			if (strlen(name) == len
			    && !strncmp(base, name, len)) {
			    *params = skip_cblanks(next + sizeof(suffix) - 1);
			    return n;
			}
		    }
		    break;
		}
		next++;
	    }
	}
    }
    return -1;
}
#endif

static char *
param_value(const char **ptr)
{
    char *value = 0;
    const char *s;
    const char *t;

    s = skip_cblanks(*ptr + 1);
    t = skip_ctext(s);
    if (t != s) {
	if ((value = strmalloc(s)) != 0) {
	    value[t - s] = EOS;
	    *ptr = t;
	}
    }
    return value;
}

/*
 * All we're really interested in are the -k and -t options.  Ignore -v and -q.
 */
static int
process_params(void)
{
    const char *s = current_params;
    char *value;

    memset(flt_options, 0, sizeof(flt_options));
    FltOptions('t') = tabstop_val(curbp);
    while (*s != EOS) {
	s = skip_cblanks(s);
	if (*s == '-') {
	    while (*s != EOS && *++s != EOS && !isSpace(*s)) {
		flt_options[CharOf(*s)] += 1;
		switch (*s) {
		case 'k':
		    if ((value = param_value(&s)) != 0) {
			flt_read_keywords(value);
			free(value);
		    }
		    break;
		case 't':
		    if ((value = param_value(&s)) != 0) {
			if ((FltOptions('t') = atoi(value)) <= 0)
			    FltOptions('t') = 8;
			free(value);
		    }
		    break;
		}
	    }
	} else {
	    s = skip_ctext(s);
	}
    }
    vile_keywords = !FltOptions('k');
    return !vile_keywords;
}

static void
save_mark(int first)
{
    if (mark_out.o > llength(mark_out.l))
	mark_out.o = llength(mark_out.l);
    if (first)
	DOT = mark_out;
    else
	MK = mark_out;
}

#ifdef HAVE_LIBDL

#ifdef RTLD_NOW
#define my_RTLD RTLD_NOW
#else
#ifdef RTLD_LAZY
#define my_RTLD RTLD_LAZY
#else
make an error
#endif
#endif

static int
load_filter(const char *name)
{
    char defining[NSTRING];
    char filename[NFILEN];
    char leafname[NSTRING];
    void *obj;
    FILTER_DEF *def;
    int found = 0;
    int tried = 0;
    const char *cp = libdir_path;

    if (strlen(name) < NSTRING - 30) {
	sprintf(defining, "define_%s", name);
	sprintf(leafname, "vile-%s-filt.so", name);
	while ((cp = parse_pathlist(cp, filename)) != 0) {
	    if (strlen(filename) + strlen(leafname) + 3 >= sizeof(filename))
		continue;
	    pathcat(filename, filename, leafname);
	    TRACE(("load_filter(%s) %s\n", filename, defining));
	    ++tried;
	    if ((obj = dlopen(filename, my_RTLD)) != 0) {
		found = 1;
		if ((def = dlsym(obj, defining)) == 0) {
		    dlclose(obj);
		    mlwarn("[Error: can't reference variable %s from %s (%s)]",
			   defining, filename, dlerror());
		    break;
		} else {
		    TRACE(("...load_filter\n"));
		    current_filter->InitFilter = def->InitFilter;
		    current_filter->DoFilter = def->DoFilter;
		    current_filter->options = def->options;
		    current_filter->loaded = 1;
		    break;
		}
	    }
	}

	if (!found && tried) {
	    mlwarn("[Error: can't load shared object %s (%s)]",
		   leafname, dlerror());
	}
    }
    return current_filter->loaded;
}
#endif /* HAVE_LIBDL */

/******************************************************************************
 * Public functions                                                           *
 ******************************************************************************/

/*
 * Trim newline from the string, returning true if it was found.
 */
int
chop_newline(char *s)
{
    size_t len = strlen(s);

    if (len != 0 && s[len - 1] == '\n') {
	s[--len] = '\0';
	return 1;
    }
    return 0;
}

void
flt_echo(const char *string, int length)
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
	&& tb_bappend(&gets_data, lvalue(mark_in.l), need) != 0
	&& tb_sappend(&gets_data, "\n") != 0
	&& tb_append(&gets_data, EOS) != 0) {
	*ptr = tb_values(gets_data);
	*len = need + len_record_sep(curbp);

	TRACE(("flt_gets %6d:%.*s\n",
	       line_no(curbp, mark_in.l),
	       *ptr ? need : 1,
	       NONNULL(*ptr)));

	mark_in.l = lforw(mark_in.l);
    } else {
	TRACE(("flt_gets - EOF\n"));
    }
    return *ptr;
}

/*
 * Return the line number at which we are about to write a token.
 */
int
flt_get_line(void)
{
    return line_no(curbp, mark_out.l);
}

/*
 * Return the column/offset for the current token.
 */
int
flt_get_col(void)
{
    return offs2col(curwp, mark_out.l, mark_out.o + 1);
}

#ifdef MDFILTERMSGS
/*
 * Clear the text from [Filter Messages].
 */
static void
init_flt_error(void)
{
    BUFFER *bp;
    if (b_val(curbp, MDFILTERMSGS)) {
	if ((bp = find_b_name(FLTMSGS_BufName)) != 0) {
	    b_clr_changed(bp);	/* so bclear does not prompt */
	    bclear(bp);
	}
    }
}
#else
#define init_flt_error()	/* nothing */
#endif

/*
 * Log an error detected by the syntax filter.
 */
void
flt_error(const char *fmt,...)
{
#ifdef MDFILTERMSGS
    if (b_val(curbp, MDFILTERMSGS)) {
	const char *filename = ((curbp != 0)
				? (isInternalName(curbp->b_fname)
				   ? curbp->b_bname
				   : curbp->b_fname)
				: "<stdin>");
	BUFFER *bp;
	va_list ap;

	if ((bp = make_ro_bp(FLTMSGS_BufName, BFINVS)) != 0) {
	    (void) b2printf(bp, "%s: %d:%d: ",
			    filename,
			    flt_get_line(),
			    flt_get_col());

	    va_start(ap, fmt);
	    (void) b2vprintf(bp, fmt, ap);
	    va_end(ap);

	    (void) b2printf(bp, "\n");
	}
    }
#endif
}

/*
 * This function is used from flex to read chunks from the input file.  We'll
 * make it simple by ending each read at the end of a line; some long lines
 * will be split across successive reads.
 */
int
flt_input(char *buffer, int max_size)
{
    const char *separator = "\n";
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
		buffer[used++] = (char) lgetc(mark_in.l, mark_in.o++);
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

const char *
flt_name(void)
{
    return current_filter ? current_filter->filter_name : "";
}

char *
flt_put_blanks(char *string)
{
    char *result = skip_blanks(string);
    if (result != string)
	flt_puts(string, result - string, "");
    return result;
}

void
flt_putc(int ch)
{
    if (ch == '\n') {
	if (mark_out.l != buf_head(curbp))
	    mark_out.l = lforw(mark_out.l);
	mark_out.o = w_left_margin(curwp);
    } else {
	mark_out.o++;
    }
}

void
flt_puts(const char *string, int length, const char *marker)
{
    char bfr1[NSTRING];
    char bfr2[NSTRING];
    char *last;
    int count;

    if (length > 0) {
	save_mark(TRUE);
	if (marker != 0 && *marker != 0 && *marker != 'N') {
	    vl_strncpy(bfr2, marker, sizeof(bfr1) - 10);
	    sprintf(bfr1, "%c%d%s:", CTL_A, length, bfr2);
	    last = bfr1 + strlen(bfr1);
	    decode_attribute(bfr1, last - bfr1, 0, &count);
	}
	flt_echo(string, length);
	save_mark(FALSE);
	if (apply_attribute()) {
	    REGIONSHAPE save_shape = regionshape;
	    regionshape = rgn_EXACT;
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
#if OPT_MAJORMODE
    if ((n = parse_filtername(name, &current_params)) >= 0) {
	TRACE(("...%s(%s)\n", builtflt[n]->filter_name, current_params));
	current_filter = builtflt[n];
	return TRUE;
    }
#endif
    for (n = 0; builtflt[n] != 0; n++) {
	if (!strcmp(name, builtflt[n]->filter_name)) {
	    current_filter = builtflt[n];
	    current_params = "";
	    TRACE(("...%s\n", builtflt[n]->filter_name));
	    return TRUE;
	}
    }
    TRACE(("...not found\n"));
    return FALSE;
}

int
flt_start(char *name)
{
    TRACE(("flt_start(%s)\n", name));
    if (flt_lookup(name)
#ifdef HAVE_LIBDL
	&& (current_filter->loaded || load_filter(current_filter->filter_name))
#endif
	) {
	MARK save_dot;
	MARK save_mk;

	save_dot = DOT;
	save_mk = MK;

	need_separator = FALSE;
	mark_in.l = lforw(buf_head(curbp));
	mark_in.o = w_left_margin(curwp);
	mark_out = mark_in;
	tb_init(&gets_data, 0);

	init_flt_error();
	flt_initialize(current_filter->filter_name);

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
#if NO_LEAKS
	current_filter->FreeFilter();
#endif

	DOT = save_dot;
	MK = save_mk;

	return TRUE;
    }
    return FALSE;
}

/*
 * The spelling filter uses this function, since it parses the buffer more
 * than once, using the parsed data for a list of words to check.  So we
 * do not wipe the symbol table.
 */
int
flt_restart(char *name)
{
    TRACE(("flt_restart(%s)\n", name));
    if (flt_lookup(name)
#ifdef HAVE_LIBDL
	&& (current_filter->loaded || load_filter(current_filter->filter_name))
#endif
	) {
	need_separator = FALSE;
	mark_in.l = lforw(buf_head(curbp));
	mark_in.o = w_left_margin(curwp);
	mark_out = mark_in;
	tb_init(&gets_data, 0);

	init_flt_error();
	set_symbol_table(current_filter->filter_name);

	return TRUE;
    }
    return FALSE;
}

static TBUFF *filter_list;

int
var_FILTER_LIST(TBUFF **rp, const char *vp)
{
    int n;

    if (rp) {
	if (filter_list == 0) {
	    for (n = 0; builtflt[n] != 0; n++) {
		if (n)
		    tb_sappend0(&filter_list, " ");
		tb_sappend0(&filter_list, builtflt[n]->filter_name);
	    }
	}
	tb_scopy(rp, tb_values(filter_list));
	return TRUE;
    } else if (vp) {
	return ABORT;		/* read-only */
    } else {
	return FALSE;
    }
}

/*
 * Check if the command name is one of vile's set/unset variants.
 */
int
vl_is_setting(const void *cmd)
{
    const CMDFUNC *ptr = (const CMDFUNC *) cmd;
    int result = 0;

    if (ptr != 0) {
	if (ptr == &f_setglobmode
	    || ptr == &f_delglobmode
	    || ptr == &f_setlocmode
	    || ptr == &f_dellocmode
	    || ptr == &f_setvar
	    || ptr == &f_unsetvar) {
	    result = 1;
	}
    }
    return result;
}

#if OPT_EXTRA_COLOR
int
vl_is_xcolor(const void *cmd)
{
    const CMDFUNC *ptr = (const CMDFUNC *) cmd;
    int result = 0;

    if (ptr != 0) {
	if (ptr == &f_set_extra_colors) {
	    result = 1;
	}
    }
    return result;
}
#endif

/*
 * Check if the command name is one of vile's submode set/unset variants.
 */
int
vl_is_majormode(const void *cmd)
{
    int result = 0;
#if OPT_MAJORMODE
    const CMDFUNC *ptr = (const CMDFUNC *) cmd;

    if (ptr != 0) {
	if (ptr == &f_define_mode
	    || ptr == &f_remove_mode) {
	    result = 1;
	}
    }
#endif
    return result;
}

/*
 * Check if the command name is one of vile's submode set/unset variants.
 */
int
vl_is_submode(const void *cmd)
{
    int result = 0;
#if OPT_MAJORMODE
    const CMDFUNC *ptr = (const CMDFUNC *) cmd;

    if (ptr != 0) {
	if (ptr == &f_define_submode
	    || ptr == &f_remove_submode) {
	    result = 1;
	}
    }
#endif
    return result;
}

int
vl_check_cmd(const void *cmd, unsigned flags)
{
    const CMDFUNC *actual = (const CMDFUNC *) cmd;
    int result = 0;

    if (actual != 0) {
	result = ((actual->c_flags & flags) != 0);
    }
    return result;
}

const void *
vl_lookup_cmd(const char *name)
{
    return (const void *) engl2fnc(name);
}

/*
 * Lookup a symbol to see if it is one of vile's modes, majormodes or
 * submodes).
 */
int
vl_lookup_mode(const char *name)
{
    return vl_find_mode(name);
}

/*
 * Lookup a symbol to see if it is one of vile's built-in symbols.
 */
int
vl_lookup_var(const char *name)
{
    int result = 0;
    if (*name == '$')
	++name;
    if (vl_lookup_statevar(name) < 0
	&& vl_lookup_mode(name) < 0) {
	result = -1;
    }
    return result;
}

#if NO_LEAKS
void
flt_leaks(void)
{
    flt_free_symtab();
    FreeIfNeeded(default_attr);
    tb_free(&filter_list);
}
#endif

#else

extern void builtflt(void);
void
builtflt(void)
{
}

#endif /* _builtflt_h */
