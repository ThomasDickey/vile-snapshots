%x COMMENT CODE

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/css-filt.l,v 1.12 2006/04/02 16:49:54 tom Exp $
 *
 * Filter to add vile "attribution" sequences to Cascading Style Sheets.
 */

#include <filters.h>

DefineFilter("css");

static void hash_color(char *, int);
static void number_units(char *, int);
static void string_escapes(char *, int);

static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

static int state;

%}

NL		\n|\r\n|\r|\f
BLANK		[[:blank:]]*

UNICODE		\\[[:alnum:]]+
ESCAPE		{UNICODE}|(\\[^\n\r\f[:alnum:]])

STRING1		\'({ESCAPE}|\\{NL}|[^\n\r\f\\'])*\'
STRING2		\"({ESCAPE}|\\{NL}|[^\n\r\f\\"])*\"
STRING		({STRING1}|{STRING2})

INVALID1	\'({ESCAPE}|\\{NL}|[^\n\r\f\\'])*
INVALID2	\"({ESCAPE}|\\{NL}|[^\n\r\f\\"])*
INVALID		({INVALID1}|{INVALID2})

IDENT		[[:alpha:]_-][[:alnum:]_-]*
UNITS		[[:alpha:]]+

HEX		[[:xdigit:]]
HASH_COLOR	#({HEX})+

DECIMAL		[[:digit:]]+
REAL		([[:digit:]]*\.[[:digit:]]+)
NUMBER		({DECIMAL}|{REAL})

URL		[[:alpha:]]+"://"[[:alnum:]#%&+\,.\/?;@_=~-]+

%%

<CODE>"!"{IDENT}	|
<CODE>@{IDENT}		|
<CODE>{IDENT}		{ WriteToken(ci_keyword_attr(yytext)); }
<CODE>[;:(){}\[\]]	{ ECHO; state = yytext[0]; }

<CODE>"/*"		{ WriteToken(Comment_attr); BEGIN(COMMENT); }
<COMMENT>[^*]*		{ WriteToken(Comment_attr); }
<COMMENT>"*"+[^*/]*	{ WriteToken(Comment_attr); }
<COMMENT>"*"+"/"	{ WriteToken(Comment_attr); BEGIN(CODE); }

<CODE>{URL}		{ WriteToken(String_attr); }
<CODE>{STRING}		{ string_escapes(yytext, yyleng); }
<CODE>{INVALID}		{ WriteToken(Error_attr); }

<CODE>{NUMBER}{UNITS}	{ number_units(yytext, yyleng); }
<CODE>{HASH_COLOR}	{ hash_color(yytext, yyleng); }
<CODE>{NUMBER}		{ WriteToken(Number_attr); }

%%

static void
hash_color(char *text, int length)
{
    if (state == ':') {
	char *attr = (length == 4 || length == 7) ? Number_attr : Error_attr;
	flt_puts(text, length, attr);
    } else {
	flt_puts(text, length, ci_keyword_attr(text + 1));
    }
}

static void
number_units(char *text, int length)
{
    int digits = length;
    char *attr;

    while (digits > 0) {
	if (isdigit(CharOf(text[digits - 1])))
	    break;
	--digits;
    }

    flt_puts(text, digits, Number_attr);
    if (digits != length) {
	set_symbol_table("cssUnits");
	if ((attr = ci_keyword_attr(text + digits)) == 0)
	    attr = Error_attr;
	flt_puts(text + digits, length - digits, attr);
	set_symbol_table(filter_def.filter_name);
    }
}

static void
string_escapes(char *text, int length)
{
    while (length > 0) {
	int n;
	int found;
	for (n = 0, found = -1; n < length; ++n) {
	    if (text[n] == '\\') {
		found = n;
		break;
	    }
	}
	if (found >= 0) {
	    if (length > 1) {
		flt_puts(text, found, String_attr);
		text += found;
		length -= found;
		if (isxdigit(CharOf(text[1]))) {
		    for (n = 2, found = 2; n < length; ++n) {
			if (isxdigit(CharOf(text[n]))) {
			    found = n + 1;
			} else {
			    break;
			}
		    }
		} else {
		    found = 2;
		}
		flt_puts(text, found, Number_attr);
		text += found;
		length -= found;
	    } else {
		break;
	    }
	} else {
	    break;
	}
    }
    flt_puts(text, length, String_attr);
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
    Ident2_attr  = class_attr(NAME_IDENT2);
    Number_attr  = class_attr(NAME_NUMBER);
    String_attr  = class_attr(NAME_LITERAL);

    state = -1;
    BEGIN(CODE);
    while (yylex() > 0) {
    }
}
