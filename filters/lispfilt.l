%x NORMAL QUOTED COMMENT

%{
/*
 * $Id: lispfilt.l,v 1.8 2003/09/17 22:11:30 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of lisp code.
 *
 * TODO:  implement readtable case directives :upcase, :downcase, :preserve
 * and :invert
 *
 * TODO:  implement other radix than 10 (that may require rewriting this in C)
 *
 * TODO:  rewrite this into C anyway, since flex refuses to match "#|" unless
 * I exclude "|" from {IDENT}.
 */

#include <filters.h>

#define FLTSTACK_EXTRA int sibling; int quoted;
#include <fltstack.h>

DefineFilter("lisp");

static char *Keyword_attr;
static char *Comment_attr;
static char *Ident_attr;
static char *String_attr;
static char *Number_attr;

static char *paren_attr(void);

%}

SIGN		[-+]
POINT		[.]
DIGIT		([0-9])
DECIMAL		[0-9]
SLASH		[/]

INTEGER		({SIGN}?{DECIMAL}+{POINT}?)|({SIGN}?{DIGIT}+)

RATIO		({DIGIT}+){SLASH}({DIGIT}+)

EXPONENT	([DEFLSdefls]{SIGN}?{DIGIT}+)
FLOAT		({SIGN}?{DECIMAL}*{POINT}{DECIMAL}+{EXPONENT}?)|({SIGN}?{DECIMAL}+({POINT}{DECIMAL}*){EXPONENT})

NUMBER		{INTEGER}|{RATIO}|{FLOAT}

COMMENT		;[^\n]*
IDENT		[^[:space:]\"\(\)|]+

LPAREN		\(
RPAREN		\)

%%

<NORMAL>#\|		{ BeginQuote(COMMENT, Comment_attr); }
<COMMENT>\|#		{ FinishQuote(NORMAL); }
<COMMENT>[\n]		{ flt_bfr_append(yytext, yyleng); }
<COMMENT>[^\n]		{ flt_bfr_append(yytext, yyleng); }

<NORMAL>{COMMENT}	{ WriteToken(Comment_attr); }

<NORMAL>'{LPAREN}	{ push_state(NORMAL);
			  FLTSTACK_THIS.quoted = 1;
			  WriteToken(paren_attr());
			}

<NORMAL>{LPAREN}	{ int save = FLTSTACK_THIS.quoted;
			  push_state(NORMAL);
			  FLTSTACK_THIS.quoted = save;
			  WriteToken(paren_attr());
			}

<NORMAL>{RPAREN}	{ WriteToken(paren_attr()); pop_state(); }

<NORMAL>{NUMBER}	{ WriteToken(Number_attr); }

<NORMAL>{IDENT}		{ WriteToken(ci_keyword_attr(yytext)); }

<NORMAL>\"		{ BeginQuote(QUOTED, String_attr); }
<QUOTED>\\.		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>[^\\\"]+	{ flt_bfr_append(yytext, yyleng); }
<QUOTED>\"		{ FinishQuote(NORMAL); }

%%

static char *
paren_attr(void)
{
    return (FLTSTACK_THIS.quoted ? String_attr : "");
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Keyword_attr = class_attr(NAME_KEYWORD);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr   = class_attr(NAME_IDENT2);
    String_attr  = class_attr(NAME_LITERAL);
    Number_attr  = class_attr(NAME_NUMBER);

    begin_state(NORMAL);
    while (yylex() > 0) {
    }
    flt_bfr_error();
}
