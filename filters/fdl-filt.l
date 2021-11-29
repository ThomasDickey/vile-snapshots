%pointer
%s PRETEXT NOTE sTEXT

%{

/*
 * $Id: fdl-filt.l,v 1.21 2013/12/02 01:32:53 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of fdl
 * input text.  This treats all of the text area as quoted, except for
 * out-of-bounds text.  Fields are shown as variable.
 */

#include <filters.h>

DefineFilter(fdl);

static char *Comment_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *String_attr;

static int next_state;

static void handle_keyword(char *name);

%}

BLANK		[ \t]*
REMAINDER	[^\r\n]*

KEYWORD		$[[:alpha:]_][[:alnum:]_]*

%%

<INITIAL,sTEXT>^{BLANK}	{ ECHO; }

<INITIAL,sTEXT>{KEYWORD}	{ handle_keyword(yytext); }
<PRETEXT>{REMAINDER}	{ ECHO;
			  if (next_state == sTEXT) {
				BEGIN(sTEXT);
			  } else {
				BEGIN(INITIAL);
			  }
			}

<NOTE>[\n]		{ ECHO; BEGIN(INITIAL); }
<NOTE>{REMAINDER}	{ WriteToken(Comment_attr);
			  BEGIN(INITIAL); }

<sTEXT>[_]+		{ WriteToken(Ident2_attr); }
<sTEXT>[^$_\r\n]+	{ WriteToken(String_attr); }

%%

static void
handle_keyword(char *name)
{
    flt_puts(name, (int) strlen(name), get_keyword_attr(name));
    next_state = INITIAL;
    if (!strcmp(name, "$rem")) {
	BEGIN(NOTE);
    } else if (!strcmp(name, "$text")) {
	BEGIN(PRETEXT);
	next_state = sTEXT;
    } else {
	BEGIN(INITIAL);
    }
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
    Ident2_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    String_attr = class_attr(NAME_LITERAL);

    next_state = INITIAL;

    BEGIN(INITIAL);
    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
