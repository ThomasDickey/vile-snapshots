/*
 *	eval.c -- function and variable evaluation
 *	original by Daniel Lawrence
 *
 * $Header: /users/source/archives/vile.vcs/RCS/eval.c,v 1.189 1999/03/27 14:20:31 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"
#include	"nevars.h"

#define	FUNC_NAMELEN	4

#define	ILLEGAL_NUM	-1
#define	MODE_NUM	-2
#define	USER_NUM	-3


#if OPT_EVAL

/* "generic" variable wrapper, to insulate some users of variables
 * from needing to distinguish the different types */
typedef struct	{
	int	v_type;	/* type of variable */
	int	v_num;	/* index, for state vars */
	UVAR *	v_ptr;	/* pointer, for temp vars */
	} VDESC;

static	SIZE_T	s2size ( char *s );
static	UVAR *	lookup_tempvar( const char *vname);
static	char *	s2offset ( char *s, char *n );
static	int	PromptAndSet ( const char *var, int f, int n );
static	int	SetVarValue ( VDESC *var, const char *value );
static	int	lookup_func(const char *name);
static	int	lookup_statevar(const char *vname);
static const char *get_tempvar_val(UVAR *p);
static const char *run_func(int fnum);
#endif

/*--------------------------------------------------------------------------*/


#if OPT_SHOW_EVAL
/* list the current vars into the current buffer */
/* ARGSUSED */
static void
makevarslist(int dum1 GCC_UNUSED, void *ptr)
{
	register UVAR *p;
	register int j;

	bprintf("--- State variables %*P\n", term.t_ncol-1, '-');
	bprintf("%s", (char *)ptr);
	for (p = temp_vars, j = 0; p != 0; p = p->next) {
		if (!j++)
			bprintf("--- Temporary variables %*P", term.t_ncol-1, '-');
		bprintf("\n%%%s = %s", p->u_name, p->u_value);
	}
}

/* Test a name to ensure that it is a mode-name, filtering out modish-stuff
 * such as "all" and "noview"
 */
static int
is_mode_name(const char *name, int showall, VALARGS *args)
{
	if (is_varmode(name)) {
		if (find_mode(curbp, name, -TRUE, args)) {
			if (!showall && !strcmp(name, args->names->shortname))
				return FALSE;
			return TRUE;
		}
		return SORTOFTRUE;
	}
	return FALSE;
}

/* filter out mode-names that apply only to abbreviations */
static const char *
get_listvalue(const char *name, int showall)
{
	VALARGS args;
	register int	s;
	int vnum;

	if ((s = is_mode_name(name, showall, &args)) == TRUE)
		return string_mode_val(&args);
	else if (s == SORTOFTRUE) {
		vnum = lookup_statevar(name);
		if (vnum != ILLEGAL_NUM)
		    return get_statevar_val(vnum);
	}
	return 0;
}
#endif /* OPT_SHOW_EVAL */

/* ARGSUSED */
#if OPT_SHOW_EVAL
int
listvars(int f, int n)
{
	char *values;
	register WINDOW *wp = curwp;
	register int s;
	register ALLOC_T t;
	register char *v;
	register const char *vv;
	static	const char fmt[] = { "$%s = %*S\n" };
	const char *const *Names = f ? list_of_modes() : statevars;
	int	showall = f ? (n > 1) : FALSE;

	/* collect data for state-variables, since some depend on window */
	for (s = t = 0; Names[s] != 0; s++) {
		if ((vv = get_listvalue(Names[s], showall)) != 0)
			t += strlen(Names[s]) + strlen(fmt) + strlen(vv);
	}
	if ((values = typeallocn(char, t)) == 0)
		return FALSE;

	for (s = 0, v = values; Names[s] != 0; s++) {
		if ((vv = get_listvalue(Names[s], showall)) != 0) {
			t = strlen(vv);
			if (t == 0) {
				t = 1;
				vv = "";
			} else if (vv[t-1] == '\n')
				t--;	/* suppress trailing newline */
			v = lsprintf(v, fmt, Names[s], t, vv);
		}
	}
	s = liststuff(VARIABLES_BufName, FALSE,
				makevarslist, 0, (void *)values);
	free(values);

	/* back to the buffer whose modes we just listed */
	swbuffer(wp->w_bufp);
	return s;
}
#endif /* OPT_SHOW_EVAL */

