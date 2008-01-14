%pointer
%x CODE HERE PATTERN POD IGNORED

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/perlfilt.l,v 1.49 2008/01/12 22:59:55 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of PERL input
 * text.
 *
 * This was based on a version written by Pierre Dittgen (dittgen@univ-mlv.fr)
 * - T.Dickey
 */

#include <filters.h>

DefineFilter("pl");

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

static int add_to_pattern(char *text);
static void end_pattern(void);
static void save_here(char *text, int length);

%}

BLANK		[ \t]*

ESCAPED		\\
SSTRING		\'(\\.|\\\n|[^'\\])*\'
DSTRING		\"(\\.|\\\n|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

KEYWORD		[[:alpha:]_][[:alnum:]_]*
QIDENT		["'`]?[[:alpha:]_][[:alnum:]_]+["'`]?

NORMALVARS	[\$%@][[:alnum:]_]+('[[:alnum:]]+)?
OTHERVARS	(\$[-_.\/,"\\#%=~|\$?\&`'+*\[\];!@<>():])|(\$\^[[:graph:]]?)
IDENT		{NORMALVARS}|{OTHERVARS}

SIGN		[-+]
DECIMAL		[[:digit:]_]+
OCTAL		0[0-7_]+
BINARY		0b[01_]+
HEXADECIMAL	0x[[:xdigit:]_]+
VERSION		v[[:digit:]_]+(\.[[:digit:]_]+)*
REAL		[-+]?([[:digit:]_]*\.[[:digit:]][[:digit:]_]*)([eE][+-]?[[:digit:]_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL}|{BINARY}|{VERSION})

%%

<CODE>"<<"{QIDENT}	{
			    BEGIN(HERE);
			    save_here(yytext, yyleng);
			    flt_bfr_append(yytext, yyleng);
			}
<HERE>{IDENT}		{
			    if (here_exp) {
				flt_bfr_finish();
				WriteToken(Ident_attr);
			    } else {
				flt_bfr_append(yytext, yyleng);
			    }
			}
<HERE>^{QIDENT}		{
			    flt_bfr_append(yytext, yyleng);
			    if (here_tag != 0 && !strcmp(here_tag, yytext)) {
				flt_bfr_finish();
				BEGIN(CODE);
			    }
			}
<HERE>\\.		|
<HERE>[^\r\n]		{ flt_bfr_append(yytext, yyleng); }
<HERE>\n		{ flt_bfr_append(yytext, yyleng); }

<CODE>[!=]"~"{BLANK}	{ WriteToken(""); BEGIN(PATTERN); }
<PATTERN>[^\r\n]	{ if (!add_to_pattern(yytext)) { end_pattern(); ECHO; } }
<PATTERN>[\n]		{ end_pattern(); ECHO; }

<CODE>^{BLANK}*#!.*$	{ WriteToken(Preproc_attr); }

<CODE>{NUMBER}		{ WriteToken(Number_attr); }

<CODE>-[[:alpha:]]	{ WriteToken(Keyword_attr); }

<CODE>{KEYWORD}		{ WriteToken(keyword_attr(yytext)); if (!strcmp(yytext, "__END__")) { BEGIN(IGNORED); } }

<CODE>{BLANK}*"#".*$	{ WriteToken(Comment_attr); }

<CODE>{IDENT}		{ WriteToken(Ident_attr); }

<CODE>{ESCAPED}		|
<CODE>{STRINGS}		{ WriteToken(String_attr); }

<IGNORED>[^\r\n]*	{ WriteToken(Comment_attr); }

<CODE>\n\n=[[:lower:]].* {
			    flt_puts("\n\n", 2, "");
			    WriteToken2(Comment_attr,2);
			    BEGIN(POD);
			}
<POD>^=cut{BLANK}\n	{ WriteToken(Comment_attr); BEGIN(CODE); }
<POD>.*			{ WriteToken(Comment_attr); }

%%

static void
save_here(char *text, int length)
{
    char *s;

    here_tag = do_alloc(here_tag, length, &here_len);
    here_exp = 1;
    if ((s = here_tag) != 0) {
	while (length--) {
	    if (isIdent(CharOf(*text))) {
		*s++ = *text;
	    } else if (*text == '\'') {
		here_exp = 0;
	    }
	    text++;
	}
	*s = 0;
    }
    flt_bfr_begin(String_attr);
}

static void
end_pattern(void)
{
    flt_bfr_finish();
    BEGIN(CODE);
}

static int
add_to_pattern(char *text)
{
    static int first, delim, count, escaped, need;

    if (!flt_bfr_length()) {
	first = delim = count = escaped = need = 0;
	if (isalpha(CharOf(*text)))
	    first = *text;
	else
	    first = delim = *text;
	if (delim)
	    need = 2;
    }
    if (flt_bfr_length() == 1 && !delim) {
	if (!isalpha(CharOf(*text)) && !isspace(CharOf(*text))) {
	    delim = *text;
	    need = (first == 's' || first == 'y' || first == 't') ? 3 : 2;
	}
    }
    if (escaped) {
	escaped = 0;
    } else {
	if (*text == '\\') {
	    escaped = 1;
	} else {
	    if (need && (count == need)) {
		if (!isalpha(CharOf(*text))) {
		    return 0;
		}
	    } else if (*text == delim) {
		count++;
	    }
	}
    }
    if (delim == 0 && !escaped && *text == ';') {
	return 0;
    }
    flt_bfr_append(text, 1);
    return 1;
}

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Comment_attr = class_attr(NAME_COMMENT);
    Preproc_attr = class_attr(NAME_PREPROC);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Ident_attr   = class_attr(NAME_IDENT);
    String_attr  = class_attr(NAME_LITERAL);
    Number_attr  = class_attr(NAME_NUMBER);

    here_exp = 0;

    BEGIN(CODE);
    while (yylex() > 0) {
    }
    flt_bfr_error();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
    flt_free(&here_tag, &here_len);
}
#endif
