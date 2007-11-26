%s CODE HERE SSTRING DSTRING RD SUBST

%o 6000

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/rb-filt.l,v 1.18 2007/11/25 20:04:46 tom Exp $
 *
 * Filter to add vile "attribution" sequences to a Ruby script
 * based on the Python filter.
 *
 * By Paul van Tilburg <paul@luon.net>
 *
 * TODO: %(q|Q|x) char .* char (string2)   to STRING
 *       ` .* `	               (command)   to STRING
 *       / .* /	               (regexp)    to STRING
 *       %r char .* char       (regexp)    to STRING
 *       #<GLOBAL|INSTANCE>    (varsubst)  to Identifier in DSTRING & HERE
 *
 * See also rubyfilt.c, which is the preferred version.
 */

#include <filters.h>
#include <fltstack.h>

DefineFilter("rb");

#define isIdent(ch) (isalnum(ch) || ch == '_')

static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static char *here_tag;
static char *here_match;
static unsigned here_len;
static int in_here;

static int save_here(char *text, int len);
static void pop_quote(char *text, int length);

%}

CHAR		\?(\\M-)?(\\C-)?.
VAR		[[:lower:]_][[:alnum:]_]*
CONST		[[:upper:]][[:alnum:]_]*
INSTANCE	@+({CONST}|{VAR})
GLOBAL		\$([-_.\/,"\\#%=~|\$?&`'+*\[\];!@<>():]|{CONST}|{VAR})

VARSUBST	#({INSTANCE}|{GLOBAL})
SUBSTL		#\{
SUBSTR		\}

COMMENT		#.*\n

BLANK		[ \t]*
SSTRING         \'(\\.|[^']|\n)*\'
DSTRING         \"(\\.|[^"]|\n)*\"
KEYWORD		[[:alpha:]_][[:alnum:]_]*
QIDENT		({SSTRING}|{DSTRING}|{KEYWORD}|"`"{KEYWORD}"`")

SIGN		[-+]
DECIMAL		[[:digit:]_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[[:xdigit:]_]+
REAL		[-+]?([[:digit:]_]*\.[[:digit:]][[:digit:]_]*)([eE][+-]?[[:digit:]_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL})[L]?|{REAL}

%%

<CODE>{CHAR}		|
<CODE>{NUMBER}		{ WriteToken(Number_attr); }
<CODE>{CONST}		{
			  char *attr = keyword_attr(yytext);
			  if (attr == 0)
			      attr = Ident2_attr;
			  WriteToken(attr);
			}
<CODE>{GLOBAL}		{ WriteToken(Ident2_attr); }
<CODE>{COMMENT}		{ WriteToken(Comment_attr); }
<CODE>{KEYWORD}		{ WriteToken(keyword_attr(yytext)); }
<CODE>\'		{ PushQuote(SSTRING, String_attr); }
<CODE>\"		{ PushQuote(DSTRING, String_attr); }
<CODE>^=begin		{
			    WriteToken(Comment_attr);
			    new_state(RD);
			}
<CODE>"<<"-?{BLANK}{QIDENT}	{
                            if (save_here(yytext, yyleng)) {
				PushQuote(HERE, String_attr);
			    } else {
				flt_error("not a here-document");
				WriteToken(Error_attr);
			    }
                        }

<HERE>^{BLANK}{QIDENT}	{
			    /* check for 'here'-document delimiter,
			       skip possible spaces first */
			    here_match = strstr(yytext, here_tag);
                            if (here_match != NULL &&
			        !strcmp(here_tag, here_match)) {
			        in_here = 0; /* ok, not in here-doc anymore */
                                pop_quote(yytext, yyleng);
                            } else {
				flt_bfr_append(yytext, yyleng);
			    }
                        }
<HERE>{SUBSTL}		{
			    if (in_here) {
				flt_bfr_finish();
				PushQuote(SUBST, Ident2_attr);
			    } else {
				flt_bfr_append(yytext, yyleng);
			    }
			}
<HERE>(\\.|.)           |
<HERE>\n                { flt_bfr_append(yytext, yyleng); }

<RD>^=end		{ WriteToken(Comment_attr); new_state(CODE); }
<RD>.*			{ WriteToken(Comment_attr); }

<SSTRING>\'		{ pop_quote(yytext, yyleng); }
<SSTRING>(\\.|.)	|
<SSTRING>[\n]		{ flt_bfr_append(yytext, yyleng); }

<DSTRING>\"		{ pop_quote(yytext, yyleng); }
<DSTRING>{VARSUBST}	{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }
<DSTRING>{SUBSTL}	{
			    flt_bfr_finish();
			    PushQuote(SUBST, Ident2_attr);
                        }
<DSTRING>(\\.|.)	|
<DSTRING>[\n]		{ flt_bfr_append(yytext, yyleng); }

<SUBST>{SUBSTR}		{ pop_quote(yytext, yyleng); }
<SUBST>(\\.|.)		|
<SUBST>[\n]		{ flt_bfr_append(yytext, yyleng); }

<CODE>\\.		|
<CODE>.			{ ECHO; }

%%

static int
save_here(char *text, int len)
{
    char *s;
    int result = 0;

    if ((here_tag = do_alloc(here_tag, len, &here_len)) != 0) {
	s = here_tag;
	text += 2;		/* skip over "<<" */
	if (*text == '-')
	    ++text;
	while (len-- > 0) {
	    if (isIdent(CharOf(*text))) {
		*s++ = *text;
	    } else if (*text == '\'' || *text == '\"') {
		;
	    } else {
		break;
	    }
	    text++;
	}
	*s = 0;
	result = (s != here_tag);
    }
    return result;
}

/*
 * Replaces PopQuote().
 */
static void
pop_quote(char *text, int length)
{
    char *attr = 0;

    flt_bfr_append(text, length);
    flt_bfr_finish();
    pop_state();
    if (FLTSTACK_OK) {
	switch (FLT_STATE) {
	case HERE:
	case SSTRING:
	case DSTRING:
	    attr = String_attr;
	    break;
	}
    }
    if (attr != 0)
	flt_bfr_begin(String_attr);
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
    Error_attr   = class_attr(NAME_ERROR);
    Ident_attr   = class_attr(NAME_IDENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    begin_state(CODE);
    while (yylex() > 0) {
    }
    flt_bfr_finish();
    end_state();
}
