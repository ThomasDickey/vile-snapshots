%x NORMAL COMMENT QUOTE1 QUOTE2 CCOMMENT

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/imakeflt.l,v 1.5 2000/11/04 20:22:39 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Imakefile
 *
 * This is based on makefile.l
 */

#include <filters.h>

DefineFilter("imake");

static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Prepro_attr;
static char *String_attr;

static char *
skip_name(char *text)
{
    while (isalnum(CharOf(*text)) || *text == '_')
	text++;
    return text;
}

static int
preprocessor(char *text, int size)
{
    char *s = skip_blanks(text);
    char *t;
    char *attr = 0;
    int save;

    if (set_symbol_table("cpre")) {
	s = skip_blanks(s + 1);	/* points to name */
	t = skip_name(s);
	save = *t;
	*t = 0;
	attr = keyword_attr(s);
	*t = save;
	set_symbol_table(filter_def.filter_name);
    }
    if (attr != 0) {
	flt_puts(text, size, Prepro_attr);
	return 1;
    }
    return 0;
}

static void
write_keyword(char *text, int size)
{
    int colon = size > 1 && text[size-1] == ':';
    int space = 0;
    char *base = text;

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

    flt_puts(text, size, keyword_attr(text));

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
    if ((ptoken - panchor) == 5
     && !strncmp(panchor, "XCOMM", 5)) {
	*attr = Comment_attr;
	*gaps = Comment_attr;
    }
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
    Prepro_attr  = class_attr(NAME_PREPROC);
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

%%

<NORMAL>^{SPACE}*([^#])?include{SPACE}*	{ write_keyword(yytext, yyleng); }	

<NORMAL>^{SPACE}*[\.!]{NAME}{SPACE}*:	{ write_keyword(yytext, yyleng); }

<NORMAL>^({SPACE}*{NAME}{SPACE}*|\\\n)+: { write_delimited(yytext, yyleng, Keyword_attr); }

<NORMAL>^{SPACE}*{NAME}{SPACE}*=	{ write_delimited(yytext, yyleng, Ident2_attr); }

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

<NORMAL>^{SPACE}*"XCOMM"$	{ WriteToken(Comment_attr); }
<NORMAL>^{SPACE}*"XCOMM"[^A-Za-z0-9_] { BeginQuote(COMMENT, Comment_attr); }

<NORMAL>\\[/]			{ WriteToken(String_attr); }
<NORMAL>"/*"			{ BeginQuote(CCOMMENT, Comment_attr); }
<CCOMMENT>("*"[^/]|\n|[^*\n])*	{ flt_bfr_append(yytext, yyleng); }
<CCOMMENT>"*/"			{ FinishQuote(NORMAL); }

<NORMAL>^{SPACE}*#{SPACE}*{NAME} { if (!preprocessor(yytext, yyleng)) {
				    BeginQuote(COMMENT, Comment_attr);
				} }
