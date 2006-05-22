%x COMMENT CODE UUID

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/midlfilt.l,v 1.4 2006/05/21 19:50:19 tom Exp $
 *
 * Filter to add vile "attribution" sequences to MIDL / ODL files.
 * - T.Dickey
 */

#include <filters.h>

DefineFilter("midl");

static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;
%}

STRINGS		\"(\\.|[^"\\]|\\\n)*\"

IDENT		[[:alpha:]][[:alnum:]_]*

SIGN		[-+]
DECIMAL		[[:digit:]_]+
REAL		[-+]?([[:digit:]_]*\.[[:digit:]][[:digit:]_]*)([eE][+-]?[[:digit:]_]+)?
NUMBER		{SIGN}?({DECIMAL}|{REAL})

%%

<CODE>{IDENT}		{ WriteToken(keyword_attr(yytext)); }
<CODE>";"		{ ECHO; }

<CODE>"/*"		{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<COMMENT>[^*]*		{ WriteToken(Comment_attr); }
<COMMENT>"*"+[^*/]*	{ WriteToken(Comment_attr); }
<COMMENT>"*"+"/"	{ WriteToken(Comment_attr); BEGIN(CODE); }

<CODE>"//"[^\r\n]*	{ WriteToken(Comment_attr); }

<CODE>{STRINGS}		{ WriteToken(String_attr); }

<CODE>{NUMBER}		{ WriteToken(Number_attr); }

<CODE>"uuid("		{ WriteToken(Ident2_attr); BEGIN(UUID); }
<UUID>[[:xdigit:]-]+	{ WriteToken(Number_attr); }
<UUID>")"		{ WriteToken(Ident2_attr); BEGIN(CODE); }
<UUID>[^)[:xdigit:][:space:]-]* { WriteToken(Error_attr); }

%%

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(CODE);
    while (yylex() > 0) {
    }
}
