%pointer
%state NORMAL PREPRO COMMENT

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/rc-filt.l,v 1.10 2008/01/12 17:21:27 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Windows
 * resource files.
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

%}

INTEGER		[-+]?([[:digit:]]+)
IDENT		[[:alpha:]][[:alnum:]_]*

STRING		\"([^"]|(""))*\"

DECIMAL		[[:digit:]_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[[:xdigit:]_]+
NUMBER		({DECIMAL}|{OCTAL}|{HEXADECIMAL})[L]?

%%

^[ \t]*#[ \t]*{IDENT}	{ show_prepro(yytext); }
{IDENT}			{ WriteToken(keyword_attr(yytext)); }
{STRING}		{ WriteToken(String_attr); }
{NUMBER}		{ WriteToken(Number_attr); }

"//"[^\r\n]*		{ WriteToken(Comment_attr); }

%%

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Comment_attr = class_attr(NAME_COMMENT);
    Number_attr  = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
