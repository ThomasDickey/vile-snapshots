%s NORMAL QUOTED QUOTES QUOTE1

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/infofilt.l,v 1.1 2002/11/03 18:05:24 tom Exp $
 *
 * Filter to add vile "attribution" sequences to an info file.
 * (An extension of "text" mode).
 */

#include <filters.h>

DefineFilter("info");

static char *Action_attr;
static char *Comment_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static int
mixedcase(const char *name)
{
    if (strlen(name) == 1)
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
}

static void
do_filter(FILE *input)
{
    yyin = input;

    Action_attr  = class_attr(NAME_ACTION);
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

%}

COMMENT		^-[-]+[ \t]+[^\n]+\n

KEYWORD		([a-zA-Z_][a-zA-Z0-9_.]*|[A-Z][+/&][A-Z])

SIGN		[-+]
DECIMAL		[0-9_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[0-9a-fA-F_]+
REAL		[-+]?([0-9_]*\.[0-9][0-9_]*)([eE][+-]?[0-9_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

NAME		[a-zA-Z0-9_.#-]+
ADDRESS		({NAME}|\.)+
EMAIL		{NAME}@{ADDRESS}

URL		[A-Za-z]+"://"[0-9a-zA-Z%/.~_#?=-]+

%%

<NORMAL>[A-Z]\'{KEYWORD} { ECHO; }

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

<NORMAL>\`		{ BeginQuote(QUOTES, String_attr); }
<QUOTES>\'		{ FinishQuote(NORMAL); }
<QUOTES>.		{ flt_bfr_append(yytext, yyleng); }
<QUOTES>^[\n]		{ FinishQuote(NORMAL); }
<QUOTES>[\n]		{ flt_bfr_append(yytext, yyleng); }

\037			|
\177			{ WriteToken(Action_attr); }

<NORMAL>.		{ ECHO; }
