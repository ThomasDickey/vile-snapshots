%s COMMENT COMMENT1

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sql-filt.l,v 1.11 2001/09/18 19:54:51 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of SQL script
 * - T.Dickey
 */

#include <filters.h>

DefineFilter("sql");

static char *Action_attr;
static char *Comment_attr;
static char *Ident_attr;
static char *Keyword_attr;
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
    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr   = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(INITIAL);
    while (yylex() > 0) {
    }
}

%}

SPACE		[ \t]*

ACTION		@

COMMENT1	^{SPACE}--[^\n]*

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

{IDENT}			{   if (!strcmp(lowercase_of(yytext), "rem")
			     || !strcmp(yytext, "remark")) {
				WriteToken(Comment_attr);
				BEGIN(COMMENT1);
			    } else {
				WriteToken(ci_keyword_attr(yytext));
			    }
			}

{HOSTVAR}		{ flt_puts(yytext, 1, Action_attr);
			  WriteToken2(Ident_attr,1);
			}

{NUMBER}		{ WriteToken(Number_attr); }

{STRING}		{ WriteToken(String_attr); }

^{SPACE}--[^\n]*	{ WriteToken(Comment_attr); BEGIN(COMMENT1); }
<COMMENT1>[^\n]*	{ WriteToken(Comment_attr); }
<COMMENT1>\n		{ ECHO; BEGIN(INITIAL); }

"/*"			{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<COMMENT>[^*]*		{ WriteToken(Comment_attr); }
<COMMENT>"*"+[^*/]*	{ WriteToken(Comment_attr); }
<COMMENT>"*"+"/"	{ WriteToken(Comment_attr); BEGIN(INITIAL); }
