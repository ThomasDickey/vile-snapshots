%pointer
%x XMLTAG XMLATTR XMLCDATA XMLPI COMMENT INLINE XQ_NOTE

%{

/*
 * $Id: xq-filt.l,v 1.22 2025/01/26 17:07:24 tom Exp $
 *
 * XQuery is case-sensitive
 * XQuery elements, attributes, and variables must be valid XML names
 * An XQuery string value can be in single or double quotes
 * An XQuery variable is defined with a $ followed by a name, e.g. $bookstore
 * XQuery comments are delimited by (: and :), e.g. (: XQuery Comment :)
 *
 * The URI of the XQuery function namespace is:
 *    http://www.w3.org/2005/02/xpath-functions
 *
 *  The default prefix for the function namespace is fn:.
 *
 *  Tip:  Functions are often called with the fn:  prefix, such as fn:string().
 *  However, since fn:  is the default prefix of the namespace, the function
 *  names do not need to be prefixed when called.
 */

#include <filters.h>

DefineFilter(xq);

#define SYMS_XMLNS	"xmlns"

#define LEN_TABLENAME	80
#define FMT_XMLNS	"xmlns_%.70s"
#define FMT_PROP	"%.70s_prop"

#ifdef FLEX_DEBUG
#define DBG_HTML(params) if (yy_flex_debug) fprintf params
#else
#define DBG_HTML(params)	/* nothing */
#endif

static char *Action_attr;
static char *Comment_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;
static char *Error_attr;

typedef struct {
    char *name;
    int line;
    int col;
} XMLTAGS;

static XMLTAGS *Tags;
static unsigned Num_tags = 0;
static int In_error = 0;
static unsigned curly_braces;

#include <fltstack.h>

static const char *prefixed_attr(const char *, char **, int);
static const char *unprefixed_attr(char *);
static const char *xml_keyword_attr(const char *name);
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

<INITIAL,INLINE>{XMLTAG}	{
			  char *text = yytext;
			  const char *attr = "";
			  if (Num_tags <= 0 || curly_braces > Num_tags) {
			      attr = get_keyword_attr(yytext);
			      if (isEmpty(attr)) {
				  attr = prefixed_attr(yytext, &text, 0);
				  if (isEmpty(attr)) {
				      attr = unprefixed_attr(yytext);
				      if (isEmpty(attr)) {
					  attr = Ident_attr;
				      }
				  }
			      }
			  }
			  flt_puts(text, (int) strlen(text), attr);
			}
<INITIAL,INLINE>"$"{IDENT}	{
			  const char *attr = "";
			  if (Num_tags <= 0 || curly_braces > Num_tags) {
			      attr = Ident2_attr;
			  }
			  WriteToken(attr);
			}
<INITIAL,INLINE>"{"	{ curly_braces = Num_tags + 1; WriteToken(Action_attr); }
<INITIAL,INLINE>"}"	{ curly_braces = Num_tags; WriteToken(Action_attr); }

<INITIAL,INLINE>"(:"	{ PushQuote(XQ_NOTE, Comment_attr); }
<XQ_NOTE>[\n]		|
<XQ_NOTE>[^\r\n:]+	|
<XQ_NOTE>[:]+[^)]	{ flt_bfr_append(yytext, yyleng); }
<XQ_NOTE>":)"		{ PopQuote(); }

<INITIAL,INLINE>"<!--"	{ PushQuote(COMMENT, Comment_attr); }
<COMMENT>[\n]		|
<COMMENT>[^\r\n-]+	|
<COMMENT>[-]+[^-\>\r\n]*	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>[-]+"->"	{ PopQuote(); }

<XMLATTR>"["		{ WriteToken(Action_attr); push_state(INLINE); }
<INLINE>"]"		{ WriteToken(Action_attr); pop_state(); }

<XMLPI>{IDENT}		|
<XMLATTR>{IDENT}	{ xml_property(yytext, yyleng); }
<XMLATTR>{XMLTAG}	{ xml_prefixed_property(yytext, yyleng); }

<INITIAL,INLINE>{STRINGS}	{
			  const char *attr = "";
			  if (Num_tags <= 0 || curly_braces > Num_tags) {
			      attr = String_attr;
			  }
			  WriteToken(attr);
			}
<INLINE>{VALUE}		|
<XMLTAG>{VALUE}		|
<XMLATTR>{VALUE}	|
<XMLTAG>{STRING}	|
<XMLPI>{STRING}		|
<XMLATTR>{STRING}	{ WriteToken(String_attr); }

<INITIAL,INLINE>{NUMBER}	{
			  const char *attr = "";
			  if (Num_tags <= 0 || curly_braces > Num_tags) {
			      attr = Number_attr;
			  }
			  WriteToken(attr);
			}
<INITIAL,INLINE>{ENTITY} |
<XMLTAG>{NUMBER}	|
<XMLPI>{NUMBER}		|
<XMLATTR>{NUMBER}	{ WriteToken(Number_attr); }

%%

#include <fltstack.h>

static const char *
xml_keyword_attr(const char *name)
{
    const char *attr = get_keyword_attr(name);

    if (isEmpty(attr))
	attr = Ident_attr;
    return attr;
}

static void
xml_prefixed_property(char *text, int len)
{
#define STR_XMLNS "xmlns:"
#define LEN_XMLNS 6

    if (len > LEN_XMLNS && !strncmp(text, STR_XMLNS, (size_t) LEN_XMLNS)) {
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

static const char *
prefixed_attr(const char *name, char **text, int ending)
{
    const char *attr = "";
    char *marker = strchr(name, ':');

    if (marker != NULL && marker[1] != '\0') {
	const char *old_table = get_symbol_table();
	char new_table[LEN_TABLENAME];

	*marker = '\0';
	set_symbol_table(SYMS_XMLNS);
	attr = get_keyword_attr(name);
	if (isEmpty(attr))
	    attr = Ident_attr;
	if (ending)
	    flt_puts("/", 1, attr);
	flt_puts(name, (int) strlen(name), attr);
	set_symbol_table(old_table);

	sprintf(new_table, FMT_XMLNS, name);

	flt_putc(':');

	*text = marker + 1;
	set_symbol_table(new_table);
	attr = get_keyword_attr(*text);
	*marker = ':';

	set_symbol_table(old_table);
    }
    return attr;
}

static const char *
unprefixed_attr(char *name)
{
    const char *attr = "";

    const char *old_table = get_symbol_table();
    char new_table[LEN_TABLENAME];

    sprintf(new_table, FMT_XMLNS, "fn");

    set_symbol_table(new_table);
    attr = get_keyword_attr(name);

    set_symbol_table(old_table);

    return attr;
}

static void
xml_command(char *text)
{
    int ending = (text[0] == '/');
    char *name = ending ? text + 1 : text;
    const char *attr = xml_keyword_attr(name);

    if (!ending) {
	size_t need = sizeof(XMLTAGS) * Num_tags;
	static size_t have = 0;

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
	attr = prefixed_attr(name, &text, ending);
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

    Action_attr = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr = class_attr(NAME_IDENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);
    Error_attr = class_attr(NAME_ERROR);

    set_symbol_table(default_table);
    curly_braces = 0;
    In_error = 0;

    begin_state(INITIAL);
    RunLEX();
    flt_bfr_error();

    if (Tags != NULL) {
	if (Num_tags > 1)
	    flt_error("expected tag:%s", Tags[Num_tags - 1].name);
	while (Num_tags > 0)
	    free(Tags[--Num_tags].name);
#if NO_LEAKS
	free(Tags);
	Tags = NULL;
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
