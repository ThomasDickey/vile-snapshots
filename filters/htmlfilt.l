%pointer
%x HTMLNOTE HTMLCODE HTMLQUO1 HTMLQUO2 AT_CMDS JS_NOTE JS_CODE ANY_MODE ASP_CODE ASP_CMT0 JSP_CODE JSP_CMT0 JSP_CMT1
%o 10000

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/htmlfilt.l,v 1.74 2009/02/21 02:20:34 tom Exp $
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

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static char *dft_language;	/* asp or jsp or nothing */
static char *use_language;	/* vbscript, javascript, C#, Java, etc */

static int got_language;	/* parse-state for language=string */
static int got_language2;
static int got_equals;		/* parse state to detect unquoted values */

static int next_state;

static int language_code(void);
static void html_command(char *text);
static void html_quotes(char *text, int len, int state, int which);
static void may_resume_html(char *text);
static void resume_attr(void);
static void setup_language(char *name);
static void using_language(void);

%}

SPACE		[ \t]

INTEGER		[-+]?([[:digit:]]+)
REAL		[-+]?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?
HEXNUM		"#"[xX]?[[:xdigit:]]+
IDENT		[[:alpha:]_][[:alnum:]_-]*

STRING		\"([^"])*\"

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

URI_REF		"%"[[:xdigit:]][[:xdigit:]]
CHR_REF		&({IDENT}|{HEXNUM});

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
			  set_symbol_table(filter_def.filter_name);
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
			  set_symbol_table(filter_def.filter_name);
			}
<ANY_MODE>[\r\n] 	|
<ANY_MODE>.		{ flt_bfr_append(yytext, yyleng); }

<JSP_CODE,JSP_CMT0,JSP_CMT1>"%>" {
			  flt_bfr_finish();
			  WriteToken(Action_attr);
			  pop_state();
			  resume_attr();
			  set_symbol_table(filter_def.filter_name);
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
			  set_symbol_table(filter_def.filter_name);
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
			  new_state(HTMLCODE);
			  got_equals = 0;
			}
<HTMLCODE>"<"		{
			  WriteToken(Error_attr);
			  flt_error("Missing \">\"");
			  got_equals = 0;
			}
<HTMLCODE>"="		{ ECHO;
			  if (got_language2) ++got_language2;
			  ++got_equals;
			}
<HTMLCODE>(\/)?">"	{ WriteToken(Keyword_attr); new_state(next_state); }

<HTMLCODE>(\/)?{IDENT}	{
    			  html_command(yytext);
			  got_equals = 0;
			}

<INITIAL>"<!--"		{ BeginQuote(HTMLNOTE, Comment_attr); }
<HTMLNOTE>[\r\n]	{ flt_bfr_append(yytext, yyleng); }
<HTMLNOTE>[^\r\n-]+	{ flt_bfr_append(yytext, yyleng); }
<HTMLNOTE>[-]+[^-\>\r\n]*	{ flt_bfr_append(yytext, yyleng); }
<HTMLNOTE>[-]+"->"	{ FinishQuote(INITIAL); }

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

static void
html_command(char *text)
{
    int ending = (text[0] == '/');
    char *temp = lowercase_of(ending ? text + 1 : text);
    int change = !got_equals
		 && (!strcmp(temp, "server")
		  || !strcmp(temp, "script"));

    got_language2 = 0;
    if (change) {
	set_symbol_table(filter_def.filter_name);
    } else if (!strcmp(temp,"language")) {
	got_language2 = 1;
    }
    flt_puts(text, strlen(text), ci_keyword_attr(temp));

    if (change) {
     	if (ending) {
	    next_state = INITIAL;
	    set_symbol_table(filter_def.filter_name);
	} else {
	    next_state = JS_CODE;
	    set_symbol_table("js");
	}
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
	unsigned need = used + len + 1;

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
    new_state(next_state);
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
	char *lowered = lowercase_of(name);

	if (*lowered == '"' || *lowered == '\'') {
	    dequote(lowered);
	}

	if (!strcmp(lowered, "c#"))
	    result = "cs";
	else if (!strcmp(lowered, "java"))
	    result = "java";
	else if (!strncmp(lowered, "javascript", 10))
	    result = "js";
	else if (!strcmp(lowered, "vbscript"))
	    result = "vbs";
    }

    return result;
}

static void
setup_language(char *name)
{
    char *keys = language2keys(name);

    if (keys != 0 && !set_symbol_table(name)) {
	use_language = keys;
	flt_make_symtab(keys);
	flt_read_keywords(MY_NAME);
	flt_read_keywords(keys);
	set_symbol_table(filter_def.filter_name);
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
