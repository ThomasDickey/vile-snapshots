%x NORMAL QUOTED

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/pot-filt.l,v 1.3 2004/12/10 00:31:30 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of gettext files.
 */

#include <filters.h>

DefineFilter("pot");

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Number_attr;
static char *Prepro_attr;
static char *String_attr;

%}

SSTRING		\'([^']|\n)*\'

NAME		[a-zA-Z0-9_]

IDENT		[a-zA-Z_]{NAME}*

INTEGER		[-+]?([0-9]+)

FORMAT		"%"[.*0-9l$#+-]*[cdeEfFgGisuxX]

%%

<NORMAL>{IDENT}		{ WriteToken(keyword_attr(yytext)); }
<NORMAL>"#:"[^\n]*	{ WriteToken(Prepro_attr); }
<NORMAL>"#"[^\n]*	{ WriteToken(Comment_attr); }
<NORMAL>{INTEGER}	{ WriteToken(Number_attr); }

<NORMAL>\\.		|
<NORMAL>{SSTRING}	{ WriteToken(String_attr); }
<NORMAL>\"		{ BeginQuote(QUOTED, String_attr); }

<QUOTED>\\.		{ flt_bfr_embed(yytext, yyleng, Action_attr); }
<QUOTED>{FORMAT}	{ flt_bfr_embed(yytext, yyleng, Number_attr); }
<QUOTED>[^%\n"\\]+	{ flt_bfr_append(yytext, yyleng); }
<QUOTED>"%%"		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>"%"		{ flt_error("unexpected '%'");
			  flt_bfr_embed(yytext, yyleng, Error_attr); }
<QUOTED>[\n]		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>"\""		{ FinishQuote(NORMAL); }

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
    Error_attr   = class_attr(NAME_ERROR);
    Ident_attr   = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    Prepro_attr  = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);

    while (yylex() > 0) {
    }
    flt_bfr_error();
}
