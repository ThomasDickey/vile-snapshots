%s TEXT VARS CODE MATH MATHC

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/latexflt.l,v 1.17 2000/02/28 12:03:27 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of LaTeX input
 * text.
 *
 * First version written by Pierre Dittgen (dittgen@univ-mlv.fr)
 * modified by Vanessa Conchodon (conchodo@univ-mlv.fr)
 */

#include <filters.h>

DefineFilter("latex");

#define NAME_MATH       "Math"
#define NAME_MATH_C     "Math_c"
#define NAME_SECTION    "Section"
#define NAME_SUBSECTION "Subsection"

static char *Comment_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Math_attr;		/* math formula [$...$]*/
static char *Math_c_attr;	/* math formula (2) [$$...$$]*/
static char *Number_attr;
static char *Section_attr;	/* subsection */
static char *String_attr;
static char *Subsection_attr;	/* section */

#define MAX_LEVEL 10

static int oldstate = TEXT;
static int state_level = 0;
static int state_stack[MAX_LEVEL];

static void
new_state(int code)
{
    oldstate = code;
    BEGIN(code);
}

static void
push_state(int code)
{
    state_stack[state_level++] = oldstate;
    new_state(code);
}

static void
pop_state(void)
{
    if (state_level > 0)
	new_state(state_stack[--state_level]);
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
    yyin = input;

    Comment_attr = class_attr(NAME_COMMENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Ident_attr   = class_attr(NAME_IDENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    Math_attr    = class_attr(NAME_MATH);
    Math_c_attr  = class_attr(NAME_MATH_C);
    Section_attr = class_attr(NAME_SECTION);
    Subsection_attr = class_attr(NAME_SUBSECTION);

    BEGIN(TEXT);
    while (yylex() > 0) {
    }
}

%}

SPACE		[ \t]
NONSPACE	[^ \t]

NAME		[a-zA-Z]+
NONNAME		[^a-zA-Z]

INTEGER		[-+]?([0-9]+)
REAL		[-+]?([0-9]*\.[0-9]+)([eE][+-]?[0-9]+)?
NUMBER		({INTEGER}|{REAL})

%%

\\verb:[^:]+:		{ WriteToken(String_attr); }
\\"begin{verbatim}"	{ WriteToken(String_attr); push_state(CODE); }
<CODE>[^\n]*		{ WriteToken(String_attr); if (strstr(yytext, "\\end{verbatim}")) pop_state(); }

\\{NAME}		{ char *attr = keyword_attr(yytext); if (!attr || !*attr) attr = Keyword_attr; WriteToken(attr); }

\\{NONNAME} 		{ flt_puts(yytext, yyleng-1, Ident_attr); flt_putc(yytext[yyleng-1]); }

<TEXT>\$[^\$] 		{ WriteToken(Math_attr); push_state(MATH);}
<MATH>[^\$|/\n]+	{ WriteToken(Math_attr); }
<MATH>\$ 		{ WriteToken(Math_attr); pop_state(); }

<TEXT>\$\$	 	{ WriteToken(Math_c_attr); push_state(MATHC);}
<MATHC>[^\$/\n]+	{ WriteToken(Math_c_attr); }
<MATHC>\$\$		{ WriteToken(Math_c_attr); pop_state(); }

\|			{ WriteToken(Ident2_attr); push_state(VARS); }
<VARS>[^\n\|]*[\n\|]	{ WriteToken(Ident2_attr); pop_state(); }

{SPACE}*\{		{ WriteToken(String_attr); }

{SPACE}*\} 		{ WriteToken(String_attr); }

<TEXT>"%".*$		{ WriteToken(Comment_attr); }

{NUMBER}		{ WriteToken(Number_attr); }
