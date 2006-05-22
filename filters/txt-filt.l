%s NORMAL QUOTED QUOTES

%o 6000

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/txt-filt.l,v 1.23 2006/05/21 19:50:48 tom Exp $
 *
 * Filter to add vile "attribution" sequences to an ordinary text file
 */

#include <filters.h>

DefineFilter("txt");

static char *Comment_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static int mixedcase(const char *name);

%}

COMMENT		(^(-[-]+[ \t]+[^\r\n]+))|([ \t]*-[ \t-]*)\n

KEYWORD		([[:alpha:]_][[:alnum:]_.]*|[[:upper:]][+/&][[:upper:]])

SIGN		[-+]
DECIMAL		[[:digit:]_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[[:xdigit:]_]+
REAL		[-+]?([[:digit:]_]*\.[[:digit:]][[:digit:]_]*)([eE][+-]?[[:digit:]_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

NAME		[[:alnum:]_.#-]+
ADDRESS		({NAME}|\.)+
EMAIL		{NAME}@{ADDRESS}

URL		[[:alpha:]]+"://"[[:alnum:]#%&+\,.\/?;@_=~-]+

%%

<NORMAL>[[:upper:]]\'{KEYWORD} { ECHO; }

<NORMAL>{KEYWORD}	{ WriteToken(mixedcase(yytext) ? "" : Keyword_attr); }
<NORMAL>{COMMENT}	{ WriteToken(Comment_attr); }
<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }
<NORMAL>{EMAIL}		{ WriteToken(Ident2_attr); }
<NORMAL>{URL}		{ WriteToken(Ident2_attr); }

<QUOTES,QUOTED>{EMAIL}	{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }
<QUOTES,QUOTED>{URL}	{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }

<NORMAL>\(\"\)		|
<NORMAL>\'.\'		{ ECHO; }

<NORMAL>\"		{ BeginQuote(QUOTED, String_attr); }
<QUOTED>\"		{ FinishQuote(NORMAL); }
<QUOTED>.		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>^[\n]		{ FinishQuote(NORMAL); }
<QUOTED>[\n]		{ flt_bfr_append(yytext, yyleng); }

<NORMAL>\`\`		{ BeginQuote(QUOTES, String_attr); }
<QUOTES>\'\'		{ FinishQuote(NORMAL); }
<QUOTES>.		{ flt_bfr_append(yytext, yyleng); }
<QUOTES>^[\n]		{ FinishQuote(NORMAL); }
<QUOTES>[\n]		{ flt_bfr_append(yytext, yyleng); }

<NORMAL>.		{ ECHO; }

%%

static int
mixedcase(const char *name)
{
    if (!strcmp(name, "A") || !strcmp(name, "I"))
	return 1;
    while (*name) {
	int ch = (*name++ & 0xff);
	if (isalpha(ch) && islower(ch))
	    return 1;
    }
    return 0;
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

    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
    flt_bfr_finish();
}
