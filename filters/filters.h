/*
 * $Id: filters.h,v 1.149 2025/01/26 17:04:12 tom Exp $
 */

#ifndef FILTERS_H
#define FILTERS_H 1
/* *INDENT-OFF* */

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
 * Our magic characters.
 */
#define CTL_A	'\001'
#define CTL_V	'\026'

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
#ifndef PATHCHR			/* may be defined in estruct.h */
#if defined(VMS) || defined(__VMS) /* predefined by DEC's VMS compilers */
# define PATHCHR ','
# define PATHSEP '/'		/* this will work only with Unix-style names */
#elif defined(_WIN32) || defined(__GO32__) || defined(__EMX__)
# define PATHCHR ';'
# define PATHSEP '\\'
#else
# define PATHCHR ':'
# define PATHSEP '/'
#endif
#endif

#if defined(VMS) || defined(__VMS) /* predefined by DEC's VMS compilers */
# define PATHDOT "[]"
# define DOT_HIDES_FILE 0
#else
# define PATHDOT "."
# if defined(_WIN32) || defined(__GO32__) || defined(__EMX__)
#  define DOT_HIDES_FILE 0
# else
#  define DOT_HIDES_FILE 1
# endif
#endif

#if defined(_WIN32)
#define HAVE_LONG_FILE_NAMES 1
#endif

typedef void (*EachKeyword)(const char *name, int size, const char *attr);

/*
 * Declared in the language-specific lex file
 */
typedef struct {
	const char *filter_name;
	int loaded;
	void (*InitFilter)(int before);
	void (*DoFilter)(FILE *in);
	const char *options;
#if NO_LEAKS
	void (*FreeFilter)(void);
#define DCL_LEXFREE static void free_filter(void);
#define REF_LEXFREE ,free_filter
#else
#define DCL_LEXFREE /* nothing */
#define REF_LEXFREE /* nothing */
#endif
} FILTER_DEF;

#ifdef LEX_IS_FLEX
#ifndef YY_CURRENT_BUFFER
#define YY_CURRENT_BUFFER yy_current_buffer
#endif
#ifndef YY_CURRENT_BUFFER_LVALUE
#define YY_CURRENT_BUFFER_LVALUE YY_CURRENT_BUFFER
#endif
#define USE_LEXFREE { yy_delete_buffer(YY_CURRENT_BUFFER); YY_CURRENT_BUFFER_LVALUE = NULL; yy_init = 1; }
#else
#define USE_LEXFREE if (yytext) { free(yytext); yytext = 0; yytextsz = 0; }
#endif

#ifdef FLEX_DEBUG
#define FLEX_PRINTF(param) if (yy_flex_debug) fprintf param
#else
#define FLEX_PRINTF(param) /* nothing */
#endif

/*
 * This is redefinable so we can make a list of all built-in filter definitions
 */
extern FILTER_DEF filter_def;

/*
 * Use flex's parser-specific wrap-function.
 */
#if defined(FLEX_SCANNER)
#undef yywrap
#endif

/*
 * Workaround for incompatibilities between "new" flex versus flex/reflex.
 * One of the problems with "new" flex is that it reverses the order of
 * definitions for yywrap.
 */
#if defined(FLEX_SCANNER) && defined(YY_FLEX_SUBMINOR_VERSION)
#define YY_SKIP_YYWRAP
#define yywrap() private_yywrap()
#define USE_LEXWRAP static int private_yywrap(void) { return 1; }
#else
#define USE_LEXWRAP /* nothing */
#endif

/*
 * We'll put a DefineFilter() in each filter program.  To handle special cases
 * such as c-filt.c, use DefineOptFilter().
 */
