%x HTMLNOTE HTMLCODE JAVANOTE JAVACODE

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/htmlfilt.l,v 1.26 2001/03/26 13:36:08 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of HTML input
 * text.
 *
 * Based on a version written by Pierre Dittgen (dittgen@univ-mlv.fr)
 */

#include <filters.h>

DefineFilter("html");

static char *Comment_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static int next_state = INITIAL;

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
    flt_puts(text, strlen(text), keyword_attr(temp));

    if (change) {
     	if (ending) {
	    next_state = INITIAL;
	    set_symbol_table(filter_def.filter_name);
	} else {
	    next_state = JAVACODE;
	    set_symbol_table("js");
	}
    }
}

/*
 * JavaScript has a number of syntactic ambiguities built into it.  Let's
 * assume that we're guaranteed that the </server> or </script> command
 * appears at the beginning of a line.
 */
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
    BEGIN(next_state);
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
    yyin = input;

    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr   = class_attr(NAME_IDENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    next_state = INITIAL;
    BEGIN(INITIAL);
    while (yylex() > 0) {
    }
    flt_bfr_error();
}

%}

SPACE		[ \t]

INTEGER		[-+]?([0-9]+)
REAL		[-+]?([0-9]*\.[0-9]+)([eE][+-]?[0-9]+)?
IDENT		[a-zA-Z_][a-zA-Z_0-9-]*
NOSTRBRK	"\\\""
STRING		\"([^"]|{NOSTRBRK})*\"

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

ESCAPED		&{IDENT};

%%

<INITIAL>"<"		{ WriteToken(Keyword_attr); BEGIN(HTMLCODE); }
<HTMLCODE>">"		{ WriteToken(Keyword_attr); BEGIN(next_state); }

<HTMLCODE>(\/)?{IDENT}	{ html_command(yytext); }

<INITIAL>"<!--"		{ BeginQuote(HTMLNOTE, Comment_attr); }
<HTMLNOTE>[\n]		{ flt_bfr_append(yytext, yyleng); }
<HTMLNOTE>[^\n-]+	{ flt_bfr_append(yytext, yyleng); }
<HTMLNOTE>[-]+[^-\>\n]*	{ flt_bfr_append(yytext, yyleng); }
<HTMLNOTE>[-]+"->"	{ FinishQuote(INITIAL); }

<INITIAL>{ESCAPED}	|
<HTMLCODE>{STRING}	{ WriteToken(String_attr); }

<JAVACODE>{IDENT}	{ WriteToken(keyword_attr(yytext)); }
<JAVACODE>{STRINGS}	{ WriteToken(String_attr); }
<JAVACODE>{INTEGER}	{ WriteToken(Number_attr); }
<JAVACODE>"//"[^\n]*	{ WriteToken(Comment_attr); }

<JAVACODE>^{SPACE}*"<"{SPACE}*(\/)?{IDENT}{SPACE}*">" { may_resume_html(yytext); }

<JAVACODE>"/*"		{ BeginQuote(JAVANOTE, Comment_attr); }
<JAVANOTE>\n		{ flt_bfr_append(yytext, yyleng); }
<JAVANOTE>[^*\n]+	{ flt_bfr_append(yytext, yyleng); }
<JAVANOTE>[*]+[^*/]*	{ flt_bfr_append(yytext, yyleng); }
<JAVANOTE>[*]+"/"	{ FinishQuote(JAVACODE); }
