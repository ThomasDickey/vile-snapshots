%x CODE HERE

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sh-filt.l,v 1.14 1998/12/22 03:08:30 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Shell script.
 */

#include <filters.h>

#include <stdio.h>
#include <string.h>

char *filter_name = "sh";

#define isIdent(ch) (isalnum(ch) || ch == '_')

static char *Comment_attr;
static char *Action_attr;
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
    Comment_attr = keyword_attr(NAME_COMMENT);
    Action_attr  = keyword_attr(NAME_ACTION);
    Ident_attr   = keyword_attr(NAME_IDENT2);
    String_attr  = keyword_attr(NAME_LITERAL);
    Number_attr  = keyword_attr(NAME_NUMBER);

    BEGIN(CODE);
    while (yylex() > 0) {
    }
}

%}

SPACE		[ \t]
STRING1		\'([^\\']|\\[^'])*\'
STRING2		\"([^"]|\\.)*\"

ACTION		[\.`{}]
IDENT		[a-zA-Z_][a-zA-Z_0-9]*
QIDENT		[a-zA-Z_0-9"']+
IDENT1		\$[a-zA-Z_0-9]+
IDENT2		\$\{[^\}]*\}
IDENTEQLS	[a-zA-Z_][a-zA-Z_0-9]*=
IDENTX		\$[\*@#\?\$!-]

INTEGER		[-+]?([0-9]+)

%%

<CODE>{IDENT}		{ WriteToken(keyword_attr(yytext)); }

<CODE>\\.		{ WriteToken(String_attr); }

<CODE>"#".*$		{ write_string(yyout, yytext, yyleng, Comment_attr); }

<CODE>{ACTION}		{ WriteToken(Action_attr); }

<CODE>{INTEGER}		{ WriteToken(Number_attr); }

<CODE>{IDENT1}		{ WriteToken(Ident_attr); }

<CODE>{IDENT2}		{ WriteToken(Ident_attr); }

<CODE>{IDENTX}		{ WriteToken(Ident_attr); }

<CODE>{IDENTEQLS}	{ write_string(yyout, yytext, yyleng-1, Ident_attr); fprintf(yyout, "="); }

<CODE>{STRING1}		{ write_string(yyout, yytext, yyleng, String_attr); }

<CODE>{STRING2}		{ write_string(yyout, yytext, yyleng, String_attr); }

<CODE>\<\<{SPACE}*{QIDENT} {
			    BEGIN(HERE);
			    save_here(yytext, yyleng);
			    add_to_here(yytext, yyleng);
			}
<HERE>\\\$		{ add_to_here(yytext, yyleng); }
<HERE>{IDENT1}		{
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
