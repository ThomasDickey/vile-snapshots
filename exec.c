/*	This file is for functions dealing with execution of
 *	commands, command lines, buffers, files and startup files
 *
 *	original by Daniel Lawrence, but
 *	much modified since then.  assign no blame to him.  -pgf
 *
 * $Header: /users/source/archives/vile.vcs/RCS/exec.c,v 1.206 1999/09/22 21:42:54 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"
#include	"nefunc.h"
#include	"nefsms.h"

#define isSPorTAB(c) ((c) == ' ' || (c) == '\t')

static	int	rangespec(const char *specp, LINEPTR *fromlinep, LINEPTR *tolinep, CMDFLAGS *flagp);

/* while loops are described by a list of small structs.  these point
 * at the line on which they were found, and at the line to which one
 * might want to go from there, i.e.:
 *
 *    |-> while ---------
 *    |     .           |
 *    |     .           |
 *    |     .           |
 *    |     break ----  |
 *    |     .        |  |
 *    |     .        |  |
 *    --- endwhile <--<-|
 *
 * the list of structs is created in a pre-processing pass in
 * setup_dobuf() and then used in perform_dobuf().
 */

typedef struct WHLOOP {
	LINEPTR	w_here;	    /* line containing this directive */
	LINEPTR	w_there;    /* if endwhile, ptr to while, else to endwhile */
	DIRECTIVE w_type;   /* block type: D_WHILE, D_BREAK, D_ENDWHILE */
	struct WHLOOP *w_next;
} WHLOOP;

static int token_ended_line;  /* did the last token end at end of line? */

#if !SMALLER
typedef struct locals {
	struct locals *next;
	char *name;
	char *value;
	} LOCALS;

typedef struct {
	int disabled;		/* nonzero to disable execution */
	int level;		/* ~if ... ~endif nesting level */
	int fired;		/* at top-level, ~if/... used */
	REGIONSHAPE shape;
	const CMDFUNC *motion;
	LOCALS *locals;
	TBUFF *prefix;
	} IFSTK;
#else
typedef struct {
	int disabled;		/* nonzero to disable execution */
	} IFSTK;		/* just a dummy variable to simplify ifdef's */
#endif

static IFSTK ifstk;
static TBUFF *line_prefix;

/* while storing away a macro, this is where it goes.
 * if NULL, we're not storing a macro.  */
static BUFFER *macrobuffer = NULL;

/*----------------------------------------------------------------------------*/

/*ARGSUSED*/
static int
eol_range(const char * buffer, unsigned cpos, int c, int eolchar GCC_UNUSED)
{
	if (is_edit_char(c))
		return FALSE;

	if (isSpecial(c)	/* sorry, cannot scroll with arrow keys */
	 || isCntrl(c))
		return TRUE;

	if (islinespecchar(c)
	 || (c == ':' && (cpos == 0 || buffer[cpos-1] == c))
	 || /* special test for 'a style mark references */
		(cpos != 0
		&& buffer[cpos-1] == '\''
		&& (isLower(c) || (c == '\'') ) ) )
		return FALSE;
	return TRUE;
}

/*
 * Returns true iff the user ended the last prompt with a carriage-return.
 */
static int
end_of_cmd(void)
{
	register int c = end_string();
	return isreturn(c);
}

/* returns true iff we are in a named-command and if the user ended the last
 * prompt with a carriage-return.
 */
int
end_named_cmd(void)
{
	if (isnamedcmd) {
		if (clexec)
			return token_ended_line;
		else
			return end_of_cmd();
	}
	return FALSE;
}

/* returns true iff we are in a named-command and if the user did not end the
 * last prompt with a carriage-return.
 */
int
more_named_cmd(void)
{
	if (isnamedcmd) {
		if (clexec)
			return !token_ended_line;
		else
			return !end_of_cmd();
	}
	return FALSE;
}

/* namedcmd:	execute a named command even if it is not bound */
#if SMALLER
#define execute_named_command namedcmd
#else
static	/* next procedure is local */
#endif

