%s NORMAL 

%o 6000

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/mcrlfilt.l,v 1.1 2006/08/08 21:58:57 Paul.van.Tilburg Exp $
 *
 * Filter to add vile "attribution" sequences to a mCRL(2) specification
 * or model, based on the text filter.
 *
 * By Paul van Tilburg <paul@luon.net> and Bas Kloet <bas@luon.net>.
 */

#include <filters.h>

DefineFilter("mcrl");

static char *Comment_attr;
static char *Keyword_attr;
static char *Number_attr;

%}

COMMENT		%.*\n

KEYWORD		[[:alpha:]_][[:alnum:]_]*

SIGN		[-+]
DECIMAL		[[:digit:]_]+
REAL		[-+]?([[:digit:]_]*\.[[:digit:]][[:digit:]_]*)([eE][+-]?[[:digit:]_]+)?
NUMBER		{SIGN}?({DECIMAL}|{REAL})

%%

<NORMAL>{KEYWORD}	{ WriteToken(keyword_attr(yytext)); }

<NORMAL>{COMMENT}	{ WriteToken(Comment_attr); }

<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }

%%

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Comment_attr = class_attr(NAME_COMMENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
    flt_bfr_finish();
}
