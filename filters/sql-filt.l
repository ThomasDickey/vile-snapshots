%s COMMENT

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sql-filt.l,v 1.6 1999/03/07 19:21:02 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of SQL script
 * - T.Dickey
 */

#include <filters.h>

char *filter_name = "sql";

static char *Action_attr;
static char *Comment_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

void
init_filter(int before GCC_UNUSED)
{
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr   = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}

%}

SPACE		[ \t]*

ACTION		@

COMMENT1	^{SPACE}(--|[rR][eE][mM]([aA][rR][kK])?[^A-Za-z])[^\n]*

UNITS		[kKmM]
INTEGER		[0-9]+
SIGN		[+-]
REAL		{INTEGER}?(\.{INTEGER}([eE]{SIGN}?{INTEGER})?)
NUMBER		{SIGN}?({INTEGER}|{REAL})({UNITS})?

SSTRING		\'([^\']|\'\')*\'
DSTRING		\"([^"])*\"
STRING		({SSTRING}|{DSTRING})

IDENT		[a-zA-Z][a-zA-Z0-9_]*
LABEL		"<<"{IDENT}">>"
HOSTVAR		":"{IDENT}

%%

{LABEL}		|
{ACTION}		{ WriteToken(Action_attr); }

{IDENT}			{ WriteToken(ci_keyword_attr(yytext)); }

{HOSTVAR}		{ write_token(yyout, yytext, 1, Action_attr);
			  WriteToken2(Ident_attr,1);
			}

{NUMBER}		{ WriteToken(Number_attr); }

{STRING}		{ WriteString(String_attr); }

{COMMENT1}		{ WriteToken(Comment_attr); }

"/*"			{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<COMMENT>[^*]*		{ WriteString(Comment_attr); }
<COMMENT>"*"+[^*/]*	{ WriteString(Comment_attr); }
<COMMENT>"*"+"/"	{ WriteToken(Comment_attr); BEGIN(INITIAL); }