%pointer
%x COMMENT CODE PERLXS

%{

/*
 * $Id: xs-filt.l,v 1.22 2013/12/02 01:32:53 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Perl/C
 * extension source
 * - T.Dickey
 */

#include <filters.h>

DefineFilter(xs);

static char *Comment_attr;
static char *Preproc_attr;
static char *Number_attr;
static char *String_attr;

static int my_state;

static void comment_or_preproc(char *text);

%}

BLANK		[ \t]*

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

INCLUDE		([<][^>]+[>]|\"[^"]+\")

KEYWORD		[[:alpha:]_][[:alnum:]_]*

SIGN		[-+]
DECIMAL		[[:digit:]_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[[:xdigit:]_]+
REAL		([[:digit:]_]*\.[[:digit:]][[:digit:]_]*)([eE]{SIGN}?[[:digit:]_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

%%

<CODE>^{BLANK}[#]include{BLANK}{INCLUDE} |
<CODE>^{BLANK}[#]{KEYWORD} { WriteToken(Preproc_attr); }

<CODE,PERLXS>{KEYWORD}	{ WriteToken(get_keyword_attr(yytext));
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

%%

static void
comment_or_preproc(char *text)
{
    static const char *tbl[] =
    {
	"define",
	"elif",
	"else",
	"endif",
	"if",
	"ifdef",
	"ifndef",
	"undef",
    };
    size_t j, k;
    char *base = text;
    text++;			/* skip "#" */
    while (isspace(CharOf(*text)))
	text++;
    for (j = 0; j < sizeof(tbl) / sizeof(tbl[0]); j++) {
	k = strlen(tbl[j]);
	if (!strncmp(tbl[j], text, k)
	    && !isalnum(CharOf(text[k]))) {
	    text += k;
	    flt_puts(base, (int) (text - base), Preproc_attr);
	    /* FIXME: this can't handle inline comment or a continuation line */
	    flt_puts(text, (int) strlen(text), "");
	    return;
	}
    }
    flt_puts(base, (int) (text - base), "");
    flt_puts(text, (int) strlen(text), Comment_attr);
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
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    my_state = CODE;
    BEGIN(my_state);
    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
