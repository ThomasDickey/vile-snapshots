/*
 *	eval.c -- function and variable evaluation
 *	original by Daniel Lawrence
 *
 * $Header: /users/source/archives/vile.vcs/RCS/eval.c,v 1.266 2000/02/27 23:53:51 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"
#include	"nevars.h"
#include	"nefsms.h"

#include	<ctype.h>

#define	ILLEGAL_NUM	-1


#if OPT_EVAL

/* "generic" variable wrapper, to insulate some users of variables
 * from needing to distinguish the different types */
typedef struct	{
	int	v_type;	/* type: values below */
	int	v_num;	/* index, for state vars */
	UVAR *	v_ptr;	/* pointer, for temp vars */
	} VWRAP;
/* values for v_type */
#define VW_NOVAR	0
#define VW_STATEVAR	1
#define VW_TEMPVAR	2
#define VW_MODE		3

typedef struct PROC_ARGS {
	struct	PROC_ARGS *nxt_args;
	int	num_args;	/* total argument count */
	TBUFF **all_args;
} PROC_ARGS;

static PROC_ARGS *arg_stack;

static	SIZE_T	s2size ( char *s );
static	char *	s2offset ( char *s, char *n );
static	int	PromptAndSet ( const char *var, int f, int n );
static	int	SetVarValue ( VWRAP *var, const char *value );
static	int	lookup_statevar(const char *vname);
#endif


/*--------------------------------------------------------------------------*/
#if OPT_SHOW_CTYPE

/* list the current character-classes into the current buffer */
/* ARGSUSED */
static void
makectypelist(int dum1 GCC_UNUSED, void *ptr GCC_UNUSED)
{
	UINT i, j;
	CHARTYPE k;
	const char *s;

	bprintf("--- Printable Characters %*P\n", term.cols-1, '-');
	for (i = 0; i < N_chars; i++) {
		bprintf("\n%d\t", i);
		if ((i == '\n') || (i == '\t')) /* vtlistc() may not do these */
			bprintf("^%c", '@' | i);
#if OPT_LOCALE
		else if (!isprint(i) && i > 127 && i < 160) /* C1 controls? */
			bprintf(
				global_w_val(WMDNONPRINTOCTAL)
				? "\\%3o"
				: "\\x%2x",
				i);
#endif
		else
			bprintf("%c", i);
		bputc('\t');
		for (j = 0; j != vl_UNUSED; j++) {
			if ((s = choice_to_name(fsm_charclass_choices, j)) != 0) {
				k = (1 << j);
				if (j != 0)
					bputc(' ');
				bprintf("%*s",
					strlen(s),
					(vl_chartypes_[i] & k)
						? s
						: "-");
			}
		}
	}
}

/* ARGSUSED */
static int
show_CharClasses(BUFFER *bp GCC_UNUSED)
{
	return liststuff(PRINTABLECHARS_BufName, FALSE, makectypelist, 0, (void *)0);
}

#if OPT_UPBUFF
static void
update_char_classes(void)
{
	update_scratch(PRINTABLECHARS_BufName, show_CharClasses);
}
#else
#define update_char_classes() /*nothing*/
#endif

/* ARGSUSED */
int
desprint(int f GCC_UNUSED, int n GCC_UNUSED)
{
	return show_CharClasses(curbp);
}

static int
cclass_complete(int c, char *buf, unsigned *pos)
{
	return kbd_complete(FALSE, c, buf, pos,
		(const char *)fsm_charclass_choices,
		sizeof(fsm_charclass_choices[0]));
}

/*
 * Using name-completion, we can only accept one name at a time - so the
 * set/unset operations do not take a list.
 */
static int
get_charclass_code(void)
{
	static TBUFF *var;
	int status;

	if (var == 0)
		tb_scopy(&var, "");
	status = kbd_reply("Character class: ", &var,
		eol_history, '=', KBD_NOEVAL|KBD_LOWERC, cclass_complete);
	return (status == TRUE)
		? choice_to_code(
			fsm_charclass_choices,
			tb_values(var),
			tb_length(var))
		: -1;
}

/*
 * Prompt for a regular expression that we can use to match single characters.
 * Actually, regexp won't really tell us that much - so we'll get an expression
 * and try it.
 */
static REGEXVAL *
get_charclass_regexp(void)
{
	static TBUFF *var;
	int status;

	if (var == 0)
		tb_scopy(&var, "");
	status = kbd_reply("Character pattern: ", &var,
		eol_history, '=', KBD_NOEVAL, no_completion);
	return (status == TRUE)
		? new_regexval(
			tb_values(var),
			TRUE)
		: 0;
}

static int
match_charclass_regexp(int ch, REGEXVAL *exp)
{
	char temp[2];
	temp[0] = ch;

	return regexec(exp->reg, temp, temp+1, 0, 0);
}

static int
update_charclasses(char *tag, int count)
{
	if (count) {
		mlwrite("[%s %d character%s]", tag, count, PLURAL(count));
		update_char_classes();
	} else {
		mlwrite("[%s no characters]", tag);
	}
	return TRUE;
}

/* ARGSUSED */
int
set_charclass(int f GCC_UNUSED, int n GCC_UNUSED)
{
	int ch;
	int code;
	int count;
	REGEXVAL *exp;

	if ((code = get_charclass_code()) > 0) {
		code = (1 << code);
		if ((exp = get_charclass_regexp()) != 0) {
			for (count = 0, ch = 0; ch < N_chars; ch++) {
				if (!istype(code, ch)
				 && match_charclass_regexp(ch, exp)) {
					vl_chartypes_[ch] |= code;
					count++;
				}
			}
			free_regexval(exp);
			return update_charclasses("Set", count);
		}
	}
	return FALSE;
}

