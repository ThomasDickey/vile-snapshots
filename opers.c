/*
 * This file contains the command processing functions for the commands
 * that take motion operators.
 * written for vile: Copyright (c) 1990, 1995 by Paul Fox
 *
 * $Header: /users/source/archives/vile.vcs/RCS/opers.c,v 1.57 1996/12/09 01:07:17 tom Exp $
 *
 */

#include	"estruct.h"
#include        "edef.h"
#include        "nefunc.h"

extern REGION *haveregion;

/* For the "operator" commands -- the following command is a motion, or
 *  the operator itself is repeated.  All operate on regions.
 */
int
operator(int f, int n, OpsFunc fn, const char *str)
{
	int c;
	int thiskey;
	int status;
	const CMDFUNC *cfp;		/* function to execute */
	char tok[NSTRING];		/* command incoming */
	BUFFER *ourbp;
#if OPT_MOUSE
	WINDOW	*wp0 = curwp;
#endif

	doingopcmd = TRUE;

	pre_op_dot = DOT;
	ourbp = curbp;

	if (havemotion != NULL) {
		cfp = havemotion;
		havemotion = NULL;
	} else {
		mlwrite("%s operation pending...",str);
		(void)update(FALSE);

		/* get the next command from the keyboard */
		/* or a command line, as approp. */
		if (clexec) {
			macarg(tok);	/* get the next token */
			if (!strcmp(tok,"lines"))
				cfp = &f_godotplus;
			else
				cfp = engl2fnc(tok);
		} else {
			thiskey = lastkey;
			c = kbd_seq();

#if OPT_MOUSE
			if (curwp != wp0) {
				unkeystroke(c);
			    	doingopcmd = FALSE;
				return FALSE;
			}
#endif
			/* allow second chance for entering counts */
			do_repeats(&c,&f,&n);

			if (thiskey == lastkey)
				cfp = &f_godotplus;
			else
				cfp = kcod2fnc(c);

		}
		if (cfp)
			mlerase();
		else {
			if (!clexec)
				lsprintf(tok, "(%d)", c);
			no_such_function(tok);
		}
	}
	if (!cfp) {
		doingopcmd = FALSE;
		return FALSE;
	}

	if ((cfp->c_flags & MOTION) == 0) {
		kbd_alarm();
		doingopcmd = FALSE;
		return(ABORT);
	}

	/* motion is interpreted as affecting full lines */
	if (regionshape == EXACT) {
	    if (cfp->c_flags & FL)
		    regionshape = FULLLINE;
	    if (cfp->c_flags & RECT)
		    regionshape = RECTANGLE;
	}

	/* and execute the motion */
	status = execute(cfp, f,n);

	if (status != TRUE) {
		doingopcmd = FALSE;
		regionshape = EXACT;
		mlforce("[Motion failed]");
		return FALSE;
	}

	opcmd = 0;

	MK = pre_op_dot;

	/* we've successfully set up a region */
	if (!fn) { /* be defensive */
		mlforce("BUG -- null func pointer in operator");
		status = FALSE;
	} else {
		status = (fn)();
	}

	if (ourbp == curbp) /* in case the func switched buffers on us */
		swapmark();

	if (regionshape == FULLLINE)
		(void)firstnonwhite(FALSE,1);

	regionshape = EXACT;

	doingopcmd = FALSE;

	haveregion = FALSE;

	return status;
}

int
operdel(int f, int n)
{
	int	status;

	opcmd = OPDEL;
	lines_deleted = 0;
	status = operator(f, n, killregion,
		regionshape == FULLLINE
			? "Delete of full lines"
			: "Delete");
	if (do_report(lines_deleted))
		mlforce("[%d lines deleted]", lines_deleted);
	return status;
}

int
operlinedel(int f, int n)
{
	regionshape = FULLLINE;
	return operdel(f,n);
}

static int
chgreg(void)
{
	if (regionshape == RECTANGLE) {
		return stringrect();
	} else {
		killregion();
		if (regionshape == FULLLINE) {
			if (backline(FALSE,1) == TRUE) 
				/* backline returns FALSE at top of buf */
				return opendown(TRUE,1);
			else
				return openup(TRUE,1);
		}
		return ins();
	}
}

int
operchg(int f, int n)
{
	int s;

	opcmd = OPOTHER;
	s = operator(f,n,chgreg,"Change");
	if (s == TRUE) swapmark();
	return s;
}

int
operlinechg(int f, int n)
{
	int s;

	regionshape = FULLLINE;
	opcmd = OPOTHER;
	s = operator(f,n,chgreg,"Change of full lines");
	if (s == TRUE) swapmark();
	return s;
}

