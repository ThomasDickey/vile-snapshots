%pointer
%s NORMAL QUOTED QUOTE1 QUOTE2

%{

/*
 * $Id: tbl-filt.l,v 1.18 2013/12/02 01:32:53 tom Exp $
 *
 * Filter to add vile "attribution" sequences to a vile mktbls datafile.
 */

#include <filters.h>

DefineFilter(tbl);

static char *Comment_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static int mixedcase(const char *name);

%}

COMMENT		[ \t]*#[^\r\n]*\n

KEYWORD		[[:alpha:]_.][[:alnum:]$_.]*

SIGN		[-+]
DECIMAL		[[:digit:]_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[[:xdigit:]_]+
REAL		[-+]?([[:digit:]_]*\.[[:digit:]][[:digit:]_]*)([eE][+-]?[[:digit:]_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

%%

<NORMAL>{KEYWORD}	{ WriteToken(mixedcase(yytext) ? get_keyword_attr(yytext) : Preproc_attr); }

<NORMAL>{COMMENT}	{ WriteToken(Comment_attr); }

<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }

<NORMAL>\<		{ BeginQuote(QUOTED, String_attr); }
<QUOTED>\>		{ FinishQuote(NORMAL); }
<QUOTED>.		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>^[\n]		{ FinishQuote(NORMAL); }
<QUOTED>[\n]		{ flt_bfr_append(yytext, yyleng); }

<NORMAL>\'		{ BeginQuote(QUOTE1, String_attr); }
<QUOTE1>\'		{ FinishQuote(NORMAL); }
<QUOTE1>.		{ flt_bfr_append(yytext, yyleng); }
<QUOTE1>^[\n]		{ FinishQuote(NORMAL); }
<QUOTE1>[\n]		{ flt_bfr_append(yytext, yyleng); }

<NORMAL>\"		{ BeginQuote(QUOTE2, String_attr); }
<QUOTE2>\"		{ FinishQuote(NORMAL); }
<QUOTE2>.		{ flt_bfr_append(yytext, yyleng); }
<QUOTE2>^[\n]		{ FinishQuote(NORMAL); }
<QUOTE2>[\n]		{ flt_bfr_append(yytext, yyleng); }

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
    InitLEX(inputs);

    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

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
