%s NORMAL ACTION

%o 6000
%a 3000

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/vilefilt.l,v 1.29 2005/02/02 01:58:22 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of vile macros
 * - T.Dickey
 */

#include <filters.h>

DefineFilter("vile");

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static char * command_attr(char *text);
static int is_funckey(const char *text);
static int is_prepro(char *text, int len);

/*
 * With regard to the comment macros defined below, note that QUOTECMT1
 * handles " as cmt delim at BOL.  QUOTECMT2 handles many cases of " as a
 * trailing cmt delim, but will not handle this:
 *
 *    write-message "x" "tricky "cmt"
 *
 * vile's command parser gets this right, but it would appear that the
 * editor's macro language comment syntax cannot be scanned with a
 * context-free tool.
 */

%}

SPACE		[ \t]+
DECIMAL		[0-9]+
HEXADECIMAL	0[xX][0-9a-fA-F_]+
NUMBER		({DECIMAL}|{HEXADECIMAL})
NAME		[a-zA-Z_][a-zA-Z_0-9]*(-[a-zA-Z_0-9]+)*

PREPROC		\~[a-z]+

SEMICMT		;.*
QUOTECMT1	^[ \t]*\"[^\n]*
QUOTECMT2	\"[^"\n]*$

CTRLA		\001
CTRLV		\026
CTRLX		\030
ESCAPE		\033
CONTROL		({CTRLA}|{CTRLX}|{ESCAPE})({CTRLV}.|\\.|[^ \t\n])*

VISIBLE		.|^.
BINDING		(M-)?((^[AX]-(^)?{VISIBLE})|((#|FN)-?{VISIBLE})|^.)

SSTRING_E	(\'[^'\n]*\n)
DSTRING_E	\"(\\.|[^\\"\n])*\n

SSTRING		(\'[^'\n]*\')
DSTRING		\"(\\.|[^\\"\n])*\"

FUNCTION	&[a-zA-Z]+

VARIABLE	[$%]{NAME}

%%

{QUOTECMT1}	{ WriteToken(Comment_attr); }
SPACE		;
{SEMICMT}	{ WriteToken(Comment_attr); }
^S$		{ WriteToken(Keyword_attr); }
^[CBSL]:	{ WriteToken(Keyword_attr); BEGIN(ACTION); }
<ACTION>[^\n:]*	{ WriteToken(Action_attr); BEGIN(NORMAL); }

{PREPROC}	{ if (is_prepro(yytext, yyleng)) {
		      WriteToken(Keyword_attr);
		  } else {
		      flt_error("unexpected directive");
		      WriteToken(Error_attr);
		  }
	        }

{NAME}		{ WriteToken(is_funckey(yytext) ? String_attr : command_attr(yytext)); }

{FUNCTION}	{ WriteToken(vl_lookup_func(yytext) >= 0 ? Ident2_attr : ""); }
{VARIABLE}	{ WriteToken(Ident2_attr); }

{NUMBER}	{ WriteToken(Number_attr); }

{SSTRING_E}	|
{DSTRING_E}	{ WriteToken(Error_attr); }

\r		|
\\.		|
{CONTROL}	|
{BINDING}	|
{SSTRING}	|
{DSTRING}	{ WriteToken(String_attr); }

{QUOTECMT2}	{ WriteToken(Comment_attr); }

"!"[^\n]*	{ WriteToken(Action_attr); }

%%

static int
is_prepro(char *text, int len)
{
    if (vile_keywords) {
	char *next = skip_blanks(text);
	int code = dname_to_dirnum(&next, (len - (next - text)));
	return code >= 0;
    }
    return 1;
}

static char *
command_attr(char *text)
{
    if (vile_keywords) {
	static char none[] = "";
	return engl2fnc(text) != 0 ? Keyword_attr : none;
    }
    return keyword_attr(text);
}

static int
is_funckey(const char *text)
{
    return (!strncmp(text, "FN-", 3));
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
    Error_attr   = class_attr(NAME_ERROR);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
}
