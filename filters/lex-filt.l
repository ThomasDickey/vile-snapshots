%s RULES RULE1 RULEX ACTIONS ACTION2 ACTION3 CODE COMMENT

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/lex-filt.l,v 1.21 2003/02/12 00:46:00 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of LEX program.
 */

#include <filters.h>
#include <fltstack.h>

DefineFilter("lex");

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
	new_state(CODE);
    } else if (section >= 1) {
	new_state(ACTIONS);
    } else {
	new_state(RULES);
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
	flt_puts(text, n, Keyword_attr);
	text += n;
	len -= n;
    }
    flt_puts(text, len, String_attr);
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
    yyin = input;
    section = 0;
    nesting = 0;
    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr   = class_attr(NAME_IDENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    begin_state(RULES);
    while (yylex() > 0) {
    }
    end_state();
}

%}

SPACE		[ \t]

DIRECTIVE	^%[^ \t]

IDENT		[a-zA-Z_][a-zA-Z_0-9]*

INTEGER		[-+]?([0-9]+)

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

PATTERN		(\\.|\[(\\.|.)*\]|[^ \t\n])+
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
		    	new_state(CODE);
			break;
		    case '}':
			set_state();
			break;
		    default:
			break;
		    }
		}

<RULES>^{IDENT}		{ WriteToken(Ident_attr); new_state(RULE1); }
<RULE1>{SPACE}+		{ ECHO; new_state(RULEX); }
<RULEX>.*		{ WriteToken(String_attr); new_state(RULES); }

<ACTIONS>^{MATCH}	{ write_states(yytext, yyleng); new_state(ACTION2); }
<ACTION2>{SPACE}+	{ ECHO; new_state(ACTION3); }
<ACTION3>\{		{ ECHO; nesting = 1; new_state(CODE); }
<ACTION3>[^\{].*$	{ ECHO; new_state(ACTIONS); }

<CODE>{IDENT}		{ WriteToken(keyword_attr(yytext)); }
<CODE>{STRINGS}		{ WriteToken(String_attr); }
<CODE>{INTEGER}		{ WriteToken(Number_attr); }
<CODE>\{		{ ECHO; nesting++; }
<CODE>\}		{ ECHO; if (--nesting <= 0) { nesting = 0; set_rules(); } }

<CODE,RULES>"/*"	{ PushQuote(COMMENT, Comment_attr); }
<COMMENT>[^*]*		{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"*"+[^*/]*	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"*"+"/"	{ PopQuote(); }

<CODE>^{SPACE}*#{SPACE}*{IDENT}({SPACE}+(\<[^>]+\>|\"[^"]+\"))? { WriteToken(Preproc_attr); }
