%x STRING

%{

/*
    $Id: erl-filt.l,v 1.5 2012/02/18 13:43:42 tom Exp $

    Filter to add vile "attribution" sequences to selected bits of Erlang
    input text.

    We do:
	atoms that are keywords:    Keyword
	all other atoms:	    none
	strings:		    String
	<<>>, braces:		    Braces
	Vars:			    Ident2
	numbers:		    Number
	comments:		    Comment

*/

#include <filters.h>

DefineFilter("erl");

static char *Keyword_attr;
static char *Comment_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;
static char *Tuple_attr;

/*  We accumulate strings into this buffer.
*/
#define	STRBUF	1024
static char strbuf[STRBUF];
static int strpos;

#define ADD_STRING add_string(yytext, yyleng)

static void add_string(char *str, int len);
static void flush_string(void);
static void start_string(void);

%}

DECIMAL		-?([0-9]+#)?([0-9]+)
REAL		-?([0-9]*\.[0-9]+)([eE][~]?[0-9]+)?
SATOM		[a-z][a-zA-Z_0-9@]*
QATOM		'[^']*'
VAR		([A-Z][a-zA-Z_0-9@]*|[A-Z][a-zA-Z_0-9@]+)

ATOM		{SATOM}|{QATOM}
NUMBER		{DECIMAL}|{REAL}

%%

{VAR}			{ WriteToken(Ident2_attr); }
{ATOM}			{ WriteToken(get_keyword_attr(yytext)); }
"_"			{ WriteToken(get_keyword_attr(yytext)); }
"$".			{ WriteToken(get_keyword_attr(yytext)); }

{NUMBER}		{ WriteToken(Number_attr); }

\%.*			{ WriteToken(Comment_attr); }

\"			{ start_string(); ADD_STRING; BEGIN(STRING); }
<STRING>\\\"		{ ADD_STRING; }
<STRING>\\\\		{ ADD_STRING; }
<STRING>\"		{ ADD_STRING; flush_string(); BEGIN(0); }
<STRING>.		{ ADD_STRING; /* cope with erroneous input */}

"=:="	|
"=/="	|
"=="	|
"/="	|
"=<"	|
">="	|
"++"	|
"--"	|
"->"	|
"+"	|
"-"	|
"*"	|
"/"	|
":"	|
"#"	|
"="	|
"!"	|
"|"	|
"("	|
")"	|
"["	|
"]"			{ WriteToken(Keyword_attr); }

"<<"	|
">>"	|
"{"	|
"}"			{ WriteToken(Tuple_attr); }

%%

static void
flush_string(void)
{
    if (strpos > 0) {
	flt_puts(strbuf, strpos, String_attr);
	strpos = 0;
    }
}

static void
start_string(void)
{
    strpos = 0;
}

static void
add_string(char *str, int len)
{
    while (len-- > 0) {
	if (strpos >= STRBUF) {
	    flush_string();
	}
	strbuf[strpos++] = *(str++);
    }
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    strpos = 0;

    Keyword_attr = class_attr(NAME_KEYWORD);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);
    Tuple_attr = class_attr("Braces");

    BEGIN(INITIAL);
    while (yylex() > 0) {
    }

    flush_string();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
