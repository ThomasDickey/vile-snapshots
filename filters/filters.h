/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/filters.h,v 1.61 2002/10/09 23:38:39 tom Exp $
 */

#ifndef FILTERS_H
#define FILTERS_H 1

#ifndef _estruct_h

#ifdef HAVE_CONFIG_H
#include <config.h>
#else
/* assume ANSI C */
# define HAVE_STDLIB_H 1
# define HAVE_STRING_H 1
#endif

#ifndef OPT_LOCALE
#define OPT_LOCALE 0
#endif

#include <sys/types.h>		/* sometimes needed to get size_t */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#else
# if !defined(HAVE_CONFIG_H) || defined(MISSING_EXTERN_MALLOC)
extern	char *	malloc	( size_t len );
# endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef MISSING_EXTERN__FILBUF
extern	int	_filbuf	( FILE *fp );
#endif

#ifdef MISSING_EXTERN__FLSBUF
extern	int	_flsbuf	( int len, FILE *fp );
#endif

#ifdef MISSING_EXTERN_FCLOSE
extern	int	fclose	( FILE *fp );
#endif

#ifdef MISSING_EXTERN_FPRINTF
extern	int	fprintf	( FILE *fp, const char *fmt, ... );
#endif

#ifdef MISSING_EXTERN_FPUTS
extern	int	fputs	( const char *s, FILE *fp );
#endif

#ifdef MISSING_EXTERN_PRINTF
extern	int	printf	( const char *fmt, ... );
#endif

#ifdef MISSING_EXTERN_SSCANF
extern	int	sscanf	( const char *src, const char *fmt, ... );
#endif

#if OPT_LOCALE
#include <locale.h>
#endif

#if defined(VMS)
#include	<stsdef.h>
#define GOODEXIT	(STS$M_INHIB_MSG | STS$K_SUCCESS)
#define BADEXIT		(STS$M_INHIB_MSG | STS$K_ERROR)
#else
#if defined(EXIT_SUCCESS) && defined(EXIT_FAILURE)
#define GOODEXIT	EXIT_SUCCESS
#define BADEXIT		EXIT_FAILURE
#else
#define GOODEXIT	0
#define BADEXIT		1
#endif
#endif

#ifndef GCC_UNUSED
#define GCC_UNUSED /*nothing*/
#endif

extern	char *home_dir(void);

#ifndef _estruct_h
typedef enum { D_UNKNOWN = -1, D_KNOWN = 0 } DIRECTIVE;
extern DIRECTIVE dname_to_dirnum(const char *cmdp, size_t length);
#endif

typedef struct { int dummy; } CMDFUNC;
extern const CMDFUNC * engl2fnc(const char *fname);

extern int vl_lookup_func(const char *name);

#endif /* _estruct_h */

#include <ctype.h>
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
 * lex should declare these:
 */
extern FILE *yyin;
extern int yylex(void);

/*
 * Declared in the language-specific lex file
 */
typedef struct {
	char *filter_name;
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
FILTER_DEF filter_def = { name, init_filter, do_filter, options }

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
 * Declared in the filters.c file.
 */

typedef struct _keyword KEYWORD;

extern char *default_attr;
extern int eqls_ch;
extern int meta_ch;
extern int vile_keywords;
extern int flt_options[256];

extern KEYWORD *is_class(char *name);
extern KEYWORD *is_keyword(char *name);
extern char *ci_keyword_attr(char *name);
extern char *class_attr(char *name);
extern char *do_alloc(char *ptr, unsigned need, unsigned *have);
extern char *get_symbol_table(void);
extern char *keyword_attr(char *name);
extern char *lowercase_of(char *name);
extern char *readline(FILE *fp, char **ptr, unsigned *len);
extern char *skip_ident(char *src);
extern int flt_bfr_length(void);
extern int set_symbol_table(const char *classname);
extern long hash_function(const char *id);
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

/*
 * Declared in filterio.c and/or builtflt.c
 */
extern char *flt_gets(char **ptr, unsigned *len);
extern char *flt_list(void);
extern char *flt_name(void);
extern char *skip_blanks(char *src);
extern int flt_input(char *buffer, int max_size);
extern int flt_lookup(char *name);
extern int flt_start(char *name);
extern void flt_echo(char *string, int length);
extern void flt_failed(const char *msg);
extern void flt_finish(void);
extern void flt_putc(int ch);
extern void flt_puts(char *string, int length, char *attribute);
extern void mlforce(const char *fmt, ...);

#ifndef strmalloc
extern char *strmalloc(const char *src);
#endif

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

#endif /* FILTERS_H */
