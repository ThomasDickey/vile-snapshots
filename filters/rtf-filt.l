%pointer

%{

/*
 * $Id: rtf-filt.l,v 1.12 2025/01/26 11:36:23 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of RTF (rich text format)
 * input text.
 */

#include <filters.h>

DefineFilter(rtf);

static char *Comment_attr;
static char *Error_attr;
static char *Number_attr;
static char *String_attr;

static const char *control_attr(char *, int);

%}

ESCAPE		\\'[[:xdigit:]][[:xdigit:]]
CTRLWORD	\\([[:lower:]])+([[:blank:]]?)
INTEGER		[[:digit:]]+

NUMBER		{INTEGER}

%%

{CTRLWORD}		{ WriteToken(control_attr(yytext, yyleng)); }
{ESCAPE}		{ WriteToken(String_attr); }
{NUMBER}		{ WriteToken(Number_attr); }

%%

static const char *
control_attr(char *text, int length)
{
    const char *attr = NULL;

    if (length > 1) {
	char save = 0;

	if (isBlank(CharOf(text[length - 1]))) {
	    save = text[--length];
	    text[length] = '\0';
	}
	attr = get_keyword_attr(text + 1);
	text[length] = save;
    }
    if (attr == NULL) {
	attr = Error_attr;
	flt_error("Unknown control word: %s", text);
    }
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

    Error_attr = class_attr(NAME_ERROR);
    Comment_attr = class_attr(NAME_COMMENT);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
