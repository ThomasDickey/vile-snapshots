%s RULES ACTIONS CODE COMMENT

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/vl-filt.l,v 1.3 2002/05/14 00:06:22 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of VeriLog
 * program.
 */

#include <filters.h>

DefineFilter("vlog");

static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static void
do_define(char *string)
{
    char *keydef = "`define";
    int lendef = 7;

    flt_puts(string, lendef, keyword_attr(keydef));
    for (string += lendef; isspace(CharOf(*string)); string++)
	flt_putc(*string);
    string--;
    *string = *keydef;
    insert_keyword(string++, Ident2_attr, 0);
    flt_puts(string, strlen(string), Ident2_attr);
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
    yyin = input;
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    Ident_attr   = class_attr(NAME_IDENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(CODE);
    while (yylex() > 0) {
    }
}

%}

SPACE		[ \t]

DIRECTIVE	`[a-zA-Z_][a-zA-Z_0-9]*
IDENT		[a-zA-Z_][a-zA-Z_0-9]*
ESCNAME		\\[^ \n]+
SYSNAME		\$[a-zA-Z_][a-zA-Z_0-9]*

DECIMAL		('[d])?([0-9_]+)
BINARY		'b([0-1_]+)
OCTAL		'o([0-7_]+)
HEXADECIMAL	'h([0-9a-f_]+)
INTEGER		{DECIMAL}|{HEXADECIMAL}|{OCTAL}|{BINARY}

DSTRING		\"(\\.|[^"\\])*\"

%%

<CODE>{ESCNAME}		{ WriteToken(keyword_attr(yytext)); }
<CODE>{SYSNAME}		{ WriteToken(keyword_attr(yytext) ? keyword_attr(yytext) : Error_attr); }
<CODE>{IDENT}		{ WriteToken(keyword_attr(yytext)); }

<CODE>`define{SPACE}+{IDENT} { do_define(yytext); }
<CODE>{DIRECTIVE}	{ WriteToken(keyword_attr(yytext) ? keyword_attr(yytext) : Error_attr); }

<CODE>{INTEGER}		{ WriteToken(Number_attr); }

<CODE>"//"[^\n]*	{ WriteToken(Comment_attr); }
<CODE>"/*"		{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<COMMENT>[^*]*		{ WriteToken(Comment_attr); }
<COMMENT>"*"+[^*/]*	{ WriteToken(Comment_attr); }
<COMMENT>"*"+"/"	{ WriteToken(Comment_attr); BEGIN(CODE); }

{DSTRING}		{ WriteToken(String_attr); }
