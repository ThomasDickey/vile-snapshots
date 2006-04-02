%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/cfg-filt.l,v 1.12 2006/04/02 16:49:05 tom Exp $
 *
 * Filter to add vile "attribution" sequences to an ordinary Lynx config-file
 */

#include <filters.h>

DefineFilter("cfg");

static char *Comment_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static int
mixedcase(const char *name)
{
    if (!strcmp(name, "A") || !strcmp(name, "I"))
	return 1;
    while (*name) {
	int ch = (*name++ & 0xff);
	if (isalpha(ch) && islower(ch))
	    return 1;
    }
    return 0;
}

%}

COMMENT		^[ \t]*#[^\n]*\n

KEYWORD		([[:alpha:]_][[:alnum:]_.]*|[[:upper:]][+/&][[:upper:]])

SIGN		[-+]
DECIMAL		[[:digit:]_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[[:xdigit:]_]+
REAL		[-+]?([[:digit:]_]*\.[[:digit:]][[:digit:]_]*)([eE][+-]?[[:digit:]_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

NAME		[[:alnum:]_.-]+
ADDRESS		({NAME}|\.)+
EMAIL		{NAME}@{ADDRESS}

URL		[[:alpha:]]+"://"[[:alnum:]#%&+\,.\/?;@_=~-]+

%%

^\.[^\n]*$	{ WriteToken(String_attr); }

{KEYWORD}	{ WriteToken(mixedcase(yytext) ? "" : Keyword_attr); }

{COMMENT}	{ WriteToken(Comment_attr); }

{NUMBER}	{ WriteToken(Number_attr); }

{EMAIL}		{ WriteToken(Ident2_attr); }
{URL}		{ WriteToken(Ident2_attr); }

.		{ ECHO; }

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
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}
