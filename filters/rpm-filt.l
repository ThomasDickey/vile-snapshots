%x META TEXT NORMAL HEREDOC QUOTED QUOTED2 SUBST

%o6000

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/rpm-filt.l,v 1.3 2002/12/11 01:58:29 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of rpm spec-file.
 */

#include <filters.h>

DefineFilter("rpm");

#define isQuote(ch) (ch != 0 && strchr("\\'\"", ch) != 0)

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Number_attr;
static char *String_attr;

#include "fltstack.h"

static void
begin_section(char *name)
{
    WriteToken(keyword_attr(name));
    if (!strcmp(name, "%description")
     || !strcmp(name, "%changelog")
     || !strcmp(name, "%package")) {
	new_state(TEXT);
    } else {
	new_state(NORMAL);
    }
}

static char *
meta_variable(char *name)
{
    char *attr = keyword_attr(name);
    return (attr != 0) ? attr : Ident_attr;
}

static int strip_tabs;
static char *here_tag;
static unsigned here_len;
static int here_exp;

static void
save_here(char *text, int length)
{
    char *s = here_tag = do_alloc(here_tag, length, &here_len);

    here_exp = 1;
    while (length--) {
	if (isQuote(*text)) {
	    here_exp = 0;
	} else if (strchr(" \t", *text) != 0) {
	    if (s != here_tag)
		break;
	} else if (strchr("<->", *text) == 0) {
	    *s++ = *text;
	}
	text++;
    }
    *s = 0;
}

static int
embed_or_append(char *text, int length)
{
    if (here_exp) {
	flt_bfr_embed(text, length, meta_variable(yytext));
    } else {
	flt_bfr_append(text, length);
    }
    return here_exp;
}

static int
got_here(char *text, int length)
{
    int pass, j, k;

    for (pass = 0; pass < 2; pass++) {
	for (j = k = 0; j < length; j++) {
	    if (isQuote(text[j])) {
		if (pass)
		    flt_bfr_embed(text + j, 1, Error_attr);
	    } else {
		if (text[j] != here_tag[k++])
		    return 0;
		if (pass)
		    flt_bfr_append(text + j, 1);
	    }
	}
    }
    return 1;
}

static void
init_filter(int before GCC_UNUSED)
{
}

/*
 * string passed to this routine is in the format:
 *
 *    [<white>]:
 */
static void
color_delimiter(char *text, int size, char *attr)
{
    char delim[2];

    delim[0] = text[--size];       /* save the trailing delimiter */
    delim[1] = text[size] = '\0';  /* chop the trailing delimiter */
    if (size)
	flt_puts(text, size , "");
    flt_puts(delim, 1, attr);
}

static void
do_filter(FILE *input)
{
    yyin = input;

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    Ident_attr   = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    here_exp = 0;
    begin_state(NORMAL);
    while (yylex() > 0) {
    }
    flt_bfr_error();
    end_state();
}

%}

SPACE		[ \t]

SSTRING		\'([^']|\n)*\'
DSTRING		\"([^"]|\n)*\"

NAME		[a-zA-Z0-9_]

WILDCARD	(\?|\*)
WILDNAME	({NAME}|{WILDCARD})
FILENAME	(([./]{WILDNAME}+)|({WILDNAME}+[./]{WILDNAME}*)|({NAME}*{WILDCARD})|\.+\/+)+

