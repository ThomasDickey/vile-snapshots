/*
 *	modes.c
 *
 * Maintain and list the editor modes and value sets.
 *
 * Original code probably by Dan Lawrence or Dave Conroy for MicroEMACS.
 * Major extensions for vile by Paul Fox, 1991
 * Majormode extensions for vile by T.E.Dickey, 1997
 *
 * $Header: /users/source/archives/vile.vcs/RCS/modes.c,v 1.175 1999/09/08 01:54:00 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"
#include	"chgdfunc.h"

#if SYS_VMS
#include <rms.h>
#endif

#define	NonNull(s)	((s == 0) ? "" : s)
#define	ONE_COL	(80/3)

#define isLocalVal(valptr)          ((valptr)->vp == &((valptr)->v))
#define makeLocalVal(valptr)        ((valptr)->vp = &((valptr)->v))

#include "nefsms.h"

/*--------------------------------------------------------------------------*/

#if OPT_UPBUFF
static	void	relist_settings (void);
#else
#define relist_settings() /* nothing */
#endif

#if OPT_ENUM_MODES
static	const FSM_CHOICES * valname_to_choices (const struct VALNAMES *names);
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
	struct VALNAMES qual[MAX_M_VALUES+1]; /* majormode qualifier names */
	struct VALNAMES used[MAX_B_VALUES+1]; /* submode names */
	struct VALNAMES subq[MAX_Q_VALUES+1]; /* submode qualifier names */
} MAJORMODE_LIST;

static MAJORMODE_LIST *my_majormodes;
static M_VALUES global_m_values;	/* dummy, for convenience */
static struct VAL *major_g_vals;	/* on/off values of major modes */
static struct VAL *major_l_vals;	/* dummy, for convenience */
static struct VALNAMES *major_valnames;

static const char **my_mode_list;	/* copy of 'all_modes[]' */
#define MODE_CLASSES 5
#define is_bool_type(type) ((type) == VALTYPE_BOOL || (type) == VALTYPE_MAJOR)

static MAJORMODE * lookup_mm_data(const char *name);
static MAJORMODE_LIST * lookup_mm_list(const char *name);
static char * majorname(char *dst, const char *majr, int flag);
static const char *ModeName(const char *name);
static int attach_mmode(BUFFER *bp, const char *name);
static int detach_mmode(BUFFER *bp, const char *name);
static int enable_mmode(const char *name, int flag);
static struct VAL *get_sm_vals(MAJORMODE *ptr);
static void init_my_mode_list(void);
static void reset_qualifier(const struct VALNAMES *names, struct VAL *values);
static void reset_sm_qualifiers(MAJORMODE *ptr);

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

#if OPT_ENUM_MODES || !SMALLER
int
choice_to_code (const FSM_CHOICES *choices, const char *name, size_t len)
{
	char temp[NSTRING];
	int code = ENUM_ILLEGAL;
	int i;

	if (len > NSTRING-1)
		len = NSTRING-1;

	memcpy(temp, name, len);
	temp[len] = EOS;
	(void)mklower(temp);

	for (i = 0; choices[i].choice_name != 0; i++) {
		if (!strncmp(temp, choices[i].choice_name, len)) {
			if (choices[i].choice_name[len] == EOS
			 || choices[i+1].choice_name == 0
			 || strncmp(temp, choices[i+1].choice_name, len))
				code = choices[i].choice_code;
			break;
		}
	}
	return code;
}

const char *
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
#endif /* OPT_ENUM_MODES || !SMALLER */

#if !SMALLER
/*
 * Add the values from one or more keywords.  If any error is found, return that
 * code instead.
 */
int
combine_choices(const FSM_CHOICES *choices, const char *string)
{
	char temp[NSTRING];
	const char *s;
	int code;
	int result = 0;

	while (*string) {
		vl_strncpy(temp, string, sizeof(temp));
		if ((s = strchr(string, '+')) != 0) {
			temp[s - string] = EOS;
			string = s + 1;
		} else {
			string += strlen(string);
		}
		if ((code = choice_to_code(choices, temp, strlen(temp))) >= 0) {
			result += code;
		} else {
			result = code;
			break;
		}
	}
	return result;
}
#endif

/*--------------------------------------------------------------------------*/

static void
set_winflags(int glob_vals, USHORT flags)
{
	if (glob_vals) {
		register WINDOW *wp;
		for_each_visible_window(wp) {
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
		/*FALLTHRU*/
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
		s = choice_to_name(valname_to_choices(names), values->vp->i);
		break;
#endif
	case VALTYPE_STRING:
		s = values->vp->p;
		break;
	case VALTYPE_REGEX:
		if (values->vp->r)
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
	static TBUFF *result;
	switch(names->type) {
#if OPT_MAJORMODE
	case VALTYPE_MAJOR:
#endif
	case VALTYPE_BOOL:
		return values->vp->i ? "TRUE" : "FALSE";
	case VALTYPE_ENUM:
#if OPT_ENUM_MODES
		{
		static	char	temp[20];	/* FIXME: const workaround */
		(void)strcpy(temp,
			choice_to_name(valname_to_choices(names), values->vp->i));
		return temp;
		}
#endif				/* else, fall-thru to use int-code */
	case VALTYPE_INT:
		return render_int(&result, values->vp->i);
	case VALTYPE_STRING:
		return NonNull(values->vp->p);
	case VALTYPE_REGEX:
		if (values->vp->r == 0)
			break;
		return NonNull(values->vp->r->pat);
	}
	return error_val;
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
		ncols	= term.cols / ONE_COL,
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
			case VALTYPE_ENUM:
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
		any  = 0;
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
			any++;
			if (++col >= perline) {
				col = 0;
				bputc('\n');
				if (++line >= offsets[1])
					break;
			} else if (line+offsets[col] >= offsets[col+1])
				break;
		}
		if (any && (pass != passes))
			bputc('\n');
		if (col != 0)
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
			term.cols-1, '-');
		globl_b_vals = get_sm_vals(localbp->majr);
	} else
