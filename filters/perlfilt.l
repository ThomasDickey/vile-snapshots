%x CODE HERE

%{
/* Filter to add vile "attribution" sequences to selected bits of PERL */
/* input text. */

/*
 * This was based on a version written by Pierre Dittgen (dittgen@univ-mlv.fr)
 * - T.Dickey
 */

#include <filters.h>

char *filter_name = "perl";

#define isIdent(ch) (isalnum(ch) || ch == '_')

static char *Comment_attr;
static char *Preproc_attr;
static char *Keyword_attr;
static char *Ident_attr;
static char *String_attr;
static char *Number_attr;

static char *here_tag;
static unsigned here_len;
static int here_exp;

static void
save_here(char *text, int length)
{
    char *s = here_tag = do_alloc(here_tag, length, &here_len);
    here_exp = 1;
    while (length--) {
	if (isIdent(*text)) {
	    *s++ = *text;
	} else if (*text == '\'') {
	    here_exp = 0;
	}
	text++;
    }
    *s = 0;
}

/*
 * Buffer text for here-documents, since most characters are attributed
 * as string, which would be a lot of overhead to mark it up.
 */
static char    *save_bfr;
static unsigned save_max;
static unsigned save_len;

static void
add_to_here(char *text, int length)
{
    save_bfr = do_alloc(save_bfr, save_len + length, &save_max);
    strncpy(save_bfr + save_len, text, length);
    save_len += length;
}

static void
flush_here(void)
{
    if (save_len) {
	write_token(yyout, save_bfr, save_len, String_attr);
	save_len = 0;
    }
}

void
init_filter(void)
{
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;
    BEGIN(CODE);
    Comment_attr = keyword_attr(NAME_COMMENT);
    Preproc_attr = keyword_attr(NAME_PREPROC);
    Keyword_attr = keyword_attr(NAME_KEYWORD);
    Ident_attr   = keyword_attr(NAME_IDENT);
    String_attr  = keyword_attr(NAME_LITERAL);
    Number_attr  = keyword_attr(NAME_NUMBER);
    while (yylex() > 0) {
    }
}

%}

BLANK		[ \t]*
NOSTRBRK	"\\\""
STRING		\"([^"]|{NOSTRBRK})*\"
KEYWORD		[a-zA-Z_][a-zA-Z_0-9]*
QIDENT		[a-zA-Z_0-9"']+
IDENT		\$[a-zA-Z_0-9]+
INTEGER		[-+]?([0-9]+)
REAL		[-+]?([0-9]*\.[0-9]+)([eE][+-]?[0-9]+)?

%%

<CODE><<{QIDENT}	{
			    BEGIN(HERE);
			    save_here(yytext, yyleng);
			    add_to_here(yytext, yyleng);
			}
<HERE>{IDENT}		{
			    if (here_exp) {
				flush_here();
				WriteToken(Ident_attr);
			    } else {
				add_to_here(yytext, yyleng);
			    }
			}
<HERE>^{QIDENT}		{
			    add_to_here(yytext, yyleng);
			    if (!strcmp(here_tag, yytext)) {
				flush_here();
				BEGIN(CODE);
			    }
			}
<HERE>[^\n]		{ add_to_here(yytext, yyleng); }
<HERE>\n		{
			    flush_here();
			    fputc(yytext[0], yyout);
			}

<CODE>^{BLANK}*#!.*$	{ WriteToken(Preproc_attr); }

<CODE>-[a-zA-Z]		{ WriteToken(Keyword_attr); }
<CODE>{KEYWORD}		{ WriteToken(keyword_attr(yytext)); }

<CODE>^{BLANK}*"#".*$	{ WriteToken(Comment_attr); }

<CODE>{INTEGER}	|
<CODE>{REAL}		{ WriteToken(Number_attr); }

<CODE>{IDENT}		{ WriteToken(Ident_attr); }

<CODE>{STRING}		{ WriteToken(String_attr); }
