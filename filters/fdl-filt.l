%s PRETEXT NOTE TEXT

%{
/*
 * Filter to add vile "attribution" sequences to selected bits of fdl
 * input text.  This treats all of the text area as quoted, except for
 * out-of-bounds text.  Fields are shown as variable.
 */

#include <filters.h>

char *filter_name = "fdl";

static char *Comment_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *String_attr;

static int next_state;

static void
handle_keyword(char *name)
{
    write_token(yyout, name, strlen(name), keyword_attr(name));
    next_state = INITIAL;
    if (!strcmp(name, "$rem")) {
	BEGIN(NOTE);
    } else if (!strcmp(name, "$text")) {
	BEGIN(PRETEXT);
	next_state = TEXT;
    } else if (!strcmp(name, "$endtext")) {
	BEGIN(PRETEXT);
    }
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
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    String_attr  = class_attr(NAME_LITERAL);

    next_state = INITIAL;

    while (yylex() > 0) {
    }
}

%}

BLANK		[ \t]*
REMAINDER	[^\n]*

KEYWORD		$[a-zA-Z_][a-zA-Z_0-9]*

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

<NOTE>{REMAINDER}	{ WriteToken(Comment_attr);
			  BEGIN(INITIAL); }

<TEXT>[_]+		{ WriteToken(Ident2_attr); }
<TEXT>[^$_\n]+		{ WriteToken(String_attr); }