#define DefineOptFilter(name,options) \
USE_LEXWRAP \
static void init_filter(int before); \
static void do_filter(FILE *Input); \
DCL_LEXFREE \
FILTER_DEF filter_def = { #name, 1, init_filter, do_filter, options REF_LEXFREE }

#define DefineFilter(name) DefineOptFilter(name,NULL)

#if NO_LEAKS
#define LoadableFilter(name) \
	extern FILTER_DEF define_##name; \
	FILTER_DEF define_##name = { #name, 0, NULL, NULL, NULL, NULL }
#else
#define LoadableFilter(name) \
	extern FILTER_DEF define_##name; \
	FILTER_DEF define_##name = { #name, 0, NULL, NULL, NULL }
#endif

#if defined(FLEX_SCANNER)
#if defined(filter_def)
#define ECHO flt_echo(yytext, yyleng);		/* in builtflt.c */
#define YY_INPUT(buf,result,max_size) result = flt_input(buf,max_size)
#define YY_FATAL_ERROR(msg) flt_failed(msg);
#else
#define ECHO flt_puts(yytext, yyleng, "");	/* in filterio.c */
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
#ifndef FLEX_SCANNER
extern FILE *yyin;
#endif

/*
 * flex "-+" option provides a C++ class for the lexer, which has a different
 * interface.  Allow this to compile (FIXME - the streams do not connect, so
 * it does not run yet - 2008/11/19).
 */
#ifdef __FLEX_LEXER_H
#define InitLEX(theInput) \
	yyFlexLexer lexer
#define RunLEX() if (flt_succeeds()) while (lexer.yylex() > 0)
#else
#define InitLEX(theInput) yyin = theInput
#define RunLEX() if (flt_succeeds()) while (yylex() > 0)
#endif

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
extern FILE *yyget_in (void);
extern FILE *yyget_out (void);
extern char *yyget_text (void);
extern int yyget_debug (void);
extern int yyget_lineno (void);
extern int yylex_destroy (void);
extern void yyset_debug (int bdebug);
extern void yyset_in (FILE * in_str);
extern void yyset_lineno (int line_number);
extern void yyset_out (FILE * out_str);
#if !defined(YY_FLEX_SUBMINOR_VERSION)
extern int yyget_leng (void);
#elif (YY_FLEX_MINOR_VERSION < 6) && (YY_FLEX_SUBMINOR_VERSION < 37)
extern yy_size_t yyget_leng (void);
#elif (YY_FLEX_MINOR_VERSION < 6)
extern yy_size_t yyget_leng (void);
#endif
/* there's also warnings for unused 'yyunput()', but I don't see a fix */
/* flex's skeleton includes <unistd.h> - no particular reason apparent */
#endif

#define YY_NO_INPUT 1		/* get rid of 'input()' function */

/*
 * Declared in the filters.c file.
 */

typedef struct _keyword KEYWORD;

extern char *default_attr;
extern char *default_table;
extern int zero_or_more;
extern int zero_or_all;
extern int eqls_ch;
extern int meta_ch;
extern int vile_keywords;
extern int flt_options[256];

#if defined(__GNUC__) && defined(_FORTIFY_SOURCE)
extern int ignore_unused;
#endif

#define FltOptions(c) flt_options[CharOf(c)]

extern KEYWORD *is_class(const char *name);
extern KEYWORD *is_keyword(const char *name);
extern KEYWORD *keyword_data(const char *name);
extern char *class_attr(const char *name);
extern const char *ci_keyword_attr(const char *name);
extern const char *ci_keyword_flag(const char *name);
extern const char *get_keyword_attr(const char *text);
extern const char *get_symbol_table(void);
extern const char *keyword_attr(const char *name);
extern const char *keyword_flag(const char *name);
extern const char *lowercase_of(const char *name);
extern char *readline(FILE *fp, char **ptr, size_t *len);
extern char *skip_ident(char *src);
extern int ci_compare(const char *a, const char *b);
extern int flt_bfr_length(void);
extern int set_symbol_table(const char *classname);
extern long hash_function(const char *id);
extern void *flt_alloc(void *ptr, size_t need, size_t *have, size_t size);
extern void flt_bfr_append(const char *text, int length);
extern void flt_bfr_begin(const char *attr);
extern void flt_bfr_embed(const char *text, int length, const char *attr);
extern void flt_bfr_error(void);
extern void flt_bfr_finish(void);
extern void flt_dump_symtab(const char *table_name);
extern void flt_free(char **p, size_t *len);
extern void flt_free_keywords(const char *classname);
extern void flt_free_symtab(void);
extern void flt_init_attr(const char *attr_name);
extern void flt_init_table(const char *table_name);
extern void flt_initialize(const char *classname);
extern void flt_make_symtab(const char *classname);
extern void flt_read_keywords(const char *classname);
extern void flt_setup_symbols(const char *table_name);
extern void insert_keyword(const char *ident, const char *attribute, int classflag);
extern void parse_keyword(char *name, int classflag);

#define type_alloc(type, ptr, need, have) (type*)flt_alloc(ptr, need, have, sizeof(type))
#define do_alloc(ptr, need, have) type_alloc(char, ptr, need, have)

/*
 * Declared in filterio.c and/or builtflt.c
 */
#ifndef VILE_PROTO_H

extern char *skip_blanks(char *src);
extern char *vile_getenv(const char *name);
extern int vl_lookup_color(const char *name);
extern int vl_lookup_func(const char *name);
extern int vl_lookup_xcolor(const char *name);
extern void mlforce(const char *fmt, ...) VILE_PRINTF(1,2);

#ifndef strmalloc
extern char *strmalloc(const char *src);
#endif

#endif

extern char *flt_gets(char **ptr, size_t *len);
extern char *flt_put_blanks(char *string);
extern const char *flt_name(void);
extern const void *vl_lookup_cmd(const char *name);
extern int chop_newline(char *s);
extern int flt_get_col(void);
extern int flt_get_line(void);
extern int flt_input(char *buffer, int max_size);
extern int flt_lookup(char *name);
extern int flt_restart(char *name);
extern int flt_start(char *name);
extern int vl_check_cmd(const void *cmd, unsigned long flags);
extern int vl_is_majormode(const void *cmd);
extern int vl_is_setting(const void *cmd);
extern int vl_is_submode(const void *cmd);
extern int vl_is_xcolor(const void *cmd);
extern int vl_lookup_mode(const char *name);
extern int vl_lookup_var(const char *name);
extern int flt_succeeds(void);
extern void flt_echo(const char *string, int length);
extern void flt_error(const char *fmt, ...) VILE_PRINTF(1,2);
extern void flt_failed(const char *msg);
extern void flt_finish(void);
extern void flt_message(const char *fmt, ...) VILE_PRINTF(1,2);
extern void flt_putc(int ch);
extern void flt_puts(const char *string, int length, const char *attribute);

/* potential symbol conflict with ncurses */
#define define_key vl_define_key

/*
 * declared in main.c or filters.c
 */

#define WriteToken(attr) flt_puts(yytext, yyleng, attr)
#define WriteToken2(attr,len) flt_puts(yytext + (len), yyleng - (len), attr)
#define WriteToken3(attr,len) flt_puts(yytext, (len), attr)

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

#if CAN_TRACE && NO_LEAKS
#include <trace.h>
#endif

#ifdef __cplusplus
}
#endif

/* *INDENT-ON* */

#endif /* FILTERS_H */
