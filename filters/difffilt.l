%s HEADER DELETE INSERT MODIFY

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/difffilt.l,v 1.2 1999/08/05 10:52:10 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of diff output
 */

#include <filters.h>

char *filter_name = "diff";

static char *Marker_attr;
static char *Header_attr;
static char *Insert_attr;
static char *Delete_attr;
static char *Modify_attr;

void
init_filter(int before GCC_UNUSED)
{
}

void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;

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
