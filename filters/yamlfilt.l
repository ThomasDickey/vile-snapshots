%pointer
%s NORMAL WantKEY HaveKEY WantVAL HaveVAL SLEAD STEXT
%p 6000
%a 6000
%e 2000
%n 1000
%o 6000
%k 2000

%{

/*
 * $Id: yamlfilt.l,v 1.49 2025/01/26 11:40:03 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of YAML program.
 *
 * Refer to
 * http://yaml.org/spec/spec.html
 * Also -
 * http://yaml.org/type/float.html
 * http://yaml.org/type/merge.html
 * http://yaml.org/type/timestamp.html
 *
 * TODO comments should be preceded by whitespace.
 * TODO uri fragment may have "#", distinct from comments that need whitespace first
 * TODO handle push/pop with indents for "-" sequences
 * TODO marker for scalar blocks should be the only text in value
 * TODO paint leading indent of scalar block using Leading_attr; 6.3 says additional whitespace in flow may include tabs
 * TODO check validity of escaped "\" characters per yaml 5.7
 * TODO use flt-buffer for STEXT
 * TODO comma is special only in {} or []
 */

#define FLTSTACK_EXTRA	int marker; int indent;
#include <filters.h>
#undef YY_NO_UNPUT

DefineFilter(yaml);

#define NAME_YAML_ANCHORS "YamlAnchors"

#define Leading_attr ""		/* "RC5" */
#define Block_attr   ""		/* "BRC1" */

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static char *Anchor_attr;

#include <fltstack.h>

static void found_key(int);
static void new_level(int);
static void reset_level(void);
static int skip_leading(void);

%}

INT_BASE_2	[-+]?0b[0-1_]+
INT_BASE_8	[-+]?0[0-7_]+
INT_BASE_10	[-+]?(0|[1-9][0-9_]*)
INT_BASE_16	[-+]?0x[0-9a-fA-F_]+
INT_BASE_60	[-+]?[1-9][0-9_]*(:[0-5]?[0-9])+

INTEGER		{INT_BASE_2}|{INT_BASE_8}|{INT_BASE_10}|{INT_BASE_16}|{INT_BASE_60}

FLT_BASE_10	[-+]?([0-9][0-9_]*)?\.[0-9_]*([eE][-+][0-9]+)?
FLT_BASE_60	[-+]?[0-9][0-9_]*(:[0-5]?[0-9])+\.[0-9_]*
FLT_INFINITE	[-+]?\.(inf|Inf|INF)
FLT_NONNUMBER	\.(nan|NaN|NAN)

FLOAT		{FLT_BASE_10}|{FLT_BASE_60}|{FLT_INFINITE}|{FLT_NONNUMBER}

TS__YMD		[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]
TS__YYYY	[0-9][0-9][0-9][0-9]
TS__MON		[0-9][0-9]?
TS__DAY		[0-9][0-9]?
TS__HOUR	([Tt]|[ \t]+)[0-9][0-9]?
TS__MIN		:[0-9][0-9]
TS__SEC		:[0-9][0-9]
TS__FRAC	(\.[0-9]*)?
TS__TZ		(([ \t]*)Z|[-+][0-9][0-9]?(:[0-9][0-9])?)?
TIMESTAMP	{TS__YMD}|{TS__YYYY}-{TS__MON}-{TS__DAY}{TS__HOUR}{TS__MIN}{TS__SEC}{TS__FRAC}{TS__TZ}

URI_CHAR	[[:alnum:]-]|("%"[[:xdigit:]][[:xdigit:]])|([;/?:@&=+$,_.!~*'()\[\]])
URI_WEB		[[:alnum:]]+"://"{URI_CHAR}+
URI_MAIL	[[:alnum:]-]+"@"[[:alnum:].-]+
URI		{URI_WEB}|{URI_MAIL}

XTYPE		"!!"[[:alnum:]_]+

SSTRING		'[^'\r\n\t]+'
DSTRING		\"(\\.|[^"\\\r\n]|\\\n)*\"

SPACE		[ \t]
ITEM		([:?-]{SPACE})

KEY_END		[ \t\r\n]
DATA_END	[ \t\r\n,{}\[\]]

BLOCK		[>|]([+-])?

TAGNAME		[[:alnum:]_]+
KEYNAME		({TAGNAME}+(([ \t-]|"::")+{TAGNAME}+)*|"<<"){SPACE}*
ANY_KEY		({INTEGER}|{FLOAT}|{DSTRING}|{KEYNAME})

%%

<NORMAL,HaveKEY>^{SPACE}*{ITEM}? {
			  new_level(yyleng);
			}

<NORMAL,WantKEY,HaveKEY>{ANY_KEY}":"{KEY_END} {
			  int len = yyleng - 1;
			  found_key(flt_get_col());
			  WriteToken3(Ident2_attr, len);
			  unput(yytext[len]);
			  new_state(HaveKEY);
			}

