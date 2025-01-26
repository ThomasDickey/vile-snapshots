%pointer
%s sTEXT NOTE

%{

/*
 * $Id: texifilt.l,v 1.16 2025/01/26 11:38:02 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of texinfo input.
 */

#include <filters.h>

DefineFilter(texi);

static char *Action_attr;
static char *Comment_attr;
static char *Number_attr;
static char *String_attr;

%}

SPACE		[ \t]

PUNCT		[!"'*.:=?@^`{}~-]

NAME		[[:alpha:]]+

INTEGER		[-+]?([[:digit:]]+)
REAL		[-+]?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?
NUMBER		({INTEGER}|{REAL})

%%

<sTEXT>"@{C}"		|
<sTEXT>"@"{SPACE}	|
<sTEXT>[{}]		|
<sTEXT>"@"{PUNCT}	{ WriteToken(String_attr); }

<sTEXT>"@"{NAME}	{ const char *attr = get_keyword_attr(yytext + 1);
			  const char *mark = Action_attr;
			  if (attr == Comment_attr) {
			      BEGIN(NOTE);
			      mark = attr;
			  } else if (attr != NULL) {
			      mark = attr;
			  }
			  flt_puts("@", 1, mark);
			  WriteToken2(get_keyword_attr(yytext + 1), 1);
			}

<NOTE>[^\r\n]*		{ WriteToken(Comment_attr); }
<NOTE>\n		{ ECHO; BEGIN(sTEXT); }

<sTEXT>{NUMBER}		{ WriteToken(Number_attr); }
<sTEXT>.		{ ECHO; }

%%

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
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    BEGIN(sTEXT);
    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
