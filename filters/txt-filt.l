%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/txt-filt.l,v 1.1 1999/04/17 15:02:07 tom Exp $
 *
 * Filter to add vile "attribution" sequences to an ordinary text file
 */

#include <filters.h>

char *filter_name = "txt";

static char *Keyword_attr;
static char *Comment_attr;

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

    Keyword_attr = class_attr(NAME_KEYWORD);
    Comment_attr = class_attr(NAME_COMMENT);

    while (yylex() > 0) {
    }
}

%}

COMMENT		^-[-]+[ \t]+[^\n]+\n

KEYWORD		[a-zA-Z_][a-zA-Z0-9_.]*

%%

{KEYWORD}	{ WriteToken(mixedcase(yytext) ? "" : Keyword_attr); }

{COMMENT}	{ WriteToken(Comment_attr); }
