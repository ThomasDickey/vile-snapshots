/*
 *	modes.c
 *
 * Maintain and list the editor modes and value sets.
 *
 * Original code probably by Dan Lawrence or Dave Conroy for MicroEMACS.
 * Major extensions for vile by Paul Fox, 1991
 * Majormode extensions for vile by T.E.Dickey, 1997
 *
 * $Header: /users/source/archives/vile.vcs/RCS/modes.c,v 1.100 1997/09/05 00:14:11 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"
#include	"chgdfunc.h"

#define	NonNull(s)	((s == 0) ? "" : s)
#define	ONE_COL	(80/3)

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
#else
#define relist_settings() /* nothing */
#endif

#if OPT_ENUM_MODES
static	const char * choice_to_name (const FSM_CHOICES *choices, int code);
static	const FSM_CHOICES * name_to_choices (const struct VALNAMES *names);
#endif

/*--------------------------------------------------------------------------*/

#if OPT_EVAL || OPT_MAJORMODE
static	const char **my_varmodes;	/* list for modename-completion */
#endif

#if OPT_MAJORMODE
typedef struct {
	char *name;		/* copy of MAJORMODE.name */
	MAJORMODE *data;	/* pointer to actual data */
	int init;		/* predefined during initialization */
	int flag;		/* true when majormode is active/usable */
	struct VALNAMES qual[MAX_M_VALUES+1]; /* qualifier names */
	struct VALNAMES used[MAX_B_VALUES+1]; /* submode names */
} MAJORMODE_LIST;

static MAJORMODE_LIST *my_majormodes;
static struct VALNAMES *major_valnames;
static struct VAL *major_g_vals;	/* on/off values of major modes */
static struct VAL *major_l_vals;	/* dummy, for convenience */

static const char **my_mode_list;	/* copy of 'all_modes[]' */
#define MODE_CLASSES 5
#define is_bool_type(type) ((type) == VALTYPE_BOOL || (type) == VALTYPE_MAJOR)

static MAJORMODE * lookup_mm_data(const char *name);
static MAJORMODE_LIST * lookup_mm_list(const char *name);
static const char *ModeName(const char *name);
static int attach_mmode(BUFFER *bp, const char *name);
static int detach_mmode(BUFFER *bp, const char *name);
static int enable_mmode(const char *name, int flag);
static void init_my_mode_list(void);

#if OPT_UPBUFF
static void relist_majormodes(void);
#endif

#else

#define MODE_CLASSES 3
#define ModeName(s) s
#define init_my_mode_list() /* nothing */
#define is_bool_type(type) ((type) == VALTYPE_BOOL)
#define my_mode_list all_modes
#define relist_majormodes() /* nothing */

#endif /* OPT_MAJORMODE */

/*--------------------------------------------------------------------------*/

static void
set_winflags(int glob_vals, USHORT flags)
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
#if OPT_MAJORMODE
	case VALTYPE_MAJOR:
#endif
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
		mlforce("BUG: bad type %s %d", ModeName(names->name), names->type);
	}

	return FALSE;
}

/*
 * Returns the value if it is a string, null otherwise.
 */
static const char *
string_val(const struct VALNAMES *names, struct VAL *values)
{
	const char *s = 0;

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

	return (s != 0 && *s == EOS) ? 0 : s;
}

/*
 * Returns the formatted length of a string value.
 */
static int
size_val(const struct VALNAMES *names, struct VAL *values)
{
	return strlen(ModeName(names->name))
		+ 3
		+ strlen(NonNull(string_val(names, values)));
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
		static	char	temp[20];	/* FIXME: const workaround */
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
const char *which,
int nflag,
int local,
const struct VALNAMES *names,
struct VAL *values,
struct VAL *globvalues)
{
	int	show[MAX_G_VALUES+MAX_B_VALUES+MAX_W_VALUES];
	int	any	= 0,
		passes	= 1,
		ncols	= term.t_ncol / ONE_COL,
		padded,
		perline,
		percol,
		total;
	register int j, pass;

