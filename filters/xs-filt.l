%x COMMENT CODE PERLXS

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/xs-filt.l,v 1.6 2000/01/31 00:23:43 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Perl/C
 * extension source
 * - T.Dickey
 */

#include <filters.h>

char *filter_name = "xs";

static char *Comment_attr;
static char *Preproc_attr;
static char *Number_attr;
static char *String_attr;

static int my_state;

static void
comment_or_preproc(char *text)
{
    static const char *tbl[] = {
	"define",
	"elif",
	"else",
	"endif",
	"if",
	"ifdef",
	"ifndef",
	"undef",
    };
    unsigned j, k;
    char *base = text;
    text++;	/* skip "#" */
    while (isspace(*text))
    	text++;
    for (j = 0; j < sizeof(tbl)/sizeof(tbl[0]); j++) {
	k = strlen(tbl[j]);
	if (!strncmp(tbl[j], text, k)
	 && !isalnum(text[k])) {
	    text += k;
	    write_token(yyout, base, text-base, Preproc_attr);
	    /* FIXME: this can't handle inline comment or a continuation line */
	    write_token(yyout, text, strlen(text), "");
	    return;
	}
    }
    write_token(yyout, base, text-base, "");
    write_token(yyout, text, strlen(text), Comment_attr);
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
    Number_attr  = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    my_state = CODE;
    BEGIN(my_state);
    while (yylex() > 0) {
    }
}
%}

BLANK		[ \t]*

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

INCLUDE		(<[^>]+>|\"[^"]+\")

KEYWORD		[a-zA-Z_][a-zA-Z_0-9]*

SIGN		[-+]
DECIMAL		[0-9_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[0-9a-fA-F_]+
REAL		[-+]?([0-9_]*\.[0-9][0-9_]*)([eE][+-]?[0-9_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

%%

<CODE>^{BLANK}#include{BLANK}{INCLUDE} |
<CODE>^{BLANK}#{KEYWORD} { WriteToken(Preproc_attr); }

<CODE,PERLXS>{KEYWORD}	{ WriteToken(keyword_attr(yytext));
			  if (!strcmp(yytext, "MODULE")) {
			      my_state = PERLXS;
			      BEGIN(my_state);
			  }
			}

<CODE,PERLXS>"/*"	{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<COMMENT>[^*]*		{ WriteToken(Comment_attr); }
<COMMENT>"*"+[^*/]*	{ WriteToken(Comment_attr); }
<COMMENT>"*"+"/"	{ WriteToken(Comment_attr); BEGIN(my_state); }

<CODE>"//".*$		{ WriteToken(Comment_attr); }

<CODE,PERLXS>{STRINGS}	{ WriteToken(String_attr); }

<CODE,PERLXS>{NUMBER}	{ WriteToken(Number_attr); }

<PERLXS>^{BLANK}#.*$	{ comment_or_preproc(yytext); }
