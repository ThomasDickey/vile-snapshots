%pointer
%x NORMAL COMMENT QUOTE1 QUOTE2

%a 6000
%o 10000

%{

/*
 * $Id: makefilt.l,v 1.67 2025/01/26 11:08:22 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Makefile
 *
 * This was based on a filter written by Pierre Dittgen (dittgen@univ-mlv.fr)
 * - T.Dickey
 */

#include <filters.h>

DefineFilter(make);

static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Preproc_attr;
static char *String_attr;

static int continued;

static void write_keyword(char *text, int size, int prepro, int caseless);
static void write_delimited(char *text, int size, int less, char *attr);
static void missing_quote(char *text, int length);

%}

SPACE		[ \t]

ID		[[:alpha:]_][[:alnum:]_]+
NAME		[[:alpha:]_][[:alnum:]_.-]+
TARGET		[[:alpha:]_][[:alnum:]_.%-]+

SUBST		"@"{NAME}"@"
MACRO		([$]?\$\{{ID}\}|[$]?\${ID}|[$]?\$[[:alpha:]@!%^&*?<]|\${SPACE}*\([^\)]+\))

OPER		[+:?]?=

%%

<NORMAL>^{SPACE}*[!]{NAME}{SPACE}*	{ write_keyword(yytext, yyleng, 1, 1); }

<NORMAL>^{SPACE}*{NAME}{SPACE}*	{ write_keyword(yytext, yyleng, 1, 0); }

<NORMAL>^{SPACE}*([-.])?include{SPACE}* { write_keyword(yytext, yyleng, 1, 0); }

<NORMAL>^{SPACE}*[\.]{TARGET}{SPACE}*:[:]? { write_keyword(yytext, yyleng, 0, 0); }

<NORMAL>^({SPACE}*{TARGET}{SPACE}*|\\\n)+: { write_delimited(yytext, yyleng, 1, Keyword_attr); }

<NORMAL>^{SPACE}*{NAME}{SPACE}*{OPER}	{
				  int oper = 1;
				  if (strchr("+:?", yytext[yyleng - 2]) != NULL)
				      ++oper;
				  write_delimited(yytext, yyleng, oper, Ident2_attr);
				}

<NORMAL>{SUBST}			{ WriteToken(Preproc_attr); }
<NORMAL>{MACRO}			{ WriteToken(Ident2_attr); }

<NORMAL>^{SPACE}\@		{ if (yyleng) {
				      flt_puts(yytext, yyleng-1, "");
				  }
				  flt_puts("@", 1, String_attr); }
<NORMAL>\\{SPACE}+\n		{ flt_error("trailing whitespace");
				  WriteToken(Error_attr); continued = 1; }
