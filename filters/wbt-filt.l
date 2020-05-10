%pointer
%x NORMAL

%{

/*
 * $Id: wbt-filt.l,v 1.13 2020/05/10 20:28:46 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of WinBatch script.
 */

#include <filters.h>

DefineFilter(wbt);

static const char *want_attr(char *want, char *name);

static char *Comment_attr;
static char *Error_attr;
static char *Number_attr;
static char *Preproc_attr;
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

<NORMAL>{IDENT}		{ WriteToken(get_keyword_attr(yytext)); }
<NORMAL>#{IDENT}	{ WriteToken(want_attr(Preproc_attr, yytext)); }
<NORMAL>"@"{IDENT}	{ WriteToken(want_attr(Number_attr, yytext)); }

<NORMAL>";"[^\r\n]*	{ WriteToken(Comment_attr); }

<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }
<NORMAL>[[:digit:]]{NUMBER}{NAME}+ {
			  flt_error("not a number");
			  WriteToken(Error_attr);
			}
<NORMAL>{STRING}	{ WriteToken(String_attr); }

%%

static const char *
want_attr(char *want, char *name)
{
    const char *have = get_keyword_attr(name);
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
    InitLEX(inputs);

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);

    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
