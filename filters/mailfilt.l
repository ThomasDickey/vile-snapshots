%s HEADER BODY

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/mailfilt.l,v 1.8 1999/12/05 23:12:20 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of mail
 */

#include <filters.h>

char *filter_name = "mail";

static char *Action_attr;
static char *Comment_attr;
static char *Ident2_attr;
static char *String_attr;

static void
write_label(char *text, int size)
{
    static char *name;
    static unsigned used;
    name = do_alloc(name, size, &used);
    strcpy(name, text);
    name[--size] = 0;
    write_token(yyout, text, size, keyword_attr(name));
    fputc(':', yyout);
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

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(HEADER);
    while (yylex() > 0) {
    }
}

%}

BLANK		[ \t]

BEGIN_HEADER	^"From "{EMAIL}

LABEL		^[A-Z]([A-Za-z-]+):

NAME		[a-zA-Z0-9_.#-]+
ADDRESS		({NAME}|\.)+
EMAIL		{NAME}@{ADDRESS}

URL		[A-Za-z]+"://"[0-9a-zA-Z%/.~_-]+

%%

<HEADER>{LABEL}		{ write_label(yytext, yyleng); }
<HEADER>^\n		{ ECHO; BEGIN(BODY); }

<HEADER,BODY>{BEGIN_HEADER}	{ WriteToken(String_attr); BEGIN(HEADER); }

{EMAIL}			{ WriteToken(Ident2_attr); }
{URL}			{ WriteToken(Ident2_attr); }