int
execute_named_command(int f, int n)
{
	register int	status;
	register const CMDFUNC *cfp; /* function to execute */
	register char *fnp;	/* ptr to the name of the cmd to exec */

	LINEPTR fromline;	/* first linespec */
	LINEPTR toline;		/* second linespec */
	MARK	save_DOT;
	MARK	last_DOT;
	static TBUFF *lspec;	/* text of line spec */
	char cspec[NLINE];	/* text of command spec */
	int cmode = 0;
	int c;
	char repeat_cmd = EOS;
	int maybe_reg, maybe_num;
	CMDFLAGS lflag, flags;

	tb_scopy(&lspec, "");
	last_DOT = DOT;

	/* prompt the user to type a named command */
	mlprompt(": ");

	/* and now get the function name to execute */
	for_ever {
		if (cmode == 0) {	/* looking for range-spec, if any */
			status = kbd_reply(
				(char *)0,	/* no-prompt => splice */
				&lspec,		/* in/out buffer */
				eol_range,
				EOS,		/* may be a conflict */
				0,		/* no expansion, etc. */
				no_completion);
			c = end_string();
			if (status != TRUE && status != FALSE) {
				if (status == SORTOFTRUE) {
					(void)keystroke(); /* cancel ungetc */
					return FALSE;
				}
				return status;
			} else if (isreturn(c) && (status == FALSE)) {
				return FALSE;
			} else {
				unkeystroke(c);	/* ...so we can splice */
			}
			cmode = 1;
			repeat_cmd = EOS;
		} else {			/* looking for command-name */
			fnp = NULL;
			status = kbd_engl_stat((char *)0, cspec, KBD_STATED);
			if (status == TRUE) {
				fnp = cspec;
#if !SMALLER
				if (isRepeatable(*fnp) && clexec) {
					repeat_cmd = *fnp;
					cmode++;
					hst_glue(EOS);
				} else
#endif
				 if (isRepeatable(*fnp) && (*fnp == end_string())) {
					unkeystroke(*fnp);
					repeat_cmd = *fnp;
					cmode++;
					hst_glue(EOS);
				} else {
					c = fnp[strlen(fnp)-1];
					if (isPunct(c)) {
						c = end_string();
						if (c != NAMEC
						 && c != ' '
						 && !isreturn(c)) {
							unkeystroke(c);
							/* e.g., !-command */
						 }
					} else {	/* e.g., ":e#" */
						c = end_string();
						if (isPunct(c)
						 && strchr(global_g_val_ptr(GVAL_EXPAND_CHARS),c) != 0)
							unkeystroke(c);
					}
					break;
				}
			} else if (status == SORTOFTRUE) {
				hst_remove((cmode > 1) ? "*" : "");
				if (cmode > 1)
					(void)keystroke(); /* eat the delete */
				if (--cmode <= 1) {
					cmode = 0;
					hst_remove(tb_values(lspec));
				}
			} else if ((status == ABORT) || (lastkey == killc)) {
				return status;
			} else {	/* status == FALSE (killc/wkillc) */
				if (cmode > 1) {
					fnp = cspec;
					cmode--;
					break;
				} else {
					if (tb_length(lspec) <= 1) {
						return status;
					} else {
						break;	/* range-only */
					}
				}
			}
		}
	}

	/* reconstruct command if user edited it */
	if (repeat_cmd != EOS && fnp != 0 && *fnp == EOS) {
		fnp[0] = repeat_cmd;
		fnp[1] = EOS;
	}

	/* infer repeat count from repeated-command */
	if (cmode > 1) {
		if (!f) {
			f = TRUE;
			n = cmode;
		} else {
			mlforce("[Redundant repeat-count]");
			return FALSE;
		}
	}

	/* parse the accumulated lspec */
	if (rangespec(tb_values(lspec), &fromline, &toline, &lflag) != TRUE) {
		mlforce("[Improper line range]");
		return FALSE;
	}

	/* if range given, and it wasn't "0" and the buffer's empty */
	if (!(lflag & (DFLALL|ZERO)) && is_empty_buf(curbp)) {
		mlforce("[No range possible in empty buffer '%s']", fnp);
		return FALSE;
	}

	/* did we get a name? */
	if (fnp == NULL) {
		if ((lflag & DFLALL)) { /* no range, no function */
			return FALSE;
		} else { /* range, no function */
			cfp = &f_gomark;
			fnp = "";
		}
	} else if ((cfp = engl2fnc(fnp)) == NULL) { /* bad function */
		return no_such_function(fnp);
	}

	ev_end_of_cmd = end_of_cmd();
	flags = cfp->c_flags;

	/* bad arguments? */
#ifdef EXRC_FILES
seems like we need one more check here -- is it from a .exrc file?
	    cmd not ok in .exrc			empty file
	if (!(flags & EXRCOK) && is_empty_buf(curbp)) {
		mlforce("[Can't use the \"%s\" command in a %s file.]",
					cmdnames[cmdidx].name, EXRC);
		return FALSE;
	}
#endif

	/* was: if (!(flags & (ZERO | EXRCOK)) && fromline == NULL ) */
	if ((lflag & ZERO)) {
		if (!(flags & ZERO)) {
			mlforce("[Can't use address 0 with \"%s\" command]", fnp);
			return FALSE;
		}
		if (fromline == null_ptr)
			fromline = buf_head(curbp); /* buffer is empty */

		/*  we're positioned at fromline == curbp->b_linep, so commands
			must be willing to go _down_ from there.  Seems easiest
			to special case the commands that prefer going up */
		if (cfp == &f_insfile) {
			/* EMPTY */ /* works okay, or acts down normally */ ;
		} else if (cfp == &f_lineputafter) {
			cfp = &f_lineputbefore;
			fromline = lforw(fromline);
		} else if (cfp == &f_opendown) {
			cfp = &f_openup;
			fromline = lforw(fromline);
		} else if (cfp == &f_gomark) {
			fromline = lforw(fromline);
		} else {
			mlforce("[Configuration error: ZERO]");
			return FALSE;
		}
		flags = cfp->c_flags;
		toline = fromline;
	}

	/* if we're not supposed to have a line no., and the line no. isn't
		the current line, and there's more than one line */
	if (!(flags & FROM) && (fromline != DOT.l) &&
			!is_empty_buf(curbp) &&
		  (lforw(lforw(buf_head(curbp))) != buf_head(curbp)) ) {
		mlforce("[Can't use address with \"%s\" command.]", fnp);
		return FALSE;
	}
	/* if we're not supposed to have a second line no., and the line no.
		isn't the same as the first line no., and there's more than
		one line */
	if (!(flags & TO) && (toline != fromline) &&
			!is_empty_buf(curbp) &&
		  (lforw(lforw(buf_head(curbp))) != buf_head(curbp)) ) {
		mlforce("[Can't use a range with \"%s\" command.]", fnp);
		return FALSE;
	}

	/* some commands have special default ranges */
	if (lflag & DFLALL) {
		if (flags & DFLALL) {
			if (cfp == &f_showlength) {
				fromline = lforw(buf_head(curbp));
				toline   = lback(buf_head(curbp));
			} else if (cfp == &f_operwrite) {
				cfp = &f_filewrite;
			} else if (cfp == &f_operglobals) {
				cfp = &f_globals;
			} else if (cfp == &f_opervglobals) {
				cfp = &f_vglobals;
			} else {
				mlforce("[Configuration error: DFLALL]");
				return FALSE;
			}
			lflag |= (FROM|TO); /* avoid prompt for line-count */
		} else if (flags & DFLNONE) {
#if OPT_SHELL
			if (cfp == &f_operfilter) {
				cfp = &f_spawn;
				(void)setmark();  /* not that it matters */
			} else
#endif
#if OPT_PERL
			if (cfp == &f_perl) {
				perl_default_region();
			} else
#endif
			{
				mlforce("[Configuration error: DFLNONE]");
				return FALSE;
			}
			fromline = toline = null_ptr;
			lflag |= (FROM|TO); /* avoid prompt for line-count */
		} else
			lflag &= ~DFLALL;
	}

	/* Process argument(s) for named-buffer & line-count, if present.  The
	 * line-count is expected only if no "to" address specification was
	 * given.
	 */
	if ((*fnp != EOS) && !end_of_cmd() && !isPunct(end_string())) {
		maybe_reg = ((flags & OPTREG) == OPTREG);
		maybe_num = ((flags & TO) == TO)
			&& !((lflag & TO) == TO);
		if (maybe_num && (lflag & FROM)) {
			toline = fromline;
			maybe_num = FALSE;
		}
	} else {
		maybe_reg =
		maybe_num = FALSE;
	}

	if (maybe_reg || maybe_num) {
		int	num,
			that = (maybe_num && maybe_reg)
				? 0
				: (maybe_num ? 1 : -1),
			last = maybe_num ? 2 : 1;

		while (!end_of_cmd() && (that < last)) {
			status = mlreply_reg_count(that, &num, &that);
			if (status == ABORT)
				return status;
			else if (status != TRUE)
				break;
			if (that == 2) {
				if (is_empty_buf(curbp)) {
				    mlforce(
			    "[No line count possible in empty buffer '%s']", fnp);
				    return FALSE;
				}
				swapmark();
				DOT.l = fromline;
				(void)forwline(TRUE, num-1);
				toline = DOT.l;
				swapmark();
			} else {
				ukb = (short)num;
				kregflag = (that == 1) ? KAPPEND : 0;
				that = 1;
				/* patch: need to handle recursion */
			}
		}
	}

	if (flags & NOMOVE)
		save_DOT = DOT;
	else if ((toline != null_ptr) || (fromline != null_ptr)) {
		/* assume it's an absolute motion */
		/* we could probably do better */
		curwp->w_lastdot = DOT;
	}
	if ((toline != null_ptr) && (flags & TO)) {
		DOT.l = toline;
		(void)firstnonwhite(FALSE,1);
		(void)setmark();
	}
	if ((fromline != null_ptr) && (flags & FROM)) {
		DOT.l = fromline;
		(void)firstnonwhite(FALSE,1);
		if (toline == null_ptr)
			(void)setmark();
	}
	if (!sameline(last_DOT, DOT))
		curwp->w_flag |= WFMOVE;

	/* and then execute the command */
	isnamedcmd = TRUE;
	havemotion = &f_gomark;
	regionshape = FULLLINE;

	/* if the command ended with a bang, let the function know
		that so it can take special action */
	if ((flags & BANG) && (fnp[strlen(fnp)-1] == '!')) {
		f = TRUE;
		n = SPECIAL_BANG_ARG;
	}

	/*
	 * Ensure that we've completed the command properly.  (We could make
	 * the same check after 'execute()', but that would tend to obscure
	 * informational/warning messages...).
	 */
	if (flags == NONE
	 && end_string() != ' '
	 && !end_of_cmd()) {
		status = FALSE;
		if (!mapped_c_avail())
			unkeystroke(end_string());
		c = kbd_seq();
		mlforce("[Extra characters after \"%s\" command]", fnp);
	} else {
		status = execute(cfp,f,n);
	}

	havemotion = NULL;
	isnamedcmd = FALSE;
	regionshape = EXACT;

	/* don't use this if the command modifies! */
	if (flags & NOMOVE)
		restore_dot(save_DOT);

	return status;
}

#if !SMALLER
/* intercept calls on 'namedcmd()' to allow logging of all commands, even
 * those that have errors in them.
 */
int
namedcmd(int f, int n)
{
	int status;
	hst_init(EOS);
	status = execute_named_command(f,n);
	hst_flush();
	return status;
}
#endif

