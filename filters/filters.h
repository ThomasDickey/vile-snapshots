/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/filters.h,v 1.24 1999/03/07 19:03:08 tom Exp $
 */

#ifndef FILTERS_H
#define FILTERS_H 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#else
/* assume ANSI C */
# define HAVE_STDLIB_H 1
# define HAVE_STRING_H 1
#endif

#include <sys/types.h>		/* sometimes needed to get size_t */

#if HAVE_STDLIB_H
#include <stdlib.h>
#else
# if !defined(HAVE_CONFIG_H) || MISSING_EXTERN_MALLOC
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

#if MISSING_EXTERN__FILBUF
extern	int	_filbuf	( FILE *fp );
#endif

#if MISSING_EXTERN__FLSBUF
extern	int	_flsbuf	( int len, FILE *fp );
#endif

#if MISSING_EXTERN_FCLOSE
extern	int	fclose	( FILE *fp );
#endif

#if MISSING_EXTERN_FPRINTF
extern	int	fprintf	( FILE *fp, const char *fmt, ... );
#endif

#if MISSING_EXTERN_FPUTS
extern	int	fputs	( const char *s, FILE *fp );
#endif

#if MISSING_EXTERN_PRINTF
extern	int	printf	( const char *fmt, ... );
#endif

#if MISSING_EXTERN_SSCANF
extern	int	sscanf	( const char *src, const char *fmt, ... );
#endif

#if OPT_LOCALE
#include <locale.h>
#endif

#include <ctype.h>

#ifndef GCC_UNUSED
#define GCC_UNUSED /*nothing*/
#endif

/*
 * Default attributes across all filters
 */
#define ATTR_ACTION  "BC1"
#define ATTR_COMMENT "C1"
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
# if defined(_WIN32) || defined(__GO32__)
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
extern FILE *yyout;
extern int yylex(void);

/*
 * Declared in the language-specific lex file
 */
extern char *filter_name;

extern int is_ident(int ch);
extern void init_filter(int before);
extern void do_filter(FILE *input, FILE *output);

/*
 * Declared in the filters.c file.
 */
extern char *ci_keyword_attr(char *name);
extern char *class_attr(char *name);
extern char *do_alloc(char *ptr, unsigned need, unsigned *have);
extern char *keyword_attr(char *name);
extern char *lowercase_of(char *name);
extern char *readline(FILE *fp, char **ptr, unsigned *len);
extern char *strmalloc(const char *src);
extern int set_symbol_table(const char *classname);
extern long hash_function(const char *id);
extern void for_each_keyword(EachKeyword func);
extern void insert_keyword(const char *ident, const char *attribute, int classflag);
extern void write_string(FILE *fp, char *string, int length, char *attribute);
extern void write_token(FILE *fp, char *string, int length, char *attribute);

#define WriteString(attr) write_string(yyout, yytext, yyleng, attr)
#define WriteToken(attr) write_token(yyout, yytext, yyleng, attr)

#define WriteString2(attr,len) write_string(yyout, yytext+len, yyleng-len, attr)
#define WriteToken2(attr,len) write_token(yyout, yytext+len, yyleng-len, attr)

#endif /* FILTERS_H */
