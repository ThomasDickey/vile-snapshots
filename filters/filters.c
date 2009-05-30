/*
 * Common utility functions for vile syntax/highlighter programs
 *
 * $Header: /users/source/archives/vile.vcs/filters/RCS/filters.c,v 1.141 2009/05/29 20:25:36 tom Exp $
 *
 */

#include <filters.h>

#define QUOTE '\''

#ifdef HAVE_LONG_FILE_NAMES
#define KEYFILE_SUFFIX ".keywords"
#else
#define KEYFILE_SUFFIX ".key"
#endif

#if DOT_HIDES_FILE
#define DOT_TO_HIDE_IT "."
#else
#define DOT_TO_HIDE_IT ""
#endif

#define VERBOSE(level,params)	if (FltOptions('v') >= level) mlforce params

#if defined(HAVE_TSEARCH) && defined(HAVE_SEARCH_H)
#define USE_TSEARCH 1
#include <search.h>
#else
#define USE_TSEARCH 0
#endif

#define HASH_LENGTH 256

struct _keyword {
    KEYWORD *kw_next;
    char *kw_name;		/* the keyword name */
    char *kw_attr;		/* its color attributes, if any */
    char *kw_flag;		/* optional flags for the syntax parser */
    unsigned kw_size;		/* strlen(kw_name) */
    unsigned short kw_type;	/* nonzero for classes */
    short kw_used;		/* nonzero for classes */
};

typedef struct _classes CLASS;

struct _classes {
    CLASS *next;
    char *name;
#if USE_TSEARCH
    void *data;
    KEYWORD *keywords;
#else
    KEYWORD **data;
#endif
};

typedef struct {
    int zero_or_more;
    int zero_or_all;
    int meta_ch;
    int eqls_ch;
    char *default_table;
    char *default_attr;
} FLTCHARS;

char *default_table;
char *default_attr;
int zero_or_more = '*';		/* zero or more of the following */
int zero_or_all = '?';		/* zero or all of the following */
int meta_ch = '.';
int eqls_ch = ':';
int vile_keywords;
int flt_options[256];

#if USE_TSEARCH
static void *my_table;
#else
static KEYWORD **my_table;
#endif

static CLASS *classes;
static CLASS *current_class;

/*
 * flt_bfr_*() function data
 */
static char *flt_bfr_text = 0;
static const char *flt_bfr_attr = "";
static unsigned flt_bfr_used = 0;
static unsigned flt_bfr_size = 0;

/*
 * OpenKeywords() function data
 */
static char *str_keyword_name = 0;
static char *str_keyword_file = 0;
static unsigned len_keyword_name = 0;
static unsigned len_keyword_file = 0;

/******************************************************************************
 * Private functions                                                          *
 ******************************************************************************/

#define KW_FLAG(p) ((p) ? ((p)->kw_type ? "class" : "keyword") : "?")

#define COPYIT(name) save->name = name
#define SAVEIT(name) if (name) save->name = strmalloc(name); else save->name = 0

static void
flt_save_chars(FLTCHARS * save)
{
    COPYIT(zero_or_more);
    COPYIT(zero_or_all);
    COPYIT(meta_ch);
    COPYIT(eqls_ch);

    SAVEIT(default_table);
    SAVEIT(default_attr);
}

#undef COPYIT
#undef SAVEIT

static void
flt_init_chars(void)
{
    zero_or_more = '*';
    zero_or_all = '?';
    meta_ch = '.';
    eqls_ch = ':';
}

#define COPYIT(name) name = save->name
#define SAVEIT(name) if (save->name) { FreeIfNeeded(name); name = save->name; }

static void
flt_restore_chars(FLTCHARS * save)
{
    COPYIT(zero_or_more);
    COPYIT(zero_or_all);
    COPYIT(meta_ch);
    COPYIT(eqls_ch);

    SAVEIT(default_table);
    SAVEIT(default_attr);
}

#undef COPYIT
#undef SAVEIT

/* FIXME */
static void
init_data(KEYWORD * data,
	  const char *name,
	  const char *attribute,
	  int classflag,
	  char *flag)
{
    data->kw_name = strmalloc(name);
    data->kw_size = strlen(data->kw_name);
    data->kw_attr = strmalloc(attribute);
    data->kw_flag = (flag != 0) ? strmalloc(flag) : 0;
    data->kw_type = classflag;
    data->kw_used = 2;
}

