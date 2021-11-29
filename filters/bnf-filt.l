%pointer

%{

/*
 * $Id: bnf-filt.l,v 1.16 2013/12/02 01:32:54 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of BNF syntax.
 */

#include <filters.h>

DefineFilter(bnf);

static char *Action_attr;
static char *Comment_attr;
static char *Number_attr;
static char *String_attr;

%}

IDENT		[@$%&]*[[:alpha:]_][[:alnum:]_-]*

NUMBER		[-+]?([[:digit:]]+|("0"[xX][[:xdigit:]]+))

ESCAPE		=
BSTRING		\'(=.|[^'=])*\'

%%

{IDENT}			{ WriteToken(get_keyword_attr(yytext)); }
"::="			|
[<>\[\]{}?|;+*/-]	{ WriteToken(Action_attr); }

"!"[^\r\n]*		{ WriteToken(Comment_attr); }

{NUMBER}		{ WriteToken(Number_attr); }
{BSTRING}		{ WriteToken(String_attr); }

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
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
