%s HEADER BODY

%o 7000

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/mailfilt.l,v 1.20 2006/05/21 19:50:19 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of mail
 */

#include <filters.h>

DefineFilter("mail");

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
    char *lname = lowercase_of(text);
    lname[--size] = 0;
    flt_puts(text, size, keyword_attr(lname));
    flt_putc(':');
}

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(HEADER);
    while (yylex() > 0) {
    }
}