static void
free_data(KEYWORD * data)
{
    if (data != 0) {
	if (data->kw_name)
	    free(data->kw_name);
	if (data->kw_attr)
	    free(data->kw_attr);
	if (data->kw_flag)
	    free(data->kw_flag);
	free(data);
    }
}

#if USE_TSEARCH
static void
free_node(void *p)
{
    free_data((KEYWORD *) p);
}

static int
compare_data(const void *a, const void *b)
{
    const KEYWORD *p = (const KEYWORD *) a;
    const KEYWORD *q = (const KEYWORD *) b;

    return strcmp(p->kw_name, q->kw_name);
}

static void
flt_tdestroy(void *root, void (*pFreeNode) (void *nodep))
{
    CLASS *p;
    if (root != 0) {
	for (p = classes; p != 0; p = p->next) {
	    if (p->data == root) {
#ifdef HAVE_TDESTROY
		tdestroy(root, pFreeNode);
		p->keywords = 0;
#else
		while (p->keywords != 0) {
		    KEYWORD *q = p->keywords;
		    p->keywords = q->kw_next;
		    if (tdelete(q, &(p->data), compare_data) != 0)
			pFreeNode(q);
		}
#endif
		break;
	    }
	}
    }
}
#endif /* USE_TSEARCH */

static void
CannotAllocate(const char *where)
{
    VERBOSE(1, ("%s: cannot allocate", where));
}

static const char *
AttrsOnce(KEYWORD * entry)
{
    entry->kw_used = 999;
    return NONNULL(entry->kw_attr);
}

static void
ExecAbbrev(char *param)
{
    zero_or_more = *param;
    VERBOSE(1, ("set abbrev '%c'\n", zero_or_more));
}

static void
ExecBrief(char *param)
{
    zero_or_all = *param;
    VERBOSE(1, ("set brief '%c'\n", zero_or_all));
}

static void
ExecClass(char *param)
{
    parse_keyword(param, 1);
}

static void
ExecDefault(char *param)
{
    char *s = skip_ident(param);
    int save = *s;

    *s = 0;
    if (!*param)
	param = NAME_KEYWORD;
    if (is_class(param)) {
	*s = save;
	flt_init_attr(param);
	VERBOSE(1, ("set default_attr '%s' %p\n", default_attr, default_attr));
    } else {
	*s = save;
	VERBOSE(1, ("not a class:%s", param));
    }
}

static void
ExecEquals(char *param)
{
    eqls_ch = *param;
}

static void
ExecMeta(char *param)
{
    meta_ch = *param;
}

/*
 * Include a symbol table from another key-file, merging it into the current
 * table.
 */
static void
ExecMerge(char *param)
{
    int save_meta = meta_ch;
    int save_eqls = eqls_ch;

    flt_read_keywords(param);

    meta_ch = save_meta;
    eqls_ch = save_eqls;
}

/*
 * Include a symbol table from another key-file, creating it in a table named
 * according to the parameter.
 */
static void
ExecSource(char *param)
{
    int save_meta = meta_ch;
    int save_eqls = eqls_ch;

    flt_make_symtab(param);
    flt_read_keywords(MY_NAME);	/* provide default values for this table */
    flt_read_keywords(param);
    set_symbol_table(flt_name());

    meta_ch = save_meta;
    eqls_ch = save_eqls;
}

/*
 * Useful for diverting to another symbol table in the same key-file, for
 * performance.
 */
static void
ExecTable(char *param)
{
    if (*param) {
	flt_make_symtab(param);
	flt_read_keywords(MY_NAME);	/* provide default values for this table */
    } else {
	set_symbol_table(default_table);
    }
}

static KEYWORD *
FindIdentifier(const char *name)
{
    unsigned size;
    KEYWORD *result = 0;

    if (name != 0 && (size = strlen(name)) != 0) {
#if USE_TSEARCH
	KEYWORD find;
	void *pp;

	find.kw_name = (char *) name;
	if ((pp = tfind(&find, &(current_class->data), compare_data)) != 0) {
	    result = *(KEYWORD **) pp;
	}
#else
	int Index = hash_function(name);

	result = my_table[Index];
	while (result != NULL) {
	    if (result->kw_size == size
		&& strcmp(result->kw_name, name) == 0) {
		break;
	    }
	    result = result->kw_next;
	}
#endif /* TSEARCH */
    }
    return result;
}

