%s NORMAL QUOTE1 QUOTE2 QUOTE3

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/py-filt.l,v 1.5 2000/01/31 00:21:19 tom Exp $
 *
 * Filter to add vile "attribution" sequences to a Python script
 */

#include <filters.h>

char *filter_name = "py";

static char *Comment_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

/*
 * Buffer text for quoted-strings
 */
static char    *save_bfr;
static unsigned save_max;
static unsigned save_len;

static void
add_to_quoted(char *text, int length)
{
    save_bfr = do_alloc(save_bfr, save_len + length + 1, &save_max);
    strncpy(save_bfr + save_len, text, length + 1);
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
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
}

%}

COMMENT		#.*\n

KEYWORD		[a-zA-Z_][a-zA-Z0-9_]*

SIGN		[-+]
DECIMAL		[0-9_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[0-9a-fA-F_]+
REAL		[-+]?([0-9_]*\.[0-9][0-9_]*)([eE][+-]?[0-9_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL})[L]?|{REAL}

%%

<NORMAL>{KEYWORD}	{ WriteToken(keyword_attr(yytext)); }

<NORMAL>{COMMENT}	{ WriteToken(Comment_attr); }

<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }

<NORMAL>\'		{ BEGIN(QUOTE1); add_to_quoted(yytext, yyleng); }
<QUOTE1>\'		{ add_to_quoted(yytext, yyleng); flush_quoted(); BEGIN(NORMAL); }
<QUOTE1>(\\.|.)		{ add_to_quoted(yytext, yyleng); }
<QUOTE1>^[\n]		{ flush_quoted(); ECHO; BEGIN(NORMAL); }
<QUOTE1>[\n]		{ flush_quoted(); ECHO; }

<NORMAL>\"		{ BEGIN(QUOTE2); add_to_quoted(yytext, yyleng); }
<QUOTE2>\"		{ add_to_quoted(yytext, yyleng); flush_quoted(); BEGIN(NORMAL); }
<QUOTE2>(\\.|.)		{ add_to_quoted(yytext, yyleng); }
<QUOTE2>^[\n]		{ flush_quoted(); ECHO; BEGIN(NORMAL); }
<QUOTE2>[\n]		{ flush_quoted(); ECHO; }

<NORMAL>\"\"\"		{ BEGIN(QUOTE3); add_to_quoted(yytext, yyleng); }
<QUOTE3>\"\"\"		{ add_to_quoted(yytext, yyleng); flush_quoted(); BEGIN(NORMAL); }
<QUOTE3>(\\.|.)		{ add_to_quoted(yytext, yyleng); }
<QUOTE3>[\n]		{ flush_quoted(); ECHO; }

<NORMAL>.		{ ECHO; }
