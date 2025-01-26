%pointer
%s RULES ACTIONS CODE COMMENT

%{

/*
 * $Id: vl-filt.l,v 1.24 2025/01/26 11:38:02 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of VeriLog
 * program.
 */

#include <filters.h>

DefineFilter(vlog);

static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static void do_define(char *string);

%}

SPACE		[ \t]

DIRECTIVE	`[[:alpha:]_][[:alnum:]_]*
IDENT		[[:alpha:]$_]+
ESCNAME		\\[[:graph:]]+
SYSNAME		\$[[:alpha:]_][[:alnum:]_]*

DECIMAL		('[dD])?([[:digit:]_]+)
BINARY		'[bB]([xXzZ?01_]+)
OCTAL		'[oO]([xXzZ?0-7_]+)
HEXADECIMAL	'[hH]([xXzZ?[:xdigit:]_]+)
INTEGER		{DECIMAL}|{HEXADECIMAL}|{OCTAL}|{BINARY}

DSTRING		\"(\\.|[^"\\])*\"

%%

<CODE>{ESCNAME}		{ WriteToken(get_keyword_attr(yytext)); }
<CODE>{SYSNAME}		{ const char *attr = get_keyword_attr(yytext);
			  if (attr != NULL) {
			      WriteToken(attr);
			  } else {
			      flt_error("unknown sysname");
			      WriteToken(Error_attr);
			  }
			}
<CODE>{IDENT}		{ WriteToken(get_keyword_attr(yytext)); }

<CODE>`define{SPACE}+{IDENT} { do_define(yytext); }
<CODE>{DIRECTIVE}	{ const char *attr = get_keyword_attr(yytext);
			  if (attr != NULL) {
			      WriteToken(attr);
			  } else {
			      flt_error("unknown directive");
			      WriteToken(Error_attr);
			  }
			}

<CODE>{INTEGER}		{ WriteToken(Number_attr); /* '_' should be embedded */ }

<CODE>"//"[^\r\n]*	{ WriteToken(Comment_attr); }
<CODE>"/*"		{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<COMMENT>[^*]*		{ WriteToken(Comment_attr); }
<COMMENT>"*"+[^*/]*	{ WriteToken(Comment_attr); }
<COMMENT>"*"+"/"	{ WriteToken(Comment_attr); BEGIN(CODE); }

{DSTRING}		{ WriteToken(String_attr); }

%%

static void
do_define(char *string)
{
    const char *keydef = "`define";
    int lendef = 7;

    flt_puts(string, lendef, get_keyword_attr(keydef));
    for (string += lendef; isspace(CharOf(*string)); string++)
	flt_putc(*string);
    string--;
    *string = *keydef;
    insert_keyword(string++, Ident2_attr, 0);
    flt_puts(string, (int) strlen(string), Ident2_attr);
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
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT);
    Ident2_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    BEGIN(CODE);
    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
