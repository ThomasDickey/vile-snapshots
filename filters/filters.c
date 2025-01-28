/*
 * Common utility functions for vile syntax/highlighter programs
 *
 * $Id: filters.c,v 1.170 2025/01/27 23:12:42 tom Exp $
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
    size_t kw_size;		/* strlen(kw_name) */
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
static char *flt_bfr_text = NULL;
static const char *flt_bfr_attr = "";
static size_t flt_bfr_used = 0;
static size_t flt_bfr_size = 0;

/*
 * OpenKeywords() function data
 */
static char *str_keyword_name = NULL;
static char *str_keyword_file = NULL;
static size_t len_keyword_name = 0;
static size_t len_keyword_file = 0;

/******************************************************************************
 * Private functions                                                          *
 ******************************************************************************/

#define KW_FLAG(p) ((p) ? ((p)->kw_type ? "class" : "keyword") : "?")

#define COPYIT(name) save->name = name
#define SAVEIT(name) if (name) save->name = strmalloc(name); else save->name = NULL

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
    data->kw_flag = (flag != NULL) ? strmalloc(flag) : NULL;
    data->kw_type = (unsigned short) classflag;
    data->kw_used = 2;
}

static void
free_data(KEYWORD * data)
{
    if (data != NULL) {
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
    if (root != NULL) {
	for (p = classes; p != NULL; p = p->next) {
	    if (p->data == root) {
		while (p->keywords != NULL) {
		    KEYWORD *q = p->keywords;
		    p->keywords = q->kw_next;
		    if (tdelete(q, &(p->data), compare_data) != NULL)
			pFreeNode(q);
		}
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
ExecAbbrev(const char *param)
{
    zero_or_more = *param;
    VERBOSE(1, ("set abbrev '%c'", zero_or_more));
}

static void
ExecBrief(const char *param)
{
    zero_or_all = *param;
    VERBOSE(1, ("set brief '%c'", zero_or_all));
}

static void
ExecClass(const char *param)
{
    char *temp = strmalloc(param);
    parse_keyword(temp, 1);
    free(temp);
}

static void
ExecDefault(const char *param)
{
    char *temp = strmalloc(param);
    char *s = skip_ident(temp);
    int save = *s;
    int isClass;

    *s = 0;
    if (!*temp) {
	free(temp);
	temp = strmalloc(NAME_KEYWORD);
	isClass = (is_class(temp) != NULL);
    } else {
	isClass = (is_class(temp) != NULL);
	*s = (char) save;
    }

    if (isClass) {
	flt_init_attr(temp);
	VERBOSE(1, ("set default_attr '%s' %p", default_attr, (void *) default_attr));
    } else {
	VERBOSE(1, ("not a class:%s", temp));
    }

    free(temp);
}

static void
ExecEquals(const char *param)
{
    eqls_ch = *param;
}

static void
ExecMeta(const char *param)
{
    meta_ch = *param;
}

/*
 * Include a symbol table from another key-file, merging it into the current
 * table.
 */
static void
ExecMerge(const char *param)
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
ExecSource(const char *param)
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
ExecTable(const char *param)
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
    KEYWORD *result = NULL;

#if USE_TSEARCH
    if (name != NULL && strlen(name) != 0) {
	KEYWORD find;
	void *pp;

	find.kw_name = (char *) name;
	if ((pp = tfind(&find, &(current_class->data), compare_data)) != NULL) {
	    result = *(KEYWORD **) pp;
	}
    }
#else
    size_t size;
    if (name != 0 && (size = strlen(name)) != 0) {
	int Index = hash_function(name);

	result = my_table[Index];
	while (result != NULL) {
	    if (result->kw_size == size
		&& strcmp(result->kw_name, name) == 0) {
		break;
	    }
	    result = result->kw_next;
	}
    }
#endif /* TSEARCH */
    return result;
}

static void
Free(char *ptr)
{
    if (ptr != NULL)
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
OpenKeywords(const char *table_name)
{
#define OPEN_IT(p) if ((fp = fopen(p, "r")) != NULL) { \
			VERBOSE(1,("Opened %s", p)); return fp; } else { \
			VERBOSE(2,("..skip %s", p)); }
#define FIND_IT(p) sprintf p; OPEN_IT(str_keyword_name)

    static char suffix[] = KEYFILE_SUFFIX;

    FILE *fp;
    const char *path;
    size_t need;
    char myLeaf[20];

    need = sizeof(suffix) + strlen(table_name) + 2;
    str_keyword_file = do_alloc(str_keyword_file, need, &len_keyword_file);
    if (str_keyword_file == NULL) {
	CannotAllocate("OpenKeywords");
	return NULL;
    }
    sprintf(str_keyword_file, "%s%s", table_name, suffix);

    if (strchr(str_keyword_file, PATHSEP) != NULL) {
	OPEN_IT(str_keyword_file);
    }

    if ((path = home_dir()) == NULL)
	path = "";

    need = strlen(path)
	+ strlen(str_keyword_file)
	+ 20;

    str_keyword_name = do_alloc(str_keyword_name, need, &len_keyword_name);
    if (str_keyword_name == NULL) {
	CannotAllocate("OpenKeywords");
	return NULL;
    }

    FIND_IT((str_keyword_name, "%s%c%s%s", PATHDOT, PATHSEP, DOT_TO_HIDE_IT, str_keyword_file));
    FIND_IT((str_keyword_name, "%s%c%s%s", path, PATHSEP, DOT_TO_HIDE_IT, str_keyword_file));
    sprintf(myLeaf, "%s%s%c", DOT_TO_HIDE_IT, MY_NAME, PATHSEP);

    FIND_IT((str_keyword_name, "%s%c%s%s", path, PATHSEP, myLeaf, str_keyword_file));

    path = vile_getenv("VILE_STARTUP_PATH");
#ifdef VILE_STARTUP_PATH
    if (path == NULL)
	path = VILE_STARTUP_PATH;
#endif
    if (path != NULL) {
	int n = 0, m;

	need = strlen(path) + strlen(str_keyword_file) + 2;
	str_keyword_name = do_alloc(str_keyword_name, need, &len_keyword_name);
	if (str_keyword_name == NULL) {
	    CannotAllocate("OpenKeywords");
	    return NULL;
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

    return NULL;
}

static int
ParseDirective(char *line)
{
    static struct {
	const char *name;
	void (*func) (const char *param);
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
    size_t n, len;

    if (*(line = skip_blanks(line)) == meta_ch) {
	line = skip_blanks(line + 1);
	if ((len = (size_t) (skip_ident(line) - line)) != 0) {
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

static size_t
TrimBlanks(char *src)
{
    size_t len = strlen(src);

    while (len != 0
	   && isspace(CharOf(src[len - 1])))
	src[--len] = 0;
    return (len);
}

/******************************************************************************
 * Public functions                                                           *
 ******************************************************************************/

int
ci_compare(const char *a, const char *b)
{
    int ch1, ch2, cmp;

    for (;;) {
	ch1 = CharOf(*a++);
	ch2 = CharOf(*b++);
	if (islower(ch1))
	    ch1 = toupper(ch1);
	if (islower(ch2))
	    ch2 = toupper(ch2);
	cmp = ch1 - ch2;
	if (cmp != 0 || ch1 == EOS || ch2 == EOS)
	    return cmp;
    }
}

const char *
ci_keyword_attr(const char *text)
{
    return keyword_attr(lowercase_of(text));
}

/*
 * The "-i" option toggles between case-independent and case-dependent.
 */
const char *
get_keyword_attr(const char *text)
{
    return keyword_attr((FltOptions('i') & 1) ? lowercase_of(text) : text);
}

const char *
ci_keyword_flag(const char *text)
{
    return keyword_flag(lowercase_of(text));
}

char *
class_attr(const char *name)
{
    KEYWORD *data;
    char *result = NULL;

    while ((data = is_class(name)) != NULL) {
	VERBOSE(data->kw_used, ("class_attr(%s) = %s",
				name, AttrsOnce(data)));
	name = result = data->kw_attr;
	VERBOSE(1, ("-> %p", (void *) result));
    }
    return result;
}

void *
flt_alloc(void *ptr, size_t need, size_t *have, size_t size)
{
    need += (2 * size);		/* allow for trailing null, etc */
    if ((need > *have) || (ptr == NULL)) {
	need *= 2;
	if (ptr != NULL)
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
flt_bfr_append(const char *text, int length)
{
    flt_bfr_text = do_alloc(flt_bfr_text, flt_bfr_used + (size_t) length, &flt_bfr_size);
    if (flt_bfr_text != NULL) {
	strncpy(flt_bfr_text + flt_bfr_used, text, (size_t) length);
	flt_bfr_used += (unsigned) length;
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
flt_bfr_embed(const char *text, int length, const char *attr)
{
    const char *save = flt_bfr_attr;

    if ((save == NULL && attr == NULL) ||
	(save != NULL && attr != NULL && !strcmp(save, attr))) {
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
	flt_puts(flt_bfr_text, (int) flt_bfr_used,
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
    return (int) flt_bfr_used;
}

static CLASS *
find_symtab(const char *table_name)
{
    CLASS *p;
    for (p = classes; p != NULL; p = p->next) {
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
	if (result != keep && ispunct(result) && vl_index(name, result) == NULL) {
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

    if (vl_index(name, dump_meta_ch) != NULL) {
	newc = dump_update2(name, dump_eqls_ch);
	flt_message("%cmeta %c\n", dump_meta_ch, newc);
	dump_meta_ch = newc;
    }
    if (vl_index(name, dump_eqls_ch) != NULL) {
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
flt_dump_symtab(const char *table_name)
{
    CLASS *p;

    if (table_name == NULL) {
	dump_init();
	for (p = classes; p != NULL; p = p->next) {
	    flt_dump_symtab(p->name);
	}
    } else if ((p = find_symtab(table_name)) != NULL) {
	dump_update(p->name);
	flt_message("%ctable %s\n", dump_meta_ch, p->name);
	dump_symtab(p, dump_class);
	dump_symtab(p, dump_keyword);
    }
}

/*----------------------------------------------------------------------------*/

void
flt_free(char **p, size_t *len)
{
    FreeAndNull(*p);
    *len = 0;
}

void
flt_free_keywords(const char *table_name)
{
    CLASS *p, *q;
#if !USE_TSEARCH
    KEYWORD *ptr;
    int i;
#endif

    VERBOSE(1, ("flt_free_keywords(%s)", table_name));
    for (p = classes, q = NULL; p != NULL; q = p, p = p->next) {
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
	    if (q != NULL)
		q->next = p->next;
	    else
		classes = p->next;
	    free(p);
	    break;
	}
    }
    my_table = (classes != NULL) ? classes->data : NULL;
    current_class = classes;
}

void
flt_free_symtab(void)
{
    while (classes != NULL)
	flt_free_keywords(classes->name);
}

void
flt_init_table(const char *table_name)
{
    if (default_table != NULL)
	FreeAndNull(default_table);
    default_table = strmalloc(table_name);

    VERBOSE(3, ("flt_init_table:%s", table_name));
}

void
flt_init_attr(const char *attr_name)
{
    if (default_attr != NULL)
	FreeAndNull(default_attr);
    default_attr = strmalloc(attr_name);

    VERBOSE(3, ("flt_init_attr:%s", attr_name));
}

/*
 * We drop the old symbol table each time we read the data, to allow keywords
 * to be removed from the table.  Also, it is necessary for some filters such
 * as m4 to be able to restart to a known state.
 */
void
flt_initialize(const char *table_name)
{
    flt_init_table(table_name);
    flt_init_attr(NAME_KEYWORD);
    flt_init_chars();

    flt_free_symtab();
    flt_make_symtab(table_name);
}

void
flt_make_symtab(const char *table_name)
{
    if (*table_name == '\0') {
	table_name = default_table;
    }

    if (!set_symbol_table(table_name)) {
	CLASS *p;

	if ((p = typecallocn(CLASS, (size_t) 1)) == NULL) {
	    CannotAllocate("flt_make_symtab");
	    return;
	}

	p->name = strmalloc(table_name);
	if (p->name == NULL) {
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
flt_setup_symbols(const char *table_name)
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
flt_read_keywords(const char *table_name)
{
    FILE *kwfile;
    char *line = NULL;
    char *name;
    size_t line_len = 0;
    FLTCHARS fltchars;

    VERBOSE(1, ("flt_read_keywords(%s)", table_name));

    flt_save_chars(&fltchars);
    flt_init_chars();

    if ((kwfile = OpenKeywords(table_name)) != NULL) {
	int linenum = 0;
	while (readline(kwfile, &line, &line_len) != NULL) {

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

const char *
get_symbol_table(void)
{
#if USE_TSEARCH
    if (current_class != NULL)
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

    if ((nxt = FindIdentifier(ident)) != NULL) {
	char *new_attr = strmalloc(attribute);
	if (new_attr != NULL) {
	    Free(nxt->kw_attr);
	    nxt->kw_attr = new_attr;
	} else {
	    nxt = NULL;
	}
    } else {
	nxt = NULL;
	if ((nxt = typecallocn(KEYWORD, (size_t) 1)) != NULL) {
	    init_data(nxt, ident, attribute, classflag, flag);

	    if (nxt->kw_name != NULL
		&& nxt->kw_attr != NULL) {
#if USE_TSEARCH
		void **pp;
		pp = (void **) tsearch(nxt, &(current_class->data), compare_data);
		if (pp != NULL) {
		    nxt = *(KEYWORD **) pp;
		} else {
		    nxt = NULL;
		}
		if (nxt != NULL) {
		    nxt->kw_next = current_class->keywords;
		    current_class->keywords = nxt;
		}
#else
		int Index = hash_function(ident);
		nxt->kw_next = my_table[Index];
		my_table[Index] = nxt;
#endif
	    } else {
		free_data(nxt);
		nxt = NULL;
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

    VERBOSE(2, ("insert_keyword(%s, %s, %d, %s)",
		ident,
		NONNULL(attribute),
		classflag,
		NONNULL(flag)));

    if (zero_or_more
	&& (mark = vl_index(ident, zero_or_more)) != NULL
	&& (mark != ident)) {
	if ((temp = strmalloc(ident)) != NULL) {

	    mark = temp + (mark - ident);
	    while (*mark == zero_or_more) {
		*mark = 0;
		insert_keyword2(temp, attribute, classflag, flag);
		if ((mark[0] = mark[1]) != 0) {
		    *(++mark) = (char) zero_or_more;
		}
	    }
	    free(temp);
	} else {
	    CannotAllocate("insert_keyword");
	}
    } else if (zero_or_all
	       && (mark = vl_index(ident, zero_or_all)) != NULL
	       && (mark != ident)) {
	if ((temp = strmalloc(ident)) != NULL) {

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
    } else if ((nxt = alloc_keyword(ident, attribute, classflag, flag)) == NULL) {
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
    insert_keyword2(ident, attribute, classflag, NULL);
}

KEYWORD *
is_class(const char *name)
{
    KEYWORD *result = FindIdentifier(name);
    if (result != NULL) {
	if (result->kw_type == 0) {
	    result = NULL;
	}
    }
    return result;
}

KEYWORD *
is_keyword(const char *name)
{
    KEYWORD *result;
    if ((result = FindIdentifier(name)) != NULL
	&& result->kw_type == 0) {
	return result;
    }
    return NULL;
}

const char *
keyword_attr(const char *name)
{
    KEYWORD *data = keyword_data(name);
    const char *result = NULL;

    if (data != NULL) {
	result = data->kw_attr;
    }
    VERBOSE(1, ("keyword_attr(%s) = %p %s", name, (const void *) result,
		NONNULL(result)));
    return result;
}

KEYWORD *
keyword_data(const char *name)
{
    KEYWORD *data = is_keyword(name);
    KEYWORD *result = NULL;

    if (data != NULL) {
	result = data;
	name = data->kw_attr;
	while ((data = is_class(name)) != NULL) {
	    result = data;
	    name = data->kw_attr;
	}
    }
    return result;
}

const char *
keyword_flag(const char *name)
{
    KEYWORD *data = is_keyword(name);
    const char *result = NULL;

    if (data != NULL) {
	result = data->kw_flag;
    }
    VERBOSE(1, ("keyword_flag(%s) = %p %s", name, (const void *) result,
		NONNULL(result)));
    return result;
}

const char *
lowercase_of(const char *text)
{
    static char *name;
    static size_t used;
    unsigned n;
    const char *result;

#if NO_LEAKS
    if (text == NULL) {
	flt_free(&name, &used);
	result = NULL;
    } else
#endif
    if ((name = do_alloc(name, strlen(text), &used)) != NULL) {
	for (n = 0; text[n] != 0; n++) {
	    if (isalpha(CharOf(text[n])) && isupper(CharOf(text[n])))
		name[n] = (char) tolower(CharOf(text[n]));
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
    char *args = NULL;
    char *flag = NULL;
    char *s, *t;
    int quoted = 0;

    VERBOSE(1, ("parse_keyword(%s, %d)", name, classflag));
    if ((s = vl_index(name, eqls_ch)) != NULL) {
	*s++ = 0;
	s = skip_blanks(s);
	if (*s != 0) {
	    args = t = s;
	    while (*s != 0) {
		if (quoted) {
		    if (*s == QUOTE) {
			if (*++s != QUOTE)
			    quoted = 0;
			if (*s == 0)
			    continue;
		    }
		} else {
		    if (*s == QUOTE) {
			quoted = 1;
			s++;
			continue;
		    } else if (!isalnum(CharOf(*s))) {
			if (!*name) {
			    args = NULL;	/* comment: ignore */
			} else if (*s == eqls_ch) {
			    flag = s;
			    *flag++ = '\0';
			    VERBOSE(2, ("found flags \"%s\" for %s", flag, name));
			    /* an empty color field inherits the default */
			    TrimBlanks(args);
			    if (*args == 0)
				args = default_attr;
			} else {
			    VERBOSE(1, ("unexpected:%s%c%s", name,
					eqls_ch, s));
			    args = NULL;	/* error: ignore */
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

    VERBOSE(2, ("parsed\tname \"%s\"\tattr \"%s\" flag \"%s\"%s",
		name,
		NONNULL(args),
		NONNULL(flag),
		(classflag || is_class(name)) ? " - class" : ""));

    if (*name && args) {
	insert_keyword2(name, args, classflag, flag);
    } else if (*name) {
	if (args == NULL) {
	    args = default_attr;
	    VERBOSE(2, ("using attr \"%s\"", args));
	}
	if (args != NULL
	    && strcmp(name, args)
	    && FindIdentifier(args) != NULL) {
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
readline(FILE *fp, char **ptr, size_t *len)
{
    char *buf = *ptr;
    size_t used = 0;

    if (buf == NULL) {
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
	    if (buf == NULL)
		return NULL;
	}
	buf[used++] = (char) ch;
	if (ch == '\n')
	    break;
    }
    buf[used] = '\0';
    *ptr = buf;
    return used ? *ptr : NULL;
}

int
set_symbol_table(const char *table_name)
{
    CLASS *p;
    for (p = classes; p != NULL; p = p->next) {
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
    (void) lowercase_of(NULL);
    FreeAndNull(default_table);
    FreeAndNull(default_attr);
}
#endif