/* parse an ex-style line spec -- code culled from elvis, file ex.c, by
	Steve Kirkendall
*/
static const char *
parse_linespec(
register const char *s,		/* start of the line specifier */
LINEPTR		*markptr)	/* where to store the mark's value */
{
	int		num;
	LINE		*lp;	/* where the linespec takes us */
	register const char *t;
	int		status;

	(void)setmark();
	lp = NULL;

	/* parse each ;-delimited clause of this linespec */
	do {
		/* skip an initial ';', if any */
		if (*s == ';')
			s++;

		/* skip leading spaces */
		while (isSPorTAB(*s))
			s++;

		/* dot means current position */
		if (*s == '.') {
			s++;
			lp = DOT.l;
		} else if (*s == '$') { /* '$' means the last line */
			s++;
			status = gotoeob(TRUE,1);
			if (status) lp = DOT.l;
		} else if (isDigit(*s)) {
			/* digit means an absolute line number */
			for (num = 0; isDigit(*s); s++) {
				num = num * 10 + *s - '0';
			}
			status = gotoline(TRUE,num);
			if (status) lp = DOT.l;
		} else if (*s == '\'') {
			/* apostrophe means go to a set mark */
			s++;
			status = gonmmark(*s);
			if (status) lp = DOT.l;
			s++;
		} else if (*s == '+') {
			s++;
			for (num = 0; isDigit(*s); s++)
				num = num * 10 + *s - '0';
			if (num == 0)
				num++;
			while (*s == '+')
				s++, num++;
			status = forwline(TRUE,num);
			if (status)
				lp = DOT.l;
		} else if (*s == '-') {
			s++;
			for (num = 0; isDigit(*s); s++)
					num = num * 10 + *s - '0';
			if (num == 0)
				num++;
			while (*s == '-')
				s++, num++;
			status = forwline(TRUE,-num);
			if (status)
				lp = DOT.l;
		}
#if PATTERNS
		else if (*s == '/' || *s == '?') { /* slash means do a search */
			/* put a null at the end of the search pattern */
			t = parseptrn(s);

			/* search for the pattern */
			lp &= ~(BLKSIZE - 1);
			if (*s == '/') {
				pfetch(markline(lp));
				if (plen > 0)
					lp += plen - 1;
				lp = m_fsrch(lp, s);
			} else {
				lp = m_bsrch(lp, s);
			}

			/* adjust command string pointer */
			s = t;
		}
#endif
		else if (*s == EOS) {	/* empty string matches '.' */
			lp = DOT.l;
		}

		/* if linespec was faulty, quit now */
		if (!lp) {
			*markptr = lp;
			swapmark();
			return s;
		}

		/* maybe add an offset */
		t = s;
		if (*t == '-' || *t == '+') {
			s++;
			for (num = 0; isDigit(*s); s++) {
				num = num * 10 + *s - '0';
			}
			if (num == 0)
				num = 1;
			if (forwline(TRUE, (*t == '+') ? num : -num) == TRUE)
				lp = DOT.l;
		}
	} while (*s == ';' || *s == '+' || *s == '-');

	*markptr = lp;
	swapmark();
	return s;
}

/* parse an ex-style line range -- code culled from elvis, file ex.c, by
	Steve Kirkendall
*/
static int
rangespec(
const char	*specp,		/* string containing a line range */
LINEPTR		*fromlinep,	/* first linespec */
LINEPTR		*tolinep,	/* second linespec */
CMDFLAGS	*flagp)
{
	register const char *scan;	/* used to scan thru specp */
	LINEPTR		fromline;	/* first linespec */
	LINEPTR		toline;		/* second linespec */

	*flagp = 0;

	/* ignore command lines that start with a double-quote */
	if (*specp == '"') {
		*fromlinep = *tolinep = DOT.l;
		return TRUE;
	}

	/* permit extra colons at the start of the line */
	while (isSPorTAB(*specp) || *specp == ':') {
		specp++;
	}

	/* parse the line specifier */
	scan = specp;
	if (*scan == '0') {
		fromline = toline = buf_head(curbp); /* _very_ top of buffer */
		*flagp |= (FROM|ZERO);
		scan++;
	} else if (*scan == '%') {
		/* '%' means all lines */
		fromline = lforw(buf_head(curbp));
		toline = lback(buf_head(curbp));
		scan++;
		*flagp |= (FROM|TO);
	} else {
		scan = parse_linespec(scan, &toline);
		*flagp |= FROM;
		if (toline == null_ptr)
			toline = DOT.l;
		fromline = toline;
		while (*scan == ',') {
			fromline = toline;
			scan++;
			scan = parse_linespec(scan, &toline);
			*flagp |= TO;
			if (toline == null_ptr) {
				/* faulty line spec */
				return FALSE;
			}
		}
	}

	if (is_empty_buf(curbp))
		fromline = toline = null_ptr;

	if (scan == specp)
		*flagp |= DFLALL;

	/* skip whitespace */
	while (isSPorTAB(*scan))
		scan++;

	if (*scan) {
		/* dbgwrite("crud at end %s (%s)",specp, scan); */
		return FALSE;
	}

	*fromlinep = fromline;
	*tolinep = toline;

	return TRUE;
}

/*	docmd:	take a passed string as a command line and translate
		it to be executed as a command. This function will be
		used by execute-command-line and by all source and
		startup files.

	format of the command line is:

		{# arg} <command-name> {<argument string(s)>}

*/

int
docmd(
char *cline,	/* command line to execute */
int execflag,
int f, int n)
{
	int status = TRUE;	/* return status of function */
	int oldcle;		/* old contents of clexec flag */
	char *oldestr;		/* original exec string */
	TBUFF *tok = 0;
	char *token;		/* next token off of command line */
	const CMDFUNC *cfp;

	set_end_string(EOS);
	oldestr = execstr;	/* save last ptr to string to execute */
	execstr = cline;	/* and set this one as current */

	do {
		if ((token = mac_tokval(&tok)) == 0) { /* grab first token */
			execstr = oldestr;
			return FALSE;
		}
		if (*token == ':') {	/* allow leading ':' on line */
			register int j;
			for (j = 0; (token[j] = token[j+1]) != EOS; j++)
				;
		}
	} while (!*token);

	/* if it doesn't look like a command (and command names can't
	 * be hidden in variables), then it must be a leading argument */
	if (toktyp(token) != TOK_LITSTR || isDigit(token[0])) {
		f = TRUE;
		n = strtol(tokval(token),0,0);

		/* and now get the command to execute */
		if ((token = mac_tokval(&tok)) == 0) {
			execstr = oldestr;
			return FALSE;
		}
	}

	/* and match the token to see if it exists */
	if ((cfp = engl2fnc(token)) == NULL) {
		status = no_such_function(token);
	} else {
		/* save the arguments and go execute the command */
		oldcle = clexec;		/* save old clexec flag */
		clexec = execflag;		/* in cline execution */
		/*
		 * Flag the first time through for some commands -- e.g.  subst
		 * must know to not prompt for strings again, and pregion must
		 * only restart the p-lines buffer once for each command.
		 */
		calledbefore = FALSE;
		status = execute(cfp,f,n);
		setcmdstatus(status);
		clexec = oldcle;
	}

	execstr = oldestr;
	tb_free(&tok);
	return status;
}

/*
 *  Call the appropriate action for a given CMDFUNC
 */
int
call_cmdfunc(const CMDFUNC *p, int f, int n)
{
    switch (p->c_flags & CMD_TYPE)
    {
    case CMD_FUNC: /* normal CmdFunc */
	return (CMD_U_FUNC(p))(f, n);

#if OPT_NAMEBST
#if OPT_PROCEDURES
    case CMD_PROC: /* named procedure */
	return dobuf(CMD_U_BUFF(p));
#endif

#if OPT_PERL
    case CMD_PERL: /* perl subroutine */
	return perl_call_sub(CMD_U_PERL(p), p->c_flags & OPER, f, n);
#endif
#endif /* OPT_NAMEBST */
    }

    mlforce("BUG: invalid CMDFUNC type");
    return FALSE;
}

