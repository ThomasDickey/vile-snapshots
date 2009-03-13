%pointer
%s MATCHING CODE

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/awk-filt.l,v 1.20 2008/11/19 23:11:46 tom Exp $
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

%}

BLANK		[ \t]*

DSTRING		\"((\\\")|[^"\r\n])*\"

KEYWORD		[[:alpha:]_][[:alnum:]_]*
IDENT		\$[[:alnum:]_]+

SIGN		[-+]
INTEGER		{SIGN}?([[:digit:]]+)
REAL		{SIGN}?([[:digit:]]*\.[[:digit:]]+)([eE]{SIGN}?[[:digit:]]+)?

PATTERN		"/"(\\.|\[(\\.|.)*\]|[^ \t\r\n])*"/"

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
    Ident_attr   = class_attr(NAME_IDENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(MATCHING);
    nesting = 0;

    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
