%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/bnf-filt.l,v 1.9 2006/05/21 19:50:19 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of BNF syntax.
 */

#include <filters.h>

DefineFilter("bnf");

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

{IDENT}			{ WriteToken(ci_keyword_attr(yytext)); }
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
    yyin = inputs;
    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}
