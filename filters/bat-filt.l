%s ECHOING QUOTED

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/bat-filt.l,v 1.12 2000/08/20 22:53:40 tom Exp $
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

/*
 * Buffer text for quoted text, allowing variables.
 */
static char    *save_bfr;
static unsigned save_max;
static unsigned save_len;

static void
add_to_quoted(char *text, int length)
{
    save_bfr = do_alloc(save_bfr, save_len + length, &save_max);
    strncpy(save_bfr + save_len, text, length);
    save_len += length;
}

static void
flush_quoted(void)
{
    if (save_len) {
	flt_puts(save_bfr, save_len, String_attr);
	save_len = 0;
    }
}

static void
write_variable(char *text, int length)
{
    char *attr = ci_keyword_attr(yytext);
    int isvar = (setting || *text == '%');

    if ((attr == 0 || *attr == 0) && isvar) {
	attr = Ident2_attr;
	insert_keyword(text, attr, 0);
    } else if (isvar) {
	attr = Ident2_attr;
    }
    flt_puts(text, length, attr);
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
    flush_quoted();
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
<INITIAL>\"			{ add_to_quoted(yytext, yyleng); BEGIN(QUOTED); }

<INITIAL>^{BLANK}*:{BLANK}*{IDENT} { WriteToken(Ident2_attr); }

<INITIAL>{IDENT}		{ write_variable(yytext, yyleng); }

<INITIAL>({IDENT2})		{ WriteToken(Ident2_attr); }

<INITIAL>^{BLANK}*{REM}$	|
<INITIAL>^{BLANK}*{REM}({BLANK}+{TEXT}*)? { WriteToken(Comment_attr); }

<INITIAL>\n			{ ECHO; setting = 0; }

<QUOTED>{IDENT2}		{ flush_quoted(); write_variable(yytext, yyleng); }
<QUOTED>[^\n\"]			{ add_to_quoted(yytext, yyleng); }
<QUOTED>[\"\n]			{ add_to_quoted(yytext, yyleng);
				  flush_quoted();
				  BEGIN(INITIAL);
				}
