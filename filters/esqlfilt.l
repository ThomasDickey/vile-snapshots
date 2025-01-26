%pointer
%x COMMENT CODE SQL

%{

/*
 * $Id: esqlfilt.l,v 1.28 2025/01/26 11:05:40 tom Exp $
 *
 * Filter to add vile "attribution" sequences to embedded SQL with C/C++.
 * - T.Dickey
 */

#include <filters.h>

DefineFilter(esql);

static const char *other_symbols = "cpp";	/* FIXME: assumes ".merge cpp" */

static char *Action_attr;
static char *Comment_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static int my_state;
static int exec_sql;
static int in_execute;
static int in_block;

static void begin_sql(void);
static void process_keyword(char *text, int len);
static void reset_symbol_table(void);

%}

SPC		[ \t]
BLANK		{SPC}*

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\]|\\\n)*\"
STRINGS		({SSTRING}|{DSTRING})

INCLUDE		("<"[^>]+">"|\"[^"]+\")

IDENT		[[:alpha:]_][[:alnum:]_]*

SIGN		[-+]
DECIMAL		[[:digit:]_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[[:xdigit:]_]+
REAL		[-+]?([[:digit:]_]*\.[[:digit:]][[:digit:]_]*)([eE][+-]?[[:digit:]_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

%%

<CODE,SQL>^{BLANK}#{BLANK}include{BLANK}{INCLUDE} |
<CODE,SQL>^{BLANK}#{BLANK}{IDENT} { WriteToken(Preproc_attr); }

<CODE>"EXEC"{SPC}+"SQL"	|
<CODE>"exec"{SPC}+"sql"	{ WriteToken(Action_attr); begin_sql(); }

<SQL>{IDENT}-{IDENT}	|
<CODE,SQL>{IDENT}	{ process_keyword(yytext, yyleng); }
<CODE,SQL>";"		{ ECHO; reset_symbol_table(); }

<CODE,SQL>"/*"		{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<COMMENT>[^*]*		{ WriteToken(Comment_attr); }
<COMMENT>"*"+[^*/]*	{ WriteToken(Comment_attr); }
<COMMENT>"*"+"/"	{ WriteToken(Comment_attr); BEGIN(my_state); }

<CODE,SQL>"//"[^\r\n]*	{ WriteToken(Comment_attr); }

<CODE,SQL>{STRINGS}	{ WriteToken(String_attr); }

<CODE,SQL>{NUMBER}	{ WriteToken(Number_attr); }

<SQL>":"{IDENT}		{ WriteToken(Ident2_attr); }

%%

static void
default_state(int value)
{
    my_state = value;
    BEGIN(my_state);
}

static void
begin_sql(void)
{
    exec_sql = 2;
    set_symbol_table("esql");
    default_state(SQL);
}

static void
reset_symbol_table(void)
{
    exec_sql = 0;
    if (!in_execute && !in_block && my_state == SQL) {
	set_symbol_table(other_symbols);
	default_state(CODE);
    }
}

static void
process_keyword(char *text, int len)
{
    const char *temp = "";
    const char *attr = NULL;

    switch (exec_sql) {
    case 0:
	/* Note: this will not highlight the "exec" keyword, because we do
	 * not have a reliable way to do lookahead without a grammar on top.
	 * However, most developers seem to be unaware that embedded SQL is
	 * case-independent, and will use either the upper or lowercase
	 * patterns below, with no intervening comments or blank lines.
	 */
	if (len == 4
	    && !strcmp(lowercase_of(text), "exec")) {
	    exec_sql = 1;
	}
	break;
    case 1:
	if (len == 3
	    && !strcmp(lowercase_of(text), "sql")) {
	    begin_sql();
	} else {
	    reset_symbol_table();
	}
	break;
    case 2:
	temp = lowercase_of(text);
	if (!strcmp(temp, "execute")) {
	    in_execute = 1;
	} else if (!strcmp(temp, "begin")) {
	    in_block = 1;
	} else if (!strcmp(temp, "end")) {
	    in_block = 0;
	}
	exec_sql++;
	break;
    }
    if ((exec_sql >= 2) || in_execute || in_block) {
	if (*temp == 0)
	    temp = lowercase_of(text);
	if ((attr = get_keyword_attr(text)) == NULL) {
	    set_symbol_table(other_symbols);
	    attr = get_keyword_attr(text);
	    set_symbol_table(default_table);
	}
	if (in_execute
	    && (exec_sql == 0)
	    && !strcmp(temp, "end-exec")) {
	    in_execute = 0;
	    reset_symbol_table();
	}
    } else {
	attr = get_keyword_attr(text);
    }
    flt_puts(text, len, attr);
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
    Ident2_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    exec_sql = 0;
    in_execute = 0;
    in_block = 0;

    set_symbol_table(other_symbols);
    default_state(CODE);
    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
