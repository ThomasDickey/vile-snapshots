%s VERB ARGS

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/tcl-filt.l,v 1.8 2000/02/10 11:30:12 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of TCL/TK script.
 */

#include <filters.h>

DefineFilter("tcl");

#define isIdent(ch) (isalnum(ch) || ch == '_')

static char *Comment_attr;
static char *Action_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keywrd2_attr;
static char *String_attr;
static char *Number_attr;

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;
    Comment_attr = class_attr(NAME_COMMENT);
    Action_attr  = class_attr(NAME_ACTION);
    Ident_attr   = class_attr(NAME_IDENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keywrd2_attr = class_attr(NAME_KEYWRD2);
    String_attr  = class_attr(NAME_LITERAL);
    Number_attr  = class_attr(NAME_NUMBER);

    BEGIN(VERB);
    while (yylex() > 0) {
    }
}

%}

SPACE		[ \t]
STRING1		\'([^\\']|\\[^'])*\'
STRING2		\"([^"]|\\.)*\"

ACTION		[\.`{}]
IDENT		[a-zA-Z_-][a-zA-Z_0-9-]*
IDENT1		\$[a-zA-Z_0-9]+
IDENT2		\$\{[^\}]*\}
IDENTX		\$[\*@#\?\$!-]
WIDGET		(\.{IDENT})+

SIGN		[-+]
DECIMAL		[0-9]+
OCTAL		0[0-7]+
HEXADECIMAL	0x[0-9a-f]+
EXP		[eE]{SIGN}?{DECIMAL}
REAL		(({DECIMAL}\.{DECIMAL}?)|({DECIMAL}?\.{DECIMAL})){EXP}?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

%%

{IDENT}			{ WriteToken(keyword_attr(yytext)); BEGIN(ARGS); }

<VERB>"#".*$		{ WriteToken(Comment_attr); }
<ARGS>[;\n]		{ ECHO; BEGIN(VERB); }

{ACTION}		{ WriteToken(Action_attr); }

{NUMBER}		{ WriteToken(Number_attr); }

{IDENT1}	|
{IDENT2}	|
{IDENTX}	|
{WIDGET}		{ WriteToken(Ident2_attr); }

\\.		|
{STRING1}	|
{STRING2}		{ WriteToken(String_attr); }
