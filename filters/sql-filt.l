%s CODE ACTION COMMENT COMMENT1

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sql-filt.l,v 1.18 2003/05/20 20:38:41 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of SQL script
 * - T.Dickey
 */

#include <filters.h>

DefineFilter("sql");

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static int leading;

%}

SPACE		[ \t]*

ACTION		@[@]?

COMMENT1	^{SPACE}--[^\n]*

UNITS		[kKmM]
INTEGER		[0-9]+
SIGN		[+-]
REAL		{INTEGER}?(\.{INTEGER}([eE]{SIGN}?{INTEGER})?)
NUMBER		{SIGN}?({INTEGER}|{REAL})({UNITS})?

SSTRING		\'([^\']|\'\')*\'
DSTRING		\"([^"])*\"
STRING		({SSTRING}|{DSTRING})

IDENT		[%]?[a-zA-Z][a-zA-Z0-9_$]*
LABEL		"<<"{IDENT}">>"
HOSTVAR		":"{IDENT}

%%

<CODE>\n		|
<CODE>^{SPACE}		{ ECHO; leading = 1; }

<CODE>{LABEL}		|
<CODE>{ACTION}		{ WriteToken(Action_attr); BEGIN(ACTION); leading = 0; }
<ACTION>\n		{ ECHO; BEGIN(CODE); }
<ACTION>[^\n]*		{ WriteToken(String_attr); BEGIN(CODE); }

<CODE>{IDENT}		{   char *text = lowercase_of(yytext);
			    char *attr = ci_keyword_attr(text);
			    if (attr == Comment_attr) {
				WriteToken(leading ? attr : Error_attr);
				BEGIN(COMMENT1);
			    } else if (attr == Action_attr) {
				WriteToken(attr);
				BEGIN(ACTION);
			    } else {
				WriteToken(attr);
				leading = 0;
			    }
			}

<CODE>{HOSTVAR}		{ flt_puts(yytext, 1, Action_attr);
			  WriteToken2(Ident_attr,1);
			  leading = 0;
			}

<CODE>{NUMBER}		{ WriteToken(Number_attr); leading = 0; }

<CODE>{STRING}		{ WriteToken(String_attr); leading = 0; }

<CODE>--[^\n]*		{ WriteToken(Comment_attr); BEGIN(COMMENT1); }
<COMMENT1>[^\n]*	{ WriteToken(Comment_attr); }
<COMMENT1>\n		{ ECHO; BEGIN(CODE); }

<CODE>"/*"		{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<COMMENT>[^*]*		{ WriteToken(Comment_attr); }
<COMMENT>"*"+[^*/]*	{ WriteToken(Comment_attr); }
<COMMENT>"*"+"/"	{ WriteToken(Comment_attr); BEGIN(CODE); }

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
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(CODE);
    leading = 1;
    while (yylex() > 0) {
    }
}
