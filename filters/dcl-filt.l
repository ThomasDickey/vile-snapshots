%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/dcl-filt.l,v 1.5 1998/12/25 22:51:58 tom Exp $
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

char *filter_name = "dcl";

static char *Action_attr;
static char *Comment_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

static int abbrev_len;

static void
add_abbrev(const char *name, int size, const char *attr)
{
    if (size > abbrev_len
     && isalpha(name[0]) && islower(name[0])
     && strncmp(name, "then", abbrev_len)
     && strncmp(name, "else", abbrev_len)
     && strncmp(name, "endif", abbrev_len)) {
	char *temp = strmalloc(name);
	temp[abbrev_len] = 0;
	if (keyword_attr(temp) == 0) {
	    insert_keyword(temp, attr);
	}
	free(temp);
    }
}

void
init_filter(int before GCC_UNUSED)
{
    /* This looks ugly, but not many languages support abbreviations.  So we
     * use a special case.
     */
    for (abbrev_len = 4; abbrev_len > 1; abbrev_len--)
	for_each_keyword(add_abbrev);
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;

    Action_attr  = keyword_attr(NAME_ACTION);
    Comment_attr = keyword_attr(NAME_COMMENT);
    Ident_attr   = keyword_attr(NAME_IDENT);
    Ident2_attr  = keyword_attr(NAME_IDENT2);
    Number_attr  = keyword_attr(NAME_NUMBER);
    String_attr  = keyword_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}

%}

BLANK		[ \t]
IDENT		[a-zA-Z_][$a-zA-Z0-9_]*

PARAM		'{IDENT}

LABEL		{IDENT}{BLANK}*:({BLANK}+|$)

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

{COMMENT}		{ WriteToken(Comment_attr); }

{NUMBER}		{ WriteToken(Number_attr); }

{STRING}		{ WriteToken(String_attr); }
