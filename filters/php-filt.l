%pointer
%x HTML CODE COMMENT HEREDOC NOWDOC

%{

/*
 * $Id: php-filt.l,v 1.28 2025/01/26 11:35:28 tom Exp $
 *
 * Filter to add vile "attribution" sequences to PHP scripts.
 * - T.Dickey
 *
 * see
 *	http://www.php.net/manual/en/index.php
 *
 * todo:
 *	Complex (curly) syntax
 *	default initialization to html mode unless preamble line is "/php", etc
 */

#include <filters.h>
#include <fltstack.h>

DefineFilter(php);

#define isQuote(ch) (ch != 0 && strchr("'", ch) != NULL)

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

static char *here_tag;
static size_t here_len;
static int here_exp;

static int got_here(char *text, int length);
static void save_here(char *text, int length);

%}

BLANK		[[:blank:]]*

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\]|\\\n)*\"
STRINGS		({SSTRING}|{DSTRING})

IDENT		[[:alpha:]_][[:alnum:]_]*

SIGN		[-+]
DECIMAL		[[:digit:]_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[[:xdigit:]_]+
REAL		[-+]?([[:digit:]_]*\.[[:digit:]][[:digit:]_]*)([eE][+-]?[[:digit:]_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

%%

<HTML>"<?"("[pP][hH][pP]")? { WriteToken(Action_attr); push_state(CODE); }
<CODE>"?>"		{ WriteToken(Action_attr); pop_state(); }

<CODE>"${"{IDENT}"}"	|
<CODE>"$"{IDENT}	{ WriteToken(Ident2_attr); }
<CODE>{IDENT}		{ WriteToken(get_keyword_attr(yytext)); }
<CODE>";"		{ ECHO; }

<CODE>"/*"		{ BeginQuote(COMMENT, Comment_attr); }
<COMMENT>[^*]*		{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"*"+[^*/]*	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"*"+"/"	{ FinishQuote(CODE); }

<CODE>"#"[^\r\n]*	{ WriteToken(Comment_attr); }
<CODE>"//"[^\r\n]*	{ WriteToken(Comment_attr); }

<CODE>{STRINGS}		{ WriteToken(String_attr); }

<CODE>{NUMBER}		{ WriteToken(Number_attr); }

<CODE>\<\<\<{BLANK}*{IDENT} {
			    save_here(yytext, yyleng);
			    push_state(HEREDOC);
			    BeginQuote(HEREDOC, String_attr);
			}
<HEREDOC>^{IDENT}[;]? {
			    int used = 0;
			    if (got_here(yytext + used, yyleng - used)) {
				flt_bfr_finish();
				pop_state();
			    } else {
				flt_bfr_append(yytext, yyleng);
			    }
			}
<HEREDOC>"${"{IDENT}"}"	|
<HEREDOC>[$]{IDENT}	{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }
<HEREDOC>\\.		{ flt_bfr_append(yytext, yyleng); }
<HEREDOC>[\r\n]		{ flt_bfr_append(yytext, yyleng); }
<HEREDOC>.		{ flt_bfr_append(yytext, yyleng); }

<CODE>\<\<\<{BLANK}*'{IDENT}' {
			    save_here(yytext, yyleng);
			    push_state(NOWDOC);
			    BeginQuote(NOWDOC, String_attr);
			}
<NOWDOC>^{IDENT}[;]? {
			    int used = 0;
			    if (got_here(yytext + used, yyleng - used)) {
				flt_bfr_finish();
				pop_state();
			    } else {
				flt_bfr_append(yytext, yyleng);
			    }
			}
<NOWDOC>[\r\n]		{ flt_bfr_append(yytext, yyleng); }
<NOWDOC>.		{ flt_bfr_append(yytext, yyleng); }

%%

#include <fltstack.h>

static int
got_here(char *text, int length)
{
    int pass, j, k;

    for (pass = 0; pass < 2; pass++) {
	for (j = k = 0; j < length; j++) {
	    if (isQuote(text[j])) {
		if (pass) {
		    flt_error("unexpected quote");
		    flt_bfr_embed(text + j, 1, Error_attr);
		}
	    } else if (text[j] == ';') {
		if (pass)
		    flt_bfr_append(text + j, 1);
	    } else {
		if (here_tag != NULL && (text[j] != here_tag[k++]))
		    return 0;
		if (pass)
		    flt_bfr_append(text + j, 1);
	    }
	}
    }
    return 1;
}

static void
save_here(char *text, int length)
{
    char *s;

    here_exp = 1;
    if ((here_tag = do_alloc(here_tag, (size_t) length, &here_len)) != NULL) {
	s = here_tag;
	while (length--) {
	    if (isQuote(*text)) {
		here_exp = 0;
	    } else if (strchr(" \t", *text) != NULL) {
		if (s != here_tag)
		    break;
	    } else if (strchr("<", *text) == NULL) {
		*s++ = *text;
	    }
	    text++;
	}
	*s = 0;
    }
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

    Action_attr = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident2_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    begin_state(HTML);
    RunLEX();
    flt_bfr_error();
    end_state();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
