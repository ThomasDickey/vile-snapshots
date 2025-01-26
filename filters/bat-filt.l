%pointer
%s LABEL ECHOING QUOTED COMMENT

%{

/*
 * $Id: bat-filt.l,v 1.38 2025/01/26 11:05:40 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of DOS (and
 * similar, such as W95, NT) batch file.
 */

#include <filters.h>

DefineFilter(bat);

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *String_attr;

static int setting;

static const char *variable_attr(char *text);
static void write_label(char *text, int length);

%}

TEXT		[^\r\n]
BLANK		[ \t\r]

IDENT		[[:alpha:]_.][[:alnum:]_.]*

PARAM		%[[:digit:]*]
VARIABLE	%%{IDENT}|%{IDENT}%
IDENT2		({PARAM}|{VARIABLE})

%%

<INITIAL>^{BLANK}*@		{ WriteToken(Action_attr); }
<INITIAL>\032			{ WriteToken(Action_attr); }

<ECHOING>[^\r\n]*		{ WriteToken(String_attr); BEGIN(INITIAL); }
<ECHOING>\n			{ ECHO; BEGIN(INITIAL); }

<INITIAL>=			{ ECHO; setting=0; }
<INITIAL>\"			{ BEGIN(QUOTED);
				  flt_bfr_begin(String_attr);
				  flt_bfr_append(yytext, yyleng);
				}

<INITIAL>^{BLANK}*:		{ WriteToken(Action_attr); BEGIN(LABEL); }
<LABEL>{BLANK}+			{ ECHO; }
<LABEL>{IDENT}			{ write_label(yytext, yyleng); BEGIN(INITIAL); }
<LABEL>[\r\n]			{ ECHO; BEGIN(INITIAL); }
<LABEL>.			{ WriteToken("U"); BEGIN(INITIAL); }

<INITIAL>{IDENT}		{ const char *temp = lowercase_of(yytext);
				  char *type = strrchr(temp, '.');
				  const char *attr;
				  int echoing = 0;

				  /* "echo." is a legal "echo", and the "."
				   * is not echoed.  So we highlight it as
				   * part of the name.
				   */
				  if (type != NULL)
				      *type = '\0';
				  attr = get_keyword_attr(temp);
				  if (!strcmp(temp, "echo")) {
					BEGIN(ECHOING);
					echoing = 1;
				  } else if (!strcmp(temp, "goto")) {
					BEGIN(LABEL);
				  } else if (attr != NULL && Comment_attr == attr) {
					BEGIN(COMMENT);
				  } else if (!strcmp(temp, "set")) {
					setting = 1;
				  }
				  if (type != NULL && echoing) {
				      int len = (int) (type - temp) + 1;

				      flt_puts(yytext, len, attr);
				      flt_puts(yytext + len, yyleng - len, String_attr);
				  } else {
				      flt_puts(yytext, yyleng, attr);
				  }
				}

<INITIAL>({IDENT2})		{ WriteToken(Ident2_attr); }

<COMMENT>{TEXT}*		{ WriteToken(Comment_attr); }
<COMMENT>\n			{ ECHO; BEGIN(INITIAL); }

<INITIAL>\n			{ ECHO; setting = 0; }

<QUOTED>{IDENT2}		{ flt_bfr_embed(yytext, yyleng, variable_attr(yytext)); }
<QUOTED>(\\\"|[^\r\n\"])+	{ flt_bfr_append(yytext, yyleng); }
<QUOTED>(.|\n)			{ flt_bfr_append(yytext, yyleng);
				  flt_bfr_finish();
				  BEGIN(INITIAL);
				}
%%

static const char *
variable_attr(char *text)
{
    const char *attr = get_keyword_attr(text);
    int isvar = (setting || *text == '%');

    if (isEmpty(attr) && isvar) {
	attr = Ident2_attr;
	insert_keyword(text, attr, 0);
    } else if (isvar) {
	attr = Ident2_attr;
    }
    return attr;
}

#define MAX_LABEL 8		/* labels are unique to only 8 chars */

static void
write_label(char *text, int length)
{
    char *next = skip_blanks(skip_blanks(text));
    size_t len = strlen(next);
    int limit = ((len <= MAX_LABEL)
		 ? length
		 : (MAX_LABEL + (int) (next - text)));

    flt_puts(text, limit, Ident2_attr);
    if (len > MAX_LABEL) {
	flt_error("label too long");
	flt_puts(next + MAX_LABEL, (int) strlen(next + MAX_LABEL), Error_attr);
    }
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

    setting = 0;
    Action_attr = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident2_attr = class_attr(NAME_IDENT2);
    String_attr = class_attr(NAME_LITERAL);

    BEGIN(INITIAL);
    RunLEX();
    flt_bfr_error();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
