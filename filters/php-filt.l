%x COMMENT CODE

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/php-filt.l,v 1.4 2005/09/04 20:37:43 tom Exp $
 *
 * Filter to add vile "attribution" sequences to PHP scripts.
 * - T.Dickey
 */

#include <filters.h>

DefineFilter("php");

static char *Action_attr;
static char *Comment_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;
%}

BLANK		[[:blank:]]*

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\]|\\\n)*\"
STRINGS		({SSTRING}|{DSTRING})

IDENT		[[:alpha:]][[:alnum:]_]*

SIGN		[-+]
DECIMAL		[[:digit:]_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[[:xdigit:]_]+
REAL		[-+]?([[:digit:]_]*\.[[:digit:]][[:digit:]_]*)([eE][+-]?[[:digit:]_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

%%

<CODE>"<?"("php")?	{ WriteToken(Action_attr); }
<CODE>"?>"		{ WriteToken(Action_attr); }

<CODE>"$"{IDENT}	{ WriteToken(Ident2_attr); }
<CODE>{IDENT}		{ WriteToken(keyword_attr(yytext)); }
<CODE>";"		{ ECHO; }

<CODE>"/*"		{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<COMMENT>[^*]*		{ WriteToken(Comment_attr); }
<COMMENT>"*"+[^*/]*	{ WriteToken(Comment_attr); }
<COMMENT>"*"+"/"	{ WriteToken(Comment_attr); BEGIN(CODE); }

<CODE>"//"[^\n]*	{ WriteToken(Comment_attr); }

<CODE>{STRINGS}		{ WriteToken(String_attr); }

<CODE>{NUMBER}		{ WriteToken(Number_attr); }

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

    BEGIN(CODE);
    while (yylex() > 0) {
    }
}