static void
Free(char *ptr)
{
    if (ptr != 0)
	free(ptr);
}

/*
 * Find the first occurrence of a file in the canonical search list:
 *	the current directory
 *	the home directory
 *	vile's subdirectory of the home directory
 *	the vile library-directory
 *
 * On Unix we look for file/directory names with a "." prefixed since that
 * hides them.
 */
static FILE *
OpenKeywords(char *table_name)
{
#define OPEN_IT(p) if ((fp = fopen(p, "r")) != 0) { \
			VERBOSE(1,("Opened %s", p)); return fp; } else { \
			VERBOSE(2,("..skip %s", p)); }
#define FIND_IT(p) sprintf p; OPEN_IT(str_keyword_name)

    static char suffix[] = KEYFILE_SUFFIX;

    FILE *fp;
    char *path;
    unsigned need;
    char myLeaf[20];

    need = sizeof(suffix) + strlen(table_name) + 2;
    str_keyword_file = do_alloc(str_keyword_file, need, &len_keyword_file);
    if (str_keyword_file == 0) {
	CannotAllocate("OpenKeywords");
	return 0;
    }
    sprintf(str_keyword_file, "%s%s", table_name, suffix);

    if (strchr(str_keyword_file, PATHSEP) != 0) {
	OPEN_IT(str_keyword_file);
    }

    if ((path = home_dir()) == 0)
	path = "";

    need = strlen(path)
	+ strlen(str_keyword_file)
	+ 20;

    str_keyword_name = do_alloc(str_keyword_name, need, &len_keyword_name);
    if (str_keyword_name == 0) {
	CannotAllocate("OpenKeywords");
	return 0;
    }

    FIND_IT((str_keyword_name, "%s%c%s%s", PATHDOT, PATHSEP, DOT_TO_HIDE_IT, str_keyword_file));
    FIND_IT((str_keyword_name, "%s%c%s%s", path, PATHSEP, DOT_TO_HIDE_IT, str_keyword_file));
    sprintf(myLeaf, "%s%s%c", DOT_TO_HIDE_IT, MY_NAME, PATHSEP);

    FIND_IT((str_keyword_name, "%s%c%s%s", path, PATHSEP, myLeaf, str_keyword_file));

    path = vile_getenv("VILE_STARTUP_PATH");
#ifdef VILE_STARTUP_PATH
    if (path == 0)
	path = VILE_STARTUP_PATH;
#endif
    if (path != 0) {
	int n = 0, m;

	need = strlen(path) + strlen(str_keyword_file) + 2;
	str_keyword_name = do_alloc(str_keyword_name, need, &len_keyword_name);
	if (str_keyword_name == 0) {
	    CannotAllocate("OpenKeywords");
	    return 0;
	}
	while (path[n] != 0) {
	    for (m = n; path[m] != 0 && path[m] != PATHCHR; m++)
		/*LOOP */ ;
	    FIND_IT((str_keyword_name, "%.*s%c%s", m - n, path + n, PATHSEP, str_keyword_file));
	    if (path[m])
		n = m + 1;
	    else
		n = m;
	}
    }

    return 0;
}

static int
ParseDirective(char *line)
{
    static struct {
	const char *name;
	void (*func) (char *param);
    } table[] = {
	/* *INDENT-OFF* */
	{ "abbrev",  ExecAbbrev   },
	{ "brief",   ExecBrief    },
	{ "class",   ExecClass    },
	{ "default", ExecDefault  },
	{ "equals",  ExecEquals   },
	{ "include", flt_read_keywords },
	{ "merge",   ExecMerge    },
	{ "meta",    ExecMeta     },
	{ "source",  ExecSource   },
	{ "table",   ExecTable    },
	/* *INDENT-ON* */

    };
    unsigned n, len;

    if (*(line = skip_blanks(line)) == meta_ch) {
	line = skip_blanks(line + 1);
	if ((len = (skip_ident(line) - line)) != 0) {
	    for (n = 0; n < sizeof(table) / sizeof(table[0]); n++) {
		if (!strncmp(line, table[n].name, len)) {
		    (*table[n].func) (skip_blanks(line + len));
		    break;
		}
	    }
	}
	return 1;
    }
    return 0;
}

static unsigned
TrimBlanks(char *src)
{
    unsigned len = strlen(src);

    while (len != 0
	   && isspace(CharOf(src[len - 1])))
	src[--len] = 0;
    return (len);
}

