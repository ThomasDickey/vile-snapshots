%s NORMAL ACTION

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/vilefilt.l,v 1.18 2000/08/28 10:31:13 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of vile macros
 * - T.Dickey
 */

#include <filters.h>

DefineFilter("vile");

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static int
is_prepro(char *text, int len)
{
    if (vile_keywords) {
	char *next = skip_blanks(text);
	int code = dname_to_dirnum(next, (size_t)(len - (next - text)));
	return code >= 0;
    }
    return 1;
}

static char *
command_attr(char *text)
{
    if (vile_keywords) {
	return engl2fnc(text) != 0 ? Keyword_attr : "";
    }
    return keyword_attr(text);
}

static int
is_funckey(char *text)
{
    return (!strncmp(text, "FN-", 3));
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
    yyin = input;

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
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

{PREPROC}	{ WriteToken(is_prepro(yytext, yyleng) ? Keyword_attr : Error_attr); }

{NAME}		{ WriteToken(is_funckey(yytext) ? String_attr : command_attr(yytext)); }

{FUNCTION}	{ WriteToken(vl_lookup_func(yytext) >= 0 ? Ident2_attr : ""); }
{VARIABLE}	{ WriteToken(Ident2_attr); }

{NUMBER}	{ WriteToken(Number_attr); }

\r		|
\\.		|
{CONTROL}	|
{BINDING}	|
{SSTRING}	|
{DSTRING}	{ WriteToken(String_attr); }

{COMMENT}	{ WriteToken(Comment_attr); }
