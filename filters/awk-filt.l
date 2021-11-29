%pointer
%s NORMAL CODE ARGS

%{

/*
 * $Id: awk-filt.l,v 1.34 2013/12/02 01:32:54 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of awk
 * input text.
 */

#define FLTSTACK_EXTRA	int parens;

#include <filters.h>
#include <fltstack.h>

DefineFilter(awk);

static void PushState(int);
static void PopState(void);

static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static int braces;

%}

BLANK		[ \t]*

DSTRING		\"(\\.|[^"\\\r\n]|\\\n)*\"

KEYWORD		[[:alpha:]_][[:alnum:]_]*
IDENT		\$[[:alnum:]_]+

SIGN		[-+]
INTEGER		{SIGN}?([[:digit:]]+)
REAL		{SIGN}?([[:digit:]]*\.[[:digit:]]+)([eE]{SIGN}?[[:digit:]]+)?

PATTERN		"/"(\\.|("["("^")?([][])*([^][])*"]")|[^\r\n/])*"/"

%%

<NORMAL,ARGS>{PATTERN}	{ WriteToken(String_attr); }

<CODE>"/"		{ ECHO; }

<CODE,NORMAL>"("	{ ECHO;
			  PushState(ARGS);
			  FLTSTACK_THIS.parens = 1;
			}
<ARGS>"("		{ ECHO;
			  ++FLTSTACK_THIS.parens;
			}
<ARGS>")"		{ ECHO;
			  if (--FLTSTACK_THIS.parens == 0) {
			      PopState();
			  }
			}
<CODE,NORMAL>")"	{ WriteToken(Error_attr); }

<NORMAL>"{"		{ ECHO; if (!braces++) { begin_state(CODE); } }
<CODE>"{"		{ ECHO; braces++; }
<ARGS>"{"		{ WriteToken(Error_attr); }

<ARGS>"}"		{ WriteToken(Error_attr); }
<CODE>"}"		{ ECHO; if (--braces == 0) { begin_state(NORMAL); } }
<NORMAL>"}"		{ WriteToken(Error_attr); }

^{BLANK}*#!.*$		{ WriteToken(Preproc_attr); }

{KEYWORD}		{ WriteToken(get_keyword_attr(yytext)); }

"#".*$			{ WriteToken(Comment_attr); }

{INTEGER}	|
{REAL}			{ WriteToken(Number_attr); }

{IDENT}			{ WriteToken(Ident_attr); }

{DSTRING}		{ WriteToken(String_attr); }

%%

#include <fltstack.h>

static void
PushState(int value)
{
    push_state(value);
}

static void
PopState(void)
{
    pop_state();
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

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    begin_state(NORMAL);
    braces = 0;
    FLTSTACK_THIS.parens = 0;

    RunLEX();
    end_state();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
