%x XMLTAG XMLATTR XMLCDATA COMMENT INLINE

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/xml-filt.l,v 1.12 2004/12/09 22:09:24 tom Exp $
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

typedef struct {
    char *name;
    int line;
    int col;
}
XMLTAGS;

static XMLTAGS *Tags;
static unsigned Num_tags = 0;
static int In_error = 0;

#include <fltstack.h>

static char * xml_attr(char *name);
static void xml_close(char *text);
static void xml_command(char *text);

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

<XMLTAG>"![CDATA["	{ WriteToken(Keyword_attr); push_state(XMLCDATA); flt_bfr_begin(String_attr); }

<XMLTAG>(\/)?">"	|
<XMLATTR>(\/)?">"	{ xml_close(yytext); pop_state(); }

<XMLTAG>(\/)?{IDENT}	{ xml_command(yytext); new_state(XMLATTR); }

<XMLCDATA>"]]"		{ flt_bfr_finish(); WriteToken(Keyword_attr); pop_state(); }
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

%%

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
	unsigned need = sizeof(XMLTAGS) * Num_tags;
	static unsigned have = 0;

	Tags = type_alloc(XMLTAGS, Tags, need, &have);
	Tags[Num_tags].name = strmalloc(text);
	Tags[Num_tags].line = flt_get_line();
	Tags[Num_tags].col = flt_get_col();
	++Num_tags;

	flt_puts(text, strlen(text), attr);
    } else {
	int bad = 0;

	if (Num_tags == 0
	|| strcmp(text + 1, Tags[Num_tags - 1].name) != 0) {
	    /*
	     * If we already reported an error, and this (new erroneous) tag
	     * does, in fact, correctly match a non-current opening tag, then
	     * pop the stack back to that tag (i.e., attempt to re-sync)
	     * and report as OK.
	     */
	    bad = 1;
	    if (Num_tags > 1 && In_error) {
		unsigned t = Num_tags - 2;
		int found = 0;
		do {
		    if (!strcmp(text + 1, Tags[t].name)) {
			found = 1;
			break;
		    }
		} while (t-- != 0);
		if (found) {	/* Matched lower tag */
		    while (Num_tags - 1 > t) {
			--Num_tags;
			free(Tags[Num_tags].name);
		    }
		    In_error = 0;
		    bad = 0;
		}
	    }
	}
	if (bad) {
	    attr = Error_attr;
	    In_error = 1;
	    if (Num_tags > 1)
		flt_error("expected tag:%s", Tags[Num_tags - 1].name);
	    else
		flt_error("mismatched tag");
	}

	if (Num_tags > 0)
	    free(Tags[--Num_tags].name);

	flt_puts(text, strlen(text), attr);
    }
}

static void
xml_close(char *text)
{
    if (text[0] == '/') {
	if (Num_tags > 0)
	    free(Tags[--Num_tags].name);
    }

    flt_puts(text, strlen(text), Action_attr);
}

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
	    free(Tags[--Num_tags].name);
#if NO_LEAKS
	free(Tags);
	Tags = 0;
#endif
    }
    if (Num_tags > 1)
	flt_error("expected tag:%s", Tags[Num_tags - 1].name);
    end_state();
}
