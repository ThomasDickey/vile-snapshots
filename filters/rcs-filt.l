%pointer
%s QUOTED

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/rcs-filt.l,v 1.16 2008/11/19 23:11:46 tom Exp $
 *
 * Filter to add vile "attribution" sequences to RCS archives.
 */

#include <filters.h>

DefineFilter("rcs");

static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

static void add_to_quoted(char *text, int length);
static void flush_quoted(void);

%}

NUMBER		[[:digit:]]+(\.[[:digit:]]+)*
NAME		[[:alnum:]]+

%%

<INITIAL>[\n]		{ ECHO; }
<INITIAL>{NAME}		{ WriteToken(keyword_attr(yytext)); }
<INITIAL>{NUMBER}	{ WriteToken(Number_attr); }
<INITIAL>"@"		{ add_to_quoted(yytext, yyleng); BEGIN(QUOTED); }
<QUOTED>[^@\r\n]*	{ add_to_quoted(yytext, yyleng); }
<QUOTED>@		{ add_to_quoted(yytext, yyleng); flush_quoted(); BEGIN(INITIAL); }
<QUOTED>[\n]		{ flush_quoted(); ECHO; }

%%

/*
 * Returns true if the parameter points to an identifier
 */
static int
is_ident(char *src)
{
    char *base = src;
    while (isalnum(CharOf(*src)))
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
    if ((save_bfr = do_alloc(save_bfr, save_len + length + 1, &save_max)) != 0) {
	strncpy(save_bfr + save_len, text, length + 1);
	save_len += length;
    } else {
	save_len = 0;
    }
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
		    flt_puts(base, ident - base, String_attr);
		base = next + 1;
		flt_puts(ident, base - ident, Ident2_attr);
	    } else {
		flt_puts(base, last - base, String_attr);
		break;
	    }
	}
	save_len = 0;
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

    Ident2_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    BEGIN(INITIAL);
    RunLEX();
    flush_quoted();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
    flt_free(&save_bfr, &save_max);
}
#endif
