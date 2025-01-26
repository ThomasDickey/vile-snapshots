%pointer
%s NAME DESC GAPS CAPS VALS LINK

%{

/*
 * $Id: tc-filt.l,v 1.42 2025/01/26 11:38:02 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of termcap
 */

#include <filters.h>
#undef YY_NO_UNPUT

DefineFilter(tc);

#define WriteAlias(attr) \
	WriteToken3(attr, yyleng - 1); \
	WriteToken2(String_attr, yyleng - 1)

static const char *Action_attr;
static const char *Comment_attr;
static const char *Error_attr;
static const char *Number_attr;
static const char *Keyword_attr;
static const char *Ident_attr;
static const char *String_attr;

static int commented;

static void begin_value(const char *name);
static void write_capname(const char *name, int len);
static void write_value(const char *name, int len, const char *attr);

%}

CAPNAME		[^\.| \\\t\r\n:=][^\.| #\\\t\r\n:=]+
DOTNAME		(\.)+{CAPNAME}

ALIAS		[[:alnum:]_\.+-]+

NUMBER		"#"[[:digit:]]+

GAPS		\\[\n]([[:blank:]]*)
CNTRL		"^"(([\\][\\]?)|[][[:alnum:]@^~_?])
ESCAPE		[\\][[:print:]]
STRING		({CNTRL}|{ESCAPE}|[^#:^\\\r\n])({CNTRL}|{ESCAPE}|[^:^\\\r\n]|{GAPS})*
STRINGER	({CNTRL}|{ESCAPE})({CNTRL}|{ESCAPE}|[^:^\\\r\n]|{GAPS})*

%%

^"#"[^\r\n]*			{ WriteToken(Comment_attr); }
\n\n				{ ECHO; BEGIN(INITIAL); }

^{ALIAS}"|"			{ WriteAlias(Ident_attr); BEGIN(NAME); }
<NAME>{ALIAS}"|"		{ WriteAlias(Ident_attr); }
<NAME>(\\\n\t)+			{ ECHO; }
<NAME>":"			{ ECHO; BEGIN(GAPS); }
<NAME>.				{ unput(yytext[0]); BEGIN(DESC); }
^{ALIAS}			{ WriteToken(Ident_attr); BEGIN(GAPS); }

<DESC>[^:\r\n]*			{ WriteToken(Comment_attr); BEGIN(GAPS); }
<GAPS,VALS>(:|:\\\n\t)+		{ commented=0; ECHO; BEGIN(CAPS); }
<GAPS,CAPS>\n(\t)+		{ flt_error("unexpected whitespace");
				  WriteToken(Error_attr); }
<GAPS,CAPS>{GAPS}		{ ECHO; }
<GAPS,CAPS>{STRINGER}		{ flt_error("unexpected string");
				  WriteToken(Error_attr); }

<CAPS>{CAPNAME}			{ write_capname(yytext, yyleng); begin_value(yytext); }
<CAPS>{DOTNAME}			{ WriteToken(Comment_attr); commented = 1; begin_value(yytext); }
<VALS>{NUMBER}			{ write_value(yytext, yyleng, Number_attr); }
<VALS>{STRING}			{ write_value(yytext, yyleng, String_attr); }
<LINK>{STRING}			{ write_value(yytext, yyleng, Ident_attr); }

%%

static const char *
check_name(const char *name)
{
    char temp[3];
    const char *attr;

    strncpy(temp, name, (size_t) 2);
    temp[2] = 0;
    attr = get_keyword_attr(temp);
    if (attr == NULL)
	attr = "";
    if (strcmp(attr, Keyword_attr))
	attr = Action_attr;	/* not necessarily an error */
    return attr;
}

static int
ok_number(const char *string)
{
    int result = 0;
    while (*string != 0) {
	if (isdigit(CharOf(*string))) {
	    result++;
	} else {
	    result = 0;
	    break;
	}
	string++;
    }
    return result;
}

static void
write_capname(const char *name, int len)
{
    flt_puts(name, (len >= 2) ? 2 : len, check_name(name));
    if (len > 2) {
	const char *attr = Error_attr;
	if (len == 3 && name[2] == '@') {
	    attr = "";
	} else if (len > 3 && name[2] == '#' && ok_number(name + 3)) {
	    attr = Number_attr;
	} else {
	    flt_error("unexpected token");
	}
	flt_puts(name + 2, len - 2, attr);
    }
}

static void
write_value(const char *name, int len, const char *attr)
{
    if (commented) {
	flt_puts(name, len, Comment_attr);
    } else {
	flt_puts(name, 1, "");
	flt_puts(name + 1, len - 1, attr);
    }
    BEGIN(GAPS);
}

static void
begin_value(const char *name)
{
    if (!strcmp(name, "tc"))
	BEGIN(LINK);
    else
	BEGIN(VALS);
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
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    Ident_attr = class_attr(NAME_IDENT);
    String_attr = class_attr(NAME_LITERAL);

    commented = 0;

    BEGIN(INITIAL);
    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
