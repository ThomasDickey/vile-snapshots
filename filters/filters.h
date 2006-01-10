/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/filters.h,v 1.96 2006/01/09 20:23:38 tom Exp $
 */

#ifndef FILTERS_H
#define FILTERS_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <flt_defs.h>

#define CharOf(c)   ((unsigned char)(c))

/* win32 "enhanced" shell cannot echo '&', which we need for builtflt.h */
#undef ADDR
#define ADDR(value) &value

#define MY_NAME "vile"

/*
 * Default attributes across all filters
 */
#define ATTR_ACTION  "BC1"
#define ATTR_COMMENT "C1"
#define ATTR_ERROR   "RC2"
#define ATTR_IDENT   ""
#define ATTR_IDENT2  "C6"
#define ATTR_KEYWORD "B"	/* was C3 */
#define ATTR_KEYWRD2 "B"
#define ATTR_LITERAL "UC5"	/* was C4 */
#define ATTR_NUMBER  "C6"	/* was C5 */
#define ATTR_PREPROC "C2"
#define ATTR_TYPES   "C4"

/*
 * Common/Special token types
 */
#define NAME_ACTION  "Action"
#define NAME_COMMENT "Comment"
#define NAME_ERROR   "Error"
#define NAME_IDENT   "Ident"
#define NAME_IDENT2  "Ident2"
#define NAME_KEYWORD "Keyword"
#define NAME_KEYWRD2 "Keyword2"
#define NAME_LITERAL "Literal"
#define NAME_NUMBER  "Number"
#define NAME_PREPROC "Preproc"
#define NAME_TYPES   "Type"

/*
 * Our magic character.
 */
#define CTL_A	'\001'

/*
 * Useful character definitions
 */
#define BQUOTE  '`'
#define SQUOTE  '\''
#define DQUOTE  '"'

#define L_ANGLE '<'
#define R_ANGLE '>'
#define L_CURLY '{'
#define R_CURLY '}'
#define L_PAREN '('
#define R_PAREN ')'
#define L_BLOCK '['
#define R_BLOCK ']'

/*
 * Pathname definitions
 */
#if defined(VMS) || defined(__VMS) /* predefined by DEC's VMS compilers */
# define PATHCHR ','
# define PATHSEP '/'		/* this will work only with Unix-style names */
# define PATHDOT "[]"
# define DOT_HIDES_FILE 0
#else
# if defined(_WIN32) || defined(__GO32__) || defined(__EMX__)
#  define PATHCHR ';'
#  define PATHSEP '\\'
#  define DOT_HIDES_FILE 0
# else
#  define PATHCHR ':'
#  define PATHSEP '/'
#  define DOT_HIDES_FILE 1
# endif
# define PATHDOT "."
#endif

#if defined(_WIN32)
#define HAVE_LONG_FILE_NAMES 1
#endif

typedef void (*EachKeyword)(const char *name, int size, const char *attr);

/*
 * Declared in the language-specific lex file
 */
typedef struct {
	char *filter_name;
	int loaded;
	void (*InitFilter)(int before);
	void (*DoFilter)(FILE *in);
	char *options;
} FILTER_DEF;

/*
 * This is redefinable so we can make a list of all built-in filter definitions
 */
extern FILTER_DEF filter_def;

/*
 * We'll put a DefineFilter() in each filter program.  To handle special cases
 * such as c-filt.c, use DefineOptFilter().
 */
#define DefineOptFilter(name,options) \
static void init_filter(int before); \
static void do_filter(FILE *Input); \
FILTER_DEF filter_def = { name, 1, init_filter, do_filter, options }

#define DefineFilter(name) DefineOptFilter(name,0)

#if defined(FLEX_SCANNER)
#if defined(filter_def)
#undef yywrap
#define ECHO flt_echo(yytext, yyleng);
#define YY_INPUT(buf,result,max_size) result = flt_input(buf,max_size)
#define YY_FATAL_ERROR(msg) flt_failed(msg);
#endif
/* quiet "gcc -Wunused" warnings */
#define YY_NEVER_INTERACTIVE 1
#define YY_ALWAYS_INTERACTIVE 0
#define YY_MAIN 0
#define YY_NO_UNPUT 1
#define YY_STACK_USED 0
#endif

/*
 * lex should declare these:
 */
extern FILE *yyin;

#if defined(__GNUC__) || defined(__INTEL_COMPILER)
extern int yylex(void);
#ifndef yywrap
extern int yywrap(void);
#endif
#endif /* __GNUC__ */

/*
 * 2003/5/20 - "new" flex 2.5.31:
 * workaround for "developers" who don't use compiler-warnings...
 * perhaps by the time "new" flex merits the term "beta", they'll fix this:
 */
