%s NORMAL QUOTED QUOTES

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/txt-filt.l,v 1.13 2000/08/09 23:40:36 tom Exp $
 *
 * Filter to add vile "attribution" sequences to an ordinary text file
 */

#include <filters.h>

DefineFilter("txt");

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
	flt_puts(save_bfr, save_len, String_attr);
	save_len = 0;
    }
}

static int
mixedcase(const char *name)
{
    if (!strcmp(name, "A") || !strcmp(name, "I"))
	return 1;
    while (*name) {
	int ch = (*name++ & 0xff);
	if (isalpha(ch) && islower(ch))
	    return 1;
    }
    return 0;
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
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
    flush_quoted();
}

%}

COMMENT		^-[-]+[ \t]+[^\n]+\n

KEYWORD		([a-zA-Z_][a-zA-Z0-9_.]*|[A-Z][+/&][A-Z])

SIGN		[-+]
DECIMAL		[0-9_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[0-9a-fA-F_]+
REAL		[-+]?([0-9_]*\.[0-9][0-9_]*)([eE][+-]?[0-9_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

NAME		[a-zA-Z0-9_.#-]+
ADDRESS		({NAME}|\.)+
EMAIL		{NAME}@{ADDRESS}

URL		[A-Za-z]+"://"[0-9a-zA-Z%/.~_#-]+

%%

<NORMAL>[A-Z]\'{KEYWORD} { ECHO; }

<NORMAL>{KEYWORD}	{ WriteToken(mixedcase(yytext) ? "" : Keyword_attr); }

<NORMAL>{COMMENT}	{ WriteToken(Comment_attr); }

<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }

<NORMAL,QUOTED>{EMAIL}	{ flush_quoted(); WriteToken(Ident2_attr); }
<NORMAL,QUOTED>{URL}	{ flush_quoted(); WriteToken(Ident2_attr); }

<NORMAL>\(\"\)		|
<NORMAL>\'.\'		{ ECHO; }

<NORMAL>\"		{ BEGIN(QUOTED); add_to_quoted(yytext, yyleng); }
<QUOTED>\"		{ add_to_quoted(yytext, yyleng); flush_quoted(); BEGIN(NORMAL); }
<QUOTED>.		{ add_to_quoted(yytext, yyleng); }
<QUOTED>^[\n]		{ flush_quoted(); ECHO; BEGIN(NORMAL); }
<QUOTED>[\n]		{ flush_quoted(); ECHO; }

<NORMAL>\`\`		{ BEGIN(QUOTES); add_to_quoted(yytext, yyleng); }
<QUOTES>\'\'		{ add_to_quoted(yytext, yyleng); flush_quoted(); BEGIN(NORMAL); }
<QUOTES>.		{ add_to_quoted(yytext, yyleng); }
<QUOTES>^[\n]		{ flush_quoted(); ECHO; BEGIN(NORMAL); }
<QUOTES>[\n]		{ flush_quoted(); ECHO; }

<NORMAL>.		{ ECHO; }
