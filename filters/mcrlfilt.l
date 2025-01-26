%pointer
%s NORMAL

%o 6000

%{

/*
 * $Id: mcrlfilt.l,v 1.9 2025/01/26 11:08:22 tom Exp $
 *
 * Filter to add vile "attribution" sequences to a mCRL(2) specification
 * or model, based on the text filter.
 *
 * By Paul van Tilburg <paul@luon.net> and Bas Kloet <bas@luon.net>.
 */

#include <filters.h>

DefineFilter(mcrl);

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

<NORMAL>{KEYWORD}	{ WriteToken(get_keyword_attr(yytext)); }

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
    InitLEX(inputs);

    Comment_attr = class_attr(NAME_COMMENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);

    BEGIN(NORMAL);
    RunLEX();
    flt_bfr_finish();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
