%s DESC GAPS CAPS VALS

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/ti-filt.l,v 1.2 1998/12/29 15:51:21 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of terminfo
 */

#include <filters.h>

char *filter_name = "ti";

static char *Action_attr;
static char *Comment_attr;
static char *Number_attr;
static char *Keyword_attr;
static char *Ident_attr;
static char *String_attr;

static void
echo_vals(char *text, int size, char *attr)
{
    fputc(*text, yyout);
    text++;
    size--;
    write_string(yyout, text, size, attr);
}

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

CAPNAME		[a-zA-Z][a-zA-Z0-9_]*

ALIAS		[a-zA-Z0-9+-]+

NUMBER		"#"[0-9]+
STRING		"="(\\[,]|[^,]|\t)*

%%

^"#"[^\n]*			{ WriteToken(Comment_attr); }

^({ALIAS}"|")+			{ WriteToken(Ident_attr); BEGIN(DESC); }
<DESC>[^,\n]*			{ WriteToken(Comment_attr); BEGIN(GAPS); }
<GAPS,VALS>(,|,\\\n\t)*		{ ECHO; BEGIN(CAPS); }
<CAPS>{CAPNAME}			{ WriteToken(check_name(yytext)); BEGIN(VALS); }
<VALS>'@'			{ WriteToken(Action_attr); BEGIN(GAPS); }
<VALS>{NUMBER}			{ echo_vals(yytext, yyleng, Number_attr); BEGIN(GAPS); }
<VALS>{STRING}			{ echo_vals(yytext, yyleng, String_attr); BEGIN(GAPS); }