#if defined(FLEX_BETA)
#define YY_NO_INPUT 1		/* get rid of 'input()' function */
extern FILE *yyget_in (void);
extern FILE *yyget_out (void);
extern char *yyget_text (void);
extern int yyget_debug (void);
extern int yyget_leng (void);
extern int yyget_lineno (void);
extern int yylex_destroy (void);
extern void yyset_debug (int bdebug);
extern void yyset_in (FILE * in_str);
extern void yyset_lineno (int line_number);
extern void yyset_out (FILE * out_str);
/* there's also warnings for unused 'yyunput()', but I don't see a fix */
/* flex's skeleton includes <unistd.h> - no particular reason apparent */
#endif

/*
 * Declared in the filters.c file.
 */

typedef struct _keyword KEYWORD;

extern char *default_attr;
extern int zero_or_more;
extern int zero_or_all;
extern int eqls_ch;
extern int meta_ch;
extern int vile_keywords;
extern int flt_options[256];

#define FltOptions(c) flt_options[CharOf(c)]

extern KEYWORD *is_class(char *name);
extern KEYWORD *is_keyword(char *name);
extern char *ci_keyword_attr(char *name);
extern char *class_attr(char *name);
extern char *get_symbol_table(void);
extern char *keyword_attr(char *name);
extern char *lowercase_of(char *name);
extern char *readline(FILE *fp, char **ptr, unsigned *len);
extern char *skip_ident(char *src);
extern int flt_bfr_length(void);
extern int set_symbol_table(const char *classname);
extern long hash_function(const char *id);
extern void *flt_alloc(void *ptr, unsigned need, unsigned *have, unsigned size);
extern void flt_bfr_append(char *text, int length);
extern void flt_bfr_begin(char *attr);
extern void flt_bfr_embed(char *text, int length, char *attr);
extern void flt_bfr_error(void);
extern void flt_bfr_finish(void);
extern void flt_free_keywords(char *classname);
extern void flt_free_symtab(void);
extern void flt_initialize(void);
extern void flt_make_symtab(char *classname);
extern void flt_read_keywords(char *classname);
extern void for_each_keyword(EachKeyword func);
extern void insert_keyword(const char *ident, const char *attribute, int classflag);
extern void parse_keyword(char *name, int classflag);

#define type_alloc(type, ptr, need, have) (type*)flt_alloc(ptr, need, have, sizeof(type))
#define do_alloc(ptr, need, have) type_alloc(char, ptr, need, have)

/*
 * Declared in filterio.c and/or builtflt.c
 */
extern char *flt_gets(char **ptr, unsigned *len);
extern char *flt_put_blanks(char *string);
extern char *skip_blanks(char *src);
extern const char *flt_name(void);
extern int chop_newline(char *s);
extern int flt_get_col(void);
extern int flt_get_line(void);
extern int flt_input(char *buffer, int max_size);
extern int flt_lookup(char *name);
extern int flt_start(char *name);
extern void flt_echo(const char *string, int length);
extern void flt_error(const char *fmt, ...) VILE_PRINTF(1,2);
extern void flt_failed(const char *msg);
extern void flt_finish(void);
extern void flt_putc(int ch);
extern void flt_puts(const char *string, int length, const char *attribute);
extern void mlforce(const char *fmt, ...) VILE_PRINTF(1,2);

#ifndef strmalloc
extern char *strmalloc(const char *src);
#endif

/*
 * declared in main.c or filters.c
 */
extern char *vile_getenv(const char *name);

#define WriteToken(attr) flt_puts(yytext, yyleng, attr)
#define WriteToken2(attr,len) flt_puts(yytext+len, yyleng-len, attr)

#define BeginQuote(state, attr) \
			BEGIN(state); \
			flt_bfr_begin(attr); \
			flt_bfr_append(yytext, yyleng)

#define FinishQuote(state) \
			flt_bfr_append(yytext, yyleng);\
			flt_bfr_finish();\
			BEGIN(state)

/* see fltstack.h */
#define PushQuote(state, attr) \
			push_state(state); \
			flt_bfr_begin(attr); \
			flt_bfr_append(yytext, yyleng)

#define PopQuote() \
			flt_bfr_append(yytext, yyleng);\
			flt_bfr_finish();\
			pop_state()

/*
 * If we're not making built-in filters, we won't be linking with trace.o,
 * and do not implement leak-checking for that case.
 */
#if !OPT_FILTER
#undef  NO_LEAKS
#define NO_LEAKS 0
#endif

#if CAN_TRACE && NO_LEAKS
#include <trace.h>
#endif

#ifdef __cplusplus
}
#endif

#endif /* FILTERS_H */
