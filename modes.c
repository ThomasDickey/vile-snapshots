/*
 *
 *	modes.c
 *
 * Maintain and list the editor modes and value sets.
 *
 * Original code probably by Dan Lawrence or Dave Conroy for MicroEMACS.
 * Major extensions for vile by Paul Fox, 1991
 *
 * $Header: /users/source/archives/vile.vcs/RCS/modes.c,v 1.92 1997/06/20 01:23:55 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"
#include	"chgdfunc.h"

#define	NonNull(s)	((s == 0) ? "" : s)
#define	ONE_COL	26
#define	NCOLS	3

#define isLocalVal(valptr)          ((valptr)->vp == &((valptr)->v))
#define makeLocalVal(valptr)        ((valptr)->vp = &((valptr)->v))

/* FIXME */
#define OPT_COLOR_CHOICES	OPT_COLOR
#define OPT_BOOL_CHOICES	1
#define OPT_POPUP_CHOICES	OPT_POPUPCHOICE
#define OPT_BACKUP_CHOICES	OPT_FILEBACK
#define OPT_HILITE_CHOICES	OPT_HILITEMATCH

#include "nefsms.h"

/*--------------------------------------------------------------------------*/

#if OPT_UPBUFF
static	void	relist_settings (void);
#endif

#if OPT_ENUM_MODES
static	const char * choice_to_name (const FSM_CHOICES *choices, int code);
static	const FSM_CHOICES * name_to_choices (const struct VALNAMES *names);
#endif

/*--------------------------------------------------------------------------*/

static void
set_winflags(int glob_vals, int flags)
{
	if (glob_vals) {
		register WINDOW *wp;
		for_each_window(wp) {
			if ((wp->w_bufp == NULL)
			 || !b_is_scratch(wp->w_bufp)
			 || !(flags & WFMODE))
				wp->w_flag |= flags;
		}
	} else {
		curwp->w_flag |= flags;
	}
}

static int
same_val(const struct VALNAMES *names, struct VAL *tst, struct VAL *ref)
{
	if (ref == 0)	/* can't test, not really true */
		return -TRUE;

	switch (names->type) {
	case VALTYPE_BOOL:
	case VALTYPE_ENUM:
	case VALTYPE_INT:
		return	(tst->vp->i == ref->vp->i);
	case VALTYPE_STRING:
		return	(tst->vp->p != 0)
		  &&	(ref->vp->p != 0)
		  &&	!strcmp(tst->vp->p, ref->vp->p);
	case VALTYPE_REGEX:
		return	(tst->vp->r->pat != 0)
		  &&	(ref->vp->r->pat != 0)
		  &&	!strcmp(tst->vp->r->pat, ref->vp->r->pat);
	default:
		mlforce("BUG: bad type %s %d", names->name, names->type);
	}

	return FALSE;
}

/*
 * Returns the formatted length of a string value.
 */
static int
size_val(const struct VALNAMES *names, struct VAL *values)
{
	register int	n = strlen(names->name) + 3;
	register const char *s = 0;

	switch (names->type) {
#if	OPT_ENUM_MODES		/* will show the enum name too */
	case VALTYPE_ENUM:
		s = choice_to_name(name_to_choices(names), values->vp->i);
		break;
#endif
	case VALTYPE_STRING:
		s = values->vp->p;
		break;
	case VALTYPE_REGEX:
		s = values->vp->r->pat;
		break;
	}
	return	n + strlen(NonNull(s));
}

/*
 * Returns a mode-value formatted as a string
 */
char *
string_mode_val(VALARGS *args)
{
	register const struct VALNAMES *names = args->names;
	register struct VAL     *values = args->local;
	switch(names->type) {
	case VALTYPE_BOOL:
		return values->vp->i ? truem : falsem;
	case VALTYPE_ENUM:
#if OPT_ENUM_MODES
		{
		static	char	temp[20];
		(void)strcpy(temp,
			choice_to_name(name_to_choices(names), values->vp->i));
		return temp;
		}
#endif				/* else, fall-thru to use int-code */
	case VALTYPE_INT:
		return l_itoa(values->vp->i);
	case VALTYPE_STRING:
		return NonNull(values->vp->p);
	case VALTYPE_REGEX:
		return NonNull(values->vp->r->pat);
	}
	return errorm;
}

/* listvalueset:  print each value in the array according to type, along with
 * its name, until a NULL name is encountered.  If not local, only print if the value in the
 * two arrays differs, or the second array is nil.  If local, print only the
 * values in the first array that are local.
 */
static int
listvalueset(
char *which,
int nflag,
int local,
const struct VALNAMES *names,
struct VAL *values,
struct VAL *globvalues)
{
	int	show[MAX_G_VALUES+MAX_B_VALUES+MAX_W_VALUES];
	int	any	= 0,
		passes	= 1,
		padded,
		perline,
		percol,
		total;
	register int j, pass;