ACTION		[\.`{}]
IDENT		[a-zA-Z_]{NAME}*
QIDENT		({SSTRING}|{DSTRING}|[^ \"'$\t\n])+
IDENT0		[-]+[0-9]*[a-zA-Z_-]+[0-9a-zA-Z_-]*
IDENT1		[%$]{NAME}+
IDENT2		[%$]\{{IDENT}\}
IDENT2L		[%$]\{{IDENT}
IDENT2R		\}
IDENTEQLS	[a-zA-Z_]{NAME}*=
IDENTX		\$[\*@#\?\$!-]

INTEGER		[-+]?([0-9]+)

SECTION		%{IDENT}

%%

<NORMAL>{SECTION}	{ begin_section(yytext); }

<TEXT>^"%"[^\n \t]*	{ begin_section(yytext); }
<TEXT>{IDENT1}		|
<TEXT>{IDENT2}		{ WriteToken(keyword_attr(yytext)); }
<TEXT>[^%\n]*		{ WriteToken(String_attr); }
<TEXT>\n		{ ECHO; }

<NORMAL>{IDENT0}	|
<NORMAL>{FILENAME}	{ ECHO; /* exclude from other classes */ }

<NORMAL>{IDENT}		{ WriteToken(keyword_attr(yytext)); }

<NORMAL>"#"[^\n]*	{ WriteToken(Comment_attr); }

<NORMAL>^{SPACE}*:	{ color_delimiter(yytext, yyleng, Action_attr); }

<NORMAL>{ACTION}	{ WriteToken(Action_attr); }

<NORMAL>{INTEGER}	{ WriteToken(Number_attr); }

<NORMAL>{IDENT1}	|
<NORMAL>{IDENT2}	{ WriteToken(meta_variable(yytext)); }
<NORMAL>{IDENT2L}	{ WriteToken(meta_variable(yytext)); push_state(SUBST); }
<NORMAL>{IDENTX}	{ WriteToken(meta_variable(yytext)); }

<NORMAL>{IDENTEQLS}	{ flt_puts(yytext, yyleng-1, meta_variable(yytext)); flt_putc('='); }

<NORMAL>\\.		|
<NORMAL>{SSTRING}	{ WriteToken(String_attr); }
<NORMAL>\"		{ push_state(QUOTED); BeginQuote(QUOTED, String_attr); }

<NORMAL>\<\<[-]?{SPACE}*{QIDENT} {
			    int n;
			    strip_tabs = 0;
			    for (n = 0; n < yyleng; n++) {
				if (yytext[n] != '<'
				 && !isspace(CharOf(yytext[n]))) {
				    strip_tabs = (yytext[n] == '-');
				    break;
				}
			    }
			    save_here(yytext, yyleng);
			    push_state(HEREDOC);
			    BeginQuote(HEREDOC,
				(strchr(yytext, '\n') != 0)
				? Error_attr
				: String_attr);
			    flt_bfr_begin(String_attr);
			}
<HEREDOC>\\\$		{ flt_bfr_append(yytext, yyleng); }
<HEREDOC>{IDENT1}	|
<HEREDOC>{IDENT2}	{ embed_or_append(yytext, yyleng); }
<HEREDOC>{IDENT2L}	{ if (embed_or_append(yytext, yyleng)) push_state(SUBST); }
<HEREDOC>^[\t]*{QIDENT}$ {
			    int used = 0;
			    if (strip_tabs) {
				used = skip_blanks(yytext) - yytext;
				if (used != 0)
				    flt_bfr_append(yytext, used);
			    }
			    if (got_here(yytext + used, yyleng - used)) {
				flt_bfr_finish();
				pop_state();
				strip_tabs = 0;
			    } else {
				flt_bfr_append(yytext, yyleng);
			    }
			}
<HEREDOC>[^\n\$]+	{ flt_bfr_append(yytext, yyleng); }
<HEREDOC>\n		{ flt_bfr_append(yytext, yyleng); }
<HEREDOC>.		{ flt_bfr_append(yytext, yyleng); }

<QUOTED>\\		{ flt_bfr_embed(yytext, yyleng, Action_attr); push_state(QUOTED2); }
<QUOTED>{IDENT1}	|
<QUOTED>{IDENT2}	{ flt_bfr_embed(yytext, yyleng, meta_variable(yytext)); }
<QUOTED>{IDENT2L}	{ if (embed_or_append(yytext, yyleng)) push_state(SUBST); }
<QUOTED>[^\n\\\"$]+	{ flt_bfr_append(yytext, yyleng); }
<QUOTED>[\n$]		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>\"		{ FinishQuote(NORMAL); pop_state(); }

<QUOTED2>(.|\n)		{ flt_bfr_embed(yytext, yyleng, Action_attr); pop_state(); }

<SUBST>{IDENT2R}	{ flt_bfr_embed(yytext, yyleng, meta_variable(yytext)); pop_state(); }
<SUBST>\"		{ push_state(QUOTED); BeginQuote(QUOTED, String_attr); }
<SUBST>[^"}]+		{ flt_bfr_embed(yytext, yyleng, ""); }
