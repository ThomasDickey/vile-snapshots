%s DELETE INSERT

%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/sccsfilt.l,v 1.4 2000/02/10 11:30:12 tom Exp $
 *
 * Filter to add vile "attribution" sequences to SCCS files.
 */

#include <filters.h>

DefineFilter("sccs");

static char *Marker_attr;
static char *Insert_attr;
static char *Delete_attr;
static char *Comment_attr;

typedef struct _States {
    struct _States *link;
    int old_state;
} States;

static States *state_stk;

static int
pushstate(int state)
{
    States *ptr = (States *)malloc(sizeof(States));
    ptr->link = state_stk;
    ptr->old_state = YYSTATE;
    state_stk = ptr;
    return state;
}

static int
popstate(void)
{
    States *ptr;
    int value = INITIAL;

    if ((ptr = state_stk) != 0) {
	value = ptr->old_state;
	state_stk = ptr->link;
	free(ptr);
    }

    return value;
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input, FILE *output)
{
    yyin = input;
    yyout = output;

    Marker_attr  = class_attr("Marker");
    Insert_attr  = class_attr("Insert");
    Delete_attr  = class_attr("Delete");
    Comment_attr = class_attr(NAME_COMMENT);

    while (yylex() > 0) {
    }
}

%}

MARKED		^\001[^\n]*

%%

{MARKED}	{ putchar('?');
		    WriteToken2((yytext[1] == 'c')
		  		? Comment_attr
				: Marker_attr, 1);
		    switch (yytext[1])
		    {
		    case 'I':
			BEGIN(pushstate(INSERT));
			break;
		    case 'D':
			BEGIN(pushstate(DELETE));
			break;
		    case 'E':
			BEGIN(popstate());
			break;
		    default:
			BEGIN(INITIAL);
			break;
		    }
		}
<INSERT>[^\n]*	{ WriteToken(Insert_attr); }

<DELETE>[^\n]*	{ WriteToken(Delete_attr); }
