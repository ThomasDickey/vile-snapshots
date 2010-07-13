%pointer

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/nr-filt.l,v 1.30 2010/07/13 13:44:40 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of NROFF
 */

#include <filters.h>
#undef YY_NO_UNPUT

DefineFilter("nr");

static char *Action_attr;
static char *Comment_attr;
static char *Keyword_attr;
static char *Ident2_attr;
static char *String_attr;

static void
write_cmd(char *text, int size)
{
    int cmd = size;
    int got = 0;
    char *s;

    for (s = text; *s; s++) {
	int ch = CharOf(*s);
	if (isalnum(ch)) {
	    got++;
	} else if ((got && isspace(ch)) || ((s != text) && ispunct(ch))) {
	    if (cmd == size)
		cmd = (int) (s - text) + 1;
	}
    }

    flt_puts(text, cmd, Keyword_attr);
    flt_puts(text+cmd, size-cmd, String_attr);
}

static void
write_font(char *text, int size)
{
    flt_puts(text, 3, Ident2_attr);
    flt_puts(text + 3, size-6, String_attr);
    flt_puts(text + size - 3, 3, Ident2_attr);
}

%}

COMMAND		^[\.'][[:blank:]]*([[:alnum:]]+|(\\[^\\\r\n\"#])+)([ \t][^\r\n]+)?
COMMENT		^[\.'][[:blank:]]*(\\\"|#)
END_COMMAND	^\.\.$

ENIT		\\[e *\n\\+-]

USE_MACRO	\(..
USE_SUBST	\\\*
FONT		\\f([BI1-4]|{USE_MACRO})

%%

{COMMAND}			{ write_cmd(yytext, yyleng); }
{END_COMMAND}			{ WriteToken(Action_attr); }

{USE_SUBST}{USE_MACRO}		{ WriteToken3(Action_attr, 3);
				  WriteToken2(String_attr, 3);
				}

\\f[PR1]			{ WriteToken(Action_attr); }
{FONT}[\\\r\n]			{ WriteToken3(Action_attr, 3);
				  unput(yytext[yyleng - 1]);
				}

{FONT}([^\\\r\n]|{ENIT})+(\\f[^PR1]|\n)	{ WriteToken(Action_attr); /* err? */ }
{FONT}([^\\\r\n]|{ENIT})+\\f[PR1]	{ write_font(yytext, yyleng); }
{ENIT}				{ WriteToken(Ident2_attr); }
\\[s][+-]?[[:digit:]]+		{ WriteToken(Ident2_attr); }

{COMMENT}[^\r\n]*		{ WriteToken(Comment_attr); }

%%

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
}

static void
do_filter(FILE *inputs)
{
    InitLEX(inputs);

    Action_attr  = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Ident2_attr  = class_attr(NAME_IDENT2);
    Keyword_attr = class_attr(NAME_KEYWORD);
    String_attr  = class_attr(NAME_LITERAL);

    RunLEX();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
