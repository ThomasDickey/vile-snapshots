%pointer
%x NORMAL QUOTED COMMENT

%{

/*
 * $Id: scm-filt.l,v 1.9 2012/02/18 13:16:47 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of scheme code.
 *
 * TODO:  implement other radix than 10 (that may require rewriting this in C)
 *
 * refs:
 *	http://www.schemers.org/Documents/Standards/R5RS/HTML/r5rs-Z-H-5.html
 */

#include <filters.h>

#define FLTSTACK_EXTRA int quoted;
#define FLTSTACK_EXTRA_PUSH FLTSTACK_THIS.quoted = 0;
#include <fltstack.h>

DefineFilter("scheme");

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Literal_attr;

static const char *paren_attr(void);
static int count_spaces(const char *text, int len);

/* This is an ugly state variable which specifies whether the next element,  *
 * regardless of state, is subject to a modifier which overrides the quoting *
 * level of the parent state. It exists because some odd permutations of     *
 * special characters would require a quadratic number of patterns to        *
 * properly highlight them without strange artifacts.                        *
 * If there is an override next_quoted is > 0, with the quoting style being  *
 * specified as next_quoted-1. This can be accessed and set with the macros  *
 * `S_QUOTED(new_style)' and `style = G_QUOTED().'                           */
static int next_quoted = 0;

#define S_QUOTED(i) (next_quoted = (i)+1)
#define G_QUOTED()  (next_quoted - 1)

%}

SIGN		[-+]
POINT		[.]
DIGIT		([[:digit:]])
DECIMAL		[[:digit:]]
SLASH		[/]

INTEGER		({SIGN}?{DECIMAL}+{POINT}?)|({SIGN}?{DIGIT}+)

RATIO		({DIGIT}+){SLASH}({DIGIT}+)

EXPONENT	([DEFLSdefls]{SIGN}?{DIGIT}+)
FLOAT		({SIGN}?{DECIMAL}*{POINT}{DECIMAL}+{EXPONENT}?)|({SIGN}?{DECIMAL}+({POINT}{DECIMAL}*){EXPONENT})

NUMBER		{INTEGER}|{RATIO}|{FLOAT}