	/*
	 * First, make a list of values we want to show.
	 * Store:
	 *	0 - don't show
	 *	1 - show in first pass
	 *	2 - show in second pass (too long)
	 */
	for (j = 0; names[j].name != 0; j++) {
		int ok = FALSE;
		show[j] = 0;
		if (local) {
			ok = is_local_val(values,j);
		} else {
			ok = (same_val(names+j, values+j, globvalues ? globvalues+j : 0) != TRUE);
		}
		if (ok) {
			if (!any++) {
				if (nflag)
					bputc('\n');
				bprintf("%s:\n", which);
			}
			switch (names[j].type) {
			case VALTYPE_STRING:
			case VALTYPE_REGEX:
				if (size_val(names+j, values+j) > ONE_COL) {
					show[j] += 1;
					passes = 2;
				}
				/* fall-thru */
			default:
				show[j] += 1;
			}
		}
	}
	total = j;

	if (any) {
		if (!passes)
			passes = 1;
	} else
		return nflag;

	/*
	 * Now, go back and display the values
	 */
	for (pass = 1; pass <= passes; pass++) {
		register int	line, col, k;
		int	offsets[NCOLS+1];

		offsets[0] = 0;
		if (pass == 1) {
			for (j = percol = 0; j < total; j++) {
				if (show[j] == pass)
					percol++;
			}
			for (j = 1; j < NCOLS; j++) {
				offsets[j]
					= (percol + NCOLS - j) / NCOLS
					+ offsets[j-1];
			}
			perline = NCOLS;
		} else {	/* these are too wide for ONE_COL */
			offsets[1] = total;
			perline = 1;
		}
		offsets[NCOLS] = total;

		line = 0;
		col  = 0;
		for_ever {
			k = line + offsets[col];
			for (j = 0; j < total; j++) {
				if (show[j] == pass) {
					if (k-- <= 0)
						break;
				}
			}
			if (k >= 0)	/* no more cells to display */
				break;

			if (col == 0)
				bputc(' ');
			padded = (col+1) < perline ? ONE_COL : 1;
			if (names[j].type == VALTYPE_BOOL) {
				bprintf("%s%s%*P",
					values[j].vp->i ? "  " : "no",
					names[j].name,
					padded, ' ');
			} else {
				VALARGS args;	/* patch */
				args.names  = names+j;
				args.local  = values+j;
				args.global = 0;
				bprintf("  %s=%s%*P",
					names[j].name,
					string_mode_val(&args),
					padded, ' ');
			}
			if (++col >= perline) {
				col = 0;
				bputc('\n');
				if (++line >= offsets[1])
					break;
			} else if (line+offsets[col] >= offsets[col+1])
				break;
		}
		if ((col != 0) || (pass != passes))
			bputc('\n');
	}
	return TRUE;
}

#ifdef lint
static	/*ARGSUSED*/ WINDOW *ptr2WINDOW(void *p) { return 0; }
#else
#define	ptr2WINDOW(p)	(WINDOW *)p
#endif

/* list the current modes into the current buffer */
/* ARGSUSED */
static
void	
makemodelist(int local, void *ptr)
{
	static	char	gg[] = "Universal",
			bb[] = "Buffer",
			ww[] = "Window";
	int	nflag, nflg2;

	register WINDOW *localwp = ptr2WINDOW(ptr);  /* alignment okay */
	register BUFFER *localbp = localwp->w_bufp;
	struct VAL	*local_b_vals = localbp->b_values.bv;
	struct VAL	*local_w_vals = localwp->w_values.wv;
#if OPT_UPBUFF
	if (relisting_b_vals != 0)
		local_b_vals = relisting_b_vals;
	if (relisting_w_vals != 0)
		local_w_vals = relisting_w_vals;
#endif

	bprintf("--- \"%s\" settings, if different than globals %*P\n",
			localbp->b_bname, term.t_ncol-1, '-');
	nflag = listvalueset(bb, FALSE, FALSE, b_valuenames, local_b_vals, global_b_values.bv);
	nflg2 = listvalueset(ww, nflag, FALSE, w_valuenames, local_w_vals, global_w_values.wv);
	if (!(nflag || nflg2))
	 	bputc('\n');
	bputc('\n');

	bprintf("--- %s settings", local ? "Local" : "Global");
	bprintf(" %*P\n", term.t_ncol - 1 - DOT.o, '-');
	if (local) {
		nflag = listvalueset(bb, nflag, local, b_valuenames, local_b_vals, (struct VAL *)0);
		(void)  listvalueset(ww, nflag, local, w_valuenames, local_w_vals, (struct VAL *)0);
	} else {
		nflag = listvalueset(gg, nflag, local, g_valuenames, global_g_values.gv, (struct VAL *)0);
		nflag = listvalueset(bb, nflag, local, b_valuenames, global_b_values.bv, (struct VAL *)0);
		(void)  listvalueset(ww, nflag, local, w_valuenames, global_w_values.wv, (struct VAL *)0);
	}
}