/******************************************************************************
 * Public functions                                                           *
 ******************************************************************************/

const char *
ci_keyword_attr(const char *text)
{
    return keyword_attr(lowercase_of(text));
}

const char *
ci_keyword_flag(const char *text)
{
    return keyword_flag(lowercase_of(text));
}

char *
class_attr(char *name)
{
    KEYWORD *data;
    char *result = 0;

    while ((data = is_class(name)) != 0) {
	VERBOSE(data->kw_used, ("class_attr(%s) = %s",
				name, AttrsOnce(data)));
	name = result = data->kw_attr;
	VERBOSE(1, ("-> %p\n", result));
    }
    return result;
}

void *
flt_alloc(void *ptr, unsigned need, unsigned *have, unsigned size)
{
    need += (2 * size);		/* allow for trailing null, etc */
    if ((need > *have) || (ptr == 0)) {
	need *= 2;
	if (ptr != 0)
	    ptr = realloc(ptr, need);
	else
	    ptr = malloc(need);
	*have = need;
    }
    return ptr;
}

/*
 * The flt_bfr_*() functions are used for managing a possibly multi-line buffer
 * that will be written as one attributed region.
 */
void
flt_bfr_append(char *text, int length)
{
    flt_bfr_text = do_alloc(flt_bfr_text, flt_bfr_used + length, &flt_bfr_size);
    if (flt_bfr_text != 0) {
	strncpy(flt_bfr_text + flt_bfr_used, text, length);
	flt_bfr_used += length;
    } else {
	CannotAllocate("flt_bfr_append");
    }
}

void
flt_bfr_begin(const char *attr)
{
    flt_bfr_finish();
    flt_bfr_attr = attr;
}

void
flt_bfr_embed(char *text, int length, const char *attr)
{
    const char *save = flt_bfr_attr;

    if ((save == 0 && attr == 0) ||
	(save != 0 && attr != 0 && !strcmp(save, attr))) {
	flt_bfr_append(text, length);
    } else {
	flt_bfr_finish();
	flt_puts(text, length, attr);
	flt_bfr_attr = save;
    }
}

void
flt_bfr_error(void)
{
    if (flt_bfr_used) {
	flt_error("unterminated buffer");
	flt_bfr_attr = class_attr(NAME_ERROR);
	flt_bfr_finish();
    }
}

void
flt_bfr_finish(void)
{
    if (flt_bfr_used) {
	flt_puts(flt_bfr_text, flt_bfr_used,
		 (flt_bfr_attr
		  ? flt_bfr_attr
		  : ""));
    }
    flt_bfr_used = 0;
    flt_bfr_attr = "";
}

int
flt_bfr_length(void)
{
    return flt_bfr_used;
}

static CLASS *
find_symtab(char *table_name)
{
    CLASS *p;
    for (p = classes; p != 0; p = p->next) {
	if (!strcmp(p->name, table_name)) {
	    break;
	}
    }
    return p;
}

/*----------------------------------------------------------------------------*/

static int dump_meta_ch;
static int dump_eqls_ch;

static void
dump_init(void)
{
    dump_meta_ch = meta_ch;
    dump_eqls_ch = eqls_ch;

    if (dump_meta_ch != '.')
	flt_message(".meta %c\n", dump_meta_ch);
    if (dump_eqls_ch != ':')
	flt_message("%cequals %c\n", dump_meta_ch, dump_eqls_ch);
}

static int
dump_update2(const char *name, int keep)
{
    int result;

    for (result = 32; result < 127; ++result) {
	if (result != keep && ispunct(result) && strchr(name, result) == 0) {
	    break;
	}
    }

    return result;
}

/*
 * emit "meta" and "equals" directives if the name contains the current meta or
 * equals characters.
 */
static void
dump_update(const char *name)
{
    int newc;

    if (strchr(name, dump_meta_ch) != 0) {
	newc = dump_update2(name, dump_eqls_ch);
	flt_message("%cmeta %c\n", dump_meta_ch, newc);
	dump_meta_ch = newc;
    }
    if (strchr(name, dump_eqls_ch) != 0) {
	dump_eqls_ch = dump_update2(name, dump_meta_ch);
	flt_message("%cequals %c\n", dump_meta_ch, dump_eqls_ch);
    }
}

