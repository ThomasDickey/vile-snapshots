%x NORMAL COMMENT QUOTE1 QUOTE2

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/makefilt.l,v 1.31 2000/08/09 23:52:36 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Makefile
 *
 * This was based on a filter written by Pierre Dittgen (dittgen@univ-mlv.fr)
 * - T.Dickey
 */

#include <filters.h>

DefineFilter("make");

static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *String_attr;

static void
write_keyword(char *text, int size)
{
    int colon = size > 1 && text[size-1] == ':';
    int space = 0;
    char *base = text;

    if (colon)
	text[--size] = 0;

    while (isspace(*text)) {
	text++;
	size--;
	space++;
    }
    if (space)
	fprintf(yyout, "%.*s", space, base);

    space = 0;
    while (isspace(text[size-1])) {
	size--;
	space++;
    }

    flt_puts(text, size, keyword_attr(text));

    if (space)
	fprintf(yyout, "%.*s", space, text + size);

    if (colon)
	flt_putc(':');
}

static void
write_delimited(char *text, int size, char *attr)
{
    flt_puts(text, size-1, attr);
    flt_puts(text+size-2, 1, "");
}

/*
 * Buffer text for quoted strings, since most characters are attributed
 * as string, which would be a lot of overhead to mark it up.
 */
static char    *save_bfr;
static unsigned save_max;
static unsigned save_len;

static void
add_to_quoted(char *text, int length)
{
    save_bfr = do_alloc(save_bfr, save_len + length, &save_max);
    strncpy(save_bfr + save_len, text, length);
    save_len += length;
}

static void
flush_quoted(void)
{
    if (save_len) {
	flt_puts(save_bfr, save_len, String_attr);
	save_len = 0;
    }
}

static void
missing_quote(char *text, int length)
{
    char *save = String_attr;
    String_attr = Error_attr;
    add_to_quoted(text, length);
    flush_quoted();
    BEGIN(NORMAL);
    String_attr = save;
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
    Error_attr   = class_attr(NAME_ERROR);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
    flush_quoted();
}

%}

SPACE		[ \t]

ID		[a-zA-Z_][a-zA-Z_0-9]+
NAME		[a-zA-Z_][a-zA-Z_0-9.-]+

MACRO		([$]?\$\{{ID}\}|[$]?\${ID}|[$]?\$[A-Za-z@!%^&*?<]|\${SPACE}*\([^\)]+\))

%%

<NORMAL>^{SPACE}*(.)?include{SPACE}*	{ write_keyword(yytext, yyleng); }	

<NORMAL>^{SPACE}*[\.!]{NAME}{SPACE}*:	{ write_keyword(yytext, yyleng); }

<NORMAL>^({SPACE}*{NAME}{SPACE}*|\\\n)+: { write_delimited(yytext, yyleng, Keyword_attr); }

<NORMAL>^{SPACE}*{NAME}{SPACE}*=	{ write_delimited(yytext, yyleng, Ident2_attr); }

<NORMAL>{MACRO}		{ WriteToken(Ident2_attr); }

<NORMAL>^{SPACE}\@	{ if (yyleng)
			    flt_puts(yytext, yyleng-1, "");
			  flt_puts("@", 1, String_attr); }
<NORMAL>\\{SPACE}+\n	{ WriteToken(Error_attr); }
<NORMAL>\\["'\n]	{ WriteToken(String_attr); }
<NORMAL>\'		{ add_to_quoted(yytext, yyleng); BEGIN(QUOTE1); }
<NORMAL>\"		{ add_to_quoted(yytext, yyleng); BEGIN(QUOTE2); }

<NORMAL>"#"			{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<COMMENT>(\\.|\\\n|[^\n])*	{ WriteToken(Comment_attr); }
<COMMENT>\n			{ ECHO; BEGIN(NORMAL); }

<QUOTE1,QUOTE2>{MACRO}		{ flush_quoted(); WriteToken(Ident2_attr); }
<QUOTE1,QUOTE2>\\{SPACE}+\n	{ flush_quoted(); WriteToken(Error_attr); }
<QUOTE1,QUOTE2>\\\n		{ add_to_quoted(yytext, yyleng); }

<QUOTE1>(\\.|[^\n\'])	{ add_to_quoted(yytext, yyleng); }
<QUOTE1>\'		{ add_to_quoted(yytext, yyleng);
			  flush_quoted();
			  BEGIN(NORMAL);
			}

<QUOTE2>(\\.|[^\n\"])	{ add_to_quoted(yytext, yyleng); }
<QUOTE2>\"		{ add_to_quoted(yytext, yyleng);
			  flush_quoted();
			  BEGIN(NORMAL);
			}

<QUOTE1,QUOTE2>[\n]	{ missing_quote(yytext, yyleng); }
