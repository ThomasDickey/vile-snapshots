%pointer
%x HTMLNOTE HTMLCODE HTMLINCL HTMLQUO1 HTMLQUO2 AT_CMDS JS_NOTE JS_CODE ANY_MODE ASP_CODE ASP_CMT0 JSP_CODE JSP_CMT0 JSP_CMT1 CSS_CODE CSS_NOTE
%o 10000

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/htmlfilt.l,v 1.96 2009/05/17 20:20:56 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of HTML input
 * text.
 *
 * Based on a version written by Pierre Dittgen (dittgen@univ-mlv.fr)
 * ----------------------------------------------------------------------------
 * HTML tags are case-insensitive - XML (see xml-filt.l) is case-sensitive.
 *
 * Some HTML files (whether suffixed with ".asp" or ".jsp") may contain markers
 * for those languages.  However, the default language for the markers depends
 * on how the file is used - so we add the "-a" and "-j" options to set the
 * default language.  If neither is given, the filter will show the marked text
 * as a quoted string.
 *
 * Even without the markers, we can still have <script> tags, specifying a
 * language, e.g.,
	<script language="vbscript">xxxx</script>
 *
 * ----------------------------------------------------------------------------
 * ASP is here:
 *	http://www.w3schools.com/asp/default.asp
 *
 * We need this information:
 *
 * <%@ directives %>
 *	The "page" directive can contain a "language" value, e.g., with
 *	"vbscript" value (the default), or "javascript".  That may be in an
 *	ordinary directive (no "page").
 *
 * <% code %>
 *
 * Comments with directives:
 * <!-- #include file="filename" -->
 * <!-- #include virtual="filename" -->
 *
 * The "global.asa" file may also contain other directives in comments, e.g.
 *	<!--METADATA TYPE="TypeLib"
 *	file="filename"
 *	uuid="typelibraryuuid"
 *	version="versionnumber"
 *	lcid="localeid"
 *	-->
 * ----------------------------------------------------------------------------
 * ASP.NET is a superset (more/less...) of ASP
 *
 * Like JSP, these are recognized:
 * <%= expression %>
 * <%-- comments --%>
 *
 * Data binding: http://msdn.microsoft.com/en-us/library/ms178366.aspx
 * <%# Bind code %>
 * <%# Eval code %>
 *
 * ----------------------------------------------------------------------------
 * JSP is here:
 *	http://java.sun.com/products/jsp/
 * It lists "Final Release of JavaServer Pages Specification".
 * JSP - http://java.sun.com/products/jsp/html/jspbasics.fm3.html
 *
 * We need this information:
 *
 * <%@ directives %>
 *	The "page" directive can contain a "language" value, e.g., with "java"
 *	value (the default).
 *
 * <%! variables; %>
 * <%= expression %>
 * <%-- comments --%>
 * <% any other code %>
 * <jsp:subtag values %>
 *
 * There can also be scripting elements, in either the tags or data, using
 * "${...}" or "#{...}" delimiters.
 * ----------------------------------------------------------------------------
 */

#include <filters.h>
#include <fltstack.h>
#undef YY_NO_UNPUT

DefineOptFilter("html", "aj");

#define SYMS_CS        "cs"
#define SYMS_CSS       "css"
#define SYMS_CSS_UNITS "cssUnits"
#define SYMS_HTMLPROP  "htmlprop"
#define SYMS_JAVA      "java"
#define SYMS_JS        "js"
#define SYMS_VBS       "vbs"

#ifdef FLEX_DEBUG
#define DBG_HTML(msg) if (yy_flex_debug) fprintf(stderr, "%s\n", msg)
#else
#define DBG_HTML(msg) /* nothing */
#endif

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static char *dft_language;	/* asp or jsp or nothing */
static char *use_language;	/* vbscript, javascript, C#, Java, etc */

static int got_tag;		/* parse state for <tag> vs <tag name=value> */
static int got_language;	/* parse-state for language=string */
static int got_language2;
static int got_equals;		/* parse state to detect unquoted values */

static int next_state;
static int css_state;

