%x COMMENT PREP_COM

%{
/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/cwebfilt.l,v 1.6 1998/12/25 21:31:40 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of Cwebx input
 * text.  Also useful for Cweb
 *
 * This is based on a filter written by Vanessa Conchodon (conchodo@univ-mlv.fr)
 * - T.Dickey
 */

#include <filters.h>

char *filter_name = "cweb";

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

void
init_filter(int before)
{
    if (before) {
	insert_keyword(NAME_TeX_command, ATTR_KEYWORD);
	insert_keyword(NAME_TeX_comment, ATTR_COMMENT);
	insert_keyword(NAME_Cweb_module, ATTR_KEYWORD);
	insert_keyword(NAME_Cweb_title,  ATTR_IDENT2);
	insert_keyword(NAME_Cweb_other,  "");
    }
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;

    Comment_attr = keyword_attr(NAME_COMMENT);
    Preproc_attr = keyword_attr(NAME_PREPROC);
    Keyword_attr = keyword_attr(NAME_KEYWORD);
    String_attr  = keyword_attr(NAME_LITERAL);
    Number_attr  = keyword_attr(NAME_NUMBER);
    Type_attr    = keyword_attr(NAME_TYPES);

    TeX_mot_cle  = keyword_attr(NAME_TeX_command);
    TeX_comment  = keyword_attr(NAME_TeX_comment);
    Cweb_nom_module = keyword_attr(NAME_Cweb_module);
    Cweb_code_titre = keyword_attr(NAME_Cweb_title);
    Cweb_code_autre = keyword_attr(NAME_Cweb_other);

    while (yylex() > 0) {
    }
}

%}

INTEGER		[-+]?([0-9]+)
REAL		[-+]?([0-9]*\.[0-9]+)([eE][+-]?[0-9]+)?
IDENT		[a-zA-Z_][a-zA-Z_0-9]*
STRING		\"(\\\.|[^\"])*\"
RC		\n

%%

#[ \t]*if[ \t]+"0"	{
			WriteToken(Preproc_attr);
			BEGIN(PREP_COM);
			}
<PREP_COM>^[^#{RC}].*$	{ WriteToken(Comment_attr); }
<PREP_COM>^{RC}		{ WriteToken(Comment_attr); }
<PREP_COM>#[ \t]*endif  { WriteToken(Preproc_attr);
			  BEGIN(0);
			}

#[ \t]*if	|
#[ \t]*if(n)?def	|
#[ \t]*else	|
#[ \t]*endif	|
#[ \t]*define	|
#[ \t]*error	|
#[ \t]*include[ \t]*<(\\\.|[^>])*>    |
#[ \t]*include[ \t]*\"(\\\.|[^\"])*\" |
#[ \t]*undef		{ WriteToken(Preproc_attr); }

auto		|
break		|
case		|
continue	|
default		|
do		|
else		|
extern		|
for		|
goto		|
if		|
return		|
sizeof		|
static		|
switch		|
volatile	|
while		|

catch		|
class		|
delete		|
friend		|
inline		|
new		|
operator	|
overload	|
private		|
protected	|
public		|
template	|
this		|
virtual			{ WriteToken(Keyword_attr); }


char		|
const		|
double		|
enum		|
float		|
int		|
long		|
struct		|
typedef		|
union		|
unsigned	|
void			{ WriteToken(Type_attr); }


"/*"			{ WriteToken(Comment_attr);
			  BEGIN(COMMENT);
			}
<COMMENT>[^\*\n]+	{ WriteToken(Comment_attr); }
<COMMENT>"*"+[^\*/\n]*	{ WriteToken(Comment_attr); }
<COMMENT>"*"+"*/"	{ WriteToken(Comment_attr);
			  BEGIN(0);
			}
<COMMENT>"*/"		{ WriteToken(Comment_attr);
			  BEGIN(0);
			}

"//".*$			{ WriteToken(Comment_attr); }


{REAL} 	|
{INTEGER}		{ WriteToken(Number_attr);}


{IDENT}			{ printf("%s", yytext); }
{STRING}		{ WriteToken(String_attr); }

\\[a-zA-Z][a-zA-Z]*	{ WriteToken(TeX_mot_cle); }
\\[^\\a-zA-z][ a-zA-Z]	{ WriteToken(Keyword_attr); }


"@*"[1-9]*[ ]*[^\.]*"." { WriteToken(Cweb_code_titre); }

"@<"[^@]*"@>"		{ WriteToken(Cweb_nom_module); }

"@".			{ WriteToken(Cweb_code_autre); }

%%

/*
"%".*$			{ WriteToken(TeX_comment); }
*/
