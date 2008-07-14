%pointer
%x NORMAL HEREDOC HEREDOC2 QUOTED SUBST

%o 10000
%a 5000

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sh-filt.l,v 1.78 2008/07/14 19:21:33 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Shell script.
 */

#include <filters.h>

#define FLTSTACK_EXTRA      int backtic;
#define FLTSTACK_EXTRA_PUSH FLTSTACK_THIS.backtic = 0;
#include <fltstack.h>

#ifdef DEBUG
DefineOptFilter("sh", "d");
#else
DefineFilter("sh");
#endif

#ifdef DEBUG
#define DPRINTF(params) if(FltOptions('d'))printf params
#else
#define DPRINTF(params)		/*nothing */
#endif

#define isQuote(ch) (ch != 0 && strchr("\\'\"", ch) != 0)

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Number_attr;
static char *String_attr;

static int strip_tabs;
static char *here_tag;
static unsigned here_len;
static int here_exp;

static int embed_or_append(char *text, int length);
static int got_here(char *text, int length);
static void color_delimiter(char *text, int size, char *attr);
static void handle_backtic(char *text, int length);
static void save_here(char *text, int length);

%}

SPACE		[ \t]

SSTRING		\'([^']|\n)*\'
DSTRING		\"([^"]|\n)*\"

NAME0		[[:alpha:]_]
NAME		[[:alnum:]_]
NAME2		({NAME}|"-")

WILDCARD	(\?|\*)
WILDNAME	({NAME}|{WILDCARD})
FILENAME	(([./]+{WILDNAME}+)|({WILDNAME}+[./]+{WILDNAME}*)|({NAME}*{WILDCARD})|\.+\/+)+

INTEGER		[-+]?([[:digit:]]+)

BACKTIC		`
ACTION		[\.{}]

IDENT		{NAME0}{NAME}*
QIDENT		({SSTRING}|{DSTRING}|[^ \"'$\t\r\n])+
IDENT0		[-]+[[:digit:]]*{NAME0}+{NAME2}*

IDENT1		\${NAME}+
IDENT2		\$\{[#]?{IDENT}\}
IDENT2L		\$\{([#]?{IDENT}|{INTEGER})
IDENT2R		\}
IDENTEQLS	{NAME0}{NAME}*=
IDENTX		\$[\*@#\?\$!-]

DASHIDENT	{NAME0}{NAME}*("-"{NAME}{NAME2}*)*

%%

<NORMAL>{IDENT0}	|
<NORMAL>{FILENAME}	{ ECHO; /* exclude from other classes */ }

<NORMAL>{DASHIDENT}	{ WriteToken(keyword_attr(yytext)); }

<NORMAL>"#"[^\r\n]*	{ WriteToken(Comment_attr); }

<NORMAL>^{SPACE}*:	{ color_delimiter(yytext, yyleng, Action_attr); }

<NORMAL>{INTEGER}	{ WriteToken(Number_attr); }

<NORMAL>{IDENT1}	|
<NORMAL>{IDENT2}	{ WriteToken(Ident_attr); }
<NORMAL>{IDENT2L}	{ WriteToken(Ident_attr); push_state(SUBST); }
<NORMAL>{IDENTX}	{ WriteToken(Ident_attr); }

<NORMAL>{IDENTEQLS}	{ flt_puts(yytext, yyleng-1, Ident_attr); flt_putc('='); }

<NORMAL>\\.		|
<NORMAL>{SSTRING}	{ WriteToken(String_attr); }
<NORMAL>\"		{ push_state(QUOTED); BeginQuote(QUOTED, String_attr); }

<NORMAL>{BACKTIC}	{ handle_backtic(yytext, yyleng); }
<NORMAL>{ACTION}	{ WriteToken(Action_attr); }

<NORMAL>\<\<[-]?{SPACE}*{QIDENT} {
			    int state;
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
			    state = here_exp ? HEREDOC : HEREDOC2;
			    push_state(state);
			    if (strchr(yytext, '\n') != 0) {
				flt_error("unexpected end of line");
				BeginQuote(state, Error_attr);
			    } else {
				BeginQuote(state, String_attr);
			    }
			    flt_bfr_begin(String_attr);
			}
<NORMAL>[\n]		{ ECHO; }

<HEREDOC>{BACKTIC}	{
			    handle_backtic(yytext, yyleng);
			}
<HEREDOC>\\\$		{ flt_bfr_append(yytext, yyleng); }
<HEREDOC>{IDENT1}	|
<HEREDOC>{IDENT2}	{ embed_or_append(yytext, yyleng); }
<HEREDOC>{IDENT2L}	{ if (embed_or_append(yytext, yyleng)) push_state(SUBST); }
<HEREDOC>^[\t]*{QIDENT}$ {
			    int used = 0;
			    if (strip_tabs) {
				used = skip_blanks(yytext) - (char *) yytext;
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
<HEREDOC>[^\r\n\$]+	{ flt_bfr_append(yytext, yyleng); }
<HEREDOC>\n		{ flt_bfr_append(yytext, yyleng); }
<HEREDOC>.		{ flt_bfr_append(yytext, yyleng); }

<HEREDOC2>^[^\r\n]+ {
			    int used = 0;
			    if (strip_tabs) {
				used = skip_blanks(yytext) - (char *) yytext;
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
<HEREDOC2>[\r\n]+	{ flt_bfr_append(yytext, yyleng); }

<QUOTED>{BACKTIC}	{ handle_backtic(yytext, yyleng); }
<QUOTED>\\[\n]		|
<QUOTED>\\.		{ flt_bfr_embed(yytext, 1, Action_attr); flt_bfr_append(yytext + 1, yyleng - 1); }
<QUOTED>{IDENT1}	|
<QUOTED>{IDENT2}	{ flt_bfr_embed(yytext, yyleng, Ident_attr); }
<QUOTED>{IDENT2L}	{ if (embed_or_append(yytext, yyleng)) push_state(SUBST); }
<QUOTED>[^`\r\n\\\"$]+	{ flt_bfr_append(yytext, yyleng); }
<QUOTED>[\n$]		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>\"		{ FinishQuote(NORMAL); pop_state(); }

