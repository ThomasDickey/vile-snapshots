%s NORMAL RESNAME RESVALUE

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/xresfilt.l,v 1.12 2000/11/04 20:23:45 tom Exp $
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
    return (isalpha(CharOf(*name)) && isupper(CharOf(*name)));
}

/*
 * Buffer text for quoted-strings
 */
static void
quoted_name(char *text, int length)
{
    char *attr = keyword_attr(lowercase_of(text));
    if (attr != 0)
	flt_bfr_embed(text, length, attr);
    else
	flt_bfr_append(text, length);
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
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
    flt_bfr_finish();
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
<RESNAME>:		{ BEGIN(RESVALUE); ECHO; flt_bfr_begin(String_attr); }

<RESVALUE>{NUMBER}	{ flt_bfr_embed(yytext, yyleng, Number_attr); }
<RESVALUE>{KEYWORD}	{ quoted_name(yytext, yyleng); }
<RESVALUE>![^\\\n]*	{ flt_bfr_embed(yytext, yyleng, Comment_attr); }
<RESVALUE>\\\n		{ flt_bfr_append(yytext, yyleng); }
<RESVALUE>({ESCAPED}|.)	{ flt_bfr_append(yytext, yyleng); }
<RESVALUE>\n		{ flt_bfr_finish(); ECHO; BEGIN(NORMAL); }

<NORMAL>{COMMENT}	{ WriteToken(Comment_attr); }