/*
 * Set tab size
 */
int
settab(int f, int n)
{
	register WINDOW *wp;
	int val;
	const char *whichtabs;
	if (b_val(curbp, MDCMOD)) {
		val = VAL_C_TAB;
		whichtabs = "C-t";
	} else {
		val = VAL_TAB;
		whichtabs = "T";
	}
	if (f && n >= 1) {
		make_local_b_val(curbp,val);
		set_b_val(curbp,val,n);
		curtabval = n;
		for_each_window(wp)
			if (wp->w_bufp == curbp) wp->w_flag |= WFHARD;
	} else if (f) {
		mlwarn("[Illegal tabstop value]");
		return FALSE;
	}
	if (!global_b_val(MDTERSE) || !f)
		mlwrite("[%sabs are %d columns apart, using %s value.]", whichtabs,
			curtabval,
			is_local_b_val(curbp,val) ? "local" : "global" );
	return TRUE;
}

/*
 * Set fill column to n.
 */
int
setfillcol(int f, int n)
{
	if (f && n >= 1) {
		make_local_b_val(curbp,VAL_FILL);
		set_b_val(curbp,VAL_FILL,n);
	} else if (f) {
		mlwarn("[Illegal fill-column value]");
		return FALSE;
	}
	if (!global_b_val(MDTERSE) || !f)
		mlwrite("[Fill column is %d, and is %s]",
			b_val(curbp,VAL_FILL),
			is_local_b_val(curbp,VAL_FILL) ? "local" : "global" );
	return(TRUE);
}

/*
 * Release storage of a REGEXVAL struct
 */
static void
free_regexval(register REGEXVAL *rp)
{
	if (rp != 0) {
		FreeAndNull(rp->pat);
		FreeAndNull(rp->reg);
		free((char *)rp);
	}
}

/*
 * Allocate/set a new REGEXVAL struct
 */
REGEXVAL *
new_regexval(const char *pattern, int magic)
{
	register REGEXVAL *rp;

	if ((rp = typealloc(REGEXVAL)) != 0) {
		rp->pat = strmalloc(pattern);
		if ((rp->reg = regcomp(rp->pat, magic)) == 0) {
			free_regexval(rp);
			rp = 0;
		}
	}
	return rp;
}

/*
 * Release storage of a VAL struct
 */
static void
free_val(const struct VALNAMES *names, struct VAL *values)
{
	switch (names->type) {
	case VALTYPE_STRING:
		FreeAndNull(values->v.p);
		break;
	case VALTYPE_REGEX:
		free_regexval(values->v.r);
		break;
	default:	/* nothing to free */
		break;
	}
}

/*
 * Copy a VAL-struct, preserving the sense of local/global.
 */
static int
copy_val(struct VAL *dst, struct VAL *src)
{
	register int local = isLocalVal(src);

	*dst = *src;
	if (local)
		makeLocalVal(dst);
	return local;
}

void
copy_mvals(
int maximum,
struct VAL *dst,
struct VAL *src)
{
	register int	n;
	for (n = 0; n < maximum; n++)
		(void)copy_val(&dst[n], &src[n]);
}

/*
 * This is a special routine designed to save the values of local modes and to
 * restore them.  The 'recompute_buffer()' procedure assumes that global modes
 * do not change during the recomputation process (so there is no point in
 * trying to convert any of those values to local ones).
 */
#if OPT_UPBUFF
void
save_vals(
int maximum,
struct VAL *gbl,
struct VAL *dst,
struct VAL *src)
{
	register int	n;
	for (n = 0; n < maximum; n++)
		if (copy_val(&dst[n], &src[n]))
			make_global_val(src, gbl, n);
}
#endif

/*
 * free storage used by local mode-values, called only when we are freeing
 * all other storage associated with a buffer or window.
 */
void
free_local_vals(
const struct VALNAMES *names,
struct VAL *gbl,
struct VAL *val)
{
	register int	j;

	for (j = 0; names[j].name != 0; j++)
		if (is_local_val(val,j)) {
			make_global_val(val, gbl, j);
			free_val(names+j, val+j);
		}
}

/*
 * Convert a string to boolean, checking for errors
 */
static int
string_to_bool(const char *base, int *np)
{
	if (is_truem(base))
		*np = TRUE;
	else if (is_falsem(base))
		*np = FALSE;
	else {
		mlforce("[Not a boolean: '%s']", base);
		return FALSE;
	}
	return TRUE;
}

