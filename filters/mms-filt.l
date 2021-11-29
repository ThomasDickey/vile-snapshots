%pointer
%a 5000
%o 6000

%{

/*
 * $Id: mms-filt.l,v 1.21 2013/12/02 01:32:53 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of VMS mms script
 * - T.Dickey
 */

#include <filters.h>

DefineFilter(mms);

static char *Comment_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *String_attr;

static void
write_keyword(char *text, int size)
{
    int colon = size > 1 && text[size - 1] == ':';
    int space = 0;

    if (colon)
	text[--size] = 0;

    while (isspace(CharOf(*text))) {
	text++;
	size--;
	space++;
    }
    while (space-- > 0)
	flt_putc(' ');

    space = 0;
    while (isspace(CharOf(text[size - 1]))) {
	text[--size] = 0;
	space++;
    }

    flt_puts(text, size, get_keyword_attr(text));

    while (space-- > 0)
	flt_putc(' ');

    if (colon)
	flt_putc(':');
}

%}

SPACE		[ \t]

SSTRING		\'\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

IDENT		[$[:alpha:]_][$[:alnum:]_]*
PARAM		'{IDENT}

INLINECOMMENT	![^\r\n]*
COMMENT		"#"[^\r\n]*

%%

^{SPACE}*(.)?include{SPACE}*	{ write_keyword(yytext, yyleng); }

^{SPACE}*[\.!]{IDENT}{SPACE}*:	{ write_keyword(yytext, yyleng); }

^{SPACE}*{IDENT}{SPACE}*:	{ WriteToken(Keyword_attr); }

^{SPACE}*{IDENT}{SPACE}*=	{ WriteToken(Ident2_attr); }

\$[@!%^&*?<]			|
\${SPACE}*\([^\)]+\)		{ WriteToken(Ident2_attr); }

{PARAM}				{ WriteToken(Ident2_attr); }

{STRINGS}			{ WriteToken(String_attr); }

{INLINECOMMENT}			|
{COMMENT}			{ WriteToken(Comment_attr); }

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
    Keyword_attr = class_attr(NAME_KEYWORD);
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