/* ARGSUSED */
int
unset_charclass(int f GCC_UNUSED, int n GCC_UNUSED)
{
	int ch;
	int code;
	int count;
	REGEXVAL *exp;

	if ((code = get_charclass_code()) > 0) {
		code = (1 << code);
		if ((exp = get_charclass_regexp()) != 0) {
			for (count = 0, ch = 0; ch < N_chars; ch++) {
				if (istype(code, ch)
				 && match_charclass_regexp(ch, exp)) {
					vl_chartypes_[ch] &= ~code;
					count++;
				}
			}
			free_regexval(exp);
			return update_charclasses("Unset", count);
		}
	}
	return FALSE;
}

/* ARGSUSED */
int
reset_charclasses(int f GCC_UNUSED, int n GCC_UNUSED)
{
	charinit();
	update_char_classes();
	return TRUE;
}

#endif /* OPT_SHOW_CTYPE */

/*
 * Compute the intersection of the character classes in the argument.
 */
static CHARTYPE
charclass_of(char *arg)
{
	CHARTYPE k = vl_chartypes_[char2int(*arg)];
	if (*arg) {
		while (*++arg) {
			k &= vl_chartypes_[char2int(*arg)];
		}
	}
	return k;
}

/*
 * Make display of the intersection of the character classes in the argument.
 */
static void
show_charclass(TBUFF **result, char *arg)
{
	CHARTYPE k = charclass_of(arg);
#if OPT_SHOW_CTYPE
	UINT j;
	const char *s;

	for (j = 0; j != vl_UNUSED; j++) {
		if (((1 << j) & k) != 0
		 && (s = choice_to_name(fsm_charclass_choices, j)) != 0) {
			if (tb_length(*result))
				tb_sappend0(result, "+");
			tb_sappend0(result, s);
		}
	}
#else /* well, show something, so we can compare against it */
	lsprintf(tb_values(*result), "%X", k);
#endif
}

/*--------------------------------------------------------------------------*/