static void
dump_class(const KEYWORD * const q)
{
    if (q->kw_type) {
	dump_update(q->kw_name);
	flt_message("%cclass %s%c%s",
		    dump_meta_ch,
		    q->kw_name,
		    dump_eqls_ch,
		    NonNull(q->kw_attr));
	if (q->kw_flag)
	    flt_message("%c%s",
			dump_eqls_ch,
			q->kw_flag);
	flt_message("\n");
    }
}

static void
dump_keyword(const KEYWORD * const q)
{
    if (!q->kw_type) {
	dump_update(q->kw_name);
	flt_message("%s%c%s",
		    q->kw_name,
		    dump_eqls_ch,
		    NonNull(q->kw_attr));
	if (q->kw_flag)
	    flt_message("%c%s",
			dump_eqls_ch,
			q->kw_flag);
	flt_message("\n");
    }
}

typedef void DumpFunc(const KEYWORD * const);

#if USE_TSEARCH
static DumpFunc *dump_func;

static void
dump_walker(const void *nodep, const VISIT which, const int depth)
{
    const KEYWORD *const p = *(KEYWORD * const *) nodep;
    (void) depth;
    if (which == postorder || which == leaf)
	dump_func(p);
}
#endif

static void
dump_symtab(CLASS * p, DumpFunc dumpFunc)
{
#if USE_TSEARCH
    dump_func = dumpFunc;
    twalk(p->data, dump_walker);
#else
    int i;
    KEYWORD *q;

    for (i = 0; i < HASH_LENGTH; i++) {
	for (q = p->data[i]; q != 0; q = q->kw_next) {
	    dumpFunc(q);
	}
    }
#endif
}

void
flt_dump_symtab(char *table_name)
{
    CLASS *p;

    if (table_name == 0) {
	dump_init();
	for (p = classes; p != 0; p = p->next) {
	    flt_dump_symtab(p->name);
	}
    } else if ((p = find_symtab(table_name)) != 0) {
	dump_update(p->name);
	flt_message("%ctable %s\n", dump_meta_ch, p->name);
	dump_symtab(p, dump_class);
	dump_symtab(p, dump_keyword);
    }
}

/*----------------------------------------------------------------------------*/

void
flt_free(char **p, unsigned *len)
{
    FreeAndNull(*p);
    *len = 0;
}

void
flt_free_keywords(char *table_name)
{
    CLASS *p, *q;
#if !USE_TSEARCH
    KEYWORD *ptr;
    int i;
#endif

    VERBOSE(1, ("flt_free_keywords(%s)", table_name));
    for (p = classes, q = 0; p != 0; q = p, p = p->next) {
	if (!strcmp(table_name, p->name)) {
#if USE_TSEARCH
	    flt_tdestroy(p->data, free_node);
#else
	    my_table = p->data;
	    for (i = 0; i < HASH_LENGTH; i++) {
		while ((ptr = my_table[i]) != 0) {
		    my_table[i] = ptr->kw_next;
		    free_data(ptr);
		}
	    }
	    free(p->data);
#endif

	    free(p->name);
	    if (q != 0)
		q->next = p->next;
	    else
		classes = p->next;
	    free(p);
	    break;
	}
    }
    my_table = (classes != 0) ? classes->data : 0;
    current_class = classes;
}

void
flt_free_symtab(void)
{
    while (classes != 0)
	flt_free_keywords(classes->name);
}

void
flt_init_table(char *table_name)
{
    if (default_table != 0)
	free(default_table);
    default_table = strmalloc(table_name);

    VERBOSE(3, ("flt_init_table:%s", table_name));
}

void
flt_init_attr(char *attr_name)
{
    if (default_attr != 0)
	free(default_attr);
    default_attr = strmalloc(attr_name);

    VERBOSE(3, ("flt_init_attr:%s", attr_name));
}

/*
 * We drop the old symbol table each time we read the data, to allow keywords
 * to be removed from the table.  Also, it is necessary for some filters such
 * as m4 to be able to restart to a known state.
 */
void
flt_initialize(char *table_name)
{
    flt_init_table(table_name);
    flt_init_attr(NAME_KEYWORD);
    flt_init_chars();

    flt_free_symtab();
    flt_make_symtab(table_name);
}

