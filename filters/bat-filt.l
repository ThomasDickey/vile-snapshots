%s ECHOING QUOTED

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/bat-filt.l,v 1.14 2000/09/14 01:00:16 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of DOS (and
 * similar, such as W95, NT) batch file.
 */

#include <filters.h>

DefineFilter("bat");

static char *Action_attr;
static char *Comment_attr;
static char *Ident2_attr;
static char *String_attr;

static int setting;

static char *
variable_attr(char *text)
{
    char *attr = ci_keyword_attr(text);
    int isvar = (setting || *text == '%');

    if ((attr == 0 || *attr == 0) && isvar) {
	attr = Ident2_attr;
	insert_keyword(text, attr, 0);
    } else if (isvar) {
	attr = Ident2_attr;
    }
    return attr;
}

static void
write_variable(char *text, int length)
{
    flt_puts(text, length, variable_attr(text));
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
    yyin = input;

    setting = 0;
    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(INITIAL);
    while (yylex() > 0) {
    }
    flt_bfr_finish();
}

%}

TEXT		[^\r\n]
BLANK		[ \t\r]

REM		[rR][eE][mM]
SET		[sS][eE][tT]
ECHO		([eE][cC][hH][oO])(\.?)

IDENT		[a-zA-Z_][a-zA-Z_0-9]*

PARAM		%[0-9]
VARIABLE	%%{IDENT}|%{IDENT}%
IDENT2		({PARAM}|{VARIABLE})

%%

<INITIAL>^{BLANK}*@		{ WriteToken(Action_attr); }
<INITIAL>\032			{ WriteToken(Action_attr); }

<INITIAL>{ECHO}			{ WriteToken(ci_keyword_attr(yytext)); BEGIN(ECHOING); }
<ECHOING>[^\n]*			{ WriteToken(String_attr); BEGIN(INITIAL); }
<ECHOING>\n			{ ECHO; BEGIN(INITIAL); }

<INITIAL>{SET}			{ WriteToken(ci_keyword_attr(yytext)); setting = 1; }
<INITIAL>=			{ ECHO; setting=0; }
<INITIAL>\"			{ BEGIN(QUOTED);
				  flt_bfr_begin(String_attr);
				  flt_bfr_append(yytext, yyleng);
				}

<INITIAL>^{BLANK}*:{BLANK}*{IDENT} { WriteToken(Ident2_attr); }

<INITIAL>{IDENT}		{ write_variable(yytext, yyleng); }

<INITIAL>({IDENT2})		{ WriteToken(Ident2_attr); }

<INITIAL>^{BLANK}*{REM}$	|
<INITIAL>^{BLANK}*{REM}({BLANK}+{TEXT}*)? { WriteToken(Comment_attr); }

<INITIAL>\n			{ ECHO; setting = 0; }

<QUOTED>{IDENT2}		{ flt_bfr_embed(yytext, yyleng, variable_attr(yytext)); }
<QUOTED>(\\\"|[^\n\"])+		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>(.|\n)			{ flt_bfr_append(yytext, yyleng);
				  flt_bfr_finish();
				  BEGIN(INITIAL);
				}