<SUBST>{SSTRING}	{ flt_bfr_embed(yytext, yyleng, String_attr); }
<SUBST>{BACKTIC}	{ handle_backtic(yytext, yyleng); }
<SUBST>{IDENT2R}	{ flt_bfr_embed(yytext, yyleng, Ident_attr); pop_state(); }
<SUBST>\\\"		{ flt_bfr_embed(yytext, yyleng, String_attr); }
<SUBST>\"		{ push_state(QUOTED); BeginQuote(QUOTED, String_attr); }
<SUBST>[^"}\\]		{ flt_bfr_embed(yytext, yyleng, ""); }

%%

static void
handle_backtic(char *text, int len)
{
    flt_bfr_finish();
    flt_puts(text, len, Action_attr);
    if (FLTSTACK_OK && FLTSTACK_THIS.backtic) {
	pop_state();
	switch (FLT_STATE) {
	case QUOTED:
	case SUBST:
	    flt_bfr_begin(String_attr);
	    break;
	}
    } else {
	flt_bfr_finish();
	push_state(NORMAL);
	if (FLTSTACK_OK)
	    FLTSTACK_THIS.backtic = 1;
    }
}

static void
save_here(char *text, int length)
{
    char *s;
    int delim = 0;

    here_exp = 1;
    DPRINTF(("save_here '%.*s' ", length, text));
    if ((here_tag = do_alloc(here_tag, length, &here_len)) != 0) {
	s = here_tag;
	/* skip leading "<<" or "<<-", and blanks */
	while (length > 0
	       && (strchr("<->", *text) != 0
		   || isspace(CharOf(*text)))) {
	    --length;
	    ++text;
	}
	while (length--) {
	    if ((delim != 0) && (*text == delim)) {
		break;
	    } else if (isQuote(*text)) {
		here_exp = 0;
		if (delim == 0) {
		    delim = *text;
		}
	    } else if ((delim == '"') && (*text == '\\')) {
		;
	    } else if ((delim == 0) && (strchr(" \t", *text) != 0)) {
		if (s != here_tag)
		    break;
	    } else {
		*s++ = *text;
	    }
	    text++;
	}
	*s = 0;
    }
    DPRINTF(("-> '%s'(%d)\n", here_tag, here_exp));
}

static int
embed_or_append(char *text, int length)
{
    /* FIXME: here_exp should always be true - verify that */
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
    int rc = 0;

    if (here_tag != 0) {
	for (pass = 0; pass < 2; pass++) {
	    for (j = k = 0; j < length; j++) {
		if (text[j] != here_tag[k++]) {
		    return 0;
		}
		if (pass)
		    flt_bfr_append(text + j, 1);
	    }
	}
	rc = 1;
    }
    return rc;
}

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
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
do_filter(FILE *inputs)
{
    yyin = inputs;

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

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
    flt_free(&here_tag, &here_len);
}
#endif
