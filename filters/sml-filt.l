%pointer
%x COMMENT STRING STRSKIP

%{

/*
 * $Id: sml-filt.l,v 1.15 2013/12/02 01:32:53 tom Exp $

    Filter to add vile "attribution" sequences to selected bits of SML
    input text.

    The main complication is handling multi-line strings.  We need to
    handle the following special case correctly

	"The quick brown \
	\fox jumps over the lazy dog.\
	\"

*/

#include <filters.h>

DefineFilter(sml);

static char *Keyword_attr;
static char *Comment_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

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

DECIMAL		[~]?([[:digit:]]+)
HEX		[~]?0[xX]([[:xdigit:]]+)
WORD		0[wW]([[:digit:]]+)
WORDHEX		0[wW][xX]([[:xdigit:]]+)
REAL		[~]?([[:digit:]]*\.[[:digit:]]+)([eE][~]?[[:digit:]]+)?
IDENT		[[:alpha:]][[:alnum:]_']*

NUMBER		{DECIMAL}|{HEX}|{WORD}|{WORDHEX}|{REAL}

%%

{IDENT}			{ WriteToken(get_keyword_attr(yytext)); }
"_"			{ WriteToken(get_keyword_attr(yytext)); }

{NUMBER}		{ WriteToken(Number_attr); }

"(*"			{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<COMMENT>[^*]*		{ WriteToken(Comment_attr); }
<COMMENT>"*"+[^*)]*	{ WriteToken(Comment_attr); }
<COMMENT>"*"+")"	{ WriteToken(Comment_attr); BEGIN(0); }

\"			{ start_string(); ADD_STRING; BEGIN(STRING); }
<STRING>\\\n		{ ADD_STRING; BEGIN(STRSKIP); }
<STRING>\\\"		{ ADD_STRING; }
<STRING>\\\\		{ ADD_STRING; }
<STRING>\"		{ ADD_STRING; flush_string(); BEGIN(0); }
<STRING>\n		{ ADD_STRING; }
<STRING>.		{ ADD_STRING; /* cope with erroneous input */}

<STRSKIP>[ \t]+		{ ADD_STRING; }
<STRSKIP>.		{ ADD_STRING; BEGIN(STRING); }
<STRSKIP>\n		{ ADD_STRING; BEGIN(STRING); /* cope with erroneous input */}

"="	|
"=>"	|
"\|"	|
"::"	|
":="	|
":"	|
"*"	|
"("	|
")"	|
"{"	|
"}"	|
"["	|
"]"	|
"->"			{ WriteToken(Keyword_attr); }

%%

static void
flush_string(void)
{
    if (strpos > 0) {
	flt_puts(strbuf, strpos, String_attr);
    }
    strpos = 0;
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
	if (strpos >= (STRBUF - 1)) {
	    flush_string();
	}
	strbuf[strpos++] = *(str++);
    }
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

    strpos = 0;

    Keyword_attr = class_attr(NAME_KEYWORD);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    BEGIN(INITIAL);
    RunLEX();

    flush_string();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
