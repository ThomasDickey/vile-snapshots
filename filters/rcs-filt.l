%pointer
%s QUOTED

%{

/*
 * $Id: rcs-filt.l,v 1.25 2025/01/26 11:36:23 tom Exp $
 *
 * Filter to add vile "attribution" sequences to RCS archives.
 */

#include <filters.h>

DefineFilter(rcs);

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
<INITIAL>{NAME}		{ WriteToken(get_keyword_attr(yytext)); }
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
    return (src != base && (*src == ':' || *src == '$'));
}

/*
 * Find an embedded RCS keyword
 */
static char *
find_ident(char *first)
{
    char *dollar = strchr(first, '$');

    if (dollar != NULL
	&& is_ident(dollar + 1)
	&& strchr(dollar + 1, '$')) {
	return dollar;
    }

    return NULL;
}

/*
 * Buffer text for quoted-strings, so we can highlight the embedded RCS
 * keywords.
 */
static char *save_bfr;
static size_t save_max;
static size_t save_len;

static void
add_to_quoted(char *text, int length)
{
    size_t extra = (size_t) length;

    if ((save_bfr = do_alloc(save_bfr, save_len + extra + 1, &save_max)) != NULL) {
	strncpy(save_bfr + save_len, text, extra + 1);
	save_len += extra;
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
	    if (ident != NULL) {
		char *next = strchr(ident + 1, '$');
		if (ident != base)
		    flt_puts(base, (int) (ident - base), String_attr);
		base = next + 1;
		flt_puts(ident, (int) (base - ident), Ident2_attr);
	    } else {
		flt_puts(base, (int) (last - base), String_attr);
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