/*
 * Convert a string to number, checking for errors
 */
int
string_to_number(const char *from, int *np)
{
	long n;
	char *p;

	/* accept decimal, octal, or hex */
	n = strtol(from, &p, 0);
	if (p == from || *p != EOS) {
		mlforce("[Not a number: '%s']", from);
		return FALSE;
	}
	*np = (int)n;
	return TRUE;
}

/*
 * Validate a 'glob' mode-value.  It is either a boolean, or it must be a
 * pipe-expression with exactly one "%s" embedded (no other % characters,
 * unless escaped).  That way, we can use the string to format the pipe
 * command.
 */
#if defined(GMD_GLOB) || defined(GVAL_GLOB)
static int
legal_glob_mode(const char *base)
{
#ifdef GVAL_GLOB	/* string */
	if (isShellOrPipe(base)) {
		register const char *s = base;
		int	count = 0;
		while (*s != EOS) {
			if (*s == '%') {
				if (*++s != '%') {
					if (*s == 's')
						count++;
					else
						count = 2;
				}
			}
			if (*s != EOS)
				s++;
		}
		if (count == 1)
			return TRUE;
	}
#endif
	if (!strcmp(base, "off")
	 || !strcmp(base, "on"))
	 	return TRUE;

	mlforce("[Illegal value for glob: '%s']", base);
	return FALSE;
}
#endif

/* 
 * FSM stands for fixed string mode, so called because the strings which the
 * user is permitted to enter are non-arbitrary (fixed).
 * 
 * It is meant to handle the following sorts of things:
 *
 * 	:set popup-choices off
 * 	:set popup-choices immediate
 * 	:set popup-choices delayed
 * 
 * 	:set error quiet
 * 	:set error beep
 * 	:set error flash
 */
#if OPT_ENUM_MODES

#if NEVER
FSM_CHOICES fsm_error[] = {
	{ "beep",      1},
	{ "flash",     2},
	{ "quiet",     0},
	END_CHOICES
};
#endif

static
struct FSM fsm_tbl[] = {
	{ "*bool",           fsm_bool_choices  },
#if OPT_COLOR_CHOICES
	{ "fcolor",          fsm_color_choices },
	{ "bcolor",          fsm_color_choices },
#endif
#if OPT_POPUP_CHOICES
	{ "popup-choices",   fsm_popup_choices },
#endif
#if NEVER
	{ "error",           fsm_error },
#endif
#if OPT_BACKUP_CHOICES
	{ "backup-style",    fsm_backup_choices },
#endif
#if OPT_HILITE_CHOICES
	{ "visual-matches",  fsm_hilite_choices },
#endif
};

static int fsm_idx;

static int
choice_to_code (const FSM_CHOICES *choices, const char *name)
{
	int code = ENUM_ILLEGAL;
	register int i;

	for (i = 0; choices[i].choice_name != 0; i++) {
		if (strcmp(name, choices[i].choice_name) == 0) {
			code = choices[i].choice_code;
			break;
		}
	}
	return code;
}

static const char *
choice_to_name (const FSM_CHOICES *choices, int code)
{
	const char *name = 0;
	register int i;

	for (i = 0; choices[i].choice_name != 0; i++) {
		if (choices[i].choice_code == code) {
			name = choices[i].choice_name;
			break;
		}
	}
	return name;
}

static const FSM_CHOICES *
name_to_choices (const struct VALNAMES *names)
{
	register SIZE_T i;

	for (i = 1; i < TABLESIZE(fsm_tbl); i++)
		if (strcmp(fsm_tbl[i].mode_name, names->name) == 0)
			return fsm_tbl[i].choices;

	return 0;
}

static int
is_fsm(const struct VALNAMES *names)
{
	register SIZE_T i;

	if (names->type == VALTYPE_ENUM
	 || names->type == VALTYPE_STRING) {
		for (i = 1; i < TABLESIZE(fsm_tbl); i++) {
			if (strcmp(fsm_tbl[i].mode_name, names->name) == 0) {
				fsm_idx = (int)i;
				return TRUE;
			}
		}
	} else if (names->type == VALTYPE_BOOL) {
		fsm_idx = 0;
		return TRUE;
	}
	fsm_idx = -1;
	return FALSE;
}

/*
 * Test if we're processing an enum-valued mode.  If so, lookup the mode value. 
 * We'll allow a numeric index also (e.g., for colors).  Note that we're
 * returning the table-value in that case, so we'll have to ensure that we
 * don't corrupt the table.
 */