<NORMAL,WantKEY,HaveKEY,WantVAL,HaveVAL>"#"[^\r\n]* {
			  WriteToken(Comment_attr);
			}

<NORMAL,WantKEY,HaveKEY,WantVAL>"---" |
<NORMAL,WantKEY,HaveKEY,WantVAL>"..." {
			  WriteToken(Action_attr);
			}

<NORMAL,WantKEY,HaveKEY,WantVAL,HaveVAL>"," {
			  WriteToken(Action_attr);
			  new_state(NORMAL);
			}

<NORMAL,WantKEY,HaveKEY,WantVAL>"{" {
			  if (1) {
			    WriteToken(Action_attr);
			    push_state(NORMAL);
			    FLTSTACK_THIS.marker = R_CURLY;
			  } else {
			    ECHO;
			  }
			}
<NORMAL,WantKEY,HaveKEY,WantVAL,HaveVAL>"}" {
			  if (FLTSTACK_THIS.marker == R_CURLY) {
			    WriteToken(Action_attr);
			    pop_state();
			  } else {
			    ECHO;
			  }
			}
<NORMAL,WantKEY,HaveKEY,WantVAL>"[" {
			  WriteToken(Action_attr);
			  push_state(NORMAL);
			  FLTSTACK_THIS.marker = R_BLOCK;
			}
<NORMAL,WantKEY,HaveKEY,WantVAL>"]" {
			  if (FLTSTACK_THIS.marker == R_BLOCK) {
			    WriteToken(Action_attr);
			    pop_state();
			  } else {
			    ECHO;
			  }
			}

<HaveKEY,WantVAL>{XTYPE}	{ WriteToken(keyword_attr(yytext)); }
<HaveKEY,WantVAL>{SPACE}	{ ECHO; }

<WantKEY,HaveKEY,WantVAL>{URI}{DATA_END}	|
<WantKEY,HaveKEY,WantVAL>{TIMESTAMP}{DATA_END}	|
<WantKEY,HaveKEY,WantVAL>{INTEGER}{DATA_END}	|
<WantKEY,HaveKEY,WantVAL>{FLOAT}{DATA_END} {
			    int len = yyleng - 1;
			    WriteToken3(Number_attr, len);
			    unput(yytext[len]);
			  }

<WantKEY,HaveKEY,WantVAL>"&"{TAGNAME} {
			  const char *attr = Ident_attr;
			  char *name = yytext + 1;
			  WriteToken3(Action_attr,1);
			  if (set_symbol_table(NAME_YAML_ANCHORS)) {
			      attr = class_attr(name);
			      if (attr == NULL) {
				  insert_keyword(name, Anchor_attr, 0);
				  attr = get_keyword_attr(name);
			      } else {
				  attr = Error_attr;
				  flt_error("Keyword \"%s\" is already an anchor", name);
			      }
			      set_symbol_table(default_table);
			  }
			  WriteToken2(attr, 1);
			}
<WantKEY,HaveKEY,WantVAL>"*"{TAGNAME} {
			  const char *attr = Error_attr;
			  char *name = yytext + 1;
			  WriteToken3(Action_attr, 1);
			  if (set_symbol_table(NAME_YAML_ANCHORS)) {
			      attr = get_keyword_attr(name);
			      if (attr == NULL) {
				  attr = Error_attr;
				  flt_error("Keyword \"%s\" is not an anchor", name);
			      }
			      set_symbol_table(default_table);
			  }
			  WriteToken2(attr, 1);
			}

<HaveKEY,WantVAL>"!"{TAGNAME} {
			  WriteToken(keyword_attr(yytext));
			  /* TODO: "!binary", etc */
			}

<HaveKEY,WantVAL>{TAGNAME} {
			  WriteToken(keyword_attr(yytext));
			  new_state(HaveVAL);
			}

<WantKEY,HaveKEY,WantVAL,HaveVAL>{SSTRING} |
<WantKEY,HaveKEY,WantVAL,HaveVAL>{DSTRING} { WriteToken(String_attr); }

<WantKEY,HaveKEY,WantVAL>{BLOCK}[\r\n] {
			  int len = yyleng - 1;
			  WriteToken3(Block_attr, len);
			  unput(yytext[len]);
			  new_state(STEXT);
			}
<SLEAD>^{SPACE}*[\r\n]+ { ECHO; }
<SLEAD>^{SPACE}*"#"[^\r\n]* {
			  int first = skip_leading();
			  if (first <= 0 || first < FLTSTACK_THIS.indent) {
			    WriteToken(Comment_attr);
			    new_state(NORMAL);
			    reset_level();
			  } else {
			    WriteToken(String_attr);
			  }
			}
