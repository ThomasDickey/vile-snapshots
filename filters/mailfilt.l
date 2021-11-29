%pointer
%s HEADER BODY

%o 7000

%{

/*
 * $Id: mailfilt.l,v 1.31 2013/12/02 01:32:53 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of mail
 */

#include <filters.h>

DefineFilter(mail);

static char *Action_attr;
static char *Comment_attr;
static char *Ident2_attr;
static char *String_attr;

static void write_label(char *text, int size);

%}

BLANK		[ \t]

LABEL		^[[:alpha:]]([[:alpha:]-]+):

NAME		[[:alnum:]_+.#-]+
ADDRESS		({NAME}|\.)+
EMAIL		{NAME}@{ADDRESS}

BEGIN_HEADER	^"From "{EMAIL}

URL		[[:alpha:]]+"://"[[:alnum:]#%&+\,.\/?;@_=~-]+

%%

<HEADER>{LABEL}		{ write_label(yytext, yyleng); }
<HEADER>^\n		{ ECHO; BEGIN(BODY); }

<HEADER,BODY>{BEGIN_HEADER}	{ WriteToken(String_attr); BEGIN(HEADER); }

{EMAIL}			{ WriteToken(Ident2_attr); }
{URL}			{ WriteToken(Ident2_attr); }

%%

static void
write_label(char *text, int size)
{
    const char *lname = lowercase_of(text);
    char temp[1024];
    char *buffer;

    if (size >= (int) sizeof(temp)) {
	buffer = strdup(lname);
    } else {
	buffer = temp;
	memcpy(temp, lname, (size_t) size);
    }
    buffer[--size] = '\0';
    flt_puts(text, size, get_keyword_attr(buffer));
    flt_putc(':');

    if (buffer != temp)
	free(buffer);
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
    Ident2_attr = class_attr(NAME_IDENT2);
    String_attr = class_attr(NAME_LITERAL);

    BEGIN(HEADER);
    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
