%state NORMAL COMMENT

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/bas-filt.l,v 1.8 2004/08/27 16:40:22 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Basic
 * input text.
 *
 * Notes:
 *	Some dialects of basic allow '_' in IDENT, some do not.
 *	Visual Basic uses UUID's and HEX constants.
 */

#include <filters.h>

DefineFilter("basic");

static char *Action_attr;
static char *Comment_attr;
static char *Number_attr;
static char *String_attr;
static char *LineNo_attr;

static int is_hex(char *s);

%}

TYPES		[%&!#$]?
HEX		[A-Fa-f0-9]+
UUID		"{"{HEX}(-{HEX})*"}"
INTEGER		[-+]?([0-9]+)
REAL		[-+]?([0-9]*\.[0-9]+)([eE][+-]?[0-9]+)?
IDENT		[a-zA-Z][a-zA-Z_0-9]*{TYPES}

DATE		"#"[ 0-9A-Za-z,:./]+"#"
STRING		\"([^"]|(""))*\"

NUMBER		{INTEGER}|{REAL}|{UUID}|({INTEGER}{HEX}|("&[Hh]"|\$){HEX}){TYPES}

%%

<NORMAL>^{INTEGER}		{ WriteToken(LineNo_attr); }
<NORMAL>{IDENT}			{ char *attr = is_hex(yytext)
						? Number_attr
						: ci_keyword_attr(yytext);
				  if (attr == 0) attr = "";
				  WriteToken(attr);
				  if (!strcmp(attr, Comment_attr)) {
				    BEGIN(COMMENT);
				  }
				}
<NORMAL>"_"$			{ WriteToken(Action_attr); }
<NORMAL>{DATE}			|
<NORMAL>{STRING}		{ WriteToken(String_attr); }
<NORMAL>{NUMBER}		{ WriteToken(Number_attr); }
<NORMAL>"'"[^\n]*		{ WriteToken(Comment_attr); }

<COMMENT>[^\n]*			{ WriteToken(Comment_attr); }
<COMMENT>[\n]			{ ECHO; BEGIN(NORMAL); }

%%

static int
is_hex(char *s)
{
    int number = 0;
    int alpha = 0;
    while (*s != 0) {
	if (isxdigit(CharOf(*s))) {
	    if (isalpha(CharOf(*s)))
	    	++alpha;
	    else
	    	++number;
	} else {
	    number = 0;
	    break;
	}
	s++;
    }
    return number && alpha;
}

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
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);
    LineNo_attr  = class_attr("LineNumber");

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
}
