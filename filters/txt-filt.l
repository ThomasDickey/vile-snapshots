%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/txt-filt.l,v 1.2 1999/06/07 10:12:27 tom Exp $
 *
 * Filter to add vile "attribution" sequences to an ordinary text file
 */

#include <filters.h>

char *filter_name = "txt";

static char *Comment_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;

static int
mixedcase(const char *name)
{
    if (!strcmp(name, "A") || !strcmp(name, "I"))
	return 1;
    while (*name) {
	int ch = *name++;
	if (isalpha(ch) && islower(ch))
	    return 1;
    }
    return 0;
}

void
init_filter(int before GCC_UNUSED)
{
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;

    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);

    while (yylex() > 0) {
    }
}

%}

COMMENT		^-[-]+[ \t]+[^\n]+\n

KEYWORD		([a-zA-Z_][a-zA-Z0-9_.]*|[A-Z][+/&][A-Z])

SIGN		[-+]
DECIMAL		[0-9_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[0-9a-fA-F_]+
REAL		[-+]?([0-9_]*\.[0-9][0-9_]+)([eE][+-]?[0-9_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

NAME		[a-zA-Z0-9_.#-]+
ADDRESS		({NAME}|\.)+
EMAIL		{NAME}@{ADDRESS}

URL		"http://"[^ ,\n]+

%%

{KEYWORD}	{ WriteToken(mixedcase(yytext) ? "" : Keyword_attr); }

{COMMENT}	{ WriteToken(Comment_attr); }

{NUMBER}	{ WriteToken(Number_attr); }

{EMAIL}		{ WriteToken(Ident2_attr); }
{URL}		{ WriteToken(Comment_attr); }