/*
 * This is the general command execution routine. It takes care of checking
 * flags, globals, etc, to be sure we're not doing something dumb.
 * Return the status of command.
 */

int
execute(
const CMDFUNC *execfunc,	/* ptr to function to execute */
int f, int n)
{
	register int status;
	register CMDFLAGS flags;

	if (execfunc == NULL) {
#if OPT_REBIND
		mlwarn("[Key not bound]");	/* complain		*/
#else
		mlwarn("[Not a command]");	/* complain		*/
#endif
		return FALSE;
	}

	flags = execfunc->c_flags;

	/* commands following operators can't be redone or undone */
	if ( !doingopcmd && !doingsweep) {
		/* don't record non-redoable cmds, */
		/* but if we're in insertmode, it's okay, since we must
			be executing a function key, like an arrow key,
			that the user will want to have replayed later */
		if ((curwp == NULL || !insertmode) && (flags & REDO) == 0)
			dotcmdstop();
		if (flags & UNDO) {
			/* undoable command can't be permitted when read-only */
			if (!(flags & VIEWOK)) {
				if (b_val(curbp,MDVIEW))
					return rdonly();
#ifdef MDCHK_MODTIME
				if (!b_is_changed(curbp) &&
					!ask_shouldchange(curbp))
					return FALSE;
#endif
			}
			if (!kbd_replaying(FALSE))
				mayneedundo();
		}
	}

	if (dotcmdactive != PLAY) {
		if (execfunc != &f_dotcmdplay) {
			/* reset dotcmdkreg on any command where ukb is
			 * unspecified.  usekreg() does it on the one's
			 * where it is specified.  */
			if (ukb == 0)
			    dotcmdkreg = 0;

			/* override the saved dot-cmd argument, if this
				is a new redoable command */
			if (flags & REDO)
			    dotcmdarg = FALSE;
		}
	} else {
		/* if we _are_ playing, re-use the previously kreg */
		if (dotcmdkreg != 0)
			ukb = dotcmdkreg;
	}

	if (curwp->w_tentative_lastdot.l == null_ptr)
		curwp->w_tentative_lastdot = DOT;

	status = call_cmdfunc(execfunc, f, n);
	if ((flags & GOAL) == 0) { /* goal should not be retained */
		curgoal = -1;
	}

	/* if motion was absolute, and it wasn't just on behalf of an
		operator, and we moved, update the "last dot" mark */
	if ((flags & ABSM) && !doingopcmd &&
			!sameline(DOT, curwp->w_tentative_lastdot)) {
		curwp->w_lastdot = curwp->w_tentative_lastdot;
	}

	curwp->w_tentative_lastdot = DOT;


	return status;
}

/* token:	chop a token off a string
		return a pointer past the token
*/

char *
get_token(
char *src,		/* source string */
TBUFF **tok,		/* destination token string */
int eolchar)
{
	register int quotef = EOS; /* nonzero iff the current string quoted */
	register int c, i, d, chr;

	tb_init(tok, EOS);
	if (src == 0)
		return src;

	/* first scan past any whitespace in the source string */
	while (isSPorTAB(*src))
		++src;

	/* scan through the source string */
	while ((c = *src) != EOS) {
		/* process special characters */
		if (c == '\\') {
			src++;
			if (*src == EOS)
				break;
			switch (c = *src++) {
				case 'r':	chr = '\r';	break;
				case 'n':	chr = '\n';	break;
				case 't':	chr = '\t';	break;
				case 'b':	chr = '\b';	break;
				case 'f':	chr = '\f';	break;
				case 'a':	chr = '\a';	break;
				case 's':	chr = ' ';	break;
				case 'e':	chr = ESC;	break;

				case 'x':
				case 'X':
					i = 2; /* allow \xNN hex */
					c = 0;
					while (isAlnum(*src) && i--) {
						if (isDigit(*src)) {
							d = *src - '0';
						} else if (isLower(*src)) {
							d = *src - 'a' + 10;
						} else {
							d = *src - 'A' + 10;
						}
						if (d > 15)
							break;
						c = (c * 16) + d;
						src++;
					}
					chr = (char)c;
					break;

				default:
					if (c >= '0' && c <= '7') {
						i = 2; /* allow \NNN octal */
						c -= '0';
						while (isDigit(*src)
						  && *src < '8'
						  && i--) {
							c = (c * 8) + (*src++ - '0');
						}
					}
					chr = (char)c;
			}
		} else {
			/* check for the end of the token */
			if (quotef != EOS) {
				if (c == quotef) {
					src++;
					break;
				}
			} else {
				if (c == eolchar) {
					if (!isSPorTAB(c))
						src++;
					break;
				} else if (c == '"') {
					quotef = c;
					/* note that leading quote
						is included */
				} else if (isSPorTAB(c)) {
					break;
				}
			}

			chr = *src++;	/* record the character */
		}
		tb_append(tok, chr);
	}

	/* scan past any whitespace remaining in the source string */
	while (isSPorTAB(*src))
		++src;
	token_ended_line = isreturn(*src) || *src == EOS;

	tb_append(tok, EOS);
	return src;
}

/*
 * Convert the string 'src' into a string that we can read back with 'token()'.
 * If it is a shell-command, this will be a single-token.  Repeated shift
 * commands are multiple tokens.
 */
int
macroize(
TBUFF	**p,
TBUFF *	src,
int	skip)
{
	register int	c;
	char	*ref	= tb_values(src); /* FIXME */
	int	multi	= !isShellOrPipe(ref);	/* shift command? */
	int	count	= 0;

	if (tb_init(p, esc_c) != 0) {
		ALLOC_T	n, len = tb_length(src);
		char	*txt = tb_values(src);

		TRACE(("macroizing %s\n", tb_visible(src)))
		(void)tb_append(p, '"');
		for (n = skip; n < len; n++) {
			c = txt[n];
			if (multi && count++)
				(void)tb_sappend(p, "\" \"");
			if (c == '\\' || c == '"')
				(void)tb_append(p, '\\');
			(void)tb_append(p, c);
		}
		(void)tb_append(p, '"');
		TRACE(("macroized %s\n", tb_visible(*p)))
		return (tb_append(p, EOS) != 0);
	}
	return FALSE;
}

/* estimate maximum number of macro tokens, for allocating arrays */
unsigned
mac_tokens(void)
{
    const char *s = execstr;
    unsigned result = 0;

    while (s != 0 && *s != EOS) {
	if (isSpace(*s)) {
	    while(isSpace(*s))
		s++;
	} else if (*s != EOS) {
	    result++;
	    while(*s != EOS && !isSpace(*s))
		s++;
	}
    }
    return result;
}

/* fetch and isolate the next token from execstr */
int
mac_token(TBUFF **tok)
{
	int savcle;
	const char *oldstr = execstr;

	savcle = clexec;
	clexec = TRUE;

	/* grab token and advance past */
	execstr = get_token(execstr, tok, EOS);

	clexec = savcle;
	return (execstr != oldstr);
}

/* fetch and isolate and evaluate the next token from execstr */
char *
mac_tokval(	/* get a macro line argument */
TBUFF **tok)	/* buffer to place argument */
{
	if (mac_token(tok) != 0) {
		/* evaluate it */
		(void)tb_scopy(tok, tokval(tb_values(*tok)));
		return tb_values(*tok);
	}
	tb_free(tok);
	return 0;
}

int
mac_literalarg(	/* get a macro line argument */
TBUFF **tok)	/* buffer to place argument */
{
	/* grab everything on this line, literally */
	(void)tb_scopy(tok, execstr);
	execstr += strlen(execstr);
	token_ended_line = TRUE;
	return TRUE;
}

#if OPT_MACRO_ARGS
/*
 * Parameter info is given as a keyword, optionally followed by a prompt string.
 */