<NORMAL>\\[\n]			{ WriteToken(String_attr); continued = 1; }
<NORMAL>[\n]			{ ECHO; continued = 0; }
<NORMAL>\\["']			{ WriteToken(String_attr); }
<NORMAL>\'			{ BeginQuote(QUOTE1, String_attr); }
<NORMAL>\"			{ BeginQuote(QUOTE2, String_attr); }

<NORMAL>"#"			{ BeginQuote(COMMENT, Comment_attr); }
<COMMENT>(\\.|\\\n|[^\r\n])*	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>\n			{ FinishQuote(NORMAL); }

<QUOTE1,QUOTE2>{MACRO}		{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }
<QUOTE1,QUOTE2>\\{SPACE}+\n	{ flt_error("trailing whitespace");
				  flt_bfr_embed(yytext, yyleng, Error_attr); }
<QUOTE1,QUOTE2>\\\n		{ flt_bfr_append(yytext, yyleng); }

<QUOTE1>(\\.|[^\r\n\'])		{ flt_bfr_append(yytext, yyleng); }
<QUOTE1>\'			{ FinishQuote(NORMAL); }

<QUOTE2>(\\.|[^\r\n\"])		{ flt_bfr_append(yytext, yyleng); }
<QUOTE2>\"			{ FinishQuote(NORMAL); }

<QUOTE1,QUOTE2>[\n]		{ missing_quote(yytext, yyleng); }

%%

/*
 * If we're looking for tabs, only tabs (optionally followed by spaces) will
 * do.
 */
static int
want_tabs(char *text, int size)
{
    int result = 0;
    int spcs = 0;
    int tabs = 0;
    int n;

    if (size > 0) {
	for (n = 0; n < size; ++n) {
	    if (text[n] == ' ') {
		if (!tabs)
		    result = !continued;
		++spcs;
	    } else if (text[n] == '\t') {
		if (spcs)
		    result = !continued;
		++tabs;
	    }
	}
	/* a little paranoia does not hurt */
	if ((spcs == FltOptions('t')) && !tabs)
	    result = 1;
    } else {
	result = 1;		/* should not happen */
    }
    return result;
}

static void
write_keyword(char *text, int size, int prepro, int caseless)
{
    int colon = 0;
    int before = 0;
    int after = 0;
    int save;
    int nospcs = 0;
    int notabs = 0;
    int tabs = 0;
    char *base = text;
    const char *attr = "";

    while (size > 1 && text[size - 1] == ':') {
	text[--size] = 0;
	++colon;
    }

    while (isspace(CharOf(*text))) {
	if (*text == '\t')
	    tabs = 1;
	text++;
	size--;
	before++;
    }

    while (isspace(CharOf(text[size - 1]))) {
	size--;
	after++;
    }

    save = text[size];
    text[size] = 0;
    attr = (caseless
	    ? ci_keyword_attr(text)
	    : keyword_attr(text));
    /*
     * Some symbols cannot be preceded by tabs - we put those in the "premake"
     * table.
     */
    if ((attr == NULL) && !continued && prepro) {
	set_symbol_table("premake");
	attr = (caseless
		? ci_keyword_attr(text)
		: keyword_attr(text));
	set_symbol_table(flt_name());
	notabs = (attr != NULL);
	if (attr == NULL)
	    nospcs = want_tabs(base, before);
    } else {
	nospcs = want_tabs(base, before);
    }
    text[size] = (char) save;

    if (before) {
	if ((tabs && notabs) || nospcs) {
	    flt_error("unexpected whitespace");
	    flt_puts(base, before, Error_attr);
	} else {
	    flt_puts(base, before, "");
	}
    }

    flt_puts(text, size, attr);

    if (after)
	flt_puts(text + size, after, "");

    while (colon--)
	flt_putc(':');
}

/*
 * string passed to this routine is in the format:
 *
 *    [<white>]<token>[<white>]
 */
static char *
write_delimited_unit(char *text, char **attr, const char **gaps)
{
    char *pwhite, *ptoken, *panchor, *pnxt_unit;

    panchor = pwhite = text;
    while (isspace(CharOf(*pwhite))) {
	pwhite++;
    }
    if (pwhite > panchor)
	flt_puts(panchor, (int) (pwhite - panchor), *gaps);
    panchor = ptoken = pwhite;
    while (*ptoken && (!isspace(CharOf(*ptoken))))
	ptoken++;
    flt_puts(panchor, (int) (ptoken - panchor), *attr);
    if (isspace(CharOf(*ptoken))) {
	panchor = ptoken;
	pwhite = ptoken + 1;	/* advance beyond initial white space byte */
	while (isspace(CharOf(*pwhite)))
	    pwhite++;
	flt_puts(panchor, (int) (pwhite - panchor), *gaps);
	pnxt_unit = pwhite;
    } else
	pnxt_unit = ptoken;
    return (pnxt_unit);
}

/*
 * string passed to this routine is in the format:
 *
 *    ([<white>]<token>[<white>]|\\\n)+<single delimiter>
 */
static void
write_delimited(char *text, int size, int less, char *attr)
{
    char *cp;
    const char *gaps = "";
    int save = text[size - less];

    text[size - less] = '\0';
    cp = text;
    do {
	if (*cp == '\\' && cp[1] == '\n') {
	    flt_puts(cp, 2, gaps);
	    cp += 2;
	} else
	    cp = write_delimited_unit(cp, &attr, &gaps);
    } while (*cp);

    text[size - less] = (char) save;
    flt_puts(text + size - less, less, gaps);
}

static void
missing_quote(char *text, int length)
{
    char *save = String_attr;
    String_attr = Error_attr;
    flt_error("missing quote");
    flt_bfr_append(text, length);
    flt_bfr_finish();
    BEGIN(NORMAL);
    String_attr = save;
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

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident2_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    continued = 0;

    BEGIN(NORMAL);
    RunLEX();
    flt_bfr_error();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
