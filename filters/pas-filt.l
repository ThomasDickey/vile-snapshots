%x NORMAL CMT_STANDARD CMT_TURBOC

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/pas-filt.l,v 1.2 2004/03/22 22:55:06 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Pascal
 * input text.  A few TurboC and Delphi extensions are recognized:
 *	#{UNSIGNED}
 *	//COMMENT
 *	{COMMENT}
 */

#include <filters.h>
#include <fltstack.h>

DefineFilter("pas");

static char *Comment_attr;
static char *Number_attr;
static char *String_attr;

%}

HEXADECIMAL	\$[0-9a-fA-F_]+
UNSIGNED	[0-9]+
INTEGER		[-+]?{UNSIGNED}
REAL		[-+]?(({UNSIGNED}?\.{UNSIGNED})|({UNSIGNED}(\.{UNSIGNED}?)?))([eE]{INTEGER})?
IDENT		[a-zA-Z_][a-zA-Z_0-9]*

CHARACTER	"#"{UNSIGNED}
STRING		\'([^'\n]|(''))*\'

NUMBER		{HEXADECIMAL}|{INTEGER}|{REAL}

%%

<NORMAL>{IDENT}		{ WriteToken(ci_keyword_attr(yytext)); }
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

<NORMAL>"//"[^\n]*	{ WriteToken(Comment_attr); /* Delphi */ }

<NORMAL>.		{ ECHO; }

%%

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Comment_attr = class_attr(NAME_COMMENT);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    begin_state(NORMAL);
    while (yylex() > 0) {
    }
    flt_bfr_error();
    end_state();
}