static int
decode_parameter_info(TBUFF *tok, PARAM_INFO *result)
{
	char name[NSTRING];
	char text[NSTRING];
	char *s = vl_strncpy(name, tb_values(tok), sizeof(name));

	if ((s = strchr(s, '=')) != 0) {
	    strcpy(text, tokval(s+1));
	    *s = EOS;
	} else {
	    text[0] = EOS;
	}
	if ((s = strchr(name, ':')) != 0)
	    *s++ = EOS;

	result->pi_type = choice_to_code(fsm_paramtypes_choices,
					 name, strlen(name));
	if (result->pi_type != PT_UNKNOWN) {
	    result->pi_text = *text ? strmalloc(text) : 0;
#if OPT_ENUM_MODES
	    if (s != 0)
		result->pi_choice = name_to_choices (s);
	    else
		result->pi_choice = 0;
#endif
	    return TRUE;
	}
	return FALSE;
}
#endif

static int
setup_macro_buffer(TBUFF *name, int flag)
{
#if OPT_MACRO_ARGS || OPT_ONLINEHELP
	static TBUFF *temp;
	unsigned limit = mac_tokens();
	unsigned count = 0;
#endif
#if OPT_ONLINEHELP
	TBUFF *helpstring = 0;		/* optional help string */
#endif
#if OPT_MAJORMODE
	VALARGS args;
#endif
	char bname[NBUFN];		/* name of buffer to use */
	BUFFER *bp;

	/* construct the macro buffer name */
	if (flag < 0)
	    (void)add_brackets(bname, tb_values(name));
	else
	    (void)lsprintf(bname, MACRO_N_BufName, flag);

	/* set up the new macro buffer */
	if ((bp = bfind(bname, BFINVS)) == NULL) {
		mlforce("[Cannot create procedure]");
		return FALSE;
	}

	/* and make sure it is empty */
	if (!bclear(bp))
		return FALSE;

	set_rdonly(bp, bp->b_fname, MDVIEW);

	/* save this into the list of : names */
#if OPT_NAMEBST
	if (flag < 0) {
	    CMDFUNC *cf = typecalloc(CMDFUNC);

	    if (!cf)
		return no_memory("registering procedure name");

#if CC_CANNOT_INIT_UNIONS
	    cf->c_union = (void *)bp;
#else
	    cf->cu.c_buff = bp;
#endif
	    cf->c_flags = UNDO|REDO|CMD_PROC|VIEWOK;

#if OPT_MACRO_ARGS || OPT_ONLINEHELP
	    if (limit != 0) {
		while (mac_token(&temp) == TRUE) {
		    switch(toktyp(tb_values(temp))) {
#if OPT_ONLINEHELP
		    case TOK_QUOTSTR:
			tb_copy(&helpstring, temp);
			break;
#endif
#if OPT_MACRO_ARGS
		    case TOK_LITSTR:
			if (cf->c_args == 0) {
			    cf->c_args = typeallocn(PARAM_INFO, limit + 1);
			    if (cf->c_args == 0)
				return no_memory("Allocating parameter info");
			}
			if (decode_parameter_info(temp, &(cf->c_args[count]))) {
			    cf->c_args[++count].pi_type = PT_UNKNOWN;
			    break;
			}
			/* FALLTHRU */
#endif
		    default:
			mlforce("[Unexpected token '%s']", tb_values(temp));
			return FALSE;
		    }
		}
	    }
#endif

#if OPT_ONLINEHELP
	    if (helpstring == 0)
		cf->c_help = "User-defined procedure";
	    else
		cf->c_help = strmalloc(tb_values(helpstring));
	    tb_free(&helpstring);
#endif

	    if (insert_namebst(tb_values(name), cf, FALSE) != TRUE)
		return FALSE;
	}
#endif /* OPT_NAMEBST */

#if OPT_MAJORMODE
	if (find_mode(bp, "vilemode", FALSE, &args) == TRUE) {
	    (void)set_mode_value(bp, "vilemode", FALSE, TRUE, FALSE, &args, (char*)0);
	}
#endif

	/* and set the macro store pointers to it */
	macrobuffer = bp;
	return TRUE;
}

/*	storemac:	Set up a macro buffer and flag to store all executed
			command lines there. 'n' is the macro number to use
 */

int
storemac(int f, int n)
{
	/* can't store macros interactively */
	if (!clexec)
	{
	    mlforce("[Cannot store macros interactively]");
	    return FALSE;
	}

	/* the numeric arg is our only way of distinguishing macros */
	if (!f) {
		mlforce("[No macro specified]");
		return FALSE;
	}

	/* range check the macro number */
	if (n < 1 || n > OPT_EXEC_MACROS) {
		mlforce("[Macro number out of range]");
		return FALSE;
	}

	return setup_macro_buffer((TBUFF *)0, n);
}

#if	OPT_PROCEDURES
/*	storeproc:	Set up a procedure buffer and flag to store all
			executed command lines there.  'n' is the macro number
			to use.
 */

int
storeproc(int f, int n)
{
	static TBUFF *name;		/* procedure name */
	register int status;		/* return status */

	/* if a number was given, then they must mean macro N */
	if (f) return storemac(f, n);

	/* can't store procedures interactively */
	if (!clexec) {
		mlforce("[Cannot store procedures interactively]");
		return FALSE;
	}

	/* get the name of the procedure */
	tb_scopy(&name, "");
	if ((status = kbd_reply("Procedure name: ", &name,
	    eol_history, ' ', KBD_NORMAL, no_completion)) != TRUE)
		return status;

	return setup_macro_buffer(name, -1);
}

static int
run_procedure(const char *name)
{
	register BUFFER *bp;		/* ptr to buffer to execute */
	register int status;		/* status return */
	char bufn[NBUFN];		/* name of buffer to execute */

	if (!*name)
		return FALSE;

	/* construct the buffer name */
	(void)add_brackets(bufn, name);

	/* find the pointer to that buffer */
	if ((bp = find_b_name(bufn)) == NULL) {
		return FALSE;
	}

	status = dobuf(bp);

	return status;
}

/*	execproc:	Execute a procedure				*/

int
execproc(int f, int n)
{
	static char name[NBUFN];
	int status;

	if ((status = mlreply("Execute procedure: ",
					name, sizeof(name))) != TRUE) {
		return status;
	}

	if (!f)
		n = 1;

	status = TRUE;
	while (status == TRUE && n--)
		status = run_procedure(name);

	return status;

}

#endif

#if ! SMALLER
/*	execbuf:	Execute the contents of a buffer of commands	*/

int
execbuf(int f, int n)
{
	register BUFFER *bp;		/* ptr to buffer to execute */
	register int status;		/* status return */
	static char bufn[NBUFN];	/* name of buffer to execute */

	if (!f)
		n = 1;

	/* find out what buffer the user wants to execute */
	if ((status = ask_for_bname("Execute buffer: ", bufn, sizeof(bufn))) != TRUE)
		return status;

	/* find the pointer to that buffer */
	if ((bp = find_b_name(bufn)) == NULL) {
		mlforce("[No such buffer \"%s\"]",bufn);
		return FALSE;
	}

	status = TRUE;
	/* and now execute it as asked */
	while (n-- > 0 && status == TRUE)
		status = dobuf(bp);

	return status;
}
#endif

/* free a list of while block pointers */
static void
free_all_whiles(WHLOOP *wp)	/* head of list */
{
	WHLOOP *tp;

	while (wp) {
	    tp = wp->w_next;
	    free((char *)wp);
	    wp = tp;
	}
}

#define DIRECTIVE_CHAR '~'

#if ! SMALLER
#define DDIR_FAILED     -1
#define DDIR_COMPLETE    0
#define DDIR_INCOMPLETE  1
#define DDIR_FORCE       2
#define DDIR_HIDDEN      3

/*
 * When we get a "~local", we save the variable's name and its value.  Save the
 * value only the first time we encounter the variable.  We will restore the
 * variables when exiting from the current buffer macro.
 *
 * FIXME:  It would be nice (but more complicated) to implement local variables
 * within while/endwhile blocks.
 */
