%pointer
%x HTMLNOTE HTMLCODE HTMLINCL HTMLQUO1 HTMLQUO2 AT_CMDS JS_NOTE JS_CODE ANY_MODE ASP_CODE ASP_CMT0 JSP_CODE JSP_CMT0 JSP_CMT1 CSS_CODE CSS_NOTE PHP_CODE PHP_COMMENT PHP_CMT0 PHP_HEREDOC PHP_NOWDOC
%o 10000
%e 2000
%a 4000
%p 3000
%k 2000

%{

/*
 * $Id: htmlfilt.l,v 1.128 2025/01/27 23:30:37 tom Exp $
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
 * PHP is here:
 *	http://www.php.net
 * ----------------------------------------------------------------------------
 */

#include <filters.h>
#include <fltstack.h>
#undef YY_NO_UNPUT

DefineOptFilter(html, "aj");

#define SYMS_CS        "cs"
#define SYMS_CSS       "css"
#define SYMS_CSS_UNITS "cssUnits"
#define SYMS_HTMLPROP  "htmlprop"
#define SYMS_JAVA      "java"
#define SYMS_JS        "js"
#define SYMS_PHP       "php"
#define SYMS_VBS       "vbs"

typedef enum {
    CODE_NONE = 0
    ,CODE_CS
    ,CODE_CSS
    ,CODE_CSS_UNITS
    ,CODE_HTMLPROP
    ,CODE_JAVA
    ,CODE_JS
    ,CODE_PHP
    ,CODE_VBS
} CODE_SYMS;

#ifdef FLEX_DEBUG
#define DBG_HTML(msg) if (yy_flex_debug) fprintf(stderr, "%s\n", msg)
#else
#define DBG_HTML(msg)		/* nothing */
#endif

#define isQuote(ch) (ch != 0 && strchr("'", ch) != NULL)

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static const char *dft_language;	/* asp or jsp or nothing */
static const char *use_language;	/* vbscript, javascript, C#, Java, etc */

static int in_tag;		/* parse-state for "<" vs "<tag" */
static int got_tag;		/* parse state for <tag> vs <tag name=value> */
static int got_language;	/* parse-state for language=string */
static int got_language2;
static int got_equals;		/* parse state to detect unquoted values */

static int next_state;
static int css_state;

static char *here_tag;
static size_t here_len;
static int here_exp;

static CODE_SYMS language2code(const char *name);
static int code2state(CODE_SYMS code);
static int got_here(char *text, int length);
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
static void save_here(char *text, int length);
static void setup_language(const char *name);
static void using_language(void);

%}

NL		\n|\r\n|\r|\f
SPACE		[ \t]
BLANK		[[:blank:]]*

SIGN		[-+]
INTEGER		{SIGN}?([[:digit:]]+)
REAL		{SIGN}?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?
HEXNUM		"#"[xX]?[[:xdigit:]]+
IDENT		[[:alpha:]_][[:alnum:]_.-]*

HTML_IDENT	[.!?]?[[:alpha:]_][[:alnum:]_.-]*

PHP_IDENT	[[:alpha:]_][[:alnum:]_]*
PHP_DECIMAL	[[:digit:]_]+
PHP_OCTAL	0[0-7_]+
PHP_HEXADECIMAL	0x[[:xdigit:]_]+
PHP_NUMBER	{SIGN}?({PHP_DECIMAL}|{PHP_OCTAL}|{PHP_HEXADECIMAL}|{REAL})

STRING		\"([^"])*\"

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

URI_REF		"%"[[:xdigit:]][[:xdigit:]]
CHR_REF		&({IDENT}|{HEXNUM});

CSS_UNICODE	\\([[:xdigit:]]{1,6})
CSS_ESCAPE	{CSS_UNICODE}|(\\[^\n\r\f[:xdigit:]])

