%x NORMAL VALUE COMMENT QUOTED

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/ini-filt.l,v 1.3 2005/05/10 20:19:46 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of M$ ini-definition
 * input text.
 */

#include <filters.h>

DefineFilter("ini");

static char *Action_attr;
static char *Comment_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

static char *
my_keyword_attr(char *name)
{
    char *result = ci_keyword_attr(name);
    if (result == 0 || *result == 0)
	result = Ident2_attr;
    return result;
}

%}

INTEGER		([0-9]+)|(0x[a-fA-Z0-9]+)
IDENT		[a-zA-Z][a-zA-Z_0-9.]*[%&!#$]?

STRING		\"([^"]|(""))*\"

NUMBER		{INTEGER}

%%

<NORMAL>"[".*"]"	{ WriteToken(Action_attr); }
<NORMAL>{IDENT}		{ WriteToken(my_keyword_attr(yytext)); }

<NORMAL>"="		{ ECHO; BEGIN(VALUE); }
<NORMAL,VALUE>{STRING}	{ WriteToken(String_attr); }
<NORMAL,VALUE>{NUMBER}	{ WriteToken(Number_attr); }
<VALUE>[\n]		{ ECHO; BEGIN(NORMAL); }

<NORMAL>"//"[^\n]*	{ WriteToken(Comment_attr); }
<NORMAL>";"[^\n]*	{ WriteToken(Comment_attr); }

<NORMAL>"/*"		{ BeginQuote(COMMENT, Comment_attr); }
<COMMENT>[^*]*		{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"*"+[^*/]*	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"*"+"/"	{ FinishQuote(NORMAL); }

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

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
}