#if OPT_EVAL

/*
 * find a function in the function list
 */
static int
lookup_func(const char *name)
{
	char downcased[FUNC_NAMELEN];
	int fnum;

	if (!*name)
		return ILLEGAL_NUM;

	/* find the function -- truncate and case-convert it first */
	mklower(strncpy0(downcased, name, FUNC_NAMELEN));

	for (fnum = 0; fnum < NFUNCS; fnum++)
		if (strcmp(downcased, funcs[fnum].f_name) == 0)
			return fnum;

	return ILLEGAL_NUM;
}

/*
 * execute a builtin function
 */
static const char *
run_func(int fnum)
{
	char arg[3][NSTRING];			/* function arguments */
	int nums[3];
	int bools[3];
	static char result[2 * NSTRING];	/* function result */
	int i, nargs;
	int args_numeric, args_boolean, ret_numeric, ret_boolean;


	arg[0][0] = arg[1][0] = arg[2][0] = EOS;

	nargs = funcs[fnum].n_args & NARGMASK;
	args_numeric = funcs[fnum].n_args & NUM;
	args_boolean = funcs[fnum].n_args & BOOL;

	ret_numeric = funcs[fnum].n_args & NRET;
	ret_boolean = funcs[fnum].n_args & BRET;

	/* fetch required arguments */
	for (i = 0; i < nargs; i++) {
		if (mac_tokval(arg[i]) != TRUE)
			return(errorm);
		if (args_numeric)
			nums[i] = scan_int(arg[i]);
		else if (args_boolean)
			bools[i] = scan_bool(arg[i]);
	}


	switch (fnum) {
	case UFADD:
		i = nums[0] + nums[1];
		break;
	case UFSUB:
		i = nums[0] - nums[1];
		break;
	case UFTIMES:
		i = nums[0] * nums[1];
		break;
	case UFDIV:
		i = nums[0] / nums[1];
		break;
	case UFMOD:
		i = nums[0] % nums[1];
		break;
	case UFNEG:
		i = -nums[0];
		break;
	case UFCAT:
		strcat(strcpy(result, arg[0]), arg[1]);
		break;
	case UFLEFT:
		strncpy0(result, arg[0], s2size(arg[1])+1);
		break;
	case UFRIGHT:
		strcpy(result, s2offset(arg[0],arg[1]));
		break;
	case UFMID:
		strncpy0(result, s2offset(arg[0],arg[1]),
					    s2size(arg[2])+1);
		break;
	case UFNOT:
		i = !bools[0];
		break;
	case UFEQUAL:
		i = (nums[0] == nums[1]);
		break;
	case UFLESS:
		i = (nums[0] < nums[1]);
		break;
	case UFGREATER:
		i = (nums[0] > nums[1]);
		break;
	case UFSEQUAL:
		i = (strcmp(arg[0], arg[1]) == 0);
		break;
	case UFSLESS:
		i = (strcmp(arg[0], arg[1]) < 0);
		break;
	case UFSGREAT:
		i = (strcmp(arg[0], arg[1]) > 0);
		break;
	case UFIND:
		strcpy(result, tokval(arg[0]));
		break;
	case UFAND:
		i = (bools[0] && bools[1]);
		break;
	case UFOR:
		i = (bools[0] || bools[1]);
		break;
	case UFLENGTH:
		i = (int)strlen(arg[0]);
		break;
	case UFUPPER:
		mkupper(strcpy(result, arg[0]));
		break;
	case UFLOWER:
		mklower(strcpy(result, arg[0]));
		break;
	case UFTRIM:
		mktrimmed(result, arg[0]);
		break;
	case UFASCII:
		i = (int)arg[0][0];
		break;
	case UFCHR:
		result[0] = (char)nums[0];
		result[1] = EOS;
		break;
	case UFGTKEY:
		result[0] = (char)keystroke_raw8();
		result[1] = EOS;
		break;
	case UFGTSEQ:
		(void)kcod2escape_seq(kbd_seq_nomap(), result);
		break;
	case UFRND:
		i = rand() % absol(nums[0]);
		i++;  /* return 1 to N */
		break;
	case UFABS:
		i = absol(nums[0]);
		break;
	case UFSINDEX:
		i = ourstrstr(arg[0], arg[1], FALSE);
		break;
	case UFENV:
		strcpy(result, GetEnv(arg[0]));
		break;
	case UFBIND:
		strcpy(result, prc2engl(arg[0]));
		break;
	case UFREADABLE:
		i = (doglob(arg[0]) &&
			cfg_locate(arg[0], FL_CDIR|FL_READABLE) != NULL);
		break;
	case UFWRITABLE:
		i = (doglob(arg[0]) &&
			cfg_locate(arg[0], FL_CDIR|FL_WRITEABLE) != NULL);
		break;
	case UFLOCMODE:
	case UFGLOBMODE:
		{ VALARGS vargs;
		if (find_mode(curbp, arg[0],
				(fnum == UFGLOBMODE), &vargs) != TRUE)
			break;
		strcpy(result, string_mode_val(&vargs));
		}
		break;
	case UFQUERY:
		{ const char *cp;
		    cp = user_reply(arg[0]);
		    strcpy(result, cp ? cp : errorm);
		}
		break;
	default:
		strcpy(result, errorm);
		break;
	}

	if (ret_numeric)
		render_int(result, i);
	else if (ret_boolean)
		render_boolean(result, i);

	return result;
}

