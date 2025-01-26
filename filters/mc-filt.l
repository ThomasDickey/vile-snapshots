%pointer
%x NORMAL NEXTCMD COMMAND FIELD NEXTARG PARAMS SSTRING DSTRING

%o 10000
%a 10000

%{

/*
 * $Id: mc-filt.l,v 1.14 2025/01/26 11:06:34 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of mailcap-file.
 */

#include <filters.h>
#include <fltstack.h>

DefineFilter(mailcap);

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Number_attr;
static char *String_attr;

%}

BLANK		[ \t]

SSTRING		\'([^']|\n)*\'

NAME		[[:alnum:]_\.-]

WILDCARD	(\?|\*)
WILDNAME	({NAME}|{WILDCARD})
FILENAME	(([./]{WILDNAME}+)|({WILDNAME}+[./]{WILDNAME}*)|({NAME}*{WILDCARD})|\.+\/+)+

INTEGER		[-+]?([[:digit:]]+)

IDENT		[[:alpha:]_]{NAME}*
IDENT1		\${NAME}+
IDENT2		\$\{[#]?{IDENT}\}
IDENTX		\$[\*@#\?\$!-]

MIMELEAF	[[:alpha:]*]([[:alnum:]*.+-])*
MIMENAME	{MIMELEAF}("/"{MIMELEAF})?

%%

<NORMAL>{MIMENAME}	{ WriteToken(Ident_attr); new_state(NEXTCMD); }
<NORMAL>^[#][^\r\n]*	{ WriteToken(Comment_attr); }
<NORMAL>{BLANK}		{ ECHO; }
<NORMAL>.		{ WriteToken(Error_attr); }

<NEXTCMD>{BLANK}	{ ECHO; }
<NEXTCMD>";"		{ ECHO; new_state(COMMAND); }
<NEXTCMD>[\r\n]		{ ECHO; new_state(NORMAL); }

<COMMAND>{BLANK}	{ ECHO; }
<COMMAND>{IDENT}	|
<COMMAND>{FILENAME}	{ ECHO; new_state(PARAMS); }
<COMMAND>[\r\n]		{ ECHO; new_state(NORMAL); }

<FIELD>{BLANK}		{ ECHO; }
<FIELD>{IDENT}		{ WriteToken(get_keyword_attr(yytext)); new_state(NEXTARG); }

<NEXTARG>{BLANK}	{ ECHO; }
<NEXTARG>";"		{ ECHO; new_state(FIELD); }
<NEXTARG>"="		{ ECHO; new_state(PARAMS); }
<NEXTARG>[\r\n]		{ ECHO; new_state(NORMAL); }

<PARAMS>"%"[nFts]	{ WriteToken(Action_attr); }
<PARAMS>"%{"{IDENT}"}"	{ WriteToken(Action_attr); }

<PARAMS>{FILENAME}	|
<PARAMS>{IDENT}		{ ECHO; }

<PARAMS>{INTEGER}	{ WriteToken(Number_attr); }

<PARAMS>{IDENT1}	|
<PARAMS>{IDENT2}	|
<PARAMS>{IDENTX}	{ WriteToken(Ident_attr); }

<PARAMS>\\[n]		{ flt_bfr_embed(yytext, 1, Action_attr); flt_bfr_append(yytext + 1, yyleng - 1); }
<PARAMS>\\.		|
<PARAMS>\'		{ PushQuote(SSTRING, String_attr); }
<PARAMS>\"		{ PushQuote(DSTRING, String_attr); }

<PARAMS>";"		{ ECHO; new_state(FIELD); }
<PARAMS>[\r\n]		{ ECHO; new_state(NORMAL); }

<DSTRING>\\[\n]		|
<DSTRING>\\.		{ flt_bfr_append(yytext, yyleng); }
<DSTRING>"%"[nFts]	{ flt_bfr_embed(yytext, yyleng, Action_attr); }
<DSTRING>"%{"{IDENT}"}"	{ flt_bfr_embed(yytext, yyleng, Action_attr); }
<DSTRING>{IDENT1}	|
<DSTRING>{IDENT2}	{ flt_bfr_embed(yytext, yyleng, Ident_attr); }
<DSTRING>[^%\n\\\"]+	{ flt_bfr_append(yytext, yyleng); }
<DSTRING>[\n]		|
<DSTRING>\"		{ PopQuote(); }

<SSTRING>\\[\n]		|
<SSTRING>\\.		{ flt_bfr_append(yytext, yyleng); }
<SSTRING>"%"[nFts]	{ flt_bfr_embed(yytext, yyleng, Action_attr); }
<SSTRING>"%{"{IDENT}"}"	{ flt_bfr_embed(yytext, yyleng, Action_attr); }
<SSTRING>[^%\n\\\']+	{ flt_bfr_append(yytext, yyleng); }
<SSTRING>[\n]		|
<SSTRING>\'		{ PopQuote(); }

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
    Error_attr = class_attr(NAME_ERROR);
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
