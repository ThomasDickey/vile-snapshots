%s HEADER DELETE INSERT MODIFY

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/difffilt.l,v 1.4 2000/02/28 11:58:59 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of diff output
 */

#include <filters.h>

DefineFilter("diff");

static char *Marker_attr;
static char *Header_attr;
static char *Insert_attr;
static char *Delete_attr;
static char *Modify_attr;

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
    yyin = input;

    Marker_attr = class_attr("Marker");
    Header_attr = class_attr("Header");
    Insert_attr = class_attr("Insert");
    Delete_attr = class_attr("Delete");
    Modify_attr = class_attr("Modify");

    while (yylex() > 0) {
    }
}

%}

BLANK		[ \t]

HEADER		^("***"|"+++"|"---"|@@|#|diff\>|Index:|Binary\>)
INSERT		^[>+]
DELETE		^[<-]
MODIFY		^[|!]

MARKED		[^\n]*\n

%%

{HEADER}		{ WriteToken(Marker_attr); BEGIN(HEADER); }
<HEADER>{MARKED}	{ WriteToken(Header_attr); BEGIN(INITIAL); }

{INSERT}		{ ECHO; BEGIN(INSERT); }
<INSERT>{MARKED}	{ WriteToken(Insert_attr); BEGIN(INITIAL); }

{DELETE}		{ ECHO; BEGIN(DELETE); }
<DELETE>{MARKED}	{ WriteToken(Delete_attr); BEGIN(INITIAL); }

{MODIFY}		{ ECHO; BEGIN(MODIFY); }
<MODIFY>{MARKED}	{ WriteToken(Modify_attr); BEGIN(INITIAL); }
