%s ECHOING QUOTED COMMENT

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/bat-filt.l,v 1.17 2003/05/20 20:38:41 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of DOS (and
 * similar, such as W95, NT) batch file.
 */

#include <filters.h>

DefineFilter("bat");

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *String_attr;

static int setting;

static char * variable_attr(char *text);
static void write_label(char *text, int length);

%}

TEXT		[^\r\n]
BLANK		[ \t\r]

IDENT		[a-zA-Z_.][a-zA-Z_0-9.]*

PARAM		%[0-9]
VARIABLE	%%{IDENT}|%{IDENT}%
IDENT2		({PARAM}|{VARIABLE})

%%

<INITIAL>^{BLANK}*@		{ WriteToken(Action_attr); }
<INITIAL>\032			{ WriteToken(Action_attr); }

<ECHOING>[^\n]*			{ WriteToken(String_attr); BEGIN(INITIAL); }
<ECHOING>\n			{ ECHO; BEGIN(INITIAL); }

<INITIAL>=			{ ECHO; setting=0; }
<INITIAL>\"			{ BEGIN(QUOTED);
				  flt_bfr_begin(String_attr);
				  flt_bfr_append(yytext, yyleng);
				}

<INITIAL>^{BLANK}*:{BLANK}*{IDENT} { write_label(yytext, yyleng); }

<INITIAL>{IDENT}		{ char *temp = lowercase_of(yytext);
				  char *attr = keyword_attr(temp);
				  if (!strcmp(temp, "echo")) {
					BEGIN(ECHOING);
				  } else if (!strcmp(temp, "rem")) {
					BEGIN(COMMENT);
				  } else if (!strcmp(temp, "set")) {
					setting = 1;
				  }
				  WriteToken(attr);
				}

<INITIAL>({IDENT2})		{ WriteToken(Ident2_attr); }

<COMMENT>{TEXT}*		{ WriteToken(Comment_attr); }
<COMMENT>\n			{ ECHO; BEGIN(INITIAL); }

<INITIAL>\n			{ ECHO; setting = 0; }

<QUOTED>{IDENT2}		{ flt_bfr_embed(yytext, yyleng, variable_attr(yytext)); }
<QUOTED>(\\\"|[^\n\"])+		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>(.|\n)			{ flt_bfr_append(yytext, yyleng);
				  flt_bfr_finish();
				  BEGIN(INITIAL);
				}
%%
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

#define LABEL 8	/* labels are unique to only 8 chars */

static void
write_label(char *text, int length)
{
    char *next = skip_blanks(skip_blanks(text)+1);
    unsigned len = strlen(next);
    unsigned limit = (len <= LABEL) ? length : (LABEL + next - text);

    flt_puts(text, limit, Ident2_attr);
    if (len > LABEL)
	flt_puts(next + LABEL, strlen(next + LABEL), Error_attr);
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    setting = 0;
    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    Ident2_attr  = class_attr(NAME_IDENT2);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(INITIAL);
    while (yylex() > 0) {
    }
    flt_bfr_finish();
}
