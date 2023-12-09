%pointer
%x NORMAL VALUE COMMENT QUOTED

%{

/*
 * $Id: ini-filt.l,v 1.20 2023/09/12 21:01:11 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of M$ ini-definition
 * input text.
 */

#include <filters.h>

DefineFilter(ini);

static char *Action_attr;
static char *Comment_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

static const char *
my_keyword_attr(char *name)
{
    const char *result = get_keyword_attr(name);
    if (isEmpty(result))
	result = Ident2_attr;
    return result;
}

%}

HEX		[[:xdigit:]]+
GUID		G"{"{HEX}("-"{HEX})+"}"

INTEGER		([[:digit:]]+)|(0x[[:xdigit:]]+)
IDENT		[[:alpha:]][[:alnum:]_.-]*[%&!#$]?

STRING		\"([^"\r\n]|(""))*\"

NUMBER		{INTEGER}|{GUID}

%%

<NORMAL>"["[^]]*"]"	{ WriteToken(Action_attr); }
<NORMAL>{IDENT}		{ WriteToken(my_keyword_attr(yytext)); }

<NORMAL>[=:]		{ ECHO; BEGIN(VALUE); }
<NORMAL,VALUE>{STRING}	{ WriteToken(String_attr); }
<NORMAL,VALUE>{NUMBER}	{ WriteToken(Number_attr); }
<VALUE>[\r\n]		{ ECHO; BEGIN(NORMAL); }

<NORMAL>"//"[^\r\n]*	{ WriteToken(Comment_attr); }
<NORMAL>[#;][^\r\n]*	{ WriteToken(Comment_attr); }

<NORMAL>"/*"		{ BeginQuote(COMMENT, Comment_attr); }
<COMMENT>[^*]*		{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"*"+[^*/]*	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"*"+"/"	{ FinishQuote(NORMAL); }

%%

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
    Ident2_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
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