#endif
	bprintf("--- \"%s\" settings, if different than globals %*P\n",
		localbp->b_bname, term.cols-1, '-');

	nflag = listvalueset(bb, FALSE, FALSE, b_valnames, local_b_vals, globl_b_vals);
	nflg2 = listvalueset(ww, nflag, FALSE, w_valnames, local_w_vals, global_w_values.wv);
	if (!(nflag || nflg2))
		bputc('\n');
	bputc('\n');

	bprintf("--- %s settings %*P\n",
		local ? "Local" : "Global", term.cols-1, '-');

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
		for_each_visible_window(wp)
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
	if (f) {
		make_local_b_val(curbp,VAL_FILL);
		set_b_val(curbp,VAL_FILL,n);
	}
	if (!global_b_val(MDTERSE) || !f)
		mlwrite("[Fill column is %d, and is %s]",
			getfillcol(curbp),
			is_local_b_val(curbp,VAL_FILL) ? "local" : "global" );
	return(TRUE);
}

/*
 * Returns the effective fill-column
 */
int
getfillcol(BUFFER *bp)
{
	int result = b_val(bp,VAL_FILL);
	if (result == 0) {
		result = term.cols - b_val(bp,VAL_WRAPMARGIN);
	} else if (result < 0) {
		result = term.cols + result;
	}
	return (result);
}

/*
 * Release storage of a REGEXVAL struct
 */
REGEXVAL *
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
 *	:set popup-choices off
 *	:set popup-choices immediate
 *	:set popup-choices delayed
 *
 *	:set error quiet
 *	:set error beep
 *	:set error flash
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

#if OPT_COLOR_CHOICES
static const char s_fcolor[]       = "fcolor";
static const char s_bcolor[]       = "bcolor";
#endif

#if OPT_COLOR_SCHEMES
static const char s_default[]      = "default";
static const char s_color_scheme[] = "color-scheme";
static const char s_palette[]      = "palette";
static const char s_video_attrs[]  = "video-attrs";
static const
FSM_CHOICES fsm_no_choices[] = {
	{ s_default,   0},
	END_CHOICES	/* ends table for name-completion */
};
#endif /* OPT_COLOR_SCHEMES */

static
struct FSM fsm_tbl[] = {
	{ "*bool",           fsm_bool_choices  },
#if OPT_COLOR_SCHEMES
	{ s_color_scheme,    fsm_no_choices },
#endif
#if OPT_COLOR_CHOICES
	{ s_fcolor,          fsm_color_choices },
	{ s_bcolor,          fsm_color_choices },
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
	{ "mcolor",          fsm_hilite_choices },
	{ "visual-matches",  fsm_hilite_choices },
	{ "mini-hilite",     fsm_hilite_choices },
#endif
#if OPT_COLOR
	{ s_video_attrs,     fsm_videoattrs_choices },
#endif
#if OPT_VTFLASHSEQ_CHOICES
	{ "vtflash",         fsm_vtflashseq_choices },
#endif
#if SYS_VMS
	{ "record-format",   fsm_recordformat_choices },
#endif
#if OPT_RECORDSEP_CHOICES
	{ "recordseparator", fsm_recordsep_choices },
#endif
};

static int fsm_idx;

static size_t
fsm_size(const FSM_CHOICES *list)
{
	size_t result = 0;
	while ((list++)->choice_name != 0)
		result++;
	return result;
}

const FSM_CHOICES *
name_to_choices (const char *name)
{
	register SIZE_T i;

	for (i = 1; i < TABLESIZE(fsm_tbl); i++)
		if (strcmp(fsm_tbl[i].mode_name, name) == 0)
			return fsm_tbl[i].choices;

	return 0;
}

