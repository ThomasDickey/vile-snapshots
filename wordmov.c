/*
 * These routines report on transitions between word boundaries, both
 *	in the punctuated vi sense, and in the whitespace/darkspace
 *	sense.  The transition is reported _after_ it has occurred.  You
 *	need to back up to get to the char. before the transition.
 *	Written for vile.  Copyright (c) 1990, 1995-1999 by Paul Fox
 *
 * $Id: wordmov.c,v 1.26 2010/02/07 13:58:08 tom Exp $
 *
 */

#include "estruct.h"
#include "edef.h"

#define WASSPACE	0
#define ISSPACE		0
#define WASIDENT	1
#define ISIDENT		1
#define WASOTHER	2
#define ISOTHER		2
#define WASNL		3
#define ISNL		3

static int ochartype;

static int
getchartype(void)
{
    CHARTYPE type;
    int result;
    int ch;

    if (is_at_end_of_line(DOT)) {
	result = (ISNL);
    } else {
	ch = CharAtDot();
	type = CTypeOf(ch);
	result = ((type & vl_space) ? ISSPACE :
		  ((type & vl_ident) ? ISIDENT : ISOTHER));
    }
    return result;
}

void
setchartype(void)
{
    ochartype = getchartype();
}

int
isnewwordf(void)
{
    int ret = FALSE;
    int type;

    type = getchartype();

    switch (ochartype) {
    case WASNL:
    case WASSPACE:
	switch (type) {
	case ISNL:
	    if (doingopcmd) {
		ret = SORTOFTRUE;
		break;
	    }
	    /* FALLTHROUGH */
	case ISSPACE:
	    ret = FALSE;
	    break;
	case ISIDENT:
	    /* FALLTHROUGH */
	case ISOTHER:
	    ret = TRUE;
	    break;
	}
	break;
    case WASIDENT:
    case WASOTHER:
	switch (type) {
	case ISNL:
	    if (doingopcmd) {
		ret = SORTOFTRUE;
		break;
	    }
	    /* FALLTHROUGH */
	case ISSPACE:
	    if (doingopcmd && opcmd != OPDEL) {
		ret = SORTOFTRUE;
		break;
	    }
	    /* FALLTHROUGH */
	case ISIDENT:
	    /* FALLTHROUGH */
	case ISOTHER:
	    ret = FALSE;
	    break;
	}
	break;
    }
    ochartype = type;
    return (ret);
}

int
isnewwordb(void)
{
    int ret = FALSE;
    int type;

    type = getchartype();

    switch (ochartype) {
    case WASNL:
    case WASSPACE:
	ret = FALSE;
	break;
    case WASIDENT:
	switch (type) {
	case ISNL:
	case ISSPACE:
	    ret = TRUE;
	    break;
	case ISIDENT:
	case ISOTHER:
	    ret = FALSE;
	    break;
	}
	break;
    case WASOTHER:
	switch (type) {
	case ISNL:
	case ISSPACE:
	    ret = TRUE;
	    break;
	case ISIDENT:
	case ISOTHER:
	    ret = FALSE;
	    break;
	}
	break;
    }
    ochartype = type;
    return (ret);
}

int
isnewviwordf(void)
{
    int ret = FALSE;
    int type;

    type = getchartype();

    switch (ochartype) {
    case WASNL:
    case WASSPACE:
	switch (type) {
	case ISNL:
	    if (doingopcmd) {
		ret = SORTOFTRUE;
		break;
	    }
	    /* FALLTHROUGH */
	case ISSPACE:
	    ret = FALSE;
	    break;
	case ISIDENT:
	    /* FALLTHROUGH */
	case ISOTHER:
	    ret = TRUE;
	    break;
	}
	break;
    case WASIDENT:
	switch (type) {
	case ISNL:
	    if (doingopcmd) {
		ret = SORTOFTRUE;
		break;
	    }
	    /* FALLTHROUGH */
	case ISSPACE:
	    if (doingopcmd && opcmd != OPDEL) {
		ret = SORTOFTRUE;
		break;
	    }
	    /* FALLTHROUGH */
	case ISIDENT:
	    ret = FALSE;
	    break;
	case ISOTHER:
	    ret = TRUE;
	    break;
	}
	break;
    case WASOTHER:
	switch (type) {
	case ISNL:
	    if (doingopcmd) {
		ret = SORTOFTRUE;
		break;
	    }
	    /* FALLTHROUGH */
	case ISSPACE:
	    if (doingopcmd && opcmd != OPDEL) {
		ret = SORTOFTRUE;
		break;
	    }
	    /* FALLTHROUGH */
	case ISOTHER:
	    ret = FALSE;
	    break;
	case ISIDENT:
	    ret = TRUE;
	    break;
	}
	break;
    }
    ochartype = type;
    return (ret);
}

