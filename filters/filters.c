/*
 * Common utility functions for vile syntax/highlighter programs
 *
 * $Header: /users/source/archives/vile.vcs/filters/RCS/filters.c,v 1.61 1999/12/27 01:43:53 tom Exp $
 *
 */

#include <filters.h>

#define MY_NAME "vile"

#define QUOTE '\''

#ifdef DEBUG
#define TRACE(p) printf p;
#else
#define TRACE(p)		/*nothing */
#endif

#if HAVE_LONG_FILE_NAMES
#define KEYFILE_SUFFIX ".keywords"
#else
#define KEYFILE_SUFFIX ".key"
#endif

#if DOT_HIDES_FILE
#define DOT_TO_HIDE_IT "."
#else
#define DOT_TO_HIDE_IT ""
#endif

#define VERBOSE(level,params)		if (verbose >= level) printf params
#define NONNULL(s)			((s) != 0) ? (s) : "<null>"

#define	typecallocn(cast,ntypes)	(cast *)calloc(sizeof(cast),ntypes)

#define HASH_LENGTH 256

struct _keyword {
    char *kw_name;
    char *kw_attr;
    unsigned kw_size;		/* strlen(kw_name) */
    unsigned short kw_flag;	/* nonzero for classes */
    unsigned short kw_used;	/* nonzero for classes */
    KEYWORD *next;
};

typedef struct _classes CLASS;

struct _classes {
    char *name;
    KEYWORD **data;
    CLASS *next;
};

int meta_ch = '.';
int eqls_ch = ':';

static unsigned TrimBlanks(char *src);
static void ReadKeywords(char *classname);
static void RemoveList(KEYWORD * k);

static KEYWORD **hashtable;
static CLASS *classes;
static char *Default_attr;
static int verbose;
static int k_used;

/******************************************************************************
 * Private functions                                                          *
 ******************************************************************************/

static char *
AttrsOnce(KEYWORD * entry)
{
    entry->kw_used = 999;
    return NONNULL(entry->kw_attr);
}

