%x NORMAL HEREDOC QUOTED

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sh-filt.l,v 1.24 1999/05/26 09:41:09 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Shell script.
 */

#include <filters.h>

char *filter_name = "sh";

#define isIdent(ch) (isalnum(ch) || ch == '_')

static char *Action_attr;
static char *Comment_attr;
static char *Ident_attr;
static char *Number_attr;
static char *String_attr;

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
add_to_quoted(char *text, int length)
{
    save_bfr = do_alloc(save_bfr, save_len + length, &save_max);
    strncpy(save_bfr + save_len, text, length);
    save_len += length;
}

static void
flush_quoted(void)
{
    if (save_len) {
	write_token(yyout, save_bfr, save_len, String_attr);
	save_len = 0;
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

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr   = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
}

%}

SPACE		[ \t]

SSTRING		\'(\\.|[^'\\])*\'

NAME		[a-zA-Z0-9_]

WILDCARD	(\?|\*)
WILDNAME	({NAME}|{WILDCARD})
FILENAME	(([./]{WILDNAME}+)|({WILDNAME}+[./]{WILDNAME}*)|({NAME}*{WILDCARD})|\.+\/+)+

ACTION		[\.`{}]
IDENT		[a-zA-Z_]{NAME}*
QIDENT		[a-zA-Z_0-9"'\\]+
IDENT0		[-]+[0-9]*[a-zA-Z_-]+[0-9a-zA-Z_-]*
IDENT1		\${NAME}+
IDENT2		\$\{[^\}]*\}
IDENTEQLS	[a-zA-Z_]{NAME}*=
IDENTX		\$[\*@#\?\$!-]

INTEGER		[-+]?([0-9]+)

%%

<NORMAL>{IDENT0}	|
<NORMAL>{FILENAME}	{ ECHO; /* exclude from other classes */ }

<NORMAL>{IDENT}		{ WriteToken(keyword_attr(yytext)); }

<NORMAL>"#"[^\n]*	{ WriteString(Comment_attr); }

<NORMAL>{ACTION}	{ WriteToken(Action_attr); }

<NORMAL>{INTEGER}	{ WriteToken(Number_attr); }

<NORMAL>{IDENT1}	|
<NORMAL>{IDENT2}	|
<NORMAL>{IDENTX}	{ WriteToken(Ident_attr); }

<NORMAL>{IDENTEQLS}	{ write_string(yyout, yytext, yyleng-1, Ident_attr); fprintf(yyout, "="); }

<NORMAL>\\.		|
<NORMAL>{SSTRING}	{ WriteString(String_attr); }
<NORMAL>\"		{ add_to_quoted(yytext, yyleng); BEGIN(QUOTED); }

<NORMAL>\<\<{SPACE}*{QIDENT} {
			    BEGIN(HEREDOC);
			    save_here(yytext, yyleng);
			    add_to_quoted(yytext, yyleng);
			}
<HEREDOC>\\\$		{ add_to_quoted(yytext, yyleng); }
<HEREDOC>{IDENT1}	{
			    if (here_exp) {
				flush_quoted();
				WriteToken(Ident_attr);
			    } else {
				add_to_quoted(yytext, yyleng);
			    }
			}
<HEREDOC>^{QIDENT}	{
			    add_to_quoted(yytext, yyleng);
			    if (!strcmp(here_tag, yytext)) {
				flush_quoted();
				BEGIN(NORMAL);
			    }
			}
<HEREDOC>[^\n]		{ add_to_quoted(yytext, yyleng); }
<HEREDOC>\n		{
			    flush_quoted();
			    fputc(yytext[0], yyout);
			}

<QUOTED>\\.		{ add_to_quoted(yytext, yyleng); }
<QUOTED>{IDENT1}	{ flush_quoted();
			  WriteToken(Ident_attr);
			}
<QUOTED>[^\n\"]		{ add_to_quoted(yytext, yyleng); }
<QUOTED>\n		{
			    flush_quoted();
			    fputc(yytext[0], yyout);
			}
<QUOTED>\"		{ add_to_quoted(yytext, yyleng);
			  flush_quoted();
			  BEGIN(NORMAL);
			}
