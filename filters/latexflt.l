%pointer
%s sTEXT VERBATIM QUOTE_TT QUOTED MATH MATHC

%o 6000
%a 4000

%{

/*
 * $Id: latexflt.l,v 1.56 2013/12/02 01:33:56 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of TeX or LaTeX
 * input text.
 *
 * First version written by Pierre Dittgen (dittgen@univ-mlv.fr)
 * modified by Vanessa Conchodon (conchodo@univ-mlv.fr)
 * rewritten by Thomas Dickey (2003).
 *
 * LIMITATIONS:
 *
 * This is really a TeX filter, which can be used for LaTeX.  LaTeX defines
 * lots of macros.  One of the problems with that, is that TeX macros can
 * redefine some syntax features.  For example, a macro definition may include
 * an unbalanced curly brace.  Since the definition may be in a different file,
 * there is no good way for this filter to keep track of these exceptions.
 *
 * LaTeX documents (does not appear in the TeX book) that a comment and
 * following whitespace are ignored completely, allowing a token to be split
 * across one or more lines.
 *
 * Math mode is toggled only outside curly braces, to avoid extra confusion
 * with macros.  Studied to see if \[ \] and \( \) markers can be used for math
 * mode as documented in LaTeX, but that turned out to not work well.
 *
 * Because LaTeX simply has so many macros, it is not worth spending time to
 * build (or use) a .keywords file.  In texmode, this is useful.
 *
 * Note: '@' is legal in some TeX library files, so it is added to the NAME
 * pattern.
 */

#include <filters.h>

/*
 * Options:
 *	-a	tells us to highlight any \keyword
 */
DefineOptFilter(latex, "a");

#define NAME_MATH       "Math"
#define NAME_MATH_C     "Math_c"
#define NAME_SECTION    "Section"
#define NAME_SUBSECTION "Subsection"

static char *Action_attr;
static char *Comment_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Math_attr;		/* math formula [$...$] */
static char *Math_c_attr;	/* math formula (2) [$$...$$] */
static char *Number_attr;
static char *Section_attr;	/* subsection */
static char *String_attr;
static char *Subsection_attr;	/* section */

static int braces;

#include <fltstack.h>

static void write_define(int keylen);
static void write_keyword(void);
static void write_param(void);
static void write_text(char *attr);

%}

SPACE		[ \t]
NONSPACE	[^ \t]

NAME		[[:alpha:]@]+
NONNAME		[^[:alpha:]@]
MIXED_ALNUM	({NAME}[[:digit:]._-]+)+{NAME}?

OCTAL		\'[0-7]+
DECIMAL		[[:digit:]]+
INTEGER		[-+]?({OCTAL}|{DECIMAL})
REAL		[-+]?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?
NUMBER		({INTEGER}|{REAL})

%%

{SPACE}+		{ WriteToken(""); }

<sTEXT>"%".*$		{ WriteToken(Comment_attr); }
<sTEXT>"#"{INTEGER}({NAME}?)	{ write_param(); }
<sTEXT>{MIXED_ALNUM}	{ WriteToken(""); }
{NUMBER}		{ WriteToken(Number_attr); }

<sTEXT>\\"char"({OCTAL}|{DECIMAL}|"`".) |
<sTEXT>\\verb:[^:]+:		{ WriteToken(String_attr); }

<sTEXT>\\"let"\\{NAME}		|
<sTEXT>\\"def"\\{NAME}		|
<sTEXT>\\"def"\\{NONNAME}	{ write_define(4); }
<sTEXT>\\"xdef"\\{NONNAME}	|
<sTEXT>\\"gdef"\\{NONNAME}	{ write_define(5); }

<sTEXT>\\"let"[^[:alpha:]]	|
<sTEXT>\\"def"[^[:alpha:]]	{ write_define(4); }

<sTEXT>\\"begintt"{NONNAME}	{
			  yyless(yyleng-1);	/* reinterp {NONAME} */
			  write_keyword();
			  push_state(QUOTE_TT);
			 }
<QUOTE_TT>\\"endtt"{NONNAME}	{
			  yyless(yyleng-1);	/* reinterp {NONAME} */
			  write_keyword();
			  pop_state();
			 }
<QUOTE_TT>[^\r\n]*		{ WriteToken(String_attr); }

<sTEXT>\\"begin{verbatim}"	{ WriteToken(String_attr); push_state(VERBATIM); }
<VERBATIM>\\"end{verbatim}"	{ WriteToken(String_attr); pop_state(); }
<VERBATIM>[^\r\n]*		{ WriteToken(String_attr); }

<sTEXT,MATH,MATHC,QUOTED>"|"	{ WriteToken(String_attr);
			  if (braces != 0)
			      ;
			  else if (FLTSTACK_OK) {
			      if (FLT_STATE == QUOTED)
				  pop_state();
			      else if (FLT_STATE != sTEXT)
				  push_state(QUOTED);
			  }
			}
<QUOTED>[^|\r\n]*	{ WriteToken(String_attr); }

\\{NAME}		{ write_keyword(); }

<sTEXT>\\\\		{ WriteToken(String_attr); }

<sTEXT>\\{NONNAME} 	{ flt_puts(yytext, yyleng, Number_attr); }

<sTEXT>\$[^\$] 		{
			  yyless(1);	/* reinterpret the second char */
			  flt_puts(yytext, 1, Action_attr);
			  if (braces == 0)
			      push_state(MATH);
			}
<MATH>[^\$\r\n]+	{ write_text(Math_attr); }
<MATH>\$ 		{ write_text(Action_attr); pop_state(); }

<sTEXT>\$\$	 	{ WriteToken(Action_attr);
			  if (braces == 0)
			      push_state(MATHC);
			}
<MATHC>\\{NONNAME} 	{ flt_puts(yytext, yyleng, Number_attr); }
<MATHC>[^#%{}\\\$/\r\n|]+	{ write_text(Math_c_attr); }
<MATHC>\$\$		{ WriteToken(Action_attr);
			  if (braces == 0)
			      pop_state();
			}
<MATHC>\${NAME}		{ write_text(Math_c_attr); }

\{			{ WriteToken(Action_attr); ++braces; }
\} 			{ WriteToken(Action_attr); if (braces) --braces; }

%%

#include <fltstack.h>

static void
write_keyword(void)
{
    const char *attr;
    if (FltOptions('a'))
	attr = Keyword_attr;
    else
	attr = get_keyword_attr(yytext);
    if (!attr || !*attr)
	attr = "";		/* not Keyword_attr! */
    WriteToken(attr);
}

static void
write_define(int keylen)
{
    int save = yytext[keylen];
    yytext[keylen] = '\0';
    flt_puts(yytext, keylen,
	     (FltOptions('a')
	      ? Keyword_attr
	      : get_keyword_attr(yytext)));
    yytext[keylen] = (char) save;
    flt_puts(yytext + keylen, yyleng - keylen, Number_attr);
}

static void
write_param(void)
{
    WriteToken(Ident2_attr);
}

static void
write_text(char *attr)
{
    WriteToken(braces ? Ident2_attr : attr);
}

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
}

static void
do_filter(FILE *inputs)
{
    InitLEX(inputs);

    Action_attr = class_attr(NAME_ACTION);

    Comment_attr = class_attr(NAME_COMMENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Ident_attr = class_attr(NAME_IDENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    Math_attr = class_attr(NAME_MATH);
    Math_c_attr = class_attr(NAME_MATH_C);
    Section_attr = class_attr(NAME_SECTION);
    Subsection_attr = class_attr(NAME_SUBSECTION);

    braces = 0;

    begin_state(sTEXT);
    RunLEX();
    end_state();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
