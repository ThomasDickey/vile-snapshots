%s DELETE INSERT

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sccsfilt.l,v 1.9 2003/02/15 11:53:27 tom Exp $
 *
 * Filter to add vile "attribution" sequences to SCCS files.
 */

#include <filters.h>

DefineFilter("sccs");

static char *Marker_attr;
static char *Insert_attr;
static char *Delete_attr;
static char *Comment_attr;
static char *Error_attr;
static char *String_attr;

#include <fltstack.h>

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
    yyin = input;

    Marker_attr  = class_attr("Marker");
    Insert_attr  = class_attr("Insert");
    Delete_attr  = class_attr("Delete");
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr   = class_attr(NAME_ERROR);
    String_attr  = class_attr(NAME_LITERAL);

    begin_state(INITIAL);
    while (yylex() > 0) {
    }
    end_state();
}

%}

MARKED		^\001[^\n]*

EXPANDED	"@(#)"[^\n]*

UNEXPANDED	"%"[A-Z]"%"

%%

<INSERT,DELETE>{UNEXPANDED}	{ WriteToken(String_attr); }
<INSERT,DELETE>{EXPANDED}	{ WriteToken(Error_attr); }

{MARKED}	{ flt_putc('\001');
		    WriteToken2((yytext[1] == 'c')
		  		? Comment_attr
				: Marker_attr, 1);
		    switch (yytext[1])
		    {
		    case 'I':
			push_state(INSERT);
			break;
		    case 'D':
			push_state(DELETE);
			break;
		    case 'E':
			pop_state();
			break;
		    default:
			new_state(INITIAL);
			break;
		    }
		}
<INSERT>.		|
<INSERT>[^%@\n]*	{ WriteToken(Insert_attr); }

<DELETE>.		|
<DELETE>[^%@\n]*	{ WriteToken(Delete_attr); }