/* find a temp variable */
static UVAR *
lookup_tempvar(const char *name)
{
	register UVAR	*p;

	if (*name) {
		for (p = temp_vars; p != 0; p = p->next)
			if (!strcmp(name, p->u_name))
				return p;
	}
	return NULL;
}

/* get a temp variable's value */
static const char *
get_tempvar_val(UVAR *p)
{
	static char result[NSTRING];
	(void)strncpy0(result, p->u_value, NSTRING);
	return result;
}

/* find a state variable */
static int
lookup_statevar(const char *name)
{
	register int vnum;	/* ordinal number of var referenced */
	if (!*name)
		return ILLEGAL_NUM;

	/* scan the list, looking for the referenced name */
	for (vnum = 0; statevars[vnum] != 0; vnum++)
		if (strcmp(name, statevars[vnum]) == 0)
			return vnum;

	return ILLEGAL_NUM;
}


/* get a state variable's value */
char *
get_statevar_val(int vnum)
{
	static char result[NSTRING*2];
	int s;

	if (vnum == ILLEGAL_NUM)
		return errorm;

	s = (*statevar_func[vnum])(result, (const char *)NULL);

	if (s == TRUE)
		return result;
	else
		return errorm;

}

static void
FindVar(		/* find a variables type and name */
char *var,		/* name of var to get */
VDESC *vd)		/* structure to hold type and ptr */
{
	UVAR *p;

	vd->v_type = vd->v_num = ILLEGAL_NUM;
	vd->v_ptr = 0;

	if (!var[1]) {
		vd->v_type = ILLEGAL_NUM;
		return;
	}

	switch (var[0]) {
	case '$': /* check for legal state var */
		vd->v_num = lookup_statevar(var+1);
		if (vd->v_num != ILLEGAL_NUM)
			vd->v_type = TKENV;
#if !SMALLER
		 else {
			VALARGS	args;
			if (is_mode_name(&var[1],
				    TRUE, &args) == TRUE) {
				vd->v_num  = MODE_NUM;
				vd->v_type = TKENV;
			}
		}
#endif
		break;

	case '%': /* check for existing legal temp variable */
		vd->v_ptr = lookup_tempvar(var+1);
		if (vd->v_ptr) {
			vd->v_type = TKVAR;
			vd->v_num  = USER_NUM;
		} else {
			p = typealloc(UVAR);
			if (p &&
			    (p->u_name = strmalloc(var+1)) != 0) {
				p->next    = temp_vars;
				p->u_value = 0;
				temp_vars  = vd->v_ptr = p;
				vd->v_type = TKVAR;
				vd->v_num  = USER_NUM;
			}
		}
		break;

	case '&':	/* indirect operator? */
		if (strncmp(var, "&ind", FUNC_NAMELEN) == 0) {
			/* grab token, and eval it */
			execstr = get_token(execstr, var, EOS);
			(void)strcpy(var, tokval(var));
			FindVar(var,vd);  /* recursive, but probably safe */
		}
		break;
	}

}

