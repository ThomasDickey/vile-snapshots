%s LEAD BODY MORE COMMENT0 COMMENT INLINE_CMT SERVER ONE_LINE

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sql-filt.l,v 1.25 2005/03/12 16:02:05 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of SQL script
 * - T.Dickey
 */

#include <filters.h>
#undef YY_NO_UNPUT

DefineOptFilter("sql", "d");

#define DPRINTF(params) if(FltOptions('d'))printf params
#define set_state(name) DPRINTF(("{" #name ":%d}", name)); BEGIN(name)
#define new_state(state) saved_state = YYSTATE; set_state(state)
#define old_state()      set_state(saved_state);


static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;

static int next_line;		/* state after one-line SQL*Plus */
static int saved_state = LEAD;

/******************************************************************************/

%}

SPACE		[ \t]+

ACTION		[@!$]

UNITS		[kKmM]
INTEGER		[0-9]+
SIGN		[+-]
REAL		{INTEGER}?(\.{INTEGER}([eE]{SIGN}?{INTEGER})?)
NUMBER		{SIGN}?({INTEGER}|{REAL})({UNITS})?

SSTRING		\'([^\']|\'\')*\'
DSTRING		\"([^"])*\"
STRING		({SSTRING}|{DSTRING})

IDENT		[%]?[a-zA-Z][a-zA-Z0-9_$#]*
LABEL		"<<"{IDENT}">>"
HOSTVAR		":"{IDENT}

%%

<LEAD>{IDENT}		{   char *text = lowercase_of(yytext);
			    char *attr = ci_keyword_attr(text);
			    DPRINTF(("{%s->%s}", text, NONNULL(attr)));
			    if (attr == Comment_attr) {
				WriteToken(attr);
				new_state(INLINE_CMT);
			    } else if (attr == 0) {
				new_state(BODY);
				if (set_symbol_table("sqlplus")) {
				    if ((attr = ci_keyword_attr(text)) != 0) {
					new_state(ONE_LINE);
					next_line = LEAD;
					/* FIXME:  a few commands have keywords,
					 * such as "set".
					 */
				    }
				    WriteToken(attr);
				    set_symbol_table(filter_def.filter_name);
				}
			    } else {
				WriteToken(attr);
				new_state(BODY);
			    }
			}

<MORE,BODY>{IDENT}	{   char *text = lowercase_of(yytext);
			    char *attr = ci_keyword_attr(text);
			    DPRINTF(("{%s->%s}", text, NONNULL(attr)));
			    if (attr == Comment_attr) {
				new_state(INLINE_CMT);
			    } else {
				WriteToken(attr);
				new_state(BODY);
			    }
			}

<MORE,BODY>{HOSTVAR}	{ flt_puts(yytext, 1, Action_attr);
			  WriteToken2(Ident_attr,1);
			  new_state(BODY);
			}

<SERVER>{IDENT}		{ WriteToken(String_attr); }
<SERVER>"."		{ ECHO; }
<SERVER>";"		{ unput(yytext[0]); old_state(); }
<SERVER>[\n]		|
<SERVER>.		{ unput(yytext[0]); old_state(); }

<MORE,BODY>{NUMBER}	{ WriteToken(Number_attr); new_state(BODY); }

<MORE,BODY>{STRING}	{ WriteToken(String_attr); new_state(BODY); }

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

<LEAD,MORE,BODY>--[^\n]* { WriteToken(Comment_attr); new_state(INLINE_CMT); }
<INLINE_CMT>[^\n]*	{ WriteToken(Comment_attr); }
<INLINE_CMT>\n		{ ECHO; old_state(); }

<LEAD,MORE,BODY>"/*"	{ WriteToken(Comment_attr);
			  new_state(COMMENT0);
			}

<COMMENT0>"*/"		{ 
			  WriteToken(Comment_attr);
			  old_state();
			}
<COMMENT0>[^\n]		{ WriteToken(Comment_attr); }
<COMMENT0>\n		{
			  ECHO;
			  set_state(COMMENT);
			}

<COMMENT>^[^\n]*"*/"	{ WriteToken(Comment_attr);
			  old_state();
			}
<COMMENT>^[^\n]*	{ WriteToken(Comment_attr); }
<COMMENT>\n		{ ECHO; }

<LEAD,MORE>{LABEL}	{ WriteToken(Action_attr); }

<LEAD,MORE>{ACTION}[^=][^\n]* {
			    WriteToken(Action_attr);
			    new_state(ONE_LINE);
			    next_line = LEAD;
			}
<BODY>{ACTION}		{
			    WriteToken(Action_attr);
			    new_state(SERVER);
			}
<ONE_LINE>[^\n]*	{ WriteToken(String_attr);
			  if (yyleng != 0 && yytext[yyleng - 1] == '-')
			  	next_line = ONE_LINE;
			    else
			  	next_line = LEAD;
			}
<ONE_LINE>\n		{ ECHO;
			  new_state(next_line);
			  next_line = LEAD;
			}

<LEAD,MORE,BODY>.	{ ECHO; }

%%

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    Ident_attr   = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    set_state(LEAD);
    while (yylex() > 0) {
    }
}