static void
CreateSymbolTable(char *classname)
{
    if (!set_symbol_table(classname)) {
	CLASS *p = typecallocn(CLASS, 1);
	p->name = strmalloc(classname);
	p->data = typecallocn(KEYWORD *, HASH_LENGTH);
	p->next = classes;
	classes = p;
	hashtable = p->data;

	VERBOSE(1, ("CreateSymbolTable(%s)\n", classname));

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
	free(Default_attr);
	Default_attr = strmalloc(param);
    } else {
	*s = save;
	VERBOSE(1, ("not a class:%s\n", param));
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
 * Include a symbol table from another key-file.
 */
static void
ExecSource(char *param)
{
    int save_meta = meta_ch;
    int save_eqls = eqls_ch;

    CreateSymbolTable(param);
    ReadKeywords(MY_NAME);	/* provide default values for this table */
    ReadKeywords(param);
    set_symbol_table(filter_name);

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
    CreateSymbolTable(param);
}

static KEYWORD *
FindIdentifier(const char *name)
{
    unsigned size;
    int Index;
    KEYWORD *hash_id = 0;

    if (name != 0 && (size = strlen(name)) != 0) {
	Index = hash_function(name);
	hash_id = hashtable[Index];
	while (hash_id != NULL) {
	    if (hash_id->kw_size == size
		&& strcmp(hash_id->kw_name, name) == 0) {
		break;
	    }
	    hash_id = hash_id->next;
	}
    }
    return hash_id;
}

static void
Free(char *ptr)
{
    if (ptr != 0)
	free(ptr);
}

static char *
home_dir(void)
{
    char *result;
#if defined(VMS)
    if ((result = getenv("SYS$LOGIN")) == 0)
	result = getenv("HOME");
#else
    result = getenv("HOME");
#if defined(_WIN32)
    if (result != 0) {
	static char desktop[256];
	char *drive = getenv("HOMEDRIVE");
	if (drive != 0) {
	    sprintf(desktop, "%s%s", drive, result);
	    result = desktop;
	}
    }
#endif
#endif
    return result;
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
OpenKeywords(char *classname)
{
#define OPEN_IT(p) if ((fp = fopen(p, "r")) != 0) { \
			VERBOSE(1,("Opened %s\n", p)); return fp; } else { \
			VERBOSE(2,("..skip %s\n", p)); }
#define FIND_IT(p) sprintf p; OPEN_IT(name)

    static char *name;
    static unsigned have;

    static char *filename;
    static unsigned have2;
    static char suffix[] = KEYFILE_SUFFIX;

    FILE *fp;
    char *path;
    unsigned need;
    char leaf[20];

    filename = do_alloc(filename, sizeof(suffix) + strlen(classname) + 2, &have2);
    sprintf(filename, "%s%s", classname, suffix);

    if (strchr(filename, PATHSEP) != 0) {
	OPEN_IT(filename);
    }

    if ((path = home_dir()) == 0)
	path = "";

    need = strlen(path)
	+ strlen(filename)
	+ 20;

    name = do_alloc(name, need, &have);

#if DOT_HIDES_FILE
    FIND_IT((name, "%s%c.%s", PATHDOT, PATHSEP, filename));
    FIND_IT((name, "%s%c.%s", path, PATHSEP, filename));
    sprintf(leaf, ".%s%c", MY_NAME, PATHSEP);
#else
    FIND_IT((name, "%s%c%s", PATHDOT, PATHSEP, filename));
    FIND_IT((name, "%s%c%s", path, PATHSEP, filename));
    sprintf(leaf, "%s%c", MY_NAME, PATHSEP);
#endif

    FIND_IT((name, "%s%c%s%s", path, PATHSEP, leaf, filename));

    path = getenv("VILE_STARTUP_PATH");
#ifdef VILE_STARTUP_PATH
    if (path == 0)
	path = VILE_STARTUP_PATH;
#endif
    if (path != 0) {
	int n = 0, m;
	name = do_alloc(name, strlen(path), &have);
	while (path[n] != 0) {
	    for (m = n; path[m] != 0 && path[m] != PATHCHR; m++)
		/*LOOP */ ;
	    FIND_IT((name, "%.*s%c%s", m - n, path + n, PATHSEP, filename));
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
	{ "class",   ExecClass    },
	{ "default", ExecDefault  },
	{ "equals",  ExecEquals   },
	{ "include", ReadKeywords },
	{ "merge",   ExecSource   },
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

static int
ProcessArgs(int argc, char *argv[], int flag)
{
    int n;
    char *s, *value;

    for (n = 1; n < argc; n++) {
	s = argv[n];
	if (*s == '-') {
	    while (*++s) {
		switch (*s) {
		case 'k':
		    value = s[1] ? s + 1 : ((n < argc) ? argv[++n] : "");
		    if (flag) {
			ReadKeywords(value);
			k_used++;
		    }
		    break;
		case 'v':
		    if (!flag)
			verbose++;
		    break;
		default:
		    fprintf(stderr, "unknown option %c\n", *s);
		    exit(BADEXIT);
		}
	    }
	} else {
	    break;
	}
    }
    return n;
}

static void
ReadKeywords(char *classname)
{
    FILE *kwfile;
    char *line = 0;
    char *name;
    unsigned line_len = 0;

    VERBOSE(1, ("ReadKeywords(%s)\n", classname));
    if ((kwfile = OpenKeywords(classname)) != NULL) {
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
}

static void
RemoveList(KEYWORD * k)
{
    if (k != NULL) {
	if (k->next != NULL)
	    RemoveList(k->next);
	free((char *) k);
    }
}

static unsigned
TrimBlanks(char *src)
{
    unsigned len = strlen(src);

    while (len != 0
	&& isspace(src[len - 1]))
	src[--len] = 0;
    return (len);
}

/******************************************************************************
 * Public functions                                                           *
 ******************************************************************************/

char *
ci_keyword_attr(char *text)
{
    return keyword_attr(lowercase_of(text));
}

char *
class_attr(char *name)
{
    KEYWORD *hash_id;
    char *result = 0;

    while ((hash_id = is_class(name)) != 0) {
	VERBOSE(hash_id->kw_used, ("class_attr(%s) = %s\n",
		name, AttrsOnce(hash_id)));
	name = result = hash_id->kw_attr;
    }
    return result;
}

char *
do_alloc(char *ptr, unsigned need, unsigned *have)
{
    need += 2;			/* allow for trailing null */
    if (need > *have) {
	if (ptr != 0)
	    ptr = realloc(ptr, need);
	else
	    ptr = malloc(need);
	*have = need;
    }
    return ptr;
}

/*
 * Iterate over the names in the hash table, not ordered.  This assumes that
 * the called function does not remove any entries from the table.  It is okay
 * to add names, since that will not alter the hash links.
 */
void
for_each_keyword(EachKeyword func)
{
    int i;
    KEYWORD *ptr;

    for (i = 0; i < HASH_LENGTH; i++) {
	for (ptr = hashtable[i]; ptr != 0; ptr = ptr->next) {
	    (*func) (ptr->kw_name, ptr->kw_size, ptr->kw_attr);
	}
    }
}

long
hash_function(const char *id)
{
    /*
     * Build more elaborate hashing scheme. If you want one.
     */
    if ((id[0] == 0) || (id[1] == 0))
	return ((int) id[0]);

    return (((int) id[0] ^ ((int) id[1] << 3) ^ ((int) id[2] >> 1)) & 0xff);
}

void
insert_keyword(const char *ident, const char *attribute, int classflag)
{
    KEYWORD *first;
    KEYWORD *nxt;
    int Index;

    if ((nxt = FindIdentifier(ident)) != 0) {
	Free(nxt->kw_attr);
	nxt->kw_attr = strmalloc(attribute);
    } else {
	nxt = first = NULL;
	Index = hash_function(ident);
	first = hashtable[Index];
	if ((nxt = typecallocn(KEYWORD, 1)) != NULL) {
	    nxt->kw_name = strmalloc(ident);
	    nxt->kw_size = strlen(nxt->kw_name);
	    nxt->kw_attr = strmalloc(attribute);
	    nxt->kw_flag = classflag;
	    nxt->kw_used = 2;
	    nxt->next = first;
	    hashtable[Index] = nxt;
	} else {
	    VERBOSE(1, ("insert_keyword: cannot allocate\n"));
	    return;
	}
    }
    VERBOSE(3, ("...\tname \"%s\"\tattr \"%s\"\n",
	    nxt->kw_name,
	    NONNULL(nxt->kw_attr)));
}

KEYWORD *
is_class(char *name)
{
    KEYWORD *hash_id;
    if ((hash_id = FindIdentifier(name)) != 0
	&& hash_id->kw_flag != 0) {
	return hash_id;
    }
    return 0;
}

KEYWORD *
is_keyword(char *name)
{
    KEYWORD *hash_id;
    if ((hash_id = FindIdentifier(name)) != 0
	&& hash_id->kw_flag == 0) {
	return hash_id;
    }
    return 0;
}

char *
keyword_attr(char *name)
{
    KEYWORD *hash_id = is_keyword(name);
    char *result = 0;

    if (hash_id != 0) {
	result = hash_id->kw_attr;
	while ((hash_id = is_class(result)) != 0)
	    result = hash_id->kw_attr;
    }
    return result;
}

char *
lowercase_of(char *text)
{
    static char *name;
    static unsigned used;
    unsigned n;

    name = do_alloc(name, strlen(text), &used);
    for (n = 0; text[n] != 0; n++) {
	if (isalpha(text[n]) && isupper(text[n]))
	    name[n] = tolower(text[n]);
	else
	    name[n] = text[n];
    }
    name[n] = 0;
    return (name);
}

void
parse_keyword(char *name, int classflag)
{
    char *args = 0;
    char *s, *t;
    int quoted = 0;

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
		    } else if (!isalnum(*s)) {
			args = 0;	/* error: ignore */
			break;
		    }
		}
		*t++ = *s++;
	    }
	    *t = 0;
	}
	TrimBlanks(name);
    }

    VERBOSE(2, ("parsed\tname \"%s\"\tattr \"%s\"%s\n",
	    name,
	    NONNULL(args),
	    (classflag || is_class(name)) ? " - class" : ""));

    if (*name && args) {
	insert_keyword(name, args, classflag);
    } else if (*name) {
	KEYWORD *hash_id;
	if (args == 0)
	    args = Default_attr;
	if ((hash_id = FindIdentifier(args)) != 0) {
	    insert_keyword(name, hash_id->kw_attr, classflag);
	}
    }
}

char *
readline(FILE * fp, char **ptr, unsigned *len)
{
    char *buf = *ptr;
    unsigned used = 0;

    if (buf == 0)
	buf = (char *) malloc(*len = BUFSIZ);
    while (!feof(fp)) {
	int ch = fgetc(fp);
	if (ch == EOF || feof(fp) || ferror(fp)) {
	    break;
	}
	if (used + 2 >= *len) {
	    *len = 3 * (*len) / 2;
	    buf = realloc(buf, *len);
	}
	buf[used++] = ch;
	if (ch == '\n')
	    break;
    }
    buf[used] = '\0';
    return used ? (*ptr = buf) : 0;
}

int
set_symbol_table(const char *classname)
{
    CLASS *p;
    for (p = classes; p != 0; p = p->next) {
	if (!strcmp(classname, p->name)) {
	    hashtable = p->data;
	    VERBOSE(3, ("set_symbol_table:%s\n", classname));
	    return 1;
	}
    }
    return 0;
}

char *
skip_blanks(char *src)
{
    while (isspace(*src))
	src++;
    return (src);
}

char *
skip_ident(char *src)
{
    while (*src != '\0' && isprint(*src) && !isspace(*src)) {
	if (*src == eqls_ch
	    || *src == meta_ch) {
	    break;
	}
	src++;
    }
    return (src);
}

char *
strmalloc(const char *src)
{
    register char *ns = (char *) malloc(strlen(src) + 1);
    if (ns != 0)
	(void) strcpy(ns, src);
    return ns;
}

/*
 * The string may contain newlines, but vile processes the length only until
 * the end of line.
 */
void
write_string(FILE * fp, char *src, int length, char *marker)
{
    if (marker == 0 || *marker == 0) {
	write_token(fp, src, length, marker);
    } else {
	while (length > 0) {
	    int n;
	    for (n = 0; n < length && src[n] != 0 && src[n] != '\n'; n++)
		/*LOOP */ ;
	    if (n == 0) {
		if (src[n])
		    fputc(src[n], fp);
	    } else {
		write_token(fp, src,
		    ((n < length) && (src[n] == '\n'))
		    ? (n + 1) : n, marker);
	    }
	    if (src[n] || (n == 0 && length > 0))
		n++;
	    length -= n;
	    src += n;
	}
    }
}

/*
 * Use this when you do not expect to handle embedded newlines.
 */
void
write_token(FILE * fp, char *string, int length, char *marker)
{
    if (length > 0) {
	if (marker != 0 && *marker != 0 && *marker != 'N')
	    fprintf(fp, "%c%i%s:", CTL_A, length, marker);
	fprintf(fp, "%.*s", length, string);
    }
}

int
main(int argc, char **argv)
{
    int n;

#if OPT_LOCALE
    setlocale(LC_CTYPE, "");
#endif

    /* get verbose option */
    (void) ProcessArgs(argc, argv, 0);

    CreateSymbolTable(filter_name);

    Default_attr = strmalloc(NAME_KEYWORD);

    init_filter(1);

    ReadKeywords(MY_NAME);
    n = ProcessArgs(argc, argv, 1);

    if (!k_used) {
	if (strcmp(MY_NAME, filter_name)) {
	    ReadKeywords(filter_name);
	}
    }
    set_symbol_table(filter_name);
    init_filter(0);

    if (n < argc) {
	char *name = argv[n++];
	FILE *fp = fopen(name, "r");
	if (fp != 0) {
	    do_filter(fp, stdout);
	    fclose(fp);
	}
    } else {
	do_filter(stdin, stdout);
    }
    fflush(stdout);
    fclose(stdout);
    exit(GOODEXIT);
}

int
yywrap(void)
{
    return 1;
}
