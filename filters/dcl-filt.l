%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/dcl-filt.l,v 1.11 2000/05/29 18:42:53 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of DCL files.
 *
 * Limitations:
 *
 * DEC DCL used to allow (probably still does) command lines without a
 * leading '$'.  The leading '$' on a line can be confused with a '$' at the
 * beginning of an identifier.
 *
 * This filter only finds a label which is followed by either whitespace
 * or the end of a line.
 *
 * FIXME: It would be nice to implement VMS's equivalent to here-documents
 * FIXME: parameters of lexical functions can be quoted, nested within a
 * quoted string.
 */

#include <filters.h>

DefineFilter("dcl");

static char *Action_attr;
static char *Comment_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
    yyin = input;

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr   = class_attr(NAME_IDENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}

%}

BLANK		[ \t]
IDENT		[a-zA-Z_][$a-zA-Z0-9_]*

PARAM		'{IDENT}

LABEL		{IDENT}{BLANK}*:({BLANK}+|$)

INLINECOMMENT	![^\n]*
COMMENT		^({BLANK}*)($)?({BLANK}*)(![^\n]*)?

NUMBER		([0-9]+)|(%[dD][0-9]+)|(%[oO][0-7]+)|(%[xX][a-fA-F0-9]+)

STRING		\"([^"\n]|(""))*\"

%%

^{BLANK}*@		{ WriteToken(Action_attr); }
\032			{ WriteToken(Action_attr); }

{LABEL}			{ WriteToken(Ident2_attr); }

{IDENT}			{ WriteToken(ci_keyword_attr(yytext)); }

"/"{IDENT}		{ WriteToken(Ident_attr); }

{PARAM}			{ WriteToken(Ident2_attr); }

{INLINECOMMENT}		|
{COMMENT}		{ WriteToken(Comment_attr); }

{NUMBER}		{ WriteToken(Number_attr); }

{STRING}		{ WriteToken(String_attr); }