#if OPT_SHOW_EVAL
/* list the current vars into the current buffer */
/* ARGSUSED */
static void
makevarslist(int dum1 GCC_UNUSED, void *ptr)
{
	UVAR *p;
	int j, k;

	bprintf("--- State variables %*P\n", term.cols-1, '-');
	bprintf("%s", (char *)ptr);
	if (arg_stack != 0 && ((k = arg_stack->num_args) != 0)) {
		bprintf("--- %s parameters %*P",
			tb_values(arg_stack->all_args[0]),
			term.cols-1, '-');
		for (j = 1; j <= k; j++) {
			bprintf("\n$%d = %s", j,
				tb_values(arg_stack->all_args[j]));
		}
	}
	for (p = temp_vars, j = 0; p != 0; p = p->next) {
		if (!j++)
			bprintf("--- Temporary variables %*P", term.cols-1, '-');
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

#if OPT_SHOW_EVAL

static int show_vars_f;
static int show_vars_n;

/* ARGSUSED */
static int
show_VariableList(BUFFER *bp GCC_UNUSED)
{
	char *values;
	register WINDOW *wp = curwp;
	register int s;
	register ALLOC_T t;
	register char *v;
	register const char *vv;
	static	const char fmt[] = { "$%s = %*S\n" };
	const char *const *Names = show_vars_f ? list_of_modes() : statevars;
	int	showall = show_vars_f ? (show_vars_n > 1) : FALSE;

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

#if OPT_UPBUFF
/*
 * If the list-variables window is visible, update it after operations that
 * would modify the list.
 */
static void
updatelistvariables(void)
{
	update_scratch(VARIABLES_BufName, show_VariableList);
}
#else
#define updatelistvariables() /*nothing*/
#endif

int
listvars(int f, int n)
{
	show_vars_f = f;
	show_vars_n = n;
	return show_VariableList(curbp);
}
#endif /* OPT_SHOW_EVAL */

#if OPT_EVAL

/*
 * Find a function in the function list.  The table is all lowercase, so we can
 * compare ignoring case.  Check for any unique abbreviation of the table's
 * names.
 */
static int
lookup_func(char *name)
{
    char downcased[NSTRING];
    int fnum = ILLEGAL_NUM;
    int n;
    unsigned m;

    TRACE(("lookup_func(%s) ", name));
    if (*name++ == '&'
     && *name != EOS) {

	mklower(vl_strncpy(downcased, name, sizeof(downcased)));

	m = strlen(downcased);
	for (n = 0; n < NFUNCS; n++) {
	    if (!strncmp(downcased, vl_ufuncs[n].f_name, m)) {
		if ((n+1 >= NFUNCS
		  || strncmp(downcased, vl_ufuncs[n+1].f_name, m)))
		    fnum = n;
		break;
	    }
	}
    }
    TRACE(("-> %d\n", fnum));
    return fnum;
}

/*
 * Find the n'th token, counting from zero.  This will return an empty
 * result only if we run past the end of the number of tokens.
 */
static void
extract_token(TBUFF **result, char *count, char *delims, char *string)
{
	int num = scan_int(count);

	while (num-- > 0) {
		string += strspn(string, delims);
		string += strcspn(string, delims);
	}

	string += strspn(string, delims);
	string[strcspn(string, delims)] = EOS;
	tb_scopy(result, string);
}

static void
path_trim(char *path)
{
#if SYS_UNIX
	while ((int)strlen(path) > 1) {
		char *last = path + strlen(path) - 1;
		if (is_slashc(*last))
			*last = EOS;
		else
			break;
	}
#else
#if OPT_MSDOS_PATH
	while (strlen(path) > 2) {
		char *last = path + strlen(path) - 1;
		if (is_slashc(*last)
		 && last[-1] != ':')
			*last = EOS;
		else
			break;
	}
#endif
#endif
}

/*
 * Extract the head of the given path
 */
static void
path_head(TBUFF **result, char *path)
{
	path = tb_values(tb_scopy(result, path));
	*pathleaf(path) = EOS;
	path_trim(path);
}

/*
 * If the given argument contains non-path characters, quote it.  Otherwise
 * simply copy it.
 */
static void
path_quote(TBUFF **result, char *path)
{
#if SYS_VMS
	tb_scopy(result, path);		/* no point in quoting here */
#else
	CHARTYPE have = charclass_of(path);
	CHARTYPE want = (vl_pathn | vl_nonspace);

	if ((want & have) != want) {
		tb_sappend(result, "\"");
#if SYS_UNIX
#define QUOTE_UNIX "\"\\$"
		if (strpbrk(path, QUOTE_UNIX)) {
		    while (*path) {
			if (strchr(QUOTE_UNIX, *path))
			    tb_append(result, BACKSLASH);
			tb_append(result, *path++);
		    }
		} else
#endif
		    tb_sappend(result, path);
		tb_sappend0(result, "\"");
	} else {
		tb_scopy(result, path);
	}
#endif
}

#define MAXARGS 3

/*
 * execute a builtin function
 */
static char *
run_func(int fnum)
{
	static TBUFF *result;		/* function result */

	TBUFF *args[MAXARGS];
	char  *arg[MAXARGS];		/* function arguments */
	char *cp;
	int nums[MAXARGS];
	int bools[MAXARGS];
	int i, nargs;
	int args_numeric, args_boolean, ret_numeric, ret_boolean;

	nargs = vl_ufuncs[fnum].f_code & NARGMASK;
	args_numeric = vl_ufuncs[fnum].f_code & NUM;
	args_boolean = vl_ufuncs[fnum].f_code & BOOL;

	ret_numeric = vl_ufuncs[fnum].f_code & NRET;
	ret_boolean = vl_ufuncs[fnum].f_code & BRET;

	TRACE(("evaluate '%s' (%#x), %d args\n",
			vl_ufuncs[fnum].f_name,
			vl_ufuncs[fnum].f_code,
			nargs));

	/* fetch required arguments */
	for (i = 0; i < nargs; i++) {
		args[i] = 0;
		if ((arg[i] = mac_tokval(&args[i])) == 0)
			return error_val;
		tb_free(&result); /* in case mac_tokval() called us */
		TRACE(("...arg[%d] = '%s'\n", i, arg[i]));
		if (args_numeric)
			nums[i] = scan_int(arg[i]);
		else if (args_boolean)
			bools[i] = scan_bool(arg[i]);
	}

	tb_init(&result, EOS);

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
		tb_scopy(&result, arg[0]);
		tb_sappend0(&result, arg[1]);
		break;
	case UFCCLASS:
		show_charclass(&result, arg[0]);
		break;
	case UFLEFT:
		tb_bappend(&result, arg[0], s2size(arg[1]));
		tb_append(&result, EOS);
		break;
	case UFRIGHT:
		tb_scopy(&result, s2offset(arg[0], arg[1]));
		break;
	case UFMID:
		tb_bappend(&result, s2offset(arg[0], arg[1]),
				    s2size(arg[2]));
		tb_append(&result, EOS);
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
	case UFINDIRECT:
		tb_scopy(&result, tokval(arg[0]));
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
		mkupper(tb_values(tb_scopy(&result, arg[0])));
		break;
	case UFLOWER:
		mklower(tb_values(tb_scopy(&result, arg[0])));
		break;
	case UFTRIM:
		mktrimmed(tb_values(tb_scopy(&result, arg[0])));
		break;
	case UFASCII:
		i = (int)arg[0][0];
		break;
	case UFCHR:
		tb_append(&result, (char)nums[0]);
		tb_append(&result, EOS);
		break;
	case UFGTKEY:
		tb_append(&result, (char)keystroke_raw8());
		tb_append(&result, EOS);
		break;
	case UFGTSEQ:
		(void)kcod2escape_seq(kbd_seq_nomap(), tb_values(result));
		result->tb_used = strlen(tb_values(result));
		break;
	case UFRANDOM: /* FALLTHRU */
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
		tb_scopy(&result, vile_getenv(arg[0]));
		break;
	case UFBIND:
		tb_scopy(&result, prc2engl(arg[0]));
		break;
	case UFREADABLE: /* FALLTHRU */
	case UFRD:
		i = (doglob(arg[0]) &&
			cfg_locate(arg[0], FL_CDIR|FL_READABLE) != NULL);
		break;
	case UFWRITABLE:
		i = (doglob(arg[0]) &&
			cfg_locate(arg[0], FL_CDIR|FL_WRITEABLE) != NULL);
		break;
	case UFEXECABLE:
		i = (doglob(arg[0]) &&
			cfg_locate(arg[0], FL_CDIR|FL_EXECABLE) != NULL);
		break;
	case UFLOCMODE:
	case UFGLOBMODE:
		{ VALARGS vargs;
		if (find_mode(curbp, arg[0],
				(fnum == UFGLOBMODE), &vargs) != TRUE)
			break;
		tb_scopy(&result, string_mode_val(&vargs));
		}
		break;
	case UFQUERY:
		cp = user_reply(arg[0]);
		tb_scopy(&result, cp ? cp : error_val);
		break;
	case UFLOOKUP:
		if ((i = combine_choices(fsm_lookup_choices, arg[0])) > 0)
			tb_scopy(&result, cfg_locate(arg[1], i));
		break;
	case UFPATH:
		switch (choice_to_code(fsm_path_choices, arg[0], strlen(arg[0]))) {
		case PATH_END:
			cp = pathleaf(arg[1]);
			if ((cp = strchr(cp, '.')) != 0)
				tb_scopy(&result, cp);
			break;
		case PATH_FULL:
			tb_scopy(&result, arg[1]);
			tb_alloc(&result, NFILEN);
			lengthen_path(tb_values(result));
			break;
		case PATH_HEAD:
			path_head(&result, arg[1]);
			break;
		case PATH_ROOT:
			tb_scopy(&result, pathleaf(arg[1]));
			if ((cp = strchr(tb_values(result), '.')) != 0)
				*cp = EOS;
			break;
		case PATH_SHORT:
			tb_scopy(&result, arg[1]);
			tb_alloc(&result, NFILEN);
			shorten_path(tb_values(result), FALSE);
			break;
		case PATH_TAIL:
			tb_scopy(&result, pathleaf(arg[1]));
			break;
		default:
			tb_scopy(&result, error_val);
			break;
		}
		break;
	case UFPATHCAT:
		tb_alloc(&result, NFILEN);
		pathcat(tb_values(result), arg[0], arg[1]);
		break;
	case UFPATHQUOTE:
		path_quote(&result, arg[0]);
		break;
	case UFTOKEN:
		extract_token(&result, arg[0], arg[1], arg[2]);
		break;
	case UFWORD:
		extract_token(&result, arg[0], "\t ", arg[1]);
		break;
	case UFREGISTER:
		i = reg2index(*arg[0]);
		if ((i = index2ukb(i)) < NKREGS
		 && i >= 0) {
			KILL *kp = kbs[i].kbufh;
			while (kp != 0) {
				tb_bappend(&result,
					   (char *)(kp->d_chunk),
					   KbSize(i,kp));
				kp = kp->d_next;
			}
			tb_append(&result, EOS);
			break;
		}
		/* FALLTHRU */
	default:
		TRACE(("unknown function #%d\n", fnum));
		tb_scopy(&result, error_val);
		break;
	}

	if (ret_numeric)
		render_int(&result, i);
	else if (ret_boolean)
		render_boolean(&result, i);

	TRACE(("-> '%s'\n", tb_values(result)));
	for (i = 0; i < nargs; i++) {
		tb_free(&args[i]);
	}
	return tb_values(result);
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
	static TBUFF *result;
	int s;

	if (vnum == ILLEGAL_NUM)
		return error_val;

	tb_init(&result, EOS);
	s = (*statevar_func[vnum])(&result, (const char *)NULL);

	if (s == TRUE)
		return tb_values(result);
	else
		return error_val;

}

static void
FindVar(		/* find a variables type and name */
char *var,		/* name of var to get */
VWRAP *vd)		/* structure to hold type and ptr */
{
	UVAR *p;

	vd->v_type = VW_NOVAR;
	vd->v_num = ILLEGAL_NUM;
	vd->v_ptr = 0;

	if (!var[1]) {
		vd->v_type = VW_NOVAR;
		return;
	}

	switch (var[0]) {
	case '$': /* check for legal state var */
		vd->v_num = lookup_statevar(var+1);
		if (vd->v_num != ILLEGAL_NUM)
			vd->v_type = VW_STATEVAR;
#if !SMALLER
		 else {
			VALARGS	args;
			if (is_mode_name(&var[1], TRUE, &args) == TRUE) {
				vd->v_type = VW_MODE;
			}
		}
#endif
		break;

	case '%': /* check for legal temp variable */
		vd->v_ptr = lookup_tempvar(var+1);
		if (vd->v_ptr) {  /* existing */
			vd->v_type = VW_TEMPVAR;
		} else {  /* new */
			p = typealloc(UVAR);
			if (p &&
			    (p->u_name = strmalloc(var+1)) != 0) {
				p->next    = temp_vars;
				p->u_value = 0;
				temp_vars  = vd->v_ptr = p;
				vd->v_type = VW_TEMPVAR;
			}
		}
		break;

	case '&':	/* indirect operator? */
		if (lookup_func(var) == UFINDIRECT) {
			TBUFF *tok = 0;
			/* grab token, and eval it */
			execstr = get_token(execstr, &tok, EOS);
			(void)vl_strncpy(var, tokval(tb_values(tok)), NLINE);
			FindVar(var,vd);  /* recursive, but probably safe */
			tb_free(&tok);
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

	/* get the variable we want to set */
	if (var == 0)
		tb_scopy(&var, "");
	status = kbd_reply("Variable name: ", &var,
		mode_eol, '=', KBD_NOEVAL|KBD_LOWERC, vars_complete);
	if (status == TRUE)
		status = PromptAndSet(tb_values(var), f, n);
	updatelistvariables();
	return status;
}

static int
PromptAndSet(const char *name, int f, int n)
{
	int status;
	VWRAP vd;
	char var[NLINE];
	char prompt[NLINE];
	char value[NLINE];

	/* look it up -- vd will describe the variable */
	FindVar(vl_strncpy(var, name, sizeof(var)), &vd);

	if (vd.v_type == VW_NOVAR) {
		mlforce("[Can't find variable '%s']", var);
		status = FALSE;
	} else if (vd.v_type == VW_MODE) {
		VALARGS	args;
		(void)find_mode(curbp, var+1, -TRUE, &args);
		set_end_string('=');
		status = adjvalueset(var+1, FALSE, TRUE, -TRUE, &args);
	} else {
		if (f == TRUE) {  /* new (numeric) value passed as arg */
			static TBUFF *tmp;
			(void)vl_strncpy(value, render_int(&tmp, n), sizeof(value));
		} else {  /* get it from user */
			value[0] = EOS;
			(void)lsprintf(prompt, "Value of %s: ", var);
			status = mlreply(prompt, value, sizeof(value));
			if (status != TRUE)
				return status;
		}

		status = SetVarValue(&vd, value);

		if (status == ABORT) {
			mlforce("[Variable %s is readonly]", var);
		} else if (status != TRUE) {
			mlforce("[Cannot set %s to %s]", var, value);
		}
	}

	return status;
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
			TRACE(("rmv_tempvar(%s) ok\n", name));
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
	TRACE(("cannot rmv_tempvar(%s)\n", name));
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
	if (toktyp(name) == TOK_LITSTR) {
		var[0] = '$';
		vl_strncpy(var+1, name, sizeof(var)-1);
	} else {
		vl_strncpy(var, name, sizeof(var));
	}

	if (value != NULL) { /* value supplied */
		VWRAP vd;
		FindVar(var, &vd);
		status = SetVarValue(&vd, value);
	} else { /* no value supplied:  interactive */
		status = PromptAndSet(var, FALSE, 0);
	}

	updatelistvariables();
	return status;
}


/* int set_statevar_val(int vnum, const char *value);  */

static int
set_statevar_val(int vnum, const char *value)
{
	return (*statevar_func[vnum])((TBUFF **)0, value);
}

/* figure out what type of variable we're setting, and do so */
static int
SetVarValue(
VWRAP *var,		/* name */
const char *value)	/* value */
{
	int status;

	status = TRUE;
	switch (var->v_type) {
	case VW_NOVAR:
	case VW_MODE:
		return FALSE;

	case VW_TEMPVAR:
		FreeIfNeeded(var->v_ptr->u_value);
		if ((var->v_ptr->u_value = strmalloc(value)) == 0)
			status = FALSE;
		break;

	case VW_STATEVAR:
		status = set_statevar_val(var->v_num, value);
		break;
	}

#if	OPT_DEBUGMACROS
	if (tracemacros) {  /* we tracing macros? */
		if (var->v_type == TOK_STATEVAR)
			mlforce("(((%s:&%s:%s)))", render_boolean(rp, status),
					statevars[var->v_num], value);
		else if (var->v_type == TOK_TEMPVAR)
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
	return status;
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

#if OPT_SHOW_COLORS
static void show_attr(int color, const char *attr, const char *name)
{
	bprintf(" %c%d%sC", CONTROL_A, strlen(name), attr);
	if (color >= 0)
		bprintf("%X", color);
	bprintf(":%s", name);
}
/*
 * This will show the foreground colors, which we can display with attributes.
 */
/* ARGSUSED */
static void
makecolorlist(int dum1 GCC_UNUSED, void *ptr GCC_UNUSED)
{
	int j, k;
	REGION region;
	char temp[11];
	char *s;

	bprintf("--- Color palette %*P\n", term.cols-1, '-');
	bprintf("\nColor name       Internal  $palette   Examples\n");
	for (j = -1; j < NCOLORS; j++) {
		if (!is_color_code(j))
			continue;
		k = j >= 0 ? ctrans[j] : j;
		bprintf("\n%16s ", get_color_name(j));

		/* show internal codes in the form that we'll read from the
		 * filters.  That's 'C' followed by a single digit, in
		 * hexadecimal because we allow up to 16 colors.
		 */
		if (j >= 0)
			bprintf("C%X%*P", j, 10, ' ');
		else
			bprintf("%*P", 10, ' ');
		/*
		 * $palette values are in decimal, but it's useful to see the
		 * hexadecimal equivalents.
		 */
		s = lsprintf(temp, "%d", k);
		if (k > 9)
			(void)lsprintf(s, " (%X)", k);
		bprintf("%s%*P", temp, sizeof(temp)-1, ' ');

		show_attr(j, "",   "Normal");
		show_attr(j, "B",  "Bold");
		show_attr(j, "I",  "Italic");
		show_attr(j, "U",  "Underline");
		show_attr(j, "R",  "Reverse");
	}
	bprintf("\n");	/* tweak to ensure we get final line */

	MK.l  = lback(buf_head(curbp));
	MK.o  = 0;
	DOT.l = lforw(buf_head(curbp));
	DOT.o = 0;

	if (getregion(&region) == TRUE)
		attribute_cntl_a_seqs_in_region(&region, FULLLINE);
}

/* ARGSUSED */
int
descolors(int f GCC_UNUSED, int n GCC_UNUSED)
{
	return liststuff(PALETTE_COLORS_BufName, FALSE, makecolorlist, 0, (void *)0);
}

#if OPT_UPBUFF
/* ARGSUSED */
static int
update_colorlist(BUFFER *bp GCC_UNUSED)
{
	return descolors(FALSE, 1);
}

void
relist_descolor(void)
{
	update_scratch(PALETTE_COLORS_BufName, update_colorlist);
}
#endif	/* OPT_UPBUFF */

#endif	/* OPT_SHOW_COLORS */

/*
 * This function is used in several terminal drivers.
 */
#if OPT_EVAL || OPT_COLOR
int
set_palette(const char *value)
{
	tb_curpalette = tb_scopy(&tb_curpalette, value);
#if OPT_COLOR
	if (term.setpal == nullterm_setpal)
		return FALSE;

	relist_descolor();
	term.setpal(tb_values(tb_curpalette));
	vile_refresh(FALSE,0);
#endif
	return TRUE;
}
#endif

/* represent integer as string */
char *
render_int(TBUFF **rp, int i)		/* integer to translate to a string */
{
	char *p, *q;

	p = tb_values(tb_alloc(rp, 32));
	q = lsprintf(p,"%d",i);
	(*rp)->tb_used = (q - p + 1);
	return p;
}

#if OPT_EVAL

/* represent boolean as string */
char *
render_boolean( TBUFF **rp, int val)
{
	static char *bools[] = { "FALSE", "TRUE" };
	return tb_values(tb_scopy(rp, bools[val ? 1 : 0]));
}

#if (SYS_WINNT||SYS_VMS)
/* unsigned to hex */
char *
render_hex(TBUFF **rp, unsigned i)
{
	char *p, *q;

	p = tb_values(tb_alloc(rp, 32));
	q = lsprintf(p,"%x",i);
	(*rp)->tb_used = (q - p + 1);
	return p;
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
	/* check for logical true and false */
	if (is_falsem(val))
		return FALSE;
	if (is_truem(val))
		return TRUE;

	/* check for "numeric truth", like C.  0 is false, everything else
	 * is true
	 */
	return (strtol(val,0,0) != 0);
}

/* discriminate types of tokens */
int
toktyp(
const char *tokn)
{
	if (tokn[0] == EOS)     return TOK_NULL;
	if (tokn[0] == DQUOTE)  return TOK_QUOTSTR;
	if (tokn[0] == SQUOTE)  return TOK_QUOTSTR;
#if ! SMALLER
	if (tokn[1] == EOS)     return TOK_LITSTR; /* only one character */
	switch (tokn[0]) {
		case '~':       return TOK_DIRECTIVE;
		case '@':       return TOK_QUERY;
		case '<':       return TOK_BUFLINE;
		case '$':       return TOK_STATEVAR;
		case '%':       return TOK_TEMPVAR;
		case '&':       return TOK_FUNCTION;
		case '*':       return TOK_LABEL;

		default:        return TOK_LITSTR;
	}
#else
	return TOK_LITSTR;
#endif
}

/*--------------------------------------------------------------------------*/

#if OPT_MACRO_ARGS

/* ARGSUSED */
static int
complete_integer(DONE_ARGS GCC_UNUSED)
{
    char *tmp;
    (void)strtol(buf, &tmp, 0);
    if ((tmp != 0) && (tmp != buf) && (*tmp == 0) && isSpace(c)) {
	if (c != NAMEC) 		/* put it back (cf: kbd_complete) */
	    unkeystroke(c);
	return TRUE;
    }
    return FALSE;
}

static int
complete_FSM(DONE_ARGS, const FSM_CHOICES *choices)
{
    int status;

    if (isDigit(*buf)) {		/* allow numbers for colors */
	status = complete_integer(c, buf, pos);
    } else if (choices != 0) {
	status = kbd_complete(FALSE, c, buf, pos,
				(const char *)choices,
				sizeof (FSM_CHOICES) );
    } else {
	status = FALSE;
    }

    return status;
}

static int
complete_boolean(DONE_ARGS)
{
    return complete_FSM(c, buf, pos, fsm_bool_choices);
}

#if OPT_ENUM_MODES
static const FSM_CHOICES *complete_enum_ptr;

static int
complete_enum(DONE_ARGS)
{
    return complete_FSM(c, buf, pos, complete_enum_ptr);
}
#endif

static int
complete_mode(DONE_ARGS)
{
    return kbd_complete(FALSE, c, buf, pos,
			(const char *)list_of_modes(),
			sizeof(char *));
}

static int
qs_vars_cmp(const void *a, const void *b)
{
	return strcmp(*(const char *const*)a, (* (const char *const*) b));
}

static char **
init_vars_cmpl(void)
{
    int pass;
    UVAR *p;
    unsigned count;
    char **list = 0;

    for (pass = 0; pass < 2; pass++) {
	count = 0;
	for (p = temp_vars; p != 0; p = p->next) {
	    if (pass != 0)
		list[count] = p->u_name;
	    count++;
	}
	if (pass == 0)
		list = typeallocn(char *, count + 1);
    }

    if (list != 0) {
	list[count] = 0;
	qsort(list, count, sizeof(char *), qs_vars_cmp);
    }

    return list;
}

static int
complete_vars(DONE_ARGS)
{
    int status = FALSE;
    char **nptr;

    if ((nptr = init_vars_cmpl()) != 0) {
	status = kbd_complete(FALSE, c, buf, pos,
			(const char *)nptr,
			sizeof(char *));
	free(nptr);
    }
    return status;
}

static int
read_argument(TBUFF **paramp, const PARAM_INFO *info)
{
    int status = TRUE;
    char *prompt;
    TBUFF *temp;
    int (*complete)(DONE_ARGS) = no_completion;
    UINT flags = 0;		/* no expansion, etc. */

    if (clexec == FALSE
     || mac_token(paramp) == 0) {
	prompt = "String";
	if (info != 0) {
	    switch (info->pi_type) {
	    case PT_BOOL:
		prompt = "Boolean";
		complete = complete_boolean;
		break;
#if OPT_ENUM_MODES
	    case PT_ENUM:
		prompt = "Enum";
		flags = KBD_NOEVAL|KBD_LOWERC;
		complete = complete_enum;
		complete_enum_ptr = info->pi_choice;
		break;
#endif
	    case PT_FILE:
		prompt = "Filename";
		break;
	    case PT_INT:
		prompt = "Integer";
		complete = complete_integer;
		break;
	    case PT_MODE:
		prompt = "Mode";
		flags = KBD_NOEVAL|KBD_LOWERC;
		complete = complete_mode;
		break;
#if OPT_MAJORMODE
	    case PT_MAJORMODE:
		prompt = "Majormode";
		flags = KBD_NOEVAL;
		complete = major_complete;
		break;
#endif
	    case PT_VAR:
		prompt = "Variable";
		complete = complete_vars;
		flags = KBD_NOEVAL|KBD_LOWERC|KBD_MAYBEC;
		break;
	    default:
		break;
	    }
	    if (info->pi_text != 0)
		prompt = info->pi_text;
	}

	temp = 0;
	tb_scopy(&temp, prompt);
	tb_sappend0(&temp, ": ");

	if (info->pi_type == PT_FILE) {
	    char fname[NFILEN];
	    status = mlreply_file(tb_values(temp), (TBUFF **)0, FILEC_UNKNOWN, fname);
	    if (status != ABORT)
		tb_scopy(paramp, fname);
	} else {
	    status = kbd_reply(tb_values(temp),
			paramp,		/* in/out buffer */
			eol_history,
			'\n',		/* expect a newline or return */
			flags,		/* no expansion, etc. */
			complete);
	}

	tb_free(&temp);
    }
    return status;
}

/*
 * When first executing a macro, save the remainder of the argument string
 * as an array of strings.
 */
void
save_arguments(BUFFER *bp)
{
    PROC_ARGS *p = typealloc(PROC_ARGS);
    int num_args = 0;
    int max_args;
    char temp[NBUFN];
    const CMDFUNC *cmd = engl2fnc(strip_brackets(temp, bp->b_bname));
    int ok = TRUE;

    if (cmd != 0 && cmd->c_args != 0) {
	for (max_args = 0; cmd->c_args[max_args].pi_type != PT_UNKNOWN; max_args++)
	    ;
    } else {
	max_args = 0;
    }
    TRACE(("save_arguments(%s)\n", bp->b_bname));

    p->nxt_args = arg_stack;
    arg_stack   = p;
    p->all_args = typecallocn(TBUFF *, max_args + 1);
    tb_scopy(&(p->all_args[num_args++]), bp->b_bname);

    while (num_args < max_args+1) {
	if (ok)
	    ok = (read_argument(&(p->all_args[num_args]), &(cmd->c_args[num_args-1])) == TRUE);
	if (ok)
	    tb_scopy(&(p->all_args[num_args]),
		tokval(tb_values(p->all_args[num_args])));
	else
	    tb_scopy(&(p->all_args[num_args]), "");
	TRACE(("...ARG%d:%s\n", num_args, tb_values(p->all_args[num_args])));
	num_args++;
    }

    p->num_args = num_args - 1;

    updatelistvariables();
}

/*
 * Pop the list of arguments off our stack
 */
/* ARGSUSED */
void
restore_arguments(BUFFER *bp GCC_UNUSED)
{
    PROC_ARGS *p = arg_stack;

    TRACE(("restore_arguments(%s)\n", bp->b_bname));

    if (p != 0) {
	arg_stack = p->nxt_args;
	while (p->num_args >= 0) {
	    tb_free(&(p->all_args[p->num_args]));
	    p->num_args -= 1;
	}
	free(p->all_args);
	free(p);
    }

    updatelistvariables();
}

/*
 * Return the argument value if it is a number in range, the number of
 * arguments for '#', and the whole argument list for '*'.  As a special case,
 * return the list with individual words quoted for '@'.
 */
static char *
get_argument(const char *name)
{
    static TBUFF *value;
    static char empty[] = "";
    int num;
    char *str;
    char *result = error_val;

    if (arg_stack != 0) {
	if (*name == '#') {
	    result = render_int(&value, arg_stack->num_args);
	} else if (*name == '*') {
	    tb_init(&value, EOS);
	    for (num = 1; num <= arg_stack->num_args; num++) {
		if (num > 1)
		    tb_append(&value, ' ');
		tb_sappend(&value, tb_values(arg_stack->all_args[num]));
	    }
	    tb_append(&value, EOS);
	    result = tb_values(value);
	} else if (*name == '@') {
	    tb_init(&value, EOS);
	    for (num = 1; num <= arg_stack->num_args; num++) {
		if (num > 1)
		    tb_append(&value, ' ');
		str = tb_values(arg_stack->all_args[num]);
		tb_append(&value, DQUOTE);
		while (*str != EOS) {
		    if (*str == BACKSLASH || *str == DQUOTE)
			tb_append(&value, BACKSLASH);
		    tb_append(&value, *str++);
		}
		tb_append(&value, DQUOTE);
	    }
	    tb_append(&value, EOS);
	    result = tb_values(value);
	} else if (isDigit(*name)) {
	    if ((num = scan_int(name)) <= arg_stack->num_args) {
		if (num > 0 || !strcmp(name, "0"))
		    result = tb_values(arg_stack->all_args[num]);
	    } else {
		result = empty;
	    }
	}
    }
    return result;
}

#endif /* OPT_MACRO_ARGS */

/*--------------------------------------------------------------------------*/

#if OPT_EVAL

/* the argument simply represents itself */
static char *
simple_arg_eval(char *argp)
{
	return argp;
}

/* the returned value is the user's response to the specified prompt */
static char *
query_arg_eval(char *argp)
{
	argp = user_reply(tokval(argp+1));
	return argp ? argp : error_val;
}

/* the returned value is the next line from the named buffer */
static char *
buffer_arg_eval(char *argp)
{
	argp = next_buffer_line(tokval(argp+1));
	return argp ? argp : error_val;
}

/* expand it as a temp variable */
static char *
tempvar_arg_eval(char *argp)
{
	static TBUFF *tmp;
	UVAR *p;
	char *result;
	if ((p = lookup_tempvar(argp+1)) != 0) {
		tb_scopy(&tmp, p->u_value);
		result = tb_values(tmp);
	} else {
		result = error_val;
	}
	return result;
}

/* state variables are expanded.  if it's
 * not an statevar, perhaps it's a mode?
 */
static char *
statevar_arg_eval(char *argp)
{
	int vnum;
	char *result;

	if ((result = get_argument(++argp)) == error_val) {
	    vnum = lookup_statevar(argp);
	    if (vnum != ILLEGAL_NUM)
		result = get_statevar_val(vnum);
#if !SMALLER
	    else {
		VALARGS	args;
		if (is_mode_name(argp, TRUE, &args) == TRUE)
		    result = string_mode_val(&args);
	    }
#endif
	}
	return result;
}

/* run a function to evalute it */
static char *
function_arg_eval(char *argp)
{
	int fnum = lookup_func(argp);
	return (fnum != ILLEGAL_NUM) ? run_func(fnum) : error_val;
}

/* goto targets evaluate to their line number in the buffer */
static char *
label_arg_eval(char *argp)
{
	static TBUFF *label;
	LINE *lp = label2lp(curbp, argp);

	if (lp)
		return render_int(&label, line_no(curbp, lp));
	else
		return "0";
}

/* the value of a quoted string begins just past the leading quote */
static char *
qstring_arg_eval(char *argp)
{
	return argp + 1;
}


static char *
directive_arg_eval(char *argp)
{
	static TBUFF *tkbuf;
#if SYS_UNIX
	/* major hack alert -- the directives start with '~'.  on
	 * unix, we'd also like to be able to expand ~user as
	 * a home directory.  handily, these two uses don't
	 * conflict.  too much. */
	tb_alloc(&tkbuf, NFILEN + strlen(argp));
	return lengthen_path(strcpy(tb_values(tkbuf), argp));
#else
	/*
	 * For other systems, we can usually expand our own "home" directory.
	 */
	if (!strncmp("~/", argp, 2)) {
		tb_alloc(&tkbuf, NFILEN + strlen(argp));
		return lengthen_path(strcpy(tb_values(tkbuf), argp));
	}
	return error_val;
#endif
}


typedef char * (ArgEvalFunc)(char *argp);

static ArgEvalFunc *eval_func[] = {
    simple_arg_eval,		/* TOK_NULL */
    query_arg_eval,		/* TOK_QUERY */
    buffer_arg_eval,		/* TOK_BUFLINE */
    tempvar_arg_eval,		/* TOK_TEMPVAR */
    statevar_arg_eval,		/* TOK_STATEVAR */
    function_arg_eval,		/* TOK_FUNCTION */
    directive_arg_eval,		/* TOK_DIRECTIVE */
    label_arg_eval,		/* TOK_LABEL */
    qstring_arg_eval,		/* TOK_QUOTSTR */
    simple_arg_eval		/* TOK_LITSTR */
};

#endif /* OPT_EVAL */

/* evaluate to the string-value of a token */
char *
tokval(char *tokn)
{
	char *result;
#if OPT_EVAL
	int toknum = toktyp(tokn);
	if (toknum < 0 || toknum > MAXTOKTYPE)
		result = error_val;
	else
		result = (*eval_func[toknum])(tokn);
#else
	result = (toktyp(tokn) == TOK_QUOTSTR) ? tokn+1 : tokn;
#endif
	return result;
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
#if OPT_BOOL_CHOICES
	return choice_to_code(fsm_bool_choices, temp, strlen(temp)) == TRUE;
#else
	return !strncmp(temp, "true", strlen(temp));
#endif
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
#if OPT_BOOL_CHOICES
	return choice_to_code(fsm_bool_choices, temp, strlen(temp)) == FALSE;
#else
	return !strncmp(temp, "false", strlen(temp));
#endif
}

#if OPT_EVAL
int
evaluate(int f, int n)
{
	static PARAM_INFO info = { PT_STR, "Expression", 0 };
	TBUFF *params = 0, *tok = 0, *cmd = 0;
	char *old, *tmp;
	int code = FALSE;

	if (read_argument(&params, &info) == TRUE) {
	    old = execstr;
	    execstr = tb_values(params);
	    TRACE(("EVAL %s\n", execstr));
	    while ((tmp = mac_tokval(&tok)) != 0) {
		if (tb_length(cmd))
		    tb_sappend0(&cmd, " ");
		tb_sappend0(&cmd, tmp);
	    }
	    if ((tmp = tb_values(cmd)) != 0) {
		execstr = tmp;
		TRACE(("...docmd %s{%s}\n", tmp, execstr));
		code = docmd(tmp, TRUE, f, n);
	    }
	    execstr = old;
	}
	tb_free(&cmd);
	tb_free(&tok);
	tb_free(&params);
	TRACE(("...EVAL ->%d\n", code));
	return code;
}

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
	return result;
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

	return s;
}
#endif

char *
mklower(char *s)
{
	char *sp;

	for (sp = s; *sp; sp++)
		if (isUpper(*sp))
			*sp = toLower(*sp);

	return s;
}

char *
mktrimmed(char *str)	/* trim whitespace */
{
	char *base = str;
	char *dst = str;

	while (*str != EOS) {
		if (isSpace(*str)) {
			if (dst != base)
				*dst++ = ' ';
			str = skip_blanks(str);
		} else {
			*dst++ = *str++;
		}
	}
	if (dst != base && isSpace(dst[-1]))
		dst--;
	*dst = EOS;
	return base;
}

int
absol(int x)		/* return absolute value of given integer */
{
	return (x < 0) ? -x : x;
}
