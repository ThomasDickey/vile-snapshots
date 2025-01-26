%pointer
%state NORMAL IFCMT COMMENT

%{

/*
 * $Id: au3-filt.l,v 1.16 2025/01/26 11:05:40 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of AutoIt
 * input text.
 *
 * todo: implement #comment-start (#cs) and #comment-end (#ce).
 * todo: parse Send-Keys within strings, including repeat-count.
 */

#include <filters.h>

DefineFilter(au3);

static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;
static int if0_level;

%}

HEX		0[Xx][[:xdigit:]]+
INTEGER		[-+]?([[:digit:]]+)
REAL		[-+]?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?

IDENT		([$@])?[[:alpha:]_][[:alnum:]_]*

SSTRING		\'([^'\r\n]|'')*\'
DSTRING		\"([^"\r\n]|"")*\"
STRING		{SSTRING}|{DSTRING}

NUMBER		{HEX}|{INTEGER}|{REAL}

%%

<NORMAL,IFCMT>^#[^\r\n]*$	{ const char *text = lowercase_of(yytext);
				  WriteToken(Preproc_attr);
				  if (!strncmp(text, "#comments-start", (size_t) 15)
				   || !strncmp(text, "#cs", (size_t) 3)) {
				    ++if0_level;
				  }
				  else if (!strncmp(text, "#comments-end", (size_t) 13)
				   || !strncmp(text, "#ce", (size_t) 3)) {
				    --if0_level;
				  }
				  if (if0_level > 0)
				      BEGIN(IFCMT);
				  else {
				    if0_level = 0;
				    BEGIN(NORMAL);
				  }
				}
<IFCMT>^[^\r\n]*$		{ WriteToken(Comment_attr); }
<IFCMT>.			{ ECHO; }

<NORMAL>;			{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<NORMAL>{IDENT}			{
				  const char *attr = get_keyword_attr(yytext);
				  if (attr == NULL) {
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
<NORMAL>"'"[^\r\n]*		{ WriteToken(Comment_attr); }

<COMMENT>[^\r\n]*		{ WriteToken(Comment_attr); }
<COMMENT>[\n]			{ ECHO; BEGIN(NORMAL); }

%%

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
}

static void
do_filter(FILE *inputs)
{
    InitLEX(inputs);

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident2_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    if0_level = 0;
    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
