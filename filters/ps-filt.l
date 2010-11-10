%pointer
%x NORMAL LIT_TEXT HEX_TEXT B85_TEXT

%o 6000

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/ps-filt.l,v 1.13 2010/11/04 09:25:29 tom Exp $
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

%}

NAME		[[:alpha:]$_!@#&*_+=|:\";'\,.?]

ACTION		[/{}[\]]
IDENT		{NAME}({NAME}|[[:digit:]])*

SIGN		[-+]
INTEGER		{SIGN}?([[:digit:]]+)
REAL		{SIGN}?[[:digit:]]+\.[[:digit:]]*|{SIGN}?\.[[:digit:]]+
RADIX		([[:digit:]]+)"#"([[:xdigit:]]+)
NUMBER		{INTEGER}|{REAL}|{RADIX}

%%

<NORMAL>{IDENT}		{ WriteToken(keyword_attr(yytext)); }

<NORMAL>"%"[^\r\n]*	{ WriteToken(Comment_attr); }

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
<HEX_TEXT>[[:xdigit:]]+	{ flt_bfr_append(yytext, yyleng); }
<HEX_TEXT>[^>[:xdigit:]]+ { flt_error("expected a number");
			  flt_bfr_embed(yytext, yyleng, Error_attr); }
<HEX_TEXT>">"		{ FinishQuote(NORMAL); }

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

    Action_attr = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    RunLEX();
    flt_bfr_error();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
