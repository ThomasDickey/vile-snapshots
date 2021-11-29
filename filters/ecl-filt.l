%pointer
%s NORMAL COMMENT QUERY

%{

/*
 * $Id: ecl-filt.l,v 1.30 2013/12/02 01:32:53 tom Exp $
 *
 * Filter to add vile "attribution" sequences to Prolog/ECLiPSe code
 *
 * Joachim Schimpf (jschimpf@infotech.monash.edu.au)
 */

#include <filters.h>
#include <fltstack.h>

DefineFilter(ecl);

static char *Comment_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

%}

LINECOMMENT	%.*\n
BRCOMMENT	"/*"[^*]*[*]+([^*/][^*]*[*]*)*"/"
BRCOMMENT1	"/*"[/]?(([^*][/]+)|[^/])*"*/"

KEYWORD		[[:lower:]][[:alnum:]_]*
IDENT		[[:upper:]][[:alnum:]_]*

STARTQUERY	^[:?]"-"/[(\[[:lower:][:blank:]\n]

ESC_HEX		\\x[[:xdigit:]]*(\\)?
ESC_MISC	\\[^x]|\\"^".

ANY_CHAR	.|{ESC_HEX}|{ESC_MISC}

SSTRING		\'({ESC_HEX}|{ESC_MISC}|\\\n|[^'\\])*\'
DSTRING		\"({ESC_HEX}|{ESC_MISC}|\\\n|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

BIN_INTEGER	("2"\'|"0b")[0-1]+
DEC_INTEGER	[[:digit:]]+
OCT_INTEGER	("8"\'|"0o")[0-7]+
HEX_INTEGER	("16"\'|"0x")[[:xdigit:]]+

INTEGER		{BIN_INTEGER}|{OCT_INTEGER}|{HEX_INTEGER}|{DEC_INTEGER}
RATIONAL	({DEC_INTEGER}_{DEC_INTEGER}|"0r"{DEC_INTEGER}"/"{DEC_INTEGER})
REAL		(([[:digit:]]*\.)?[[:digit:]]+)(([eE][+-]?[[:digit:]]+)|Inf)?
NUMBER		[-]?({INTEGER}|{RATIONAL}|{REAL})


%%

<NORMAL>{KEYWORD}	{ WriteToken(get_keyword_attr(yytext)); }
<NORMAL>{IDENT}		{ WriteToken(""); }

<NORMAL>{LINECOMMENT}	{ WriteToken(Comment_attr); }
<NORMAL>{BRCOMMENT}	{ WriteToken(Comment_attr); }

<NORMAL>{STARTQUERY}	{ PushQuote(QUERY, Preproc_attr); }
<QUERY>\./[ \t\n]	{ PopQuote(); }
<QUERY>0\'{ANY_CHAR}	{ flt_bfr_embed(yytext, yyleng, Number_attr); }
<QUERY>".."		{ flt_bfr_append(yytext, yyleng); }
<QUERY>{NUMBER}		{ flt_bfr_embed(yytext, yyleng, Number_attr); }
<QUERY>{STRINGS}	{ flt_bfr_embed(yytext, yyleng, String_attr); }
<QUERY>{LINECOMMENT}	{ flt_bfr_embed(yytext, yyleng, Comment_attr); }
<QUERY>{BRCOMMENT}	{ flt_bfr_embed(yytext, yyleng, Comment_attr); }
<QUERY>.		{ flt_bfr_append(yytext, yyleng); }
<QUERY>[\r\n]		{ flt_bfr_append(yytext, yyleng); }

<NORMAL>0\'{ANY_CHAR}	{ WriteToken(Number_attr); }
<NORMAL>".."		{ ECHO; }
<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }
<NORMAL>{STRINGS}	{ WriteToken(String_attr); }
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

    Comment_attr = class_attr(NAME_COMMENT);
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
