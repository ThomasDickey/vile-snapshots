%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/ada-filt.l,v 1.11 2005/07/11 22:30:54 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Ada
 * input text.
 */

#include <filters.h>

DefineFilter("ada");

static char *Comment_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

%}

INTEGER		[-+]?([0-9]+)
REAL		[-+]?([0-9]*\.[0-9]+)([eE][+-]?[0-9]+)?
IDENT		[a-zA-Z_][a-zA-Z_0-9]*

QUOTECHAR	\'([^']|(''))\'
STRING		\"([^"]|(""))*\"

NUMBER		{INTEGER}|{REAL}

%%

{IDENT}			{ WriteToken(ci_keyword_attr(yytext)); }
"<<"{IDENT}">>"		{ WriteToken(Ident2_attr); }
{QUOTECHAR}		|
{STRING}		{ WriteToken(String_attr); }
{NUMBER}		{ WriteToken(Number_attr); }
"--"[^\n]*		{ WriteToken(Comment_attr); }

%%

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}
