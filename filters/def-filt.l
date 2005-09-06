%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/def-filt.l,v 1.7 2005/09/04 21:10:12 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of M$ linker-definition
 * input text.
 */

#include <filters.h>

DefineFilter("def");

static char *Comment_attr;
static char *Number_attr;
static char *String_attr;

%}

INTEGER		([[:digit:]]+)|(0x[[:xdigit:]]+)
IDENT		[!$]?[[:alpha:]][[:alnum:]_]*[%&!#$]?

SSTRING		\'([^']|(''))*\'
DSTRING		\"([^"]|(""))*\"
STRING		{SSTRING}|{DSTRING}

NUMBER		{INTEGER}

%%

{IDENT}			{ WriteToken(ci_keyword_attr(yytext)); }
{STRING}		{ WriteToken(String_attr); }
{NUMBER}		{ WriteToken(Number_attr); }
";"[^\n]*		{ WriteToken(Comment_attr); }

%%

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Comment_attr = class_attr(NAME_COMMENT);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}
