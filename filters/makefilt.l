%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/makefilt.l,v 1.17 1999/03/07 19:19:55 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Makefile
 *
 * This was based on a filter written by Pierre Dittgen (dittgen@univ-mlv.fr)
 * - T.Dickey
 */

#include <filters.h>

char *filter_name = "make";

static char *Comment_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *String_attr;

static void
write_keyword(char *text, int size)
{
    int colon = size > 1 && text[size-1] == ':';
    int space = 0;

    if (colon)
	text[--size] = 0;

    while (isspace(*text)) {
	text++;
	size--;
	space++;
    }
    if (space)
	fprintf(yyout, "%*s", space, " ");

    space = 0;
    while (isspace(text[size-1])) {
	text[--size] = 0;
	space++;
    }

    write_token(yyout, text, size, keyword_attr(text));

    if (space)
	fprintf(yyout, "%*s", space, " ");

    if (colon)
	fputc(':', yyout);
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
    String_attr  = class_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}

%}

SPACE		[ \t]

SSTRING		\'((\\.)|[^\'])*\'
DSTRING		\"((\\\.)|[^\"])*\"
STRINGS		({SSTRING}|{DSTRING})

NAME		[a-zA-Z_][a-zA-Z_0-9.-]+

%%

^{SPACE}*(.)?include{SPACE}*	{ write_keyword(yytext, yyleng); }	

^{SPACE}*[\.!]{NAME}{SPACE}*:	{ write_keyword(yytext, yyleng); }

^{SPACE}*{NAME}{SPACE}*:	{ WriteToken(Keyword_attr); }

^{SPACE}*{NAME}{SPACE}*=	{ WriteToken(Ident2_attr); }

\$[A-Za-z@!%^&*?<]		|
\${SPACE}*\([^\)]+\)		{ WriteToken(Ident2_attr); }

{STRINGS}			{ WriteString(String_attr); }

"#".*$				{ WriteToken(Comment_attr); }
