%pointer
%s NORMAL RESNAME RESVALUE COMMENT

%{

/*
 * $Id: xresfilt.l,v 1.32 2025/01/26 11:40:03 tom Exp $
 *
 * Filter to add vile "attribution" sequences to an X resource file.
 */

#include <filters.h>

DefineFilter(xres);

static char *Comment_attr;
static char *Error_attr;
static char *Ident2_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static int escaped_nl(const char *, int);
static int isClass(char *name);
static int preprocessor(char *text, int size);
static void quoted_name(char *text, int length);

%}

SPACE		[ \t]

COMMENT		![^\r\n]*

SIGN		[-+]
DECIMAL		[[:digit:]]+
OCTAL		0[0-7]+
HEX		[[:xdigit:]]
HEXADECIMAL	0x{HEX}+
REAL		[-+]?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?
RGB		[Rr][Gg][Bb]:{HEX}+"/"{HEX}+"/"{HEX}+
COLOR		(#{HEX}+|{RGB})
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL}|{COLOR})

ESCAPED		(\\[0-7][0-7][0-7]|\\.)

NAME		[[:alnum:]_-]+
KEYWORD		({NAME}|[<]{NAME}[>])

%%

<NORMAL>[?*.]		{ BEGIN(RESNAME); ECHO; }
<NORMAL,RESNAME>{NAME}	{ WriteToken(isClass(yytext) ? Keyword_attr : Ident2_attr); }
<RESNAME>[:]		{ BEGIN(RESVALUE); ECHO; flt_bfr_begin(String_attr); }

<RESVALUE>{NUMBER}	{ flt_bfr_embed(yytext, yyleng, Number_attr); }
<RESVALUE>{KEYWORD}	{ quoted_name(yytext, yyleng); }
<RESVALUE>\\\n		{ flt_bfr_append(yytext, yyleng); }
<RESVALUE>({ESCAPED}|.)	{ flt_bfr_append(yytext, yyleng); }
<RESVALUE>\n		{ flt_bfr_finish(); ECHO; BEGIN(NORMAL); }

<NORMAL>{COMMENT}	{ WriteToken(Comment_attr);
			  if (escaped_nl(yytext, yyleng)) {
			      BEGIN(COMMENT);
			  }
			}
<COMMENT>[^\r\n]*	{ WriteToken(Comment_attr);
                          if (!escaped_nl(yytext, yyleng)) {
			      BEGIN(NORMAL);
			  }
		        }

<NORMAL>^{SPACE}*#{SPACE}*{NAME} {
			    if (!preprocessor(yytext, yyleng)) {
				flt_error("unknown directive");
				WriteToken(Error_attr);
			    }
			}

%%

static int
escaped_nl(const char *text, int size)
{
    return (size > 0 && text[size - 1] == '\\');
}

static char *
skip_name(char *text)
{
    while (isalnum(CharOf(*text)) || *text == '_')
	text++;
    return text;
}

static int
preprocessor(char *text, int size)
{
    char *s = skip_blanks(text);
    char *t;
    const char *attr = NULL;
    int save;

    if (set_symbol_table("cpre")) {
	s = skip_blanks(s + 1);	/* points to name */
	t = skip_name(s);
	save = *t;
	*t = 0;
	attr = get_keyword_attr(s);
	*t = (char) save;
	set_symbol_table(default_table);
    }
    if (attr != NULL) {
	flt_puts(text, size, Preproc_attr);
	return 1;
    }
    return 0;
}

static int
isClass(char *name)
{
    return (isalpha(CharOf(*name)) && isupper(CharOf(*name)));
}

/*
 * Buffer text for quoted-strings
 */
static void
quoted_name(char *text, int length)
{
    const char *attr = get_keyword_attr(lowercase_of(text));
    if (attr != NULL)
	flt_bfr_embed(text, length, attr);
    else
	flt_bfr_append(text, length);
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
    Ident2_attr = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    BEGIN(NORMAL);
    RunLEX();
    flt_bfr_finish();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