static int language_code(void);
static void begin_htmlcode(void);
static void css_hash(char *text, int length);
static void css_string_escapes(char *text, int length);
static void css_units(char *text, int length);
static void finish_htmlcode(void);
static void html_command(const char *text);
static void html_quotes(char *text, int len, int state, int which);
static void may_resume_html(char *text);
static void resume_attr(void);
static void setup_language(char *name);
static void using_language(void);

%}

NL		\n|\r\n|\r|\f
SPACE		[ \t]
NONASCII	[^\001-\126]

INTEGER		[-+]?([[:digit:]]+)
REAL		[-+]?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?
HEXNUM		"#"[xX]?[[:xdigit:]]+
IDENT		[[:alpha:]_][[:alnum:]_.-]*

HTML_IDENT	[.!?]?[[:alpha:]_][[:alnum:]_.-]*

STRING		\"([^"])*\"

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

URI_REF		"%"[[:xdigit:]][[:xdigit:]]
CHR_REF		&({IDENT}|{HEXNUM});

CSS_UNICODE	\\([[:xdigit:]]{1,6})
CSS_ESCAPE	{CSS_UNICODE}|(\\[^\n\r\f[:xdigit:]])

CSS_NMSTART	[[:alpha:]_]|{NONASCII}|{CSS_ESCAPE}
CSS_NMCHAR	[[:alnum:]_-]|{NONASCII}|{CSS_ESCAPE}
CSS_IDENT	[-]?[[:alpha:]_-][[:alnum:]_-]*
CSS_NAME	{CSS_NMCHAR}+

CSS_HASH	#{CSS_NAME}

CSS_UNITS	("%"|[[:alpha:]]+)

CSS_STRING1	\'({CSS_ESCAPE}|\\{NL}|[^\n\r\f\\'])*\'
CSS_STRING2	\"({CSS_ESCAPE}|\\{NL}|[^\n\r\f\\"])*\"
CSS_STRING	({CSS_STRING1}|{CSS_STRING2})

CSS_INVALID1	\'({CSS_ESCAPE}|\\{NL}|[^\n\r\f\\'])*
CSS_INVALID2	\"({CSS_ESCAPE}|\\{NL}|[^\n\r\f\\"])*
CSS_INVALID	({CSS_INVALID1}|{CSS_INVALID2})

%%