void
flt_make_symtab(char *table_name)
{
    if (*table_name == '\0') {
	table_name = default_table;
    }

    if (!set_symbol_table(table_name)) {
	CLASS *p;

	if ((p = typecallocn(CLASS, 1)) == 0) {
	    CannotAllocate("flt_make_symtab");
	    return;
	}

	p->name = strmalloc(table_name);
	if (p->name == 0) {
	    free(p);
	    CannotAllocate("flt_make_symtab");
	    return;
	}
#if !USE_TSEARCH
	p->data = typecallocn(KEYWORD *, HASH_LENGTH);
	if (p->data == 0) {
	    free(p->name);
	    free(p);
	    CannotAllocate("flt_make_symtab");
	    return;
	}
#endif

	p->next = classes;
	classes = p;
	my_table = p->data;
	current_class = p;

	VERBOSE(1, ("flt_make_symtab(%s)", table_name));

	/*
	 * Mark all of the standard predefined classes when we first create a
	 * symbol table.  Some filters may define their own special classes,
	 * and not all filters use all of these classes, but it's a lot simpler
	 * than putting the definitions into every ".key" file.
	 */
	insert_keyword(NAME_ACTION, ATTR_ACTION, 1);
	insert_keyword(NAME_COMMENT, ATTR_COMMENT, 1);
	insert_keyword(NAME_ERROR, ATTR_ERROR, 1);
	insert_keyword(NAME_IDENT, ATTR_IDENT, 1);
	insert_keyword(NAME_IDENT2, ATTR_IDENT2, 1);
	insert_keyword(NAME_KEYWORD, ATTR_KEYWORD, 1);
	insert_keyword(NAME_KEYWRD2, ATTR_KEYWRD2, 1);
	insert_keyword(NAME_LITERAL, ATTR_LITERAL, 1);
	insert_keyword(NAME_NUMBER, ATTR_NUMBER, 1);
	insert_keyword(NAME_PREPROC, ATTR_PREPROC, 1);
	insert_keyword(NAME_TYPES, ATTR_TYPES, 1);
    }
}

void
flt_setup_symbols(char *table_name)
{
    if (!set_symbol_table(table_name)) {
	flt_make_symtab(table_name);
	flt_read_keywords(MY_NAME);
	flt_read_keywords(table_name);
	/* set back to the name we asked for, in case reader switched */
	set_symbol_table(table_name);
    }
}

void
flt_read_keywords(char *table_name)
{
    FILE *kwfile;
    char *line = 0;
    char *name;
    unsigned line_len = 0;
    FLTCHARS fltchars;

    VERBOSE(1, ("flt_read_keywords(%s)", table_name));

    flt_save_chars(&fltchars);
    flt_init_chars();

    if ((kwfile = OpenKeywords(table_name)) != NULL) {
	int linenum = 0;
	while (readline(kwfile, &line, &line_len) != 0) {

	    name = skip_blanks(line);
	    if (TrimBlanks(name) == 0)
		continue;
	    if (ParseDirective(name))
		continue;

	    VERBOSE(2, ("line %3d:", ++linenum));
	    parse_keyword(name, 0);
	}
	fclose(kwfile);
    }
    Free(line);

    flt_restore_chars(&fltchars);
}

char *
get_symbol_table(void)
{
#if USE_TSEARCH
    if (current_class != 0)
	return current_class->name;
#else
    CLASS *p;
    for (p = classes; p != 0; p = p->next) {
	if (my_table == p->data) {
	    return p->name;
	}
    }
#endif
    return "?";
}

long
hash_function(const char *id)
{
    /*
     * Build more elaborate hashing scheme. If you want one.
     */
    if ((id[0] == 0) || (id[1] == 0))
	return (CharOf(id[0]));

    return ((CharOf(id[0]) ^ (CharOf(id[1]) << 3) ^ (CharOf(id[2]) >> 1)) & 0xff);
}

static KEYWORD *
alloc_keyword(const char *ident, const char *attribute, int classflag, char *flag)
{
    KEYWORD *nxt;
    int Index;

    if ((nxt = FindIdentifier(ident)) != 0) {
	Free(nxt->kw_attr);
	if ((nxt->kw_attr = strmalloc(attribute)) == NULL) {
	    free(nxt);
	    nxt = 0;
	}
    } else {
	nxt = NULL;
	Index = hash_function(ident);
	if ((nxt = typecallocn(KEYWORD, 1)) != NULL) {
	    init_data(nxt, ident, attribute, classflag, flag);

	    if (nxt->kw_name != 0
		&& nxt->kw_attr != 0) {
#if USE_TSEARCH
		void **pp;
		pp = (void **) tsearch(nxt, &(current_class->data), compare_data);
		if (pp != 0) {
		    nxt = *(KEYWORD **) pp;
		} else {
		    nxt = 0;
		}
		if (nxt != 0) {
		    nxt->kw_next = current_class->keywords;
		    current_class->keywords = nxt;
		}
#else
		nxt->kw_next = my_table[Index];
		my_table[Index] = nxt;
#endif
	    } else {
		free_data(nxt);
		nxt = 0;
	    }
	}
    }
    return nxt;
}

