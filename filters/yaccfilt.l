%s RULES ACTIONS CODE COMMENT

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/yaccfilt.l,v 1.21 2003/05/24 00:49:25 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of YACC program.
 */

#include <filters.h>

DefineFilter("yacc");

typedef enum {
    RuleSection = 0
    ,ActionSection
    ,CodeSection
} SECTIONS;

static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static char *save_attr = "";

static SECTIONS section = RuleSection;
static int brace_level = 0;

#include <fltstack.h>

static SECTIONS NextSection(SECTIONS);
static void set_state(void);

%}

SPACE		[ \t]

DIRECTIVE	^%[^ \t\n]+

IDENT		[a-zA-Z_][a-zA-Z_0-9]*
QIDENT		\$([0-9$]|\<{IDENT}\>)

INTEGER		[-+]?([0-9]+|("0"[xX][0-9a-fA-Z]+))

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\])*\"
STRINGS		({SSTRING}|{DSTRING})

PATTERN		(\\.|\[(\\.|.)*\]|[^ \t\n])+

%%

<INITIAL,RULES,ACTIONS,CODE>{DIRECTIVE}	{
		    save_attr = "";
		    WriteToken(keyword_attr(yytext));
		    switch(yytext[1]) {
		    case '%':
		    	section = NextSection(section);
			set_state();
			break;
		    case '{':
		    	push_state(CODE);
			break;
		    case '}':
		    	pop_state();
			break;
		    default:
			if (!strcmp(yytext+1, "token")) {
			    save_attr = Ident2_attr;
			} else if (!strcmp(yytext+1, "type")) {
			    save_attr = Ident_attr;
			}
			break;
		    }
		}

<RULES>{IDENT}		{ WriteToken(save_attr); insert_keyword(yytext, save_attr, 0); }
<RULES>\<{IDENT}\>	{ WriteToken(Ident2_attr); }
<RULES>\{		{ ECHO; push_state(CODE); }
<RULES>;		{ WriteToken(String_attr); }

<ACTIONS>{IDENT}	{ WriteToken(keyword_attr(yytext)); }
<ACTIONS>\{		{ ECHO; brace_level = 1; push_state(CODE); }
<ACTIONS>;		{ WriteToken(String_attr); }

<CODE>{IDENT}		{ WriteToken(keyword_attr(yytext)); }
<CODE>{INTEGER}		{ WriteToken(Number_attr); }
<CODE>\{		{ ECHO; brace_level++; }
<CODE>\}		{ ECHO;
			  if (--brace_level <= 0) {
			      brace_level = 0;
			      if (section == ActionSection)
				  pop_state();
			  }
		        }

"/*"			{ PushQuote(COMMENT, Comment_attr); }
<COMMENT>[^*]*		{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"*"+[^*/]*	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"*"+"/"	{ PopQuote(); }

^{SPACE}*#{SPACE}*{IDENT}({SPACE}+(\<[^>]+\>|\"[^"]+\"))? { WriteToken(Preproc_attr); }
{QIDENT}		{ WriteToken(Ident2_attr); }
{STRINGS}		{ WriteToken(String_attr); }

%%

static SECTIONS
NextSection(SECTIONS next)
{
    switch (next) {
    case RuleSection:
	next = ActionSection;
	break;
    case ActionSection:
	next = CodeSection;
	break;
    case CodeSection:
	break;
    }
    return next;
}

static void
set_state(void)
{
    if (section >= CodeSection) {
	new_state(CODE);
    } else if (section >= ActionSection) {
	new_state(ACTIONS);
    } else {
	new_state(RULES);
    }
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs)
{
    yyin = inputs;
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    Ident_attr   = class_attr(NAME_IDENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr  = class_attr(NAME_LITERAL);

    section = RuleSection;
    brace_level = 0;
    save_attr = Error_attr;

    begin_state(RULES);
    while (yylex() > 0) {
    }
    end_state();
}
