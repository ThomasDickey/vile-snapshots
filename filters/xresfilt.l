%s NORMAL RESNAME RESVALUE

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/xresfilt.l,v 1.7 2000/02/10 11:30:12 tom Exp $
 *
 * Filter to add vile "attribution" sequences to an X resource file.
 */

#include <filters.h>

DefineFilter("xres");

static char *Comment_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static int
isClass(char *name)
{
    return (isalpha(*name) && isupper(*name));
}

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

static void
quoted_name(char *text, int length)
{
    char *attr = keyword_attr(lowercase_of(text));
    if (attr != 0) {
	flush_quoted();
	write_token(yyout, text, length, attr);
    } else {
	add_to_quoted(text, length);
    }
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;

    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
}

%}

COMMENT		![^\n]*\n

SIGN		[-+]
DECIMAL		[0-9]+
OCTAL		0[0-7]+
HEX		[0-9a-fA-Z]
HEXADECIMAL	0x{HEX}+
REAL		[-+]?([0-9]*\.[0-9]+)([eE][+-]?[0-9]+)?
RGB		[Rr][Gg][Bb]:{HEX}+"/"{HEX}+"/"{HEX}+
COLOR		(#{HEX}+|{RGB})
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL}|{COLOR})

ESCAPED		(\\[0-7][0-7][0-7]|\\.)

NAME		[a-zA-Z0-9_-]+
KEYWORD		({NAME}|<{NAME}>)

%%

<NORMAL>[?*.]		{ BEGIN(RESNAME); ECHO; }
<NORMAL,RESNAME>{NAME}	{ WriteToken(isClass(yytext) ? Keyword_attr : Ident2_attr); }
<RESNAME>:		{ BEGIN(RESVALUE); ECHO; }

<RESVALUE>{NUMBER}	{ flush_quoted(); WriteToken(Number_attr); }
<RESVALUE>{KEYWORD}	{ quoted_name(yytext, yyleng); }
<RESVALUE>![^\\\n]*	{ flush_quoted(); WriteToken(Comment_attr); }
<RESVALUE>\\\n		{ add_to_quoted(yytext, yyleng); }
<RESVALUE>({ESCAPED}|.)	{ add_to_quoted(yytext, yyleng); }
<RESVALUE>\n		{ flush_quoted(); ECHO; BEGIN(NORMAL); }

<NORMAL>{COMMENT}	{ WriteToken(Comment_attr); }
