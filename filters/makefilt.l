%x NORMAL COMMENT QUOTE1 QUOTE2

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/makefilt.l,v 1.43 2002/07/03 10:27:42 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Makefile
 *
 * This was based on a filter written by Pierre Dittgen (dittgen@univ-mlv.fr)
 * - T.Dickey
 */

#include <filters.h>

DefineFilter("make");

static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Preproc_attr;
static char *Keyword_attr;
static char *String_attr;

static void
write_keyword(char *text, int size, int prepro, int caseless)
{
    int colon = size > 1 && text[size-1] == ':';
    int space = 0;
    int save;
    char *base = text;
    char *attr = "";

    if (colon)
	text[--size] = 0;

    while (isspace(CharOf(*text))) {
	text++;
	size--;
	space++;
    }
    if (space)
	flt_puts(base, space, "");

    space = 0;
    while (isspace(CharOf(text[size-1]))) {
	size--;
	space++;
    }

    save = text[size];
    text[size] = 0;
    attr = caseless
	 ? ci_keyword_attr(text)
    	 : keyword_attr(text);
    if ((attr == 0) && prepro) {
	set_symbol_table("premake");
	attr = caseless
	     ? ci_keyword_attr(text)
	     : keyword_attr(text);
	flt_puts(text, size, attr);
	set_symbol_table(flt_name());
    } else {
	flt_puts(text, size, attr);
    }
    text[size] = save;

    if (space)
	flt_puts(text + size, space, "");

    if (colon)
	flt_putc(':');
}

/*
 * string passed to this routine is in the format:
 *
 *    [<white>]<token>[<white>]
 */
static char *
write_delimited_unit(char *text, char **attr, char **gaps)
{
    char *pwhite, *ptoken, *panchor, *pnxt_unit;

    panchor = pwhite = text;
    while (isspace(CharOf(*pwhite))) {
        pwhite++;
    }
    if (pwhite > panchor)
        flt_puts(panchor, pwhite - panchor, *gaps);
    panchor = ptoken = pwhite;
    while (*ptoken && (! isspace(CharOf(*ptoken))))
        ptoken++;
    flt_puts(panchor, ptoken - panchor, *attr);
    if (isspace(CharOf(*ptoken)))
    {
        panchor = ptoken;
        pwhite  = ptoken + 1;  /* advance beyond initial white space byte */
        while (isspace(CharOf(*pwhite)))
            pwhite++;
        flt_puts(panchor, pwhite - panchor, *gaps);
        pnxt_unit = pwhite;
    }
    else
        pnxt_unit = ptoken;
    return (pnxt_unit);
}

/*
 * string passed to this routine is in the format:
 *
 *    ([<white>]<token>[<white>]|\\\n)+<single delimiter>
 */
static void
write_delimited(char *text, int size, char *attr)
{
    char *cp, delim[2];
    char *gaps = "";

    delim[0] = text[size - 1];         /* save the trailing delimiter */
    delim[1] = text[size - 1] = '\0';  /* chop the trailing delimiter */
    cp       = text;
    do
    {
        if (*cp == '\\' && cp[1] == '\n')
        {
            flt_puts(cp, 2, gaps);
            cp += 2;
        }
        else
            cp = write_delimited_unit(cp, &attr, &gaps);
    }
    while (*cp);

    flt_puts(delim, 1, gaps);
}

static void
missing_quote(char *text, int length)
{
    char *save = String_attr;
    String_attr = Error_attr;
    flt_bfr_append(text, length);
    flt_bfr_finish();
    BEGIN(NORMAL);
    String_attr = save;
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
    yyin = input;

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    while (yylex() > 0) {
    }
    flt_bfr_error();
}

%}

SPACE		[ \t]

ID		[a-zA-Z_][a-zA-Z_0-9]+
NAME		[a-zA-Z_][a-zA-Z_0-9.-]+

MACRO		([$]?\$\{{ID}\}|[$]?\${ID}|[$]?\$[A-Za-z@!%^&*?<]|\${SPACE}*\([^\)]+\))

OPER		[+:?]?=

%%

<NORMAL>^[ ]*[!]{NAME}{SPACE}*	{ write_keyword(yytext, yyleng, 1, 1); }

<NORMAL>^[ ]*{NAME}{SPACE}*	{ write_keyword(yytext, yyleng, 1, 1); }

<NORMAL>^{SPACE}*([-.])?include{SPACE}* { write_keyword(yytext, yyleng, 1, 0); }

<NORMAL>^{SPACE}*[\.]{NAME}{SPACE}*:	{ write_keyword(yytext, yyleng, 0, 0); }

<NORMAL>^({SPACE}*{NAME}{SPACE}*|\\\n)+: { write_delimited(yytext, yyleng, Keyword_attr); }

<NORMAL>^{SPACE}*{NAME}{SPACE}*{OPER}	{ write_delimited(yytext, yyleng, Ident2_attr); }

<NORMAL>{MACRO}			{ WriteToken(Ident2_attr); }

<NORMAL>^{SPACE}\@		{ if (yyleng)
				    flt_puts(yytext, yyleng-1, "");
				  flt_puts("@", 1, String_attr); }
<NORMAL>\\{SPACE}+\n		{ WriteToken(Error_attr); }
<NORMAL>\\["'\n]		{ WriteToken(String_attr); }
<NORMAL>\'			{ BeginQuote(QUOTE1, String_attr); }
<NORMAL>\"			{ BeginQuote(QUOTE2, String_attr); }

<NORMAL>"#"			{ BeginQuote(COMMENT, Comment_attr); }
<COMMENT>(\\.|\\\n|[^\n])*	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>\n			{ FinishQuote(NORMAL); }

<QUOTE1,QUOTE2>{MACRO}		{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }
<QUOTE1,QUOTE2>\\{SPACE}+\n	{ flt_bfr_embed(yytext, yyleng, Error_attr); }
<QUOTE1,QUOTE2>\\\n		{ flt_bfr_append(yytext, yyleng); }

<QUOTE1>(\\.|[^\n\'])		{ flt_bfr_append(yytext, yyleng); }
<QUOTE1>\'			{ FinishQuote(NORMAL); }

<QUOTE2>(\\.|[^\n\"])		{ flt_bfr_append(yytext, yyleng); }
<QUOTE2>\"			{ FinishQuote(NORMAL); }

<QUOTE1,QUOTE2>[\n]		{ missing_quote(yytext, yyleng); }
