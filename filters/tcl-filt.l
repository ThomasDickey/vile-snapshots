%pointer
%s VERB NOTE REGOPT REGARG ARGS DATA DQUOTED

%a 5000

%{

/*
 * $Id: tcl-filt.l,v 1.65 2016/12/12 01:22:58 tom Exp $
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

#define FLTSTACK_EXTRA int square;
#include <fltstack.h>
#undef YY_NO_UNPUT

DefineFilter(tcl);

#define isIdent(ch) (isalnum(ch) || ch == '_')

static char *Action_attr;
static char *Braces_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Ident_attr;
static char *Keywrd2_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static void tcl_regexp(const char *text, int length);
static void unput_string(const char *text, int length);

%}

BLANK		[ \t]

ACTION		([\.`]|(\\\r?\n))

SWITCH		(--)|((-)[[:alpha:]_][[:alnum:]_-]*)
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

ESC_C		"\\"[bfnrtv\\]
ESC_8		"\\"{OCTAL}{1,3}
ESC_x		"\\x"[[:xdigit:]]{2,2}
ESC_u		"\\u"[[:xdigit:]]{4,4}
ESCAPE		({ESC_x}|{ESC_u}|{ESC_8}|{ESC_C})

%%

<VERB,ARGS,DATA>"{"{BLANK}*"}"	{ WriteToken(Braces_attr); }
<VERB,ARGS,DATA>"{"[\\]+."}"	|
<VERB,ARGS,DATA>"{&"[[:alpha:]]+;"}"	|
<VERB,ARGS,DATA>"{&#"[[:digit:]]+;"}"	|
<VERB,ARGS,DATA>"{"[^*{}[:space:][:digit:]]"}"	{
				  WriteToken3(Braces_attr,1);
				  flt_puts(yytext + 1, yyleng - 2, String_attr);
				  WriteToken2(Braces_attr, yyleng - 1);
				}
<VERB,ARGS,DATA>("{*}")?[{]	{ WriteToken(Braces_attr);
				  push_state(VERB);
				  FLTSTACK_THIS.square = 0;
				}
<VERB,ARGS,DATA>[}]		{
				  if (stk_level >= 0) {
				    int matched = (FLTSTACK_THIS.square == 0);
				    WriteToken(Braces_attr);
				    pop_state();
				    if (!matched) {
				      while (stk_level >= 0 && FLTSTACK_THIS.square) {
				        pop_state();
				      }
				    }
				  } else {
				    flt_error("unexpected right-brace");
				    WriteToken(Error_attr);
				  }
				}

<VERB>{IDENT}			{
				  const char *attr = get_keyword_attr(yytext);
				  WriteToken(attr);
				  if (!strcmp(yytext, "regsub")
				   || !strcmp(yytext, "regexp")) {
				    new_state(REGOPT);
				  } else {
				    new_state((attr && *attr) ? ARGS : DATA);
				  }
				}
<ARGS>{IDENT}			{ WriteToken(get_keyword_attr(yytext)); }
<DATA>{IDENT}			{ ECHO; }

<VERB>"#"			{ BeginQuote(NOTE, Comment_attr); }
<NOTE>[^\r\n\\]+		{ flt_bfr_append(yytext, yyleng); }
<NOTE>"\\"\r?\n			{ flt_bfr_append(yytext, yyleng); }
<NOTE>\r?\n			{ FinishQuote(VERB); }
<NOTE>.				{ flt_bfr_append(yytext, yyleng); }

<ARGS,DATA>"#"[[:xdigit:]]+	{ WriteToken(Number_attr); }
<ARGS,DATA>(";"|(\r?\n))	{ ECHO; new_state(VERB); }

<VERB,ARGS,DATA>{ACTION}	{ WriteToken(Action_attr); }

<VERB,ARGS,DATA>{NONUMBER}	{ flt_error("not a number"); WriteToken(Error_attr); }
<VERB,ARGS,DATA>{NUMBER}	{ WriteToken(Number_attr); }

<VERB,ARGS,DATA>{IDENT1}	|
<VERB,ARGS,DATA>{IDENT2}	|
<VERB,ARGS,DATA>{IDENTX}	|
<VERB,ARGS,DATA>{WIDGET}	{ WriteToken(Ident2_attr); }

<VERB,ARGS,DATA>\\\"	{ WriteToken(""); }

<VERB>{FORMAT}		{ WriteToken(String_attr); new_state(ARGS); }
<ARGS,DATA>{FORMAT}	{ WriteToken(String_attr); }
<VERB,ARGS,DATA>\\.	{ WriteToken(String_attr); }
<VERB,ARGS,DATA>["]	{ PushQuote(DQUOTED, String_attr); }
<DQUOTED>{ESCAPE}	{ flt_bfr_embed(yytext, yyleng, Preproc_attr); }
<DQUOTED>"\\"(\r?\n)	{ flt_bfr_embed(yytext, yyleng, Action_attr); }
<DQUOTED>"\\".		{ flt_bfr_embed(yytext, yyleng, Number_attr); }
<DQUOTED>(\r?\n)+	{ flt_bfr_append(yytext, yyleng); }
<DQUOTED>[^\\"\[]+	{ flt_bfr_append(yytext, yyleng); }
<DQUOTED>["]		{ PopQuote(); }
<DQUOTED>[\[]		{
			  flt_bfr_finish();
			  WriteToken(Braces_attr);
			  push_state(VERB);
			  FLTSTACK_THIS.square = 1;
			}
<VERB,ARGS,DATA,REGOPT,REGARG>[\[]	{
			  WriteToken(Braces_attr);
			  push_state(VERB);
			  FLTSTACK_THIS.square = 1;
			}
<VERB,ARGS,DATA,REGOPT,REGARG>[\]]	{
			  if (stk_level > 0) {
			    if (FLTSTACK_THIS.square) {
				WriteToken(Braces_attr);
				pop_state();
			    } else {
				WriteToken("");
			    }
			  } else {
			    flt_error("unexpected right-bracket");
			    WriteToken(Error_attr);
			  }
			  if (cur_state == DQUOTED) {
			    flt_bfr_begin(String_attr);
			  }
			}

<REGOPT>{SWITCH}	{ WriteToken(get_keyword_attr(yytext)); }
<REGOPT>[[:space:]]+	{ ECHO; }
<REGOPT>("\\".)+	|
<REGOPT>.		{ unput_string(yytext, yyleng); new_state(REGARG); }

<REGARG>"{"([^}]|\\.|"{"[^\\}]*"}")*"}"	|
<REGARG>[^[:space:]]+	{ tcl_regexp(yytext, yyleng); }

%%

#include <fltstack.h>

static void
unput_string(const char *text, int length)
{
    while (--length >= 0) {
	FLEX_PRINTF((stderr, "unput(%c)\n", text[length]));
	unput(text[length]);
    }
}

static void
tcl_regexp(const char *text, int length)
{
    if (*text == L_CURLY) {
	int spaces = 0;
	int escape = 0;
	int j, k;

	flt_bfr_finish();
	flt_bfr_begin(String_attr);
	flt_bfr_embed(text++, 1, Action_attr);
	length -= 2;
	for (j = 0; j < length; j = k) {
	    char *attr = String_attr;
	    int ch = CharOf(text[j]);
	    k = j + 1;
	    if (escape) {
		escape = 0;
		spaces = 0;
	    } else if (ch == '\\') {
		int known = 0;
		int valid = 1;
		escape = 1;
		spaces = 0;
		if (k < length) {
		    ch = CharOf(text[k]);
		    if (strchr("abBefnrtvdswDSWaAmMyYZbceimnprstw", ch)) {
			known = 1;
		    } else if (ch >= '0' && ch <= '7') {
			int n;
			for (n = 1; n <= 3; ++n) {
			    int kn = j + n;
			    if (kn >= length
				|| text[kn] < '0'
				|| text[kn] > '7') {
				break;
			    } else {
				known = n;
			    }
			}
		    } else {
			if (ch == 'x') {
			    known = 3;
			} else if (ch == 'u') {
			    known = 5;
			} else if (ch == 'U') {
			    known = 9;
			}
			if (known) {
			    /* validate the hex-digits */
			    int n;
			    int keep = 0;
			    for (n = 2; n <= known; ++n) {
				int kn = j + n;
				if (kn >= length
				    || !isxdigit(CharOf(text[kn]))) {
				    valid = 0;
				    break;
				} else {
				    keep = n;
				}
			    }
			    known = keep;
			}
		    }
		    /* if greater than zero, "known" is the number of escaped
		     * characters not counting "/".
		     */
		    if (known) {
			attr = valid ? Preproc_attr : Error_attr;
			k = j + known + 1;
		    } else {
			if (k < length)
			    ++k;
			attr = Number_attr;
		    }
		}
	    } else if (isspace(CharOf(ch))) {
		spaces = 1;
	    } else if (spaces && (ch == '#')) {
		attr = Comment_attr;
		for (k = j; k < length; ++k) {
		    if (text[k] == '\n') {
			break;
		    }
		}
	    }
	    if (attr == String_attr)
		flt_bfr_append(text + j, k - j);
	    else
		flt_bfr_embed(text + j, k - j, attr);
	}
	flt_bfr_embed(text + length, 1, Action_attr);
	flt_bfr_finish();
    } else if (strchr("^\\", *yytext)) {
	flt_puts(text, length, String_attr);
    } else {
	unput_string(text, length);
	new_state(ARGS);
    }
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
    Braces_attr = class_attr("Braces");
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident2_attr = class_attr(NAME_IDENT2);
    Ident_attr = class_attr(NAME_IDENT);
    Keywrd2_attr = class_attr(NAME_KEYWRD2);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    begin_state(VERB);
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
