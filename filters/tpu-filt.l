%pointer
%state NORMAL

%{

/*
 * $Id: tpu-filt.l,v 1.4 2025/01/26 11:44:07 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of TPU files.
 */

#include <filters.h>

DefineFilter(tpu);

static char *Comment_attr;
static char *Ident_attr;
static char *Number_attr;
static char *String_attr;

%}

BLANK		[ \t]
IDENT		([%])?[[:alpha:]_][$[:alnum:]_]*

INLINECOMMENT	![^\r\n]*
COMMENT		^({BLANK}*)($)?({BLANK}*)(![^\r\n]*)?

BINARYNUM	"%"[bB][01]+
OCTALNUM	"%"[oO][0-7]+
HEXNUM		"%"[xX][0-9a-fA-Z]+
NUMBER		([[:digit:]]+)

SSTRING		\'([^'\r\n]|(''))*\'
DSTRING		\"([^"\r\n]|(""))*\"

%%

<NORMAL>{INLINECOMMENT}	|
<NORMAL>{COMMENT}	{ WriteToken(Comment_attr); }

<NORMAL>{BINARYNUM}	|
<NORMAL>{OCTALNUM}	|
<NORMAL>{HEXNUM}	|
<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }

<NORMAL>{IDENT}		{ WriteToken(ci_keyword_attr(yytext)); }

<NORMAL>{SSTRING}	{ WriteToken(String_attr); }
<NORMAL>{DSTRING}	{ WriteToken(String_attr); }

<NORMAL>[\n]		{ ECHO; }

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
    Ident_attr = class_attr(NAME_IDENT);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
