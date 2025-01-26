/*
 * $Id: builtflt.c,v 1.104 2025/01/26 16:50:14 tom Exp $
 *
 * Main program and I/O for builtin vile syntax/highlighter programs
 */

#include "estruct.h"
#include "builtflt.h"

#ifdef _builtflt_h

#include "edef.h"
#include "nefunc.h"
#include "nevars.h"

#include <stdarg.h>
#include <setjmp.h>

#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif

static jmp_buf flt_recovery;
static FILTER_DEF *current_filter;
static MARK mark_in;
static MARK mark_out;
static TBUFF *gets_data;
static const char *current_params;
static int need_separator;

#define FLT_PUTC(ch) \
    if (filter_only > 0) { \
	putchar(ch); \
    } else { \
	if (ch == '\n') { \
	    if (mark_out.l != buf_head(curbp)) \
		mark_out.l = lforw(mark_out.l); \
	    mark_out.o = w_left_margin(curwp); \
	} else { \
	    mark_out.o++; \
	} \
    }

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

    *params = NULL;

    if (valid_buffer(curbp)
	&& curbp->majr != NULL
	&& !strcmp(curbp->majr->shortname, major_name)
	&& (temp = b_val_ptr(curbp, VAL_FILTERNAME)) != NULL) {
	if (!strncmp(temp, prefix, sizeof(prefix) - 1)) {
	    char *base = temp + sizeof(prefix) - 1;
	    char *next = base;
	    int n;

	    while (*next) {
		if (!strncmp(next, suffix, sizeof(suffix) - 1)
		    && (isSpace(next[sizeof(suffix) - 1])
			|| (next[sizeof(suffix) - 1] == EOS))) {
		    size_t len = (size_t) (next - base);
		    for (n = 0; builtflt[n] != NULL; n++) {
			const char *name = builtflt[n]->filter_name;
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
    char *value = NULL;
    const char *s;
    const char *t;

    s = skip_cblanks(*ptr + 1);
    t = skip_ctext(s);
    if (t != s) {
	if ((value = strmalloc(s)) != NULL) {
	    value[t - s] = EOS;
	    *ptr = t;
	}
    }
    return value;
}

/*
 * All we're really interested in are the -k and -t options.  Ignore -v and -q.
 * Return true if we had a "-k" option.
 */
static int
ProcessArgs(int flag)
{
    const char *s = current_params;
    char *value;

    if (!flag) {
	char *command = NULL;
	size_t needed = strlen(current_filter->filter_name) + 11 + strlen(s);
	if (*s)
	    ++needed;
	if ((command = malloc(needed + 1)) != NULL) {
	    sprintf(command, "vile-%s-filt", current_filter->filter_name);
	    if (*s) {
		strcat(command, " ");
		strcat(command, s);
	    }
	    flt_puts(command, (int) needed, "M");
	    free(command);
	}
    }

    memset(flt_options, 0, sizeof(flt_options));
    FltOptions('t') = tabstop_val(curbp);
    while (*s != EOS) {
	s = skip_cblanks(s);
	if (*s == '-') {
	    while (*s != EOS && *++s != EOS && !isSpace(*s)) {
		flt_options[CharOf(*s)] += 1;
		switch (*s) {
		case 'k':
		    if ((value = param_value(&s)) != NULL) {
			if (flag) {
			    flt_init_table(value);
			    flt_setup_symbols(value);
			}
			free(value);
		    }
		    break;
		case 't':
		    if ((value = param_value(&s)) != NULL) {
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
	int first = TRUE;

	/* potential symbol conflict with ncurses */
	if (!strcmp(name, "key")) {
	    sprintf(defining, "vl_define_%s", name);
	} else {
	    sprintf(defining, "define_%s", name);
	}

	sprintf(leafname, "vile-%s-filt.so", name);
	while ((cp = parse_pathlist(cp, filename, &first)) != NULL) {
	    if (strlen(filename) + strlen(leafname) + 3 >= sizeof(filename))
		continue;
	    pathcat(filename, filename, leafname);
	    TRACE(("load_filter(%s) %s\n", filename, defining));
	    ++tried;
	    if ((obj = dlopen(filename, my_RTLD)) != NULL) {
		found = 1;
		if ((def = dlsym(obj, defining)) == NULL) {
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
    int ch;
    while (length-- > 0) {
	ch = *string++;
	FLT_PUTC(ch);
    }
}

/*
 * Followup from flt_failed() by returning zero if we are recovering from a
 * fatal error.
 */
int
flt_succeeds(void)
{
    return !setjmp(flt_recovery);
}

/*
 * This is used to stop lex/flex from exiting the program on a syntax error.
 */
void
flt_failed(const char *msg)
{
    mlforce("[Filter: %s]", msg);
    longjmp(flt_recovery, 1);
}

void
flt_finish(void)
{
    tb_free(&gets_data);
}

#define MustFixupCrLf() \
    (   (b_val(curbp, VAL_RECORD_SEP) == RS_CR) \
     || (b_val(curbp, VAL_RECORD_SEP) == RS_LF)  )

/*
 * Syntax filters expect text to be separated by newlines, and don't
 * expect embedded carriage returns.  Try to meet that expectation.
 */
static int
fixup_cr_lf(int ch)
{
    if (b_val(curbp, VAL_RECORD_SEP) == RS_CR) {
	if (ch == '\n')
	    ch = ' ';
	else if (ch == '\r')
	    ch = '\n';
    } else if (b_val(curbp, VAL_RECORD_SEP) == RS_LF) {
	if (ch == '\r')
	    ch = ' ';
    }
    return ch;
}

char *
flt_gets(char **ptr, size_t *len)
{
    int need = is_header_line(mark_in, curbp) ? -1 : llength(mark_in.l);

    *len = 0;
    *ptr = NULL;

    if (need >= 0
	&& tb_init(&gets_data, 0) != NULL
	&& tb_bappend(&gets_data, lvalue(mark_in.l), (size_t) need) != NULL
	&& tb_append(&gets_data, use_record_sep(curbp)) != NULL
	&& tb_append(&gets_data, EOS) != NULL) {
	*ptr = tb_values(gets_data);
	*len = (unsigned) (need + len_record_sep(curbp));

	TRACE(("flt_gets %6d:%.*s\n",
	       line_no(curbp, mark_in.l),
	       *ptr ? need : 1,
	       NONNULL(*ptr)));

	mark_in.l = lforw(mark_in.l);

	/*
	 * Syntax filters expect text to be separated by newlines, and don't
	 * expect embedded carriage returns.  Try to meet that expectation.
	 */
	if (MustFixupCrLf()) {
	    size_t len2 = tb_length(gets_data);
	    if (len2 != 0) {
		size_t n;
		char *values = tb_values(gets_data);
		int ch;

		for (n = 0; n < len2; ++n) {
		    ch = values[n];
		    if (isreturn(ch))
			values[n] = (char) fixup_cr_lf(ch);
		}
	    }
	}
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
    return offs2col(curwp, mark_out.l, mark_out.o);
}

#ifdef MDFILTERMSGS
/*
 * Get the pointer to filter-messages buffer.  Ensure that its "filtermsgs"
 * mode is off, so we don't get caught by trying to use it as input/output.
 */
static BUFFER *
find_filtermsgs(void)
{
    BUFFER *bp = make_ro_bp(FLTMSGS_BufName, BFINVS);
    if (bp != NULL) {
	set_local_b_val(bp, MDFILTERMSGS, FALSE);
    }
    return bp;
}

/*
 * Clear the text from [Filter Messages].
 */
static void
init_flt_error(void)
{
    BUFFER *bp;
    if (b_val(curbp, MDFILTERMSGS)) {
	if ((bp = find_filtermsgs()) != NULL) {
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
flt_error(const char *fmt, ...)
{
#ifdef MDFILTERMSGS
    if (b_val(curbp, MDFILTERMSGS)) {
	const char *filename = ((curbp != NULL)
				? (isInternalName(curbp->b_fname)
				   ? curbp->b_bname
				   : curbp->b_fname)
				: "<stdin>");
	BUFFER *bp;
	va_list ap;

	if ((bp = find_filtermsgs()) != NULL) {
	    (void) b2printf(bp, "%s: %d:%d: ",
			    filename,
			    flt_get_line(),
			    flt_get_col() + 1);

	    va_start(ap, fmt);
	    (void) b2vprintf(bp, fmt, ap);
	    va_end(ap);

	    (void) b2printf(bp, "\n");
	}
    }
#endif
}

/*
 * Log an message from the syntax filter.
 */
void
flt_message(const char *fmt, ...)
{
#ifdef MDFILTERMSGS
    if (b_val(curbp, MDFILTERMSGS)) {
	BUFFER *bp;
	va_list ap;

	if ((bp = find_filtermsgs()) != NULL) {
	    va_start(ap, fmt);
	    (void) b2vprintf(bp, fmt, ap);
	    va_end(ap);
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
    int need = (int) strlen(separator);
    int used = 0;
    int ch;

    if (need_separator) {
	strcpy(buffer, separator);
	used = need;
	need_separator = FALSE;
    }
    if (!is_header_line(mark_in, curbp)) {
	while (used < max_size) {
	    if (mark_in.o < llength(mark_in.l)) {
		ch = lgetc(mark_in.l, mark_in.o++);
		if (filter_only) {
		    buffer[used++] = (char) ch;
		} else {
		    if (isreturn(ch))
			ch = fixup_cr_lf(ch);
		    buffer[used++] = (char) ch;
		}
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
	flt_puts(string, (int) (result - string), "");
    return result;
}

void
flt_putc(int ch)
{
    FLT_PUTC(ch);
}

static int
hex2int(int ch)
{
    int value;

    if (ch >= '0' && ch <= '9')
	value = (ch - '0');
    else if (ch >= 'A' && ch <= 'Z')
	value = (ch - 'A') + 10;
    else if (ch >= 'a' && ch <= 'z')
	value = (ch - 'a') + 10;
    else
	value = 0;

    return value;
}

static int
int2hex(int value)
{
    int ch;

    if (value >= 0 && value <= 9)
	ch = value + '0';
    else if (value >= 10 && value <= 15)
	ch = (value - 10) + 'A';
    else
	ch = '?';

    return ch;
}

static int
lookup_ctrans(int ch)
{
    int value = hex2int(ch);
    return int2hex(ctrans[value % NCOLORS]);
}

void
flt_puts(const char *string, int length, const char *marker)
{
    char bfr1[NSTRING];
    char *last;
    int count;

    if (filter_only > 0) {
	if (marker != NULL && *marker != EOS && *marker != 'N') {
	    size_t limit = sizeof(bfr1) - 2;

	    strncpy(bfr1, marker, limit)[limit] = EOS;
	    for (last = bfr1; *last; ++last) {
		if (*last == 'C' && isXDigit(CharOf(last[1]))) {
		    ++last;
		    *last = (char) lookup_ctrans(CharOf(*last));
		}
	    }
	    printf("%c%d%s:", CTL_A, length, bfr1);
	}
	flt_echo(string, length);
    } else {
	if (length > 0 && marker != NULL && *marker != EOS && *marker != 'N') {
	    size_t prefix;

	    save_mark(TRUE);

	    sprintf(bfr1, "%c%d", CTL_A, length);
	    prefix = strlen(bfr1);

	    vl_strncpy(bfr1 + prefix, marker, sizeof(bfr1) - (prefix + 3));
	    last = bfr1 + strlen(bfr1);
	    *last++ = ':';
	    *last = EOS;

	    decode_attribute(bfr1, (size_t) (last - bfr1), (size_t) 0, &count);

	    if (count > 0) {
		flt_echo(string, length);
		save_mark(FALSE);

		if (apply_attribute()) {
		    REGIONSHAPE save_shape = regionshape;
		    regionshape = rgn_EXACT;
		    (void) attributeregion();
		    videoattribute = 0;
		    regionshape = save_shape;
		}
	    } else {
		save_mark(FALSE);
	    }
	} else {
	    flt_echo(string, length);
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
    for (n = 0; builtflt[n] != NULL; n++) {
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
    int rc = FALSE;

    TRACE((T_CALLED "flt_start(%s)\n", name));
    if (flt_lookup(name)
#ifdef HAVE_LIBDL
	&& (current_filter->loaded || load_filter(current_filter->filter_name))
#endif
	) {
	MARK save_dot;
	MARK save_mk;
	int nextarg;

	save_dot = DOT;
	save_mk = MK;

	need_separator = FALSE;
	mark_in.l = lforw(buf_head(curbp));
	mark_in.o = w_left_margin(curwp);
	mark_out = mark_in;
	tb_init(&gets_data, 0);

	init_flt_error();

	(void) ProcessArgs(0);

	flt_initialize(current_filter->filter_name);

	current_filter->InitFilter(1);

	/* setup colors for the filter's default-table */
	flt_read_keywords(MY_NAME);
	if (strcmp(MY_NAME, current_filter->filter_name)) {
	    flt_read_keywords(current_filter->filter_name);
	}

	nextarg = ProcessArgs(1);
	if (nextarg == 0) {
	    if (strcmp(MY_NAME, default_table)
		&& strcmp(current_filter->filter_name, default_table)) {
		flt_read_keywords(default_table);
	    }
	}
	set_symbol_table(default_table);
	if (FltOptions('Q')) {
	    flt_dump_symtab(NULL);
	} else {
	    current_filter->InitFilter(0);
	    current_filter->DoFilter(stdin);
	}
#if NO_LEAKS
	current_filter->FreeFilter();
#endif

	DOT = save_dot;
	MK = save_mk;

	rc = TRUE;
    }
    returnCode(rc);
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
	set_symbol_table(default_table);

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
	if (filter_list == NULL) {
	    for (n = 0; builtflt[n] != NULL; n++) {
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

    if (ptr != NULL) {
	if (ptr == &f_setglobmode
	    || ptr == &f_delglobmode
	    || ptr == &f_setlocmode
	    || ptr == &f_dellocmode
#if OPT_EVAL
	    || ptr == &f_setvar
	    || ptr == &f_unsetvar
#endif
	    ) {
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

    if (ptr != NULL) {
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

    if (ptr != NULL) {
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

    if (ptr != NULL) {
	if (ptr == &f_define_submode
	    || ptr == &f_remove_submode) {
	    result = 1;
	}
    }
#endif
    return result;
}

int
vl_check_cmd(const void *cmd, unsigned long flags)
{
    const CMDFUNC *actual = (const CMDFUNC *) cmd;
    int result = 0;

    if (actual != NULL) {
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
    FreeAndNull(default_table);
    FreeAndNull(default_attr);
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