<SLEAD>^{SPACE}*{ITEM}? {
			  if (yyleng > FLTSTACK_THIS.indent) {
			    WriteToken(String_attr);
			    new_state(STEXT);
			  } else {
			    new_level(yyleng);
			  }
			}
<SLEAD>^{SPACE}*[^-]{ANY_KEY}":"{KEY_END} {
			  int first = skip_leading();
			  if (first <= 0 || first < FLTSTACK_THIS.indent) {
			    int len = yyleng - 1;
			    found_key(flt_get_col() + first);
			    WriteToken3(Ident2_attr, len);
			    unput(yytext[len]);
			    new_state(HaveKEY);
			  } else {
			    WriteToken(String_attr);
			    new_state(STEXT);
			  }
			}
<STEXT>[\r\n]+		{ ECHO; new_state(SLEAD); /* ignore blank lines */ }
<STEXT>[^\r\n]*		{ WriteToken(String_attr); }

<NORMAL>[\r\n]+ 	{ ECHO; new_state(NORMAL); }
<WantKEY>[\r\n]+ 	{ ECHO; new_state(NORMAL); reset_level(); }
<HaveKEY>[\r\n]+ 	{ ECHO; }
<WantVAL>[\r\n]+ 	{ ECHO; new_state(NORMAL); }
<HaveVAL>[\r\n]+ 	{ ECHO; new_state(NORMAL); }

<NORMAL>{SPACE}		{ ECHO; }
<WantKEY>{SPACE}	{ ECHO; }
<HaveVAL>{SPACE}	{ ECHO; }

<NORMAL>.		{ ECHO; }
<WantKEY>.		{ ECHO; new_state(HaveVAL); }
<HaveKEY>.		{ ECHO; new_state(HaveVAL); }
<WantVAL>.		{ ECHO; new_state(HaveVAL); }
<HaveVAL>.		{ ECHO; }

%%

#include <fltstack.h>

static void
new_level(int indent)
{
    int n;
    int err = -1;
    int item = 0;
    const char *attr = Leading_attr;
    flt_bfr_begin(attr);
    for (n = 0; n < indent; ++n) {
	switch (yytext[n]) {
	case ' ':
	    flt_bfr_append(yytext + n, 1);
	    break;
	case '\t':
	    err = n;
	    flt_bfr_embed(yytext + n, 1, Error_attr);
	    break;
	default:
	    item = 1;
	    flt_bfr_embed(yytext + n, 1, Action_attr);
	    break;
	}
    }
    if (++err) {
	flt_error("tabs are not allowed in leading indent");
    }
    flt_bfr_finish();
    if (indent > FLTSTACK_THIS.indent) {
	int state = (item
		     ? WantKEY
		     : (FLTSTACK_THIS.state == HaveKEY
			? HaveKEY
			: NORMAL));
	push_state(state);
	FLTSTACK_THIS.indent = indent;
	FLTSTACK_THIS.marker = 0;
    } else if (indent < FLTSTACK_THIS.indent) {
	pop_state();
	if (item)
	    new_state(WantKEY);
	if (FLTSTACK_THIS.indent != indent)
	    flt_error("expected indent %d, have %d",
		      FLTSTACK_THIS.indent, indent);
    } else {
	new_state(WantKEY);
    }
}

static void
reset_level(void)
{
    stk_level = 0;
    FLTSTACK_THIS.indent = 0;
}

static int
skip_leading(void)
{
    int n;
    int first = -1;
    for (n = 0; n < yyleng; ++n) {
	if (!isspace(CharOf(yytext[n]))) {
	    first = n;
	    break;
	}
    }
    return first;
}

static void
found_key(int indent)
{
    if (indent <= 0) {
	reset_level();
    } else {
	FLTSTACK_THIS.indent = indent;
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
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    flt_make_symtab(NAME_YAML_ANCHORS);
    set_symbol_table(NAME_YAML_ANCHORS);
    if ((Anchor_attr = class_attr(NAME_YAML_ANCHORS)) == NULL)
	Anchor_attr = Keyword_attr;

    set_symbol_table(default_table);

    FLEX_PRINTF((stderr, "state NORMAL  %d\n", NORMAL));
    FLEX_PRINTF((stderr, "state WantKEY %d\n", WantKEY));
    FLEX_PRINTF((stderr, "state HaveKEY %d\n", HaveKEY));
    FLEX_PRINTF((stderr, "state WantVAL %d\n", WantVAL));
    FLEX_PRINTF((stderr, "state HaveVAL %d\n", HaveVAL));
    FLEX_PRINTF((stderr, "state SLEAD   %d\n", SLEAD));
    FLEX_PRINTF((stderr, "state STEXT   %d\n", STEXT));

    begin_state(NORMAL);
    FLTSTACK_THIS.marker = EOS;
    FLTSTACK_THIS.indent = 0;

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
