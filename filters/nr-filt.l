%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/nr-filt.l,v 1.6 1999/02/12 11:25:45 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of NROFF
 */

#include <filters.h>

char *filter_name = "nroff";

static char *Action_attr;
static char *Comment_attr;
static char *Keyword_attr;
static char *Ident2_attr;
static char *String_attr;

static void
write_cmd(char *text, int size)
{
    int cmd = size;
    char *s;
    for (s = text; *s; s++) {
	if (isspace(*s)) {
	    if (cmd == size)
		cmd = (s - text) + 1;
	}
    }
    write_token(yyout, text, cmd, Keyword_attr);
    write_string(yyout, text+cmd, size-cmd, String_attr);
}

static void
write_font(char *text, int size)
{
    write_token(yyout, text, 3, Ident2_attr);
    write_string(yyout, text+3, size-6, String_attr);
    write_token(yyout, text + size - 3, 3, Ident2_attr);
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

    Action_attr  = keyword_attr(NAME_ACTION);
    Comment_attr = keyword_attr(NAME_COMMENT);
    Ident2_attr  = keyword_attr(NAME_IDENT2);
    Keyword_attr = keyword_attr(NAME_KEYWORD);
    String_attr  = keyword_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}

%}

COMMAND		^(\.[a-zA-Z0-9]+([ \t][^\n]+)?|'\\\"[^\n]+)
COMMENT		^\.\\\"

ENIT		\\[e *\n\\-]

FONT		\\f[A-Z1-3]

%%

{COMMAND}			{ write_cmd(yytext, yyleng); }

{FONT}([^\\\n]|{ENIT})+(\\f[^PR]|\n)	{ WriteToken(Action_attr); /* err? */ }
{FONT}([^\\\n]|{ENIT})+\\f[PR]	{ write_font(yytext, yyleng); }
{ENIT}				{ WriteToken(Ident2_attr); }
\\[s][+-]?[0-9]+		{ WriteToken(Ident2_attr); }

{COMMENT}[^\n]*			{ WriteToken(Comment_attr); }
