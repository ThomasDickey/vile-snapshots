%pointer
%x NORMAL COMMENT QUOTED

%{

/*
 * $Id: as-filt.l,v 1.16 2025/01/26 11:05:40 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of assembler files.
 */

#include <filters.h>

DefineFilter(as);

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static const char *expect_attr(char *name);

%}

SSTRING		\'[^'\r\n]*\'

NAME		[[:alnum:]_.$]

IDENT		[[:alpha:]_.]{NAME}*

BINARY		0[bB][0-1]+
DECIMAL		[[:digit:]]+
HEX		0[xX][[:xdigit:]]+
INTEGER		({BINARY}|{DECIMAL}|{HEX})

FORMAT		"%"[.*[:digit:]l#+-]*[cdeEfFgGisuxX]

%%

<NORMAL>"$"		{ WriteToken(Preproc_attr); }
<NORMAL>{IDENT}		{ WriteToken(get_keyword_attr(yytext)); }
<NORMAL>"%"{IDENT}	{ WriteToken(expect_attr(yytext)); }
<NORMAL>";"[^\r\n]*	{ WriteToken(Comment_attr); }
<NORMAL>{INTEGER}	{ WriteToken(Number_attr); }

<NORMAL>"^#.*"		{ WriteToken(Preproc_attr); }

<NORMAL>\\[^\r\n]	|
<NORMAL>{SSTRING}	{ WriteToken(String_attr); }
<NORMAL>\"		{ BeginQuote(QUOTED, String_attr); }

<QUOTED>\\[^\r\n]	{ flt_bfr_embed(yytext, yyleng, Action_attr); }
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

static const char *
expect_attr(char *name)
{
    const char *attr = get_keyword_attr(name);
    if (attr == NULL) {
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
    InitLEX(inputs);

    Action_attr = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
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