static int
push_variable(char *name)
{
	LOCALS *p;

	TRACE(("push_variable: {%s}%s\n", name, execstr))

	switch (toktyp(name)) {
	case TOK_STATEVAR:
	case TOK_TEMPVAR:
		break;
	default:
		return FALSE;
	}

	/* Check if we've saved this before */
	for (p = ifstk.locals; p != 0; p = p->next) {
		if (!strcmp(name, p->name))
			return TRUE;
	}

	/* We've not saved it - do it now */
	p = typealloc(LOCALS);
	p->next = ifstk.locals;
	p->name = strmalloc(name);
	p->value = (char *)tokval(name);
	if (!strcmp(p->value, error_val)) {
		/* special case: so we can delete vars */
		if (toktyp(name) != TOK_TEMPVAR) {
			free(p->name);
			free((char *)p);
			return FALSE;
		}
	} else {
		p->value = strmalloc(p->value);
	}

	ifstk.locals = p;

	return TRUE;
}

/*
 * On exit from the buffer, restore variables declared local, in reverse order.
 */
static void
pop_variable(void)
{
	LOCALS *p = ifstk.locals;
	ifstk.locals = p->next;
	TRACE(("pop_variable(%s) %s\n", p->name, p->value))
	if (!strcmp(p->value, error_val)) {
		rmv_tempvar(p->name);
	} else {
		set_state_variable(p->name, p->value);
		free(p->value);
	}
	free(p->name);
	free((char *)p);
}

static void
push_buffer(IFSTK *save)
{
	static const IFSTK new_ifstk = {0,0,0,EXACT,0,0,0}; /* all 0's */

	*save  = ifstk;
	save->shape = regionshape;
	save->motion = havemotion;
	save->prefix = line_prefix;

	ifstk       = new_ifstk;
	havemotion  = NULL;
	regionshape = EXACT;
	line_prefix = 0;
}

static void
pop_buffer(IFSTK *save)
{
	while (ifstk.locals)
		pop_variable();

	tb_free(&line_prefix);

	ifstk       = *save;
	havemotion  = ifstk.motion;
	regionshape = ifstk.shape;
	line_prefix = ifstk.prefix;
}

static DIRECTIVE
dname_to_dirnum(const char *cmdp, size_t length)
{
	DIRECTIVE dirnum = D_UNKNOWN;
	if ((--length > 0)
	 && (*cmdp++ == DIRECTIVE_CHAR)) {
		size_t n;
		for (n = 0; n < length; n++) {
			if (!isAlnum(cmdp[n]))
				length = n;
		}
		dirnum = choice_to_code(fsm_directive_choices, cmdp, length);
	}

	return dirnum;
}

#define directive_name(code) choice_to_name(fsm_directive_choices, code)

static int
unbalanced_directive(DIRECTIVE dirnum)
{
	mlforce("[Unexpected directive: %s]", directive_name(dirnum));
	return DDIR_FAILED;
}

static int
navigate_while(LINEPTR *lpp, WHLOOP *whlist)
{
	WHLOOP *wht;

	/* find the while control block we're on.  do what it says.
	 *  (i.e. if we're at a break or while, we're sent to the
	 *  bottom, if we're at an endwhile, we're sent to the top.)
	 */
	for (wht = whlist; wht != 0; wht = wht->w_next) {
		if (wht->w_here == *lpp) {
			*lpp = wht->w_there;
			return TRUE;
		}
	}
	return FALSE;
}


static int
begin_directive(
	char **const cmdpp,
	DIRECTIVE dirnum,
	WHLOOP *whlist,
	BUFFER *bp,
	LINEPTR *lpp)
{
	int status = DDIR_COMPLETE; /* assume directive is self-contained */
	TBUFF *argtkn = 0;
	char *value;
	char *old_execstr = execstr;

	execstr = *cmdpp;

	switch (dirnum) {
	case D_LOCAL:
		while (mac_token(&argtkn) == TRUE) {
			value = tb_values(argtkn);
			if (push_variable(value) != TRUE) {
				mlforce("[cannot save '%s']", value);
				status = DDIR_FAILED;
				break;
			}
		}
		break;

	case D_WHILE:
		if (!ifstk.disabled) {
			if ((value = mac_tokval(&argtkn)) == 0) {
				status = DDIR_INCOMPLETE;
				break;
			} else if (scan_bool(value) != TRUE) {
				if (navigate_while(lpp, whlist) != TRUE)
					status = unbalanced_directive(dirnum);
			}
		}
		break;

	case D_BREAK:
		if (!ifstk.disabled) {
			if (navigate_while(lpp, whlist) != TRUE)
				status = unbalanced_directive(dirnum);
		}
		break;

	case D_ENDWHILE:
		if (!ifstk.disabled) {
			if (navigate_while(lpp, whlist) != TRUE)
				status = unbalanced_directive(dirnum);
			else
				*lpp = lback(*lpp);
		}
		break;


	case D_IF:
		ifstk.level++;
		if (!ifstk.disabled) {
			ifstk.fired = FALSE;
			ifstk.disabled = ifstk.level;
			if ((value = mac_tokval(&argtkn)) == 0)
				status = DDIR_INCOMPLETE;
			else if (scan_bool(value) == TRUE) {
				ifstk.disabled = 0;
				ifstk.fired = TRUE;
			}
		}
		break;

	case D_ELSEIF:
		if (ifstk.level == 0) {
			status = unbalanced_directive(dirnum);
		} else {
			if (ifstk.fired) {
				if (!ifstk.disabled)
					ifstk.disabled = ifstk.level;
			} else if ((value = mac_tokval(&argtkn)) == 0) {
				status = DDIR_INCOMPLETE;
			} else if (!ifstk.fired
			  && ifstk.disabled == ifstk.level
			  && (scan_bool(value) == TRUE)) {
				ifstk.disabled = 0;
				ifstk.fired = TRUE;
			}
		}
		break;

	case D_ELSE:
		if (ifstk.level == 0) {
			status = unbalanced_directive(dirnum);
		} else {
			if (ifstk.fired) {
				if (!ifstk.disabled)
					ifstk.disabled = ifstk.level;
			} else if (ifstk.disabled == ifstk.level) {
				ifstk.disabled = 0;
				ifstk.fired = TRUE;
			}
		}
		break;

	case D_ENDIF:
		if (ifstk.level == 0) {
			status = unbalanced_directive(dirnum);
		} else {
			ifstk.level--;
			if (ifstk.disabled > ifstk.level)
			{
				ifstk.disabled = 0;
				ifstk.fired = TRUE;
			}
		}
		break;

	case D_GOTO:
		if (!ifstk.disabled) {
			TBUFF *label = 0;
			LINEPTR glp;	/* line to goto */

			/* grab label to jump to */
			*cmdpp = (char *)get_token(*cmdpp, &label, EOS);
#if MAYBE
			/* i think a simple re-eval would get us variant
			 * targets, i.e. ~goto %somelabel.  */
			tb_scopy(label, tokval(tb_values(label)));
#endif
			value = tb_values(label);
			glp = label2lp(bp, value);
			if (glp == 0) {
				mlforce("[No such label \"%s\"]", value);
				status = DDIR_FAILED;
			} else {
				*lpp = glp;
			}
			tb_free(&label);
		}
		break;

	case D_RETURN:
		if (!ifstk.disabled)
			status = DDIR_INCOMPLETE;
		break;

	case D_FORCE:
		status = DDIR_FORCE;
		break;

	case D_HIDDEN:
		status = DDIR_HIDDEN;
		break;

	case D_ELSEWITH:
		if (!tb_length(line_prefix)) {
			status = unbalanced_directive(dirnum);
			break;
		}
		/* FALLTHRU */
	case D_WITH:
		while (isBlank(*execstr))
			execstr++;
		tb_scopy(&line_prefix, execstr);
		status = DDIR_COMPLETE;
		break;

	case D_ENDWITH:
		tb_free(&line_prefix);
		status = DDIR_COMPLETE;
		break;

	case D_UNKNOWN:
	case D_ENDM:
		break;
	}
	execstr = old_execstr;
	tb_free(&argtkn);
	return status;
}

