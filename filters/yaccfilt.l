%pointer
%s RULES ACTIONS CODE COMMENT

%{

/*
 * $Id: yaccfilt.l,v 1.45 2016/12/11 16:04:15 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of YACC program.
 */

#include <filters.h>

DefineFilter(yacc);

typedef enum {
    RuleSection = 0
    ,ActionSection
    ,CodeSection
} SECTIONS;

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;
static char *Terminal_attr;

static const char *save_attr = "";

static SECTIONS section = RuleSection;
static int brace_level = 0;
static int nested_section = 0;

#include <fltstack.h>

static SECTIONS NextSection(SECTIONS);
static const char *merged_attr(char *);
static int DeclareTerminal(const char *);
static void DefineTerminal(char *);
static void PopCode(void);
static void PushCode(void);
static void set_state(void);

%}

SPACE		[ \t]

DIRECTIVE	^%[^[:space:]]+

IDENT		[[:alpha:]_][[:alnum:]_]*
QIDENT		\$([[:digit:]$]|\<{IDENT}\>)

INTEGER		[-+]?([[:digit:]]+|("0"[xX][[:xdigit:]]+))

SSTRING		\'(\\.|[^'\\]|\\[\r\n])*\'
DSTRING		\"(\\.|[^"\\]|\\[\r\n])*\"
STRINGS		({SSTRING}|{DSTRING})

PATTERN		(\\.|\[(\\.|.)*\]|[^[:space:]])+

%%

<RULES,ACTIONS,CODE>{DIRECTIVE}	{
		    save_attr = "";
		    WriteToken(merged_attr(yytext));
		    switch(yytext[1]) {
		    case '%':
		    	section = NextSection(section);
			set_state();
			break;
		    case '{':
			++nested_section;
		    	PushCode();
			break;
		    case '}':
			--nested_section;
		    	PopCode();
			break;
		    default:
			if (DeclareTerminal(yytext+1)) {
			    save_attr = Terminal_attr;
			}
			break;
		    }
		}

<RULES>{IDENT}		{ WriteToken(save_attr);
			  if (*save_attr)
			      DefineTerminal(yytext);
			}
<RULES>"<"{IDENT}">"	{ WriteToken(Action_attr); }
<RULES>\{		{ ECHO; PushCode(); }
<RULES>;		{ WriteToken(String_attr); }

<ACTIONS>{IDENT}	{ DefineTerminal(yytext);
			  WriteToken(Terminal_attr);
			}
<ACTIONS>\{		{ ECHO; brace_level = 1; PushCode(); }
<ACTIONS>;		{ WriteToken(String_attr); }

<CODE>{IDENT}		{ WriteToken(merged_attr(yytext)); }
<RULES,CODE>{INTEGER}	{ WriteToken(Number_attr); }
<CODE>"//".*$		{ WriteToken(Comment_attr); }
<CODE>\{		{ ECHO; brace_level++; }
<CODE>\}		{ ECHO;
			  if (--brace_level <= 0) {
			      brace_level = 0;
			      if (section <= ActionSection && !nested_section) {
				  PopCode();
			      }
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

#include <fltstack.h>

static void
DefineTerminal(char *text)
{
    if (!is_class(text))
	insert_keyword(text, Terminal_attr, 0);
}

static const char *
merged_attr(char *text)
{
    const char *attr = get_keyword_attr(text);
    if ((cur_state == CODE) && isEmpty(attr)) {
	set_symbol_table(default_table);
	attr = get_keyword_attr(text);
	set_symbol_table("c");
    }
    return attr;
}

static void
PushCode(void)
{
    push_state(CODE);
    set_symbol_table("c");
}

static void
PopCode(void)
{
    pop_state();
    set_symbol_table(default_table);
}

static int
DeclareTerminal(const char *text)
{
    static const char *table[] =
    {
	"left",
	"nonassoc",
	"right",
	"start",
	"token",
	"term",
	"type",
    };
    unsigned n;
    int result = 0;
    for (n = 0; n < TABLESIZE(table); ++n) {
	if (!strcmp(text, table[n])) {
	    result = 1;
	    break;
	}
    }
    return result;
}

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

    /* this is defined in yacc.key */
    Terminal_attr = class_attr("Terminal");
    if (isEmpty(Terminal_attr))
	Terminal_attr = Ident2_attr;

    section = RuleSection;
    nested_section = 0;
    brace_level = 0;
    save_attr = Error_attr;

    begin_state(RULES);
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
