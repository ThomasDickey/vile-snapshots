%s ECHOING

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/bat-filt.l,v 1.6 2000/02/10 11:30:12 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of DOS (and
 * similar, such as W95, NT) batch file.
 */

#include <filters.h>

DefineFilter("bat");

static char *Action_attr;
static char *Comment_attr;
static char *Ident2_attr;
static char *String_attr;

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    String_attr  = class_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}

%}

TEXT		[^\r\n]
BLANK		[ \t\r]
REM		[rR][eE][mM]
IDENT		[a-zA-Z_][a-zA-Z0-9]*

PARAM		%[0-9]
VARIABLE	%%[a-zA-Z0-9]+

%%

^{BLANK}*@			{ WriteToken(Action_attr); }
\032				{ WriteToken(Action_attr); }

[eE][cC][hH][oO]		{ WriteToken(ci_keyword_attr(yytext)); BEGIN(ECHOING); }
<ECHOING>[^\n]*			{ WriteToken(String_attr); BEGIN(INITIAL); }

^{BLANK}*:{BLANK}*{IDENT}	{ WriteToken(Ident2_attr); }

{IDENT}				{ WriteToken(ci_keyword_attr(yytext)); }

({PARAM}|{VARIABLE})		{ WriteToken(Ident2_attr); }

^{BLANK}*{REM}$	|
^{BLANK}*{REM}({BLANK}+{TEXT}*)? { WriteToken(Comment_attr); }
