%x HTMLNOTE HTMLCODE HTMLQUOT JS_NOTE JS_CODE JSP_MODE

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/htmlfilt.l,v 1.40 2007/05/08 23:38:55 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of HTML input
 * text.
 *
 * Based on a version written by Pierre Dittgen (dittgen@univ-mlv.fr)
 */

#include <filters.h>
#include <fltstack.h>
#undef YY_NO_UNPUT

DefineOptFilter("html", "j");

static char *Action_attr;
static char *Comment_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static int next_state;

static void html_command(char *text);
static void may_resume_html(char *text);

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

<INITIAL>"<%"		{ WriteToken(Action_attr);
			  push_state(JSP_MODE);
			  flt_bfr_begin(String_attr);
			}
<JSP_MODE>"%>"		{
			  flt_bfr_finish();
			  WriteToken(Action_attr);
			  pop_state();
			}
<JSP_MODE>[\n]		|
<JSP_MODE>.		{ flt_bfr_append(yytext, yyleng); }

<INITIAL>"<"		{ WriteToken(Keyword_attr); new_state(HTMLCODE); }
<HTMLCODE>">"		{ WriteToken(Keyword_attr); new_state(next_state); }

<HTMLCODE>(\/)?{IDENT}	{ html_command(yytext); }

<INITIAL>"<!--"		{ BeginQuote(HTMLNOTE, Comment_attr); }
<HTMLNOTE>[\n]		{ flt_bfr_append(yytext, yyleng); }
<HTMLNOTE>[^\r\n-]+	{ flt_bfr_append(yytext, yyleng); }
<HTMLNOTE>[-]+[^-\>\r\n]*	{ flt_bfr_append(yytext, yyleng); }
<HTMLNOTE>[-]+"->"	{ FinishQuote(INITIAL); }

<INITIAL,HTMLCODE>{URI_REF}	|
<INITIAL,HTMLCODE>{CHR_REF}	{ WriteToken(Number_attr); }

<HTMLCODE>["]		{ BeginQuote(HTMLQUOT, String_attr); }
<HTMLQUOT>{URI_REF}	|
<HTMLQUOT>{CHR_REF}	{ flt_bfr_embed(yytext, yyleng, Number_attr); }
<HTMLQUOT>[^"]		{ flt_bfr_append(yytext, yyleng); }
<HTMLQUOT>["]		{ FinishQuote(HTMLCODE); }

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
			      int used = last - yytext;
			      WriteToken3(Comment_attr, used);
			      used = strlen(next);
			      while (used-- > 0)
				  unput(next[used]);
			      free(next);
			  } else {
			      WriteToken(Comment_attr);
			  }
			}

<JS_CODE>"<"{SPACE}*(\/)?{IDENT}{SPACE}*">" { may_resume_html(yytext); }

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
    int change = !strcmp(temp, "server")
	      || !strcmp(temp, "script");

    if (change) {
	set_symbol_table(filter_def.filter_name);
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
do_filter(FILE *inputs)
{
    yyin = inputs;

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr   = class_attr(NAME_IDENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    next_state = INITIAL;
    begin_state(INITIAL);
    yylex();
    flt_bfr_error();
    end_state();
}
