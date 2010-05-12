%pointer
%x XMLTAG XMLATTR XMLCDATA XMLPI COMMENT INLINE

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/xml-filt.l,v 1.33 2010/05/11 23:09:44 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of XML input
 * text.
 *
 * By Adam S. Denton (adenton@yahoo.com)
 * and Thomas E. Dickey
 *
 * Strongly based on the HTML filter program
 * based on a version written by Pierre Dittgen (dittgen@univ-mlv.fr)
 */

#include <filters.h>

DefineFilter("xml");

#define SYMS_XMLNS	"xmlns"

#define LEN_TABLENAME	80
#define FMT_XMLNS	"xmlns_%.70s"
#define FMT_PROP	"%.70s_prop"

#ifdef FLEX_DEBUG
#define DBG_HTML(params) if (yy_flex_debug) fprintf params
#else
#define DBG_HTML(params) /* nothing */
#endif

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

static const char * xml_keyword_attr(const char *name);
static void xml_close(char *text);
static void xml_command(char *text);
static void xml_prefixed_property(char *text, int len);
static void xml_property(char *text, int len);

%}

SPACE		[ \t]

INTEGER		[-+]?([[:digit:]]+)
REAL		[-+]?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?
HEXNUM		"#"[[:xdigit:]]+
NUMBER		{INTEGER}|{REAL}|{HEXNUM}

IDENT		[[:alpha:]_!?][[:alnum:]_.-]*

XMLTAG		{IDENT}(":"{IDENT})?

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
<INITIAL,INLINE>"<?"	{ WriteToken(Action_attr); push_state(XMLPI); }

<XMLTAG>"![CDATA["	{ WriteToken(Keyword_attr); push_state(XMLCDATA); flt_bfr_begin(String_attr); }

<XMLTAG>(\/)?">"	|
<XMLPI>"?>"		|
<XMLATTR>(\/)?">"	{ xml_close(yytext); pop_state(); }

<XMLTAG>(\/)?{XMLTAG}	{ char tablename[LEN_TABLENAME];

			  flt_setup_symbols(default_table);
			  xml_command(yytext);

			  sprintf(tablename, FMT_PROP, default_table);
			  flt_setup_symbols(tablename);

			  new_state(XMLATTR);
			}

<XMLCDATA>"]]"		{ flt_bfr_finish(); WriteToken(Keyword_attr); pop_state(); }
<XMLCDATA>[\n]		|
<XMLCDATA>.		{ flt_bfr_append(yytext, yyleng); }

<INITIAL,INLINE>"<!--"	{ PushQuote(COMMENT, Comment_attr); }
<COMMENT>[\n]		|
<COMMENT>[^\r\n-]+	|
<COMMENT>[-]+[^-\>\r\n]*	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>[-]+"->"	{ PopQuote(); }

<XMLATTR>"["		{ WriteToken(Action_attr); push_state(INLINE); }
<INLINE>"]"		{ WriteToken(Action_attr); pop_state(); }

<INLINE>{IDENT}		|
<XMLPI>{IDENT}		|
<XMLATTR>{IDENT}	{ xml_property(yytext, yyleng); }
<XMLATTR>{XMLTAG}	{ xml_prefixed_property(yytext, yyleng); }

<INLINE>{VALUE}		|
<XMLTAG>{VALUE}		|
<XMLATTR>{VALUE}	|
<XMLTAG>{STRING}	|
<XMLPI>{STRING}		|
<XMLATTR>{STRING}	{ WriteToken(String_attr); }

<INITIAL,INLINE>{ENTITY} |
<XMLTAG>{NUMBER}	|
<XMLPI>{NUMBER}		|
<XMLATTR>{NUMBER}	{ WriteToken(Number_attr); }

%%

#include <fltstack.h>

static const char *
xml_keyword_attr(const char *name)
{
    const char *attr = keyword_attr(name);

    if (attr == 0)
	attr = Ident_attr;
    return attr;
}

static void
xml_prefixed_property(char *text, int len)
{
#define STR_XMLNS "xmlns:"
#define LEN_XMLNS 6

    if (len > LEN_XMLNS && !strncmp(text, STR_XMLNS, LEN_XMLNS)) {
	const char *old_table = get_symbol_table();
	char *marker = (text + LEN_XMLNS - 1);

	set_symbol_table(SYMS_XMLNS);
	insert_keyword(text + LEN_XMLNS, Action_attr, 0);
	set_symbol_table(old_table);

	*marker = '\0';
	flt_puts(text, LEN_XMLNS - 1, xml_keyword_attr(text));
	*marker = ':';
	flt_putc(*marker++);
	flt_puts(marker, len - LEN_XMLNS, Action_attr);
    } else {
	flt_puts(text, (int) strlen(text), xml_keyword_attr(text));
    }
}

static void
xml_property(char *text, int len)
{
    flt_puts(text, len, xml_keyword_attr(text));
}

static void
xml_command(char *text)
{
    int ending = (text[0] == '/');
    char *name = ending ? text + 1 : text;
    const char *attr = xml_keyword_attr(name);

    if (!ending) {
	unsigned need = sizeof(XMLTAGS) * Num_tags;
	static unsigned have = 0;

	Tags = type_alloc(XMLTAGS, Tags, need, &have);
	Tags[Num_tags].name = strmalloc(text);
	Tags[Num_tags].line = flt_get_line();
	Tags[Num_tags].col = flt_get_col();
	++Num_tags;

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
    }
    if (isEmpty(attr)) {
	char *marker = strchr(text, ':');
	if (marker != 0 && marker[1] != '\0') {
	    const char *old_table = get_symbol_table();
	    char new_table[LEN_TABLENAME];

	    *marker = '\0';
	    set_symbol_table(SYMS_XMLNS);
	    flt_puts(text, (int) strlen(text), keyword_attr(name));
	    set_symbol_table(old_table);

	    sprintf(new_table, FMT_XMLNS, name);

	    flt_putc(':');

	    text = marker + 1;
	    set_symbol_table(new_table);
	    attr = keyword_attr(text);
	    *marker = ':';

	    set_symbol_table(old_table);
	}
    }
    flt_puts(text, (int) strlen(text), attr);
}

static void
xml_close(char *text)
{
    if (text[0] == '/') {
	if (Num_tags > 0)
	    free(Tags[--Num_tags].name);
    }

    flt_puts(text, (int) strlen(text), Action_attr);

    set_symbol_table(default_table);
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

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr   = class_attr(NAME_IDENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);
    Error_attr   = class_attr(NAME_ERROR);

    set_symbol_table(default_table);

    begin_state(INITIAL);
    RunLEX();
    flt_bfr_error();

    if (Tags != 0) {
	while (Num_tags-- > 0) {
	    flt_error("expected tag:%s", Tags[Num_tags].name);
	    free(Tags[Num_tags].name);
	}
#if NO_LEAKS
	free(Tags);
	Tags = 0;
#endif
    }
    end_state();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
