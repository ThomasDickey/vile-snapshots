%x COMMENT CODE

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/rexxfilt.l,v 1.3 2000/01/31 00:23:08 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of REXX source
 * - T.Dickey
 */

#include <filters.h>

char *filter_name = "rexx";

static char *Comment_attr;
static char *Number_attr;
static char *String_attr;

static int my_state;

void
init_filter(int before GCC_UNUSED)
{
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;

    Comment_attr = class_attr(NAME_COMMENT);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    my_state = CODE;
    BEGIN(my_state);
    while (yylex() > 0) {
    }
}
%}

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

KEYWORD		[a-zA-Z@#$!?_][a-zA-Z0-9.@#$!?_]*

SIGN		[-+]
DECIMAL		[0-9_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[0-9a-fA-F_]+
REAL		[-+]?([0-9_]*\.[0-9][0-9_]*)([eE][+-]?[0-9_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

%%

<CODE>{KEYWORD}		{ WriteToken(ci_keyword_attr(yytext)); }

<CODE>"/*"		{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<COMMENT>[^*]*		{ WriteToken(Comment_attr); }
<COMMENT>"*"+[^*/]*	{ WriteToken(Comment_attr); }
<COMMENT>"*"+"/"	{ WriteToken(Comment_attr); BEGIN(my_state); }

<CODE>{STRINGS}		{ WriteToken(String_attr); }

<CODE>{NUMBER}		{ WriteToken(Number_attr); }
