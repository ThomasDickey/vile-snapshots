%x MATH MATHC

%{
/* LaTeX Filter......................................................... */
/* Filter to add vile "attribution" sequences to selected bits of LaTeX  */
/* input text.								 */

/*
 * First version written by Pierre Dittgen (dittgen@univ-mlv.fr)
 * modified by Vanessa Conchodon (conchodo@univ-mlv.fr)
 */

#include <filters.h>

char *filter_name = "latex";

#define NAME_MATH       "Math"
#define NAME_MATH_C     "Math_c"
#define NAME_SECTION    "Section"
#define NAME_SUBSECTION "Subsection"

static char *Comment_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Math_attr;		/* math formula [$...$]*/
static char *Math_c_attr;	/* math formula (2) [$$...$$]*/
static char *Number_attr;
static char *Section_attr;	/* subsection */
static char *String_attr;
static char *Subsection_attr;	/* section */

void
init_filter(void)
{
    insert_keyword(NAME_MATH,    ATTR_LITERAL);
    insert_keyword(NAME_MATH_C,  ATTR_LITERAL);
    insert_keyword(NAME_SECTION, ATTR_IDENT);
    insert_keyword(NAME_SUBSECTION,ATTR_IDENT);
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;
    Comment_attr = keyword_attr(NAME_COMMENT);
    Keyword_attr = keyword_attr(NAME_KEYWORD);
    Ident_attr   = keyword_attr(NAME_IDENT);
    Number_attr  = keyword_attr(NAME_NUMBER);
    String_attr  = keyword_attr(NAME_LITERAL);

    Math_attr    = keyword_attr(NAME_MATH);
    Math_c_attr  = keyword_attr(NAME_MATH_C);
    Section_attr = keyword_attr(NAME_SECTION);
    Subsection_attr = keyword_attr(NAME_SUBSECTION);

    while (yylex() > 0) {
    }
}

%}

INTEGER		[-+]?([0-9]+)
REAL		[-+]?([0-9]*\.[0-9]+)([eE][+-]?[0-9]+)?

%%

\\"section"	{ WriteToken(Section_attr); }
\\"subsection"	{ WriteToken(Subsection_attr); }

\\[a-zA-Z][a-zA-Z]* { WriteToken(Keyword_attr); }
\\[^\a-zA-z][ a-zA-Z] { WriteToken(Keyword_attr); }


\\.[^ ] 	{ write_token(yyout, yytext, yyleng-1, Ident_attr); fprintf(yyout, "%c", yytext[yyleng-1]); }

\$[^\$] 	{ WriteToken(Math_attr);
		  BEGIN(MATH);}
<MATH>[^\$/\n]+ { WriteToken(Math_attr); }
<MATH>\$ 	{ WriteToken(Math_attr);
                  BEGIN(0);
		  }

\$\$	 	{ WriteToken(Math_c_attr);
		  BEGIN(MATHC);}
<MATHC>[^\$/\n]+ { WriteToken(Math_c_attr); }
<MATHC>\$\$	{ WriteToken(Math_c_attr);
                  BEGIN(0);
		  }

" "*\{	 	{ WriteToken(String_attr); }

" "*\} 		{ WriteToken(String_attr); }

"%".*$		{ WriteToken(Comment_attr); }

{INTEGER}	|
{REAL}		{ WriteToken(Number_attr); }

[a-zA-Z]+	{ fprintf(yyout, "%s", yytext); }
