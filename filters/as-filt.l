%x NORMAL COMMENT QUOTED

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/as-filt.l,v 1.6 2006/05/21 19:50:19 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of assembler files.
 */

#include <filters.h>

DefineFilter("as");

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Number_attr;
static char *Prepro_attr;
static char *String_attr;

static char * expect_attr(char *name);

%}

SSTRING		\'([^']|\n)*\'

NAME		[[:alnum:]_.$]

IDENT		[[:alpha:]_.]{NAME}*

BINARY		0[bB][0-1]+
DECIMAL		[[:digit:]]+
HEX		0[xX][[:xdigit:]]+
INTEGER		({BINARY}|{DECIMAL}|{HEX})

FORMAT		"%"[.*[:digit:]l#+-]*[cdeEfFgGisuxX]

%%

<NORMAL>"$"		{ WriteToken(Prepro_attr); }
<NORMAL>{IDENT}		{ WriteToken(ci_keyword_attr(yytext)); }
<NORMAL>"%"{IDENT}	{ WriteToken(expect_attr(yytext)); }
<NORMAL>";"[^\r\n]*	{ WriteToken(Comment_attr); }
<NORMAL>{INTEGER}	{ WriteToken(Number_attr); }

<NORMAL>"^#.*"		{ WriteToken(Prepro_attr); }

<NORMAL>\\.		|
<NORMAL>{SSTRING}	{ WriteToken(String_attr); }
<NORMAL>\"		{ BeginQuote(QUOTED, String_attr); }

<QUOTED>\\.		{ flt_bfr_embed(yytext, yyleng, Action_attr); }
<QUOTED>{FORMAT}	{ flt_bfr_embed(yytext, yyleng, Number_attr); }
<QUOTED>[^%\r\n"\\]+	{ flt_bfr_append(yytext, yyleng); }
<QUOTED>"%%"		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>"%"		{ flt_bfr_embed(yytext, yyleng, Error_attr); }
<QUOTED>[\n]		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>"\""		{ FinishQuote(NORMAL); }

<NORMAL>"/*"		{ BeginQuote(COMMENT, Comment_attr); }
<COMMENT>[^*]*		{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"*"+[^*/]*	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"*"+"/"	{ FinishQuote(NORMAL); }

%%

static char *
expect_attr(char *name)
{
    char *attr = ci_keyword_attr(name);
    if (attr == 0) {
	attr = Error_attr;
	flt_error("unexpected token");
    }
    return attr;
}

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    Ident_attr   = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    Prepro_attr  = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);

    while (yylex() > 0) {
    }
    flt_bfr_error();
}
