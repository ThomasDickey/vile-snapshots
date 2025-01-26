%pointer
%s NAME DESC GAPS CAPS VALS

%{

/*
 * $Id: ti-filt.l,v 1.31 2025/01/26 11:38:02 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of terminfo
 */

#include <filters.h>
#undef YY_NO_UNPUT

DefineFilter(ti);

#define WriteAlias(attr) \
	WriteToken3(attr, yyleng - 1); \
	WriteToken2(String_attr, yyleng - 1)

static char *Action_attr;
static char *Comment_attr;
static char *Number_attr;
static char *Keyword_attr;
static char *Ident_attr;
static char *String_attr;

static int commented;

static const char *check_name(const char *name);
static void echo_vals(char *text, int size, char *attr);

%}

SPACE		[ \t]

CAPNAME		[[:alpha:]][[:alnum:]_]*
DOTNAME		\.{CAPNAME}

ALIAS		[[:alnum:]][[:alnum:]_.+-]*

COMMA		[,]
ESCNL		{COMMA}{SPACE}*\\\n\t

HEXNUM		"0x"[[:xdigit:]]+
DECNUM		[[:digit:]]+

NUMBER		"#"({HEXNUM}|{DECNUM})
STRING		"="(\\.|\^.|[^,\^\\])*

%%

^"#"[^\r\n]*			{ WriteToken(Comment_attr); }

^{ALIAS}","			{ flt_puts(yytext, yyleng - 1, Ident_attr);
				  flt_putc(',');
				  BEGIN(CAPS);
				}
^{ALIAS}"|"			{ WriteAlias(Ident_attr); BEGIN(NAME); }
<NAME>{ALIAS}"|"		{ WriteAlias(Ident_attr); }
<NAME>.				{ unput(yytext[0]); BEGIN(DESC); }
<DESC>[^,\r\n]+			{ WriteToken(Comment_attr); BEGIN(GAPS); }
<DESC>.				{ unput(yytext[0]); BEGIN(GAPS); }
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

static const char *
check_name(const char *name)
{
    const char *attr = get_keyword_attr(name);
    if (attr == NULL)
	attr = "";
    if (strcmp(attr, Keyword_attr))
	attr = Action_attr;
    return attr;
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

    Action_attr = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    Ident_attr = class_attr(NAME_IDENT);
    String_attr = class_attr(NAME_LITERAL);

    commented = 0;

    BEGIN(INITIAL);
    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
