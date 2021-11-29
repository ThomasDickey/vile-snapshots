/*
 * These functions perform vi's on-this-line character scanning functions.
 * written for vile.  Copyright (c) 1990, 1995-2002 by Paul Fox
 *
 * $Id: csrch.c,v 1.35 2006/11/06 21:00:51 tom Exp $
 *
*/

#include "estruct.h"
#include "edef.h"

static USHORT lstscan;
static int lstchar;

#define BACK	0
#define FORW	1
#define DIREC	1

#define F	0
#define T	2
#define TYPE	2

static int
fscan(int f, int n, int c)
{
    int i;
    int doto;

    n = need_at_least(f, n, 1);

    lstchar = c;
    lstscan = FORW;

    doto = DOT.o;

    i = doto + 1;
    while (i < llength(DOT.l)) {
	if (c == lgetc(DOT.l, i)) {
	    doto = i;
	    n--;
	    if (!n)
		break;
	}
	i++;
    }

    if (i >= llength(DOT.l)) {
	return (FALSE);
    }
    if (doingopcmd && !doingsweep)
	doto++;
    else if (doingsweep)
	sweephack = TRUE;

    DOT.o = doto;
    curwp->w_flag |= WFMOVE;
    return (TRUE);

}

static int
bscan(int f, int n, int c)
{
    int i;
    int doto;

    n = need_at_least(f, n, 1);

    lstchar = c;
    lstscan = BACK;

    doto = DOT.o;

    i = doto - 1;
    while (i >= w_left_margin(curwp)) {
	if (c == lgetc(DOT.l, i)) {
	    doto = i;
	    n--;
	    if (!n)
		break;
	}
	i--;
    }

    if (i < w_left_margin(curwp)) {
	return (FALSE);
    }

    DOT.o = doto;
    curwp->w_flag |= WFMOVE;
    return (TRUE);

}

static int
get_csrch_char(int *cp)
{
    int c;

    if (clexec || isnamedcmd) {
	int status;
	static char cbuf[2];
	if ((status = mlreply("Scan for: ", cbuf, 2)) != TRUE)
	    return status;
	c = cbuf[0];
    } else {
	c = keystroke();
	if (c == quotec)
	    c = keystroke_raw8();
	else if (ABORTED(c))
	    return FALSE;
    }

    *cp = c;
    return TRUE;
}

/* f */
int
fcsrch(int f, int n)
{
    int c, s;

    s = get_csrch_char(&c);
    if (s != TRUE)
	return s;

    return (fscan(f, n, c));
}

/* F */
int
bcsrch(int f, int n)
{
    int c, s;

    s = get_csrch_char(&c);
    if (s != TRUE)
	return s;

    return (bscan(f, n, c));
}

/* t */
int
fcsrch_to(int f, int n)
{
    int s;
    s = fcsrch(f, n);
    if (s == TRUE)
	s = backchar(FALSE, 1);
    lstscan |= T;
    return (s);
}

/* T */
int
bcsrch_to(int f, int n)
{
    int s;
    s = bcsrch(f, n);
    if (s == TRUE)
	s = forwchar(FALSE, 1);
    lstscan |= T;
    return (s);
}

/* ; */
int
rep_csrch(int f, int n)
{
    int s;
    USHORT ls = lstscan;

    if ((ls & DIREC) == FORW) {
	s = fscan(f, n, lstchar);
	if ((ls & TYPE) == T) {
	    if (s == TRUE)
		s = backchar(FALSE, 1);
	    lstscan |= T;
	}
	return (s);
    } else {
	s = bscan(f, n, lstchar);
	if ((ls & TYPE) == T) {
	    if (s == TRUE)
		s = forwchar(FALSE, 1);
	    lstscan |= T;
	}
	return (s);
    }
}

/* , */
int
rev_csrch(int f, int n)
{
    int s;

    lstscan ^= DIREC;
    s = rep_csrch(f, n);
    lstscan ^= DIREC;
    return (s);
}
