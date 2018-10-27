%pointer
%x NORMAL CMT_STANDARD CMT_TURBOC

%{

/*
 * $Id: pas-filt.l,v 1.13 2018/10/27 11:15:37 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Pascal
 * input text.  A few TurboC and Delphi extensions are recognized:
 *	#{UNSIGNED}
 *	//COMMENT
 *	{COMMENT}
 */

#define UNUSED_POP_STATE
#include <filters.h>
#include <fltstack.h>

DefineFilter(pas);

static char *Comment_attr;
static char *Number_attr;
static char *String_attr;

%}

HEXADECIMAL	\$[[:xdigit:]_]+
UNSIGNED	[[:digit:]]+
INTEGER		[-+]?{UNSIGNED}
REAL		[-+]?(({UNSIGNED}?\.{UNSIGNED})|({UNSIGNED}(\.{UNSIGNED}?)?))([eE]{INTEGER})?
IDENT		[[:alpha:]_][[:alnum:]_]*

CHARACTER	"#"{UNSIGNED}
STRING		\'([^'\r\n]|(''))*\'

NUMBER		{HEXADECIMAL}|{INTEGER}|{REAL}

%%

<NORMAL>{IDENT}		{ WriteToken(get_keyword_attr(yytext)); }
<NORMAL>{STRING}	{ WriteToken(String_attr); }
<NORMAL>{CHARACTER}	|
<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }

<NORMAL>"{"		{ BeginQuote(CMT_TURBOC, Comment_attr); }
<CMT_TURBOC>"}"		{ FinishQuote(NORMAL); }
<CMT_TURBOC>\n		|
<CMT_TURBOC>.		{ flt_bfr_append(yytext, yyleng); }

<NORMAL>"(*"		{ BeginQuote(CMT_STANDARD, Comment_attr); }
<CMT_STANDARD>"*)"	{ FinishQuote(NORMAL); }
<CMT_STANDARD>\n	|
<CMT_STANDARD>.		{ flt_bfr_append(yytext, yyleng); }

<NORMAL>"//"[^\r\n]*	{ WriteToken(Comment_attr); /* Delphi */ }

<NORMAL>.		{ ECHO; }

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
