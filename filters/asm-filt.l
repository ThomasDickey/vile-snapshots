%pointer
%x NORMAL QUOTE1

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/asm-filt.l,v 1.11 2010/11/04 09:25:28 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of M$ assembler files.
 */

#include <filters.h>

DefineFilter("asm");

#define NAME_QUOTED "Quoted"

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Number_attr;
static char *Prepro_attr;
static char *Quoted_attr;
static char *String_attr;

%}

SSTRING		\'([^'\r\n]|'')*\'
DSTRING		\"([^"\r\n]|"")*\"
STRING		{SSTRING}|{DSTRING}

NAME		[[:alnum:]_$?@]

IDENT		[[:alpha:]_.]{NAME}*

BINARY		[0-1]+[bB]
DECIMAL		[[:digit:]]+
HEX		[[:xdigit:]]+[hH]
FLOAT		[[:xdigit:]]+[rR]
REAL		[-+]?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?
NUMBER		({HEX}|{FLOAT}|{BINARY}|{REAL}|{DECIMAL})

FORMAT		"%"[.*[:digit:]l#+-]*[cdeEfFgGisuxX]

%%

<NORMAL>{IDENT}		{ const char *attr = ci_keyword_attr(yytext);
			  WriteToken(attr);
			  if (attr == Quoted_attr) {
			      BEGIN(QUOTE1);
			  }
			}
<NORMAL>";"[^\r\n]*	{ WriteToken(Comment_attr); }

<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }
<NORMAL>[[:digit:]]{NUMBER}{NAME}+ {
			  flt_error("not a number");
			  WriteToken(Error_attr);
			}
<QUOTE1>[<>]		{ WriteToken(Action_attr); }
<QUOTE1>[^\r\n<>]*	{ WriteToken(String_attr); }
<QUOTE1>\n		{ ECHO; BEGIN(NORMAL); }

<NORMAL>\\.		|
<NORMAL>{STRING}	{ WriteToken(String_attr); }

%%

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
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    Prepro_attr = class_attr(NAME_PREPROC);
    Quoted_attr = class_attr(NAME_QUOTED);
    String_attr = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);

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
