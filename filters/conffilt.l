%pointer
%x NORMAL QUOTED

%o 10000
%a 5000

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/conffilt.l,v 1.9 2010/11/04 09:25:28 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of config-file.
 */

#include <filters.h>
#include <fltstack.h>

DefineFilter("conf");

static char *Action_attr;
static char *Comment_attr;
static char *Ident_attr;
static char *Number_attr;
static char *String_attr;

%}

SSTRING		\'([^'\n]|(\\[\n]))*\'

NAME		[[:alnum:]_]

WILDCARD	(\?|\*)
WILDNAME	({NAME}|{WILDCARD})
FILENAME	(([./]{WILDNAME}+)|({WILDNAME}+[./]{WILDNAME}*)|({NAME}*{WILDCARD})|\.+\/+)+

INTEGER		[-+]?([[:digit:]]+)

IDENT		[[:alpha:]_]{NAME}*
IDENT0		[-]+[[:digit:]]*[[:alpha:]_-]+[[:alnum:]_-]*
IDENT1		\${NAME}+
IDENT2		\$\{[#]?{IDENT}\}
IDENTX		\$[\*@#\?\$!-]

%%

<NORMAL>{IDENT0}	|
<NORMAL>{FILENAME}	{ ECHO; /* exclude from other classes */ }

<NORMAL>{IDENT}		{ WriteToken(keyword_attr(yytext)); }

<NORMAL>[;#][^\r\n]*	{ WriteToken(Comment_attr); }

<NORMAL>{INTEGER}	{ WriteToken(Number_attr); }

<NORMAL>{IDENT1}	|
<NORMAL>{IDENT2}	|
<NORMAL>{IDENTX}	{ WriteToken(Ident_attr); }

<NORMAL>\\[n]		{ WriteToken3(Action_attr, 1); WriteToken2("", 1); } 
<NORMAL>\\.		|
<NORMAL>{SSTRING}	{ WriteToken(String_attr); }
<NORMAL>\"		{ PushQuote(QUOTED, String_attr); }

<NORMAL>[\n]		{ ECHO; }

<QUOTED>\\[\n]		|
<QUOTED>\\.		{ flt_bfr_embed(yytext, 1, Action_attr); flt_bfr_append(yytext + 1, yyleng - 1); }
<QUOTED>{IDENT1}	|
<QUOTED>{IDENT2}	{ flt_bfr_embed(yytext, yyleng, Ident_attr); }
<QUOTED>[^`\r\n\\\"]+	{ flt_bfr_append(yytext, yyleng); }
<QUOTED>[\n]		|
<QUOTED>\"		{ PopQuote(); }

%%

#include <fltstack.h>

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
    Ident_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    begin_state(NORMAL);

    RunLEX();
    flt_bfr_error();
    end_state();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
