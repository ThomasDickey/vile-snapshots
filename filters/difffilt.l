%pointer
%s HEADER sDELETE INSERT MODIFY

%{

/*
 * $Id: difffilt.l,v 1.13 2013/12/02 01:32:54 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of diff output
 */

#include <filters.h>

DefineFilter(diff);

static char *Marker_attr;
static char *Header_attr;
static char *Insert_attr;
static char *Delete_attr;
static char *Modify_attr;

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

{DELETE}		{ ECHO; BEGIN(sDELETE); }
<sDELETE>{MARKED}	{ WriteToken(Delete_attr); BEGIN(INITIAL); }

{MODIFY}		{ ECHO; BEGIN(MODIFY); }
<MODIFY>{MARKED}	{ WriteToken(Modify_attr); BEGIN(INITIAL); }

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

    Marker_attr = class_attr("Marker");
    Header_attr = class_attr("Header");
    Insert_attr = class_attr("Insert");
    Delete_attr = class_attr("Delete");
    Modify_attr = class_attr("Modify");

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
