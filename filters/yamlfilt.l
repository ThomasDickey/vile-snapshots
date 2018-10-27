%pointer
%s NORMAL WHITE

%{

/*
 * $Id: yamlfilt.l,v 1.3 2018/10/27 12:42:03 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of YAML program.
 *
 * YAML comments begin either at the start of a line, or after whitespace.
 * This filter uses lex start-conditions NORMAL/WHITE to handle the latter.
 */

#define UNUSED_POP_STATE
#include <filters.h>

DefineFilter(yaml);

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

#include <fltstack.h>

%}

SSTRING		'[^']+'
DSTRING		\"([^"\r\n]|(""))*\"

%%

[[:space:]]+	{ ECHO; BEGIN(WHITE); }

<WHITE>"#"[^\r\n]*	{ WriteToken(Comment_attr); } 

<WHITE>{SSTRING}	|
<WHITE>{DSTRING}	{ WriteToken(String_attr); BEGIN(NORMAL); }

.		{ ECHO; BEGIN(NORMAL); }

%%

#include <fltstack.h>

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
}

static void
do_filter(FILE *inputs)
{
    InitLEX(inputs);
    Action_attr = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    begin_state(WHITE);
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
