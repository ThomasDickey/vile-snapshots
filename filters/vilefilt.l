%s NORMAL ACTION

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/vilefilt.l,v 1.14 1999/12/17 23:01:47 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of vile macros
 * - T.Dickey
 */

#include <filters.h>

char *filter_name = "vile";

static char *Action_attr;
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

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
}

%}

SPACE		[ \t]*
NUMBER		[0-9]+
NAME		[a-zA-Z_][a-zA-Z_0-9]*(-[a-zA-Z_0-9]+)*

PREPROC		^{SPACE}\~[a-z]+

COMMENT		^{SPACE}[";].*

CTRLA		\001
CTRLV		\026
CTRLX		\030
ESCAPE		\033
CONTROL		({CTRLA}|{CTRLX}|{ESCAPE})({CTRLV}.|\\.|[^ \t\n])*

VISIBLE		.|^.
BINDING		(M-)?((^[AX]-(^)?{VISIBLE})|((#|FN)-?{VISIBLE})|^.)

SSTRING		(\'(\\.|[^'\n])*\')
DSTRING		\"(\\.|[^\\"\n])*\"

FUNCTION	&[a-zA-Z]+

VARIABLE	[$%]{NAME}

%%

^S$		{ WriteToken(Keyword_attr); }
^[CBSL]:	{ WriteToken(Keyword_attr); BEGIN(ACTION); }
<ACTION>[^\n:]*	{ WriteToken(Action_attr); BEGIN(NORMAL); }

{PREPROC}	{ WriteToken(Keyword_attr); }

{NAME}		{ WriteToken(is_funckey(yytext) ? String_attr : keyword_attr(yytext)); }

{VARIABLE}	|
{FUNCTION}	{ WriteToken(Ident2_attr); }

{NUMBER}	{ WriteString(Number_attr); }

\r		|
\\.		|
{CONTROL}	|
{BINDING}	|
{SSTRING}	|
{DSTRING}	{ WriteString(String_attr); }

{COMMENT}	{ WriteToken(Comment_attr); }
