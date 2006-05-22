%s PRETEXT NOTE TEXT

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/fdl-filt.l,v 1.12 2006/05/21 19:50:19 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of fdl
 * input text.  This treats all of the text area as quoted, except for
 * out-of-bounds text.  Fields are shown as variable.
 */

#include <filters.h>

DefineFilter("fdl");

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

<INITIAL,TEXT>^{BLANK}	{ ECHO; }

<INITIAL,TEXT>{KEYWORD}	{ handle_keyword(yytext); }
<PRETEXT>{REMAINDER}	{ ECHO;
			  if (next_state == TEXT) {
				BEGIN(TEXT);
			  } else {
				BEGIN(INITIAL);
			  }
			}

<NOTE>[\n]		{ ECHO; BEGIN(INITIAL); }
<NOTE>{REMAINDER}	{ WriteToken(Comment_attr);
			  BEGIN(INITIAL); }

<TEXT>[_]+		{ WriteToken(Ident2_attr); }
<TEXT>[^$_\r\n]+	{ WriteToken(String_attr); }

%%

static void
handle_keyword(char *name)
{
    flt_puts(name, strlen(name), keyword_attr(name));
    next_state = INITIAL;
    if (!strcmp(name, "$rem")) {
	BEGIN(NOTE);
    } else if (!strcmp(name, "$text")) {
	BEGIN(PRETEXT);
	next_state = TEXT;
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
    yyin = inputs;

    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    String_attr  = class_attr(NAME_LITERAL);

    next_state = INITIAL;

    BEGIN(INITIAL);
    while (yylex() > 0) {
    }
}