static void
insert_keyword2(const char *ident, const char *attribute, int classflag, char *flag)
{
    KEYWORD *nxt;
    char *mark;
    char *temp;

    VERBOSE(2, ("insert_keyword(%s, %s, %d, %s)\n",
		ident,
		NONNULL(attribute),
		classflag,
		NONNULL(flag)));

    if (zero_or_more
	&& (mark = strchr(ident, zero_or_more)) != 0
	&& (mark != ident)) {
	if ((temp = strmalloc(ident)) != 0) {

	    mark = temp + (mark - ident);
	    while (*mark == zero_or_more) {
		*mark = 0;
		insert_keyword2(temp, attribute, classflag, flag);
		if ((mark[0] = mark[1]) != 0) {
		    *(++mark) = zero_or_more;
		}
	    }
	    free(temp);
	} else {
	    CannotAllocate("insert_keyword");
	}
    } else if (zero_or_all
	       && (mark = strchr(ident, zero_or_all)) != 0
	       && (mark != ident)) {
	if ((temp = strmalloc(ident)) != 0) {

	    mark = temp + (mark - ident);
	    if (*mark == zero_or_all) {
		*mark = 0;
		insert_keyword2(temp, attribute, classflag, flag);
		while ((mark[0] = mark[1]) != 0)
		    ++mark;
		insert_keyword2(temp, attribute, classflag, flag);
	    }
	    free(temp);
	} else {
	    CannotAllocate("insert_keyword");
	}
    } else if ((nxt = alloc_keyword(ident, attribute, classflag, flag)) == 0) {
	CannotAllocate("insert_keyword");
    } else {
	VERBOSE(3, ("...\tname \"%s\"\tattr \"%s\"",
		    nxt->kw_name,
		    NONNULL(nxt->kw_attr)));
    }
}

void
insert_keyword(const char *ident, const char *attribute, int classflag)
{
    insert_keyword2(ident, attribute, classflag, 0);
}

KEYWORD *
is_class(const char *name)
{
    KEYWORD *result = FindIdentifier(name);
    if (result != 0) {
	if (result->kw_type == 0) {
	    result = 0;
	}
    }
    return result;
}

KEYWORD *
is_keyword(const char *name)
{
    KEYWORD *result;
    if ((result = FindIdentifier(name)) != 0
	&& result->kw_type == 0) {
	return result;
    }
    return 0;
}

const char *
keyword_attr(const char *name)
{
    KEYWORD *data = keyword_data(name);
    const char *result = 0;

    if (data != 0) {
	result = data->kw_attr;
    }
    VERBOSE(1, ("keyword_attr(%s) = %p %s\n", name, result, NONNULL(result)));
    return result;
}

KEYWORD *
keyword_data(const char *name)
{
    KEYWORD *data = is_keyword(name);
    KEYWORD *result = 0;

    if (data != 0) {
	result = data;
	name = data->kw_attr;
	while ((data = is_class(name)) != 0) {
	    result = data;
	    name = data->kw_attr;
	}
    }
    return result;
}

const char *
keyword_flag(const char *name)
{
    KEYWORD *data = keyword_data(name);
    const char *result = 0;

    if (data != 0) {
	result = data->kw_flag;
    }
    VERBOSE(1, ("keyword_flag(%s) = %p %s\n", name, result, NONNULL(result)));
    return result;
}

const char *
lowercase_of(const char *text)
{
    static char *name;
    static unsigned used;
    unsigned n;
    const char *result;

#if NO_LEAKS
    if (text == 0) {
	flt_free(&name, &used);
	result = 0;
    } else
#endif
    if ((name = do_alloc(name, strlen(text), &used)) != 0) {
	for (n = 0; text[n] != 0; n++) {
	    if (isalpha(CharOf(text[n])) && isupper(CharOf(text[n])))
		name[n] = tolower(CharOf(text[n]));
	    else
		name[n] = text[n];
	}
	name[n] = 0;
	result = name;
    } else {
	result = text;		/* not good, but nonfatal */
	CannotAllocate("lowercase_of");
    }
    return result;
}

