%s DESC GAPS CAPS VALS

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/ti-filt.l,v 1.15 2004/08/08 18:56:50 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of terminfo
 */

#include <filters.h>

DefineFilter("ti");

static char *Action_attr;
static char *Comment_attr;
static char *Number_attr;
static char *Keyword_attr;
static char *Ident_attr;
static char *String_attr;

static int commented;

static char * check_name(char *name);
static void echo_vals(char *text, int size, char *attr);

%}

SPACE		[ \t]

CAPNAME		[a-zA-Z][a-zA-Z0-9_]*
DOTNAME		\.{CAPNAME}

ALIAS		[a-zA-Z_\.0-9+-]+

COMMA		[,]
ESCNL		{COMMA}{SPACE}*\\\n\t

NUMBER		"#"[0-9]+
STRING		"="(\\.|\^.|[^,\^\\])*

%%

^"#"[^\n]*			{ WriteToken(Comment_attr); }

^{ALIAS}","			{ flt_puts(yytext, yyleng - 1, Ident_attr);
				  flt_putc(',');
				  BEGIN(CAPS);
				}
^({ALIAS}"|")+			{ WriteToken(Ident_attr); BEGIN(DESC); }
<DESC>[^,\n]*			{ WriteToken(Comment_attr); BEGIN(GAPS); }
<GAPS,VALS>({COMMA}|{ESCNL})*	{ commented = 0; ECHO; BEGIN(CAPS); }
<CAPS>{CAPNAME}			{ WriteToken(check_name(yytext)); BEGIN(VALS); }
<CAPS>{DOTNAME}			{ WriteToken(Comment_attr); commented = 1; BEGIN(VALS); }
<VALS>'@'			{ WriteToken(Action_attr); BEGIN(GAPS); }
<VALS>{NUMBER}			{ echo_vals(yytext, yyleng, Number_attr); BEGIN(GAPS); }
<VALS>{STRING}			{ echo_vals(yytext, yyleng, String_attr); BEGIN(GAPS); }

%%

static void
echo_vals(char *text, int size, char *attr)
{
    if (commented) {
	attr = Comment_attr;
    } else {
	flt_putc(*text);
	text++;
	size--;
    }
    flt_puts(text, size, attr);
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

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    Ident_attr   = class_attr(NAME_IDENT);
    String_attr  = class_attr(NAME_LITERAL);

    commented = 0;

    BEGIN(INITIAL);
    while (yylex() > 0) {
    }
}