int
isnewviwordb(void)
{
    int ret = FALSE;
    int type;

    type = getchartype();

    switch (ochartype) {
    case WASNL:
    case WASSPACE:
	ret = FALSE;
	break;
    case WASIDENT:
	switch (type) {
	case ISNL:
	case ISSPACE:
	case ISOTHER:
	    ret = TRUE;
	    break;
	case ISIDENT:
	    ret = FALSE;
	    break;
	}
	break;
    case WASOTHER:
	switch (type) {
	case ISNL:
	case ISSPACE:
	case ISIDENT:
	    ret = TRUE;
	    break;
	case ISOTHER:
	    ret = FALSE;
	    break;
	}
	break;
    }
    ochartype = type;
    return (ret);
}

int
isendwordf(void)
{
    int ret = FALSE;
    int type;

    type = getchartype();

    switch (ochartype) {
    case WASNL:
    case WASSPACE:
	ret = FALSE;
	break;
    case WASIDENT:
	switch (type) {
	case ISNL:
	case ISSPACE:
	    if (doingopcmd)
		ret = SORTOFTRUE;
	    else
		ret = TRUE;
	    if (doingsweep)
		sweephack = TRUE;
	    break;
	case ISIDENT:
	case ISOTHER:
	    ret = FALSE;
	    break;
	}
	break;
    case WASOTHER:
	switch (type) {
	case ISNL:
	case ISSPACE:
	    if (doingopcmd)
		ret = SORTOFTRUE;
	    else
		ret = TRUE;
	    if (doingsweep)
		sweephack = TRUE;
	    break;
	case ISIDENT:
	case ISOTHER:
	    ret = FALSE;
	    break;
	}
	break;
    }
    ochartype = type;
    return (ret);
}

int
isendviwordf(void)
{
    int ret = FALSE;
    int type;

    type = getchartype();

    switch (ochartype) {
    case WASNL:
    case WASSPACE:
	ret = FALSE;
	break;
    case WASIDENT:
	switch (type) {
	case ISNL:
	case ISSPACE:
	case ISOTHER:
	    if (doingopcmd)
		ret = SORTOFTRUE;
	    else
		ret = TRUE;
	    if (doingsweep)
		sweephack = TRUE;
	    break;
	case ISIDENT:
	    ret = FALSE;
	    break;
	}
	break;
    case WASOTHER:
	switch (type) {
	case ISNL:
	case ISSPACE:
	case ISIDENT:
	    if (doingopcmd)
		ret = SORTOFTRUE;
	    else
		ret = TRUE;
	    if (doingsweep)
		sweephack = TRUE;
	    break;
	case ISOTHER:
	    ret = FALSE;
	    break;
	}
	break;
    }
    ochartype = type;
    return (ret);
}

#ifdef template
int
isANYTHING(void)
{
    int ret = FALSE;
    int type;

    type = getchartype();

    switch (ochartype) {
    case WASNL:
    case WASSPACE:
	switch (type) {
	case ISNL:
	case ISSPACE:
	    ret = FALSE;
	    break;
	case ISIDENT:
	    ret = FALSE;
	    break;
	case ISOTHER:
	    ret = FALSE;
	    break;
	}
	break;
    case WASIDENT:
	switch (type) {
	case ISNL:
	case ISSPACE:
	    ret = TRUE;
	    break;
	case ISIDENT:
	    ret = FALSE;
	    break;
	case ISOTHER:
	    ret = TRUE;
	    break;
	}
	break;
    case WASOTHER:
	switch (type) {
	case ISNL:
	case ISSPACE:
	    ret = TRUE;
	    break;
	case ISIDENT:
	    ret = TRUE;
	    break;
	case ISOTHER:
	    ret = FALSE;
	    break;
	}
	break;
    }
    ochartype = type;
    return (ret);
}
#endif /* template */