void
parse_keyword(char *name, int classflag)
{
    char *args = 0;
    char *flag = 0;
    char *s, *t;
    int quoted = 0;

    VERBOSE(1, ("parse_keyword(%s, %d)\n", name, classflag));
    if ((s = strchr(name, eqls_ch)) != 0) {
	*s++ = 0;
	s = skip_blanks(s);
	if (*s != 0) {
	    args = t = s;
	    while (*s != 0) {
		if (quoted) {
		    if (*s == QUOTE) {
			if (*++s != QUOTE)
			    quoted = 0;
		    }
		} else {
		    if (*s == QUOTE) {
			quoted = 1;
			s++;
		    } else if (!isalnum(CharOf(*s))) {
			if (!*name) {
			    args = 0;	/* comment: ignore */
			} else if (*s == eqls_ch) {
			    flag = s;
			    *flag++ = '\0';
			    VERBOSE(2, ("found flags \"%s\" for %s\n", flag, name));
			    /* an empty color field inherits the default */
			    TrimBlanks(args);
			    if (*args == 0)
				args = default_attr;
			} else {
			    VERBOSE(1, ("unexpected:%s%c%s\n", name,
					eqls_ch, s));
			    args = 0;	/* error: ignore */
			}
			break;
		    }
		}
		*t++ = *s++;
	    }
	    *t = 0;
	}
	TrimBlanks(name);
    }

    VERBOSE(2, ("parsed\tname \"%s\"\tattr \"%s\"%s",
		name,
		NONNULL(args),
		(classflag || is_class(name)) ? " - class" : ""));

    if (*name && args) {
	insert_keyword2(name, args, classflag, flag);
    } else if (*name) {
	KEYWORD *data;
	if (args == 0) {
	    args = default_attr;
	    VERBOSE(2, ("using attr \"%s\"", args));
	}
	if (strcmp(name, args)
	    && (data = FindIdentifier(args)) != 0) {
	    /*
	     * Insert the classname rather than the data->kw_attr value,
	     * since insert_keyword makes a copy of the string we pass to it.
	     * Retrieving the attribute from the copy will give the unique
	     * attribute string belonging to the class, so it is possible at
	     * runtime to compare pointers from keyword_attr() to determine if
	     * two tokens have the same class.  sql-filt.l uses this feature.
	     */
	    insert_keyword2(name, args, classflag, flag);
	}
    }
}

char *
readline(FILE *fp, char **ptr, unsigned *len)
{
    char *buf = *ptr;
    unsigned used = 0;

    if (buf == 0) {
	*len = BUFSIZ;
	buf = typeallocn(char, *len);
    }
    while (!feof(fp)) {
	int ch = vl_getc(fp);
	if (ch == EOF || feof(fp) || ferror(fp)) {
	    break;
	}
	if (used + 2 >= *len) {
	    *len = 3 * (*len) / 2;
	    buf = typereallocn(char, buf, *len);
	    if (buf == 0)
		return 0;
	}
	buf[used++] = ch;
	if (ch == '\n')
	    break;
    }
    buf[used] = '\0';
    *ptr = buf;
    return used ? *ptr : 0;
}

int
set_symbol_table(const char *table_name)
{
    CLASS *p;
    for (p = classes; p != 0; p = p->next) {
	if (!strcmp(table_name, p->name)) {
	    my_table = p->data;
	    current_class = p;
	    VERBOSE(3, ("set_symbol_table:%s", table_name));
	    return 1;
	}
    }
    return 0;
}

char *
skip_ident(char *src)
{
    while (*src != '\0' && isprint(CharOf(*src)) && !isspace(CharOf(*src))) {
	if (*src == eqls_ch
	    || *src == meta_ch) {
	    break;
	}
	src++;
    }
    return (src);
}

int
yywrap(void)
{
    return 1;
}

#if NO_LEAKS
void
filters_leaks(void)
{
    flt_free_symtab();
    flt_free(&str_keyword_name, &len_keyword_name);
    flt_free(&str_keyword_file, &len_keyword_file);

    flt_free(&flt_bfr_text, &flt_bfr_size);
    (void) lowercase_of(0);
    FreeAndNull(default_table);
    FreeAndNull(default_attr);
}
#endif
