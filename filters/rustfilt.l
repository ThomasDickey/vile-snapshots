%pointer
%s NORMAL CNOTE PREPRO ATTRS

%{

/*
 * $Id: rustfilt.l,v 1.11 2018/10/30 00:19:13 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of RUST program.
 *
 * Notes:
 * The byte-string literals such as
 *	"br###"{DSTRING}"###"
 * have to be done explicitly in the lexer for each length.
 */

#include <filters.h>

DefineFilter(rust);

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

#include <fltstack.h>

%}

IDENT		[[:alpha:]_][[:alnum:]_]*
MACRO		{IDENT}!
ESC_STRING	([\\][0nrt'"\\]|([\\]x[[:xdigit:]]([[:xdigit:]])?))
SSTRING		[']({ESC_STRING}|[^'\\])*[']
DSTRING		["]({ESC_STRING}|[^"\r\n]|[\\][\r\n])*["]
RAW_STRING	["]({ESC_STRING}|[^\r\n]|[\\][\r\n])*["]

DIGITS		[[:digit:]_]
INTEGER		{DIGITS}+([ui](8|16|32|64|128))?
FLOATING	(({DIGITS}+"."{DIGITS}*)|({DIGITS}?"."|{DIGITS}+))([e]([+-])?{DIGITS}+)?([f](32|64))?

%%

<NORMAL>"#"("!")?"["	{ PushQuote(PREPRO, Preproc_attr); }
<NORMAL>^"["{IDENT}"]"	{ WriteToken(Preproc_attr); /* section */ }
<NORMAL>"//"[^\r\n]*	{ WriteToken(Comment_attr); }

<PREPRO>{DSTRING}	{ flt_bfr_embed(yytext, yyleng, String_attr); }
<PREPRO>"]"		{ PopQuote(); }
<PREPRO>"("		{ flt_bfr_finish();
			  PushQuote(ATTRS, "");
			}
<PREPRO>[ \t\r\n]	{ flt_bfr_embed(yytext, yyleng, ""); }
<PREPRO>.		{ flt_bfr_append(yytext, yyleng); }

<ATTRS>{DSTRING}	{ flt_bfr_embed(yytext, yyleng, String_attr); }
<ATTRS>")"		{ PopQuote();
			  flt_bfr_begin(Preproc_attr);
			}
<ATTRS>.		{ flt_bfr_append(yytext, yyleng); }

<NORMAL>"/*"		{ PushQuote(CNOTE, Comment_attr); }
<CNOTE>"*/"		{ PopQuote(); }
<CNOTE>[\r\n]		{ flt_bfr_append(yytext, yyleng); }
<CNOTE>.		{ flt_bfr_append(yytext, yyleng); }

<NORMAL>"b"{SSTRING}	|
<NORMAL>"b"{DSTRING}	|
<NORMAL>"br"{DSTRING}	|
<NORMAL>"br#"{RAW_STRING}"#"	|
<NORMAL>"br##"{RAW_STRING}"##"	|
<NORMAL>"br###"{RAW_STRING}"###"	|
<NORMAL>"br####"{RAW_STRING}"####"	|
<NORMAL>{DSTRING}	{ WriteToken(String_attr); }

<NORMAL>{INTEGER}	|
<NORMAL>{FLOATING}	{ WriteToken(Number_attr); }

<NORMAL>{MACRO}		{ WriteToken(Ident2_attr); }
<NORMAL>{IDENT}		{ WriteToken(keyword_attr(yytext)); }
<NORMAL>.		{ ECHO; }

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
    Ident_attr = class_attr(NAME_IDENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
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
