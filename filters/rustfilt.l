%pointer
%s NORMAL

%{

/*
 * $Id: rustfilt.l,v 1.5 2018/03/27 00:59:34 tom Exp $
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
ESC_STRING	([\\][0nrt'"\\]|([\\]x[[:xdigit:]]([[:xdigit:]])?))
SSTRING		[']({ESC_STRING}|[^'\\])*[']
DSTRING		["]({ESC_STRING}|[^"\r\n]|[\\][\r\n])*["]
RAW_STRING	["]({ESC_STRING}|[^\r\n]|[\\][\r\n])*["]

DIGITS		[[:digit:]_]
INTEGER		{DIGITS}+([ui](8|16|32|64|128))?
FLOATING	(({DIGITS}+"."{DIGITS}*)|({DIGITS}?"."|{DIGITS}+))([e]([+-])?{DIGITS}+)?([f](32|64))?

%%

^"#"[^\r\n]*	{ WriteToken(Preproc_attr); }
"//"[^\r\n]*	{ WriteToken(Comment_attr); }

"b"{SSTRING}	|
"b"{DSTRING}	|
"br"{DSTRING}	|
"br#"{RAW_STRING}"#"	|
"br##"{RAW_STRING}"##"	|
"br###"{RAW_STRING}"###"	|
"br####"{RAW_STRING}"####"	|
{DSTRING}	{ WriteToken(String_attr); }

{INTEGER}	|
{FLOATING}	{ WriteToken(Number_attr); }

{IDENT}		{ WriteToken(keyword_attr(yytext)); }
.		{ ECHO; }

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