/*
 * Handles name-completion for variables.  Note that since the first character
 * can be either a '$' or '%', we'll postpone name-completion until we've seen
 * a '$'.
 */
static int
vars_complete(
int	c,
char	*buf,
unsigned *pos)
{
	int	status;
	if (buf[0] == '$') {
		*pos -= 1;	/* account for leading '$', not in tables */
		status = kbd_complete(FALSE, c, buf+1, pos,
				(const char *)list_of_modes(), sizeof(char *));
		*pos += 1;
	} else {
		if (c != NAMEC) /* cancel the unget */
			unkeystroke(c);
		status = TRUE;
	}
	return status;
}

/*
 * this is the externally bindable command function.
 * assign a variable a new value -- any type of variable.
 */
int
setvar(int f, int n)
{
	int status;
	static TBUFF *var;

	/* first get the variable to set.. */
	if (var == 0)
		tb_scopy(&var, "");
	status = kbd_reply("Variable name: ", &var,
		mode_eol, '=', KBD_NOEVAL|KBD_LOWERC, vars_complete);
	if (status != TRUE)
		return(status);
	return PromptAndSet(tb_values(var), f, n);
}

static int
PromptAndSet(const char *name, int f, int n)
{
	int status;
	VDESC vd;
	char var[NLINE];
	char prompt[NLINE];
	char value[NLINE];

	/* look it up -- vd will describe the variable */
	FindVar(strcpy(var, name), &vd);

	if (vd.v_type == ILLEGAL_NUM) {
		mlforce("[Can't find variable '%s']", var);
		return(FALSE);
	} else if (vd.v_type == TKENV && vd.v_num == MODE_NUM) {
		VALARGS	args;
		(void)find_mode(curbp, var+1, -TRUE, &args);
		set_end_string('=');
		return adjvalueset(var+1, TRUE, -TRUE, &args);
	}

	if (f == TRUE) {  /* new (numeric) value passed as arg */
		(void)render_int(value, n);
	} else {  /* get it from user */
		value[0] = EOS;
		(void)lsprintf(prompt, "Value of %s: ", var);
		status = mlreply(prompt, value, sizeof(value));
		if (status != TRUE)
			return(status);
	}

	status = SetVarValue(&vd, value);

	if (status == ABORT)
		mlforce("[Variable %s is readonly]", var);
	else if (status != TRUE)
		mlforce("[Cannot set %s to %s]", var, value);

	return(status);
}

/*
 * Remove a temp variable.  That's the only type of variable that we can remove.
 */
int
rmv_tempvar(const char *name)
{
	register UVAR *p, *q;

	if (*name == '%')
		name++;

	for (p = temp_vars, q = 0; p != 0; q = p, p = p->next) {
		if (!strcmp(p->u_name, name)) {
			TRACE(("rmv_tempvar(%s) ok\n", name))
			if (q != 0)
				q->next = p->next;
			else
				temp_vars = p->next;
			free(p->u_value);
			free(p->u_name);
			free((char *)p);
			return TRUE;
		}
	}
	TRACE(("cannot rmv_tempvar(%s)\n", name))
	return FALSE;
}

/* entrypoint from modes.c and exec.c, to set state variables (one
 * interactive (from modes.c), one non-interactive) */
