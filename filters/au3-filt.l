%state NORMAL IFCMT COMMENT

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/au3-filt.l,v 1.4 2005/09/04 21:10:12 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of AutoIt
 * input text.
 *
 * todo: implement #comment-start (#cs) and #comment-end (#ce).
 * todo: parse Send-Keys within strings, including repeat-count.
 */

#include <filters.h>

DefineFilter("au3");

static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *Prepro_attr;
static char *String_attr;
static int if0_level;

%}

HEX		0[Xx][[:xdigit:]]+
INTEGER		[-+]?([[:digit:]]+)
REAL		[-+]?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?

IDENT		([$@])?[[:alpha:]_][[:alnum:]_]*

SSTRING		\'([^'\n]|'')*\'
DSTRING		\"([^"\n]|"")*\"
STRING		{SSTRING}|{DSTRING}

NUMBER		{HEX}|{INTEGER}|{REAL}

%%

<NORMAL,IFCMT>^#[^\n]*$		{ char *text = lowercase_of(yytext);
				  WriteToken(Prepro_attr);
				  if (!strncmp(text, "#comments-start", 15)
				   || !strncmp(text, "#cs", 3)) {
				    ++if0_level;
				  }
				  else if (!strncmp(text, "#comments-end", 13)
				   || !strncmp(text, "#ce", 3)) {
				    --if0_level;
				  }
				  if (if0_level > 0)
				      BEGIN(IFCMT);
				  else {
				    if0_level = 0;
				    BEGIN(NORMAL);
				  }
				}
<IFCMT>^[^\n]*$			{ WriteToken(Comment_attr); }
<IFCMT>.			{ ECHO; }

<NORMAL>;			{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<NORMAL>{IDENT}			{
				  char *attr = ci_keyword_attr(yytext);
				  if (attr == 0) {
				      if (yytext[0] == '$') {
					  attr = Ident2_attr;
				      } else if (yytext[0] == '@') {
					  attr = Error_attr;
					  flt_error("unexpected '@'");
				      }
				  }
				  WriteToken(attr);
				}
<NORMAL>{STRING}		{ WriteToken(String_attr); }
<NORMAL>{NUMBER}		{ WriteToken(Number_attr); }
<NORMAL>"'"[^\n]*		{ WriteToken(Comment_attr); }

<COMMENT>[^\n]*			{ WriteToken(Comment_attr); }
<COMMENT>[\n]			{ ECHO; BEGIN(NORMAL); }

%%

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    Prepro_attr  = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    if0_level = 0;
    while (yylex() > 0) {
    }
}
