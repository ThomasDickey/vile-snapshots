%x NORMAL QUOTE1

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/asm-filt.l,v 1.3 2004/12/10 00:16:37 tom Exp $
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

SSTRING		\'([^'\n]|'')*\'
DSTRING		\"([^"\n]|"")*\"
STRING		{SSTRING}|{DSTRING}

NAME		[a-zA-Z0-9_$?@]

IDENT		[a-zA-Z_.]{NAME}*

BINARY		[0-1]+[bB]
DECIMAL		[0-9]+
HEX		[0-9a-fA-F]+[hH]
FLOAT		[0-9a-fA-F]+[rR]
REAL		[-+]?([0-9]*\.[0-9]+)([eE][+-]?[0-9]+)?
NUMBER		({HEX}|{FLOAT}|{BINARY}|{REAL}|{DECIMAL})

FORMAT		"%"[.*0-9l#+-]*[cdeEfFgGisuxX]

%%

<NORMAL>{IDENT}		{ char *attr = ci_keyword_attr(yytext);
			  WriteToken(attr);
			  if (attr == Quoted_attr) {
			      BEGIN(QUOTE1);
			  }
			}
<NORMAL>";"[^\n]*	{ WriteToken(Comment_attr); }

<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }
<NORMAL>[0-9]{NUMBER}{NAME}+ {
			  flt_error("not a number");
			  WriteToken(Error_attr);
			}
<QUOTE1>[<>]		{ WriteToken(Action_attr); }
<QUOTE1>[^\n<>]*	{ WriteToken(String_attr); }
<QUOTE1>\n		{ ECHO; BEGIN(NORMAL); }

<NORMAL>\\.		|
<NORMAL>{STRING}	{ WriteToken(String_attr); }

%%

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    Ident_attr   = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    Prepro_attr  = class_attr(NAME_PREPROC);
    Quoted_attr  = class_attr(NAME_QUOTED);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);

    while (yylex() > 0) {
    }
    flt_bfr_error();
}
