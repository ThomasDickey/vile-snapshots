%x HTMLNOTE HTMLCODE JAVANOTE JAVACODE

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/htmlfilt.l,v 1.15 1999/05/01 02:45:01 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of HTML input
 * text.
 *
 * Based on a version written by Pierre Dittgen (dittgen@univ-mlv.fr)
 */

#include <filters.h>

char *filter_name = "html";

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
	set_symbol_table(filter_name);
    }
    write_token(yyout, text, strlen(text), keyword_attr(temp));

    if (change) {
     	if (ending) {
	    next_state = INITIAL;
	    set_symbol_table(filter_name);
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

    write_token(yyout, text, left - text, "");
    write_token(yyout, left, 1, Keyword_attr);
    next = ++left;
    while (isspace(*next))
	next++;
    write_token(yyout, left, next-left, "");
    left = next;
    while (!isspace(*next) && *next != '>')
	next++;
    save = *next;
    *next = 0;
    html_command(left);
    *next = save;
    write_token(yyout, next, strlen(next), Keyword_attr);
    BEGIN(next_state);
}

void
init_filter(int before GCC_UNUSED)
{
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;

    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr   = class_attr(NAME_IDENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
    exit(0);
}

%}

SPACE		[ \t]

INTEGER		[-+]?([0-9]+)
REAL		[-+]?([0-9]*\.[0-9]+)([eE][+-]?[0-9]+)?
IDENT		[a-zA-Z_][a-zA-Z_0-9]*
NOSTRBRK	"\\\""
STRING		\"([^"]|{NOSTRBRK})*\"

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

%%

<INITIAL>"<"		{ WriteToken(Keyword_attr); BEGIN(HTMLCODE); }
<HTMLCODE>">"		{ WriteToken(Keyword_attr); BEGIN(next_state); }

<HTMLCODE>(\/)?{IDENT}	{ html_command(yytext); }

<INITIAL>"<!--"		{ WriteToken(Comment_attr);
			  BEGIN(HTMLNOTE);           }
<HTMLNOTE>[^-]+		{ WriteString(Comment_attr); }
<HTMLNOTE>"-"+[^->]*	{ WriteString(Comment_attr); }
<HTMLNOTE>"-"+"->"	{ WriteToken(Comment_attr);
			  BEGIN(INITIAL);            }

<HTMLCODE>{STRING}	{ WriteString(String_attr); }

<JAVACODE>{IDENT}	{ WriteToken(keyword_attr(yytext)); }
<JAVACODE>{STRINGS}	{ WriteString(String_attr); }
<JAVACODE>{INTEGER}	{ WriteToken(Number_attr); }
<JAVACODE>"//"[^\n]*	{ WriteToken(Comment_attr); }

<JAVACODE>^{SPACE}*"<"{SPACE}*(\/)?{IDENT}{SPACE}*">" { may_resume_html(yytext); }

<JAVACODE>"/*"		{ WriteToken(Comment_attr); BEGIN(JAVANOTE); }
<JAVANOTE>[^*]*		{ WriteString(Comment_attr); }
<JAVANOTE>"*"+[^*/]*	{ WriteString(Comment_attr); }
<JAVANOTE>"*"+"/"	{ WriteToken(Comment_attr); BEGIN(JAVACODE); }
