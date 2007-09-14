%s NORMAL ACTION REGISTER SETTINGS SUBMODE_NAME SUBMODE_VALS

%o 6000
%a 3000

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/vilefilt.l,v 1.41 2007/09/14 00:12:04 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of vile macros
 * - T.Dickey
 */

#include <filters.h>

#if defined(_estruct_h) && !SMALLER && defined(HAVE_CONFIG_H)
#define HAVE_DIRECTIVE 0
#else
#define HAVE_DIRECTIVE 0
#endif

DefineFilter("vile");

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static int prepro_with = 0;

static char * command_attr(char *text);
static int is_funckey(const char *text);
static int is_prepro(char *text, int len);
static int is_prepro_endwith(int code);
static int is_prepro_with(int code);

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
DECIMAL		[[:digit:]]+
HEXADECIMAL	0[xX][[:xdigit:]_]+
NUMBER		({DECIMAL}|{HEXADECIMAL})
NAME		[[:alpha:]_][[:alnum:]_]*(-[[:alnum:]_]+)*([!])?

PREPROC		\~[[:lower:]]+

SEMICMT		;.*
QUOTECMT1	^[ \t]*\"[^\n]*
QUOTECMT2	\"[^"\n]*$

CTRLV		\026
CONTROL		([\001-\007\013\016-\037])({CTRLV}.|\\.|[^ \t\n])*

VISIBLE		.|^.
BINDING		(M-)?((^[AX]-(^)?{VISIBLE})|((#|FN)-?{VISIBLE})|^.)

SSTRING		(\'(\\.|[^'\n])*\')
DSTRING		\"(\\.|[^\\"\n])*\"

FUNCTION	&[[:alpha:]]+

PARAMETER	[$]{DECIMAL}
VARIABLE	[$%]{NAME}

REGISTER	[<.;"[:alnum:]]
NONREGISTER	[^<.;"[:alnum:][:space:]]

%%

{QUOTECMT1}	{ WriteToken(Comment_attr); }
SPACE		;
{SEMICMT}	{ WriteToken(Comment_attr); }
^S$		{ WriteToken(Keyword_attr); }
^[CBSL]:	{ WriteToken(Keyword_attr); BEGIN(ACTION); }

<REGISTER>{REGISTER} { WriteToken(String_attr); BEGIN(NORMAL); }
<REGISTER>{NONREGISTER} { WriteToken(Error_attr); BEGIN(NORMAL); }

<ACTION>[^\n:]*	{ WriteToken(Action_attr); BEGIN(NORMAL); }

{PREPROC}	{ int prepro = is_prepro(yytext, yyleng);
		  if (prepro >= 0) {
		      WriteToken(Preproc_attr);
		      if (is_prepro_with(prepro)) {
			  BEGIN(NORMAL);
			  prepro_with = 1;
		      } else if (is_prepro_endwith(prepro)) {
			  BEGIN(NORMAL);
			  prepro_with = 0;
		      }
		  } else {
		      flt_error("unexpected directive");
		      WriteToken(Error_attr);
		  }
	        }

{NAME}		{ char *attr = "";
		  attr = is_funckey(yytext)
			 ? String_attr
			 : command_attr(yytext);
		  if (vile_keywords) {
		      switch (YYSTATE) {
		      case NORMAL:
			  if (vl_is_setting(yytext)) {
			      BEGIN(SETTINGS);
			  } else if (vl_is_register(yytext)) {
			      BEGIN(REGISTER);
			  }
			  break;
		      case REGISTER:
			  attr = String_attr;
			  if (yyleng != 1) attr = Error_attr;
			  BEGIN(NORMAL);
			  break;
		      case SUBMODE_NAME:
			  if (*attr == 0) attr = "U";
			  BEGIN(SUBMODE_VALS);
			  break;
		      case SUBMODE_VALS:
			  /* FALLTHRU */
		      case SETTINGS:
			  if (vl_lookup_mode(yytext) >= 0) {
			      attr = Keyword_attr;
			  } else {
			      attr = Error_attr;
			      flt_error("unknown mode");
			  }
			  break;
		      }
		      if (vl_is_submode(yytext)) {
			  BEGIN(SUBMODE_NAME);
		      }
		  }
		  WriteToken(attr);
		}

{FUNCTION}	{ char *attr = "";
		  if (vile_keywords) {
		      if (vl_lookup_func(yytext) >= 0) {
			  attr = Ident2_attr;
		      } else {
			  flt_error("unknown function");
			  attr = Error_attr;
		      }
		  }
		  WriteToken(attr);
		}
{PARAMETER}	{ WriteToken(Ident2_attr); }
{VARIABLE}	{ char *attr = Ident2_attr;
		  if (vile_keywords) {
		      if (yytext[0] == '$') {
			  if (vl_lookup_var(yytext) < 0) {
			      flt_error("unknown variable");
			      attr = Error_attr;
			  }
		      }
		  }
		  WriteToken(attr);
		}
{NUMBER}	{ WriteToken(Number_attr); }

\\[\n]		{ WriteToken(String_attr); }
[\n]		{ ECHO; if (!prepro_with) { BEGIN(NORMAL); } }

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
is_prepro_with(int code)
{
    int rc = 0;
#if HAVE_DIRECTIVE
    if (code == D_WITH
	|| code == D_ELSEWITH)
	rc = 1;
#else
    (void) code;
#endif
    return rc;
}

static int
is_prepro_endwith(int code)
{
    int rc = 0;
#if HAVE_DIRECTIVE
    if (code == D_ENDWITH)
	rc = 1;
#else
    (void) code;
#endif
    return rc;
}

static int
is_prepro(char *text, int len)
{
    if (vile_keywords) {
	char *next = skip_blanks(text);
	return dname_to_dirnum(&next, (len - (next - text)));
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
    (void) before;
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
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    prepro_with = 0;
    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
}
