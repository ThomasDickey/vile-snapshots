%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/makefilt.l,v 1.10 1998/12/25 21:33:29 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Makefile
 *
 * This was based on a filter written by Pierre Dittgen (dittgen@univ-mlv.fr)
 * - T.Dickey
 */

#include <filters.h>

char *filter_name = "make";

static char *Comment_attr;
static char *Keyword_attr;
static char *Ident_attr;

void
init_filter(int before GCC_UNUSED)
{
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;
    Comment_attr = keyword_attr(NAME_COMMENT);
    Keyword_attr = keyword_attr(NAME_KEYWORD);
    Ident_attr   = keyword_attr(NAME_IDENT2);
    while (yylex() > 0) {
    }
}

%}

%%

^[ \t]*[a-zA-Z_][a-zA-Z_0-9.]*[ \t]*: { WriteToken(Keyword_attr); }

^[ \t]*[a-zA-Z_][a-zA-Z_0-9]*[ \t]*= { WriteToken(Ident_attr); }

\$[A-Za-z@!%^&*?]	|
\$[ \t]*\([^\)]+\) { WriteToken(Ident_attr); }

"#".*$		{ WriteToken(Comment_attr); }