<INITIAL,HTMLCODE,HTMLNOTE,HTMLQUO1,HTMLQUO2>"<%"([@!=#]|"--")? {
			  flt_bfr_finish();
			  if (dft_language != 0) {
			      WriteToken3(Action_attr, 2);
			      switch (yytext[2]) {
			      case '-':
				  push_state(ANY_MODE);
				  unput('-');
				  unput('-');
				  flt_bfr_begin(Comment_attr);
				  break;
			      case '@':
				  WriteToken2(Action_attr, 2);
				  push_state(AT_CMDS);
				  break;
			      default:
				  WriteToken2(Action_attr, 2);
				  push_state(language_code());
				  using_language();
				  break;
			      }
			  } else {
			      WriteToken(Action_attr);
			      push_state(ANY_MODE);
			      flt_bfr_begin(String_attr);
			  }
			}

<AT_CMDS>"%>"		{
			  flt_bfr_finish();
			  WriteToken(Action_attr);
			  pop_state();
			  resume_attr();
			}
<AT_CMDS>{IDENT}	{
			  got_language = !strcmp("language", lowercase_of(yytext));
			  WriteToken("U"); /* FIXME - asp.key, jsp.key ? */
			}
<AT_CMDS>"="		{ ECHO; if (got_language) ++got_language; }
<AT_CMDS>{STRING}	{
			  WriteToken(String_attr);
			  if (got_language > 1) {
			      setup_language(yytext);
			  }
			}
<AT_CMDS>[\r\n] 	|
<AT_CMDS>.		{ ECHO; }

<ANY_MODE>"%>" {
			  flt_bfr_finish();
			  WriteToken(Action_attr);
			  pop_state();
			  resume_attr();
			}
<ANY_MODE>[\r\n] 	|
<ANY_MODE>.		{ flt_bfr_append(yytext, yyleng); }

<JSP_CODE,JSP_CMT0,JSP_CMT1>"%>" {
			  flt_bfr_finish();
			  WriteToken(Action_attr);
			  pop_state();
			  resume_attr();
			}
<JSP_CODE>{IDENT}	{ WriteToken(keyword_attr(yytext)); }
<JSP_CODE>{STRINGS}	{ WriteToken(String_attr); }
<JSP_CODE>{INTEGER}	|
<JSP_CODE>0[xX][[:xdigit:]]+	|
<JSP_CODE>{REAL}	{ WriteToken(Number_attr); }
<JSP_CODE>"//"		{ BeginQuote(JSP_CMT0, Comment_attr); }
<JSP_CODE>"/*"		{ BeginQuote(JSP_CMT1, Comment_attr); }
<JSP_CODE>[\r\n] 	|
<JSP_CODE>.		{ ECHO; }

<JSP_CMT0>[\r\n]	{ flt_bfr_finish(); ECHO; new_state(JSP_CODE); }
<JSP_CMT0>.		{ flt_bfr_append(yytext, yyleng); }

<JSP_CMT1>"*/"		{ FinishQuote(JSP_CODE); }
<JSP_CMT1>[\r\n]	|
<JSP_CMT1>.		{ flt_bfr_append(yytext, yyleng); }

<ASP_CODE,ASP_CMT0>"%>" {
			  flt_bfr_finish();
			  WriteToken(Action_attr);
			  pop_state();
			  resume_attr();
			}
<ASP_CODE>{IDENT}	{ WriteToken(ci_keyword_attr(yytext)); }
<ASP_CODE>{STRING}	{ WriteToken(String_attr); }
<ASP_CODE>{INTEGER}	|
<ASP_CODE>[&][hH][[:xdigit:]]+	|
<ASP_CODE>{REAL}	{ WriteToken(Number_attr); }
<ASP_CODE>"'"		{ BeginQuote(ASP_CMT0, Comment_attr); }
<ASP_CODE>[\r\n] 	|
<ASP_CODE>.		{ ECHO; }

<ASP_CMT0>[\r\n]	{ flt_bfr_finish(); ECHO; new_state(ASP_CODE); }
<ASP_CMT0>.		{ flt_bfr_append(yytext, yyleng); }

<INITIAL,CSS_CODE>"<"	{
			  WriteToken(Keyword_attr);
			  begin_htmlcode();
			}
<HTMLCODE>"<"		{
			  WriteToken(Error_attr);
			  flt_error("Missing \">\"");
			  got_equals = 0;
			}
<HTMLCODE>"="		{
			  ECHO;
			  if (got_language2) ++got_language2;
			  ++got_equals;
			}
<HTMLCODE>(\/)?">" 	{
			  WriteToken(Keyword_attr);
			  finish_htmlcode();
			}

<HTMLCODE>(\/)?{HTML_IDENT} {
    			  html_command(yytext);
			  got_equals = 0;
			}

<INITIAL>"<!--"		{ PushQuote(HTMLNOTE, Comment_attr); }
<HTMLNOTE>[\r\n]	{ flt_bfr_append(yytext, yyleng); }
<HTMLNOTE>[^\r\n-]+	{ flt_bfr_append(yytext, yyleng); }
<HTMLNOTE>[-]+[^-\>\r\n]*	{ flt_bfr_append(yytext, yyleng); }
<HTMLNOTE>[-]+"->"	{ PopQuote(); }

<INITIAL,HTMLCODE>{URI_REF}	|
<INITIAL,HTMLCODE>{CHR_REF}	{ WriteToken(Number_attr); }

<HTMLCODE>[']		{ html_quotes(yytext, yyleng, HTMLQUO1, 0); }
<HTMLQUO1>{URI_REF}	|
<HTMLQUO1>{CHR_REF}	{ html_quotes(yytext, yyleng, HTMLQUO1, 1); }
<HTMLQUO1>[^']		{ html_quotes(yytext, yyleng, HTMLQUO1, 2); }
<HTMLQUO1>[']		{ html_quotes(yytext, yyleng, HTMLCODE, 3); }

<HTMLCODE>["]		{ html_quotes(yytext, yyleng, HTMLQUO2, 0); }
<HTMLQUO2>{URI_REF}	|
<HTMLQUO2>{CHR_REF}	{ html_quotes(yytext, yyleng, HTMLQUO2, 1); }
<HTMLQUO2>[^"]		{ html_quotes(yytext, yyleng, HTMLQUO2, 2); }
<HTMLQUO2>["]		{ html_quotes(yytext, yyleng, HTMLCODE, 3); }

<HTMLCODE>{REAL}	|
<HTMLCODE>{INTEGER}	|
<HTMLCODE>{HEXNUM}	{ WriteToken(Number_attr); }

<CSS_CODE>[;:(){}\[\]]	{ ECHO; css_state = yytext[0]; }
<CSS_CODE>@{CSS_IDENT}	|
<CSS_CODE>{CSS_IDENT}	{ WriteToken(ci_keyword_attr(yytext)); }
<CSS_CODE>{INTEGER}{CSS_UNITS}	{ css_units(yytext, yyleng); }
<CSS_CODE>{CSS_STRING}	{ css_string_escapes(yytext, yyleng); }
<CSS_CODE>{CSS_HASH}	{ css_hash(yytext, yyleng); }
<CSS_CODE>{REAL}	{ WriteToken(Number_attr); }
<CSS_CODE>{CSS_INVALID}	{ WriteToken(Error_attr); }
<CSS_CODE>"<!--"	{ WriteToken(Comment_attr);
			  push_state(CSS_CODE);
			}
<CSS_CODE>"-->"		{ WriteToken(Comment_attr);
			  pop_state();
			}

<CSS_CODE>"/*"		{ WriteToken(Comment_attr); BEGIN(CSS_NOTE); }
<CSS_NOTE>[^*]*		{ WriteToken(Comment_attr); }
<CSS_NOTE>"*"+[^*/]*	{ WriteToken(Comment_attr); }
<CSS_NOTE>"*"+"/"	{ WriteToken(Comment_attr); BEGIN(CSS_CODE); }


<JS_CODE>{IDENT}	{ WriteToken(keyword_attr(yytext)); }
<JS_CODE>{STRINGS}	{ WriteToken(String_attr); }
<JS_CODE>{INTEGER}	{ WriteToken(Number_attr); }
<JS_CODE>"<!--"[^\r\n]* { WriteToken(Comment_attr); }
<JS_CODE>"//"[^\r\n]*	{
			  char *last = strstr(yytext, "-->");
			  char *next = 0;
			  if (last != 0) {
			      last += 3;
			      if (*last != '\0') {
				  next = strdup(last);
			      }
			  }
			  if (next != 0) {
			      int used = last - (char *) yytext;
			      WriteToken3(Comment_attr, used);
			      used = strlen(next);
			      while (used-- > 0)
				  unput(next[used]);
			      free(next);
			  } else {
			      WriteToken(Comment_attr);
			  }
			}

<JS_CODE>"<"{SPACE}*(\/)?{IDENT}{SPACE}*">" {
			  got_equals = 0;
    			  may_resume_html(yytext);
			}

<JS_CODE>"/*"		{ BeginQuote(JS_NOTE, Comment_attr); }
<JS_NOTE>\n		{ flt_bfr_append(yytext, yyleng); }
<JS_NOTE>[^*\r\n]+	{ flt_bfr_append(yytext, yyleng); }
<JS_NOTE>[*]+[^*/]*	{ flt_bfr_append(yytext, yyleng); }
<JS_NOTE>[*]+"/"	{ FinishQuote(JS_CODE); }

%%

#include <fltstack.h>

static void
html_command(const char *text)
{
    int ending = (text[0] == '/');
    const char *theText = (ending ? text + 1 : text);
    int exact = ispunct(CharOf(*theText));
    const char *temp = exact
		       ? theText
		       : lowercase_of(theText);
    const char *attr = 0;
    int change = 0;

    if (!got_tag && !got_equals) {
	if (!strcmp(temp, "server")
	    || !strcmp(temp, "script")) {
	    change = JS_CODE;
	} else if (!strcmp(temp, "style")) {
	    change = CSS_CODE;
	}
    }
    got_language2 = 0;
    if (change) {
	set_symbol_table(default_table);
    } else if (!strcmp(temp,"language")) {
	got_language2 = 1;
    }
    attr = exact
	   ? keyword_attr(temp)
	   : ci_keyword_attr(temp);
    if (!attr || !*attr)
	flt_error("Unknown keyword: %s", temp);
    flt_puts(text, strlen(text), attr);

    if (got_tag && !strcmp(temp, "src")) {
	next_state = HTMLINCL;
	DBG_HTML("next_state HTMLINCL");
    } else {
	switch (change) {
	case JS_CODE:
	    if (ending) {
		next_state = INITIAL;
		DBG_HTML("next_state INITIAL");
	    } else if (next_state != HTMLINCL) {
		next_state = JS_CODE;
		DBG_HTML("next_state JS_CODE");
	    }
	    break;
	case CSS_CODE:
	    if (ending) {
		next_state = INITIAL;
		DBG_HTML("next_state INITIAL");
	    } else {
		next_state = CSS_CODE;
		DBG_HTML("next_state CSS_CODE");
	    }
	    break;
	}
    }
    if (!got_tag++) {
	flt_setup_symbols(SYMS_HTMLPROP);
    }
}

static void
html_quotes(char *text, int len, int state, int which)
{
    switch (which) {
    case 0:
	new_state(state);
	flt_bfr_begin(String_attr);
	flt_bfr_append(text, len);
	break;
    case 1:
	flt_bfr_embed(text, len, Number_attr);
	break;
    case 2:
	flt_bfr_append(text, len);
	break;
    case 3:
	flt_bfr_append(text, len);
	flt_bfr_finish();
	new_state(state);
	break;
    }

    /*
     * Check for language="foo" (the reason for this function).
     */
    if (got_language2) {
	static char *temp;
	static unsigned used = 0;
	unsigned need = (temp ? strlen(temp) : 0) + len + 1;

	temp = do_alloc(temp, need, &used);
	if (temp != 0) {
	    if (which == 0) {
		*temp = 0;
		strcat(temp, text);
	    } else {
		strcat(temp, text);
		if (which == 3) {
		    got_language2 = 0;
		    setup_language(temp);
		    using_language();
		}
	    }
	}
    }
}

static void
resume_attr(void)
{
    switch (FLT_STATE) {
    case HTMLNOTE:
    case JS_NOTE:
	flt_bfr_begin(Comment_attr);
	break;
    case HTMLQUO1:
    case HTMLQUO2:
	flt_bfr_begin(String_attr);
	break;
    }
    set_symbol_table(default_table);
}

static void
begin_htmlcode(void)
{
    new_state(HTMLCODE);
    got_equals = 0;
    got_tag = 0;
}

static void
finish_htmlcode(void)
{
    if (next_state == HTMLINCL)
	next_state = INITIAL;
    new_state(next_state);

    switch (next_state) {
    case CSS_CODE:
	DBG_HTML("setting symbols for CSS");
	flt_setup_symbols(SYMS_CSS);
	break;
    case JS_CODE:
	DBG_HTML("setting symbols for JS");
	flt_setup_symbols(SYMS_JS);
	break;
    default:
    case INITIAL:
    case HTMLINCL:
	DBG_HTML("setting symbols for HTML");
	set_symbol_table(default_table);
	break;
    }
    got_tag = 0;
}

static void
may_resume_html(char *text)
{
    char *left = strchr(text, '<');
    char *next;
    int save;

    flt_puts(text, left - text, "");
    flt_puts(left, 1, Keyword_attr);
    next = ++left;
    while (isspace(CharOf(*next)))
	next++;
    flt_puts(left, next-left, "");
    left = next;
    while (!isspace(CharOf(*next)) && *next != '>')
	next++;
    save = *next;
    *next = 0;
    html_command(left);
    *next = save;
    flt_puts(next, strlen(next), Keyword_attr);
    finish_htmlcode();
}

static void
css_hash(char *text, int length)
{
    if (css_state == ':') {
	char *attr = (length == 4 || length == 7) ? Number_attr : Error_attr;
	flt_puts(text, length, attr);
    } else {
	flt_puts(text, length, ci_keyword_attr(text + 1));
    }
}

static void
css_string_escapes(char *text, int length)
{
    while (length > 0) {
	int n;
	int found;
	for (n = 0, found = -1; n < length; ++n) {
	    if (text[n] == '\\') {
		found = n;
		break;
	    }
	}
	if (found >= 0) {
	    if (length > 1) {
		flt_puts(text, found, String_attr);
		text += found;
		length -= found;
		if (isxdigit(CharOf(text[1]))) {
		    for (n = 2, found = 2; n < length; ++n) {
			if (isxdigit(CharOf(text[n]))) {
			    found = n + 1;
			} else {
			    break;
			}
		    }
		} else {
		    found = 2;
		}
		flt_puts(text, found, Number_attr);
		text += found;
		length -= found;
	    } else {
		break;
	    }
	} else {
	    break;
	}
    }
    flt_puts(text, length, String_attr);
}

static void
css_units(char *text, int length)
{
    int digits = length;
    const char *attr;

    while (digits > 0) {
	if (isdigit(CharOf(text[digits - 1])))
	    break;
	--digits;
    }

    flt_puts(text, digits, Number_attr);
    if (digits != length) {
	if (text[digits] == '%') {
	    flt_puts(text + digits, length - digits, "");
	} else {
	    flt_setup_symbols(SYMS_CSS);
	    /* units are normally loaded as part of css.key */
	    flt_setup_symbols(SYMS_CSS_UNITS);
	    if ((attr = ci_keyword_attr(text + digits)) == 0)
		attr = Error_attr;
	    flt_puts(text + digits, length - digits, attr);
	    set_symbol_table(SYMS_CSS);
	}
    }
}

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
}

static void
dequote(char *buffer)
{
    char delim = *buffer;
    int n;

    for (n = 0; (buffer[n] = buffer[n + 1]) != '\0'; ++n) {
	if (buffer[n] == delim) {
	    break;
	}
    }
    buffer[n] = '\0';
}

static char *
language2keys(char *name)
{
    char *result = 0;

    if (name != 0) {
	const char *lowered = lowercase_of(name);
	unsigned len = strlen(lowered);
	char temp[80];

	if ((len + 1) >= sizeof(temp))
	    len = (sizeof(temp) - 1);

	strncpy(temp, lowered, len);
	if (*temp == '"' || *temp == '\'') {
	    dequote(temp);
	}

	if (!strcmp(lowered, "c#"))
	    result = SYMS_CS;
	else if (!strcmp(lowered, "java"))
	    result = SYMS_JAVA;
	else if (!strncmp(lowered, "javascript", 10))
	    result = SYMS_JS;
	else if (!strcmp(lowered, "vbscript"))
	    result = SYMS_VBS;
    }

    return result;
}

static void
setup_language(char *name)
{
    char *keys = language2keys(name);

    if (keys != 0 && !set_symbol_table(keys)) {
	use_language = keys;
	flt_setup_symbols(keys);
	set_symbol_table(default_table);
    }
}

static int
language_code(void)
{
    int result = ANY_MODE;

    if (use_language != 0) {
	if (*use_language == 'v') {	/* vbs = vbscript */
	    result = ASP_CODE;		/* Basic-syntax */
	} else {
	    result = JSP_CODE;		/* C-syntax */
	}
    }
    return result;
}

static void
using_language(void)
{
    if (use_language != 0) {
	set_symbol_table(use_language);
    }
}

static void
do_filter(FILE *inputs)
{
    InitLEX(inputs);

    dft_language = 0;
    got_language = 0;
    use_language = 0;

    got_equals = 0;
    got_language2 = 0;

    if (FltOptions('a')) {
	dft_language = "asp";
	setup_language("vbscript");
    } else if (FltOptions('j')) {
	dft_language = "jsp";
	setup_language("java");
    }

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    Ident_attr   = class_attr(NAME_IDENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    next_state = INITIAL;
    begin_state(INITIAL);
    css_state = -1;
    RunLEX();
    flt_bfr_error();
    end_state();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
