%s MATCHING CODE

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/awk-filt.l,v 1.12 2000/03/17 01:56:25 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of awk
 * input text.
 */

#include <filters.h>

DefineFilter("awk");

static char *Comment_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static int nesting;

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
    yyin = input;

    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr   = class_attr(NAME_IDENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(MATCHING);
    nesting = 0;

    while (yylex() > 0) {
    }
}

%}

BLANK		[ \t]*

DSTRING		\"((\\\")|[^"\n])*\"

KEYWORD		[a-zA-Z_][a-zA-Z_0-9]*
IDENT		\$[a-zA-Z_0-9]+

SIGN		[-+]
INTEGER		{SIGN}?([0-9]+)
REAL		{SIGN}?([0-9]*\.[0-9]+)([eE]{SIGN}?[0-9]+)?

PATTERN		"/"(\\\.|[^ /\n])*"/"

%%

<MATCHING>{PATTERN}	{ WriteToken(String_attr); }

<CODE>"/"		{ ECHO; }

"{"			{ ECHO; if (!nesting++) { BEGIN(CODE); } }

"}"			{ ECHO; if (--nesting == 0) { BEGIN(MATCHING); } }

^{BLANK}*#!.*$		{ WriteToken(Preproc_attr); }

{KEYWORD}		{ WriteToken(keyword_attr(yytext)); }

^{BLANK}*"#".*$		{ WriteToken(Comment_attr); }

{INTEGER}	|
{REAL}			{ WriteToken(Number_attr); }

{IDENT}			{ WriteToken(Ident_attr); }

{DSTRING}		{ WriteToken(String_attr); }
