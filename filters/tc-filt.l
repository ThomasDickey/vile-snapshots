%s DESC GAPS CAPS VALS

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/tc-filt.l,v 1.4 1999/01/05 03:06:58 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of termcap
 */

#include <filters.h>

char *filter_name = "tc";

static char *Action_attr;
static char *Comment_attr;
static char *Number_attr;
static char *Keyword_attr;
static char *Ident_attr;
static char *String_attr;

static int commented;

static char *
check_name(char *name)
{
    char *attr = keyword_attr(name);
    if (attr == 0)
	attr = "";
    if (strcmp(attr, Keyword_attr))
    	attr = Action_attr;
    return attr;
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

    Action_attr  = keyword_attr(NAME_ACTION);
    Comment_attr = keyword_attr(NAME_COMMENT);
    Keyword_attr = keyword_attr(NAME_KEYWORD);
    Number_attr  = keyword_attr(NAME_NUMBER);
    Ident_attr   = keyword_attr(NAME_IDENT);
    String_attr  = keyword_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}

%}

BLANK		[ \t]
CAPCHAR		[^# \t\n:=]
CAPNAME		{CAPCHAR}{CAPCHAR}
DOTNAME		(\.)+{CAPNAME}

ALIAS		[a-zA-Z0-9-]+

NUMBER		"#"[0-9]+
STRING		(\\[:]|[^:])*

%%

^"#"[^\n]*			{ WriteToken(Comment_attr); }

^({ALIAS}"|")*			{ WriteToken(Ident_attr); BEGIN(DESC); }
<DESC>[^:\n]*			{ WriteToken(Comment_attr); BEGIN(GAPS); }
<GAPS,VALS>(:|:\\\n\t)*		{ commented=0; ECHO; BEGIN(CAPS); }
<CAPS>{CAPNAME}			{ WriteToken(check_name(yytext)); BEGIN(VALS); }
<CAPS>{DOTNAME}			{ WriteToken(Comment_attr); commented = 1; BEGIN(VALS); }
<VALS>{NUMBER}			{ WriteToken(commented ? Comment_attr : Number_attr); BEGIN(GAPS); }
<VALS>{STRING}			{ WriteString(commented ? Comment_attr : String_attr); BEGIN(GAPS); }
