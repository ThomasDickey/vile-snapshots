%s NORMAL COMMENT QUERY

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/ecl-filt.l,v 1.3 2004/11/23 22:37:09 tom Exp $
 *
 * Filter to add vile "attribution" sequences to Prolog/ECLiPSe code
 *
 * Joachim Schimpf (j.schimpf@ic.ac.uk)
 */

#include <filters.h>

DefineFilter("ecl");

static char *Comment_attr;
static char *Preproc_attr;

%}

LINECOMMENT	%.*\n
BRCOMMENT	"/*"[^*]*[*]+([^*/][^*]*[*]*)*"/"
BRCOMMENT1	"/*"[/]?(([^*][/]+)|[^/])*"*/"

KEYWORD		[a-z][a-zA-Z0-9_]*

STARTQUERY	^[:?]"-/"[a-z \t\n]

SSTRING		\'(\\.|\\\n|[^'\\])*\'
DSTRING		\"(\\.|\\\n|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})


%%

<NORMAL>{KEYWORD}	{ WriteToken(keyword_attr(yytext)); }

<NORMAL>{LINECOMMENT}	{ WriteToken(Comment_attr); }
<NORMAL>{BRCOMMENT}	{ WriteToken(Comment_attr); }

<NORMAL>{STARTQUERY}	{ WriteToken(Preproc_attr); BEGIN(QUERY); }
<QUERY>\./[ \t\n]	{ WriteToken(Preproc_attr); BEGIN(NORMAL); }
<QUERY>{STRINGS}	{ WriteToken(Preproc_attr); }
<QUERY>{LINECOMMENT}	{ WriteToken(Comment_attr); }
<QUERY>{BRCOMMENT}	{ WriteToken(Comment_attr); }
<QUERY>.		{ WriteToken(Preproc_attr); }

<NORMAL>0\'		{ ECHO; }
<NORMAL>{STRINGS}	{ ECHO; }
<NORMAL>.		{ ECHO; }

%%

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Comment_attr = class_attr(NAME_COMMENT);
    Preproc_attr  = class_attr(NAME_PREPROC);

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
}