static WHLOOP *
alloc_WHLOOP(WHLOOP *whp, DIRECTIVE dirnum, LINEPTR lp)
{
	WHLOOP *nwhp;

	if ((nwhp = typealloc(WHLOOP)) == 0) {
		mlforce("[Out of memory during '%s' scan]",
			directive_name(dirnum));
		free_all_whiles(whp);
		return 0;
	}
	nwhp->w_here = lp;
	nwhp->w_there = 0;
	nwhp->w_type = dirnum;
	nwhp->w_next = whp;
	return nwhp;
}

static int
setup_dobuf(BUFFER *bp, WHLOOP **result)
{
	int status = TRUE;
	LINEPTR lp;
	char *cmdp;
	WHLOOP *twhp, *whp = 0;
	DIRECTIVE dirnum;

	/* create the WHILE blocks */
	bp->b_dot.o = 0;
	*result = 0;
	for_each_line(lp, bp) {
		int i;

		bp->b_dot.l = lp;

		/* find the first printable character */
		cmdp = lp->l_text;
		i = llength(lp);
		while (i > 0 && isBlank(*cmdp)) {
			cmdp++;
			i--;
		}

		/* blank line */
		if (i <= 0)
			continue;

		dirnum = dname_to_dirnum(cmdp, (size_t)i);
		if (dirnum == D_WHILE ||
		    dirnum == D_BREAK ||
		    dirnum == D_ENDWHILE) {

			if ((whp = alloc_WHLOOP(whp, dirnum, lp)) == 0) {
				status = FALSE;
				break;
			}

			if (dirnum == D_ENDWHILE) {
				/* walk back, filling in unfilled
				 * 'there' pointers on the breaks,
				 * until we come to a while with an
				 * unfilled 'there'.  fill it in, and
				 * use its 'here' to fill in the
				 * 'there' pointer on the endwhile.
				 */
				 twhp = whp->w_next;
				 while (twhp) {
					if (!twhp->w_there) {
						twhp->w_there = whp->w_here;
						if (twhp->w_type == D_WHILE) {
							whp->w_there =
								twhp->w_here;
							break;
						}
					}
					twhp = twhp->w_next;
				 }
				 if (!twhp) {
					/* no while for an endwhile */
					mlforce(
					    "[%s doesn't follow %s in '%s']",
						directive_name(D_ENDWHILE),
						directive_name(D_WHILE),
						bp->b_bname);
					status = FALSE;
					break;
				}
			}

		}
	}

	/* no endwhile for a while or break */
	if (status == TRUE && whp && whp->w_type != D_ENDWHILE) {
		mlforce("[%s with no matching %s in '%s']",
			directive_name(D_WHILE),
			directive_name(D_ENDWHILE),
			bp->b_bname);
		status = FALSE;
	}

	*result = whp;
	return status;
}
#else
#define dname_to_dirnum(cmdp,length) \
		(cmdp[0] == DIRECTIVE_CHAR && !strcmp(cmdp+1, "endm") \
		? D_ENDM \
		: D_UNKNOWN)
#define push_buffer(save) /* nothing */
#define pop_buffer(save) /* nothing */
#endif

#if OPT_TRACE && !SMALLER
static const char *TraceIndent(int level, const char *cmdp, size_t length)
{
	static	const char indent[] = ".  .  .  .  .  .  .  .  ";
	switch (dname_to_dirnum(cmdp, length)) {
	case D_ELSE:	/* FALLTHRU */
	case D_ELSEIF:	/* FALLTHRU */
	case D_ENDIF:
		if (level > 0)
			level--;
		break;
	default:
		break;
	}
	level = strlen(indent) - (3 * level);
	if (level < 0)
		level = 0;
	return &indent[level];
}
#define TRACE_INDENT(level, cmdp) TraceIndent(level, cmdp, linlen)
#else
#define TRACE_INDENT(level, cmdp) "" /* nothing */
#endif

static int
perform_dobuf(BUFFER *bp, WHLOOP *whlist)
{
	int status = TRUE;
	int glue = 0;		/* nonzero to append lines */
	LINEPTR lp;
	size_t linlen;
	DIRECTIVE dirnum;
	WINDOW *wp;
	char *linebuf = 0;	/* buffer holding copy of line executing */
	char *cmdp;		/* text to execute */
	int save_clhide = clhide;
	int save_no_errs = no_errs;

	static BUFFER *dobuferrbp;

	/* starting at the beginning of the buffer */
	for_each_line(lp, bp) {
		bp->b_dot.l = lp;

		/* calculate the line length and make a local copy */

		if (llength(lp) <= 0)
			linlen = 0;
		else
			linlen = llength(lp);

		if (glue) {
			if ((linebuf = castrealloc(char, linebuf, glue+linlen+1)) == 0) {
				status = no_memory("during macro execution");
				break;
			}
			cmdp = linebuf + glue;
			glue  = 0;
		} else {
			if (linebuf != 0)
				free(linebuf);

			if ((linebuf = cmdp = castalloc(char, linlen+1)) == 0) {
				status = no_memory("during macro execution");
				break;
			}
		}

		if (linlen != 0)
			(void)strncpy(cmdp, lp->l_text, linlen);
		cmdp[linlen] = EOS;	/* make sure it ends */

		/* compress out leading whitespace */
		{
			char *src = cmdp;
			char *dst = cmdp;
			while (isBlank(*src))
				src++;
			while ((*dst++ = *src++) != EOS)
				;
			linlen -= (size_t)(src - dst);
		}

		/*
		 * If the last character on the line is a backslash, glue the
		 * following line to the one we're processing.
		 */
		if (lforw(lp) != buf_head(bp)
		 && linlen != 0
		 && cmdp[linlen-1] == '\\') {
			glue = linlen + (size_t)(cmdp - linebuf) - 1;
			continue;
		}
		cmdp = linebuf;
		while (isBlank(*cmdp))
			++cmdp;

		/* Skip comments and blank lines.
		 * ';' for uemacs backward compatibility, and
		 * '"' for vi compatibility
		 */
		if (*cmdp == ';'
		 || *cmdp == '"'
		 || *cmdp == EOS)
			continue;

#if	OPT_DEBUGMACROS
		/* echo lines and get user confirmation when debugging */
		if (tracemacros) {
			mlforce("<<<%s:%d/%d:%s>>>", bp->b_bname,
				ifstk.level, ifstk.disabled,
				cmdp);
			(void)update(TRUE);

			/* and get the keystroke */
			if (ABORTED(keystroke())) {
				mlforce("[Macro aborted]");
				status = FALSE;
				break;
			}
		}
#endif
		TRACE(("<<<%s%s:%d/%d%c%s%s>>>\n",
			(bp == curbp) ? "*" : "",
			bp->b_bname, ifstk.level, ifstk.disabled,
			ifstk.fired ? '+' : ' ',
			TRACE_INDENT(ifstk.level, cmdp),
			cmdp))

		if (*cmdp == DIRECTIVE_CHAR) {

			dirnum = dname_to_dirnum(cmdp, linlen);
			if (dirnum == D_UNKNOWN) {
				mlforce("[Unknown directive \"%s\"]", cmdp);
				status = FALSE;
				break;
			}

			/* service the ~endm directive here */
			if (dirnum == D_ENDM && !ifstk.disabled) {
				if (!macrobuffer) {
					mlforce(
					"[No macro definition in progress]");
					status = FALSE;
					break;
				}
				macrobuffer->b_dot.l =
					lforw(buf_head(macrobuffer));
				macrobuffer->b_dot.o = 0;
				macrobuffer = NULL;
				continue;
			}
		} else {
			dirnum = D_UNKNOWN;
		}

		/* if macro store is on, just salt this away */
		if (macrobuffer) {
			/* allocate the space for the line */
			if (addline(macrobuffer, cmdp, -1) == FALSE) {
				mlforce("[Out of memory while storing macro]");
				status = FALSE;
				break;
			}
			continue;
		}

		/* goto labels are no-ops */
		if (*cmdp == '*')
			continue;

		no_errs = save_no_errs;

#if ! SMALLER
		/* deal with directives */
		if (dirnum != D_UNKNOWN) {
			int code;

			/* move past directive */
			while (*cmdp && !isBlank(*cmdp))
				++cmdp;

			code = begin_directive(&cmdp, dirnum, whlist, bp, &lp);
			if (code == DDIR_FAILED) {
				status = FALSE;
				break;
			} else if (code == DDIR_COMPLETE) {
				continue;
			} else if (code == DDIR_INCOMPLETE) {
				status = TRUE; /* not exactly an error */
				break;
			} else if (code == DDIR_FORCE) {
				no_errs = TRUE;
			} else if (code == DDIR_HIDDEN) {
				no_errs = TRUE;
				clhide = TRUE;
			}
		} else if (*cmdp != DIRECTIVE_CHAR) {
			/* prefix lines with "WITH" value, if any */
			if (tb_length(line_prefix)) {
				long adj = cmdp - linebuf;
				char *temp = (char *)malloc(
					tb_length(line_prefix) +
					strlen(cmdp) + 2);
				(void)lsprintf(temp, "%s %s",
					tb_values(line_prefix), cmdp);
				free(linebuf);
				linebuf = temp;
				cmdp = linebuf + adj;
			}
		}
#endif

		/* if we are scanning and not executing..go back here */
		if (ifstk.disabled)
			status = TRUE;
		else
			status = docmd(cmdp,TRUE,FALSE,1);

		if (no_errs)
			status = TRUE;

		clhide = save_clhide;
		no_errs = save_no_errs;

		if (status != TRUE) {
			/* update window if visible */
			for_each_visible_window(wp) {
				if (wp->w_bufp == bp) {
					wp->w_dot.l = lp;
					wp->w_dot.o = 0;
					wp->w_flag |= WFHARD;
				}
			}
			/* in any case set the buffer's dot */
			bp->b_dot.l = lp;
			bp->b_dot.o = 0;
			bp->b_wline.l = lforw(buf_head(bp));
			if (dobuferrbp == NULL) {
				dobuferrbp = bp;
				(void)swbuffer(bp);
				kbd_alarm();
			}
			break;
		}
	}

	if (linebuf != 0)
		free(linebuf);

	return status;
}