CSS_NMSTART	[[:alpha:]_]|{CSS_ESCAPE}
CSS_NMCHAR	[[:alnum:]_-]|{CSS_ESCAPE}
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
			  if (dft_language != NULL) {
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
			  got_language = !ci_compare("language", yytext);
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

<INITIAL>"<"		{
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
			  ++got_equals;
			}
<HTMLCODE>(\/)?">" 	{
			  WriteToken(Keyword_attr);
			  finish_htmlcode();
			}

<HTMLCODE>(\/){HTML_IDENT} {
    			  html_command(yytext);
			  got_equals = 0;
			}

<HTMLCODE>"?" {
			  if (!in_tag) {
			      WriteToken(Action_attr);
			      new_state(PHP_CODE);
			      setup_language("php");
			      using_language();
			  } else {
			      ECHO;
			  }
			  ++in_tag;
			}

<HTMLCODE>{HTML_IDENT} {
			  if (!in_tag
			      && !ci_compare(yytext, "?php")) {
			      WriteToken(Action_attr);
			      new_state(PHP_CODE);
			      setup_language("php");
			      using_language();
			  } else {
			      html_command(yytext);
			      got_equals = 0;
			  }
			  ++in_tag;
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

<CSS_CODE>[,;:(){}\[\]]	{ ECHO; css_state = yytext[0]; }
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

<CSS_CODE>"</"{IDENT}	{
 			  WriteToken(Keyword_attr);
			  next_state = INITIAL;
 			  begin_htmlcode();
 			}

<JS_CODE>{IDENT}	{ WriteToken(keyword_attr(yytext)); }
<JS_CODE>{STRINGS}	{ WriteToken(String_attr); }
<JS_CODE>{INTEGER}	{ WriteToken(Number_attr); }
<JS_CODE>"<!--"([^\r\n]*"-->")?	{ WriteToken(Comment_attr); }
<JS_CODE>"//"[^\r\n]*	{
			  char *last = strstr(yytext, "-->");
			  char *next = NULL;
			  if (last != NULL) {
			      last += 3;
			      if (*last != '\0') {
				  next = strdup(last);
			      }
			  }
			  if (next != NULL) {
			      int used = (int) (last - (char *) yytext);
			      WriteToken3(Comment_attr, used);
			      used = (int) strlen(next);
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
<JS_NOTE>[\r\n]		{ flt_bfr_append(yytext, yyleng); }
<JS_NOTE>[^*\r\n]+	{ flt_bfr_append(yytext, yyleng); }
<JS_NOTE>[*]+[^*/]*	{ flt_bfr_append(yytext, yyleng); }
<JS_NOTE>[*]+"/"	{ FinishQuote(JS_CODE); }

<PHP_CODE,PHP_CMT0>"?>" {
			  flt_bfr_finish();
			  WriteToken(Action_attr);
			  pop_state();
			  resume_attr();
			}

<PHP_CODE>"${"{PHP_IDENT}"}"	|
<PHP_CODE>"$"{PHP_IDENT}	{ WriteToken(Ident2_attr); }
<PHP_CODE>{PHP_IDENT}		{ WriteToken(get_keyword_attr(yytext)); }
<PHP_CODE>";"		{ ECHO; }

<PHP_CODE>"/*"		{ BeginQuote(PHP_COMMENT, Comment_attr); }
<PHP_COMMENT>[^*]*		{ flt_bfr_append(yytext, yyleng); }
<PHP_COMMENT>"*"+[^*/]*	{ flt_bfr_append(yytext, yyleng); }
<PHP_COMMENT>"*"+"/"	{ FinishQuote(PHP_CODE); }

<PHP_CODE>"#"		|
<PHP_CODE>"//"		{ BeginQuote(PHP_CMT0, Comment_attr); }
<PHP_CMT0>[\r\n]	{ flt_bfr_finish(); ECHO; new_state(PHP_CODE); }
<PHP_CMT0>.		{ flt_bfr_append(yytext, yyleng); }

<PHP_CODE>{STRINGS}	{ WriteToken(String_attr); }

<PHP_CODE>{PHP_NUMBER}	{ WriteToken(Number_attr); }

<PHP_CODE>\<\<\<{BLANK}*{PHP_IDENT} {
			    save_here(yytext, yyleng);
			    push_state(PHP_HEREDOC);
			    BeginQuote(PHP_HEREDOC, String_attr);
			}
<PHP_HEREDOC>^{PHP_IDENT}[;]? {
			    int used = 0;
			    if (got_here(yytext + used, yyleng - used)) {
				flt_bfr_finish();
				pop_state();
			    } else {
				flt_bfr_append(yytext, yyleng);
			    }
			}
<PHP_HEREDOC>"${"{PHP_IDENT}"}"	|
<PHP_HEREDOC>[$]{PHP_IDENT}	{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }
<PHP_HEREDOC>\\.	{ flt_bfr_append(yytext, yyleng); }
<PHP_HEREDOC>[\r\n]	{ flt_bfr_append(yytext, yyleng); }
<PHP_HEREDOC>.		{ flt_bfr_append(yytext, yyleng); }

<PHP_CODE>\<\<\<{BLANK}*'{PHP_IDENT}' {
			    save_here(yytext, yyleng);
			    push_state(PHP_NOWDOC);
			    BeginQuote(PHP_NOWDOC, String_attr);
			}
<PHP_NOWDOC>^{PHP_IDENT}[;]? {
			    int used = 0;
			    if (got_here(yytext + used, yyleng - used)) {
				flt_bfr_finish();
				pop_state();
			    } else {
				flt_bfr_append(yytext, yyleng);
			    }
			}
<PHP_NOWDOC>[\r\n]	{ flt_bfr_append(yytext, yyleng); }
<PHP_NOWDOC>.		{ flt_bfr_append(yytext, yyleng); }

%%

#include <fltstack.h>

/*
 * This is called to process either any of the names in an html command, e.g.,
 * <name>
 * <foo name>
 * <foo bar name=value>
 * </name>
 *
 * The first/last cases are distinguished from the attributes by the got_tag
 * and in_tag flags.
 */
static void
html_command(const char *text)
{
    int ending = (text[0] == '/');
    const char *theText = (ending ? text + 1 : text);
    int exact = ispunct(CharOf(*theText));
    const char *temp = (exact
			? theText
			: lowercase_of(theText));
    const char *attr = NULL;
    int change = 0;

    in_tag = 0;
    if (!got_tag && !got_equals) {
	if (!strcmp(temp, "server")) {
	    /*
	     * Example:
	     * <server>...</server> (nonstandard, but used)
	     */
	    change = JS_CODE;
	} else if (!strcmp(temp, "script")) {
	    /*
	     * There are several choices for the language, e.g., asp,
	     * javascript, php.
	     *
	     * Examples:
	     * <script>...</script> (ambiguous, but sometimes used)
	     * <script type="JavaScript">...</script> (preferred)
	     * <script language="JavaScript">...</script>
	     */
	    change = JS_CODE;
	} else if (!strcmp(temp, "style")) {
	    /*
	     * Example:
	     * <style type="text/css"> (type must be given)
	     */
	    change = CSS_CODE;
	}
    }
    got_language2 = 0;
    if (change) {
	set_symbol_table(default_table);
    } else if (!strcmp(temp, "type")) {
	got_language2 = 1;
    } else if (!strcmp(temp, "language")) {
	got_language2 = 1;
    }
    attr = (exact
	    ? keyword_attr(temp)
	    : ci_keyword_attr(temp));
    if (!attr || !*attr)
	flt_error("Unknown keyword: %s", temp);
    flt_puts(text, (int) strlen(text), attr);

    if (got_tag && !strcmp(temp, "src")) {
	if (next_state == INITIAL) {
	    next_state = HTMLINCL;
	    DBG_HTML("next_state HTMLINCL");
	}
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
     * Check for type="foo" or language="foo" (the reason for this function).
     */
    if (got_language2) {
	static char *temp;
	static size_t used = 0;
	size_t need = (temp ? strlen(temp) : 0) + (size_t) (len + 1);
	CODE_SYMS check;

	temp = do_alloc(temp, need, &used);
	if (temp != NULL) {
	    if (which == 0) {
		*temp = 0;
		strcat(temp, text);
	    } else {
		strcat(temp, text);
		if (which == 3) {
		    got_language2 = 0;
		    if ((check = language2code(temp)) != 0) {
			next_state = code2state(check);
		    }
		}
	    }
	}
    }
}

static void
resume_attr(void)
{
    if (FLTSTACK_OK) {
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
    }
    set_symbol_table(default_table);
}

static void
begin_htmlcode(void)
{
    new_state(HTMLCODE);
    got_equals = 0;
    got_tag = 0;
    in_tag = 0;
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
    case PHP_CODE:
	DBG_HTML("setting symbols for PHP");
	flt_setup_symbols(SYMS_PHP);
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

    if (left != NULL) {
	flt_puts(text, (int) (left - text), "");
	flt_puts(left, 1, Keyword_attr);
	next = ++left;
	while (isspace(CharOf(*next)))
	    next++;
	flt_puts(left, (int) (next - left), "");
	left = next;
	while (!isspace(CharOf(*next)) && *next != '>')
	    next++;
	save = *next;
	*next = 0;
	html_command(left);
	*next = (char) save;
	flt_puts(next, (int) strlen(next), Keyword_attr);
	finish_htmlcode();
    }
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
	    if ((attr = ci_keyword_attr(text + digits)) == NULL)
		attr = Error_attr;
	    flt_puts(text + digits, length - digits, attr);
	    set_symbol_table(SYMS_CSS);
	}
    }
}

/* TODO: php-filt.l */
static int
got_here(char *text, int length)
{
    int pass, j, k;

    for (pass = 0; pass < 2; pass++) {
	for (j = k = 0; j < length; j++) {
	    if (isQuote(text[j])) {
		if (pass) {
		    flt_error("unexpected quote");
		    flt_bfr_embed(text + j, 1, Error_attr);
		}
	    } else if (text[j] == ';') {
		if (pass)
		    flt_bfr_append(text + j, 1);
	    } else {
		if (here_tag != NULL && (text[j] != here_tag[k++]))
		    return 0;
		if (pass)
		    flt_bfr_append(text + j, 1);
	    }
	}
    }
    return 1;
}

static void
save_here(char *text, int length)
{
    char *s;

    here_exp = 1;
    if ((here_tag = do_alloc(here_tag, (size_t) length, &here_len)) != NULL) {
	s = here_tag;
	while (length--) {
	    if (isQuote(*text)) {
		here_exp = 0;
	    } else if (strchr(" \t", *text) != NULL) {
		if (s != here_tag)
		    break;
	    } else if (strchr("<", *text) == NULL) {
		*s++ = *text;
	    }
	    text++;
	}
	*s = 0;
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

static CODE_SYMS
language2code(const char *name)
{
    CODE_SYMS result = CODE_NONE;

    if (name != NULL) {
	char *name2 = strmalloc(name);
	const char *lowered = lowercase_of(name2);
	size_t len = strlen(lowered);
	char temp[80];
	const char *compare = lowered;

	if ((len + 1) >= sizeof(temp))
	    len = (sizeof(temp) - 1);

	strncpy(temp, lowered, len);
	if (*temp == '"' || *temp == '\'') {
	    dequote(temp);
	    compare = temp;
	}

	if (!strcmp(compare, "c#"))
	    result = CODE_CS;
	else if (!strcmp(compare, "css"))
	    result = CODE_CSS;
	else if (!strcmp(compare, "java"))
	    result = CODE_JAVA;
	else if (!strncmp(compare, "javascript", (size_t) 10))
	    result = CODE_JS;
	else if (!strcmp(compare, "php"))
	    result = CODE_PHP;
	else if (!strcmp(compare, "vbscript"))
	    result = CODE_VBS;

	free(name2);
    }

    return result;
}

static const char *
language2keys(const char *name)
{
    const char *result = NULL;

    if (name != NULL) {
	switch (language2code(name)) {
	case CODE_NONE:
	    break;
	case CODE_CS:
	    result = SYMS_CS;
	    break;
	case CODE_CSS:
	    result = SYMS_CSS;
	    break;
	case CODE_CSS_UNITS:
	    result = SYMS_CSS_UNITS;
	    break;
	case CODE_HTMLPROP:
	    result = SYMS_HTMLPROP;
	    break;
	case CODE_JAVA:
	    result = SYMS_JAVA;
	    break;
	case CODE_JS:
	    result = SYMS_JS;
	    break;
	case CODE_PHP:
	    result = SYMS_PHP;
	    break;
	case CODE_VBS:
	    result = SYMS_VBS;
	    break;
	}
    }

    return result;
}

static int
code2state(CODE_SYMS name)
{
    int result = INITIAL;

    if (name != 0) {
	switch (name) {
	case CODE_CS:
	    result = JS_CODE;
	    break;
	case CODE_CSS:
	    result = CSS_CODE;
	    break;
	case CODE_CSS_UNITS:
	    result = CSS_CODE;
	    break;
	case CODE_HTMLPROP:
	    result = HTMLCODE;
	    break;
	case CODE_JAVA:
	    result = JS_CODE;
	    break;
	default:
	case CODE_JS:
	    result = JS_CODE;
	    break;
	case CODE_PHP:
	    result = PHP_CODE;
	    break;
	case CODE_VBS:
	    result = ASP_CODE;
	    break;
	}
    }

    return result;
}

static void
setup_language(const char *name)
{
    const char *keys = language2keys(name);

    if (keys != NULL && !set_symbol_table(keys)) {
	use_language = keys;
	flt_setup_symbols(keys);
	set_symbol_table(default_table);
    }
}

static int
language_code(void)
{
    int result = ANY_MODE;

    if (use_language != NULL) {
	if (*use_language == 'v') {	/* vbs = vbscript */
	    result = ASP_CODE;	/* Basic-syntax */
	} else if (*use_language == 'p') {
	    result = PHP_CODE;	/* PHP-syntax */
	} else {
	    result = JSP_CODE;	/* C-syntax */
	}
    }
    return result;
}

static void
using_language(void)
{
    if (use_language != NULL) {
	DBG_HTML("using_language:");
	DBG_HTML(use_language);
	set_symbol_table(use_language);
    }
}

static void
do_filter(FILE *inputs)
{
    InitLEX(inputs);

    dft_language = NULL;
    got_language = 0;
    use_language = NULL;

    in_tag = 0;
    got_tag = 0;
    got_equals = 0;
    got_language2 = 0;

    if (FltOptions('a')) {
	dft_language = "asp";
	setup_language("vbscript");
    } else if (FltOptions('j')) {
	dft_language = "jsp";
	setup_language("java");
    }

    Action_attr = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

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
