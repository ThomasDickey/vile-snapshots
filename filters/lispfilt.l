%pointer
%x NORMAL QUOTED COMMENT

%{

/*
 * $Id: lispfilt.l,v 1.26 2012/02/18 12:59:14 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of lisp code.
 *
 * TODO:  implement readtable case directives :upcase, :downcase, :preserve
 * and :invert
 *
 * TODO:  implement other radix than 10 (that may require rewriting this in C)
 *
 * refs:
 *	http://www.schemers.org/Documents/Standards/R5RS/HTML/r5rs-Z-H-5.html
 *	http://www.lisp.org/HyperSpec/Body/sec_2-4-8.html
 */

#include <filters.h>

#define FLTSTACK_EXTRA int quoted;
#define FLTSTACK_EXTRA_PUSH FLTSTACK_THIS.quoted = 0;
#include <fltstack.h>

DefineFilter("lisp");

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Literal_attr;

static const char *paren_attr(void);

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
IDENT		[^[:space:]\"\,'`\(\)|#]+

LPAREN		\(
RPAREN		\)

%%

<NORMAL>#[Bb][01]+		{ WriteToken(Number_attr); }
<NORMAL>#[Oo][01234567]+	{ WriteToken(Number_attr); }
<NORMAL>#[Xx][[:xdigit:]]+	{ WriteToken(Number_attr); }

<NORMAL>#\\[[:alpha:]][[:alnum:]_]+	|
<NORMAL>#\\.  			{ WriteToken(Literal_attr); }

<NORMAL>#[^|BbOoXx\\]		{ WriteToken(""); /* later */ }

<NORMAL>#\|		{ BeginQuote(COMMENT, Comment_attr); }
<COMMENT>\|#		{ FinishQuote(NORMAL); }
<COMMENT>[\n]		{ flt_bfr_append(yytext, yyleng); }
<COMMENT>[^\r\n]	{ flt_bfr_append(yytext, yyleng); }

<NORMAL>{COMMENT}	{ WriteToken(Comment_attr); }

<NORMAL>['`]{LPAREN}	{ push_state(NORMAL);
			  if (FLTSTACK_OK)
			      FLTSTACK_THIS.quoted = (yytext[0] == '`' ? 2 : 1);
			  WriteToken(paren_attr());
			}

<NORMAL>['`]		{ WriteToken(Literal_attr); }

<NORMAL>","[@]?{LPAREN}	{ int save = FLTSTACK_OK ? FLTSTACK_THIS.quoted : 0;
			  push_state(NORMAL);
			  if (save != 2 && FLTSTACK_OK)
			      FLTSTACK_THIS.quoted = save;
			  WriteToken3((save ? Ident_attr : Error_attr),(yytext[1] == '@' ? 2 : 1));
			  WriteToken2(paren_attr(),(yytext[1] == '@' ? 2 : 1));
			}

<NORMAL>","[@]?		{
			  WriteToken((FLTSTACK_OK && FLTSTACK_THIS.quoted) ? Ident_attr : Error_attr);
			}

<NORMAL>{LPAREN}	{ int save = FLTSTACK_OK ? FLTSTACK_THIS.quoted : 0;
			  push_state(NORMAL);
			  if (FLTSTACK_OK)
			      FLTSTACK_THIS.quoted = save;
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
