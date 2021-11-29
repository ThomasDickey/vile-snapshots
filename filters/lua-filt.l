%pointer
%x LONG_STR LONG_CMT

%{

/*
 * $Id: lua-filt.l,v 1.15 2014/03/30 18:27:27 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of lua
 * input text.
 */

#include <filters.h>
#include <fltstack.h>

DefineFilter(lua);

static char *Comment_attr;
static char *Number_attr;
static char *String_attr;

static int long_str = 0;	/* level for long-bracket */

%}

INTEGER		[-+]?([[:digit:]]+)
REAL		[-+]?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?
IDENT		[[:alpha:]_][[:alnum:]_]*

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

NUMBER		{INTEGER}|{REAL}

LONG_L		"["[=]*"["
LONG_R		"]"[=]*"]"

%%

<INITIAL>{IDENT}	{ WriteToken(get_keyword_attr(yytext)); }
<INITIAL>{STRINGS}	{ WriteToken(String_attr); }
<INITIAL>{NUMBER}	{ WriteToken(Number_attr); }

<INITIAL>"--"{LONG_L}	{ WriteToken(Comment_attr); push_state(LONG_CMT); long_str = yyleng - 2; }
<LONG_CMT>{LONG_R}	{ WriteToken(Comment_attr); if ((int) yyleng == long_str) pop_state(); }
<LONG_CMT>[^[]=]+	{ WriteToken(Comment_attr); }
<LONG_CMT>.		{ WriteToken(Comment_attr); }

<INITIAL>"--"[^\r\n]*	{ WriteToken(Comment_attr); }

<INITIAL>{LONG_L}	{ WriteToken(String_attr); push_state(LONG_STR); long_str = yyleng; }
<LONG_STR>{LONG_R}	{ WriteToken(String_attr); if ((int) yyleng == long_str) pop_state(); }
<LONG_STR>[^[]=]+	{ WriteToken(String_attr); }
<LONG_STR>.		{ WriteToken(String_attr); }

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

    Comment_attr = class_attr(NAME_COMMENT);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    begin_state(INITIAL);
    RunLEX();
    end_state();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
