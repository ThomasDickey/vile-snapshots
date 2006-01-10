%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/nr-filt.l,v 1.18 2006/01/09 19:04:36 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of NROFF
 */

#include <filters.h>

DefineFilter("nr");

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
	if (isspace(CharOf(*s))) {
	    if (cmd == size)
		cmd = (s - text) + 1;
	}
    }
    flt_puts(text, cmd, Keyword_attr);
    flt_puts(text+cmd, size-cmd, String_attr);
}

static void
write_font(char *text, int size)
{
    flt_puts(text, 3, Ident2_attr);
    flt_puts(text + 3, size-6, String_attr);
    flt_puts(text + size - 3, 3, Ident2_attr);
}

%}

COMMAND		^(\.[[:blank:]]*[[:alnum:]]+([ \t][^\n]+)?|'\\\"[^\n]+)
COMMENT		^\.[[:blank:]]*\\\"

ENIT		\\[e *\n\\+-]

FONT		\\f[[:upper:]1-4]

%%

{COMMAND}			{ write_cmd(yytext, yyleng); }

{FONT}([^\\\n]|{ENIT})+(\\f[^PR1]|\n)	{ WriteToken(Action_attr); /* err? */ }
{FONT}([^\\\n]|{ENIT})+\\f[PR1]	{ write_font(yytext, yyleng); }
{ENIT}				{ WriteToken(Ident2_attr); }
\\[s][+-]?[[:digit:]]+		{ WriteToken(Ident2_attr); }

{COMMENT}[^\n]*			{ WriteToken(Comment_attr); }

%%

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    String_attr  = class_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}
