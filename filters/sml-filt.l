%x COMMENT STRING STRSKIP

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sml-filt.l,v 1.2 2000/08/09 23:49:32 tom Exp $

    Filter to add vile "attribution" sequences to selected bits of SML
    input text.

    The main complication is handling multi-line strings.  We need to
    handle the following special case correctly

	"The quick brown \
	\fox jumps over the lazy dog.\
	\"

*/

#include <filters.h>

DefineFilter("sml");

static char *Keyword_attr;
static char *Comment_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

/*  We accumulate strings into this buffer.
*/
#define	STRBUF	1024
static char	strbuf[STRBUF];
static int	strpos;

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

#define ADD_STRING add_string(yytext, yyleng)

static void
add_string(char* str, int len)
{
    while (len-- > 0) {
	if (strpos >= STRBUF) {
	    flush_string();
	}
	strbuf[strpos++] = *(str++);
    }
}

void
init_filter(int before GCC_UNUSED)
{
}

void
do_filter(FILE *input)
{
    yyin = input;

    strpos = 0;

    Keyword_attr = class_attr(NAME_KEYWORD);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(INITIAL);
    while (yylex() > 0) {
    }

    flush_string();
}

%}

DECIMAL		[~]?([0-9]+)
HEX		[~]?0[xX]([0-9A-Fa-f]+)
WORD		0[wW]([0-9]+)
WORDHEX		0[wW][xX]([0-9A-Fa-f]+)
REAL		[~]?([0-9]*\.[0-9]+)([eE][~]?[0-9]+)?
IDENT		[a-zA-Z][a-zA-Z_0-9']*

NUMBER		{DECIMAL}|{HEX}|{WORD}|{WORDHEX}|{REAL}

%%

{IDENT}			{ WriteToken(keyword_attr(yytext)); }
"_"			{ WriteToken(keyword_attr(yytext)); }

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
