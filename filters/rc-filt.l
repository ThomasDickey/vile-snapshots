%pointer
%state NORMAL PREPRO COMMENT

%{

/*
 * $Id: rc-filt.l,v 1.18 2016/12/16 01:01:39 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Windows
 * resource files.
 */

#include <filters.h>

DefineFilter(rc);

static char *Comment_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static void
show_prepro(char *text)
{
    char *s = skip_blanks(text);

    flt_puts(text, (int) (s - text), "");
    flt_puts(s, 1, Preproc_attr);
    s = skip_blanks(text = ++s);
    flt_puts(text, (int) (s - text - 1), "");
    set_symbol_table("cpre");
    flt_puts(text, (int) strlen(text), get_keyword_attr(text));
    set_symbol_table(default_table);
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
{IDENT}			{ WriteToken(get_keyword_attr(yytext)); }
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
    InitLEX(inputs);

    Comment_attr = class_attr(NAME_COMMENT);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
