%s CODE ACTION SERVER COMMENT COMMENT1

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sql-filt.l,v 1.21 2004/12/10 00:41:30 tom Exp $
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
<CODE>^{SPACE}		{ ECHO; }

<CODE>{LABEL}		|
<CODE>{ACTION}		{ WriteToken(Action_attr);
			    if (leading) {
				BEGIN(ACTION);
				leading = 0;
			    } else {
				BEGIN(SERVER);
			    }
			}
<ACTION>\n		{ ECHO; BEGIN(CODE); leading = 1; }
<ACTION>[^\n]*		{ WriteToken(String_attr); }

<SERVER>{IDENT}		{ WriteToken(""); }
<SERVER>"."		{ WriteToken(""); }
<SERVER>";"		{ ECHO; BEGIN(CODE); leading = 1; }
<SERVER>.		{ ECHO; BEGIN(CODE); }

<CODE>{IDENT}		{   char *text = lowercase_of(yytext);
			    char *attr = ci_keyword_attr(text);
			    if (attr == Comment_attr) {
				if (leading) {
				    WriteToken(attr);
				} else {
				    flt_error("illegal position for comment");
				    WriteToken(Error_attr);
				}
				BEGIN(COMMENT1);
			    } else if (leading
				    && attr == 0
				    && set_symbol_table("sqlplus")
				    && (attr = ci_keyword_attr(text)) != 0) {
				set_symbol_table(filter_def.filter_name);
				WriteToken(attr);
				BEGIN(ACTION);
				/* FIXME: a few commands have keywords, such
				 * as "set".
				 */
				leading = 0;
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

<CODE>";"		{ ECHO; leading = 1; }

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
