%x XMLTAG XMLATTR XMLCDATA COMMENT INLINE

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/xml-filt.l,v 1.10 2002/12/30 15:41:28 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of XML input
 * text.
 *
 * By Adam S. Denton (adenton@yahoo.com)
 *
 * Strongly based on the HTML filter program
 * based on a version written by Pierre Dittgen (dittgen@univ-mlv.fr)
 */

#include <filters.h>

DefineFilter("xml");

static char *Action_attr;
static char *Comment_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;
static char *Error_attr;

static char **Tags;
static unsigned Num_tags = 0;
static int In_error = 0;

#include <fltstack.h>

static char *
xml_attr(char *name)
{
    char *attr = keyword_attr(name);

    if (attr == 0)
	attr = Ident_attr;
    return attr;
}

static void
xml_command(char *text)
{
    int ending = (text[0] == '/');
    char *name = ending ? text + 1 : text;
    char *attr = xml_attr(name);

    if (!ending) {
	unsigned need = sizeof(char *) * Num_tags;
	static unsigned have = 0;

	Tags = type_alloc(char *, Tags, need, &have);
	Tags[Num_tags++] = strmalloc(text);

	flt_puts(text, strlen(text), attr);
    } else {
	int bad = 0;

	if (Num_tags == 0
	|| strcmp(text + 1, Tags[Num_tags - 1]) != 0) {
	    /*
	     * If we already reported an error, and this (new erroneous) tag
	     * does, in fact, correctly match a non-current opening tag, then
	     * pop the stack back to the that tag (i.e., attempt to re-sync)
	     * and report as OK.
	     */
	    bad = 1;
	    if (Num_tags > 1 && In_error) {
		unsigned t = Num_tags - 2;
		int found = 0;
		do {
		    if (!strcmp(text + 1, Tags[t])) {
			found = 1;
			break;
		    }
		} while (t-- != 0);
		if (found) {	/* Matched lower tag */
		    while (Num_tags - 1 > t)
			free(Tags[--Num_tags]);
		    In_error = 0;
		    bad = 0;
		}
	    }
	}
	if (bad) {
	    attr = Error_attr;
	    In_error = 1;
	}

	if (Num_tags > 0)
	    free(Tags[--Num_tags]);

	flt_puts(text, strlen(text), attr);
    }
}

static void
xml_close(char *text)
{
    if (text[0] == '/') {
	if (Num_tags > 0)
	    free(Tags[--Num_tags]);
    }

    flt_puts(text, strlen(text), Action_attr);
}

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
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);
    Error_attr   = class_attr(NAME_ERROR);

    begin_state(INITIAL);
    while (yylex() > 0) {
    }
    flt_bfr_error();

    if (Tags != 0) {
	while (Num_tags > 0)
	    free(Tags[--Num_tags]);
#if NO_LEAKS
	free(Tags);
	Tags = 0;
#endif
    }
    end_state();
}

%}

SPACE		[ \t]

INTEGER		[-+]?([0-9]+)
REAL		[-+]?([0-9]*\.[0-9]+)([eE][+-]?[0-9]+)?
HEXNUM		"#"[0-9A-Fa-f]+
NUMBER		{INTEGER}|{REAL}|{HEXNUM}

IDENT		[a-zA-Z_!?][a-zA-Z_0-9.-]*

HSTRING		\"[^"]*\"
QSTRING		\\["]([^"]|\\["])*\\["]
STRING		{HSTRING}|{QSTRING}

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

ENTITY		&{IDENT};
VALUE		#{IDENT}

%%

<INITIAL,INLINE>"<"	{ WriteToken(Action_attr); push_state(XMLTAG); }

<XMLTAG>"![CDATA["	{ WriteToken(Keyword_attr); BEGIN(XMLCDATA); flt_bfr_begin(String_attr); }

<XMLTAG>(\/)?">"	|
<XMLATTR>(\/)?">"	{ xml_close(yytext); pop_state(); }

<XMLTAG>(\/)?{IDENT}	{ xml_command(yytext); BEGIN(XMLATTR); }

<XMLCDATA>"]]"		{ flt_bfr_finish(); WriteToken(Keyword_attr); BEGIN(XMLTAG); }
<XMLCDATA>[\n]		|
<XMLCDATA>.		{ flt_bfr_append(yytext, yyleng); }

<INITIAL,INLINE>"<!--"	{ PushQuote(COMMENT, Comment_attr); }
<COMMENT>[\n]		|
<COMMENT>[^\n-]+	|
<COMMENT>[-]+[^-\>\n]*	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>[-]+"->"	{ PopQuote(); }

<XMLATTR>"["		{ WriteToken(Action_attr); push_state(INLINE); }
<INLINE>"]"		{ WriteToken(Action_attr); pop_state(); }

<INLINE>{IDENT}		|
<XMLATTR>{IDENT}	{ WriteToken(xml_attr(yytext)); }

<INLINE>{VALUE}		|
<XMLTAG>{VALUE}		|
<XMLATTR>{VALUE}	|
<XMLTAG>{STRING}	|
<XMLATTR>{STRING}	{ WriteToken(String_attr); }

<INITIAL,INLINE>{ENTITY} |
<XMLTAG>{NUMBER}	|
<XMLATTR>{NUMBER}	{ WriteToken(Number_attr); }