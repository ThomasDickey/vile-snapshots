%{

/*
 * Filter to add vile "attribution" sequences to selected bits of vile macros
 * - T.Dickey
 */

#include <filters.h>

char *filter_name = "vile";

static char *Comment_attr;
static char *Keyword_attr;
static char *String_attr;
static char *Ident_attr;

void
init_filter(void)
{
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;
    Comment_attr = keyword_attr(NAME_COMMENT);
    Keyword_attr = keyword_attr(NAME_KEYWORD);
    String_attr  = keyword_attr(NAME_LITERAL);
    Ident_attr   = keyword_attr(NAME_IDENT2);
    while (yylex() > 0) {
    }
}

%}

SPACE		[ \t]*
PREPROC		^{SPACE}\~[a-z]+
FUNCTION	&[a-z]+
COMMENT		^{SPACE}[";].*
STRING		\"([^"]|\\[^"])*\"
VARIABLE	[$%][a-zA-Z][a-zA-Z0-9]*

%%

{PREPROC}	{ WriteToken(Keyword_attr); }

{VARIABLE}	|
{FUNCTION}	{ WriteToken(Ident_attr); }

{STRING}	{ WriteToken(String_attr); }

{COMMENT}	{ WriteToken(Comment_attr); }
