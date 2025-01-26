%pointer
%x CODE COMMENT HERE_2Q HERE_1Q

%{

/*
 * $Id: ps1-filt.l,v 1.15 2025/01/26 11:35:28 tom Exp $
 *
 * Filter to add vile "attribution" sequences to PowerShell scripts.
 * - T.Dickey
 *
 * see
 *	http://technet.microsoft.com/en-us/magazine/hh551144.aspx
 *	http://technet.microsoft.com/en-us/magazine/dd347662.aspx
 *	https://svn.fperrin.net/powershell-mode/powershell-mode.el
 *
 * TODO: flag errors on other unterminated strings
 */

#define UNUSED_POP_STATE
#include <filters.h>
#include <fltstack.h>

DefineFilter(ps1);

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

%}

BLANK		[[:blank:]]*

SSTRING		\'([^'])*\'
DSTRING		\"([^`"]|(`.))*\"
STRINGS		({SSTRING}|{DSTRING})

NAME		[[:alpha:]_][[:alnum:]_]*
IDENT		((-)?{NAME})|({NAME}(-{NAME})*)

SIGN		[-+]
DECIMAL		[[:digit:]_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[[:xdigit:]_]+
REAL		[-+]?([[:digit:]_]*\.[[:digit:]][[:digit:]_]*)([eE][+-]?[[:digit:]_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

%%

<CODE>"${"{IDENT}"}"	|
<CODE>"$"{IDENT}	{ WriteToken(Ident2_attr); }
<CODE>{IDENT}		{ WriteToken(get_keyword_attr(yytext)); }
<CODE>";"		{ ECHO; }

<CODE>"<#"		{ BeginQuote(COMMENT, Comment_attr); }
<COMMENT>[^#]*		{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"#"+[^#>]*	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"#"+">"	{ FinishQuote(CODE); }

<CODE>"#"[^\r\n]*	{ WriteToken(Comment_attr); }

<CODE>{STRINGS}		{ WriteToken(String_attr); }
<CODE>[`][\r\n]		{ WriteToken3(Action_attr,1); WriteToken2("",1); }
<CODE>[`][0abfnrtv]	{ WriteToken(Action_attr); }
<CODE>[`].		{ WriteToken(Error_attr); }

<CODE>{NUMBER}		{ WriteToken(Number_attr); }

<CODE>"@"["]		{
			    BeginQuote(HERE_2Q, String_attr);
			}
<HERE_2Q>["]"@"		{
			    FinishQuote(CODE);
			    flt_bfr_finish();
			}
<HERE_2Q>[`][\r\n]	{ flt_bfr_embed(yytext, 1, Action_attr);
			  flt_bfr_embed(yytext + 1, 1, ""); }
<HERE_2Q>[`][0abfnrtv]	{ flt_bfr_embed(yytext, yyleng, Action_attr); }
<HERE_2Q>"$"{IDENT}	{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }
<HERE_2Q>[\r\n]		|
<HERE_2Q>.		{ flt_bfr_append(yytext, yyleng); }

<CODE>"@"[']		{
			    BeginQuote(HERE_1Q, String_attr);
			}
<HERE_1Q>[']"@"		{
			    FinishQuote(CODE);
			    flt_bfr_finish();
			}
<HERE_1Q>[\r\n]		|
<HERE_1Q>.		{ flt_bfr_append(yytext, yyleng); }

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
    Ident2_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    begin_state(CODE);
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
