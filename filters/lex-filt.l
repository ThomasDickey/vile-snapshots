%s RULES RULE1 RULEX ACTIONS ACTION1 ACTION2 ACTION3 CODE COMMENT

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/lex-filt.l,v 1.5 1998/12/22 03:03:17 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of LEX program.
 */

#include <filters.h>

char *filter_name = "lex";

static char *Comment_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static int section = 0;
static int nesting = 0;

static void
set_state(void)
{
    if (section >= 2) {
	BEGIN(CODE);
    } else if (section >= 1) {
	BEGIN(ACTIONS);
    } else {
	BEGIN(RULES);
    }
}

static void
set_rules(void)
{
    if (section >= 1)
	set_state();
}

void
init_filter(void)
{
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;
    Comment_attr = keyword_attr(NAME_COMMENT);
    Ident_attr   = keyword_attr(NAME_IDENT);
    Keyword_attr = keyword_attr(NAME_KEYWORD);
    Number_attr  = keyword_attr(NAME_NUMBER);
    Preproc_attr = keyword_attr(NAME_PREPROC);
    String_attr  = keyword_attr(NAME_LITERAL);

    BEGIN(RULES);
    while (yylex() > 0) {
    }
}

%}

SPACE		[ \t]

DIRECTIVE	^%[^ \t]

IDENT		[a-zA-Z_][a-zA-Z_0-9]*

INTEGER		[-+]?([0-9]+)

SSTRING		\'(\\\.|[^\'])*\'
DSTRING		\"(\\\.|[^\"])*\"
STRINGS		({SSTRING}|{DSTRING})

STATES		<({IDENT}|\,)+>
PATTERN		(\\\.|[^ \t])+

%%

<INITIAL,RULES,ACTIONS,CODE>{DIRECTIVE}	{
		    WriteToken(Keyword_attr);
		    switch(yytext[1]) {
		    case '%':
		    	section++;
			set_state();
			break;
		    case '{':
		    	BEGIN(CODE);
			break;
		    case '}':
			set_state();
			break;
		    default:
			break;
		    }
		}

<RULES>^{IDENT}		{ WriteToken(Ident_attr); BEGIN(RULE1); }
<RULE1>{SPACE}+		{ ECHO; BEGIN(RULEX); }
<RULEX>.*		{ WriteToken(String_attr); BEGIN(RULES); }

<ACTIONS>^{STATES}?	{ WriteToken(Keyword_attr); BEGIN(ACTION1); }
<ACTION1>{PATTERN}	{ WriteToken(String_attr); BEGIN(ACTION2); }
<ACTION2>{SPACE}+	{ ECHO; BEGIN(ACTION3); }
<ACTION3>\{		{ ECHO; nesting = 1; BEGIN(CODE); }
<ACTION3>[^\{].*$	{ ECHO; BEGIN(ACTIONS); }

<CODE>{IDENT}		{ WriteToken(keyword_attr(yytext)); }
<CODE>{STRINGS}		{ WriteToken(String_attr); }
<CODE>{INTEGER}		{ WriteToken(Number_attr); }
<CODE>\{		{ ECHO; nesting++; }
<CODE>\}		{ ECHO; if (--nesting <= 0) { nesting = 0; set_rules(); } }

<CODE>"/*"		{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<COMMENT>[^*]*		{ WriteToken(Comment_attr); }
<COMMENT>"*"+[^*/]*	{ WriteToken(Comment_attr); }
<COMMENT>"*"+"/"	{ WriteToken(Comment_attr); BEGIN(CODE); }

<CODE>^{SPACE}*#{SPACE}*{IDENT}({SPACE}+(\<[^>]+\>|\"[^"]+\"))? { WriteToken(Preproc_attr); }
