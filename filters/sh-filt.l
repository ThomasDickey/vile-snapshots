%pointer
%x NORMAL HEREDOC HEREDOC2 DQUOTED SQUOTED EXPRS BRACES BRACES2 BRACES3 KSH_CQUOTE KSH_MATH

%e 1500
%p 4000
%o 10000
%a 5000
%k 2000

%{

/*
 * $Id: sh-filt.l,v 1.186 2025/01/27 23:36:13 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Shell script.
 */

#include <filters.h>
#undef YY_NO_UNPUT

#define FLTSTACK_EXTRA		int backtic; \
				int action;
#define FLTSTACK_EXTRA_PUSH	FLTSTACK_THIS.backtic = 0;\
				FLTSTACK_THIS.action = 0;
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

#define LEN_KSH_TEST_DOWN 2	/* "[[" will expect "]]" */
#define LEN_BACKTIC1_DOWN 2	/* "$(" */
#define LEN_BACKTIC2_DOWN 3	/* "$((" */
#define LEN_BRACEGRP_DOWN 3	/* "$("{space} */

#define isQuote(ch) (ch != 0 && strchr("\\'\"", ch) != NULL)

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
static int here_next;

static int pop_backtic1(const char *text, int len, int test);
static int is_numeric(const char *text, int len);
static void begin_braces(const char *text, int length);
static void braces_operator(const char *text, int length);
static void check_heredoc(char *value, int length);
static void color_delimiter(char *text, int size, char *attr);
static void embed_or_append(const char *text, int length);
static void embed_vname(const char *text, int length, const char *dft);
static void end_braces(const char *text, int length);
static void handle_backtic2(const char *text, int length, int test, int which);
static void handle_backtic1(const char *text, int length, int test, int which);
static void handle_parens(const char *text, int length, int test, int which, int state);
static void resume_state(void);
static void save_here(const char *text, int length);
static void write_vname(const char *text, int length);

%}

SPACE		[[:blank:]]
CRLF		[\r\n]

COMMENT		"#"[^\r\n]*

SSTRING		\'([^']|\n)*\'
DSTRING		\"([^"]|\n)*\"

NAME0		[[:alpha:]_]
NAME		[[:alnum:]_]
NAME2		({NAME}|"-")

