%state NORMAL PREPRO COMMENT

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/rc-filt.l,v 1.1 2001/03/21 01:29:34 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Basic 
 * input text.
 *
 * Notes:
 *	Some dialects of basic allow '_' in IDENT, some do not.
 */

#include <filters.h>

DefineFilter("rc");

static char *Comment_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static void
show_prepro(char *text)
{
    char *s = skip_blanks(text);

    flt_puts(text, s - text, "");
    flt_puts(s, 1, Preproc_attr);
    s = skip_blanks(text = ++s);
    flt_puts(text, s - text, "");
    set_symbol_table("cpre");
    flt_puts(text, strlen(text), keyword_attr(text));
    set_symbol_table(filter_def.filter_name);
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
    Number_attr  = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}
%}

INTEGER		[-+]?([0-9]+)
IDENT		[a-zA-Z][a-zA-Z_0-9]*

STRING		\"([^"]|(""))*\"

DECIMAL		[0-9_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[0-9a-fA-F_]+
NUMBER		({DECIMAL}|{OCTAL}|{HEXADECIMAL})[L]?

%%

^[ \t]*#[ \t]*{IDENT}	{ show_prepro(yytext); }
{IDENT}			{ WriteToken(keyword_attr(yytext)); }
{STRING}		{ WriteToken(String_attr); }
{NUMBER}		{ WriteToken(Number_attr); }

"//"[^\n]*		{ WriteToken(Comment_attr); }
