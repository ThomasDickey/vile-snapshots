%pointer

%{

/*
 * $Id: def-filt.l,v 1.16 2013/12/02 01:32:54 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of M$ linker-definition
 * input text.
 */

#include <filters.h>

DefineFilter(def);

static char *Comment_attr;
static char *Number_attr;
static char *String_attr;

%}

INTEGER		([[:digit:]]+)|(0x[[:xdigit:]]+)
IDENT		[!$]?[[:alpha:]][[:alnum:]_]*[%&!#$]?

SSTRING		\'([^']|(''))*\'
DSTRING		\"([^"]|(""))*\"
STRING		{SSTRING}|{DSTRING}

NUMBER		{INTEGER}

%%

{IDENT}			{ WriteToken(get_keyword_attr(yytext)); }
{STRING}		{ WriteToken(String_attr); }
{NUMBER}		{ WriteToken(Number_attr); }
";"[^\r\n]*		{ WriteToken(Comment_attr); }

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
