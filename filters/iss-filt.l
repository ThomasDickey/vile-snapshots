%x SETUP FLAGS VALUE OTHER COMMENT CODE CMT_STANDARD CMT_TURBOC

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/iss-filt.l,v 1.4 2005/09/04 21:15:43 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Inno Setup
 * input text.
 */

#include <filters.h>

DefineFilter("iss");

static char *Action_attr;
static char *Comment_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

%}

BLANK		[ \t]

IDENT		[[:alpha:]_][[:alnum:]_]*

STRING		\'([^'\n]|(''))*\'

HEXADECIMAL	\$[[:xdigit:]_]+
UNSIGNED	[[:digit:]]+
INTEGER		[-+]?{UNSIGNED}
REAL		[-+]?(({UNSIGNED}?\.{UNSIGNED})|({UNSIGNED}(\.{UNSIGNED}?)?))([eE]{INTEGER})?

CHARACTER	"#"{UNSIGNED}

NUMBER		{HEXADECIMAL}|{INTEGER}|{REAL}

%%

<SETUP,FLAGS,CODE>^"["[^]\n]*"]"	{ WriteToken(Action_attr);
			  if (!strcmp(yytext, "[Code]")) {
			    BEGIN(CODE);
			    set_symbol_table("pas");
			  } else {
			    if (!strcmp(yytext, "[Setup]")) {
				BEGIN(SETUP);
			    } else {
				BEGIN(FLAGS);
			    }
			    set_symbol_table(filter_def.filter_name);
			  }
			}
<SETUP,FLAGS,CODE>{IDENT} { WriteToken(ci_keyword_attr(yytext)); }

<SETUP>"="		{ ECHO; BEGIN(VALUE); }
<SETUP,VALUE>{STRING}	{ WriteToken(String_attr); }
<SETUP,VALUE>{NUMBER}	{ WriteToken(Number_attr); }
<VALUE>[\n]		{ ECHO; BEGIN(SETUP); }

<SETUP,FLAGS>"//"[^\n]*	{ WriteToken(Comment_attr); }
<SETUP,FLAGS>^";"[^\n]*	{ WriteToken(Comment_attr); }

<FLAGS>":"{BLANK}*	{ ECHO; BEGIN(OTHER); }
<OTHER>[^;\n]+		{ WriteToken(String_attr); }
<OTHER>";"		{ ECHO; BEGIN(FLAGS); }
<OTHER>[\n]		{ ECHO; BEGIN(FLAGS); }

<CODE>{STRING}		{ WriteToken(String_attr); }
<CODE>{CHARACTER}	|
<CODE>{NUMBER}		{ WriteToken(Number_attr); }

<CODE>"{"		{ BeginQuote(CMT_TURBOC, Comment_attr); }
<CMT_TURBOC>"}"		{ FinishQuote(CODE); }
<CMT_TURBOC>\n		|
<CMT_TURBOC>.		{ flt_bfr_append(yytext, yyleng); }

<CODE>"(*"		{ BeginQuote(CMT_STANDARD, Comment_attr); }
<CMT_STANDARD>"*)"	{ FinishQuote(CODE); }
<CMT_STANDARD>\n	|
<CMT_STANDARD>.		{ flt_bfr_append(yytext, yyleng); }

<CODE>"//"[^\n]*	{ WriteToken(Comment_attr); /* Delphi */ }

<CODE>.			{ ECHO; }

%%

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(FLAGS);
    while (yylex() > 0) {
    }
}
