%pointer
%s LEAD BODY MORE COMMENT INLINE_CMT SERVER ONE_LINE

%{

/*
 * $Id: sql-filt.l,v 1.52 2025/01/26 11:38:02 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of SQL script
 * - T.Dickey
 */

#include <filters.h>
#undef YY_NO_UNPUT

DefineOptFilter(sql, "d");

#define DPRINTF(params) if(FltOptions('d'))printf params
#define set_state(name) DPRINTF(("{@%d:" #name ":%d}", __LINE__, name)); BEGIN(name); began_state = name
#define new_state(state) saved_state = began_state; set_state(state)
#define old_state()      set_state(saved_state);

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static int next_line;		/* state after one-line SQL*Plus */
static int began_state;
static int saved_state;

static void begin_oneliner(void);
static void finish_oneliner(void);
static int is_oneliner(const char *name);
static void init_filter(int before GCC_UNUSED);
static void do_filter(FILE *inputs);

#if NO_LEAKS
static void free_filter(void);
#endif

/******************************************************************************/

%}

SPACE		[ \t]+

SHELL		[$!]
ACTION		[@][@]?

UNITS		[kKmM]
HEX		0[xX][[:xdigit:]]+
INTEGER		[[:digit:]]+
SIGN		[+-]
REAL		{INTEGER}?(\.{INTEGER}([eE]{SIGN}?{INTEGER})?)
NUMBER		{SIGN}?({INTEGER}|{REAL})({UNITS})?

SSTRING		\'([^\']|\'\')*\'
DSTRING		\"([^"])*\"
STRING		({SSTRING}|{DSTRING})