static const char *
legal_fsm(const char *val)
{
	if (fsm_idx >= 0) {
		int i;
		int idx = fsm_idx;
		const FSM_CHOICES *p = fsm_tbl[idx].choices;
		const char *s;

		if (isdigit(*val)) {
			if (!string_to_number(val, &i))
				return 0;
			if ((s = choice_to_name(p, i)) != 0)
				return s;
		} else {
			if (choice_to_code(p, val) != ENUM_ILLEGAL)
				return val;
		}
		mlforce("[Illegal value for %s: '%s']",
			fsm_tbl[idx].mode_name,
			val);
		return 0;
	}
	return val;
}

static int
fsm_complete(int c, char *buf, unsigned *pos)
{
    if (isdigit(*buf)) {		/* allow numbers for colors */
	if (c != NAMEC)  		/* put it back (cf: kbd_complete) */
	    unkeystroke(c);
	return isspace(c);
    }
    return kbd_complete(FALSE, c, buf, pos,
                        (const void *)(fsm_tbl[fsm_idx].choices),
			sizeof (FSM_CHOICES) );
}
#endif	/* OPT_ENUM_MODES */

/*
 * Lookup the mode named with 'cp[]' and adjust its value.
 */
int
adjvalueset(
const char *cp,			/* name of the mode we are changing */
int setting,			/* true if setting, false if unsetting */
int global,
VALARGS *args)			/* symbol-table entry for the mode */
{
	const struct VALNAMES *names = args->names;
	struct VAL     *values = args->local;
	struct VAL     *globls = args->global;
	REGEXVAL *r;

	struct VAL oldvalue;
	char prompt[NLINE];
	char respbuf[NFILEN];
	int no = !strncmp(cp, "no", 2);
	const char *rp = no ? cp+2 : cp;
	int nval, status = TRUE;
	int unsetting = !setting && !global;

	if (no && (names->type != VALTYPE_BOOL))
		return FALSE;		/* this shouldn't happen */

	/*
	 * Check if we're allowed to change this mode in the current context.
	 */
	if ((names->side_effect != 0)
	 && !(*(names->side_effect))(args, (values==globls), TRUE))
		return FALSE;

	/* get a value if we need one */
	if ((end_string() == '=')
	 || (names->type != VALTYPE_BOOL && !unsetting)) {
		int	regex = (names->type == VALTYPE_REGEX);
		int	opts = regex ? 0 : KBD_NORMAL;
		int	eolchar = (names->type == VALTYPE_REGEX
				|| names->type == VALTYPE_STRING) ? '\n' : ' ';
		int	(*complete) (DONE_ARGS) = no_completion;

		respbuf[0] = EOS;
		(void)lsprintf(prompt, "New %s %s: ",
			cp,
			regex ? "pattern" : "value");

#if OPT_ENUM_MODES
		if (is_fsm(names))
			complete = fsm_complete;
#endif

		status = kbd_string(prompt, respbuf, sizeof(respbuf), eolchar,
		               opts, complete);
		if (status != TRUE)
			return status;
		if (!strlen(rp = respbuf))
			return FALSE;
#if defined(GMD_GLOB) || defined(GVAL_GLOB)
		if (!strcmp(names->name, "glob")
		 && !legal_glob_mode(rp))
		 	return FALSE;
#endif
#if OPT_ENUM_MODES
		 if ((rp = legal_fsm(rp)) == 0)
		    return FALSE;
#endif  
		/* Test after fsm, to allow translation */
		if (names->type == VALTYPE_BOOL) {
			if (!string_to_bool(rp, &setting))
				return FALSE;
		}
	}
#if OPT_HISTORY
	else
		hst_glue(' ');
#endif

	/* save, to simplify no-change testing */
	(void)copy_val(&oldvalue, values);

	if (unsetting) {
		make_global_val(values, globls, 0);
	} else {
		makeLocalVal(values);	/* make sure we point to result! */

		/* we matched a name -- set the value */
		switch(names->type) {
		case VALTYPE_BOOL:
			values->vp->i = no ? !setting : setting;
			break;

		case VALTYPE_ENUM:
#if OPT_ENUM_MODES
			{
				const FSM_CHOICES *fp = name_to_choices(names);

				if (isdigit(*rp)) {
					if (!string_to_number(rp, &nval))
						return FALSE;
					if (choice_to_name(fp, nval) == 0)
						nval = ENUM_ILLEGAL;
				} else {
					nval = choice_to_code(fp, rp);
				}
				if (nval == ENUM_ILLEGAL) {
					mlforce("[Not a legal enum-index: %s]",
						rp);
					return FALSE;
				}
			}
			values->vp->i = nval;
			break;
#endif /* OPT_ENUM_MODES */

		case VALTYPE_INT:
			if (!string_to_number(rp, &nval))
				return FALSE;
			values->vp->i = nval;
			break;

		case VALTYPE_STRING:
			values->vp->p = strmalloc(rp);
			break;

		case VALTYPE_REGEX:
			if ((r = new_regexval(rp, TRUE)) == 0)
				return FALSE;
			values->vp->r = r;
			break;

		default:
			mlforce("BUG: bad type %s %d", names->name, names->type);
			return FALSE;
		}
	}

	/*
	 * Set window flags (to force the redisplay as needed), and apply
	 * side-effects.
	 */
	status = TRUE;
	if (!same_val(names, values, &oldvalue)
	 && (names->side_effect != 0)
	 && !(*(names->side_effect))(args, (values==globls), FALSE))
		status = FALSE;

	if (isLocalVal(&oldvalue))
		free_val(names, &oldvalue);
	return status;
}

