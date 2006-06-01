%state NORMAL COMMENT

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/bas-filt.l,v 1.14 2006/05/28 00:36:07 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Basic
 * input text.
 *
 * Notes:
 *	Some dialects of basic allow '_' in IDENT, some do not.
 *	Visual Basic uses UUID's and HEX constants.
 *
 *	This parser is too simple to distinguish between variable names
 *	and functions which happen to be spelled the same (since it is
 *	not doing a lookahead for a left parenthesis).  It also is not
 *	distinguishing between class member names (foo.bar) and keywords
 *	without the "." preceding.  The latter is doable.
 */

#include <filters.h>

DefineFilter("basic");

static char *Action_attr;
static char *Comment_attr;
static char *Number_attr;
static char *String_attr;
static char *LineNo_attr;

static int first_token;
static int handle_attrs;
static int within_attr;

static int is_hex(char *s);

%}

TYPES		[%&!#$]?
HEX		[[:xdigit:]]+
UUID		"{"{HEX}(-{HEX})*"}"
INTEGER		[-+]?([[:digit:]]+)
REAL		[-+]?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?
IDENT		[[:alpha:]][[:alnum:]_]*{TYPES}

DATE		"#"[ [:alnum:]\,:./]+"#"
STRING		\"([^"]|(""))*\"

NUMBER		{INTEGER}|{REAL}|{UUID}|({INTEGER}{HEX}|("&"[Hh]|\$){HEX}){TYPES}

%%

<NORMAL>^{INTEGER}		{ WriteToken(LineNo_attr); }
<NORMAL>{IDENT}			{ char *attr = is_hex(yytext)
						? Number_attr
						: ci_keyword_attr(yytext);
				  if (attr == 0) attr = "";
				  if (!strcmp(attr, Comment_attr)) {
				    BEGIN(COMMENT);
				  } else if (first_token
					     && !strcmp(yytext, "VERSION")) {
				      handle_attrs = 1;
				  } else if (handle_attrs) {
				      if (!strcmp(yytext, "Begin"))
					  ++within_attr;
				      else if (!strcmp(yytext, "End"))
					  ++within_attr;
				      else if (!strcmp(yytext, "Attribute"))
					  within_attr = handle_attrs = 0;
				      else if (within_attr)
					  attr = "";
				  }
				  WriteToken(attr);
				  first_token = 0;
				}
<NORMAL>"_"$			{ WriteToken(Action_attr); }
<NORMAL>{DATE}			|
<NORMAL>{STRING}		{ WriteToken(String_attr); }
<NORMAL>{NUMBER}		{ WriteToken(Number_attr); }
<NORMAL>"'"[^\r\n]*		{ WriteToken(Comment_attr); }

<COMMENT>[^\r\n]*		{ WriteToken(Comment_attr); }
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
    (void) before;
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

    first_token = 1;
    handle_attrs = 0;
    within_attr = 0;
    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
}