int
set_state_variable(const char *name, const char *value)
{
	int status;
	char var[NLINE];

	/* it may not be "marked" yet -- unadorned names are assumed to
	 *  be "state variables
	 */
	if (toktyp(name) == TKLIT) {
		var[0] = '$';
		strcpy(var+1, name);
	} else {
		strcpy(var, name);
	}

	if (value != NULL) { /* value supplied */
		VDESC vd;
		FindVar(var, &vd);
		if (vd.v_type == ILLEGAL_NUM)
			return FALSE;
		status = SetVarValue(&vd, value);
	} else { /* no value supplied:  interactive */
		status = PromptAndSet(var, FALSE, 0);
	}

	return status;
}


/* int set_statevar_val(int vnum, const char *value);  */

static int
set_statevar_val(int vnum, const char *value)
{
	return (*statevar_func[vnum])((char *)NULL, value);
}

/* figure out what type of variable we're setting, and do so */
static int
SetVarValue(
VDESC *var,		/* name */
const char *value)	/* value */
{
	int status;

	status = TRUE;
	switch (var->v_type) {
	case TKVAR:
		FreeIfNeeded(var->v_ptr->u_value);
		if ((var->v_ptr->u_value = strmalloc(value)) == 0)
			status = FALSE;
		break;

	case TKENV:
		status = set_statevar_val(var->v_num, value);
		break;
	}

#if	OPT_DEBUGMACROS
	if (macbug) {  /* we tracing macros? */
		if (var->v_type == TKENV)
			mlforce("(((%s:&%s:%s)))", render_boolean(rp, status),
					statevars[var->v_num], value);
		else if (var->v_type == TKENV)
			mlforce("(((%s:%%%s:%s)))", render_boolean(rp, status),
					var->v_ptr->u_name, value);

		(void)update(TRUE);

		/* make the user hit a key */
		if (ABORTED(keystroke())) {
			mlforce("[Aborted]");
			status = FALSE;
		}
	}
#endif
	return(status);
}
#endif

const char *
skip_cblanks(const char *src)
{
	while (isSpace(*src))
		src++;
	return src;
}

const char *
skip_cstring(const char *src)
{
	return src + strlen(src);
}

const char *
skip_ctext(const char *src)
{
	while (*src != EOS && !isSpace(*src))
		src++;
	return src;
}

/* If we've #define'd const, it's because it doesn't work.  Conversely, if
 * it's not #define'd, we really need distinct string functions.
 */
#ifndef const
char *
skip_blanks(char *src)
{
	while (isSpace(*src))
		src++;
	return src;
}

char *
skip_string(char *src)
{
	return src + strlen(src);
}

char *
skip_text(char *src)
{
	while (*src != EOS && !isSpace(*src))
		src++;
	return src;
}
#endif

#if OPT_COLOR
void
set_ctrans(const char *thePalette)
{
	int n = 0, value;
	char *next;

	while (*thePalette != EOS) {
		thePalette = skip_cblanks(thePalette);
		value = (int) strtol(thePalette, &next, 0);
		if (next == thePalette)
			break;
		thePalette = next;
		ctrans[n] = value;
		if (++n >= NCOLORS)
			break;
	}
}
#endif

/*
 * This function is used in several terminal drivers.
 */
#if OPT_EVAL || OPT_COLOR
int
set_palette(const char *value)
{
	tb_curpalette = tb_scopy(&tb_curpalette, value);
#if OPT_COLOR
	if (term.t_setpal != null_t_setpal) {
		TTspal(tb_values(tb_curpalette));
		vile_refresh(FALSE,0);
		return TRUE;
	}
	return FALSE;
#else
	return TRUE;
#endif
}
#endif

/* represent integer as string */
char *
render_int(char *rp, int i)		/* integer to translate to a string */
{
	(void)lsprintf(rp,"%d",i);
	return rp;
}

#if OPT_EVAL

/* represent boolean as string */
char *
render_boolean( char *rp, int val)
{
	static char *bools[] = { "FALSE", "TRUE" };
	return strcpy(rp, bools[val ? 1 : 0]);
}

