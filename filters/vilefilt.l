%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/vilefilt.l,v 1.7 1998/12/29 15:08:44 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of vile macros
 * - T.Dickey
 */

#include <filters.h>

char *filter_name = "vile";

static char *Comment_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static int
is_funckey(char *text)
{
    return (!strncmp(text, "FN-", 3));
}

void
init_filter(int before GCC_UNUSED)
{
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;

    Comment_attr = keyword_attr(NAME_COMMENT);
    Ident2_attr  = keyword_attr(NAME_IDENT2);
    Keyword_attr = keyword_attr(NAME_KEYWORD);
    Number_attr  = keyword_attr(NAME_NUMBER);
    String_attr  = keyword_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}

%}

SPACE		[ \t]*
NUMBER		[0-9]+
NAME		[a-zA-Z][a-zA-Z0-9]*(-[a-zA-Z0-9]+)*

PREPROC		^{SPACE}\~[a-z]+

COMMENT		^{SPACE}[";].*

ESCAPED		\\.
CTRLV		\026
VISIBLE		.|^.
BINDING		(M-)?((^[AX]-(^)?{VISIBLE})|((#|FN)-?{VISIBLE})|^.)
DSTRING		\"((\\.)|[^\"\n])*\"

FUNCTION	&[a-zA-Z]+

VARIABLE	[$%][a-zA-Z][a-zA-Z0-9]*

%%

{PREPROC}	{ WriteToken(Keyword_attr); }

{NAME}		{ WriteToken(is_funckey(yytext) ? String_attr : keyword_attr(yytext)); }

{VARIABLE}	|
{FUNCTION}	{ WriteToken(Ident2_attr); }

{NUMBER}	{ WriteString(Number_attr); }

{ESCAPED}	|
{CTRLV}		|
{BINDING}	|
{DSTRING}	{ WriteString(String_attr); }

{COMMENT}	{ WriteToken(Comment_attr); }