/* ARGSUSED */
int
listmodes(int f, int n GCC_UNUSED)
{
	register WINDOW *wp = curwp;
	register int s;

	s = liststuff(SETTINGS_BufName, FALSE, makemodelist,f,(void *)wp);
	/* back to the buffer whose modes we just listed */
	if (swbuffer(wp->w_bufp))
		curwp = wp;
	return s;
}

/*
 * The 'mode_complete()' and 'mode_eol()' functions are invoked from
 * 'kbd_reply()' to setup the mode-name completion and query displays.
 */
static int
mode_complete(DONE_ARGS)
{
	return kbd_complete(FALSE, c, buf, pos,
		(const void *)&all_modes[0], sizeof(all_modes[0]));
}

int
/*ARGSUSED*/
mode_eol(const char * buffer GCC_UNUSED, unsigned cpos GCC_UNUSED, int c, int eolchar)
{
	return (c == ' ' || c == eolchar);
}

int
find_mode(const char *mode, int global, VALARGS *args)
{
	register const char *rp = !strncmp(mode, "no", 2) ? mode+2 : mode;
	register int	class;
	register int	j;

	for (class = 0; class < 3; class++) {
		switch (class) {
		default: /* universal modes */
			args->names  = g_valuenames;
			args->global = global_g_values.gv;
			args->local  = (global != FALSE)
				? args->global
				: (struct VAL *)0;
			break;
		case 1:	/* buffer modes */
			args->names  = b_valuenames;
			args->global = global_b_values.bv;
			args->local  = (global == TRUE)
				? args->global
				: ((curbp != 0)
					? curbp->b_values.bv
					: (struct VAL *)0);
			break;
		case 2:	/* window modes */
			args->names  = w_valuenames;
			args->global = global_w_values.wv;
			args->local  = (global == TRUE)
				? args->global
				: ((curwp != 0)
					? curwp->w_values.wv
					: (struct VAL *)0);
			break;
		}
		if (args->local != 0) {
			for (j = 0; args->names[j].name != NULL; j++) {
				if (!strcmp(rp, args->names[j].name)
				 || !strcmp(rp, args->names[j].shortname)) {
					args->names  += j;
					args->local  += j;
					args->global += j;
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

/*
 * Process a single mode-setting
 */
static int
do_a_mode(int kind, int global)
{
	VALARGS	args;
	register int	s;
	static TBUFF *cbuf; 	/* buffer to receive mode name into */

	/* prompt the user and get an answer */
	tb_scopy(&cbuf, "");
	if ((s = kbd_reply(
		global	? "Global value: "
			: "Local value: ",
		&cbuf,
		mode_eol, '=', KBD_NORMAL|KBD_LOWERC, mode_complete)) != TRUE)
		return ((s == FALSE) ? SORTOFTRUE : s);

	if (!strcmp(tb_values(cbuf), "all")) {
		hst_glue(' ');
		return listmodes(FALSE,1);
	}

	if ((s = find_mode(tb_values(cbuf), global, &args)) != TRUE) {
#if OPT_EVAL
		if (!global && (s = find_mode(tb_values(cbuf), TRUE, &args)) == TRUE) {
			mlforce("[Not a local mode: \"%s\"]", tb_values(cbuf));
			return FALSE;
		}
		return set_variable(tb_values(cbuf));
#else
		mlforce("[Not a legal set option: \"%s\"]", tb_values(cbuf));
#endif
	} else if ((s = adjvalueset(tb_values(cbuf), kind, global, &args)) != 0) {
		if (s == TRUE)
			mlerase();	/* erase the junk */
		return s;
	}

	return FALSE;
}

/*
 * Process the list of mode-settings
 */
static int
adjustmode(	/* change the editor mode status */
int kind,	/* true = set,		false = delete */
int global)	/* true = global flag,	false = current buffer flag */
{
	int s;
	int anything = 0;

	if (kind && global && isreturn(end_string()))
		return listmodes(TRUE,1);

	while (((s = do_a_mode(kind, global)) == TRUE) && (end_string() == ' '))
		anything++;
	if ((s == SORTOFTRUE) && anything) /* fix for trailing whitespace */
		return TRUE;

#if OPT_UPBUFF
	/* if the settings are up, redisplay them */
	relist_settings();
#endif /* OPT_UPBUFF */

	if (curbp) {
		curtabval = tabstop_val(curbp);
		curswval = shiftwid_val(curbp);
	}

	return s;
}

/*
 * Buffer-animation for [Settings]
 */
#if OPT_UPBUFF
static int
show_Settings(BUFFER *bp)
{
	b_clr_obsolete(bp);
	return listmodes(FALSE, 1);
}

static void
relist_settings(void)
{
	update_scratch(SETTINGS_BufName, show_Settings);
}
#endif	/* OPT_UPBUFF */

/* ARGSUSED */
int
setlocmode(int f GCC_UNUSED, int n GCC_UNUSED)	/* prompt and set an editor mode */
{
	return adjustmode(TRUE, FALSE);
}

/* ARGSUSED */
int
dellocmode(int f GCC_UNUSED, int n GCC_UNUSED)	/* prompt and delete an editor mode */
{
	return adjustmode(FALSE, FALSE);
}

/* ARGSUSED */
int
setglobmode(int f GCC_UNUSED, int n GCC_UNUSED)	/* prompt and set a global editor mode */
{
	return adjustmode(TRUE, TRUE);
}

/* ARGSUSED */
int
delglobmode(int f GCC_UNUSED, int n GCC_UNUSED)	/* prompt and delete a global editor mode */
{
	return adjustmode(FALSE, TRUE);
}

/*
 * The following functions are invoked to carry out side effects of changing
 * modes.
 */
/*ARGSUSED*/
int
chgd_autobuf(VALARGS *args GCC_UNUSED, int glob_vals, int testing GCC_UNUSED)
{
	if (glob_vals)
		sortlistbuffers();
	return TRUE;
}

/*ARGSUSED*/
int
chgd_buffer(VALARGS *args GCC_UNUSED, int glob_vals, int testing GCC_UNUSED)
{
	if (!glob_vals) {	/* i.e., ":setl" */
		if (curbp == 0)
			return FALSE;
		b_clr_counted(curbp);
		(void)bsizes(curbp);
	}
	return TRUE;
}

int
chgd_charset(VALARGS *args, int glob_vals, int testing)
{
	if (!testing) {
		charinit();
	}
	return chgd_window(args, glob_vals, testing);
}

#if OPT_COLOR
int
chgd_color(VALARGS *args, int glob_vals, int testing)
{
	if (!testing) {
		if (&args->local->vp->i == &gfcolor)
			TTforg(gfcolor);
		else if (&args->local->vp->i == &gbcolor)
			TTbacg(gbcolor);
		set_winflags(glob_vals, WFHARD|WFCOLR);
		vile_refresh(FALSE,0);
	}
	return TRUE;
}

#endif	/* OPT_COLOR */

#if OPT_EVAL
static void
set_fsm_choice(const char *name, const FSM_CHOICES *choices)
{
	size_t n;
	for (n = 0; n < TABLESIZE(fsm_tbl); n++) {
		if (!strcmp(name, fsm_tbl[n].mode_name)) {
			fsm_tbl[n].choices = choices;
			break;
		}
	}
}
#endif	/* OPT_EVAL */

#if OPT_COLOR
static int
reset_color(int n)
{
	if (global_g_val(n) > ncolors) {
		set_global_g_val(n, global_g_val(n) % ncolors);
		return TRUE;
	}
	return FALSE;
}

/*
 * Set the number of colors to a subset of that which is configured.  The main
 * use for this is to switch between 16-colors and 8-colors, though it should
 * work for setting any power of 2 up to the NCOLORS value.
 */
int set_ncolors(int n)
{
	static int initialized;
#if OPT_ENUM_MODES
	static FSM_CHOICES *my_colors;
	static FSM_CHOICES *my_hilite;
	const FSM_CHOICES *the_colors, *the_hilite;
	size_t s, d;
#endif

	if (n > NCOLORS || n < 2)
		return FALSE;
	if (!initialized)
		initialized = n;
	if (n > initialized)
		return FALSE;
	ncolors = n;
	if (reset_color(GVAL_FCOLOR)
	 || reset_color(GVAL_BCOLOR)) {
		vile_refresh(FALSE,0);
	}

#if OPT_ENUM_MODES
	if (ncolors == NCOLORS) {
		the_colors = fsm_color_choices;
		the_hilite = fsm_hilite_choices;
	} else {
		my_colors = typecallocn(FSM_CHOICES,TABLESIZE(fsm_color_choices));
		my_hilite = typecallocn(FSM_CHOICES,TABLESIZE(fsm_hilite_choices));
		the_colors = my_colors;
		the_hilite = my_hilite;
		for (s = d = 0; s < TABLESIZE(fsm_color_choices)-1; s++) {
			my_colors[d] = fsm_color_choices[s];
			if (my_colors[d].choice_code > 0) {
				if (!(my_colors[d].choice_code %= ncolors))
					continue;
			}
			if (strncmp(my_colors[d].choice_name, "bright", 6))
				d++;
		}
		my_colors[d].choice_name = 0;

		for (s = d = 0; s < TABLESIZE(fsm_hilite_choices)-1; s++) {
			my_hilite[d] = fsm_hilite_choices[s];
			if (my_hilite[d].choice_code & VASPCOL) {
				unsigned code = my_hilite[d].choice_code % NCOLORS;
				if (code != 0) {
					if ((code %= ncolors) == 0)
						continue;
					my_hilite[d].choice_code = VASPCOL | code;
				}
				if (strncmp(my_hilite[d].choice_name, "bright", 6))
					d++;
			} else {
				d++;
			}
		}
		my_hilite[d].choice_name = 0;
	}
	set_fsm_choice("fcolor", the_colors);
	set_fsm_choice("bcolor", the_colors);
	set_fsm_choice("visual-matches", the_hilite);
#endif /* OPT_ENUM_MODES */
	return TRUE;
}
#endif	/* OPT_COLOR */

	/* Report mode that cannot be changed */
/*ARGSUSED*/
int
chgd_disabled(VALARGS *args, int glob_vals GCC_UNUSED, int testing GCC_UNUSED)
{
	mlforce("[Cannot change \"%s\" ]", args->names->name);
	return FALSE;
}

	/* Change "fences" mode */
/*ARGSUSED*/
int
chgd_fences(VALARGS *args, int glob_vals GCC_UNUSED, int testing)
{
	if (!testing) {
		/* was even number of fence pairs specified? */
		char *value = args->local->v.p;
		size_t len = strlen(value);

		if (len & 1) {
			value[len-1] = EOS;
			mlwrite(
			"[Fence-pairs not in pairs:  truncating to \"%s\"",
				value);
			return FALSE;
		}
	}
	return TRUE;
}

	/* Change a "major" mode */
int
chgd_major(VALARGS *args, int glob_vals, int testing)
{
	/* prevent major-mode changes for scratch-buffers */
	if (testing) {
		if (!glob_vals) {
			if (b_is_scratch(curbp))
				return chgd_disabled(args, glob_vals, testing);
		}
	} else {
		set_winflags(glob_vals, WFMODE);
	}
	return TRUE;
}

	/* Change a major mode that affects the windows on the buffer */
int
chgd_major_w(VALARGS *args, int glob_vals, int testing)
{
	if (testing) {
		if (!chgd_major(args, glob_vals, testing))
			return FALSE;
		return chgd_window(args, glob_vals, testing);
	}

	set_winflags(glob_vals, WFHARD|WFMODE);
	return TRUE;
}

	/* Change something on the mode/status line */
/*ARGSUSED*/
int
chgd_status(VALARGS *args GCC_UNUSED, int glob_vals, int testing)
{
	if (!testing) {
		set_winflags(glob_vals, WFSTAT);
	}
	return TRUE;
}

	/* Change a mode that affects the windows on the buffer */
/*ARGSUSED*/
int
chgd_window(VALARGS *args GCC_UNUSED, int glob_vals, int testing)
{
	if (!testing) {
		set_winflags(glob_vals, WFHARD);
	}
	return TRUE;
}

	/* Change the working mode */
#if OPT_WORKING
/*ARGSUSED*/
int
chgd_working(VALARGS *args GCC_UNUSED, int glob_vals, int testing GCC_UNUSED)
{
	if (glob_vals)
		imworking(0);
	return TRUE;
}
#endif

	/* Change the xterm-mouse mode */
/*ARGSUSED*/
int
chgd_xterm(VALARGS *args GCC_UNUSED, int glob_vals, int testing GCC_UNUSED)
{
#if	OPT_XTERM
	if (glob_vals) {
		int	new_state = global_g_val(GMDXTERM_MOUSE);
		set_global_g_val(GMDXTERM_MOUSE,TRUE);
		if (!new_state)	TTkclose();
		else		TTkopen();
		set_global_g_val(GMDXTERM_MOUSE,new_state);
	}
#endif
	return TRUE;
}

	/* Change a mode that affects the search-string highlighting */
/*ARGSUSED*/
int
chgd_hilite(VALARGS *args GCC_UNUSED, int glob_vals GCC_UNUSED, int testing)
{
	if (!testing)
		attrib_matches();
	return TRUE;
}
