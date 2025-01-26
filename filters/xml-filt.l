%pointer
%x XMLTAG XMLATTR XMLCDATA XMLPI COMMENT INLINE USERSYM PARMREF

%{

/*
 * $Id: xml-filt.l,v 1.55 2025/01/26 17:07:24 tom Exp $
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

DefineOptFilter(xml, "AM");

#define SYMS_XMLNS	"xmlns"
#define SYMS_PROPS	"xmlprops"

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
static int ant_macro, ant_state, mvn_state;

#include <fltstack.h>

static const char *xml_keyword_attr(const char *name);
static void xml_close(char *text);
static void xml_command(char *text, int len);
static void xml_prefixed_property(char *text, int len);
static void xml_property(char *text, int len);
static void add_xmlprop(char *text, int len);
static void show_xml_string(char *text, int len);
static const char *get_xmlprop(char *text, int len);

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

<INITIAL,INLINE>"${"	{
			  if (FltOptions('A') || FltOptions('M')) {
			    WriteToken(Action_attr);
			    push_state(USERSYM);
			    flt_bfr_begin("");
			  } else {
			    ECHO;
			  }
			}
<USERSYM>"}"		{
			  flt_bfr_finish();
			  pop_state();
			  WriteToken(Action_attr);
    			}
<USERSYM>"@{"		{
			  if (ant_macro) {
			    push_state(PARMREF);
			    flt_bfr_embed(yytext, yyleng, Action_attr);
			  } else {
			    flt_bfr_append(yytext, yyleng);
			  }
    			}
<USERSYM>"@"[^{]	|
<USERSYM>[^@}]+		{
			  flt_bfr_embed(yytext, yyleng, get_xmlprop(yytext, yyleng));
			}
<PARMREF>"}"		{
			  pop_state();
			  flt_bfr_embed(yytext, yyleng, Action_attr);
    			}
<PARMREF>[^}]+		{
			  flt_bfr_embed(yytext, yyleng, get_xmlprop(yytext, yyleng));
    			}

<XMLTAG>"![CDATA["	{ WriteToken(Keyword_attr); push_state(XMLCDATA); flt_bfr_begin(String_attr); }

<XMLTAG>(\/)?">"	|
<XMLPI>"?>"		|
<XMLATTR>(\/)?">"	{ ant_state = 0; xml_close(yytext); pop_state(); }

<XMLTAG>(\/)?{XMLTAG}	{ char tablename[LEN_TABLENAME];

			  flt_setup_symbols(default_table);
			  xml_command(yytext, yyleng);

			  sprintf(tablename, FMT_PROP, default_table);
			  flt_setup_symbols(tablename);

			  new_state(XMLATTR);
			}

<XMLCDATA>"]]>"		{ flt_bfr_finish();
			  WriteToken3(Keyword_attr, yyleng - 1);
		          flt_puts(">", 1, Action_attr);
			  pop_state();
			}
<XMLCDATA>[\r\n]	|
<XMLCDATA>.		{ flt_bfr_append(yytext, yyleng); }

<INITIAL,INLINE>"<!--"	{ PushQuote(COMMENT, Comment_attr); }
<COMMENT>[\r\n]		|
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
<XMLPI>{STRING}		{ WriteToken(String_attr); }
<XMLATTR>{STRING}	{
			    if (FltOptions('A') || FltOptions('M')) {
			        if (ant_state == 2) {
				    add_xmlprop(yytext+1, yyleng-2);
				    WriteToken(String_attr);
				} else if (ant_state == 0 && mvn_state == 0) {
				    show_xml_string(yytext, yyleng);
				} else {
				    WriteToken(String_attr);
				}
			    } else {
				WriteToken(String_attr);
			    }
			}

<INITIAL,INLINE>{ENTITY} |
<XMLTAG>{NUMBER}	|
<XMLPI>{NUMBER}		|
<XMLATTR>{NUMBER}	{ WriteToken(Number_attr); }

%%

#include <fltstack.h>

static void
add_xmlprop(char *text, int len)
{
    const char *old_table = get_symbol_table();
    char save = text[len];
    text[len] = '\0';
    if (ant_macro) {
	set_symbol_table(default_table);
	insert_keyword(text, Keyword_attr, 0);
    } else {
	set_symbol_table(SYMS_PROPS);
	insert_keyword(text, Ident2_attr, 0);
    }
    set_symbol_table(old_table);
    text[len] = save;
}

static const char *
get_xmlprop(char *text, int len)
{
    const char *old_table = get_symbol_table();
    char save = text[len];
    const char *result;

    text[len] = '\0';
    set_symbol_table(SYMS_PROPS);
    result = xml_keyword_attr(text);
    set_symbol_table(old_table);
    if (isEmpty(result) && FltOptions('M')) {
	const char *attr = get_keyword_attr(text);
	if (!isEmpty(attr))
	    result = Ident2_attr;
    }
    text[len] = save;

    return result;
}

/*
 * Use this for string literals which may have an embedded ant/maven property.
 */
static void
show_xml_string(char *text, int len)
{
    while (len > 0) {
	char *first;
	char *last;
	int skip;

	if ((first = strstr(text, "${")) == NULL ||
	    (last = strstr(first, "}")) == NULL) {
	    flt_puts(text, len, String_attr);
	    break;
	}
	if (first != text) {
	    flt_puts(text, (int) (first - text), String_attr);
	}
	flt_puts(first, 2, Action_attr);
	skip = (int) (last - (first + 2));
	if (skip > 0)
	    flt_puts(first + 2, skip, Ident2_attr);
	flt_puts(last, 1, Action_attr);
	skip = (int) (last - text + 1);
	text += skip;
	len -= skip;
    }
}

static const char *
xml_keyword_attr(const char *name)
{
    const char *attr = get_keyword_attr(name);

    if (attr == NULL)
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
    flt_puts(text, len, get_xmlprop(text, len));

    if (ant_state) {
	/*
	 * Works, but is a little looser than needed for these cases:
	 * <property name="xxx" />
	 * <require property="xxx" />
	 */
	if (!strcmp(text, "name") ||
	    !strcmp(text, "property")) {
	    ant_state = 2;
	} else {
	    ant_state = 1;
	}
    }
}

static void
xml_command(char *text, int len)
{
    int ending = (text[0] == '/');
    char *name = ending ? text + 1 : text;
    const char *attr = xml_keyword_attr(name);

    if (FltOptions('A')) {
	if (!strcmp(text + ending, "macrodef")) {
	    ant_state = !ending;
	    ant_macro = !ending;
	}
	if (!strcmp(text + ending, "property") ||
	    !strcmp(text + ending, "require")) {
	    ant_state = !ending;
	    ant_macro = 0;
	}
    }
    if (FltOptions('M')) {
	if (!strcmp(text + ending, "properties")) {
	    mvn_state = !ending;
	} else if (mvn_state) {
	    if (!ending)
		add_xmlprop(text, len);
	    attr = Ident2_attr;
	}
    }

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
	char *marker = strchr(text, ':');
	if (marker != NULL && marker[1] != '\0') {
	    const char *old_table = get_symbol_table();
	    char new_table[LEN_TABLENAME];

	    *marker = '\0';
	    set_symbol_table(SYMS_XMLNS);
	    flt_puts(text, (int) strlen(text), get_keyword_attr(name));
	    set_symbol_table(old_table);

	    sprintf(new_table, FMT_XMLNS, name);

	    flt_putc(':');

	    text = marker + 1;
	    set_symbol_table(new_table);
	    attr = get_keyword_attr(text);
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

    Action_attr = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr = class_attr(NAME_IDENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);
    Error_attr = class_attr(NAME_ERROR);

    set_symbol_table(default_table);

    ant_macro = 0;
    ant_state = 0;
    mvn_state = 0;

    begin_state(INITIAL);
    RunLEX();
    flt_bfr_error();

    if (Tags != NULL) {
	while (Num_tags > 0) {
	    --Num_tags;
	    flt_error("expected tag:%s", Tags[Num_tags].name);
	    free(Tags[Num_tags].name);
	}
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
