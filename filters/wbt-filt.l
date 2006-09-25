%x NORMAL

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/wbt-filt.l,v 1.5 2006/05/21 19:50:48 tom Exp $
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

BSTRING		\`([^`\r\n]|``)*\`
SSTRING		\'([^'\r\n]|'')*\'
DSTRING		\"([^"\r\n]|"")*\"
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

<NORMAL>";"[^\r\n]*	{ WriteToken(Comment_attr); }

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
    (void) before;
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