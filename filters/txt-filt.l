%pointer
%s sTEXT NOTE QUOTED QUOTES

%a 10000
%o 10000

%{

/*
 * $Id: txt-filt.l,v 1.43 2025/01/26 11:38:02 tom Exp $
 *
 * Filter to add vile "attribution" sequences to an ordinary text file
 */

#include <filters.h>
#include <fltstack.h>

DefineFilter(txt);

static char *Comment_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static int mixedcase(const char *name);

%}

DASHES		(-([[:blank:]]-)+)

KEYWORD		([[:alpha:]_][[:alnum:]_.]*|[[:upper:]][+/&][[:upper:]])

SIGN		[-+]
DECIMAL		[[:digit:]_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[[:xdigit:]_]+
REAL		([[:digit:]_]*\.[[:digit:]][[:digit:]_]*)([eE]{SIGN}?[[:digit:]_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

IDENT		"$"{KEYWORD}:[^$\r\n]+"$"

NAME		[[:alnum:]_.#-]+
ADDRESS		({NAME}|\.)+
EMAIL		{NAME}@{ADDRESS}

URL		[[:alpha:]]+"://"[[:alnum:]#%&+\,.\/?;@_=~-]+

%%

<sTEXT>[[:upper:]]\'{KEYWORD} { ECHO; }

<sTEXT>^[[:blank:]]*"--"[[:blank:]]	{ PushQuote(NOTE, Comment_attr); }
<sTEXT>^[-]+$		|
<sTEXT>{DASHES}		{ WriteToken(Comment_attr); }
<sTEXT>{KEYWORD}	{ const char *attr = get_keyword_attr(yytext);
			  if (attr == NULL) attr = (mixedcase(yytext) ? "" : Keyword_attr);
			  WriteToken(attr);
			}
<sTEXT>{NUMBER}		{ WriteToken(Number_attr); }
<sTEXT>{EMAIL}		{ WriteToken(Ident2_attr); }
<sTEXT>{URL}		{ WriteToken(Ident2_attr); }
<sTEXT>{IDENT}		{ WriteToken(Ident2_attr); }

<QUOTES,QUOTED>{EMAIL}	{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }
<QUOTES,QUOTED>{URL}	{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }
<QUOTES,QUOTED>{IDENT}	{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }

<sTEXT>\(\"\)		|
<sTEXT>\'.\'		{ ECHO; }

<sTEXT,NOTE>\"		{ PushQuote(QUOTED, String_attr); }
<QUOTED>\"		{ PopQuote(); }
<QUOTED>.		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>^[\n]		{ PopQuote(); }
<QUOTED>[\n]		{ flt_bfr_append(yytext, yyleng); }

<sTEXT,NOTE>\`\`	{ PushQuote(QUOTES, String_attr); }
<QUOTES>\'\'		{ PopQuote(); }
<QUOTES>.		{ flt_bfr_append(yytext, yyleng); }
<QUOTES>^[\n]		{ PopQuote(); }
<QUOTES>[\n]		{ flt_bfr_append(yytext, yyleng); }

<sTEXT>[\r\n]		{ ECHO; }
<sTEXT>.		{ ECHO; }

<NOTE>[\r\n]		{ PopQuote(); }
<NOTE>{KEYWORD}		{ flt_bfr_embed(yytext, yyleng, mixedcase(yytext) ? Comment_attr : Keyword_attr); }
<NOTE>{NUMBER}		{ flt_bfr_embed(yytext, yyleng, Number_attr); }
<NOTE>{EMAIL}		{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }
<NOTE>{URL}		{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }
<NOTE>{IDENT}		{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }
<NOTE>\(\"\)		|
<NOTE>\'.\'		|
<NOTE>.			{ flt_bfr_embed(yytext, yyleng, Comment_attr); }

%%

#include <fltstack.h>

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
    (void) before;
}

static void
do_filter(FILE *inputs)
{
    InitLEX(inputs);

    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    begin_state(sTEXT);
    RunLEX();
    flt_bfr_finish();
    end_state();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
