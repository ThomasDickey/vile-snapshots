%s RULES RULE1 RULEX ACTIONS ACTION2 ACTION3 CODE COMMENT

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/lex-filt.l,v 1.15 2000/01/31 00:22:49 tom Exp $
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

/* For some reason, I cannot get flex to yield the substrings for <STATES>
 * and {PATTERN}, so I merged the two in the actions and split them here.
 */
static void
write_states(char *text, int len)
{
    int n;
    if (text[0] == '<') {	/* only happens if we have {STATES} */
	n = 1 + strchr(text, '>') - text;
	write_token(yyout, text, n, Keyword_attr);
	text += n;
	len -= n;
    }
    write_token(yyout, text, len, String_attr);
}

void
init_filter(int before GCC_UNUSED)
{
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;
    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr   = class_attr(NAME_IDENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(RULES);
    while (yylex() > 0) {
    }
}

%}

SPACE		[ \t]

DIRECTIVE	^%[^ \t]

IDENT		[a-zA-Z_][a-zA-Z_0-9]*

INTEGER		[-+]?([0-9]+)

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

PATTERN		(\\\.|[^ \t\n])+
STATES		(<({IDENT}|\,)+>)
MATCH		{STATES}?{PATTERN}

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

<ACTIONS>^{MATCH}	{ write_states(yytext, yyleng); BEGIN(ACTION2); }
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
