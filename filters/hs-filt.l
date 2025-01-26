%pointer
%x NORMAL COMMENT PREPRO PREPMORE PREPNOTE

%{

/*
 * $Id: hs-filt.l,v 1.30 2025/01/26 11:05:40 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Haskell
 * input text.
 *
 * http://cs.anu.edu.au/Student/comp1100/haskell/tourofsyntax.html
 * http://www.haskell.org/onlinereport/syntax-iso.html
 */

#include <filters.h>
#include <fltstack.h>

DefineFilter(hs);

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

%}

BLANK		[[:blank:]]

OCTAL		0[oO][0-7]+
HEXADECIMAL	0[xX][[:xdigit:]]+
INTEGER		[-+]?([[:digit:]]+)
REAL		[-+]?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?
IDENT		[[:alpha:]_][[:alnum:]_]*(\')?

ASCII		("^"([[:upper:]^@_])|NUL|SOH|STX|ETX|EOT|ENQ|ACK|BEL|BS|HT|LF|VT|FF|CR|SO|SI|DLE|DC1|DC2|DC3|DC4|NAK|SYN|ETB|CAN|EM|SUB|ESC|FS|GS|RS|US|SP|DEL)
CHARESC		[abfnrtv\\"'&]
OCT_ESC		(o[0-7]+)
DEC_ESC		([[:digit:]]+)
HEX_ESC		(x[[:xdigit:]]+)
ESCAPED		\\({DEC_ESC}|{OCT_ESC}|{HEX_ESC}|{CHARESC}|{ASCII})

QUOTECHAR	\'({ESCAPED}|[^'\\])\'
STRING		\"({ESCAPED}|[^"\\])*\"

NUMBER		{OCTAL}|{HEXADECIMAL}|{INTEGER}|{REAL}

%%

<NORMAL>"{"[-]+		{ PushQuote(COMMENT, Comment_attr); }
<COMMENT>[-]+"}"	{ PopQuote(); }
<COMMENT>[\r\n]+	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>[^-\r\n]+	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>[-]+$		{ flt_bfr_append(yytext, yyleng); }
<COMMENT>[-]+[^}-]	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>.		{ flt_bfr_append(yytext, yyleng); }

<NORMAL>"`"{IDENT}"`"	{ WriteToken(Action_attr); }
<NORMAL>{IDENT}		{ WriteToken(get_keyword_attr(yytext)); }
<NORMAL>{QUOTECHAR}	|
<NORMAL>{STRING}	{ WriteToken(String_attr); }
<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }
<NORMAL>"--"[^\r\n]*	{ WriteToken(Comment_attr); }

<NORMAL>^{BLANK}*[#]+	{ PushQuote(PREPRO, Preproc_attr); }
<NORMAL>.		{ ECHO; }

<PREPRO>{BLANK}+	{ flt_bfr_append(yytext, yyleng); }
<PREPRO>{IDENT}		{ const char *attr;
			  set_symbol_table("cpre");
			  attr = get_keyword_attr(yytext);
			  if (attr == NULL)
			      attr = Error_attr;
			  flt_bfr_embed(yytext, yyleng, attr);
			  set_symbol_table(default_table);
			  new_state(PREPMORE);
			 }
<PREPRO>.		{ flt_bfr_append(yytext, yyleng); }
<PREPMORE>[\r\n]	{
			  flt_bfr_append(yytext, yyleng);
			  flt_bfr_finish();
			  pop_state();
			}
<PREPMORE>\\.		{ flt_bfr_embed(yytext, yyleng, String_attr); }
<PREPMORE>{IDENT}	{ flt_bfr_embed(yytext, yyleng, get_keyword_attr(yytext)); }
<PREPMORE>[^\\\r\n]	{ flt_bfr_embed(yytext, yyleng, ""); }
<PREPMORE>{STRING}	{ flt_bfr_embed(yytext, yyleng, String_attr); }

<PREPMORE>"/*"		{ PushQuote(PREPNOTE, Comment_attr); }
<PREPNOTE>[^*]*		{ flt_bfr_append(yytext, yyleng); }
<PREPNOTE>"*"+[^*/]*	{ flt_bfr_append(yytext, yyleng); }
<PREPNOTE>"*"+"/"	{ PopQuote(); flt_bfr_begin(""); }

%%

#include <fltstack.h>

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
}

static void
do_filter(FILE *inputs)
{
    InitLEX(inputs);

    Action_attr = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident2_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    begin_state(NORMAL);
    RunLEX();
    flt_bfr_error();
    end_state();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
