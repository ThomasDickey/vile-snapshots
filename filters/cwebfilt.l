%pointer
%x COMMENT PREP_COM

%{

/*
 * $Id: cwebfilt.l,v 1.24 2013/12/02 01:32:54 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Cwebx input
 * text.  Also useful for Cweb
 *
 * This is based on a filter written by Vanessa Conchodon (conchodo@univ-mlv.fr)
 * - T.Dickey
 */

#include <filters.h>

DefineFilter(cweb);

#define NAME_TeX_command "TeX_command"
#define NAME_TeX_comment "TeX_comment"
#define NAME_Cweb_module "Cweb_module"
#define NAME_Cweb_title  "CWeb_code_title"
#define NAME_Cweb_other  "CWeb_code_other"

static char *Comment_attr;
static char *Preproc_attr;
static char *Keyword_attr;
static char *Type_attr;
static char *String_attr;
static char *Number_attr;
static char *TeX_mot_cle;
static char *TeX_comment;
static char *Cweb_nom_module;
static char *Cweb_code_titre;
static char *Cweb_code_autre;

static int in_code;		/* FIXME: should implement a stack */

%}

SPC		[ ]
BLANK		[ \t]

INTEGER		[-+]?([[:digit:]]+)
REAL		[-+]?([[:digit:]]*\.[[:digit:]]+)([eE][+-]?[[:digit:]]+)?
IDENT		[[:alpha:]_][[:alnum:]_]*

SSTRING		\'(\\.|[^'\\]|@@)\'
DSTRING		\"(\\.|[^"\\])*\"
STRING		{SSTRING}|{DSTRING}
RC		\n

%%

#{BLANK}*if{BLANK}+"0"	{
			WriteToken(Preproc_attr);
			BEGIN(PREP_COM);
			}
<PREP_COM>^[^#{RC}].*$	{ WriteToken(Comment_attr); }
<PREP_COM>^{RC}		{ WriteToken(Comment_attr); }
<PREP_COM>#{BLANK}*endif  { WriteToken(Preproc_attr);
			  BEGIN(0);
			}

#{BLANK}*if	|
#{BLANK}*if(n)?def	|
#{BLANK}*else	|
#{BLANK}*endif	|
#{BLANK}*define	|
#{BLANK}*error	|
#{BLANK}*include{BLANK}*"<"(\\\.|[^>])*">"    |
#{BLANK}*include{BLANK}*\"(\\\.|[^\"])*\" |
#{BLANK}*undef		{ WriteToken(Preproc_attr); }

{IDENT}			{ WriteToken(in_code ? get_keyword_attr(yytext) : ""); }


"/*"			{ WriteToken(Comment_attr);
			  BEGIN(COMMENT);
			}
<COMMENT>[^\*\r\n]+	{ WriteToken(Comment_attr); }
<COMMENT>"*"+[^\*/\r\n]*	{ WriteToken(Comment_attr); }
<COMMENT>"*"+"*/"	{ WriteToken(Comment_attr);
			  BEGIN(0);
			}
<COMMENT>"*/"		{ WriteToken(Comment_attr);
			  BEGIN(0);
			}

"//".*$			{ WriteToken(Comment_attr); }


{REAL} 	|
{INTEGER}		{ WriteToken(Number_attr);}

{STRING}		{ WriteToken(String_attr); }

\\[[:alpha:]][[:alpha:]]*	{ WriteToken(TeX_mot_cle); }
\\[^\\[:alpha:]][ [:alpha:]]	{ WriteToken(Keyword_attr); }


"@*"[1-9]*[ ]*[^\.]*"." { WriteToken(Cweb_code_titre); }

"@<"[^@]*"@>"		{ WriteToken(Cweb_nom_module); }
"@<"[^@]*"@>"[=;]	{ WriteToken(Cweb_nom_module); in_code = 1; }

"@"c			{ WriteToken(Preproc_attr); in_code = 1; }
"@"[hd]			{ WriteToken(Preproc_attr); }

"@"{SPC}		{ WriteToken(Cweb_code_autre); in_code = 0; }
"@".			{ WriteToken(Cweb_code_autre); }

^"%".*$			{ WriteToken(TeX_comment); }

%%

static void
init_filter(int before)
{
    if (before) {
	insert_keyword(NAME_TeX_command, ATTR_KEYWORD, 1);
	insert_keyword(NAME_TeX_comment, ATTR_COMMENT, 1);
	insert_keyword(NAME_Cweb_module, ATTR_KEYWORD, 1);
	insert_keyword(NAME_Cweb_title, ATTR_IDENT2, 1);
	insert_keyword(NAME_Cweb_other, "", 1);
    }
}

static void
do_filter(FILE *inputs)
{
    InitLEX(inputs);

    in_code = 0;
    Comment_attr = class_attr(NAME_COMMENT);
    Preproc_attr = class_attr(NAME_PREPROC);
    Keyword_attr = class_attr(NAME_KEYWORD);
    String_attr = class_attr(NAME_LITERAL);
    Number_attr = class_attr(NAME_NUMBER);
    Type_attr = class_attr(NAME_TYPES);

    TeX_mot_cle = class_attr(NAME_TeX_command);
    TeX_comment = class_attr(NAME_TeX_comment);
    Cweb_nom_module = class_attr(NAME_Cweb_module);
    Cweb_code_titre = class_attr(NAME_Cweb_title);
    Cweb_code_autre = class_attr(NAME_Cweb_other);

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
