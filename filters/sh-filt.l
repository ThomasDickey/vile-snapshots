%x NORMAL HEREDOC QUOTED SUBST

%o6000
%a3000

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sh-filt.l,v 1.55 2003/05/03 00:03:17 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Shell script.
 */

#include <filters.h>

DefineFilter("sh");

#define isQuote(ch) (ch != 0 && strchr("\\'\"", ch) != 0)

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Number_attr;
static char *String_attr;

/*
 * We do not need much of a stack, since not much is recursive.  Just in case,
 */
typedef struct {
    int backtic;
    int state;
} STACK;
static STACK * stk_state;
static int stk_limit;
static int stk_level;

static void
pop_stk(void)
{
    int state = NORMAL;
    if (--stk_level >= 0)
	state = stk_state[stk_level].state;
    BEGIN(state);
}

static void
push_stk(int state)
{
    if (++stk_level >= stk_limit) {
	unsigned have = sizeof(STACK) * stk_limit;
	unsigned want = sizeof(STACK) * (stk_limit += (20 + stk_level));
	stk_state = type_alloc(STACK,(void *)stk_state, want, &have);
    }
    stk_state[stk_level].state = state;
    stk_state[stk_level].backtic = 0;
    BEGIN(state);
}

static void
handle_backtic(void)
{
    if (stk_state[stk_level].backtic) {
	pop_stk();
    } else {
	flt_bfr_finish();
	push_stk(NORMAL);
	stk_state[stk_level].backtic = 1;
    }
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
	flt_bfr_embed(text, length, Ident_attr);
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
    stk_level = -1;
    push_stk(NORMAL);

    while (yylex() > 0) {
    }
    flt_bfr_error();
}

%}

SPACE		[ \t]

SSTRING		\'([^']|\n)*\'
DSTRING		\"([^"]|\n)*\"

NAME		[a-zA-Z0-9_]

WILDCARD	(\?|\*)
WILDNAME	({NAME}|{WILDCARD})
FILENAME	(([./]{WILDNAME}+)|({WILDNAME}+[./]{WILDNAME}*)|({NAME}*{WILDCARD})|\.+\/+)+

INTEGER		[-+]?([0-9]+)

BACKTIC		`
ACTION		[\.{}]
IDENT		[a-zA-Z_]{NAME}*
QIDENT		({SSTRING}|{DSTRING}|[^ \"'$\t\n])+
IDENT0		[-]+[0-9]*[a-zA-Z_-]+[0-9a-zA-Z_-]*
IDENT1		\${NAME}+
IDENT2		\$\{{IDENT}\}
IDENT2L		\$\{({IDENT}|{INTEGER})
IDENT2R		\}
IDENTEQLS	[a-zA-Z_]{NAME}*=
IDENTX		\$[\*@#\?\$!-]

%%

<NORMAL>{IDENT0}	|
<NORMAL>{FILENAME}	{ ECHO; /* exclude from other classes */ }

<NORMAL>{IDENT}		{ WriteToken(keyword_attr(yytext)); }

<NORMAL>"#"[^\n]*	{ WriteToken(Comment_attr); }

<NORMAL>^{SPACE}*:	{ color_delimiter(yytext, yyleng, Action_attr); }

<NORMAL>{INTEGER}	{ WriteToken(Number_attr); }

<NORMAL>{IDENT1}	|
<NORMAL>{IDENT2}	{ WriteToken(Ident_attr); }
<NORMAL>{IDENT2L}	{ WriteToken(Ident_attr); push_stk(SUBST); }
<NORMAL>{IDENTX}	{ WriteToken(Ident_attr); }

<NORMAL>{IDENTEQLS}	{ flt_puts(yytext, yyleng-1, Ident_attr); flt_putc('='); }

<NORMAL>\\.		|
<NORMAL>{SSTRING}	{ WriteToken(String_attr); }
<NORMAL>\"		{ push_stk(QUOTED); BeginQuote(QUOTED, String_attr); }

<NORMAL>{BACKTIC}	{ handle_backtic(); WriteToken(Action_attr); }
<NORMAL>{ACTION}	{ WriteToken(Action_attr); }

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
			    push_stk(HEREDOC);
			    BeginQuote(HEREDOC,
				(strchr(yytext, '\n') != 0)
				? Error_attr
				: String_attr);
			    flt_bfr_begin(String_attr);
			}
<HEREDOC>{BACKTIC}	{
			    if (here_exp) {
				handle_backtic();
				WriteToken(Action_attr);
			    } else {
				flt_bfr_append(yytext, yyleng);
			    }
			}
<HEREDOC>\\\$		{ flt_bfr_append(yytext, yyleng); }
<HEREDOC>{IDENT1}	|
<HEREDOC>{IDENT2}	{ embed_or_append(yytext, yyleng); }
<HEREDOC>{IDENT2L}	{ if (embed_or_append(yytext, yyleng)) push_stk(SUBST); }
<HEREDOC>^[\t]*{QIDENT}$ {
			    int used = 0;
			    if (strip_tabs) {
				used = skip_blanks(yytext) - yytext;
				if (used != 0)
				    flt_bfr_append(yytext, used);
			    }
			    if (got_here(yytext + used, yyleng - used)) {
				flt_bfr_finish();
				pop_stk();
				strip_tabs = 0;
			    } else {
				flt_bfr_append(yytext, yyleng);
			    }
			}
<HEREDOC>[^\n\$]+	{ flt_bfr_append(yytext, yyleng); }
<HEREDOC>\n		{ flt_bfr_append(yytext, yyleng); }
<HEREDOC>.		{ flt_bfr_append(yytext, yyleng); }

<QUOTED>{BACKTIC}	{ handle_backtic(); WriteToken(Action_attr); }
<QUOTED>\\[\n]		|
<QUOTED>\\.		{ flt_bfr_embed(yytext, 1, Action_attr); flt_bfr_append(yytext + 1, yyleng - 1); }
<QUOTED>{IDENT1}	|
<QUOTED>{IDENT2}	{ flt_bfr_embed(yytext, yyleng, Ident_attr); }
<QUOTED>{IDENT2L}	{ if (embed_or_append(yytext, yyleng)) push_stk(SUBST); }
<QUOTED>[^`\n\\\"$]+	{ flt_bfr_append(yytext, yyleng); }
<QUOTED>[\n$]		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>\"		{ FinishQuote(NORMAL); pop_stk(); }

<SUBST>{BACKTIC}	{ WriteToken(Action_attr); handle_backtic(); }
<SUBST>{IDENT2R}	{ flt_bfr_embed(yytext, yyleng, Ident_attr); pop_stk(); }
<SUBST>\"		{ push_stk(QUOTED); BeginQuote(QUOTED, String_attr); }
<SUBST>[^"}]+		{ flt_bfr_embed(yytext, yyleng, ""); }
