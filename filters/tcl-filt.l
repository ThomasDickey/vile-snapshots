%s VERB ARGS

%a 2500

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/tcl-filt.l,v 1.12 2002/05/11 18:04:30 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of TCL/TK script.
 *
 * note -
 * Solaris 2.7's lex:
 * 251/1000 nodes(%e), 757/2500 positions(%p), 85/500 (%n), 5443 transitions,
 * 301/10000 packed char classes(%k),  2015/2500 packed transitions(%a),  2605/3000 output slots(%o)
 */

#include <filters.h>

DefineFilter("tcl");

#define isIdent(ch) (isalnum(ch) || ch == '_')

static char *Action_attr;
static char *Braces_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Ident_attr;
static char *Keywrd2_attr;
static char *Number_attr;
static char *String_attr;

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
    yyin = input;
    Action_attr  = class_attr(NAME_ACTION);
    Braces_attr  = class_attr("Braces");
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Ident_attr   = class_attr(NAME_IDENT);
    Keywrd2_attr = class_attr(NAME_KEYWRD2);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(VERB);
    while (yylex() > 0) {
    }
}

%}

SPACE		[ \t]

SSTRING		\'([^\\'\n]|\\[^'\n])*\'
DSTRING		\"([^"\n]|\\[^\n])*[\"]?
STRINGS		({SSTRING}|{DSTRING})

ACTION		([\.`]|\\\n)

BASICIDENT      [a-zA-Z_-][a-zA-Z_0-9-]*
IDENT           (::)?{BASICIDENT}(::{BASICIDENT})*
IDENT1		\${IDENT}
IDENT2		\$\{[^\}]*\}
IDENTX		\$[\*@#\?\$!-]
WIDGET		(\.{IDENT})+

SIGN		[-+]
DECIMAL		[0-9]+
OCTAL		0[0-7]+
HEXADECIMAL	0x[0-9a-fA-Z]+
EXP		[eE]{SIGN}?{DECIMAL}
REAL		(({DECIMAL}\.{DECIMAL}?)|({DECIMAL}?\.{DECIMAL})){EXP}?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

NODECIMAL	[0-9]+[g-zG-Z]+
NOOCTAL      	0[0-7]*([89]+[0-7]*)+
NOHEXADECIMAL	0X[0-9a-fA-Z]+
NOREAL		\.?{EXP}
NONUMBER	{SIGN}?({NODECIMAL}|{NOOCTAL}|{NOHEXADECIMAL}|{NOREAL})

%%

[{}]			{ WriteToken(Braces_attr); }
{IDENT}			{ WriteToken(keyword_attr(yytext)); BEGIN(ARGS); }

<VERB>"#"([^\n]|\\\n)*$	{ WriteToken(Comment_attr); }
<ARGS>[;\n]		{ ECHO; BEGIN(VERB); }

{ACTION}		{ WriteToken(Action_attr); }

{NONUMBER}		{ WriteToken(Error_attr); }
{NUMBER}		{ WriteToken(Number_attr); }

{IDENT1}	|
{IDENT2}	|
{IDENTX}	|
{WIDGET}		{ WriteToken(Ident2_attr); }

\{\"\}          |
\\\"			{ WriteToken(""); }

\\.		|
{STRINGS}		{ WriteToken(String_attr); }
