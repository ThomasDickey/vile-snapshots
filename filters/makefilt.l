%x NORMAL QUOTE1 QUOTE2

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/makefilt.l,v 1.24 1999/11/06 02:02:21 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Makefile
 *
 * This was based on a filter written by Pierre Dittgen (dittgen@univ-mlv.fr)
 * - T.Dickey
 */

#include <filters.h>

char *filter_name = "make";

static char *Comment_attr;
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

    write_token(yyout, text, size, keyword_attr(text));

    if (space)
	fprintf(yyout, "%.*s", space, text + size);

    if (colon)
	fputc(':', yyout);
}

static void
write_delimited(char *text, int size, char *attr)
{
    write_string(yyout, text, size-1, attr);
    write_string(yyout, text+size-2, 1, "");
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
	write_token(yyout, save_bfr, save_len, String_attr);
	save_len = 0;
    }
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

    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
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
			    fprintf(yyout, "%.*s", yyleng-1, yytext);
			  write_string(yyout, "@", 1, String_attr); }
<NORMAL>\\["'\n]	{ WriteString(String_attr); }
<NORMAL>\'		{ add_to_quoted(yytext, yyleng); BEGIN(QUOTE1); }
<NORMAL>\"		{ add_to_quoted(yytext, yyleng); BEGIN(QUOTE2); }

<NORMAL>"#".*$		{ WriteToken(Comment_attr); }

<QUOTE1>{MACRO}		{ flush_quoted(); WriteToken(Ident2_attr); }
<QUOTE1>(\\.|[^\n\'])	{ add_to_quoted(yytext, yyleng); }
<QUOTE1>\'		{ add_to_quoted(yytext, yyleng);
			  flush_quoted();
			  BEGIN(NORMAL);
			}

<QUOTE2>{MACRO}		{ flush_quoted(); WriteToken(Ident2_attr); }
<QUOTE2>(\\.|[^\n\"])	{ add_to_quoted(yytext, yyleng); }
<QUOTE2>\"		{ add_to_quoted(yytext, yyleng);
			  flush_quoted();
			  BEGIN(NORMAL);
			}