static const FSM_CHOICES *
valname_to_choices (const struct VALNAMES *names)
{
	return name_to_choices(names->name);
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

		if (isDigit(*val)) {
			if (!string_to_number(val, &i))
				return 0;
			if ((s = choice_to_name(p, i)) != 0)
				return s;
		} else {
			if (choice_to_code(p, val, strlen(val)) != ENUM_ILLEGAL)
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
    if (isDigit(*buf)) {		/* allow numbers for colors */
	if (c != NAMEC) 		/* put it back (cf: kbd_complete) */
	    unkeystroke(c);
	return isSpace(c);
    }
    return kbd_complete(FALSE, c, buf, pos,
			(const char *)(fsm_tbl[fsm_idx].choices),
			sizeof (FSM_CHOICES) );
}
#endif	/* OPT_ENUM_MODES */

/*
 * Lookup the mode named with 'cp[]' and adjust its value.
 */
int
adjvalueset(
const char *cp,			/* name of the mode we are changing */
int defining,			/* for majormodes, suppress side_effect */
int setting,			/* true if setting, false if unsetting */
int global,
VALARGS *args)			/* symbol-table entry for the mode */
{
	const struct VALNAMES *names = args->names;
	struct VAL     *values = args->local;
	struct VAL     *globls = args->global;

	char prompt[NLINE];
	char respbuf[NFILEN];
	int no = !strncmp(cp, "no", 2);
	const char *rp = NULL;
	int status = TRUE;
	int unsetting = !setting && !global;

	if (no && !is_bool_type(names->type))
		return FALSE;		/* this shouldn't happen */

	/*
	 * Check if we're allowed to change this mode in the current context.
	 */
	if (!defining
	 && (names->side_effect != 0)
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
	}
#if OPT_HISTORY
	else
		hst_glue(' ');
#endif
	status = set_mode_value(curbp, cp, defining, setting, global, args, rp);
	TRACE(("...adjvalueset(%s)=%d\n", cp, status))

	return status;
}

int
set_mode_value(BUFFER *bp, const char *cp, int defining, int setting, int global, VALARGS *args, const char *rp)
{
	const struct VALNAMES *names = args->names;
	struct VAL     *values = args->local;
	struct VAL     *globls = args->global;
	REGEXVAL *r;

	struct VAL oldvalue;
	int no = !strncmp(cp, "no", 2);
	int nval, status = TRUE;
	int unsetting = !setting && !global;
	int changed = FALSE;

	if (rp == NULL) {
		rp = no ? cp+2 : cp;
	} else {
		if (no && !is_bool_type(names->type))
			return FALSE;		/* this shouldn't happen */

		/*
		 * Check if we're allowed to change this mode in the current context.
		 */
		if (!defining
		 && (names->side_effect != 0)
		 && !(*(names->side_effect))(args, (values==globls), TRUE)) {
			return FALSE;
		}

#if defined(GMD_GLOB) || defined(GVAL_GLOB)
		if (!strcmp(names->name, "glob")
		 && !legal_glob_mode(rp))
			return FALSE;
#endif
#if OPT_ENUM_MODES
		(void) is_fsm(names);		/* evaluated for its side effects */
		if ((rp = legal_fsm(rp)) == 0)
			return FALSE;
#endif
		/* Test after fsm, to allow translation */
		if (is_bool_type(names->type)) {
			if (!string_to_bool(rp, &setting))
				return FALSE;
		}
	}

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
				changed = detach_mmode(bp, names->shortname);
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
					? detach_mmode(bp, names->shortname)
					: attach_mmode(bp, names->shortname);
			}
			break;
#endif
		case VALTYPE_BOOL:
			values->vp->i = no ? !setting : setting;
			break;

		case VALTYPE_ENUM:
#if OPT_ENUM_MODES
			{
				const FSM_CHOICES *fp = valname_to_choices(names);

				if (isDigit(*rp)) {
					if (!string_to_number(rp, &nval))
						return FALSE;
					if (choice_to_name(fp, nval) == 0)
						nval = ENUM_ILLEGAL;
				} else {
					nval = choice_to_code(fp, rp, strlen(rp));
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
			if ((r = new_regexval(rp, TRUE)) == 0) {
				values->vp->r = new_regexval("", TRUE);
				return FALSE;
			}
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

	if (!defining
	 && changed
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
		(const char *)&my_mode_list[0], sizeof(my_mode_list[0]));
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
find_mode(BUFFER *bp, const char *mode, int global, VALARGS *args)
{
	register const char *rp = !strncmp(mode, "no", 2) ? mode+2 : mode;
	register int	mode_class;
	register int	j;

	TRACE(("find_mode(%s) %s\n", mode, global ? "global" : "local"))

	for (mode_class = 0; mode_class < MODE_CLASSES; mode_class++) {
		memset(args, 0, sizeof(*args));
		switch (mode_class) {
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
				: ((bp != 0)
					? bp->b_values.bv
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
				: ((bp != 0)
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
				TRACE(("...found class %d %s\n", mode_class, rp))
#if OPT_MAJORMODE
				if (mode_class == 3) {
					char *it = (bp->majr != 0)
						? bp->majr->name
						: "?";
					make_global_val(args->local,args->global,0);
					if (global) {
						MAJORMODE_LIST *ptr =
							lookup_mm_list(it);
						args->local[0].v.i =
							(ptr != 0 && ptr->flag);
						;
					} else {
						char temp[NSTRING];
						majorname(temp, it, TRUE);
						make_local_val(args->local,0);
						args->local[0].v.i = !strcmp(temp, rp);
					}
				}
#endif
				return TRUE;
			}
		}
	}
#if OPT_MAJORMODE
	/* major submodes (buffers) */
	if (my_majormodes != 0) {
		int k = 0;
		size_t n = strlen(rp);

		for (j = 0; my_majormodes[j].name; j++) {
			MAJORMODE_LIST *p = my_majormodes+j;
			struct VAL *my_vals = get_sm_vals(p->data);
			size_t len = strlen(p->name);

			if (n >= len
			 && !strncmp(rp, p->name, len)
			 && (k = lookup_valnames(rp+len+(rp[len]=='-'), b_valnames)) >= 0
			 && is_local_val(my_vals,k)) {
				TRACE(("...found submode %s\n", b_valnames[k].name))
				if (global == FALSE) {
					if (bp != 0
					 && (bp->majr == 0
					  || strcmp(bp->majr->name, p->name))) {
						TRACE(("...not applicable\n"))
						return FALSE;
					}
					args->names  = b_valnames + k;
					args->global = my_vals + k;
					args->local  = ((bp != 0)
							? bp->b_values.bv + k
							: (struct VAL *)0);
				} else {
					args->names  = b_valnames + k;
					args->global = my_vals + k;
					args->local  = args->global;
				}
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
	static TBUFF *cbuf;	/* buffer to receive mode name into */

	/* prompt the user and get an answer */
	tb_scopy(&cbuf, "");
	if ((s = kbd_reply(
		global	? "Global value: "
			: "Local value: ",
		&cbuf,
		mode_eol, '=', KBD_NORMAL, mode_complete)) != TRUE)
		return ((s == FALSE) ? SORTOFTRUE : s);

	if (!strcmp(tb_values(cbuf), "all")) {
		hst_glue(' ');
		return listmodes(FALSE,1);
	}

	if ((s = find_mode(curbp, tb_values(cbuf), global, &args)) != TRUE) {
#if OPT_EVAL
		if (!global && (s = find_mode(curbp, tb_values(cbuf), TRUE, &args)) == TRUE) {
			mlforce("[Not a local mode: \"%s\"]", tb_values(cbuf));
			return FALSE;
		}
		return set_state_variable(tb_values(cbuf),NULL);
#else
		mlforce("[Not a legal set option: \"%s\"]", tb_values(cbuf));
#endif
	} else if ((s = adjvalueset(tb_values(cbuf), FALSE, kind, global, &args)) != 0) {
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
			term.setfore(gfcolor);
		else if (&args->local->vp->i == &gbcolor)
			term.setback(gbcolor);
		set_winflags(glob_vals, WFHARD|WFCOLR);
		vile_refresh(FALSE,0);
	}
	return TRUE;
}

#if OPT_ENUM_MODES || OPT_COLOR_SCHEMES
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
int set_colors(int n)
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
		my_colors = typecallocn(FSM_CHOICES,fsm_size(fsm_color_choices));
		my_hilite = typecallocn(FSM_CHOICES,fsm_size(fsm_hilite_choices));
		the_colors = my_colors;
		the_hilite = my_hilite;
		for (s = d = 0; fsm_color_choices[s].choice_name != 0; s++) {
			my_colors[d] = fsm_color_choices[s];
			if (my_colors[d].choice_code > 0) {
				if (!(my_colors[d].choice_code %= ncolors))
					continue;
			}
			if (strncmp(my_colors[d].choice_name, "bright", 6))
				d++;
		}
		my_colors[d].choice_name = 0;

		for (s = d = 0; fsm_hilite_choices[s].choice_name != 0; s++) {
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
	set_fsm_choice(s_fcolor, the_colors);
	set_fsm_choice(s_bcolor, the_colors);
	set_fsm_choice("visual-matches", the_hilite);
	set_fsm_choice("mini-hilite", the_hilite);
#endif /* OPT_ENUM_MODES */
	return TRUE;
}
#endif	/* OPT_COLOR */

#if OPT_SHOW_COLORS
const char *get_color_name(int n)
{
	static char temp[80];
#if OPT_COLOR_CHOICES
	int j;
	struct VALNAMES names;
	const FSM_CHOICES *the_colors;
	names.name = s_fcolor;
	the_colors = valname_to_choices(&names);
	for (j = 0; the_colors[j].choice_name != 0; j++) {
		if (n == the_colors[j].choice_code)
			return the_colors[j].choice_name;
	}
#endif
	lsprintf(temp, "color #%d", n);
	return temp;
}
#endif

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

	/* Change the "undoable" mode */
int
chgd_undoable(VALARGS *args, int glob_vals, int testing)
{
	if (chgd_major(args, glob_vals, testing)) {
		if (!testing
		 && !(args->local->v.i))
			freeundostacks(curbp,TRUE);
		return TRUE;
	}
	return FALSE;
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

char *
get_record_sep(BUFFER *bp)
{
	char *s = "";

	switch (b_val(bp, VAL_RECORD_SEP)) {
	case RS_LF:
		s = "\n";
		break;
	case RS_CRLF:
		s = "\r\n";
		break;
	case RS_CR:
		s = "\r";
		break;
	}

	return s;
}

void
set_record_sep(BUFFER *bp, RECORD_SEP value)
{
	set_b_val(bp, MDDOS, (value == RS_CRLF));
	set_b_val(bp, VAL_RECORD_SEP, value);
	b_clr_counted(bp);
	(void)bsizes(bp);
	relist_settings();
	updatelistbuffers();
}

	/* Change the record separator */
int
chgd_rs(VALARGS *args, int glob_vals, int testing)
{
	if (!testing) {
		if (curbp == 0)
			return FALSE;
		set_record_sep(curbp, args->local->vp->i);
	}

	return chgd_major_w(args, glob_vals, testing);
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
#if	OPT_XTERM
/*ARGSUSED*/
int
chgd_xterm( VALARGS *args GCC_UNUSED, int glob_vals, int testing GCC_UNUSED)
{
	if (glob_vals) {
		int	new_state = global_g_val(GMDXTERM_MOUSE);
		set_global_g_val(GMDXTERM_MOUSE,TRUE);
		if (!new_state)	term.kclose();
		else		term.kopen();
		set_global_g_val(GMDXTERM_MOUSE,new_state);
	}
	return TRUE;
}
#else
/*ARGSUSED*/
int
chgd_xterm( VALARGS *args GCC_UNUSED, int glob_vals GCC_UNUSED, int testing GCC_UNUSED)
{
	return TRUE;
}
#endif

	/* Change a mode that affects the search-string highlighting */
/*ARGSUSED*/
int
chgd_hilite(VALARGS *args GCC_UNUSED, int glob_vals GCC_UNUSED, int testing)
{
	if (!testing) {
		attrib_matches();
#if OPT_COLOR
		set_winflags(glob_vals, WFHARD|WFCOLR);
		vile_refresh(FALSE,0);
#endif
	}
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
 * Returns true if the given pointer is in all_modes[].  We use this to check
 * whether a name is in the original const array or not, so we can free it.
 */
static int
in_all_modes(const char *name)
{
	unsigned n;
	for (n = 0; all_modes[n] != 0; n++) {
		if (all_modes[n] == name)
			return TRUE;
	}
	return FALSE;
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
is_identifier (const char *name)
{
	int first = TRUE;;

	while (*name != EOS) {
		if (first) {
			if (!isAlpha(*name))
				return FALSE;
			first = FALSE;
		} else if (!isident(*name))
			return FALSE;
		name++;
	}
	return TRUE;
}

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
		dst = typeallocn(char, strlen(TheMajor) + strlen(name) + 3);
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

		TRACE(("insert_per_major %ld %s\n", (long) count, name))

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
				if (my_mode_list != all_modes
				 && !in_all_modes(my_mode_list[j])) {
					free(TYPECAST(char,my_mode_list[j]));
				}
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
check_majormode_name(const char *name, int defining)
{
	int status = TRUE;
	if (defining && lookup_mm_data(name) != 0) {
		TRACE(("Mode '%s' already exists\n", name))
		status = SORTOFTRUE;
	} else if (!defining && lookup_mm_data(name) == 0) {
		TRACE(("Mode '%s' does not exist\n", name))
		status = SORTOFTRUE;
	}
	return status;
}

int
major_complete(DONE_ARGS)
{
	return kbd_complete(FALSE, c, buf, pos, (const char *)&my_majormodes[0],
		sizeof(my_majormodes[0]));
}

static int
prompt_majormode(char **result, int defining)
{
	static TBUFF *cbuf;	/* buffer to receive mode name into */
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
		/* check for legal name (alphanumeric) */
		if ((status = is_identifier(tb_values(cbuf))) != TRUE) {
			mlwarn("[Not an identifier: %s]", tb_values(cbuf));
			return status;
		}
		if ((status = is_varmode(tb_values(cbuf))) == TRUE) {
			*result = tb_values(cbuf);
			status = check_majormode_name(*result, defining);
			if (status != TRUE && status != SORTOFTRUE)
				mlwarn("[No such majormode: %s]", *result);
			return status;
		}
	}
	if (status != FALSE)
		mlwarn("[Illegal name: %s]", tb_values(cbuf));
	return status;
}

static int
submode_complete(int c, char *buf, unsigned *pos)
{
	return kbd_complete(FALSE, c, buf, pos, (const char *)&all_submodes[0],
		sizeof(all_submodes[0]));
}

/*
 * Set the submode values to a known state
 */
static void
init_sm_vals (struct VAL *dst)
{
	int k;
	for (k = 0; k < MAX_B_VALUES; k++) {
		make_global_val(dst, global_b_values.bv, k);
	}
}

/*
 * Free data allocated in get_sm_vals().
 */
static void
free_sm_vals(MAJORMODE *ptr)
{
	MINORMODE *p;

	while (ptr->sm != 0) {
		p = ptr->sm;
		ptr->sm = p->sm_next;
		free_local_vals(b_valnames, global_b_values.bv, p->sm_vals.bv);
		free(p->sm_name);
		free(p);
	}
}

/*
 * Using the currently specified 'group' qualifier, lookup the corresponding
 * MINORMODE structure and return a pointer to the B_VALUES VALS data.  If
 * no structure is found, create one.
 *
 * We maintain the list of MINORMODE structures in the order they are defined.
 * This has the desirable side-effect of returning immediately for the default
 * "" group.
 */
static struct VAL *
get_sm_vals(MAJORMODE *ptr)
{
	MINORMODE *p, *q;
	char *name = ptr->mq.qv[QVAL_GROUP].vp->p;

	for (p = ptr->sm, q = 0; p != 0; q = p, p = p->sm_next) {
		if (!strcmp(name, p->sm_name)) {
			break;
		}
	}
	if (p == 0) {
		p = typecalloc(MINORMODE);
		p->sm_name = strmalloc(name);
		init_sm_vals(&(p->sm_vals.bv[0]));
		if (q != 0)
			q->sm_next = p;
		else
			ptr->sm = p;
	}
	TRACE(("...get_sm_vals(%s:%s)\n", ptr->name, p->sm_name))
	return &(p->sm_vals.bv[0]);
}

/*
 * Returns the name of the n'th submode group
 */
char *
get_submode_name(BUFFER *bp, int n)
{
	char *result = "";
	MAJORMODE *data;

	if (n == 0) {
		result = "";
	} else if ((data = bp->majr) != 0) {
		MINORMODE *sm = data->sm;
		while (n-- > 0) {
			if ((sm = sm->sm_next) == 0) {
				break;
			} else if (n == 0) {
				result = sm->sm_name;
				break;
			}
		}
	}
	return (result);
}

/*
 * Fetch a pointer to the n'th group of submode values.  The 0th group consists
 * of the normal buffer values.  Other values are defined in the majormode.
 *
 * FIXME: shouldn't I have a local submode value for the data which is defined
 * in a group?
 */
struct VAL *
get_submode_vals(BUFFER *bp, int n)
{
	struct VAL *result = 0;
	MAJORMODE *data;

	if (n == 0) {
		result = bp->b_values.bv;
	} else if ((data = bp->majr) != 0) {
		MINORMODE *sm = data->sm;
		while (n-- > 0) {
			if ((sm = sm->sm_next) == 0) {
				break;
			} else if (n == 0) {
				result = sm->sm_vals.bv;
				break;
			}
		}
	}
	return (result);
}

struct VAL *
get_submode_valx(BUFFER *bp, int n, int *m)
{
	struct VAL *result = 0;
	int next = *m + 1;

	if (next != n) {
		if ((result = get_submode_vals(bp, next)) == 0) {
			next = 0;
			if (next != n) {
				result = get_submode_vals(bp, next);
			}
		}

	}
	*m = next;
	return (result);
}

static int ok_subqual(MAJORMODE *ptr, char *name)
{
	VALARGS args;
	int j;

	if ((j = lookup_valnames(name, q_valnames)) >= 0) {
		args.names  = q_valnames;
		args.local  = ptr->mq.qv;
		args.global = global_m_values.mv;
	} else {
		return FALSE;
	}

	args.names  += j;
	args.global += j;
	args.local  += j;

	return adjvalueset(name, TRUE, TRUE, FALSE, &args);
}

static int
prompt_submode(MAJORMODE *ptr, char **result, int defining)
{
	static TBUFF *cbuf;	/* buffer to receive mode name into */
	register const char *rp;
	int status;

	/* prompt the user and get an answer */
	tb_scopy(&cbuf, "");
	while ((status = kbd_reply("submode: ",
		&cbuf,
		eol_history, '=',
		KBD_NORMAL,
		submode_complete)) == TRUE) {
		if ((status = ok_subqual(ptr, tb_values(cbuf))) == TRUE) {
			continue;
		} else if ((status = ok_submode(tb_values(cbuf))) == TRUE) {
			*result = tb_values(cbuf);
			rp = !strncmp(*result, "no", 2) ? *result+2 : *result;
			status = check_majormode_name(rp, defining);
		}
		break;
	}
	if (status != TRUE
	 && status != SORTOFTRUE)
		mlwarn("[Illegal submode name: %s]", tb_values(cbuf));
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
	VALARGS args;

	if (bp != 0) {
		if (bp->majr != 0
		 && strcmp(bp->majr->name, name) != 0)
			(void) detach_mmode(bp, bp->majr->name);

		TRACE(("attach_mmode '%s' to '%s'\n", name, bp->b_bname))
		if ((bp->majr = lookup_mm_data(name)) != 0) {
			struct VAL *mm = get_sm_vals(bp->majr);

			/* adjust buffer modes */
			for (n = 0; n < MAX_B_VALUES; n++) {
				if (!is_local_b_val(bp,n)
				 && is_local_val(mm,n)) {
					make_global_val(bp->b_values.bv, mm, n);
					if (b_valnames[n].side_effect != 0) {
						args.names = &(b_valnames[n]);
						args.local = &(bp->b_values.bv[n]);
						args.global = &mm[n];
						b_valnames[n].side_effect(&args,
						 			TRUE,
									FALSE);
					}
				} else if (n == MDDOS
				 && is_local_val(mm,n)
				 && !b_val(bp,n)) {
					/*
					 * This is a special case - we need a
					 * way to force vile to go back and
					 * strip the ^M's from the end of each
					 * line when reading a .bat file on
					 * a Unix system.
					 */
					set_dosmode(0,1);
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
	MAJORMODE *mp = 0;

	if (bp != 0
	 && (mp = bp->majr) != 0
	 && !strcmp(mp->name, name)) {
		TRACE(("detach_mmode '%s', given '%s'\n", name, mp->name))
		/* readjust the buffer's modes */
		for (n = 0; n < MAX_B_VALUES; n++) {
			if (!is_local_b_val(bp,n)
			 && is_local_val(get_sm_vals(mp),n)) {
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
	size_t j, k;
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
				free_local_vals(b_valnames, global_b_values.bv, get_sm_vals(ptr));
				for (k = 0; k < MAX_M_VALUES; k++) {
					free_val(m_valnames+k, my_majormodes[j].data->mm.mv+k);
					free(TYPECAST(char,my_majormodes[j].qual[k].name));
					free(TYPECAST(char,my_majormodes[j].qual[k].shortname));
				}
				for (k = 0; k < MAX_Q_VALUES; k++) {
					free_val(q_valnames+k, my_majormodes[j].data->mq.qv+k);
					free(TYPECAST(char,my_majormodes[j].subq[k].name));
					free(TYPECAST(char,my_majormodes[j].subq[k].shortname));
				}
				free_sm_vals(ptr);
				free(ptr->name);
				free(TYPECAST(char,ptr));
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
					free(TYPECAST(char,major_valnames[n].name));
					free(TYPECAST(char,major_valnames[n].shortname));
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
		my_mode_list = TYPECAST(const char *,all_modes);
}

static int
extend_mode_list(int increment)
{
	int j = count_modes();
	int k = increment + j + 1;

	TRACE(("extend_mode_list from %d by %d\n", j, increment))

	if (my_mode_list == all_modes) {
		my_mode_list = typeallocn(const char *, k);
		memcpy((char *)my_mode_list, all_modes, (j+1) * sizeof(*my_mode_list));
	} else {
		my_mode_list = typereallocn(const char *, my_mode_list, k);
	}
	return j;
}

static struct VAL *
extend_VAL_array(struct VAL *ptr, size_t item, size_t len)
{
	size_t j, k;

	TRACE(("extend_VAL_array %p item %ld of %ld\n", ptr, (long)item, (long)len))

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
	free_val(names, values);
	switch (names->type) {
	case VALTYPE_STRING:
		values->v.p = strmalloc(s);
		break;
	case VALTYPE_REGEX:
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
 * Reset/initialize all of the qualifiers.  We will use any that become set as
 * parameters for the current submode operation.
 */
static void
reset_sm_qualifiers(MAJORMODE *ptr)
{
	int k;
	for (k = 0; k < MAX_Q_VALUES; k++)
		reset_qualifier(q_valnames+k, ptr->mq.qv+k);
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
	MINORMODE *vals;

	if (my_majormodes != 0) {
		for (j = 0; my_majormodes[j].name != 0; j++) {
			if (local)
				TheMajor = my_majormodes[j].name;
			nflag = 0;
			data = my_majormodes[j].data;
			bprintf("--- \"%s\" majormode settings %*P\n",
				my_majormodes[j].name,
				term.cols-1, '-');
			nflag = listvalueset("Qualifier", FALSE, TRUE,
				m_valnames,
				data->mm.mv,
				data->mm.mv);
			for (vals = data->sm; vals != 0; vals = vals->sm_next) {
				char *group = vals->sm_name;
				char *name = (char *)malloc(80 + strlen(group));
				if (*group)
					lsprintf(name, "Buffer (\"%s\" group)", group);
				else
					strcpy(name, "Buffer");
				nflag = listvalueset(name, nflag, TRUE,
					b_valnames,
					vals->sm_vals.bv,
					global_b_values.bv);
				free(name);
			}
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
	major_valnames[j].name	      = strmalloc(temp);
	major_valnames[j].shortname   = strmalloc(name);
	major_valnames[j].type	      = VALTYPE_MAJOR;
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

	/* copy array to get types, then overwrite the name-pointers */
	memcpy(my_majormodes[j].subq, q_valnames, sizeof(q_valnames));
	for (k = 0; k < MAX_Q_VALUES; k++) {
		reset_qualifier(q_valnames+k, my_majormodes[j].data->mq.qv+k);
		my_majormodes[j].subq[k].name =
				strmalloc(q_valnames[k].name);
		my_majormodes[j].subq[k].shortname =
				strmalloc(q_valnames[k].shortname);
	}

	init_sm_vals(get_sm_vals(my_majormodes[j].data));

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

	ptr = lookup_mm_data(name);
	reset_sm_qualifiers(ptr);

	if ((status = prompt_submode(ptr, &subname, TRUE)) != TRUE) {
		reset_sm_qualifiers(ptr);
		return status;
	}

	rp = !strncmp(subname, "no", 2) ? subname+2 : subname;
	if ((j = lookup_valnames(rp, m_valnames)) >= 0) {
		qualifier   = TRUE;
		args.names  = m_valnames;
		args.local  = ptr->mm.mv;
		args.global = global_m_values.mv;
	} else if ((j = lookup_valnames(rp, b_valnames)) >= 0) {
		args.names  = b_valnames;
		args.local  = get_sm_vals(ptr);
		args.global = defining ? args.local : global_b_values.bv;
	} else {
		mlwarn("[BUG: no such submode: %s]", rp);
		reset_sm_qualifiers(ptr);
		return FALSE;
	}

	args.names  += j;
	args.global += j;
	args.local  += j;

	/*
	 * We store submodes in the majormode as local values.
	 */
	status = adjvalueset(subname, TRUE, defining, FALSE, &args);

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
	reset_sm_qualifiers(ptr);
	return status;
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

#if !OPT_CASELESS
/*
 * For the given majormode (by index into my_majormodes[]), return the
 * corresponding buffer mode value.
 */
static int
get_mm_b_val(int n, int m)
{
	struct VAL *bv = get_sm_vals(my_majormodes[n].data);
	return (bv[m].vp->i);
}
#endif

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
	if (my_majormodes != 0
	 && bp->b_fname != 0
	 && !isInternalName(bp->b_fname)) {
		size_t n = 0;
		int savecase = ignorecase;
#if OPT_CASELESS || SYS_VMS
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

		for (n = 0; my_majormodes[n].name != 0; n++) {
			if (my_majormodes[n].flag) {
				regexp *exp = get_mm_rexp(n, MVAL_PREAMBLE);
#if OPT_CASELESS || SYS_VMS
				ignorecase = TRUE;
#else
				ignorecase = get_mm_b_val(n, MDIGNCASE);
#endif
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
		struct VAL *q = get_sm_vals(p);
		q[n].v.i = value;
		make_local_val(q, n);
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

#if OPT_COLOR_SCHEMES
typedef struct {
	char *	name;
	UINT	code;
	int	fcol;
	int	bcol;
	int	attr;	/* bold, reverse, underline, etc. */
	char *	list;
} PALETTES;

static UINT current_scheme;
static UINT num_schemes;
static PALETTES *my_schemes;
static FSM_CHOICES *my_scheme_choices;

static int
set_scheme_color(const FSM_CHOICES *fp, int *d, char *s)
{
#if OPT_ENUM_MODES
	int nval;

	if (isDigit(*s)) {
		if (!string_to_number(s, &nval))
			return FALSE;
		if (choice_to_name(fp, nval) == 0)
			nval = ENUM_ILLEGAL;
	} else {
		nval = choice_to_code(fp, s, strlen(s));
	}
	*d = nval;
	return TRUE;
#else
	return string_to_number(s, d);
#endif
}

static void
set_scheme_string(char **d, char *s)
{
	if (*d)
		free(*d);
	*d = s ? strmalloc(s) : 0;
}

/*
 * Find a scheme
 */
static PALETTES *
find_scheme(const char *name)
{
	PALETTES *result = 0, *p;

	if ((p = my_schemes) != 0) {
		while (p->name != 0) {
			if (!strcmp(p->name, name)) {
				result = p;
				break;
			}
			p++;
		}
	}
	return result;
}

static PALETTES *
find_scheme_by_code(UINT code)
{
	PALETTES *result = 0, *p;

	if ((p = my_schemes) != 0) {
		while (p->name != 0) {
			if (p->code == code) {
				result = p;
				break;
			}
			p++;
		}
	}
	return result;
}

/*
 * Update the list of choices for color scheme
 */
static void
update_scheme_choices(void)
{
	int n;
	if (my_scheme_choices != 0) {
		my_scheme_choices = typereallocn(FSM_CHOICES, my_scheme_choices, num_schemes);
	} else {
		my_scheme_choices = typeallocn(FSM_CHOICES, num_schemes);
	}
	for (n = 0; n < (int) num_schemes; n++) {
		my_scheme_choices[n].choice_name = my_schemes[n].name;
		my_scheme_choices[n].choice_code = my_schemes[n].code;
	}
	set_fsm_choice(s_color_scheme, my_scheme_choices);
}

static int
same_string(char *p, char *q)
{
	if (p == 0) p = "";
	if (q == 0) q = "";
	return !strcmp(p, q);
}

/*
 * Set the current color scheme, given a pointer to its attributes
 */
static int
set_current_scheme(PALETTES *p)
{
	PALETTES *q = find_scheme_by_code(current_scheme);

	current_scheme = p->code;

	if (p->fcol != q->fcol
	 || p->bcol != q->bcol
	 || p->attr != q->attr
	 || !same_string(p->list, q->list)) {

		if (p->list != 0)
			set_palette(p->list);

		set_global_g_val(GVAL_FCOLOR,p->fcol);
		term.setfore(gfcolor);

		set_global_g_val(GVAL_BCOLOR,p->bcol);
		term.setback(gbcolor);

		set_global_g_val(GVAL_VIDEO,p->attr);

		return TRUE;
	}
	return FALSE;
}

/*
 * Remove a color scheme, except for the 'default' scheme.
 */
static int
free_scheme(char *name)
{
	PALETTES *p = 0;

	if (strcmp(name, s_default)
	 && (p = find_scheme(name)) != 0) {
		UINT code = p->code;

		free (p->name);
		if (p->list != 0) free (p->list);
		while (p->name != 0) {
			p[0] = p[1];
			p++;
		}
		num_schemes--;
		update_scheme_choices();

		if (code == current_scheme)
			(void) set_current_scheme(find_scheme(s_default));

		return TRUE;
	}
	return FALSE;
}

/*
 * Allocate a new scheme
 */
static PALETTES *
alloc_scheme(const char *name)
{
	static UINT code;
	PALETTES *result;

	if ((result = find_scheme(name)) == 0) {
		int len = ++num_schemes;
		if (len == 1) {
			len = ++num_schemes;
			my_schemes = typecallocn(PALETTES, len);
		} else {
			my_schemes = typereallocn(PALETTES, my_schemes, len);
		}
		len--;	/* point to list-terminator */
		my_schemes[len].name = 0;
		while (--len > 0) {
			my_schemes[len] = my_schemes[len-1];
			if (my_schemes[len-1].name != 0
			 && strcmp(my_schemes[len-1].name, name) < 0)
				break;
		}
		my_schemes[len].name = strmalloc(name);
		my_schemes[len].code = code++;	/* unique identifier */
		my_schemes[len].fcol = -1;
		my_schemes[len].bcol = -1;
		my_schemes[len].attr = 0;
		my_schemes[len].list = 0;
		result = &my_schemes[len];
		update_scheme_choices();
	}
	return result;
}

static int
scheme_complete(int c, char *buf, unsigned *pos)
{
	return kbd_complete(FALSE, c, buf, pos, (const char *)&my_schemes[0],
		sizeof(my_schemes[0]));
}

static int
prompt_scheme_name(char **result, int defining)
{
	static TBUFF *cbuf;	/* buffer to receive mode name into */
	int status;

	/* prompt the user and get an answer */
	tb_scopy(&cbuf, "");
	if ((status = kbd_reply("name: ",
		&cbuf,
		eol_history, ' ',
		KBD_NORMAL,	/* FIXME: KBD_MAYBEC if !defining */
		(defining || clexec)
			? no_completion
			: scheme_complete)) == TRUE) {
		/* check for legal name (alphanumeric) */
		if ((status = is_identifier(tb_values(cbuf))) != TRUE) {
			mlwarn("[Not an identifier: %s]", tb_values(cbuf));
			return status;
		}
		*result = tb_values(cbuf);
		return status;
	}
	return status;
}

/* this table must be sorted, since we do name-completion on it */
static const struct VALNAMES scheme_values[] = {
	{ s_bcolor,      s_bcolor,        VALTYPE_ENUM,     0 },
	{ s_fcolor,      s_fcolor,        VALTYPE_ENUM,     0 },
	{ s_palette,     s_palette,       VALTYPE_STRING,   0 },
	{ s_video_attrs, s_video_attrs,   VALTYPE_ENUM,     0 },
	{ 0,             0,               0,                0 }
	};

static int
scheme_value_complete(int c, char *buf, unsigned *pos)
{
	return kbd_complete(FALSE, c, buf, pos,
		(const char *)&scheme_values[0],
		sizeof(scheme_values[0]));
}

static int
prompt_scheme_value(PALETTES *p)
{
	static TBUFF *cbuf;	/* buffer to receive mode name into */
	int status;

	/* prompt the user and get an answer */
	tb_scopy(&cbuf, "");
	if ((status = kbd_reply("scheme value: ",
		&cbuf,
		eol_history, '=',
		KBD_NORMAL,	/* FIXME: KBD_MAYBEC if !defining */
		scheme_value_complete)) == TRUE) {
		char	respbuf[NFILEN];
		char *	name = tb_values(cbuf);
		int	code = *name;
		int	eolchar = (code == *s_palette) ? '\n' : ' ';
		int	(*complete) (DONE_ARGS) = no_completion;
#if OPT_ENUM_MODES
		const FSM_CHOICES *fp = 0;
		const struct VALNAMES *names;
		for (code = 0; scheme_values[code].name; code++) {
			if (!strcmp(name, scheme_values[code].name)) {
				names = &scheme_values[code];
				if (is_fsm(names))
					complete = fsm_complete;
				fp = valname_to_choices(names);
				break;
			}
		}
#else
		void *fp = 0;
#endif
		tb_sappend0(&cbuf, " value: ");
		*respbuf = EOS;
		status = kbd_string(name, respbuf, sizeof(respbuf), eolchar,
				KBD_NORMAL, complete);
		if (*name == *s_video_attrs) {
			status = set_scheme_color(fp, &(p->attr), respbuf);
		} else if (*name == *s_bcolor) {
			status = set_scheme_color(fp, &(p->bcol), respbuf);
		} else if (*name == *s_fcolor) {
			status = set_scheme_color(fp, &(p->fcol), respbuf);
		} else if (*name == *s_palette) {
			set_scheme_string(&(p->list), respbuf);
			return status;
		}
		if (status == TRUE)
			return (end_string() == ' ') ? -1 : TRUE;
	}
	return status;
}

#if OPT_UPBUFF
static int
update_schemelist(BUFFER *bp GCC_UNUSED)
{
	return desschemes(FALSE, 1);
}
#endif	/* OPT_UPBUFF */

/*
 * Define the default color scheme, based on the current settings of color
 * and $palette.
 */
void
init_scheme(void)
{
	PALETTES *p = alloc_scheme(s_default);
	char *list = tb_values(tb_curpalette);

	p->fcol = gfcolor;
	p->bcol = gbcolor;
	if (list != 0)
		p->list = strmalloc(list);
}

/*
 * Define a color scheme, e.g.,
 *	define-color-scheme {name} [fcolor={value}|bcolor={value}|{value}]
 * where the {value}'s are all color names or constants 0..NCOLORS-1.
 */
int
define_scheme(int f GCC_UNUSED, int n GCC_UNUSED)
{
	char *name;
	int status;

	if ((status = prompt_scheme_name(&name, TRUE)) != FALSE) {
		PALETTES *p = alloc_scheme(name);
		while ((status = prompt_scheme_value(p)) < 0)
			;
	}
	update_scratch(COLOR_SCHEMES_BufName, update_schemelist);
	return status;
}

/*
 * Remove a color scheme
 */
int
remove_scheme(int f GCC_UNUSED, int n GCC_UNUSED)
{
	char *name;
	int status;

	if (my_schemes == 0 || my_schemes->name == 0) {
		status = FALSE;
	} else if ((status = prompt_scheme_name(&name, FALSE)) != FALSE) {
		status = free_scheme(name);
	}
	update_scratch(COLOR_SCHEMES_BufName, update_schemelist);
	return status;
}

static void
makeschemelist(int dum1 GCC_UNUSED, void *ptr GCC_UNUSED)
{
	static const char fmt[] = "\n   %s=%s";
	PALETTES *p;
	int num = (num_schemes > 1) ? (num_schemes-1) : 0;

	bprintf("There %s %d color scheme%s defined",
		(num != 1) ? "are" : "is", num, PLURAL(num));

	for (p = my_schemes; p != 0 && p->name != 0; p++) {
		bprintf("\n\n%s", p->name);
		bprintf(fmt, s_fcolor, get_color_name(p->fcol));
		bprintf(fmt, s_bcolor, get_color_name(p->bcol));
		bprintf(fmt, s_video_attrs,
			choice_to_name(fsm_videoattrs_choices, p->attr));
		if (p->list != 0)
			bprintf(fmt, s_palette, p->list);
	}
}

int
desschemes(int f GCC_UNUSED, int n GCC_UNUSED)
{
	return liststuff(COLOR_SCHEMES_BufName, FALSE, makeschemelist, 0, (void *)0);
}

int
chgd_scheme(VALARGS *args, int glob_vals, int testing)
{
	if (!testing) {
		if (set_current_scheme(&my_schemes[args->local->vp->i])) {
			set_winflags(glob_vals, WFHARD|WFCOLR);
			vile_refresh(FALSE,0);
		}
	}
	return TRUE;
}
#endif /* OPT_COLOR_SCHEMES */

/*--------------------------------------------------------------------------*/
#if SYS_VMS
const char *
vms_record_format(int code)
{
	return choice_to_name (fsm_recordformat_choices, code);
}
#endif

/*--------------------------------------------------------------------------*/

#if NO_LEAKS
void
mode_leaks(void)
{
#if OPT_COLOR_SCHEMES
	while (my_schemes != 0 && my_schemes->name != 0)
		if (!free_scheme(my_schemes->name))
			break;
	if (my_schemes != 0) {	/* it's ok to free the default scheme */
		FreeAndNull(my_schemes->list);
		FreeAndNull(my_schemes->name);
		free(my_schemes);
	}
	FreeAndNull(my_scheme_choices);
#endif

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
	if (my_mode_list != all_modes
	 && my_mode_list != 0) {
		int j = count_modes();
		while (j > 0)
			remove_per_major(j--, my_mode_list[0]);
		FreeAndNull(my_mode_list);
	}
	FreeAndNull(major_valnames);
#endif
}
#endif /* NO_LEAKS */
