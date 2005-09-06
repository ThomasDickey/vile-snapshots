%x NORMAL

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/wbt-filt.l,v 1.3 2005/09/04 21:15:43 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of WinBatch script.
 */

#include <filters.h>

DefineFilter("wbt");

static char *want_attr(char *want, char *name);

static char *Comment_attr;
static char *Error_attr;
static char *Number_attr;
static char *Prepro_attr;
static char *String_attr;

%}

BSTRING		\`([^`\n]|``)*\`
SSTRING		\'([^'\n]|'')*\'
DSTRING		\"([^"\n]|"")*\"
STRING		{DSTRING}|{SSTRING}|{BSTRING}

NAME		[[:alnum:]]

IDENT		[[:alpha:]]{NAME}*

DECIMAL		[[:digit:]]+
REAL		[-+]?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?
NUMBER		({REAL}|{DECIMAL})

%%

<NORMAL>{IDENT}		{ WriteToken(ci_keyword_attr(yytext)); }
<NORMAL>#{IDENT}	{ WriteToken(want_attr(Prepro_attr, yytext)); }
<NORMAL>"@"{IDENT}	{ WriteToken(want_attr(Number_attr, yytext)); }

<NORMAL>";"[^\n]*	{ WriteToken(Comment_attr); }

<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }
<NORMAL>[[:digit:]]{NUMBER}{NAME}+ {
			  flt_error("not a number");
			  WriteToken(Error_attr);
			}
<NORMAL>{STRING}	{ WriteToken(String_attr); }

%%

static char *
want_attr(char *want, char *name)
{
    char *have = ci_keyword_attr(name);
    if (have != want)
	have = Error_attr;
    return have;
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    Number_attr  = class_attr(NAME_NUMBER);
    Prepro_attr  = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);

    while (yylex() > 0) {
    }
}