/*
 * dobuf:	execute the contents of a buffer
 */
int
dobuf(BUFFER *bp)	/* buffer to execute */
{
	int status = FALSE;
	WHLOOP *whlist;
	int save_no_msgs;

	static int dobufnesting; /* flag to prevent runaway recursion */

	save_arguments(bp);
	save_no_msgs = no_msgs;
	no_msgs = TRUE;

	beginDisplay();
	if (++dobufnesting < 9) {

#if ! SMALLER
		if (setup_dobuf(bp, &whlist) != TRUE) {
			status = FALSE;
		} else
#else
		whlist = NULL;
#endif
		{
			IFSTK save_ifstk;
			push_buffer(&save_ifstk);
			status = perform_dobuf(bp, whlist);
			pop_buffer(&save_ifstk);
		}

		macrobuffer = NULL;
		free_all_whiles(whlist);
	}
	dobufnesting--;
	endofDisplay();

	no_msgs = save_no_msgs;
	restore_arguments(bp);

	return status;
}


/*
 * Common function for startup-file, and for :so command.
 */
int
do_source(char *fname, int n, int optional)
{
	register int status;	/* return status of name query */
	char *fspec;		/* full file spec */

	/* look up the path for the file */
	fspec = cfg_locate(fname, LOCATE_SOURCE);

	/* nonexistant */
	if (fspec == NULL)
		return optional ? TRUE : no_such_file(fname);

	/* do it */
	while (n-- > 0)
		if ((status=dofile(fspec)) != TRUE)
			return status;

	return TRUE;
}

#if ! SMALLER
/* ARGSUSED */
int
execfile(	/* execute a series of commands in a file */
int f GCC_UNUSED, int n)	/* default flag and numeric arg to pass on to file */
{
	register int status;
	char fname[NFILEN];	/* name of file to execute */
	static	TBUFF	*last;

	if ((status = mlreply_file("File to execute: ", &last, FILEC_READ, fname)) != TRUE)
		return status;

	return do_source(fname, n, FALSE);
}
#endif

static L_NUM
get_b_lineno(BUFFER *bp)
{
	L_NUM result;
	WINDOW *wp;

	/* try to save the original location, so we can restore it */
	if (curwp->w_bufp == bp)
		result = line_no(bp, curwp->w_dot.l);
	else if ((wp = bp2any_wp(bp)) != 0)
		result = line_no(bp, wp->w_dot.l);
	else
		result = line_no(bp, bp->b_dot.l);
	return result;
}

static void
set_b_lineno(BUFFER *bp, L_NUM n)
{
	WINDOW *wp;
	LINE *lp;

	for_each_line(lp,bp) {
		if (--n <= 0) {
			bp->b_dot.l = lp;
			bp->b_dot.o = 0;
			for_each_visible_window(wp) {
				if (wp->w_bufp == bp) {
					wp->w_dot = bp->b_dot;
					wp->w_flag |= WFMOVE;
				}
			}
			break;
		}
	}
}

/*	dofile:	yank a file into a buffer and execute it
		if there are no errors, delete the buffer on exit */

int
dofile(
char *fname)		/* file name to execute */
{
	register BUFFER *bp;	/* buffer to place file to execute */
	register int status;	/* results of various calls */
	int clobber = FALSE;
	int original;

	if (fname == 0 || *fname == EOS)
		return no_file_found();

	/*
	 * Check first for the name, assuming it's a filename.  If we don't
	 * find an existing buffer with that filename, create a buffer.
	 */
	if ((bp = find_b_file(fname)) == 0) {
		if ((bp = make_bp(fname, 0)) == 0)
			return FALSE;
		clobber = TRUE;
	}

	bp->b_flag |= BFEXEC;

	/* try to save the original location, so we can restore it */
	original = get_b_lineno(bp);

	/* and try to read in the file to execute */
	if ((status = readin(fname, FALSE, bp, TRUE)) == TRUE) {

		/* go execute it! */
		status = dobuf(bp);

		/*
		 * If no errors occurred, and if the buffer isn't displayed,
		 * remove it, unless it was loaded before we entered this
		 * function.  In that case, (try to) jump back to the original
		 * location.
		 */
		if (status != TRUE)
			(void)swbuffer(bp);
		else if ((bp->b_nwnd == 0) && clobber)
			(void)zotbuf(bp);
		else
			set_b_lineno(bp,original);
	}
	return status;
}

/*	cbuf:	Execute the contents of a numbered buffer	*/

static int
cbuf(
int f, int n,	/* default flag and numeric arg */
int bufnum)	/* number of buffer to execute */
{
	register BUFFER *bp;		/* ptr to buffer to execute */
	register int status;		/* status return */
	static char bufname[NBUFN];

	if (!f) n = 1;

	/* make the buffer name */
	(void)lsprintf(bufname, MACRO_N_BufName, bufnum);

	/* find the pointer to that buffer */
	if ((bp = find_b_name(bufname)) == NULL) {
		mlforce("[Macro %d not defined]", bufnum);
		return FALSE;
	}

	status = TRUE;
	/* and now execute it as asked */
	while (n-- > 0 && status == TRUE)
		status = dobuf(bp);

	return status;

}

#include "neexec.h"