int
operjoin(int f, int n)
{
	opcmd = OPOTHER;
	return operator(f,n,joinregion,"Join");
}

int
operyank(int f, int n)
{
	MARK savedot;
	int s;
	savedot = DOT;
	opcmd = OPDEL;
	s = operator(f,n,yankregion,"Yank");
	DOT = savedot;
	return s;
}

int
operlineyank(int f, int n)
{
	MARK savedot;
	int s;
	savedot = DOT;
	regionshape = FULLLINE;
	opcmd = OPOTHER;
	s = operator(f,n,yankregion,"Yank of full lines");
	DOT = savedot;
	return s;
}

int
operflip(int f, int n)
{
	opcmd = OPOTHER;
	return operator(f,n,flipregion,"Flip case");
}

int
operupper(int f, int n)
{
	opcmd = OPOTHER;
	return operator(f,n,upperregion,"Upper case");
}

int
operlower(int f, int n)
{
	opcmd = OPOTHER;
	return operator(f,n,lowerregion,"Lower case");
}

/*
 * The shift commands are special, because vi allows an implicit repeat-count
 * to be specified by repeating the '<' or '>' operators.
 */
static int
shift_n_times(int f, int n, OpsFunc func, char *msg)
{
	register int status = FALSE;

	regionshape = FULLLINE;
	opcmd = OPOTHER;

	if (havemotion != NULL) {
		const CMDFUNC *cfp = havemotion;
		while (n-- > 0) {
			havemotion = cfp;
			if ((status = operator(FALSE,1, func, msg)) != TRUE)
				break;
		}
	} else
		status = operator(f,n, func, msg);
	return status;
}

int
operlshift(int f, int n)
{
	return shift_n_times(f,n,shiftlregion,"Left shift");
}

int
operrshift(int f, int n)
{
	return shift_n_times(f,n,shiftrregion,"Right shift");
}

int
operwrite(int f, int n)
{
        register int    s;
        char fname[NFILEN];

	if (ukb != 0) {
	        if ((s=mlreply_file("Write to file: ", (TBUFF **)0,
				FILEC_WRITE|FILEC_PROMPT, fname)) != TRUE)
	                return s;
		return kwrite(fname,TRUE);
	} else {
		opcmd = OPOTHER;
		return operator(f,n,writeregion,"File write");
	}
}

#if OPT_FORMAT
int
operformat(int f, int n)
{
	regionshape = FULLLINE;
	opcmd = OPOTHER;
	return operator(f,n,formatregion,"Format");
}
#endif

int
operfilter(int f, int n)
{
	regionshape = FULLLINE;
	opcmd = OPOTHER;
	return operator(f,n,filterregion,"Filter");
}


int
operprint(int f, int n)
{
	regionshape = FULLLINE;
	opcmd = OPOTHER;
	return operator(f,n,plineregion,"Line print");
}

int
operpprint(int f, int n)
{
	regionshape = FULLLINE;
	opcmd = OPOTHER;
	return operator(f,n,pplineregion,"Line-number print");
}

int
operlist(int f, int n)
{
	regionshape = FULLLINE;
	opcmd = OPOTHER;
	return operator(f,n,llineregion,"Line list");
}

int
opersubst(int f, int n)
{
	regionshape = FULLLINE;
	opcmd = OPOTHER;
	return operator(f,n,substregion,"Substitute");
}

int
opersubstagain(int f, int n)
{
	regionshape = FULLLINE;
	opcmd = OPOTHER;
	return operator(f,n,subst_again_region,"Substitute-again");
}

#if OPT_AEDIT
int
operentab(int f, int n)
{
	regionshape = FULLLINE;
	opcmd = OPOTHER;
	return operator(f,n,entab_region,"Spaces-->Tabs");
}
#endif

#if OPT_AEDIT
int
operdetab(int f, int n)
{
	regionshape = FULLLINE;
	opcmd = OPOTHER;
	return operator(f,n,detab_region,"Tabs-->Spaces");
}
#endif

#if OPT_AEDIT
int
opertrim(int f, int n)
{
	regionshape = FULLLINE;
	opcmd = OPOTHER;
	return operator(f,n,trim_region,"Trim whitespace");
}
#endif

#if OPT_AEDIT
int
operblank(int f, int n)
{
	opcmd = OPOTHER;
	return operator(f,n,blank_region,"Blanking");
}
#endif

int
operopenrect(int f, int n)
{
	opcmd = OPOTHER;
	regionshape = RECTANGLE;
	return operator(f,n,openregion,"Opening");
}

