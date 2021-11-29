%pointer

%{

/*
 * $Id: ada-filt.l,v 1.22 2013/12/02 01:32:54 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Ada
 * input text.
 */

#include <filters.h>

DefineFilter(ada);

static char *Comment_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

%}

INTEGER		[-+]?([[:digit:]]+)
REAL		[-+]?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?
IDENT		[[:alpha:]_][[:alnum:]_]*

QUOTECHAR	\'([^']|(''))\'
STRING		\"([^"]|(""))*\"

NUMBER		{INTEGER}|{REAL}

ATTRIBUTE	\'{IDENT}

%%

{IDENT}			{ WriteToken(get_keyword_attr(yytext)); }
"<<"{IDENT}">>"		{ WriteToken(Ident2_attr); }
{QUOTECHAR}		|
{STRING}		{ WriteToken(String_attr); }
{NUMBER}		{ WriteToken(Number_attr); }
"--"[^\r\n]*		{ WriteToken(Comment_attr); }
{ATTRIBUTE}		{
			  set_symbol_table("ada_attribute");
			  WriteToken(get_keyword_attr(yytext + 1));
			  set_symbol_table(default_table);
			}

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
    Ident2_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
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
