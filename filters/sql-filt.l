%s CODE ACTION SERVER COMMENT COMMENT1

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sql-filt.l,v 1.22 2005/02/07 20:29:31 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of SQL script
 * - T.Dickey
 */

#include <filters.h>

DefineOptFilter("sql", "d");

#define DPRINTF(params) if(FltOptions('d'))printf params
#define set_state(name) DPRINTF(("{" #name "}")); BEGIN(name)

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
				set_state(ACTION);
				leading = 0;
			    } else {
				set_state(SERVER);
			    }
			}
<ACTION>\n		{ ECHO; set_state(CODE); leading = 1; }
<ACTION>[^\n]*		{ WriteToken(String_attr); }

<SERVER>{IDENT}		{ WriteToken(""); }
<SERVER>"."		{ WriteToken(""); }
<SERVER>";"		{ ECHO; set_state(CODE); leading = 1; }
<SERVER>.		{ ECHO; set_state(CODE); }

<CODE>{IDENT}		{   char *text = lowercase_of(yytext);
			    char *attr = ci_keyword_attr(text);
			    DPRINTF(("{%s->%s}", text, NONNULL(attr)));
			    if (attr == Comment_attr) {
				if (leading) {
				    WriteToken(attr);
				} else {
				    flt_error("illegal position for comment");
				    WriteToken(Error_attr);
				}
				set_state(COMMENT1);
			    } else if (leading && attr == 0) {
				if (set_symbol_table("sqlplus")) {
				    if ((attr = ci_keyword_attr(text)) != 0) {
					set_state(ACTION);
					/* FIXME:  a few commands have keywords,
					 * such as "set".
					 */
				    }
				    WriteToken(attr);
				    set_symbol_table(filter_def.filter_name);
				    leading = 0;
				}
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

<CODE>--[^\n]*		{ WriteToken(Comment_attr); set_state(COMMENT1); }
<COMMENT1>[^\n]*	{ WriteToken(Comment_attr); }
<COMMENT1>\n		{ ECHO; set_state(CODE); }

<CODE>"/*"		{ WriteToken(Comment_attr); set_state(COMMENT); }
<COMMENT>[^*]*		{ WriteToken(Comment_attr); }
<COMMENT>"*"+[^*/]*	{ WriteToken(Comment_attr); }
<COMMENT>"*"+"/"	{ WriteToken(Comment_attr); set_state(CODE); }

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

    set_state(CODE);
    leading = 1;
    while (yylex() > 0) {
    }
}