	if (ncols > MAXCOLS)
		ncols = MAXCOLS;

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
			switch (names[j].type) {
			case VALTYPE_STRING:
			case VALTYPE_REGEX:
				if (string_val(names+j, values+j) == 0) {
					ok = FALSE;
					break;
				}
				if (size_val(names+j, values+j) > ONE_COL) {
					show[j] += 1;
					passes = 2;
				}
				/* fall-thru */
			default:
				show[j] += 1;
			}
		}
		if (ok && !any++) {
			if (nflag)
				bputc('\n');
			bprintf("%s:\n", which);
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
		int	offsets[MAXCOLS+1];

		offsets[0] = 0;
		if (pass == 1) {
			for (j = percol = 0; j < total; j++) {
				if (show[j] == pass)
					percol++;
			}
			for (j = 1; j < ncols; j++) {
				offsets[j]
					= (percol + ncols - j) / ncols
					+ offsets[j-1];
			}
			perline = ncols;
		} else {	/* these are too wide for ONE_COL */
			offsets[1] = total;
			perline = 1;
		}
		offsets[ncols] = total;

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
			if (is_bool_type(names[j].type)) {
				bprintf("%s%s%*P",
					values[j].vp->i ? "  " : "no",
					ModeName(names[j].name),
					padded, ' ');
			} else {
				VALARGS args;	/* patch */
				args.names  = names+j;
				args.local  = values+j;
				args.global = 0;
				bprintf("  %s=%s%*P",
					ModeName(names[j].name),
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
	static
	const	char	gg[] = "Universal",
			bb[] = "Buffer",
			ww[] = "Window";
	int	nflag, nflg2;

	register WINDOW *localwp = ptr2WINDOW(ptr);  /* alignment okay */
	register BUFFER *localbp = localwp->w_bufp;
	struct VAL	*local_b_vals = localbp->b_values.bv;
	struct VAL	*local_w_vals = localwp->w_values.wv;
	struct VAL	*globl_b_vals = global_b_values.bv;

#if OPT_UPBUFF
	if (relisting_b_vals != 0)
		local_b_vals = relisting_b_vals;
	if (relisting_w_vals != 0)
		local_w_vals = relisting_w_vals;
#endif

#if OPT_MAJORMODE
	if (local && (localbp->majr != 0)) {
		bprintf("--- \"%s\" settings, if different than \"%s\" majormode %*P\n",
			localbp->b_bname,
			localbp->majr->name,
			term.t_ncol-1, '-');
		globl_b_vals = localbp->majr->mb.bv;
	} else
#endif
	bprintf("--- \"%s\" settings, if different than globals %*P\n",
		localbp->b_bname, term.t_ncol-1, '-');

	nflag = listvalueset(bb, FALSE, FALSE, b_valnames, local_b_vals, globl_b_vals);
	nflg2 = listvalueset(ww, nflag, FALSE, w_valnames, local_w_vals, global_w_values.wv);
	if (!(nflag || nflg2))
	 	bputc('\n');
	bputc('\n');

	bprintf("--- %s settings %*P\n",
		local ? "Local" : "Global", term.t_ncol-1, '-');

#if OPT_MAJORMODE
	if (!local) {
		int n;
		for (n = 0; major_valnames[n].name != 0; n++) {
			make_local_val(major_g_vals, n);
			major_g_vals[n].vp->i = my_majormodes[n].flag;
		}
		nflag = listvalueset("Majormodes", nflag, local, major_valnames, major_g_vals, (struct VAL *)0);
	}
#endif
	if (local) {
		nflag = listvalueset(bb, nflag, local, b_valnames, local_b_vals, (struct VAL *)0);
		(void)  listvalueset(ww, nflag, local, w_valnames, local_w_vals, (struct VAL *)0);
	} else {
		nflag = listvalueset(gg, nflag, local, g_valnames, global_g_values.gv, (struct VAL *)0);
		nflag = listvalueset(bb, nflag, local, b_valnames, globl_b_vals, (struct VAL *)0);
		(void)  listvalueset(ww, nflag, local, w_valnames, global_w_values.wv, (struct VAL *)0);
	}
}

/*
 * Set tab size
 */
int
settab(int f, int n)
{
	register WINDOW *wp;
#if OPT_MAJORMODE
	int val = VAL_TAB;
	const char *whichtabs = "T";
#else
	int val;
	const char *whichtabs;
	if (is_c_mode(curbp)) {
		val = VAL_C_TAB;
		whichtabs = "C-t";
	} else {
		val = VAL_TAB;
		whichtabs = "T";
	}
#endif
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
static REGEXVAL *
free_regexval(register REGEXVAL *rp)
{
	if (rp != 0) {
		FreeAndNull(rp->pat);
		FreeAndNull(rp->reg);
		free((char *)rp);
	}
	return 0;
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
		if ((rp->reg = regcomp(rp->pat, magic)) == 0)
			rp = free_regexval(rp);
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
		values->v.r = free_regexval(values->v.r);
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

	for (j = 0; names[j].name != 0; j++) {
		if (is_local_val(val,j)) {
			make_global_val(val, gbl, j);
			free_val(names+j, val+j);
		}
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
	} else if (is_bool_type(names->type)) {
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
	int changed = FALSE;

	if (no && !is_bool_type(names->type))
		return FALSE;		/* this shouldn't happen */

	/*
	 * Check if we're allowed to change this mode in the current context.
	 */
	if ((names->side_effect != 0)
	 && !(*(names->side_effect))(args, (values==globls), TRUE)) {
		return FALSE;
	 }

	/* get a value if we need one */
	if ((end_string() == '=')
	 || (!is_bool_type(names->type) && !unsetting)) {
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
		if (is_bool_type(names->type)) {
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
#if OPT_MAJORMODE
		switch(names->type) {
		case VALTYPE_MAJOR:
			if (values == globls)
				changed = enable_mmode(names->shortname, FALSE);
			else
				changed = detach_mmode(curbp, names->shortname);
			break;
		}
#endif
	} else {
		makeLocalVal(values);	/* make sure we point to result! */

		/* we matched a name -- set the value */
		switch(names->type) {
#if OPT_MAJORMODE
		case VALTYPE_MAJOR:
			values->vp->i = no ? !setting : setting;
			if (values == globls) {
				changed = enable_mmode(names->shortname, values->vp->i);
			} else {
				changed = no
					? detach_mmode(curbp, names->shortname)
					: attach_mmode(curbp, names->shortname);
			}
			break;
#endif
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
	if (!same_val(names, values, &oldvalue))
		changed = TRUE;

	if (changed
	 && (names->side_effect != 0)
	 && !(*(names->side_effect))(args, (values==globls), FALSE))
		status = FALSE;

	if (isLocalVal(&oldvalue)
	 && (values != globls))
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
	init_my_mode_list();

	return kbd_complete(FALSE, c, buf, pos,
		(const void *)&my_mode_list[0], sizeof(my_mode_list[0]));
}

int
/*ARGSUSED*/
mode_eol(const char * buffer GCC_UNUSED, unsigned cpos GCC_UNUSED, int c, int eolchar)
{
	return (c == ' ' || c == eolchar);
}

static int
lookup_valnames(const char *rp, const struct VALNAMES *table)
{
	register int j;

	for (j = 0; table[j].name != 0; j++) {
		if (!strcmp(rp, table[j].name)
		 || !strcmp(rp, table[j].shortname)) {
			return j;
		}
	}
	return -1;
}

int
find_mode(const char *mode, int global, VALARGS *args)
{
	register const char *rp = !strncmp(mode, "no", 2) ? mode+2 : mode;
	register int	class;
	register int	j;

	TRACE(("find_mode(%s) %s\n", mode, global ? "global" : "local"))

	for (class = 0; class < MODE_CLASSES; class++) {
		memset(args, 0, sizeof(*args));
		switch (class) {
		default: /* universal modes */
			args->names  = g_valnames;
			args->global = global_g_values.gv;
			args->local  = (global != FALSE)
				? args->global
				: (struct VAL *)0;
			break;
		case 1:	/* buffer modes */
			args->names  = b_valnames;
			args->global = global_b_values.bv;
			args->local  = (global == TRUE)
				? args->global
				: ((curbp != 0)
					? curbp->b_values.bv
					: (struct VAL *)0);
			break;
		case 2:	/* window modes */
			args->names  = w_valnames;
			args->global = global_w_values.wv;
			args->local  = (global == TRUE)
				? args->global
				: ((curwp != 0)
					? curwp->w_values.wv
					: (struct VAL *)0);
			break;
#if OPT_MAJORMODE
		case 3: /* major modes */
			args->names  = major_valnames;
			args->global = major_g_vals;
			args->local  = (global == TRUE)
				? args->global
				: ((curbp != 0)
					? major_l_vals
					: (struct VAL *)0);
			break;
		case 4: /* major submodes (qualifiers) */
			if (my_majormodes != 0) {
				size_t n = strlen(rp);

				for (j = 0; my_majormodes[j].name; j++) {
					MAJORMODE_LIST *p = my_majormodes+j;
					size_t len = strlen(p->name);

					if (n >= len
					 && !strncmp(rp, p->name, len)
					 && (lookup_valnames(rp, p->qual)) >= 0) {
						args->names  = p->qual;
						args->global = p->data->mm.mv;
						args->local  = (global != FALSE)
							? args->global
							: (struct VAL *)0;
						break;
					}
				}
			}
			break;
#endif
		}
		if (args->names != 0
		 && args->local != 0) {
			if ((j = lookup_valnames(rp, args->names)) >= 0) {
				args->names  += j;
				args->local  += j;
				args->global += j;
				TRACE(("...found class %d\n", class))
				return TRUE;
			}
		}
	}
#if OPT_MAJORMODE
	/* major submodes (buffers) */
	if (my_majormodes != 0 && global != FALSE) {
		int k;
		size_t n = strlen(rp);

		for (j = 0; my_majormodes[j].name; j++) {
			MAJORMODE_LIST *p = my_majormodes+j;
			size_t len = strlen(p->name);

			if (n >= len
			 && !strncmp(rp, p->name, len)
			 && (k = lookup_valnames(rp+len+(rp[len]=='-'), b_valnames)) >= 0
			 && is_local_val(p->data->mb.bv,k)) {
				TRACE(("...found submode %s\n", b_valnames[k].name))
				args->names  = b_valnames + k;
				args->global = p->data->mb.bv + k;
				args->local  = args->global;
				return TRUE;
			}
		}
	}
#endif
	TRACE(("...not found\n"))
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

	/* if the settings are up, redisplay them */
	relist_settings();
	relist_majormodes();

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

static int
reset_color(int n)
{
	if (global_g_val(n) > ncolors) {
		set_global_g_val(n, global_g_val(n) % ncolors);
		return TRUE;
	}
	return FALSE;
}

#if OPT_ENUM_MODES
static FSM_CHOICES *my_colors;
static FSM_CHOICES *my_hilite;
#endif

/*
 * Set the number of colors to a subset of that which is configured.  The main
 * use for this is to switch between 16-colors and 8-colors, though it should
 * work for setting any power of 2 up to the NCOLORS value.
 */
int set_ncolors(int n)
{
	static int initialized;
#if OPT_ENUM_MODES
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

/*--------------------------------------------------------------------------*/

#if OPT_EVAL || OPT_MAJORMODE
/*
 * Test for mode-names that we'll not show in the variable name-completion.
 */
int
is_varmode (const char *name)
{
	return (strncmp(name, "no", 2)
	   &&   strcmp(name, "all"));
}

/*
 * Returns the current number of items in the list of modes
 */
static size_t
count_modes (void)
{
	size_t n;

	init_my_mode_list();

	for (n = 0; my_mode_list[n] != 0; n++)
		;
	return n;
}

/*
 * Return a list of only the modes that can be set with ":setv", ignoring
 * artifacts such as "all".
 */
const char *const *
list_of_modes (void)
{
	if (my_varmodes == 0) {
		const char *const *s;
		const char **d;
		size_t n = count_modes();
		my_varmodes = typeallocn(const char *, n + 1);
		for (s = my_mode_list, d = my_varmodes; (*d = *s) != 0; s++) {
			if (is_varmode(*d)) {
				d++;
			}
		}
	}
	return my_varmodes;
}
#endif /* OPT_EVAL || OPT_MAJORMODE */

/*--------------------------------------------------------------------------*/

#if OPT_MAJORMODE
static int
ok_submode(const char *name)
{
	/* like is_varmode, but allow "no" prefix */
	return strcmp(name, "all") != 0 ? TRUE : FALSE;
}

/* format the name of a majormode's qualifier */
static char *
per_major(char *dst, const char *majr, int code, int brief)
{
	if (brief) {
		(void) lsprintf(dst, "%s%s", majr, m_valnames[code].shortname);
	} else {
		(void) lsprintf(dst, "%s-%s", majr, m_valnames[code].name);
	}
	return dst;
}

/* format the name of a majormode's submode */
static char *
per_submode(char *dst, const char *majr, int code, int brief)
{
	if (brief) {
		if (!strcmp(b_valnames[code].shortname, "X")) {
			*dst = EOS;
			return 0;
		}
		(void) lsprintf(dst, "%s%s", majr, b_valnames[code].shortname);
	} else {
		(void) lsprintf(dst, "%s-%s", majr, b_valnames[code].name);
	}
	return dst;
}

static char *TheMajor;

static const char *
ModeName(const char *name)
{
	if (TheMajor != 0) {
		static char *dst;
		if (dst != 0)
			free(dst);
		dst = malloc(strlen(TheMajor) + strlen(name) + 3);
		(void) lsprintf(dst, "%s-%s", TheMajor, name);
		return dst;
	}
	return name;
}

/* format the name of a majormode */
static char *
majorname(char *dst, const char *majr, int flag)
{
	(void) lsprintf(dst, "%s%smode", flag ? "" : "no", majr);
	return dst;
}

/*
 * Returns the current number of items in the list of modes
 */
static size_t
count_majormodes (void)
{
	size_t n = 0;

	if (my_majormodes != 0) {
		for (n = 0; my_majormodes[n].name != 0; n++)
			;
	}
	return n;
}

static int
found_per_submode(const char *majr, int code)
{
	size_t n;
	char temp[NSTRING];

	init_my_mode_list();

	(void) per_submode(temp, majr, code, TRUE);
	for (n = 0; my_mode_list[n] != 0; n++) {
		if (!strcmp(my_mode_list[n], temp))
			return TRUE;
	}
	return FALSE;
}

/*
 * Insert 'name' into 'my_mode_list[]', which has 'count' entries.
 */
static size_t
insert_per_major(size_t count, const char *name)
{
	if (name != 0) {
		size_t j, k;

		TRACE(("insert_per_major %d %s\n", count, name))

		for (j = 0; j < count; j++) {
			if (strcmp(my_mode_list[j], name) > 0)
				break;
		}
		for (k = ++count; k != j; k--)
			my_mode_list[k] = my_mode_list[k-1];
		my_mode_list[j] = strmalloc(name);
	}
	return count;
}

/*
 * Remove 'name' from 'my_mode_list[]', which has 'count' entries.
 */
static size_t
remove_per_major(size_t count, const char *name)
{
	if (name != 0) {
		size_t j, k;

		for (j = 0; j < count; j++) {
			if (strcmp(my_mode_list[j], name) == 0) {
				count--;
				for (k = j; k <= count; k++)
					my_mode_list[k] = my_mode_list[k+1];
				break;
			}
		}
	}
	return count;
}

/*
 * Lookup a majormode's data area, given its short name, e.g., "c" vs "cmode". 
 * We store the majormodes in an array to simplify name completion, though this
 * complicates definition and removal.
 */
static MAJORMODE *
lookup_mm_data(const char *name)
{
	size_t n;
	if (my_majormodes != 0) {
		for (n = 0; my_majormodes[n].name != 0; n++) {
			if (!strcmp(name, my_majormodes[n].name))
				return my_majormodes[n].data;
		}
	}
	return 0;
}

/*
 * Lookup a majormode's data area, given its short name, e.g., "c" vs "cmode". 
 * We store the majormodes in an array to simplify name completion, though this
 * complicates definition and removal.
 */
static MAJORMODE_LIST *
lookup_mm_list(const char *name)
{
	size_t n;
	if (my_majormodes != 0) {
		for (n = 0; my_majormodes[n].name != 0; n++) {
			if (!strcmp(name, my_majormodes[n].name))
				return my_majormodes+n;
		}
	}
	return 0;
}

/* Check if a majormode is predefined.  There are some things we don't want to
 * do to them (such as remove them).
 */
static int predef_majormode(const char *name)
{
	size_t n;
	int status = FALSE;

	if (my_majormodes != 0) {
		for (n = 0; my_majormodes[n].name != 0; n++) {
			if (!strcmp(name, my_majormodes[n].name)) {
				status = my_majormodes[n].init;
				break;
			}
		}
	}
	return status;
}

static int
major_complete(int c, char *buf, unsigned *pos)
{
	return kbd_complete(FALSE, c, buf, pos, (const char *)&my_majormodes[0],
		sizeof(my_majormodes[0]));
}

static int
prompt_majormode(char **result, int defining)
{
	static TBUFF *cbuf; 	/* buffer to receive mode name into */
	int status;

	/* prompt the user and get an answer */
	tb_scopy(&cbuf, "");
	if ((status = kbd_reply("majormode: ",
		&cbuf,
		eol_history, ' ',
		KBD_NORMAL,	/* FIXME: KBD_MAYBEC if !defining */
		(defining || clexec)
			? no_completion
			: major_complete)) == TRUE) {
		/* FIXME: check for legal name (alphanumeric) */
		if ((status = is_varmode(tb_values(cbuf))) == TRUE) {
			*result = tb_values(cbuf);
			if (defining && lookup_mm_data(*result) != 0) {
				TRACE(("Mode already exists\n"))
				return SORTOFTRUE;
			} else if (!defining && lookup_mm_data(*result) == 0) {
				TRACE(("Mode does not exist\n"))
				return SORTOFTRUE;
			}
			return TRUE;
		}
	}
	if (status != FALSE)
		mlwarn("[Illegal name %s]", tb_values(cbuf));
	return status;
}

static int
submode_complete(int c, char *buf, unsigned *pos)
{
	return kbd_complete(FALSE, c, buf, pos, (const char *)&all_submodes[0],
		sizeof(all_submodes[0]));
}

static int
prompt_submode(char **result, int defining)
{
	static TBUFF *cbuf; 	/* buffer to receive mode name into */
	register const char *rp;
	int status;

	/* prompt the user and get an answer */
	tb_scopy(&cbuf, "");
	if ((status = kbd_reply("submode: ",
		&cbuf,
		eol_history, '=',
		KBD_NORMAL,
		submode_complete)) == TRUE) {
		if ((status = ok_submode(tb_values(cbuf))) == TRUE) {
			*result = tb_values(cbuf);
			rp = !strncmp(*result, "no", 2) ? *result+2 : *result;
			if (defining && lookup_mm_data(rp) != 0) {
				TRACE(("Mode already exists\n"))
				return SORTOFTRUE;
			} else if (!defining && lookup_mm_data(rp) == 0) {
				TRACE(("Mode does not exist\n"))
				return SORTOFTRUE;
			}
			return TRUE;
		}
	}
	mlwarn("[Illegal name %s]", tb_values(cbuf));
	return status;
}

/*
 * Attach a buffer to the given majormode.  Adjust all of the non-local buffer
 * modes to point to the majormode's values where those in turn are local.
 */
static int
attach_mmode(BUFFER *bp, const char *name)
{
	int n;

	if (bp != 0) {
		if (bp->majr != 0
		 && strcmp(bp->majr->name, name) != 0)
			(void) detach_mmode(bp, bp->majr->name);

		TRACE(("attach_mmode '%s' to '%s'\n", name, bp->b_bname))
		if ((bp->majr = lookup_mm_data(name)) != 0) {
			struct VAL *mm = bp->majr->mb.bv;

			/* adjust buffer modes */
			for (n = 0; n < MAX_B_VALUES; n++) {
				if (!is_local_b_val(bp,n)
				 && is_local_val(mm,n)) {
					make_global_val(bp->b_values.bv, mm, n);
				}
			}
			return TRUE;
		}
		return (bp->majr != 0);
	}
	
	return FALSE;
}

/*
 * Detach a buffer from the given majormode.  Modify the buffer's minor modes
 * to point to global modes where they've been pointed to the majormode's data.
 */
static int
detach_mmode(BUFFER *bp, const char *name)
{
	size_t n;
	MAJORMODE *mp;

	if (bp != 0
	 && (mp = bp->majr) != 0
	 && !strcmp(mp->name, name)) {
		TRACE(("detach_mmode '%s', given '%s'\n", name, mp->name))
		/* readjust the buffer's modes */
		for (n = 0; n < MAX_B_VALUES; n++) {
			if (!is_local_b_val(bp,n)
			 && is_local_val(mp->mb.bv,n)) {
				make_global_b_val(bp,n);
			}
		}
		relist_settings();
		bp->majr = 0;
		return TRUE;
	}
	
	return FALSE;
}

static int
enable_mmode(const char *name, int flag)
{
	MAJORMODE_LIST *ptr = lookup_mm_list(name);
	if (ptr != 0
	 && ptr->flag != flag) {
		ptr->flag = flag;
		return TRUE;
	}
	return FALSE;
}

static int
free_majormode(const char *name)
{
	MAJORMODE *ptr = lookup_mm_data(name);
	size_t j;
	int n;
	char temp[NSTRING];
	BUFFER *bp;

	if (ptr != 0) {
		int init = TRUE;
		for (j = 0; my_majormodes[j].name != 0; j++) {
			if (my_majormodes[j].data == ptr) {
				init = my_majormodes[j].init;
				for_each_buffer(bp) {
					if (detach_mmode(bp, my_majormodes[j].name)) {
						set_winflags(TRUE, WFHARD|WFMODE);
					}
				}
				free_local_vals(m_valnames, major_g_vals, ptr->mm.mv);
				free_local_vals(b_valnames, global_b_values.bv, ptr->mb.bv);
				free(ptr->name);
				free(ptr);
				do {
					my_majormodes[j] = my_majormodes[j+1];
				} while (my_majormodes[j++].name != 0);
				break;
			}
		}
		if (my_mode_list != all_modes && !init) {
			j = count_modes();
			j = remove_per_major(j, majorname(temp, name, FALSE));
			j = remove_per_major(j, majorname(temp, name, TRUE));
			for (n = 0; n < MAX_M_VALUES; n++) {
				j = remove_per_major(j,
					per_major(temp, name, n, TRUE));
				j = remove_per_major(j,
					per_major(temp, name, n, FALSE));
			}
		}
		if (major_valnames != 0) {
			for (n = 0; major_valnames[n].name != 0; n++) {
				if (!strcmp(name, major_valnames[n].shortname)) {
					free((char *)major_valnames[n].name);
					free((char *)major_valnames[n].shortname);
					while (major_valnames[n].name != 0) {
						major_valnames[n] =
						major_valnames[n+1];
						n++;
					}
					break;
				}
			}
		}
		return TRUE;
	}
	return FALSE;
}

static void
init_my_mode_list(void)
{
	if (my_mode_list == 0)
		my_mode_list = (const char **)all_modes;
}

static int
extend_mode_list(int increment)
{
	int j = count_modes();
	int k = increment + j + 1;

	TRACE(("extend_mode_list from %d by %d\n", j, increment))

	if (my_mode_list == all_modes) {
		my_mode_list = typeallocn(const char *, k);
		memcpy(my_mode_list, all_modes, (j+1) * sizeof(*my_mode_list));
	} else {
		my_mode_list = typereallocn(const char *, my_mode_list, k);
	}
	return j;
}

static struct VAL *
extend_VAL_array(struct VAL *ptr, size_t item, size_t len)
{
	size_t j, k;

	TRACE(("extend_VAL_array %p item %d of %d\n", ptr, item, len))

	if (ptr == 0) {
		ptr = typeallocn(struct VAL, len + 1);
	} else {
		ptr = typereallocn(struct VAL, ptr, len + 1);
		for (j = k = 0; j < len; j++) {
			k = (j >= item) ? j+1 : j;
			ptr[k] = ptr[j];
			make_local_val(ptr, k);
		}
	}
	make_local_val(ptr, item);
	make_local_val(ptr, len);
	ptr[item].v.i = FALSE;
	return ptr;
}

static void
set_qualifier(const struct VALNAMES *names, struct VAL *values, const char *s)
{
	switch (names->type) {
	case VALTYPE_STRING:
		if (values->v.p)
			free(values->v.p);
		values->v.p = strmalloc(s);
		break;
	case VALTYPE_REGEX:
		free_regexval(values->v.r);
		values->v.r = new_regexval(s, TRUE);
		break;
	}
	make_local_val(values, 0);
}

static void
reset_qualifier(const struct VALNAMES *names, struct VAL *values)
{
	set_qualifier(names, values, "");
}

/*
 * Buffer-animation for [Major Modes]
 */
#if OPT_UPBUFF
static int
show_majormodes(BUFFER *bp)
{
	b_clr_obsolete(bp);
	return list_majormodes(FALSE, 1);
}

static void
relist_majormodes(void)
{
	update_scratch(MAJORMODES_BufName, show_majormodes);
}
#endif	/* OPT_UPBUFF */

/* list the current modes into the current buffer */
/* ARGSUSED */
static void	
makemajorlist(int local, void *ptr GCC_UNUSED)
{
	int j;
	int nflag;
	MAJORMODE *data;

	if (my_majormodes != 0) {
		for (j = 0; my_majormodes[j].name != 0; j++) {
			if (local)
				TheMajor = my_majormodes[j].name;
			nflag = 0;
			data = my_majormodes[j].data;
			bprintf("--- \"%s\" majormode settings %*P\n",
				my_majormodes[j].name,
				term.t_ncol-1, '-');
			nflag = listvalueset("Qualifier", FALSE, TRUE,
				m_valnames,
				data->mm.mv,
				data->mm.mv);
			nflag = listvalueset("Buffer",    nflag, TRUE,
				b_valnames,
				data->mb.bv,
				global_b_values.bv);
			if (my_majormodes[j+1].data)
				bputc('\n');
		}
	}
	TheMajor = 0;
}

/* ARGSUSED */
int
list_majormodes(int f, int n GCC_UNUSED)
{
	register WINDOW *wp = curwp;
	register int s;

	s = liststuff(MAJORMODES_BufName, FALSE, makemajorlist,f,(void *)wp);
	/* back to the buffer whose modes we just listed */
	if (swbuffer(wp->w_bufp))
		curwp = wp;

	return s;
}

int
alloc_mode(const char *name, int predef)
{
	size_t j, k;
	int n;
	char temp[NSTRING];

	if (major_valnames == 0) {
		major_valnames = typecallocn(struct VALNAMES, 2);
		j = 0;
		k = 1;
	} else {
		k = count_majormodes();
		major_valnames = typereallocn(struct VALNAMES, major_valnames, k+2);
		for (j = k++; j != 0; j--) {
			major_valnames[j] = major_valnames[j-1];
			if (strcmp(major_valnames[j-1].shortname, name) < 0) {
				break;
			}
		}
	}

	(void) majorname(temp, name, TRUE);
	major_valnames[j].name        = strmalloc(temp);
	major_valnames[j].shortname   = strmalloc(name);
	major_valnames[j].type        = VALTYPE_MAJOR;
	major_valnames[j].side_effect = chgd_major_w;

	memset(major_valnames+k, 0, sizeof(*major_valnames));

	/* build arrays needed for 'find_mode()' bookkeeping */
	major_g_vals = extend_VAL_array(major_g_vals, j, k);
	major_l_vals = extend_VAL_array(major_l_vals, j, k);

	if (my_majormodes == 0) {
		my_majormodes = typecallocn(MAJORMODE_LIST, 2);
		j = 0;
		k = 1;
	} else {
		k = count_majormodes();
		my_majormodes = typereallocn(MAJORMODE_LIST, my_majormodes, k+2);
		for (j = k++; j != 0; j--) {
			my_majormodes[j] = my_majormodes[j-1];
			if (strcmp(my_majormodes[j-1].name, name) < 0) {
				break;
			}
		}
	}

	my_majormodes[j].data = typecalloc(MAJORMODE);
	my_majormodes[j].name = my_majormodes[j].data->name = strmalloc(name);
	my_majormodes[j].init = predef;
	my_majormodes[j].flag = TRUE;
	memset(my_majormodes+k, 0, sizeof(*my_majormodes));

	for (k = 0; k < MAX_B_VALUES; k++) {
		make_global_val(my_majormodes[j].data->mb.bv, global_b_values.bv, k);
	}

	/* copy array to get types, then overwrite the name-pointers */
	memcpy(my_majormodes[j].qual, m_valnames, sizeof(m_valnames));
	for (k = 0; k < MAX_M_VALUES; k++) {
		reset_qualifier(m_valnames+k, my_majormodes[j].data->mm.mv+k);
		my_majormodes[j].qual[k].name =
				strmalloc(per_major(temp, name, k, TRUE));
		my_majormodes[j].qual[k].shortname =
				strmalloc(per_major(temp, name, k, FALSE));
	}

	/*
	 * Create the majormode-specific names.  If this is predefined, we
	 * already had mktbls do this.
	 */
	if (!predef) {
		j = extend_mode_list((MAX_M_VALUES + 2) * 2);
		j = insert_per_major(j, majorname(temp, name, FALSE));
		j = insert_per_major(j, majorname(temp, name, TRUE));
		for (n = 0; n < MAX_M_VALUES; n++) {
			j = insert_per_major(j, per_major(temp, name, n, TRUE));
			j = insert_per_major(j, per_major(temp, name, n, FALSE));
		}
	}

	return TRUE;
}

/* ARGSUSED */
int
define_mode(int f GCC_UNUSED, int n GCC_UNUSED)
{
	char *name;
	int status;

	if ((status = prompt_majormode(&name, TRUE)) == TRUE) {
		TRACE(("define majormode:%s\n", name))
		status = alloc_mode(name, FALSE);
		relist_settings();
		relist_majormodes();
	} else if (status == SORTOFTRUE) {
		status = TRUE;	/* don't complain if it's already true */
	}
	return status;
}

static int
do_a_submode(int defining)
{
	char *name;
	char *subname;
	int status;
	MAJORMODE *ptr;
	VALARGS args;
	int j, k;
	int qualifier = FALSE;
	char temp[NSTRING];
	char *rp;

	if ((status = prompt_majormode(&name, FALSE)) != TRUE)
		return status;

	if ((status = prompt_submode(&subname, TRUE)) != TRUE)
		return status;

	ptr = lookup_mm_data(name);
	rp = !strncmp(subname, "no", 2) ? subname+2 : subname;
	if ((j = lookup_valnames(rp, m_valnames)) >= 0) {
		qualifier   = TRUE;
		args.names  = m_valnames;
		args.global = ptr->mm.mv;
		args.local  = ptr->mm.mv;
	} else if ((j = lookup_valnames(rp, b_valnames)) >= 0) {
		args.names  = b_valnames;
		args.global = defining ? ptr->mb.bv :global_b_values.bv;
		args.local  = ptr->mb.bv;
	} else {
		mlwarn("[BUG: no such submode %s]", rp);
		return FALSE;
	}

	args.names  += j;
	args.global += j;
	args.local  += j;

	/*
	 * We store submodes in the majormode as local values.
	 */
	status = adjvalueset(subname, defining, FALSE, &args);

	/*
	 * Check if we deleted one of the qualifiers, since there's no global
	 * value to inherit back to, we'll have to ensure there's valid data.
	 */
	if (status == TRUE
	 && qualifier
	 && !defining) {
		reset_qualifier(args.names, args.global);
	}

	if (status == TRUE
	 && !qualifier) {
		if (defining && found_per_submode(name, j)) {
			TRACE(("submode names for %d present\n", j))
		} else if (defining) {
			TRACE(("construct submode names for %d\n", j))
			k = extend_mode_list(2);
			k = insert_per_major(k,
				per_submode(temp, name, j, TRUE));
			k = insert_per_major(k,
				per_submode(temp, name, j, FALSE));
		} else {
			TRACE(("destroy submode names for %d\n", j))
			k = count_modes();
			k = remove_per_major(k,
				per_submode(temp, name, j, TRUE));
			k = remove_per_major(k,
				per_submode(temp, name, j, FALSE));
		}
	}

	/* FIXME: remember to adjust all buffers that used this mode, in
	 * case we make a minor mode part-of or removed from the major mode.
	 */
	relist_settings();
	relist_majormodes();
	return TRUE;
}

/* ARGSUSED */
int
define_submode(int f GCC_UNUSED, int n GCC_UNUSED)
{
	return do_a_submode(TRUE);
}

/* ARGSUSED */
int
remove_submode(int f GCC_UNUSED, int n GCC_UNUSED)
{
	return do_a_submode(FALSE);
}

/* ARGSUSED */
int
remove_mode(int f GCC_UNUSED, int n GCC_UNUSED)
{
	char *name;
	int status;

	if ((status = prompt_majormode(&name, FALSE)) == TRUE) {
		if ((status = !predef_majormode(name)) == TRUE) {
			TRACE(("remove majormode:%s\n", name))
			free_majormode(name);
			relist_settings();
			relist_majormodes();
		} else {
			mlwarn("[This is a predefined mode: %s]", name);
		}
	} else if (status == SORTOFTRUE) {
		status = TRUE;
	}
	return status;
}

/*
 * Returns the regular expression for the given indices, checking that the
 * pattern is non-null.
 */
static regexp *
get_mm_rexp(int n, int m)
{
	struct VAL *mv = my_majormodes[n].data->mm.mv;

	if (mv[m].vp->r != 0
	 && mv[m].vp->r->pat != 0
	 && mv[m].vp->r->pat[0] != 0
	 && mv[m].vp->r->reg != 0) {
		TRACE(("get_mm_rexp(%s) %s\n",
			my_majormodes[n].name,
			mv[m].vp->r->pat))
		return mv[m].vp->r->reg;
	}
	return 0;
}

/*
 * Use a regular expression (normally a suffix, such as ".c") to match the
 * buffer's filename.  If found, set the first matching majormode.
 */
void
setm_by_suffix(register BUFFER *bp)
{
	if (my_majormodes != 0) {
		size_t n = 0;
		int savecase = ignorecase;
#if OPT_CASELESS
		ignorecase = TRUE;
#else
		ignorecase = FALSE;
#endif

		for (n = 0; my_majormodes[n].name != 0; n++) {
			if (my_majormodes[n].flag) {
				regexp *exp = get_mm_rexp(n, MVAL_SUFFIXES);
				if (exp != 0
				 && regexec(exp, bp->b_fname, (char *)0, 0, -1)) {
					attach_mmode(bp, my_majormodes[n].name);
					break;
				}
			}
		}
		ignorecase = savecase;
	}
}

static LINE *
get_preamble(register BUFFER *bp)
{
	if (!is_empty_buf(bp)) {
		LINE *lp = lforw(buf_head(bp));
		if (lisreal(lp))
			return lp;
	}
	return 0;
}

/*
 * Match the first line of the buffer against a regular expression, setting
 * the first matching majormode, if any.
 */
void
setm_by_preamble(register BUFFER *bp)
{
	LINE *lp = get_preamble(bp);

	if (lp != 0
	 && my_majormodes != 0) {
		size_t n = 0;
		int savecase = ignorecase;
#if OPT_CASELESS
		ignorecase = TRUE;
#else
		ignorecase = FALSE;
#endif

		for (n = 0; my_majormodes[n].name != 0; n++) {
			if (my_majormodes[n].flag) {
				regexp *exp = get_mm_rexp(n, MVAL_PREAMBLE);
				if (exp != 0
				 && lregexec(exp, lp, 0, llength(lp))) {
					attach_mmode(bp, my_majormodes[n].name);
					break;
				 }
			}
		}
		ignorecase = savecase;
	}
}

void
set_submode_val(const char *name, int n, int value)
{
	MAJORMODE *p;
	TRACE(("set_majormode_val(%s, %d, %d)\n", name, n, value))
	if ((p = lookup_mm_data(name)) != 0) {
		p->mb.bv[n].v.i = value;
		make_local_val(p->mb.bv, n);
	}
}

void
set_majormode_rexp(const char *name, int n, const char *r)
{
	MAJORMODE *p;
	TRACE(("set_majormode_rexp(%s, %d, %s)\n", name, n, r))
	if ((p = lookup_mm_data(name)) != 0)
		set_qualifier(m_valnames+n, p->mm.mv + n, r);
}
#endif /* OPT_MAJORMODE */

/*--------------------------------------------------------------------------*/

#if NO_LEAKS
void
mode_leaks(void)
{
#if OPT_ENUM_MODES && OPT_COLOR
	FreeAndNull(my_colors);
	FreeAndNull(my_hilite);
#endif

#if OPT_EVAL || OPT_MAJORMODE
	FreeAndNull(my_varmodes);
#endif
#if OPT_MAJORMODE
	while (my_majormodes != 0 && my_majormodes->name != 0) {
		char temp[NSTRING];
		free_majormode(strcpy(temp, my_majormodes->name));
	}
	FreeAndNull(my_majormodes);

	FreeAndNull(major_g_vals);
	FreeAndNull(major_l_vals);
	FreeAndNull(my_mode_list);
	FreeAndNull(major_valnames);
#endif
}
#endif /* NO_LEAKS */