COMMENT		;[^\r\n]*
SPACING		[[:space:]\"\,'`\(\)\[\]|#]
IDENT		[^[:space:]\"\,'`\(\)\[\]|#]+

LPAREN		[\(\[]
RPAREN		[\)\]]

%%

<NORMAL>#[Bb][01]+		{ WriteToken(Number_attr); }
<NORMAL>#[Oo][01234567]+	{ WriteToken(Number_attr); }
<NORMAL>#[Xx][[:xdigit:]]+	{ WriteToken(Number_attr); }

<NORMAL>#\\[[:alpha:]][[:alnum:]_]+	|
<NORMAL>#\\.  			{ WriteToken(Literal_attr); }

<NORMAL>#{LPAREN}	{ int save = FLTSTACK_OK ? FLTSTACK_THIS.quoted : 0;
			  push_state(NORMAL);
			  if (FLTSTACK_OK)
			      FLTSTACK_THIS.quoted = save;
			  WriteToken(save ? paren_attr() : Error_attr);
			}

<NORMAL>#[^|BbOoXx\\]		{ WriteToken(""); /* later */ }

<NORMAL>#\|		{ BeginQuote(COMMENT, Comment_attr); }
<COMMENT>\|#		{ FinishQuote(NORMAL); }
<COMMENT>[\n]		{ flt_bfr_append(yytext, yyleng); }
<COMMENT>[^\r\n]	{ flt_bfr_append(yytext, yyleng); }

<NORMAL>{COMMENT}	{ WriteToken(Comment_attr); }

<NORMAL>['`][[:space:]]*#?{LPAREN}	{
			  yyless(yyleng-1);
			  if (FLTSTACK_OK && FLTSTACK_THIS.quoted == 0)
			    S_QUOTED(yytext[0] == '`' ? 2 : 1);
			  WriteToken3(Literal_attr,1);
			  WriteToken2("",1);
			}

<NORMAL>{LPAREN}[[:space:]]*(quasi)?quote{SPACING}	{
			  int save = FLTSTACK_OK ? FLTSTACK_THIS.quoted : 0;
			  int skip = count_spaces(yytext+1, yyleng-1);
			  yyless(yyleng-1);
			  push_state(NORMAL);
			  if (FLTSTACK_OK)
			      FLTSTACK_THIS.quoted = save ? save : (yytext[yyleng-6] == 'i' ? 2 : 1);
			  WriteToken3(paren_attr(),1);
			  flt_puts(yytext+1, skip, "");
			  flt_puts(yytext+1+skip, yyleng-1-skip, get_keyword_attr(yytext+1+skip));
			}

<NORMAL>['`]\,@?	{
			  yyless(1);
			  if (FLTSTACK_OK && FLTSTACK_THIS.quoted == 0)
			    S_QUOTED((yytext[0] == '`') ? 2 : 1);
			  WriteToken(Literal_attr);
			}

<NORMAL>['`]		{ WriteToken(Literal_attr); }

<NORMAL>\,@?[[:space:]]*{LPAREN}	{
			  int skip = (yytext[1] == '@' ? 2 : 1);
			  int save = (next_quoted ? G_QUOTED() :
			             (FLTSTACK_OK ? FLTSTACK_THIS.quoted : 0));
			  yyless(yyleng-1);
			  WriteToken3((save ? Ident_attr : Error_attr),skip);
			  WriteToken2("",skip);
			  if (save == 2)
			    S_QUOTED(0);
			}

<NORMAL>{LPAREN}[[:space:]]*unquote(-splicing)?{SPACING}	{
			  int save = (next_quoted ? G_QUOTED() :
			             (FLTSTACK_OK ? FLTSTACK_THIS.quoted : 0));
			  int skip = count_spaces(yytext+1, yyleng-1);
			  yyless(yyleng-1);
			  push_state(NORMAL);
			  if (save != 2 && FLTSTACK_OK)
			    FLTSTACK_THIS.quoted = save;
			  WriteToken3(paren_attr(),1);
			  flt_puts(yytext+1, skip, "");
			  flt_puts(yytext+1+skip, yyleng-1-skip,
				   save == 0
				   ? Error_attr
				   : get_keyword_attr(yytext+1+skip));
			  next_quoted = 0;
			}

<NORMAL>\,@?		{
			  WriteToken(((next_quoted && G_QUOTED()) ||
			             (FLTSTACK_OK && FLTSTACK_THIS.quoted)) ?
			             Ident_attr : Error_attr);
			  next_quoted = 0;
			}

<NORMAL>{LPAREN}	{ int save = FLTSTACK_OK ? FLTSTACK_THIS.quoted : 0;
			  push_state(NORMAL);
			  if (FLTSTACK_OK)
			      FLTSTACK_THIS.quoted = next_quoted ? G_QUOTED() : save;
			  next_quoted = 0;
			  WriteToken(paren_attr());
			}

<NORMAL>{RPAREN}	{ WriteToken(paren_attr()); pop_state(); }

<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }

<NORMAL>{IDENT}		{ WriteToken(get_keyword_attr(yytext)); }

<NORMAL>\"		{ BeginQuote(QUOTED, Literal_attr); }
<QUOTED>\\.		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>[^\\\"]+	{ flt_bfr_append(yytext, yyleng); }
<QUOTED>\"		{ FinishQuote(NORMAL); }

%%

#include <fltstack.h>

static const char *
paren_attr(void)
{
    return ((FLTSTACK_OK && FLTSTACK_THIS.quoted) ? Literal_attr : Action_attr);
}

static int
count_spaces(const char *text, int len)
{
    int i;

    for (i = 0; (i < len) && isspace(CharOf(text[i])); i++) {
	;
    }
    return i;
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
    Ident_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    Literal_attr = class_attr(NAME_LITERAL);

    begin_state(NORMAL);
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
