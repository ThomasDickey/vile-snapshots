%s QUOTED

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/rcs-filt.l,v 1.4 2000/02/10 11:30:12 tom Exp $
 *
 * Filter to add vile "attribution" sequences to RCS archives.
 */

#include <filters.h>

DefineFilter("rcs");

static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

/*
 * Returns true if the parameter points to an identifier
 */
static int
is_ident(char *src)
{
    char *base = src;
    while (isalnum(*src))
	src++;
    return (src != base && (*src == ':' || *src == '$')) ;
}

/*
 * Find an embedded RCS keyword
 */
static char *
find_ident(char *first)
{
    char *dollar = strchr(first, '$');

    if (dollar != 0
     && is_ident(dollar+1)
     && strchr(dollar+1, '$')) {
	return dollar;
    }

    return 0;
}

/*
 * Buffer text for quoted-strings, so we can highlight the embedded RCS
 * keywords.
 */
static char    *save_bfr;
static unsigned save_max;
static unsigned save_len;

static void
add_to_quoted(char *text, int length)
{
    save_bfr = do_alloc(save_bfr, save_len + length + 1, &save_max);
    strncpy(save_bfr + save_len, text, length + 1);
    save_len += length;
}

static void
flush_quoted(void)
{
    if (save_len) {
	char *base = save_bfr;
	char *last = save_bfr + save_len;
	while (base < last) {
	    char *ident = find_ident(base);
	    if (ident != 0) {
		char *next = strchr(ident+1, '$');
		if (ident != base)
		    write_token(yyout, base, ident - base, String_attr);
		base = next + 1;
		write_token(yyout, ident, base - ident, Ident2_attr);
	    } else {
		write_token(yyout, base, last - base, String_attr);
		break;
	    }
	}
	save_len = 0;
    }
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;

    Ident2_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    while (yylex() > 0) {
    }
}

%}

NUMBER		[0-9]+(\.[0-9]+)*
NAME		[a-zA-Z0-9]+

%%

<INITIAL>[\n]		{ ECHO; }
<INITIAL>{NAME}		{ WriteToken(keyword_attr(yytext)); }
<INITIAL>{NUMBER}	{ WriteToken(Number_attr); }
<INITIAL>"@"		{ add_to_quoted(yytext, yyleng); BEGIN(QUOTED); }
<QUOTED>[^@\n]*		{ add_to_quoted(yytext, yyleng); }
<QUOTED>@		{ add_to_quoted(yytext, yyleng); flush_quoted(); BEGIN(INITIAL); }
<QUOTED>[\n]		{ flush_quoted(); ECHO; }