IDENT		[%]?[[:alpha:]][[:alnum:]_$#]*
LABEL		"<<"{IDENT}">>"
HOSTVAR		":"{IDENT}

%%

<LEAD>{IDENT}		{   const char *text = lowercase_of(yytext);
			    const char *attr = get_keyword_attr(text);
			    DPRINTF(("{%s->%s}", text, NONNULL(attr)));
			    if (attr == Comment_attr) {
				WriteToken(attr);
				new_state(INLINE_CMT);
			    } else if (attr == NULL) {
				new_state(BODY);
				if (set_symbol_table("sqlplus")) {
				    attr = get_keyword_attr(text);
				    WriteToken(attr);
				    if (attr != NULL
					&& is_oneliner(text)) {
					begin_oneliner();
					/* FIXME:  a few commands have keywords,
					 * such as "set".
					 */
				    }
				    set_symbol_table(default_table);
				}
			    } else {
				WriteToken(attr);
				new_state(BODY);
			    }
			}

<MORE,BODY>{IDENT}	{   const char *text = lowercase_of(yytext);
			    const char *attr = get_keyword_attr(text);
			    DPRINTF(("{%s->%s}", text, NONNULL(attr)));
			    if (attr == Comment_attr) {
				new_state(INLINE_CMT);
			    } else {
				WriteToken(attr);
				if (!strcmp(yytext, "then")
				 || !strcmp(yytext, "else")) {
				    new_state(LEAD);
				} else {
				    new_state(BODY);
				}
			    }
			}

<LEAD,MORE,BODY>{HOSTVAR}	{ flt_puts(yytext, 1, Action_attr);
			  WriteToken2(Ident_attr,1);
			  new_state(BODY);
			}

<SERVER>{IDENT}		{ WriteToken(String_attr); }
<SERVER>"."		{ ECHO; }
<SERVER>";"		{ unput(yytext[0]); old_state(); }
<SERVER>[\n]		|
<SERVER>.		{ unput(yytext[0]); old_state(); }

<LEAD,MORE,BODY>{HEX}	|
<LEAD,MORE,BODY>{NUMBER}	{ WriteToken(Number_attr); new_state(BODY); }

<LEAD,MORE,BODY>{STRING}	{ WriteToken(String_attr); new_state(BODY); }

<SERVER,MORE,BODY>"&"	{ ECHO; }
<MORE,BODY>"<>"		{ ECHO; }
<MORE,BODY>"^="		{ ECHO; }
<MORE,BODY>"~="		{ ECHO; }
<MORE,BODY>"!="		{ ECHO; }
<MORE,BODY>"=>"		{ ECHO; }
<MORE,BODY>">="		{ ECHO; }
<MORE,BODY>"<="		{ ECHO; }
<MORE,BODY>":="		{ ECHO; }
<MORE,BODY>"||"		{ ECHO; }
<MORE,BODY>"**"		{ ECHO; }
<MORE,BODY>"<"		{ ECHO; }
<MORE,BODY>">"		{ ECHO; }
<MORE,BODY>"="		{ ECHO; }
<MORE,BODY>";"		{ ECHO; new_state(LEAD); }
<MORE,BODY>","		{ ECHO; }
<MORE,BODY>"("		{ ECHO; }
<MORE,BODY>")"		{ ECHO; }
<MORE,BODY>"-"		{ ECHO; }
<MORE,BODY>"+"		{ ECHO; }
<MORE,BODY>"*"		{ ECHO; }
<BODY>"/"		{ ECHO; }
<MORE,BODY>"."		{ ECHO; }

<LEAD,MORE>"/"		{ ECHO; new_state(LEAD); }

<LEAD,MORE,BODY>\r	{ ECHO; }
<LEAD>\n		{ ECHO; }
<MORE,BODY>\n		{ ECHO; new_state(MORE); }
<LEAD,MORE,BODY>^{SPACE} { ECHO;
			}
<LEAD,MORE,BODY>{SPACE}	{ ECHO; }

<LEAD,MORE,BODY>--[^\r\n]* { WriteToken(Comment_attr); new_state(INLINE_CMT); }
<INLINE_CMT>[^\r\n]*	{ WriteToken(Comment_attr); }
<INLINE_CMT>\n		{ ECHO; old_state(); }

<LEAD,MORE,BODY>"/*"	{ new_state(COMMENT);
			  flt_bfr_begin(Comment_attr);
			  flt_bfr_append(yytext, yyleng);
			}
<COMMENT>[*]+"/"	{ flt_bfr_append(yytext, yyleng);
			  flt_bfr_finish();
			  old_state();
			}
<COMMENT>[*]$		|
<COMMENT>[*][^/]	|
<COMMENT>[^\r\n*]*	|
<COMMENT>[\r\n]		{ flt_bfr_append(yytext, yyleng); }

<LEAD,MORE>{LABEL}	{ WriteToken(Action_attr); }

<LEAD,MORE>{SHELL}[^=][^\r\n]* {
			    WriteToken(Action_attr);
			    begin_oneliner();
			}
<LEAD,MORE>{ACTION}[^=][^[:space:]]* {
			    WriteToken(Action_attr);
			    begin_oneliner();
			}
<BODY>{SHELL}		|
<BODY>{ACTION}		{
			    WriteToken(Action_attr);
			    new_state(SERVER);
			}
<ONE_LINE>[^\r\n]*	{ flt_bfr_append(yytext, yyleng);
			  if (yyleng != 0 && yytext[yyleng - 1] == '-')
			  	next_line = ONE_LINE;
			    else
			  	next_line = LEAD;
			}
<ONE_LINE>[\r\n]	{
			  if (next_line == LEAD) {
			      finish_oneliner();
			      ECHO;
			  } else {
			      flt_bfr_append(yytext, yyleng);
			  }
			}

<LEAD,MORE,BODY>.	{ ECHO; }

%%

static void
begin_oneliner(void)
{
    new_state(ONE_LINE);
    next_line = LEAD;
    flt_bfr_begin(String_attr);
}

static void
finish_oneliner(void)
{
    flt_bfr_finish();
    new_state(next_line);
    next_line = LEAD;
}

static int
is_oneliner(const char *name)
{
    const char *flags = ci_keyword_flag(name);
    int result = 0;
    if (flags != NULL) {
	result = (strchr(flags, '1') != NULL);
    }
    return result;
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
    String_attr = class_attr(NAME_LITERAL);

    began_state = LEAD;
    saved_state = LEAD;
    set_state(LEAD);
    RunLEX();
    finish_oneliner();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
