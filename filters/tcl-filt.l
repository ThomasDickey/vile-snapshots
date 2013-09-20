%pointer
%s VERB ARGS DATA DQUOTED

%a 5000

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/tcl-filt.l,v 1.45 2013/09/18 22:27:43 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of TCL/TK script.
 *
 * http://tmml.sourceforge.net/doc/tcl/Tcl.html
 *
 * note -
 * Solaris 2.7's lex:
 * 251/1000 nodes(%e), 757/2500 positions(%p), 85/500 (%n), 5443 transitions,
 * 301/10000 packed char classes(%k),  2015/2500 packed transitions(%a),  2605/3000 output slots(%o)
 */

#include <filters.h>
#include <fltstack.h>

DefineFilter("tcl");

#define isIdent(ch) (isalnum(ch) || ch == '_')

static char *Action_attr;
static char *Braces_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Ident_attr;
static char *Keywrd2_attr;
static char *Number_attr;
static char *String_attr;

%}

SPACE		[ \t]

ACTION		([\.`]|\\\n)

BASICIDENT      (--)|((-)?[[:alpha:]_][[:alnum:]_-]*)
IDENT           (::)?{BASICIDENT}(::{BASICIDENT})*
IDENT1		\${IDENT}
IDENT2		\$\{[^\}]*\}
IDENTX		\$[\*@#\?\$!-]
WIDGET		(\.{IDENT})+

SIGN		[-+]
DECIMAL		[[:digit:]]+
OCTAL		0[0-7]+
HEXADECIMAL	0x[[:xdigit:]]+
EXP		[eE]{SIGN}?{DECIMAL}
REAL		(({DECIMAL}\.{DECIMAL}?)|({DECIMAL}?\.{DECIMAL})){EXP}?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

FORMAT		"%"[-+#%[:alnum:]]+

NODECIMAL	[[:digit:]]+[g-zG-Z]+
NOOCTAL      	0[0-7]*([89]+[0-7]*)+
NOHEXADECIMAL	0X[[:xdigit:]]+
NOREAL		\.?{EXP}
NONUMBER	{SIGN}?({NODECIMAL}|{NOOCTAL}|{NOHEXADECIMAL}|{NOREAL})

%%

<VERB,ARGS,DATA>"{"{SPACE}*"}"	{ WriteToken(Braces_attr); }
<VERB,ARGS,DATA>("{*}")?[{]	{ WriteToken(Braces_attr); push_state(VERB); }
<VERB,ARGS,DATA>[}]		{ WriteToken(Braces_attr); pop_state(); }

<VERB>{IDENT}			{
				  const char *attr = get_keyword_attr(yytext);
				  WriteToken(attr);
				  new_state((attr && *attr) ? ARGS : DATA);
				}
<ARGS>{IDENT}			{ WriteToken(get_keyword_attr(yytext)); }
<DATA>{IDENT}			{ ECHO; }

<VERB>"#"([^\r\n]|\\\n)*$	{ WriteToken(Comment_attr); new_state(ARGS); }
<ARGS,DATA>"#"[[:xdigit:]]+	{ WriteToken(Number_attr); }
<ARGS,DATA>[;\n]		{ ECHO; new_state(VERB); }

<VERB,ARGS,DATA>{ACTION}	{ WriteToken(Action_attr); }

<VERB,ARGS,DATA>{NONUMBER}	{ flt_error("not a number"); WriteToken(Error_attr); }
<VERB,ARGS,DATA>{NUMBER}	{ WriteToken(Number_attr); }

<VERB,ARGS,DATA>{IDENT1}	|
<VERB,ARGS,DATA>{IDENT2}	|
<VERB,ARGS,DATA>{IDENTX}	|
<VERB,ARGS,DATA>{WIDGET}	{ WriteToken(Ident2_attr); }

<VERB,ARGS,DATA>\{\"\}	|
<VERB,ARGS,DATA>\\\"	{ WriteToken(""); }

<VERB>{FORMAT}		{ WriteToken(String_attr); new_state(ARGS); }
<ARGS,DATA>{FORMAT}	{ WriteToken(String_attr); }
<VERB,ARGS,DATA>\\.	{ WriteToken(String_attr); }
<VERB,ARGS,DATA>["]	{ PushQuote(DQUOTED, String_attr); }
<DQUOTED>(\\.|[^\\"\[]|(\\)?[\r\n])+	{ flt_bfr_append(yytext, yyleng); }
<DQUOTED>["]		{ PopQuote(); }
<DQUOTED>[\[]		{
			  flt_bfr_finish();
			  WriteToken(Braces_attr);
			  push_state(VERB);
			}
<VERB,ARGS,DATA>[\[]	{
			  WriteToken(Braces_attr);
			  push_state(VERB);
			}
<VERB,ARGS,DATA>[\]]	{
			  if (stk_level > 0) {
			    WriteToken(Braces_attr);
			    pop_state();
			  } else {
			    flt_error("unexpected right-bracket");
			    WriteToken(Error_attr);
			  }
			  if (cur_state == DQUOTED) {
			    flt_bfr_begin(String_attr);
			  }
			}

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
    Braces_attr = class_attr("Braces");
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident2_attr = class_attr(NAME_IDENT2);
    Ident_attr = class_attr(NAME_IDENT);
    Keywrd2_attr = class_attr(NAME_KEYWRD2);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    begin_state(VERB);
    RunLEX();
    end_state();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
