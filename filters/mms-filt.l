%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/mms-filt.l,v 1.4 2000/02/10 11:30:12 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of VMS mms script
 * - T.Dickey
 */

#include <filters.h>

DefineFilter("mms");

static char *Comment_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *String_attr;

static void
write_keyword(char *text, int size)
{
    int colon = size > 1 && text[size-1] == ':';
    int space = 0;

    if (colon)
	text[--size] = 0;

    while (isspace(*text)) {
	text++;
	size--;
	space++;
    }
    if (space)
	fprintf(yyout, "%*s", space, " ");

    space = 0;
    while (isspace(text[size-1])) {
	text[--size] = 0;
	space++;
    }

    write_token(yyout, text, size, keyword_attr(text));

    if (space)
	fprintf(yyout, "%*s", space, " ");

    if (colon)
	fputc(':', yyout);
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;

    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    String_attr  = class_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}

%}

SPACE		[ \t]

SSTRING		\'\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

IDENT		[$a-zA-Z_][$a-zA-Z0-9_]*
PARAM		'{IDENT}

INLINECOMMENT	![^\n]*
COMMENT		"#"[^\n]*

%%

^{SPACE}*(.)?include{SPACE}*	{ write_keyword(yytext, yyleng); }	

^{SPACE}*[\.!]{IDENT}{SPACE}*:	{ write_keyword(yytext, yyleng); }

^{SPACE}*{IDENT}{SPACE}*:	{ WriteToken(Keyword_attr); }

^{SPACE}*{IDENT}{SPACE}*=	{ WriteToken(Ident2_attr); }

\$[@!%^&*?<]			|
\${SPACE}*\([^\)]+\)		{ WriteToken(Ident2_attr); }

{PARAM}				{ WriteToken(Ident2_attr); }

{STRINGS}			{ WriteToken(String_attr); }

{INLINECOMMENT}			|
{COMMENT}			{ WriteToken(Comment_attr); }
