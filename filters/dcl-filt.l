%state NORMAL LITERAL

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/dcl-filt.l,v 1.14 2004/05/15 11:49:42 tom Exp $
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
 * FIXME: are '$' legal in labels?  DEC says alphanumerics, but I see '_' used.
 * If a leading '$' were legal, I'd have to introduce more states.
 * FIXME: it would be nice to highlight labels that are given in goto and
 * /err= options.
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

static char *look_for = 0;

static void
handle_ident(char *text, int length)
{
    char *attr = ci_keyword_attr(text);
    char *temp = lowercase_of(text);
    flt_puts(text, length, attr);
    if (!strcmp(temp, "deck")) {
	/* FIXME: "DECK" can also have an option /dollars whose value would
	 * be the actual string to search for.
	 */
	look_for = "eod";
    }
}

static void
handle_newline(void)
{
    flt_putc('\n');
    if (look_for != 0) {
	BEGIN(LITERAL);
    }
}

static void
handle_literal(char *text, int length)
{
    int dollars = 0;

    if (*text == '$') {
	flt_puts(text, 1, Comment_attr);
	++text;
	--length;
	dollars = 1;
    }
    while (length > 0 && isspace(CharOf(*text))) {
	flt_putc(*text);
	++text;
	--length;
    }
    if (length > 0) {
	char *attr = String_attr;

	if (dollars) {
	    char *temp = lowercase_of(text);
	    if (!strcmp(temp, look_for)) {
		look_for = 0;
		attr = ci_keyword_attr(temp);
		BEGIN(NORMAL);
	    }
	}
	flt_puts(text, length, attr);
    }
}

%}

BLANK		[ \t]
LABEL		[a-zA-Z0-9_]+
IDENT		[a-zA-Z_][$a-zA-Z0-9_]*
OPTION		"/"{IDENT}

PARAM		'{IDENT}

LABELLINE	{LABEL}{BLANK}*:$
LABELPLUS	{LABEL}{BLANK}*:{BLANK}+

INLINECOMMENT	![^\n]*
COMMENT		^({BLANK}*)($)?({BLANK}*)(![^\n]*)?

NUMBER		([0-9]+)|(%[dD][0-9]+)|(%[oO][0-7]+)|(%[xX][a-fA-F0-9]+)

STRING		\"([^"\n]|(""))*\"

%%

<NORMAL>^{BLANK}*@	{ WriteToken(Action_attr); }
<NORMAL>\032		{ WriteToken(Action_attr); }

<NORMAL>{LABELLINE}	|
<NORMAL>{LABELPLUS}	{ WriteToken(Ident2_attr); }

<NORMAL>{IDENT}		{ handle_ident(yytext, yyleng); }

<NORMAL>{OPTION}	{ WriteToken(Ident_attr); }

<NORMAL>{PARAM}		{ WriteToken(Ident2_attr); }

<NORMAL>{INLINECOMMENT}	|
<NORMAL>{COMMENT}	{ WriteToken(Comment_attr); }

<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }

<NORMAL>{STRING}	{ WriteToken(String_attr); }

<NORMAL>[\n]		{ handle_newline(); }

<LITERAL>[^\n]+		{ handle_literal(yytext, yyleng); }
<LITERAL>[\n]		{ ECHO; }

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
    Ident_attr   = class_attr(NAME_IDENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    look_for = 0;
    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
}