WILDCARD	(\?|\*)
WILDNAME	({NAME}|{WILDCARD})
WILDGLUE	({WILDCARD}[#]?)
FILENAME	(([./]+{WILDNAME}+)|({WILDNAME}+[./]+{WILDNAME}*)|({NAME}*{WILDCARD})|{WILDGLUE}|\.+\/+)+

SIGN		[-+]

C_INT8		0[0-7]+
C_INT10		[[:digit:]]+
C_INT16		0x[[:xdigit:]]+
C_INTEGER	({C_INT10}|{C_INT8}|{C_INT16})
C_EXP		[efgEFG]{SIGN}?{C_INTEGER}
C_FLOAT		((({C_INT10}"."?)({C_INT10})?)|(({C_INT10})?("."{C_INT10}))){C_EXP}?
KSH_NUMBER	{SIGN}?({C_INTEGER}|{C_FLOAT})
BASExx_NUMBER	[0-9]+#[[:alnum:]@_]+
SHELL_NUMBER	({KSH_NUMBER}|{BASExx_NUMBER})
MAYBE_NOT	[.[:alnum:]\/*?_-]*
MAYBE_NUMBER	({MAYBE_NOT}{SHELL_NUMBER}{MAYBE_NOT})

SQUARE_DOWN	"["
SQUARE_UP	"]"

KSH_INDEX	"["[#*@]"]"

KSH_TEST_DOWN	"[["
KSH_TEST_UP	"]]"

BACKTIC2_DOWN	"$(("
BACKTIC2_UP	"))"
BACKTIC1_DOWN	"$("
BACKTIC1_UP	")"

BRACEGRP_DOWN	"${"[ \t\r\n]
BRACEGRP_UP	"}"

BACKTIC		`
ACTION		[\.{}]

BRACES		"${"
END_BRACES	"}"

IDENT		{NAME0}{NAME}*
QIDENT		{SSTRING}|{DSTRING}|[^ \"'$\t\r\n(){}]+
IDENT0		[-]+[[:digit:]]*{NAME0}+{NAME2}*

VNAME		[.]?{IDENT}([.]{IDENT})*

IDENT1		\${NAME}+
IDENT2		\$({NAME}+|{VNAME})
IDENTX		\$[\*@#\?\$!-]

DASHIDENT	{IDENT}("-"{NAME}{NAME2}*)*

%%

<NORMAL>{MAYBE_NUMBER}	{ if (is_numeric(yytext, yyleng)) {
			    WriteToken(Number_attr);
			  } else {
			    WriteToken("");
			  }
			}

<NORMAL>{KSH_INDEX}	{ flt_putc(L_BLOCK);
			  flt_puts(yytext + 1, 1, Action_attr);
			  flt_putc(R_BLOCK);
			}

<NORMAL>{IDENT0}	|
<NORMAL>{FILENAME}	{ WriteToken(""); /* exclude from other classes */ }

<NORMAL>{DASHIDENT}	{ WriteToken(get_keyword_attr(yytext)); }

<NORMAL>{SPACE}{COMMENT} {
			  WriteToken3("",1);
			  WriteToken2(Comment_attr,1);
			}
<NORMAL>^{COMMENT}	{ WriteToken(Comment_attr); }

<NORMAL>^{SPACE}*:	{ color_delimiter(yytext, yyleng, Action_attr); }

<NORMAL>{BRACEGRP_DOWN}	{ if (FltOptions('K')) {
			    handle_backtic1(yytext, yyleng, yyleng, 1);
			  } else {
			    ECHO;
			  }
			}
<NORMAL>{BRACEGRP_UP}	{ if (FltOptions('K')) {
			    if (!pop_backtic1(yytext, yyleng, LEN_BRACEGRP_DOWN)) {
			      WriteToken(Action_attr);
			    }
			  } else {
			    WriteToken(Action_attr);
			  }
			}

<NORMAL>{BRACES}	{ begin_braces(yytext, yyleng); }

<NORMAL>{IDENT2}	{ write_vname(yytext, yyleng); }
<NORMAL>{IDENTX}	{ WriteToken(Ident_attr); }

<NORMAL>{IDENT}"="	{ flt_puts(yytext, yyleng-1, Ident_attr); flt_putc('='); }

<NORMAL>\\.		|
<NORMAL>{SSTRING}	{ WriteToken(String_attr); }
<NORMAL>\'		{ PushQuote(SQUOTED, String_attr); }
<NORMAL>\"		{ PushQuote(DQUOTED, String_attr); }

<NORMAL>{BACKTIC2_UP}	{ if (!pop_backtic1(yytext, yyleng, LEN_BACKTIC2_DOWN)) {
			    FLEX_PRINTF((stderr, "cannot pop '$((' level %d\n", stk_level));
			    if (!pop_backtic1(yytext, 1, LEN_BACKTIC1_DOWN)) {
			      FLEX_PRINTF((stderr, "cannot pop '$(' level %d\n", stk_level));
			      if (FLTSTACK_USED && stk_state[stk_level-1].state == NORMAL) {
			        pop_state();
				resume_state();
			      }
			      flt_putc(R_PAREN);
			    }
			    unput(R_PAREN);
			  }
			}
<NORMAL>{KSH_TEST_UP}	{ if (!pop_backtic1(yytext, yyleng, LEN_KSH_TEST_DOWN)) {
			    FLEX_PRINTF((stderr, "cannot pop '[[' level %d ...']]'\n", stk_level));
			    handle_parens(yytext, 1, 1, -1, NORMAL);
			    unput(R_BLOCK);
			  }
			}
<NORMAL>{BACKTIC1_UP}	{ if (!pop_backtic1(yytext, yyleng, LEN_BACKTIC1_DOWN)) {
			    FLEX_PRINTF((stderr, "cannot pop '$(' level %d\n", stk_level));
			    if (FLTSTACK_USED && stk_state[stk_level-1].state == NORMAL) {
			      pop_state();
			      resume_state();
			    }
			    ECHO;
			  }
			}
<NORMAL>{BACKTIC2_DOWN} { handle_backtic2(yytext, yyleng, yyleng, 1); }
<NORMAL>{BACKTIC1_DOWN}	{ handle_backtic1(yytext, yyleng, yyleng, 1); }
<NORMAL>{BACKTIC}	{ handle_backtic1(yytext, yyleng, yyleng, -1); }
<NORMAL>{KSH_TEST_DOWN}	{ handle_backtic1(yytext, yyleng, yyleng, 1); }
<NORMAL>{ACTION}	{ WriteToken(Action_attr); }
<NORMAL>"<#"		{
			  if (FltOptions('K')) {
			    WriteToken3("", 1);
			    WriteToken2(Action_attr, 1);
			  } else {
			    WriteToken2(Action_attr, 1);
			    unput('#');
			  }
			}

<NORMAL>{SQUARE_DOWN}	{ handle_parens(yytext, yyleng, yyleng, 1, NORMAL); }
<NORMAL>{SQUARE_UP}	{ handle_parens(yytext, yyleng, yyleng, -1, NORMAL); }

<NORMAL,EXPRS>\<\<\<[-]?{SPACE}*({QIDENT}|{IDENT1}) {
			  if (FltOptions('K')) {
			    int n;
			    for (n = 0; n < (int) yyleng; ++n) {
				if (yytext[n] != '<' && yytext[n] != '-')
				    break;
			    }
			    WriteToken3(Action_attr, n);
			    WriteToken2(String_attr, n);
			  } else {
			    flt_error("here-strings are nonstandard");
			    WriteToken(Error_attr);
			  }
			}

<NORMAL>\<\<[-]?{SPACE}*{QIDENT}{CRLF}+ {
			    int size = yyleng;
			    while (size > 0
				   && isspace(CharOf(yytext[size - 1])))
				--size;
			    save_here(yytext, size);
			    push_state(here_exp);
			    flt_puts(yytext, size, String_attr); /* FIXME */
			    flt_bfr_begin(String_attr);
			    flt_bfr_append(yytext + size, yyleng - size);
			}
<NORMAL>\<\<[-]?{SPACE}*{QIDENT} {
			    save_here(yytext, yyleng);
			    here_next = here_exp;
			    WriteToken(String_attr);
			}
<NORMAL>"(("		{ if (FltOptions('K')) {
			    WriteToken(Action_attr);
			    push_state(KSH_MATH);
			  } else {
			    ECHO;
			  }
    			}
<NORMAL>"$'"		{ if (FltOptions('K')) {
			    PushQuote(KSH_CQUOTE, String_attr);
			  } else {
			    ECHO;
			  }
			}
<NORMAL>"("		{ ECHO; push_state(NORMAL); }
<NORMAL>\\[\n]		{ ECHO; }
<NORMAL>[\n]		{ ECHO;
			  if (here_next) {
			    push_state(here_next);
			    here_next = 0;
			    flt_bfr_begin(String_attr);
			  }
			}

<KSH_MATH>"))"		{
			  WriteToken(Action_attr);
			  pop_state();
			  resume_state();
			}
<KSH_MATH>{IDENT1}	|
<KSH_MATH>{VNAME}	{ WriteToken(Ident_attr); }
<KSH_MATH>{BACKTIC2_DOWN} { handle_backtic2(yytext, yyleng, yyleng, 1); }
<KSH_MATH>{BACKTIC1_DOWN} { handle_backtic1(yytext, yyleng, yyleng, 1); }
<KSH_MATH>{BACKTIC}	{ handle_backtic1(yytext, yyleng, yyleng, -1); }
<KSH_MATH>{KSH_TEST_DOWN} { handle_backtic1(yytext, yyleng, yyleng, 1); }
<KSH_MATH>{ACTION}	{ WriteToken(Action_attr); }
<KSH_MATH>{SQUARE_DOWN}	{ handle_parens(yytext, yyleng, yyleng, 1, KSH_MATH); }
<KSH_MATH>{SQUARE_UP}	{ handle_parens(yytext, yyleng, yyleng, -1, KSH_MATH); }
<KSH_MATH>{BRACES}	{ begin_braces(yytext, yyleng); }
<KSH_MATH>{IDENTX}	{ WriteToken(Ident_attr); }
<KSH_MATH>{SHELL_NUMBER} { WriteToken(Number_attr); }
<KSH_MATH>.		{ ECHO; }

<KSH_CQUOTE>[']		{
			  flt_bfr_append("'", 1);
			  flt_bfr_finish();
			  pop_state();
			  resume_state();
			}
<KSH_CQUOTE>\\[0-7]{1,3}  {
			  flt_bfr_embed(yytext, 1, Action_attr);
			  flt_bfr_embed(yytext + 1, yyleng - 1, Number_attr);
			}
<KSH_CQUOTE>\\[[:graph:]] { flt_bfr_embed(yytext, yyleng, Action_attr); }
<KSH_CQUOTE>{CRLF}	|
<KSH_CQUOTE>.		{ flt_bfr_embed(yytext, yyleng, String_attr); }

<HEREDOC>{BACKTIC2_DOWN} { handle_backtic2(yytext, yyleng, yyleng, 1); }
<HEREDOC>{BACKTIC1_DOWN} { handle_backtic1(yytext, yyleng, yyleng, 1); }
<HEREDOC>{BACKTIC}	{ handle_backtic1(yytext, yyleng, yyleng, -1); }
<HEREDOC>{IDENT2}	{ if (here_exp == HEREDOC) {
			      embed_vname(yytext, yyleng, String_attr);
			  } else {
			      flt_bfr_append(yytext, yyleng);
			  }
			}
<HEREDOC>{BRACES}	{ embed_or_append(yytext, yyleng); }
<HEREDOC>^[\t]*{QIDENT}$ { check_heredoc(yytext, yyleng); }
<HEREDOC>\\[\$\`\"\\\n]	{
			  flt_bfr_embed(yytext, 1, Action_attr);
			  flt_bfr_embed(yytext+1, 1, String_attr);
			}
<HEREDOC>[^\r\n`\\\$]+	{ check_heredoc(yytext, yyleng); }
<HEREDOC>\n		{ flt_bfr_append(yytext, yyleng); }
<HEREDOC>.		{ flt_bfr_append(yytext, yyleng); }

<HEREDOC2>^[^\r\n]+	{ check_heredoc(yytext, yyleng); }
<HEREDOC2>{CRLF}+	{ flt_bfr_append(yytext, yyleng); }

<DQUOTED>{BACKTIC1_UP}	{ if (!pop_backtic1(yytext, yyleng, LEN_BACKTIC1_DOWN))
			      flt_bfr_append(yytext, yyleng);
			}
<DQUOTED>{BACKTIC2_DOWN} { handle_backtic2(yytext, yyleng, yyleng, 1); }
<DQUOTED>{BACKTIC1_DOWN} { handle_backtic1(yytext, yyleng, yyleng, 1); }
<DQUOTED>{BACKTIC}	{ handle_backtic1(yytext, yyleng, yyleng, -1); }
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
<EXPRS>{BACKTIC2_DOWN}	{ handle_backtic2(yytext, yyleng, yyleng, 1); }
<EXPRS>{BACKTIC2_UP}	{ handle_backtic2(yytext, yyleng, LEN_BACKTIC2_DOWN, 0); }
<EXPRS>{BACKTIC1_DOWN}	{ handle_backtic1(yytext, yyleng, yyleng, 1); }
<EXPRS>{BACKTIC}	{ handle_backtic1(yytext, yyleng, yyleng, -1); }
<EXPRS>\\[\n]		|
<EXPRS>\\.		{ flt_bfr_embed(yytext, 1, Action_attr); flt_bfr_append(yytext + 1, yyleng - 1); }
<EXPRS>{IDENT2}		{ embed_vname(yytext, yyleng, ""); }
<EXPRS>{BRACES}		{ begin_braces(yytext, yyleng); }
<EXPRS>{C_INT10}	{ flt_bfr_embed(yytext, yyleng, Number_attr); }
<EXPRS>[^`\r\n\\\$)]+	{ flt_bfr_append(yytext, yyleng); }

<BRACES>{END_BRACES}	{ end_braces(yytext, yyleng); }
<BRACES>{VNAME}		{ embed_vname(yytext, yyleng, "");
			  new_state(BRACES2);
			}
<BRACES>{C_INT10}	{ flt_bfr_embed(yytext, yyleng, Ident_attr); new_state(BRACES2); }
<BRACES>[?#!%]		{ flt_bfr_embed(yytext, yyleng, Action_attr); }
<BRACES>.		{ flt_bfr_append(yytext, yyleng); }

<BRACES2>{END_BRACES}	{ end_braces(yytext, yyleng); }
<BRACES2>\'		{ PushQuote(SQUOTED, String_attr); }
<BRACES2>\"		{ PushQuote(DQUOTED, String_attr); }
<BRACES2>"\\"\r?\n	|
<BRACES2>"\\".		{ flt_bfr_embed(yytext, yyleng, Action_attr); }
<BRACES2>"<"		{ flt_bfr_embed(yytext, yyleng, Action_attr);
			  FLTSTACK_THIS.action++;
			}
<BRACES2>[-:=?+*@#%/^,]+ {
			  if (FLTSTACK_THIS.action++) {
			    flt_bfr_append(yytext, yyleng);
			  } else {
			    braces_operator(yytext, yyleng);
			  }
			}
<BRACES2>{KSH_INDEX}	{ flt_bfr_embed(yytext, 1, "");
			  flt_bfr_embed(yytext + 1, 1, Action_attr);
			  flt_bfr_embed(yytext + 2, 1, "");
			}
<BRACES2>{SQUARE_DOWN}	{ handle_parens(yytext, yyleng, yyleng, 1, NORMAL); }
<BRACES2>{SQUARE_UP}	{ handle_parens(yytext, yyleng, yyleng, -1, NORMAL); }
<BRACES2>[^-:=?+*@#%/^,"'}[:blank:][]]+ { flt_bfr_embed(yytext, yyleng, ""); } /* FIXME - error */
<BRACES2>{SPACE}	{ flt_bfr_append(yytext, yyleng); }
<BRACES2>.		|
<BRACES2>{CRLF}		{ flt_bfr_append(yytext, yyleng); }

<BRACES3>{BACKTIC2_DOWN} { handle_backtic2(yytext, yyleng, yyleng, 1); }
<BRACES3>{BACKTIC1_DOWN} { handle_backtic1(yytext, yyleng, yyleng, 1); }
<BRACES3>{BACKTIC}	{ handle_backtic1(yytext, yyleng, yyleng, -1); }
<BRACES3>\\\"		{ flt_bfr_embed(yytext, yyleng, String_attr); }
<BRACES3>\\[^"]		{ flt_bfr_embed(yytext, 1, Action_attr); flt_bfr_append(yytext + 1, yyleng - 1); }
<BRACES3>\"		{ PushQuote(DQUOTED, String_attr); }
<BRACES3>\'		{ PushQuote(SQUOTED, String_attr); }
<BRACES3>{IDENT}	{ flt_bfr_embed(yytext, yyleng, ""); }
<BRACES3>{C_INT10}	{ flt_bfr_embed(yytext, yyleng, ""); }
<BRACES3>{IDENT1}	{ flt_bfr_embed(yytext, yyleng, Ident_attr); }
<BRACES3>"${"		{ begin_braces(yytext, yyleng); }
<BRACES3>"$'"		{ PushQuote(KSH_CQUOTE, String_attr); }
<BRACES3>{END_BRACES}	{ end_braces(yytext, yyleng); }
<BRACES3>"$"		{ flt_bfr_append("$", 1); }
<BRACES3>"{"		{ begin_braces(yytext, yyleng); }
<BRACES3>[^$}[:alnum:]_\\\"']+ { flt_bfr_embed(yytext, yyleng, ""); }

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

    if (FLTSTACK_OK) {
	result = (FLTSTACK_THIS.backtic == test);
    }
    if (result) {
	handle_backtic1(text, len, test, 0);
    }
    return result;
}

/*
 * test = the length of the "push" string, e.g., 1="`", 2="$(".
 * which = 1 for push, 0 for pop, -1 for either.
 */
static void
handle_backtic(const char *text, int len, int test, int which, int state)
{
    flt_bfr_finish();
    flt_puts(text, len, Action_attr);
    if (FLTSTACK_OK && (which <= 0) && (FLTSTACK_THIS.backtic == test)) {
	pop_state();
	resume_state();
    } else {
	flt_bfr_finish();
	/* distinguish "$((" from "$("{space} */
	if ((test == LEN_BACKTIC2_DOWN) && (text[test] == L_PAREN)) {
	    push_state(EXPRS);
	    flt_bfr_begin(String_attr);
	} else {
	    push_state(state);
	}
	if (FLTSTACK_OK)
	    FLTSTACK_THIS.backtic = len;
    }
}

static void
handle_backtic2(const char *text, int len, int test, int which)
{
    handle_backtic(text, len, test, which, KSH_MATH);
}

static void
handle_backtic1(const char *text, int len, int test, int which)
{
    handle_backtic(text, len, test, which, NORMAL);
}

static void
handle_parens(const char *text, int len, int test, int which, int state)
{
    flt_bfr_finish();
    flt_puts(text, len, "");
    if (FLTSTACK_OK && (which <= 0) && (FLTSTACK_THIS.backtic == test)) {
	pop_state();
	resume_state();
    } else {
	push_state(state);
	if (FLTSTACK_OK)
	    FLTSTACK_THIS.backtic = len;
    }
}

static void
save_here(const char *text, int length)
{
    char *s;
    int n;
    int delim = 0;

    DPRINTF(("save_here '%.*s' ", length, text));
    here_exp = HEREDOC;
    strip_tabs = 0;
    for (n = 0; n < (int) yyleng; n++) {
	if (yytext[n] != '<'
	    && !isspace(CharOf(yytext[n]))) {
	    strip_tabs = (yytext[n] == '-');
	    break;
	}
    }
    if ((here_tag = do_alloc(here_tag, (size_t) length, &here_len)) != NULL) {
	s = here_tag;
	/* skip leading "<<" or "<<-", and blanks */
	while (length > 0
	       && (strchr("<->", *text) != NULL
		   || isspace(CharOf(*text)))) {
	    --length;
	    ++text;
	}
	while (length-- > 0) {
	    if ((delim != 0) && (*text == delim)) {
		break;
	    } else if (isQuote(*text)) {
		here_exp = HEREDOC2;
		if (delim == 0) {
		    delim = *text;
		}
	    } else if ((delim == '"') && (*text == '\\')) {
		;
	    } else if ((delim == 0) && (strchr(" \t", *text) != NULL)) {
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
    if (here_exp == HEREDOC) {
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

	if (dot != NULL) {
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

/* MAYBE_NUMBER is over-generous to allow this function to distinguish numbers
 * from almost-numbers (or just names).
 */
#define isoctal(c) ((c) >= '0' && (c) <= '7')
static int
is_numeric(const char *text, int length)
{
    int have_base = 0;
    int have_dot = 0;
    int have_exp = 0;
    int result = 1;
    int radix = 0;
    int j = 0;
    int k;

    if (text[j] == '-') {
	++j;
    } else if (text[j] == '+') {
	++j;
    }
    if (length > 1 && text[j] == '0') {
	if ((length > 2) && (text[j + 1] == 'x')) {
	    radix = 16;
	    j += 2;
	} else {
	    radix = 8;
	    for (k = j + 1; k < length; ++k) {
		if (!isoctal(CharOf(text[k]))) {
		    radix = 0;
		    break;
		}
	    }
	    if (radix)
		j += 1;
	}
    }
    for (k = j; k < length; ++k) {
	if (text[k] == '#') {
	    have_base = 1;
	    break;
	}
    }
    if (FltOptions('K') && have_base) {
	/* BASExx_NUMBER guarantees a number, but after that we validate */
	for (k = j; k < length; ++k) {
	    int ch = CharOf(text[k]);
	    if (ch == '#') {
		have_base = 2;
	    } else if (have_base == 1) {
		radix = (radix * 10) + (ch - '0');
	    } else {
		int check;
		if (radix <= 0 || radix > 64) {
		    result = 0;
		    break;
		} else if (radix <= 36) {
		    check = (isdigit(ch)
			     ? (ch - '0')
			     : (isalpha(ch)
				? (toupper(ch) - 'A' + 10)
				: 64));
		} else {
		    check = (isdigit(ch)
			     ? (ch - '0')
			     : (isalpha(ch)
				? (islower(ch)
				   ? (ch - 'a' + 10)
				   : (ch - 'A' + 36))
				: ((ch == '@')
				   ? 62
				   : 63)));
		}
		if (check >= radix) {
		    result = 0;
		    break;
		}
	    }
	}
    } else {
	const char *ExpFlags = (radix == 16) ? "pP" : "efgEFG";
	for (k = j; k < length; ++k) {
	    int ch = CharOf(text[k]);
	    if (radix == 16 && !have_exp && isxdigit(ch)) {
		;
	    } else if (radix == 8) {
		if (!isoctal(ch)) {
		    result = 0;
		    break;
		}
	    } else if ((have_exp == 0) && (ch == '.')) {
		have_dot = 1;
	    } else if ((have_exp == 0) && strchr(ExpFlags, ch) != NULL) {
		have_exp = 1;
	    } else if ((have_exp == 1) && strchr("+-", ch) != NULL) {
		have_exp = 2;
	    } else if (isdigit(ch)) {
		if (have_exp)
		    have_exp = 4;
	    } else {
		result = 0;
		break;
	    }
	}

	if (FltOptions('K')) {
	    if (have_exp && (have_exp != 4)) {
		result = 0;
	    }
	} else if (have_dot || have_exp) {
	    result = 0;
	}
    }

    return result;
}

static int
got_here(const char *text, int length)
{
    int pass, j, k;
    int rc = 0;

    if (here_tag != NULL) {
	for (pass = 0; pass < 2; pass++) {
	    for (j = 0; j < length; j++) {
		if (here_tag[j] == '\0') {
		    /*
		     * Check for special case of $(xxx) handled below.
		     */
		    if (stk_level < 2
			|| text[j] != R_PAREN) {
			return 0;
		    }
		} else if (text[j] != here_tag[j]) {
		    return 0;
		}
		if (pass) {
		    if (here_tag[j] == '\0') {
			/*
			 * This would be used if we have a here-document inside
			 * a $(xxx) command, which ends with its trailing
			 * parenthesis on the line with the matching tag.
			 */
			for (k = length - 1; k >= j; --k) {
			    unput(text[k]);
			}
			break;
		    }
		} else if (here_tag[j] == '\0') {
		    break;
		}
	    }
	    if (here_tag[j] != '\0') {
		return 0;
	    }
	}
	rc = 1;
    }
    return rc;
}

static char *
skip_tabs(char *value)
{
    while (*value == '\t')
	value++;
    return value;
}

static void
check_heredoc(char *value, int length)
{
    int used = 0;
    if (strip_tabs) {
	used = (int) (skip_tabs(value) - (char *) value);
	if (used != 0)
	    flt_bfr_append(value, used);
    }
    if (got_here(value + used, length - used)) {
	flt_bfr_append(value + used, length - used);
	flt_bfr_finish();
	pop_state();
	resume_state();
	FreeAndNull(here_tag);
	here_exp = 0;
	here_next = 0;
    } else {
	flt_bfr_append(value + used, length - used);
    }
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
    here_next = 0;
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
