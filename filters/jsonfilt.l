%pointer
%x COMMENT INLINE

%{

/*
 * $Id: jsonfilt.l,v 1.14 2025/01/26 11:06:34 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of JSON input
 * text.  JSON officially has no comments; some applications use JSON-syntax
 * with comments (this highlighter allows that).
 *
 * TODO: track levels of curly/square brackets, flag error on mismatch.
 * TODO: if newline in quoted string, error the rest of string.
 * TODO: mark all errors with messages
 */

#include <filters.h>

DefineFilter(json);

static char *Action_attr;
static char *Comment_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *String_attr;
static char *Error_attr;

#include <fltstack.h>

static void write_string(const char *, int);

%}

NO_OCTAL	0[0-7]+
NO_HEX		0[xX][[:xdigit:]]+
NO_NUMBER	{NO_OCTAL}|{NO_HEX}

SIGN		[+-]
QS		({SIGN}?)
UNSIGNED	(([1-9][[:digit:]]*)|0)
FRAC		([[:digit:]]*\.[[:digit:]]+)
INTEGER		{QS}{UNSIGNED}
REAL		{QS}({FRAC}|{UNSIGNED})([eE]{QS}{UNSIGNED})?
NUMBER		{INTEGER}|{REAL}

IDENT		[[:alpha:]][[:alnum:]]*

STRING		\"(\\.|[^"\\])*\"

%%

<INITIAL>"/*"		{ PushQuote(COMMENT, Comment_attr); }
<COMMENT>"*/"		{ PopQuote(); }
<COMMENT>[\r\n] |
<COMMENT>.		{ flt_bfr_append(yytext, yyleng); }

<INITIAL>"//"		{ PushQuote(INLINE, Comment_attr); }
<INLINE>[\r\n]		{ PopQuote(); }
<INLINE>[^\r\n]		{ flt_bfr_append(yytext, yyleng); }

<INITIAL>{IDENT}	{ const char *attr = get_keyword_attr(yytext);
			  if (attr == NULL)
			      attr = Error_attr;
			  WriteToken(attr);
			}

<INITIAL>{STRING}	{ write_string(yytext, yyleng); }

<INITIAL>{NUMBER}	{ WriteToken(Number_attr); }
<INITIAL>{NO_NUMBER}	{ WriteToken(Error_attr); }

%%

#include <fltstack.h>

static void
write_string(const char *text, int length)
{
    int n;

    flt_bfr_begin(String_attr);
    for (n = 0; n < length; ++n) {
	int ch = text[n];
	if (ch == '\\') {
	    int xx = text[++n];
	    int ok = 2;
	    switch (xx) {
	    case '"':
	    case '\\':
	    case '/':
	    case 'b':
	    case 'f':
	    case 'n':
	    case 'r':
	    case 't':
		break;
	    case 'u':
		if (n + 4 < length) {
		    ok = 6;
		} else {
		    ok = 0;
		}
		break;
	    default:
		ok = 0;
		break;
	    }
	    if (ok) {
		flt_bfr_append(text + n - 1, ok);
		n += ok - 2;
	    } else {
		flt_bfr_embed(text + n - 1, 2, Error_attr);
	    }
	} else {
	    flt_bfr_append(text + n, 1);
	}
    }
    flt_bfr_finish();
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
    Ident_attr = class_attr(NAME_IDENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    String_attr = class_attr(NAME_LITERAL);
    Error_attr = class_attr(NAME_ERROR);

    set_symbol_table(default_table);

    begin_state(INITIAL);
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
