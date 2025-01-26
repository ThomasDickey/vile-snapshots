%pointer
%x NORMAL QUOTED

%o 10000
%a 5000

%{

/*
 * $Id: conffilt.l,v 1.20 2025/01/26 11:08:08 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of config-file.
 */

#include <filters.h>
#include <fltstack.h>
#include <vl_ctype.h>

DefineFilter(conf);

static char *Action_attr;
static char *Comment_attr;
static char *Ident_attr;
static char *Number_attr;
static char *String_attr;

%}

SSTRING		\'([^'\r\n]|(\\[\r]?[\n]))*\'

NAME		[[:alnum:]_]

WILDCARD	(\?|\*)
WILDNAME	({NAME}|{WILDCARD})
FILENAME	(([./]{WILDNAME}+)|({WILDNAME}+[./]{WILDNAME}*)|({NAME}*{WILDCARD})|\.+\/+)+

INTEGER		[-+]?([[:digit:]]+)

IDENT		[[:alpha:]_]{NAME}*
IDENT0		[-]+[[:digit:]]*[[:alpha:]_-]+[[:alnum:]_-]*
IDENT1		\${NAME}+
IDENT2		\$\{[#]?{IDENT}\}
IDENTX		\$[\*@#\?\$!-]

CMT_MARK	[;#]
TXT_MARK	[!@$%^&+*_=:"'<>,./?|\\]

BLANK		[ \t]

%%

<NORMAL>{IDENT0}	|
<NORMAL>{FILENAME}	{ ECHO; /* exclude from other classes */ }

<NORMAL>^{BLANK}*{TXT_MARK}{IDENT} {
			  const char *attr;
			  int skip = 0;

			  while (isblank(CharOf(yytext[skip]))) {
			      ++skip;
			  }
			  attr = get_keyword_attr(yytext + skip);
			  if (isEmpty(attr)) {
			      attr = get_keyword_attr(yytext + skip + 1);
			      if (!isEmpty(attr)) {
				  ++skip;
			      }
			  }
			  if (skip && !isEmpty(attr)) {
			      WriteToken3("", skip);
			      WriteToken2(attr, skip);
			  } else {
			      WriteToken(attr);
			  }
			}
<NORMAL>{IDENT}		{ WriteToken(get_keyword_attr(yytext)); }

<NORMAL>{CMT_MARK}[^\r\n]* { WriteToken(Comment_attr); }

<NORMAL>{INTEGER}	{ WriteToken(Number_attr); }

<NORMAL>{IDENT1}	|
<NORMAL>{IDENT2}	|
<NORMAL>{IDENTX}	{ WriteToken(Ident_attr); }

<NORMAL>\\[n]		{ WriteToken3(Action_attr, 1); WriteToken2("", 1); }
<NORMAL>\\.		|
<NORMAL>{SSTRING}	{ WriteToken(String_attr); }
<NORMAL>\"		{ PushQuote(QUOTED, String_attr); }

<NORMAL>.		{ ECHO; }

<QUOTED>\\[\r\n]	|
<QUOTED>\\.		{ flt_bfr_embed(yytext, 1, Action_attr); flt_bfr_append(yytext + 1, yyleng - 1); }
<QUOTED>{IDENT1}	|
<QUOTED>{IDENT2}	{ flt_bfr_embed(yytext, yyleng, Ident_attr); }
<QUOTED>[^`\r\n\\\"]+	{ flt_bfr_append(yytext, yyleng); }
<QUOTED>[\r\n]		|
<QUOTED>\"		{ PopQuote(); }
<QUOTED>.		{ flt_bfr_append(yytext, yyleng); }

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
    Comment_attr = class_attr(NAME_COMMENT);
    Ident_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

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
