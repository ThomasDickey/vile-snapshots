%x CODE HERE PATTERN POD IGNORED

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/perlfilt.l,v 1.29 2000/06/18 19:56:28 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of PERL input
 * text.
 *
 * This was based on a version written by Pierre Dittgen (dittgen@univ-mlv.fr)
 * - T.Dickey
 */

#include <filters.h>

DefineFilter("perl");

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
	flt_puts(save_bfr, save_len, String_attr);
	save_len = 0;
    }
}

static void
end_pattern(void)
{
    flush_here();
    BEGIN(CODE);
}

static int
add_to_pattern(char *text)
{
    static int first, delim, count, escaped, need;

    if (!save_len) {
	first = delim = count = escaped = need = 0;
	if (isalpha(*text))
	    first = *text;
	else
	    first = delim = *text;
	if (delim)
	    need = 2;
    }
    if (save_len == 1 && !delim) {
	if (!isalpha(*text) && !isspace(*text)) {
	    delim = *text;
	    need = (first == 's') ? 3 : 2;
	}
    }
    if (escaped) {
	escaped = 0;
    } else {
	if (*text == '\\') {
	    escaped = 1;
	} else {
	    if (need && (count == need)) {
		if (!isalpha(*text)) {
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
    add_to_here(text, 1);
    return 1;
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
    yyin = input;
    BEGIN(CODE);
    Comment_attr = class_attr(NAME_COMMENT);
    Preproc_attr = class_attr(NAME_PREPROC);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Ident_attr   = class_attr(NAME_IDENT);
    String_attr  = class_attr(NAME_LITERAL);
    Number_attr  = class_attr(NAME_NUMBER);
    while (yylex() > 0) {
    }
}

%}

BLANK		[ \t]*

ESCAPED		\\
SSTRING		\'(\\.|\\\n|[^'\\])*\'
DSTRING		\"(\\.|\\\n|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

KEYWORD		[a-zA-Z_][a-zA-Z_0-9]*
QIDENT		[a-zA-Z_0-9"']+

NORMALVARS	[\$%@][a-zA-Z_0-9]+('[a-zA-Z0-9]+)?
OTHERVARS	(\$[-_.\/,"\\#%=~|\$?&`'+*\[\];!@<>():])|(\$\^[@-\177]?)
IDENT		{NORMALVARS}|{OTHERVARS}

SIGN		[-+]
DECIMAL		[0-9_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[0-9a-fA-F_]+
REAL		[-+]?([0-9_]*\.[0-9][0-9_]*)([eE][+-]?[0-9_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

%%

<CODE><<{BLANK}{QIDENT}	{
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
<HERE>\\.		|
<HERE>[^\n]		{ add_to_here(yytext, yyleng); }
<HERE>\n		{
			    flush_here();
			    flt_putc(yytext[0]);
			}

<CODE>[!=]"~"{BLANK}	{ WriteToken(""); BEGIN(PATTERN); }
<PATTERN>[^\n]		{ if (!add_to_pattern(yytext)) { end_pattern(); ECHO; } }
<PATTERN>[\n]		{ end_pattern(); ECHO; }

<CODE>^{BLANK}*#!.*$	{ WriteToken(Preproc_attr); }

<CODE>-[a-zA-Z]		{ WriteToken(Keyword_attr); }
<CODE>{KEYWORD}		{ WriteToken(keyword_attr(yytext)); if (!strcmp(yytext, "__END__")) { BEGIN(IGNORED); } }

<CODE>{BLANK}*"#".*$	{ WriteToken(Comment_attr); }

<CODE>{NUMBER}		{ WriteToken(Number_attr); }

<CODE>{IDENT}		{ WriteToken(Ident_attr); }

<CODE>{ESCAPED}		|
<CODE>{STRINGS}		{ WriteToken(String_attr); }

<IGNORED>[^\n]*		{ WriteToken(Comment_attr); }

<CODE>\n\n=[a-z].*	{
			    flt_puts("\n\n", 2, "");
			    WriteToken2(Comment_attr,2);
			    BEGIN(POD);
			}
<POD>^=cut{BLANK}\n	{ WriteToken(Comment_attr); BEGIN(CODE); }
<POD>.*			{ WriteToken(Comment_attr); }