#if (SYS_WINNT||SYS_VMS)
/* unsigned to hex */
char *
render_hex(char *rp, unsigned i)
{
	(void)lsprintf(rp,"%x",i);
	return rp;
}
#endif

/* pull an integer out of a string.  allow 'c' character constants as well */
int
scan_int(const char *s)
{
	char *ns;
	long l;

	if (s[0] == '\'' && s[2] == '\'' && s[3] == EOS)
		return s[1];

	l = strtol(s, &ns, 0);

	/* the endpointer better point at a 0, otherwise
	 * it wasn't all digits */
	if (*s && !*ns)
		return (int)l;

	return 0;
}

#endif

/* pull a boolean value out of a string */
int
scan_bool(const char *val)
{
	/* check for logical values */
	if (is_falsem(val))
		return(FALSE);
	if (is_truem(val))
		return(TRUE);

	/* check for numeric truth (!= 0) */
	return((strtol(val,0,0) != 0));
}

/* discriminate types of tokens */
int
toktyp(
const char *tokn)
{

	if (tokn[0] == EOS)     return TKNUL;
	if (tokn[0] == '"')     return TKSTR;
#if ! SMALLER
	if (tokn[1] == EOS)     return TKLIT; /* only one character */
	switch (tokn[0]) {
		case '~':       return TKDIR;
		case '@':       return TKARG;
		case '<':       return TKBUF;
		case '$':       return TKENV;
		case '%':       return TKVAR;
		case '&':       return TKFUN;
		case '*':       return TKLBL;

		default:        return TKLIT;
	}
#else
	return TKLIT;
#endif
}

#if OPT_EVAL

/* the argument simply represents itself */
static const char *
simple_arg_eval(const char *argp)
{
	return argp;
}

/* the returned value is the user's response to the specified prompt */
static const char *
interactive_arg_eval(const char *argp)
{
	argp = user_reply(tokval(argp+1));
	return argp ? argp : errorm;
}

/* the returned value is the next line from the named buffer */
static const char *
buffer_arg_eval(const char *argp)
{
	argp = next_buffer_line(tokval(argp+1));
	return argp ? argp : errorm;
}

/* expand it as a temp variable */
static const char *
tempvar_arg_eval(const char *argp)
{
	UVAR *p;
	p = lookup_tempvar(argp+1);
	return p ? get_tempvar_val(p) : errorm;
}

/* state variables are expanded.  if it's
 * not an statevar, perhaps it'a a mode?
 */
static const char *
statevar_arg_eval(const char *argp)
{
	int vnum;
	vnum = lookup_statevar(argp+1);
	if (vnum != ILLEGAL_NUM)
		return get_statevar_val(vnum);
#if !SMALLER
	{
	    VALARGS	args;
	    if (is_mode_name(argp+1, TRUE, &args) == TRUE)
		    return string_mode_val(&args);
	}
#endif
	return errorm;
}

/* run a function to evalute it */
static const char *
function_arg_eval(const char *argp)
{
	int fnum = lookup_func(argp+1);
	return (fnum != ILLEGAL_NUM) ? run_func(fnum) : errorm;
}

/* goto targets evaluate to their line number in the buffer */
static const char *
label_arg_eval(const char *argp)
{
	static char label[NSTRING];
	LINE *lp = label2lp(curbp, argp);

	if (lp)
		return render_int(label, line_no(curbp, lp));
	else
		return "0";
}

/* the value of a quoted string begins just past the leading quote */
static const char *
qstring_arg_eval(const char *argp)
{
	return argp + 1;
}


static const char *
directive_arg_eval(const char *argp)
{
#if SYS_UNIX
	static TBUFF *tkbuf;
	/* major hack alert -- the directives start with '~'.  on
	 * unix, we'd also like to be able to expand ~user as
	 * a home directory.  handily, these two uses don't
	 * conflict.  too much. */
	tb_alloc(&tkbuf, NFILEN);
	return(lengthen_path(strcpy(tb_values(tkbuf), argp)));
#else
	return errorm;
#endif
}


