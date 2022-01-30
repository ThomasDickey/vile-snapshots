%pointer
%s sTEXT QUOTED QUOTES HTML

%a 10000
%o 10000

%{

/*
 * $Id: md-filt.l,v 1.8 2022/01/25 23:31:31 tom Exp $
 *
 * Filter to add vile "attribution" sequences to a markdown file.
 *
 * https://daringfireball.net/projects/markdown/
 * https://www.markdownguide.org/
 *
 * TODO rework {INDENT}* of {ITEM} into nested lists and validate the blanks
 */

#include <filters.h>
#include <fltstack.h>

DefineFilter(md);

static char *Action_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static int nbl;	/* count characters on non-blank line */
static int pbl;	/* previous line's count for "nbl" */

%}

DASHES		[*_-][*_\t -]+[*_-]
DASHED		[=]+

INDENT		("    "|\t)
ITEM		([*+-]|[[:digit:]]+\.)[ \t]+

SIGN		[-+]
DECIMAL		[[:digit:]_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[[:xdigit:]_]+
REAL		([[:digit:]_]*\.[[:digit:]][[:digit:]_]*)([eE]{SIGN}?[[:digit:]_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

NAME		[[:alnum:]_.#-]+
ADDRESS		({NAME}|\.)+
EMAIL		{NAME}@{ADDRESS}

URL		[[:alpha:]]+"://"[[:alnum:]#%&+\,.\/?;@_=~-]+

%%

<sTEXT,HTML>^[\r\n]	{ pbl = nbl; nbl = 0;
			  if (cur_state == HTML) {
			    PopQuote();
			  } else {
			    ECHO;
			  }
			  new_state(sTEXT);
			}

<sTEXT>^\<.*\>		{ PushQuote(HTML, Preproc_attr); }
<HTML>[\r\n]		|
<HTML>.			{ flt_bfr_append(yytext, yyleng); }

<sTEXT>^{DASHED}+	{ WriteToken(Action_attr); }
<sTEXT>^{DASHES}$	{ WriteToken(Action_attr); }

<sTEXT>^{ITEM}		{ WriteToken(Action_attr); }

<sTEXT>{NUMBER}		{ WriteToken(Number_attr); }
<sTEXT>{EMAIL}		{ WriteToken(Ident2_attr); }
<sTEXT>{URL}		{ WriteToken(Ident2_attr); }

<QUOTES,QUOTED>{EMAIL}	{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }
<QUOTES,QUOTED>{URL}	{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }

<sTEXT>\(\"\)		|
<sTEXT>\'.\'		{ ECHO; }

<sTEXT>\"		{ PushQuote(QUOTED, String_attr); }
<QUOTED>\"		{ PopQuote(); }
<QUOTED>.		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>^[\n]		{ PopQuote(); }
<QUOTED>[\n]		{ flt_bfr_append(yytext, yyleng); }

<sTEXT>\`\`		{ PushQuote(QUOTES, String_attr); }
<QUOTES>\'\'		{ PopQuote(); }
<QUOTES>.		{ flt_bfr_append(yytext, yyleng); }
<QUOTES>^[\n]		{ PopQuote(); }
<QUOTES>[\n]		{ flt_bfr_append(yytext, yyleng); }

<sTEXT>[\r\n]		{ ECHO; }
<sTEXT>.		{ ECHO; }

%%

#include <fltstack.h>

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
    Ident2_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    nbl = pbl = 0;
    begin_state(sTEXT);
    RunLEX();
    flt_bfr_finish();
    end_state();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
