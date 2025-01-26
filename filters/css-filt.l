%pointer
%x CSS_CODE CSS_NOTE

%e 2000
%k 2000

%{

/*
 * $Id: css-filt.l,v 1.31 2025/01/26 11:05:40 tom Exp $
 *
 * Filter to add vile "attribution" sequences to Cascading Style Sheets.
 *
 * http://www.w3.org/TR/REC-CSS1/
 * http://www.w3.org/TR/CSS2/
 */

#include <filters.h>

DefineFilter(css);

static const char *css_keyword_attr(char *);
static void css_hash(char *, int);
static void css_number_units(char *, int);
static void css_string_escapes(char *, int);

static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *String_attr;

static int css_state;

%}

NL		\n|\r\n|\r|\f
BLANK		[[:blank:]]*

CSS_UNICODE	\\([[:xdigit:]]{1,6})
CSS_ESCAPE	{CSS_UNICODE}|(\\[^\n\r\f[:xdigit:]])

CSS_STRING1	\'({CSS_ESCAPE}|\\{NL}|[^\n\r\f\\'])*\'
CSS_STRING2	\"({CSS_ESCAPE}|\\{NL}|[^\n\r\f\\"])*\"
CSS_STRING	({CSS_STRING1}|{CSS_STRING2})

CSS_INVALID1	\'({CSS_ESCAPE}|\\{NL}|[^\n\r\f\\'])*
CSS_INVALID2	\"({CSS_ESCAPE}|\\{NL}|[^\n\r\f\\"])*
CSS_INVALID	({CSS_INVALID1}|{CSS_INVALID2})

CSS_NMSTART	[[:alpha:]_]|{CSS_ESCAPE}
CSS_NMCHAR	[[:alnum:]_-]|{CSS_ESCAPE}
CSS_IDENT	[-]?{CSS_NMSTART}{CSS_NMCHAR}*
CSS_NAME	{CSS_NMCHAR}+

CSS_UNITS	("%"|[[:alpha:]]+)

CSS_HASH	#{CSS_NAME}

DECIMAL		[[:digit:]]+
REAL		([[:digit:]]*\.[[:digit:]]+)
NUMBER		({DECIMAL}|{REAL})

URL		[[:alpha:]]+"://"[[:alnum:]#%&+\,.\/?;@_=~-]+

%%

<CSS_CODE>"!"{CSS_IDENT}	|
<CSS_CODE>@{CSS_IDENT}	|
<CSS_CODE>{CSS_IDENT}	{ WriteToken(css_keyword_attr(yytext)); }
<CSS_CODE>[,;:(){}\[\]]	{ ECHO; css_state = yytext[0]; }

<CSS_CODE>"/*"		{ WriteToken(Comment_attr); BEGIN(CSS_NOTE); }
<CSS_NOTE>[^*]*		{ WriteToken(Comment_attr); }
<CSS_NOTE>"*"+[^*/]*	{ WriteToken(Comment_attr); }
<CSS_NOTE>"*"+"/"	{ WriteToken(Comment_attr); BEGIN(CSS_CODE); }

<CSS_CODE>{URL}		{ WriteToken(String_attr); }
<CSS_CODE>{CSS_STRING}	{ css_string_escapes(yytext, yyleng); }
<CSS_CODE>{CSS_INVALID}	{ WriteToken(Error_attr); }

<CSS_CODE>{NUMBER}{CSS_UNITS}	{ css_number_units(yytext, yyleng); }
<CSS_CODE>{CSS_HASH}	{ css_hash(yytext, yyleng); }
<CSS_CODE>{NUMBER}	{ WriteToken(Number_attr); }

%%

static const char *
css_keyword_attr(char *text)
{
    const char *attr = get_keyword_attr(text);
    if (isEmpty(attr))
	attr = Ident_attr;
    return attr;
}

static void
css_hash(char *text, int length)
{
    if (css_state == ':') {
	char *attr = (length == 4 || length == 7) ? Number_attr : Error_attr;
	flt_puts(text, length, attr);
    } else {
	flt_puts(text, length, css_keyword_attr(text + 1));
    }
}

static void
css_number_units(char *text, int length)
{
    int digits = length;
    const char *attr;

    while (digits > 0) {
	if (isdigit(CharOf(text[digits - 1])))
	    break;
	--digits;
    }

    flt_puts(text, digits, Number_attr);
    if (digits != length) {
	if (text[digits] == '%') {
	    flt_puts(text + digits, length - digits, "");
	} else {
	    set_symbol_table("cssUnits");
	    if ((attr = get_keyword_attr(text + digits)) == NULL)
		attr = Error_attr;
	    flt_puts(text + digits, length - digits, attr);
	    set_symbol_table(default_table);
	}
    }
}

static void
css_string_escapes(char *text, int length)
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
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);

    css_state = -1;
    BEGIN(CSS_CODE);
    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