typedef const char * (ArgEvalFunc)(const char *argp);

ArgEvalFunc *eval_func[] = {
    simple_arg_eval,		/* TKNUL */
    interactive_arg_eval,	/* TKARG */
    buffer_arg_eval,		/* TKBUF */
    tempvar_arg_eval,		/* TKVAR */
    statevar_arg_eval,		/* TKENV */
    function_arg_eval,		/* TKFUN */
    directive_arg_eval,		/* TKDIR */
    label_arg_eval,		/* TKLBL */
    qstring_arg_eval,		/* TKSTR */
    simple_arg_eval		/* TKLIT */
};

#endif /* OPT_EVAL */

/* evaluate to the string-value of a token */
const char *
tokval(const char *tokn)
{
#if OPT_EVAL
	int toknum = toktyp(tokn);
	if (toknum < 0 || toknum > MAXTOKTYPE)
		return errorm;
	return (*eval_func[toknum])(tokn);
#else
	return (toktyp(tokn) == TKSTR) ? tokn+1 : tokn;
#endif
}

/*
 * Return true if the argument is one of the strings that we accept as a
 * synonym for "true".
 */
int
is_truem(const char *val)
{
	char	temp[8];
	(void)mklower(strncpy0(temp, val, sizeof(temp)));
	return (!strcmp(temp, "yes")
	   ||   !strcmp(temp, "true")
	   ||   !strcmp(temp, "t")
	   ||   !strcmp(temp, "y")
	   ||   !strcmp(temp, "on"));
}

/*
 * Return true if the argument is one of the strings that we accept as a
 * synonym for "false".
 */
int
is_falsem(const char *val)
{
	char	temp[8];
	(void)mklower(strncpy0(temp, val, sizeof(temp)));
	return (!strcmp(temp, "no")
	   ||   !strcmp(temp, "false")
	   ||   !strcmp(temp, "f")
	   ||   !strcmp(temp, "n")
	   ||   !strcmp(temp, "off"));
}

#if OPT_EVAL
/* use this as a wrapper when evaluating an array index, etc., that cannot
 * be negative.
 */
static SIZE_T
s2size(char *s)
{
	int	n = strtol(s,0,0);
	if (n < 0)
		n = 0;
	return n;
}

/* use this to return a pointer to the string after the n-1 first characters */
static char *
s2offset(char *s, char *n)
{
	SIZE_T	len = strlen(s) + 1;
	UINT	off = s2size(n);
	if (off > len)
		off = len;
	if (off == 0)
		off = 1;
	return s + (off - 1);
}

LINE *
label2lp (BUFFER *bp, const char *label)
{
	LINE *glp;
	LINE *result = 0;
	size_t len = strlen(label);

	if (len > 1) {
		for_each_line(glp, bp) {
			int need = len + 1;
			if (glp->l_used >= need
			 && glp->l_text[0] == '*'
			 && !memcmp(&glp->l_text[1], label, len)) {
				result = glp;
				break;
			}
		}
	}
	return(result);
}

#endif

#if OPT_EVAL || !SMALLER
char *
mkupper(char *s)
{
	char *sp;

	for (sp = s; *sp; sp++)
		if (isLower(*sp))
			*sp = toUpper(*sp);

	return(s);
}
#endif

char *
mklower(char *s)
{
	char *sp;

	for (sp = s; *sp; sp++)
		if (isUpper(*sp))
			*sp = toLower(*sp);

	return(s);
}

char *
mktrimmed(char *rp, char *str)	/* trim whitespace */
{
	register char *dst = rp;

	while (*str != EOS) {
		if (isSpace(*str)) {
			if (dst != rp)
				*dst++ = ' ';
			str = skip_blanks(str);
		} else {
			*dst++ = *str++;
		}
	}
	if (dst != rp && isSpace(dst[-1]))
		dst--;
	*dst = EOS;
	return rp;
}

int
absol(int x)		/* take the absolute value of an integer */
{
	return(x < 0 ? -x : x);
}
