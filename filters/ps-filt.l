%x NORMAL LIT_TEXT HEX_TEXT B85_TEXT

%o6000

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/ps-filt.l,v 1.2 2002/10/11 20:42:59 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of PostScript
 * files.
 */

#include <filters.h>

DefineFilter("ps");

#define isQuote(ch) (ch != 0 && strchr("\\'\"", ch) != 0)

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Number_attr;
static char *String_attr;

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
    yyin = input;

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    Ident_attr   = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
    flt_bfr_error();
}

%}

NAME		[a-zA-Z$_!@#&*_+=|:";',.?]

ACTION		[/{}[\]]
IDENT		{NAME}({NAME}|[0-9])*

SIGN		[-+]
INTEGER		{SIGN}?([0-9]+)
REAL		{SIGN}?[0-9]+\.[0-9]*|{SIGN}?\.[0-9]+
RADIX		([0-9]+)"#"([0-9A-Fa-f]+)
NUMBER		{INTEGER}|{REAL}|{RADIX}

%%

<NORMAL>{IDENT}		{ WriteToken(keyword_attr(yytext)); }

<NORMAL>"%"[^\n]*	{ WriteToken(Comment_attr); }

<NORMAL>{ACTION}	{ WriteToken(Action_attr); }

<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }

<NORMAL>"("		{ BeginQuote(LIT_TEXT, String_attr); }
<LIT_TEXT>\\[nrtbf\\()]	{ flt_bfr_embed(yytext, yyleng, Ident_attr); }
<LIT_TEXT>\\[0-7]+	{ flt_bfr_embed(yytext, yyleng, Ident_attr); }
<LIT_TEXT>\\\n		{ flt_bfr_embed(yytext, yyleng, Ident_attr); }
<LIT_TEXT>[^\\)]	{ flt_bfr_append(yytext, yyleng); }
<LIT_TEXT>")"		{ FinishQuote(NORMAL); }

<NORMAL>"<<"	|
<NORMAL>">>"		{ WriteToken(Action_attr); }

<NORMAL>"<"		{ BeginQuote(HEX_TEXT, String_attr); }
<HEX_TEXT>[0-9A-Fa-f]+	{ flt_bfr_append(yytext, yyleng); }
<HEX_TEXT>[^>0-9A-Fa-f]+ { flt_bfr_embed(yytext, yyleng, Error_attr); }
<HEX_TEXT>">"		{ FinishQuote(NORMAL); }
