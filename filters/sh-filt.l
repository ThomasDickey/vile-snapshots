%pointer
%x NORMAL HEREDOC HEREDOC2 DQUOTED SQUOTED EXPRS BRACES BRACES2 BRACES3

%o 10000
%a 5000

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sh-filt.l,v 1.121 2013/12/02 01:33:56 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Shell script.
 */

#include <filters.h>
#undef YY_NO_UNPUT

#define FLTSTACK_EXTRA      int backtic;
#define FLTSTACK_EXTRA_PUSH FLTSTACK_THIS.backtic = 0;
#include <fltstack.h>

#ifdef DEBUG
#define OPTS "dK"
#else
#define OPTS "K"
#endif

DefineOptFilter(sh, OPTS);

#ifdef DEBUG
#define DPRINTF(params) if(FltOptions('d'))printf params
#else
#define DPRINTF(params)		/*nothing */
#endif

#define LEN_BACKTIC1_DOWN 2	/* "$(" */
#define LEN_BACKTIC2_DOWN 3	/* "$((" */

#define isQuote(ch) (ch != 0 && strchr("\\'\"", ch) != 0)

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Number_attr;
static char *String_attr;

static int strip_tabs;
static char *here_tag;
static size_t here_len;
static int here_exp;

static int got_here(const char *text, int length);
static int pop_backtic1(const char *text, int len, int test);
static void begin_braces(const char *text, int length);
static void braces_operator(const char *text, int length);
static void color_delimiter(char *text, int size, char *attr);
static void embed_or_append(const char *text, int length);
static void embed_vname(const char *text, int length, const char *dft);
static void end_braces(const char *text, int length);
static void handle_backtic(const char *text, int length, int test, int which);
static void save_here(const char *text, int length);
static void write_vname(const char *text, int length);

%}

SPACE		[[:blank:]]

SSTRING		\'([^']|\n)*\'
DSTRING		\"([^"]|\n)*\"

NAME0		[[:alpha:]_]
NAME		[[:alnum:]_]
NAME2		({NAME}|"-")

