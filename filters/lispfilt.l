%x NORMAL QUOTED

%{
/*
 * $Id: lispfilt.l,v 1.3 2003/05/20 20:38:41 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of lisp code.
 */

#include <filters.h>

DefineFilter("lisp");

static char *Keyword_attr;
static char *Comment_attr;
static char *Ident_attr;
static char *String_attr;
static char *Number_attr;

%}

COMMENT		;[^\n]*
IDENT		[^[:space:][:digit:]\"\(\)]+
NUMBER		(\#[BbOoDdXxEeIi])?([+-])?([[:digit:]/.e])+

%%

<NORMAL>{COMMENT}	{ WriteToken(Comment_attr); }

<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }

<NORMAL>\({IDENT}	{ flt_putc('('); WriteToken2(Keyword_attr, 1); }

<NORMAL>{IDENT}		{ WriteToken(Ident_attr); }

<NORMAL>\"		{ BeginQuote(QUOTED, String_attr); }

<QUOTED>\\.		{ flt_bfr_append(yytext, yyleng); }

<QUOTED>[^\\\"]+	{ flt_bfr_append(yytext, yyleng); }

<QUOTED>\"		{ FinishQuote(NORMAL); }

%%

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Keyword_attr  = class_attr(NAME_KEYWORD);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr   = class_attr(NAME_IDENT2);
    String_attr  = class_attr(NAME_LITERAL);
	Number_attr  = class_attr(NAME_NUMBER);

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
    flt_bfr_error();
}
