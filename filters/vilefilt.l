%pointer
%s NORMAL ACTION REGISTER SETTINGS MAJORMODE_NAME SUBMODE_NAME SUBMODE_VALS WITH_DEFS LITERAL

%o 10000
%a 6000

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/vilefilt.l,v 1.57 2008/01/22 00:30:22 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of vile macros
 * - T.Dickey
 */

#include <filters.h>

#define FLTSTACK_EXTRA int with_state; int with_active;
#define FltStack_WITH_STATE   FLTSTACK_THIS.with_state
#define FltStack_WITH_ACTIVE  FLTSTACK_THIS.with_state
#include <fltstack.h>

#if defined(_estruct_h) && !SMALLER
#define HAVE_DIRECTIVE 1
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

static char * command_attr(char *text);
static int is_funckey(const char *text);
static void handle_keyword(char *text, int len);
static void handle_newline(void);
static void handle_prepro(char *text, int len);

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
^[CBSL]:	{ WriteToken(Keyword_attr); new_state(ACTION); }

<REGISTER>{REGISTER} { WriteToken(String_attr); new_state(NORMAL); }
<REGISTER>{NONREGISTER} { WriteToken(Error_attr); new_state(NORMAL); }

<ACTION>[^\n:]*	{ WriteToken(Action_attr); new_state(NORMAL); }

{PREPROC}	{ handle_prepro(yytext, yyleng); }

{NAME}		{ handle_keyword(yytext, yyleng); }

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

\\[\n]		{ WriteToken(String_attr); handle_newline(); }
[\n]		{ ECHO; handle_newline(); }

\r		|
\\.		|
{CONTROL}	|
{BINDING}	|
{SSTRING}	|
{DSTRING}	{ WriteToken(String_attr); }

{QUOTECMT2}	{ WriteToken(Comment_attr); }

<LITERAL>[^\n]*	{ WriteToken(String_attr); }

<SUBMODE_VALS,SETTINGS>[=]	{ WriteToken(Action_attr); }

"!"[^\n]*	{ WriteToken(Action_attr); }

%%

static void
handle_keyword(char *text, int len)
{
    char *attr = is_funckey(text)
		 ? String_attr
		 : command_attr(text);

#if HAVE_DIRECTIVE
    if (vile_keywords) {
	const void *cmd = vl_lookup_cmd(text);

	switch (cur_state) {
	case NORMAL:
	    if (vl_is_setting(cmd)) {
		new_state(SETTINGS);
	    } else if (vl_check_cmd(cmd, USEREG)) {
		new_state(REGISTER);
	    } else if (vl_check_cmd(cmd, EXTRA)) {
		new_state(LITERAL);
	    }
	    break;
	case REGISTER:
	    attr = String_attr;
	    if (yyleng != 1) attr = Error_attr;
	    new_state(NORMAL);
	    break;
	case MAJORMODE_NAME:
	    if (*attr == 0) attr = "U";
	    new_state(NORMAL);
	    break;
	case SUBMODE_NAME:
	    if (*attr == 0) attr = "U";
	    new_state(SUBMODE_VALS);
	    break;
	case SUBMODE_VALS:
	    /* FALLTHRU */
	case SETTINGS:
	    if (vl_lookup_mode(text) >= 0) {
		attr = Keyword_attr;
	    } else if (text[0] == '$' && vl_lookup_var(text) >= 0) {
		attr = Ident2_attr;
	    } else {
		/* FIXME - values to set can be a variety of enum's or
		 * special-syntax.  Just ignore them.
		 */
		attr = "";
	    }
	    break;
	}
	if (vl_is_majormode(cmd)) {
	    new_state(MAJORMODE_NAME);
	} else if (vl_is_submode(cmd)) {
	    new_state(SUBMODE_NAME);
	}
    }
#endif
    flt_puts(text, len, attr);
}

static void
handle_newline(void)
{
    if (vile_keywords && FLTSTACK_OK && FltStack_WITH_ACTIVE) {
	if (FltStack_WITH_STATE < 0) {
	    FltStack_WITH_STATE = cur_state;
	}
	new_state(FltStack_WITH_STATE);
    }
}

static void
handle_prepro(char *text, int len)
{
    char *attr = "";
    if (vile_keywords) {
	char *next = skip_blanks(text);

	attr = Preproc_attr;
	switch (dname_to_dirnum(&next, (len - (next - text)))) {
#if HAVE_DIRECTIVE
	case D_WITH:
	    push_state(WITH_DEFS);
	    if (FLTSTACK_OK) {
		FltStack_WITH_ACTIVE = 1;
		FltStack_WITH_STATE = -1;
	    } else {
		attr = Error_attr;
	    }
	    break;
	case D_ELSEWITH:
	    if (FLTSTACK_OK) {
		FltStack_WITH_ACTIVE = 1;
		FltStack_WITH_STATE = -1;
	    } else {
		attr = Error_attr;
	    }
	    break;
	case D_ENDWITH:
	    pop_state();
	    break;
#endif
	case D_UNKNOWN:
	    flt_error("unexpected directive");
	    attr = Error_attr;
	    break;
	default:
	    break;
	}
    }
    new_state(NORMAL);
    flt_puts(text, len, attr);
}

static char *
command_attr(char *text)
{
    if (vile_keywords) {
	static char none[] = "";
	return vl_lookup_cmd(text) != 0 ? Keyword_attr : none;
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

    begin_state(NORMAL);
    FltStack_WITH_ACTIVE = 0;
    FltStack_WITH_STATE = NORMAL;
    while (yylex() > 0) {
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