WILDCARD	(\?|\*)
WILDNAME	({NAME}|{WILDCARD})
WILDGLUE	({WILDCARD}[#]?)
FILENAME	(([./]+{WILDNAME}+)|({WILDNAME}+[./]+{WILDNAME}*)|({NAME}*{WILDCARD})|{WILDGLUE}|\.+\/+)+

INTEGER		[[:digit:]]+
ANY_INTEGER	[-+]?{INTEGER}

BACKTIC2_DOWN	"$(("
BACKTIC2_UP	"))"
BACKTIC1_DOWN	"$("
BACKTIC1_UP	")"

BACKTIC		`
ACTION		[\.{}]

BRACES		"${"
END_BRACES	"}"

IDENT		{NAME0}{NAME}*
QIDENT		({SSTRING}|{DSTRING}|[^ \"'$\t\r\n(){}])+
IDENT0		[-]+[[:digit:]]*{NAME0}+{NAME2}*

VNAME		[.]?{IDENT}([.]{IDENT})*

IDENT1		\${NAME}+
IDENT2		\$({NAME}+|{VNAME})
IDENTX		\$[\*@#\?\$!-]

DASHIDENT	{IDENT}("-"{NAME}{NAME2}*)*

%%

<NORMAL>{IDENT0}	|
<NORMAL>{FILENAME}	{ WriteToken(""); /* exclude from other classes */ }

<NORMAL>{DASHIDENT}	{ WriteToken(get_keyword_attr(yytext)); }

<NORMAL>"#"[^\r\n]*	{ WriteToken(Comment_attr); }

<NORMAL>^{SPACE}*:	{ color_delimiter(yytext, yyleng, Action_attr); }

<NORMAL>{ANY_INTEGER}	{ WriteToken(Number_attr); }

<NORMAL>{BRACES}	{ begin_braces(yytext, yyleng); }

<NORMAL>{IDENT2}	{ write_vname(yytext, yyleng); }
<NORMAL>{IDENTX}	{ WriteToken(Ident_attr); }

<NORMAL>{IDENT}"="	{ flt_puts(yytext, yyleng-1, Ident_attr); flt_putc('='); }

<NORMAL>\\.		|
<NORMAL>{SSTRING}	{ WriteToken(String_attr); }
<NORMAL>\'		{ PushQuote(SQUOTED, String_attr); }
<NORMAL>\"		{ PushQuote(DQUOTED, String_attr); }

<NORMAL>{BACKTIC1_UP}	{ if (!pop_backtic1(yytext, yyleng, LEN_BACKTIC1_DOWN))
			      ECHO;
			}
<NORMAL>{BACKTIC2_DOWN} { handle_backtic(yytext, yyleng, yyleng, 1); }
<NORMAL>{BACKTIC1_DOWN}	{ handle_backtic(yytext, yyleng, yyleng, 1); }
<NORMAL>{BACKTIC}	{ handle_backtic(yytext, yyleng, yyleng, -1); }
<NORMAL>{ACTION}	{ WriteToken(Action_attr); }

<NORMAL>\<\<\<[-]?{SPACE}*{QIDENT} {
			  flt_error("here-strings are nonstandard");
			  WriteToken(Error_attr);
			}

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

<HEREDOC>{BACKTIC2_DOWN} { handle_backtic(yytext, yyleng, yyleng, 1); }
<HEREDOC>{BACKTIC1_DOWN} { handle_backtic(yytext, yyleng, yyleng, 1); }
<HEREDOC>{BACKTIC}	{ handle_backtic(yytext, yyleng, yyleng, -1); }
<HEREDOC>{IDENT2}	{ if (here_exp) {
			      embed_vname(yytext, yyleng, String_attr);
			  } else {
			      flt_bfr_append(yytext, yyleng);
			  }
			}
<HEREDOC>{BRACES}	{ embed_or_append(yytext, yyleng); }
<HEREDOC>^[\t]*{QIDENT}$ {
			    int used = 0;
			    if (strip_tabs) {
				used = (int) (skip_blanks(yytext) - (char *) yytext);
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
<HEREDOC>\\[\$\`\"\\\n]	{
			  flt_bfr_embed(yytext, 1, Action_attr);
			  flt_bfr_embed(yytext+1, 1, String_attr);
			}
<HEREDOC>[^\r\n`\\\$]+	{ flt_bfr_append(yytext, yyleng); }
<HEREDOC>\n		{ flt_bfr_append(yytext, yyleng); }
<HEREDOC>.		{ flt_bfr_append(yytext, yyleng); }

<HEREDOC2>^[^\r\n]+ {
			    int used = 0;
			    if (strip_tabs) {
				used = (int) (skip_blanks(yytext) - (char *) yytext);
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

<DQUOTED>{BACKTIC1_UP}	{ if (!pop_backtic1(yytext, yyleng, LEN_BACKTIC1_DOWN)) 
			      flt_bfr_append(yytext, yyleng);
			}
<DQUOTED>{BACKTIC2_DOWN} { handle_backtic(yytext, yyleng, yyleng, 1); }
<DQUOTED>{BACKTIC1_DOWN} { handle_backtic(yytext, yyleng, yyleng, 1); }
<DQUOTED>{BACKTIC}	{ handle_backtic(yytext, yyleng, yyleng, -1); }
<DQUOTED>\\[\n]		|
<DQUOTED>\\.		{ flt_bfr_embed(yytext, 1, Action_attr); flt_bfr_append(yytext + 1, yyleng - 1); }
<DQUOTED>{IDENT2}	{ embed_vname(yytext, yyleng, String_attr); }
<DQUOTED>{BRACES}	{ begin_braces(yytext, yyleng); }
<DQUOTED>[^`\r\n\\\"$]+	{ flt_bfr_append(yytext, yyleng); }
<DQUOTED>[\n$]		{ flt_bfr_append(yytext, yyleng); }
<DQUOTED>\"		{ PopQuote(); }

<SQUOTED>[^']		{ flt_bfr_append(yytext, yyleng); }
<SQUOTED>\'		{ PopQuote(); }

<EXPRS>{BACKTIC1_UP}	{ if (!pop_backtic1(yytext, yyleng, LEN_BACKTIC1_DOWN)) 
			      flt_bfr_append(yytext, yyleng);
			}
<EXPRS>{BACKTIC2_DOWN}	{ handle_backtic(yytext, yyleng, yyleng, 1); }
<EXPRS>{BACKTIC2_UP}	{ handle_backtic(yytext, yyleng, LEN_BACKTIC2_DOWN, 0); }
<EXPRS>{BACKTIC1_DOWN}	{ handle_backtic(yytext, yyleng, yyleng, 1); }
<EXPRS>{BACKTIC}	{ handle_backtic(yytext, yyleng, yyleng, -1); }
<EXPRS>\\[\n]		|
<EXPRS>\\.		{ flt_bfr_embed(yytext, 1, Action_attr); flt_bfr_append(yytext + 1, yyleng - 1); }
<EXPRS>{IDENT2}		{ embed_vname(yytext, yyleng, ""); }
<EXPRS>{BRACES}		{ begin_braces(yytext, yyleng); }
<EXPRS>{INTEGER}	{ flt_bfr_embed(yytext, yyleng, Number_attr); }
<EXPRS>[^`\r\n\\\$)]+	{ flt_bfr_append(yytext, yyleng); }

<BRACES>{END_BRACES}	{ end_braces(yytext, yyleng); }
<BRACES>{VNAME}		{ embed_vname(yytext, yyleng, ""); new_state(BRACES2); }
<BRACES>{INTEGER}	{ flt_bfr_embed(yytext, yyleng, Ident_attr); new_state(BRACES2); }
<BRACES>[?#!%]		{ flt_bfr_embed(yytext, yyleng, Action_attr); }
<BRACES>[^?#!%}[:alnum:]_-]+ { flt_bfr_append(yytext, yyleng); }

<BRACES2>{END_BRACES}	{ end_braces(yytext, yyleng); }
<BRACES2>[-:=?+*@#%/^,]+ { braces_operator(yytext, yyleng); }
<BRACES2>[^-:=?+*@#%/^,}[:blank:]]+ { flt_bfr_embed(yytext, yyleng, ""); } /* FIXME - error */
<BRACES2>{SPACE}	{ flt_bfr_append(yytext, yyleng); }

<BRACES3>{BACKTIC2_DOWN} { handle_backtic(yytext, yyleng, yyleng, 1); }
<BRACES3>{BACKTIC1_DOWN} { handle_backtic(yytext, yyleng, yyleng, 1); }
<BRACES3>{BACKTIC}	{ handle_backtic(yytext, yyleng, yyleng, -1); }
<BRACES3>\\\"		{ flt_bfr_embed(yytext, yyleng, String_attr); }
<BRACES3>\\[^"]		{ flt_bfr_embed(yytext, 1, Action_attr); flt_bfr_append(yytext + 1, yyleng - 1); }
<BRACES3>\"		{ PushQuote(DQUOTED, String_attr); }
<BRACES3>\'		{ PushQuote(SQUOTED, String_attr); }
<BRACES3>{IDENT}	{ flt_bfr_embed(yytext, yyleng, ""); }
<BRACES3>{INTEGER}	{ flt_bfr_embed(yytext, yyleng, ""); }
<BRACES3>"${"		{ begin_braces(yytext, yyleng); }
<BRACES3>{END_BRACES}	{ end_braces(yytext, yyleng); }
<BRACES3>[^}[:alnum:]_\\\"']+ { flt_bfr_embed(yytext, yyleng, ""); }

%%

#include <fltstack.h>

static void
resume_state(void)
{
    switch (FLT_STATE) {
    case DQUOTED:
    case EXPRS:
    case HEREDOC:
	flt_bfr_begin(String_attr);
	break;
    }
}

static void
begin_braces(const char *text, int length)
{
    flt_bfr_finish();
    push_state(BRACES);

    flt_bfr_begin(Ident_attr);
    flt_bfr_append(text, length);
}

static void
braces_operator(const char *text, int length)
{
    /* *INDENT-OFF* */
    static const struct {
	int length;
	const char *text;
    } table[] = {
	{ 3, "::=" },		/*             zsh */
	{ 2, "##" },		/* sh bash ksh zsh */
	{ 2, "%%" },		/* sh bash ksh zsh */
	{ 2, ",," },		/*         ksh     */
	{ 2, "/#" },		/*         ksh     */
	{ 2, "/%" },		/*         ksh     */
	{ 2, "//" },		/*         ksh zsh */
	{ 2, ":#" },		/*             zsh */
	{ 2, ":+" },		/* sh bash ksh zsh */
	{ 2, ":-" },		/* sh bash ksh zsh */
	{ 2, ":=" },		/* sh bash ksh zsh */
	{ 2, ":?" },		/* sh bash ksh zsh */
	{ 2, "^^" },		/*             zsh */
	{ 1, "#" },		/* sh bash ksh zsh */
	{ 1, "%" },		/* sh bash ksh     */
	{ 1, "*" },		/*    bash ksh     */
	{ 1, "+" },		/* sh bash ksh zsh */
	{ 1, "," },		/*         ksh     */
	{ 1, "-" },		/* sh      ksh zsh */
	{ 1, "!" },		/*    bash ksh     */
	{ 1, "/" },		/*    bash ksh zsh */
	{ 1, ":" },		/*    bash ksh     */
	{ 1, "=" },		/* sh      ksh zsh */
	{ 1, "?" },		/* sh bash ksh zsh */
	{ 1, "@" },		/*    bash ksh     */
	{ 1, "~" },		/*             zsh */
	{ 1, "^" },		/*             zsh */
    };
    /* *INDENT-ON* */

    unsigned n;
    const char *attr = Error_attr;

    for (n = 0; n < TABLESIZE(table); ++n) {
	if (table[n].length <= length) {
	    if (!strncmp(table[n].text, text, (size_t) table[n].length)) {
		attr = Action_attr;
		if (length > table[n].length) {
		    char *temp = strmalloc(text);
		    while (length > table[n].length) {
			unput(temp[--length]);
		    }
		    free(temp);
		}
		break;
	    }
	}
    }

    flt_bfr_embed(text, length, attr);
    new_state(BRACES3);
}

static void
end_braces(const char *text, int length)
{
    flt_bfr_embed(text, length, Ident_attr);
    flt_bfr_finish();

    pop_state();
    resume_state();
}

static int
pop_backtic1(const char *text, int len, int test)
{
    int result = 0;

    if (FLTSTACK_OK)
	result = (FLTSTACK_THIS.backtic == test);
    if (result) {
	handle_backtic(text, len, test, 0);
    }
    return result;
}

/*
 * test = the length of the "push" string, e.g., 1="`", 2="$(".
 * which = 1 for push, 0 for pop, -1 for either.
 */
static void
handle_backtic(const char *text, int len, int test, int which)
{
    flt_bfr_finish();
    flt_puts(text, len, Action_attr);
    if (FLTSTACK_OK && (which <= 0) && (FLTSTACK_THIS.backtic == test)) {
	pop_state();
	resume_state();
    } else {
	flt_bfr_finish();
	switch (test) {
	case LEN_BACKTIC2_DOWN:
	    push_state(EXPRS);
	    flt_bfr_begin(String_attr);
	    break;
	default:
	    push_state(NORMAL);
	    break;
	}
	if (FLTSTACK_OK)
	    FLTSTACK_THIS.backtic = len;
    }
}

static void
save_here(const char *text, int length)
{
    char *s;
    int delim = 0;

    here_exp = 1;
    DPRINTF(("save_here '%.*s' ", length, text));
    if ((here_tag = do_alloc(here_tag, (size_t) length, &here_len)) != 0) {
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

static void
embed_or_append(const char *text, int length)
{
    if (here_exp) {
	begin_braces(text, length);
    } else {
	flt_bfr_append(text, length);
    }
}

/*
 * Korn shell allows dots in variable names; Bourne does not.
 */
static int
vname_allows_dots(const char *text, int length)
{
    int result = length;

    if (!FltOptions('K')) {
	char *dot = strchr(text, '.');

	if (dot != 0) {
	    result = (int) (dot - text);
	    if (result == (*text != '$'))
		++result;
	}
    }
    return result;
}

static void
embed_vname(const char *text, int length, const char *dft)
{
    int actual = vname_allows_dots(text, length);

    if (actual != 0)
	flt_bfr_embed(text, actual, Ident_attr);
    if (actual < length) {
	flt_bfr_embed(text + actual, length - actual, dft);
    }
}

static void
write_vname(const char *text, int length)
{
    int actual = vname_allows_dots(text, length);

    if (actual != 0)
	flt_puts(text, actual, Ident_attr);
    if (actual < length) {
	flt_puts(text + actual, length - actual, "");
    }
}

static int
got_here(const char *text, int length)
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

    delim[0] = text[--size];	/* save the trailing delimiter */
    delim[1] = text[size] = '\0';	/* chop the trailing delimiter */
    if (size)
	flt_puts(text, size, "");
    flt_puts(delim, 1, attr);
}

static void
do_filter(FILE *inputs)
{
    InitLEX(inputs);

    Action_attr = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    here_exp = 0;
    begin_state(NORMAL);

    RunLEX();
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
