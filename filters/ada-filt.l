%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/ada-filt.l,v 1.5 1999/03/07 19:16:03 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Ada 
 * input text.
 */

#include <filters.h>

char *filter_name = "ada";

static char *Comment_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

void
init_filter(int before GCC_UNUSED)
{
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;

    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
    exit(0);
}
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
