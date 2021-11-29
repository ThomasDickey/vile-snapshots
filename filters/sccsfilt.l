%pointer
%s sDELETE INSERT

%a 10000
%o 10000

%{

/*
 * $Id: sccsfilt.l,v 1.23 2013/12/02 01:32:53 tom Exp $
 *
 * Filter to add vile "attribution" sequences to SCCS files.
 */

#include <filters.h>

DefineFilter(sccs);

static char *Marker_attr;
static char *Insert_attr;
static char *Delete_attr;
static char *Comment_attr;
static char *Error_attr;
static char *String_attr;

#include <fltstack.h>

%}

MARKED		^\001[^\r\n]*

EXPANDED	"@(#)"[^\r\n]*

UNEXPANDED	"%"[[:upper:]]"%"

%%

<INSERT,sDELETE>{UNEXPANDED}	{ WriteToken(String_attr); }
<INSERT,sDELETE>{EXPANDED}	{ flt_error("expanded keywords");
				  WriteToken(Error_attr); }

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
			push_state(sDELETE);
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
<INSERT>[^%@\r\n]*	{ WriteToken(Insert_attr); }

<sDELETE>.		|
<sDELETE>[^%@\r\n]*	{ WriteToken(Delete_attr); }

%%

#include <fltstack.h>

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
}

static void
do_filter(FILE *inputs)
{
    InitLEX(inputs);

    Marker_attr = class_attr("Marker");
    Insert_attr = class_attr("Insert");
    Delete_attr = class_attr("Delete");
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    String_attr = class_attr(NAME_LITERAL);

    begin_state(INITIAL);
    RunLEX();
    end_state();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
