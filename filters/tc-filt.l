%s DESC GAPS CAPS VALS LINK

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/tc-filt.l,v 1.26 2006/05/30 20:24:21 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of termcap
 */

#include <filters.h>

DefineFilter("tc");

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Number_attr;
static char *Keyword_attr;
static char *Ident_attr;
static char *String_attr;

static int commented;

static void begin_value(char *name);
static void write_capname(char *name, int len);
static void write_value(char *name, int len, char *attr);

%}

CAPNAME		[^\.| #\\\t\r\n:=]+
DOTNAME		(\.)+{CAPNAME}

ALIAS		[[:alnum:]_\.+-]+

NUMBER		"#"[[:digit:]]+
STRING		[^#:\\\r\n](\\[.\n]|[^:\r\n])*

%%

^"#"[^\r\n]*			{ WriteToken(Comment_attr); }
\n\n				{ ECHO; BEGIN(INITIAL); }

^({ALIAS}"|"|(\\\n\t)*)*	{ WriteToken(Ident_attr); BEGIN(DESC); }
^{ALIAS}			{ WriteToken(Ident_attr); BEGIN(GAPS); }

<DESC>[^:\r\n]*			{ WriteToken(Comment_attr); BEGIN(GAPS); }
<GAPS,VALS>(:|:\\\n\t)+		{ commented=0; ECHO; BEGIN(CAPS); }
<GAPS,CAPS>\n(\t)+		{ flt_error("unexpected whitespace");
				  WriteToken(Error_attr); }
<CAPS>{CAPNAME}			{ write_capname(yytext, yyleng); begin_value(yytext); }
<CAPS>{DOTNAME}			{ WriteToken(Comment_attr); commented = 1; begin_value(yytext); }
<VALS>{NUMBER}			{ write_value(yytext, yyleng, Number_attr); }
<VALS>{STRING}			{ write_value(yytext, yyleng, String_attr); }
<LINK>{STRING}			{ write_value(yytext, yyleng, Ident_attr); }

%%

static char *
check_name(char *name)
{
    char temp[3];
    char *attr;

    strncpy(temp, name, 2);
    temp[2] = 0;
    attr = keyword_attr(temp);
    if (attr == 0)
	attr = "";
    if (strcmp(attr, Keyword_attr))
    	attr = Action_attr;	/* not necessarily an error */
    return attr;
}

static int
ok_number(char *string)
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
write_capname(char *name, int len)
{
    flt_puts(name, (len >= 2) ? 2 : len, check_name(name));
    if (len > 2) {
	char *attr = Error_attr;
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
write_value(char *name, int len, char *attr)
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
begin_value(char *name)
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
    yyin = inputs;

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr  = class_attr(NAME_NUMBER);
    Ident_attr   = class_attr(NAME_IDENT);
    String_attr  = class_attr(NAME_LITERAL);

    commented = 0;

    BEGIN(INITIAL);
    while (yylex() > 0) {
    }
}
