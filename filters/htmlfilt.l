%x COMMENT CODE

%{
/*
 * Filter to add vile "attribution" sequences to selected bits of HTML input
 * text.
 *
 * written by Pierre Dittgen (dittgen@univ-mlv.fr)
 */

#include <filters.h>

char *filter_name = "html";

static char *Comment_attr;
static char *Keyword_attr;
static char *Ident_attr;
static char *String_attr;

/*
 * We've parsed a string that may begin with a slash, and then consists of a
 * case-independent name.
 */
static int
is_html_keyword(char *text)
{
    return is_ci_keyword((text[0] == '/') ? (text+1) : text);
}

void
init_filter(void)
{
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;
    Comment_attr = keyword_attr(NAME_COMMENT);
    Keyword_attr = keyword_attr(NAME_KEYWORD);
    Ident_attr   = keyword_attr(NAME_IDENT);
    String_attr  = keyword_attr(NAME_LITERAL);
    while (yylex() > 0) {
    }
    exit(0);
}

%}

INTEGER		[-+]?([0-9]+)
REAL		[-+]?([0-9]*\.[0-9]+)([eE][+-]?[0-9]+)?
IDENT		[a-zA-Z_][a-zA-Z_0-9]*
NOSTRBRK	"\\\""
STRING		\"([^"]|{NOSTRBRK})*\"

%%

"<"		{ printf("%c1%s:<", CTL_A, Keyword_attr); BEGIN(CODE); }
<CODE>">"	{ printf("%c1%s:>", CTL_A, Keyword_attr); BEGIN(0);    }

<CODE>(\/)?{IDENT}	{ if (is_html_keyword(yytext)) WriteToken(Keyword_attr); else WriteToken(Ident_attr); }

"<!--"			{ WriteToken(Comment_attr);
			  BEGIN(COMMENT);           }
<COMMENT>[^-]+		{ WriteString(Comment_attr); }
<COMMENT>"-"+[^->]*	{ WriteString(Comment_attr); }
<COMMENT>"-->"		{ WriteToken(Comment_attr);
			  BEGIN(0);                 }

{IDENT}		{ printf("%s", yytext); }
<CODE>{STRING}	{ WriteString(String_attr); }
