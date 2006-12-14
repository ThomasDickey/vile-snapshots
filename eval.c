/*
 *	eval.c -- function and variable evaluation
 *	original by Daniel Lawrence
 *
 * $Header: /users/source/archives/vile.vcs/RCS/eval.c,v 1.356 2006/11/24 20:13:47 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"
#include	"nevars.h"
#include	"nefsms.h"

#include	<ctype.h>

#define	ILLEGAL_NUM	-1

#if OPT_EVAL

#if OPT_FILTER
#include	<filters.h>
#endif

/* "generic" variable wrapper, to insulate some users of variables
 * from needing to distinguish the different types */
typedef struct {
    int v_type;			/* type: values below */
    int v_num;			/* index, for state vars */
    UVAR *v_ptr;		/* pointer, for temp vars */
} VWRAP;

/* values for v_type */
#define VW_NOVAR	0
#define VW_STATEVAR	1
#define VW_TEMPVAR	2
#define VW_MODE		3

typedef struct PROC_ARGS {
    struct PROC_ARGS *nxt_args;
    int num_args;		/* total argument count */
    TBUFF **all_args;
    TBUFF *result;		/* function can assign to $return */
} PROC_ARGS;

static PROC_ARGS *arg_stack;

static size_t s2size(const char *s);
static char **init_vars_cmpl(void);
static char *get_statevar_val(int vnum);
static const char *s2offset(const char *s, const char *n);
static int SetVarValue(VWRAP * var, const char *name, const char *value);
static int lookup_statevar(const char *vname);
static void FindVar(char *var, VWRAP * vd);
static void free_vars_cmpl(void);
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

    bprintf("--- Printable Characters for locale %s (%s) ",
	    NONNULL(vl_locale),
	    NONNULL(vl_encoding));
    bpadc('-', term.cols - DOT.o);
    bputc('\n');

    for (i = 0; i < N_chars; i++) {
	bprintf("\n%d\t", (int) i);
	if ((i == '\n') || (i == '\t'))		/* vtlistc() may not do these */
	    bprintf("^%c", (int) ('@' | i));
#if OPT_LOCALE
	else if (!isPrint(i) && i > 127 && i < 160)	/* C1 controls? */
	    bprintf(global_w_val(WMDNONPRINTOCTAL)
		    ? "\\%3o"
		    : "\\x%2x",
		    i);
#endif
	else
	    bprintf("%c", (int) i);
	bputc('\t');
	for (j = 0; j != vl_UNUSED; j++) {
	    if ((s = choice_to_name(fsm_charclass_choices, j)) != 0) {
		k = (1 << j);
		if (j != 0)
		    bputc(' ');
		bprintf("%*s",
			(int) strlen(s),
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
    return liststuff(PRINTABLECHARS_BufName,
		     FALSE, makectypelist, 0, (void *) 0);
}

#if OPT_UPBUFF
static void
update_char_classes(void)
{
    update_scratch(PRINTABLECHARS_BufName, show_CharClasses);
}

#else
#define update_char_classes()	/*nothing */
#endif

/* ARGSUSED */
int
desprint(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return show_CharClasses(curbp);
}

static int
cclass_complete(DONE_ARGS)
{
    return kbd_complete(PASS_DONE_ARGS,
			(const char *) fsm_charclass_choices,
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
		       eol_history, '=',
		       KBD_NOEVAL | KBD_LOWERC,
		       cclass_complete);
    return (status == TRUE)
	? choice_to_code(fsm_charclass_choices,
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
    return ((status == TRUE)
	    ? new_regexval(tb_values(var), TRUE)
	    : 0);
}

static int
match_charclass_regexp(int ch, REGEXVAL * exp)
{
    char temp[2];
    temp[0] = (char) ch;

    return regexec(exp->reg, temp, temp + 1, 0, 0);
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

#if OPT_EVAL
/*
 * Compute the intersection of the character classes in the argument.
 */
static CHARTYPE
charclass_of(const char *arg)
{
    CHARTYPE k;
    if (arg == error_val) {
	k = 0;
    } else {
	k = vl_chartypes_[char2int(*arg)];
	if (*arg) {
	    while (*++arg) {
		k &= vl_chartypes_[char2int(*arg)];
	    }
	}
    }
    return k;
}

/*
 * Make display of the intersection of the character classes in the argument.
 */
static void
show_charclass(TBUFF **result, const char *arg)
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
#endif

/*--------------------------------------------------------------------------*/

#if OPT_SHOW_EVAL
/* list the current vars into the current buffer */
/* ARGSUSED */
static void
makevarslist(int dum1 GCC_UNUSED, void *ptr)
{
    UVAR *p;
    int j, k;

    bprintf("--- State variables ");
    bpadc('-', term.cols - DOT.o);
    bputc('\n');

    bprintf("%s", (char *) ptr);
    if (arg_stack != 0 && ((k = arg_stack->num_args) != 0)) {
	bprintf("--- %s parameters ",
		tb_values(arg_stack->all_args[0]));
	bpadc('-', term.cols - DOT.o);
	for (j = 1; j <= k; j++) {
	    bprintf("\n$%d = %s", j,
		    tb_values(arg_stack->all_args[j]));
	}
    }
    if (temp_vars != 0)
	bputc('\n');
    for (p = temp_vars, j = 0; p != 0; p = p->next) {
	if (!j++) {
	    bprintf("--- Temporary variables ");
	    bpadc('-', term.cols - DOT.o);
	}
	bprintf("\n%%%s = %s", p->u_name, p->u_value);
    }
}

/* Test a name to ensure that it is a mode-name, filtering out modish-stuff
 * such as "all" and "noview"
 */
static int
is_mode_name(const char *name, int showall, VALARGS * args)
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
    int s;
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
    WINDOW *wp = curwp;
    int s;
    size_t t;
    char *v;
    const char *vv;
    static const char fmt[] =
    {"$%s = %.*s\n"};
    const char *const *Names;
    int showall;
    int rc = FALSE;

    TRACE((T_CALLED "show_VariableList()\n"));

    Names = show_vars_f ? list_of_modes() : statevars;
    if (Names != 0) {
	showall = show_vars_f ? (show_vars_n > 1) : FALSE;
	/* collect data for state-variables, since some depend on window */
	for (s = 0, t = 0; Names[s] != 0; s++) {
	    if ((vv = get_listvalue(Names[s], showall)) != 0)
		t += strlen(Names[s]) + strlen(fmt) + strlen(vv);
	}

	beginDisplay();
	if ((values = typeallocn(char, t)) != 0) {
	    for (s = 0, v = values; Names[s] != 0; s++) {
		if ((vv = get_listvalue(Names[s], showall)) != 0) {
		    t = strlen(vv);
		    if (t == 0) {
			t = 1;
			vv = "";
		    } else if (vv[t - 1] == '\n')
			t--;	/* suppress trailing newline */
		    v = lsprintf(v, fmt, Names[s], t, vv);
		}
	    }
	    rc = liststuff(VARIABLES_BufName, FALSE,
			   makevarslist, 0, (void *) values);
	    free(values);

	    /* back to the buffer whose modes we just listed */
	    swbuffer(wp->w_bufp);
	}
	endofDisplay();
    }
    returnCode(rc);
}

#if OPT_UPBUFF
/*
 * If the list-variables window is visible, update it after operations that
 * would modify the list.
 */
void
updatelistvariables(void)
{
    update_scratch(VARIABLES_BufName, show_VariableList);
}
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
int
vl_lookup_func(const char *name)
{
    char downcased[NSTRING];
    int fnum = ILLEGAL_NUM;
    int n;
    size_t m;

    if (name[0] == '&'
	&& name[1] != EOS) {

	mklower(vl_strncpy(downcased, name + 1, sizeof(downcased)));

	m = strlen(downcased);
	for (n = 0; n < NFUNCS; n++) {
	    if (!strncmp(downcased, vl_ufuncs[n].f_name, m)) {
		if ((n + 1 >= NFUNCS
		     || m == strlen(vl_ufuncs[n].f_name)
		     || strncmp(downcased, vl_ufuncs[n + 1].f_name, m)))
		    fnum = n;
		break;
	    }
	}
    }
    TRACE(("vl_lookup_func(%s) = %d\n", TRACE_NULL(name), fnum));
    return (fnum);
}

/*
 * Find the given token, returning true if it is found in the string.
 */
static int
search_token(const char *token, const char *delims, const char *string)
{
    size_t length = strlen(token);

    while (*string != EOS) {
	const char *first = string;
	string += strcspn(first, delims);
	if (string - first == (int) length
	    && !strncmp(first, token, length))
	    return TRUE;
	string += strspn(string, delims);
    }
    return FALSE;
}

/*
 * Find the count'th token, counting from zero.  This will return an empty
 * result only if we run past the end of the number of tokens.
 */
static void
extract_token(TBUFF **result, const char *count, const char *delims, const char *string)
{
    int num = scan_int(count);

    while (num-- > 0) {
	string += strspn(string, delims);
	string += strcspn(string, delims);
    }

    string += strspn(string, delims);
    tb_init(result, 0);
    tb_bappend(result, string, strcspn(string, delims));
    tb_append(result, EOS);
}

/*
 * Translate string 'from' 'to'.
 */
static void
translate_string(TBUFF **result, const char *from, const char *to, const char *string)
{
    int len_to = (int) strlen(to);
    char *s, *t;

    if (tb_scopy(result, string) != 0) {
	if ((s = tb_values(*result)) != 0) {
	    for (; *s != EOS; ++s) {
		if ((t = strchr(from, *s)) != 0
		    && (int) (t - from) < len_to) {
		    *s = to[t - from];
		}
	    }
	}
    }
}

static void
path_trim(char *path)
{
#if SYS_UNIX
    while ((int) strlen(path) > 1) {
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
static char *
path_head(TBUFF **result, const char *path)
{
    char *temp = tb_values(tb_scopy(result, path));
    if (temp != error_val && temp != 0) {
	*pathleaf(temp) = EOS;
	path_trim(temp);
    }
    return temp;
}

/*
 * If the given argument contains non-path characters, quote it.  Otherwise
 * simply copy it.
 */
static void
path_quote(TBUFF **result, const char *path)
{
#if SYS_VMS
    tb_scopy(result, path);	/* no point in quoting here */
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

static void
render_date(TBUFF **result, char *format, time_t stamp)
{
#ifdef HAVE_STRFTIME
    struct tm *tm = localtime(&stamp);
    char temp[NSTRING];

    if (strftime(temp, sizeof(temp) - 1, format, tm) != 0)
	tb_scopy(result, temp);
    else
#endif
    {
	/* FIXME (2001/5/20) I'd write a q/d fallback for strftime() using
	 * struct tm, but don't want to get sidetracked with its portability
	 * -TD
	 */
	*result = tb_string(ctime(&stamp));
	tb_unput(*result);	/* trim the newline */
	tb_append(result, EOS);
    }
}

static void
default_mode_value(TBUFF **result, char *name)
{
    VALARGS args;
    VWRAP vd;
    int mode_class, mode_name;
    struct VAL mode;

    FindVar(name, &vd);
    switch (vd.v_type) {
    case VW_STATEVAR:
	tb_scopy(result, init_state_value(vd.v_num));
	break;
    case VW_MODE:
	memset(&args, 0, sizeof(args));
	for (mode_class = UNI_MODE; mode_class <= WIN_MODE; mode_class++) {
	    switch (mode_class) {
	    default:
	    case UNI_MODE:	/* universal modes */
		args.names = g_valnames;
		break;
	    case BUF_MODE:	/* buffer modes */
		args.names = b_valnames;
		break;
	    case WIN_MODE:	/* window modes */
		args.names = w_valnames;
		break;
	    }
	    if ((mode_name = lookup_valnames(name, args.names)) >= 0) {
		memset(&mode, 0, sizeof(mode));
		init_mode_value(&mode, (MODECLASS) mode_class, mode_name);
		args.names += mode_name;
		args.global = &mode;
		args.local = args.global;
		tb_scopy(result, string_mode_val(&args));
		break;
	    }
	}
	break;
    default:
	tb_init(result, EOS);	/* FIXME: handle %name and &ind name */
    }
}

/*
 * Based on mac_tokval(), but unlike that, strips quotes which came from
 * evaluating via run_func().
 */
static char *
dequoted_parameter(TBUFF **tok)
{
    char *previous;
    const char *newvalue;
    int strip;

    if (mac_token(tok) != 0) {
	previous = tb_values(*tok);

	switch (toktyp(previous)) {
	default:
	case TOK_NULL:
	case TOK_QUERY:
	case TOK_BUFLINE:
	case TOK_TEMPVAR:
	case TOK_DIRECTIVE:
	case TOK_LABEL:
	case TOK_LITSTR:
	    newvalue = tokval(previous);
	    strip = FALSE;
	    break;
	case TOK_FUNCTION:
	    newvalue = tokval(previous);
	    strip = (toktyp(newvalue) == TOK_QUOTSTR);
	    break;
	case TOK_QUOTSTR:
	    strip = TRUE;
	    newvalue = previous;
	    break;
	}

	if (newvalue == (const char *) error_val) {
	    return error_val;
	} else if (strip) {
	    if (((const char *) previous) != newvalue) {
		TBUFF *fix = 0;
		tb_scopy(&fix, newvalue);
		tb_free(tok);
		*tok = fix;
	    }
	    tb_dequote(tok);
	} else if (((const char *) previous) != newvalue) {
	    tb_scopy(tok, newvalue);
	}
	return (tb_values(*tok));
    }
    tb_free(tok);
    return (0);
}

#define UNI_CLASSNAME "universal"
#define BUF_CLASSNAME "buffer"
#define WIN_CLASSNAME "window"
#define SUB_CLASSNAME "submode"
#define MAJ_CLASSNAME "Majormode"

#define COLOR_CLASSNAME "color"
#define MODES_CLASSNAME "mode"

/*
 * Check if the given value is an instance of the given class.  That's easier
 * to work with in scripts than trying to use a feature and (then) trying to
 * recover if it doesn't exist.
 *
 * Note:  The classnames are chosen to allow single character abbreviations.
 */
static int
test_isa_class(const char *classname, const char *value)
{
    VALARGS args;
    int result = FALSE;
    size_t len = strlen(classname);

    if (!strncmp(classname, BUF_CLASSNAME, len)) {
	result = (find_b_name(value) != 0);
    } else if (!strncmp(classname, COLOR_CLASSNAME, len)) {
#if OPT_COLOR_CHOICES
	result = (choice_to_code(fsm_color_choices, value, len) != ENUM_ILLEGAL);
#endif
    } else if (!strncmp(classname, MODES_CLASSNAME, len)) {
	if (find_mode_class(curbp, value, TRUE, &args, UNI_MODE)
	    || find_mode_class(curbp, value, TRUE, &args, BUF_MODE)
	    || find_mode_class(curbp, value, TRUE, &args, WIN_MODE))
	    result = TRUE;
#if OPT_MAJORMODE
    } else if (!strncmp(classname, SUB_CLASSNAME, len)) {
	result = find_mode_class(curbp, value, TRUE, &args, SUB_MODE);
    } else if (!strncmp(classname, MAJ_CLASSNAME, len)) {
	result = find_mode_class(curbp, value, TRUE, &args, MAJ_MODE);
#endif
    }
    return result;
}

/*
 * Reverse of the "&isa" operator.
 */
static void
find_classof(TBUFF **result, const char *value)
{
    static const char *table[] =
    {
	BUF_CLASSNAME,
	COLOR_CLASSNAME,
	MODES_CLASSNAME,
	SUB_CLASSNAME,
	MAJ_CLASSNAME
    };
    size_t n;
    int found = FALSE;

    tb_init(result, EOS);
    for (n = 0; n < TABLESIZE(table); ++n) {
	if (test_isa_class(table[n], value)) {
	    found = TRUE;
	    if (tb_length(*result))
		tb_sappend0(result, " ");
	    tb_sappend0(result, table[n]);
	    break;
	}
    }
    if (!found)
	tb_error(result);
}

/*
 * Find the mode's class (universal, buffer, window, major).
 */
static void
find_modeclass(TBUFF **result, const char *value)
{
    VALARGS args;

    if (find_mode_class(curbp, value, TRUE, &args, UNI_MODE))
	tb_scopy(result, UNI_CLASSNAME);
    else if (find_mode_class(curbp, value, TRUE, &args, BUF_MODE))
	tb_scopy(result, BUF_CLASSNAME);
    else if (find_mode_class(curbp, value, TRUE, &args, WIN_MODE))
	tb_scopy(result, WIN_CLASSNAME);
#if OPT_MAJORMODE
    else if (find_mode_class(curbp, value, TRUE, &args, SUB_MODE))
	tb_scopy(result, SUB_CLASSNAME);
    else if (find_mode_class(curbp, value, TRUE, &args, MAJ_MODE))
	tb_scopy(result, MAJ_CLASSNAME);
#endif
    else
	tb_error(result);
}

#define MAXARGS 3

/*
 * execute a builtin function
 */
static char *
run_func(int fnum)
{
    static TBUFF *result;	/* function result */

    REGEXVAL *exp;
    BUFFER *bp;
    TBUFF *args[MAXARGS];
    TBUFF *juggle = 0;
    char *arg[MAXARGS];		/* function arguments */
    char *cp;
    const char *sp;
    int args_numeric, args_boolean, ret_numeric, ret_boolean;
    int bools[MAXARGS];
    int i, nargs;
    int is_error = FALSE;
    long value = 0;
    long nums[MAXARGS];

    TRACE((T_CALLED "run_func(%d:%s)\n", fnum, vl_ufuncs[fnum].f_name));

    nargs = vl_ufuncs[fnum].f_code & NARGMASK;
    args_numeric = vl_ufuncs[fnum].f_code & NUM;
    args_boolean = vl_ufuncs[fnum].f_code & BOOL;

    ret_numeric = vl_ufuncs[fnum].f_code & NRET;
    ret_boolean = vl_ufuncs[fnum].f_code & BRET;

    TPRINTF(("** evaluate '%s' (0x%x), %d %s args returning %s\n",
	     vl_ufuncs[fnum].f_name,
	     vl_ufuncs[fnum].f_code,
	     nargs,
	     (args_numeric
	      ? "numeric"
	      : (args_boolean
		 ? "boolean"
		 : "string")),
	     (ret_numeric
	      ? "numeric"
	      : (ret_boolean
		 ? "boolean"
		 : "string"))));

    /* fetch required arguments */
    for (i = 0; i < nargs; i++) {
	args[i] = 0;
	if ((arg[i] = dequoted_parameter(&args[i])) == 0
	    || (arg[i] == error_val)
	    || isTB_ERRS(args[i])) {
	    arg[i] = error_val;
	    is_error = TRUE;
	}
	tb_free(&result);	/* in case mac_tokval() called us */
	TPRINTF(("...arg[%d] = '%s'\n", i, arg[i]));
	if (args_numeric)
	    nums[i] = scan_long(arg[i]);
	else if (args_boolean)
	    bools[i] = scan_bool(arg[i]);
    }

    tb_init(&result, EOS);

    switch ((UFuncCode) fnum) {
    case UFADD:
	value = nums[0] + nums[1];
	break;
    case UFSUB:
	value = nums[0] - nums[1];
	break;
    case UFTIMES:
	value = nums[0] * nums[1];
	break;
    case UFDIV:
	if (nums[1] == 0)
	    is_error = TRUE;
	if (!is_error)
	    value = nums[0] / nums[1];
	break;
    case UFMOD:
	if (nums[1] <= 0)
	    is_error = TRUE;
	if (!is_error)
	    value = nums[0] % nums[1];
	break;
    case UFNEG:
	value = -nums[0];
	break;
    case UFCAT:
	tb_scopy(&result, arg[0]);
	tb_sappend0(&result, arg[1]);
	break;
    case UFISA:
	value = test_isa_class(arg[0], arg[1]);
	break;
    case UFCLASSOF:
	find_classof(&result, arg[0]);
	break;
    case UFMCLASS:
	find_modeclass(&result, arg[0]);
	break;
    case UFCCLASS:
	show_charclass(&result, arg[0]);
	break;
    case UFDATE:
	render_date(&result, arg[0], nums[1]);
	break;
    case UFLEFT:
	tb_bappend(&result, arg[0], s2size(arg[1]));
	tb_append(&result, EOS);
	break;
    case UFRIGHT:
	tb_scopy(&result, s2offset(arg[0], arg[1]));
	break;
    case UFMID:
	tb_bappend(&result, s2offset(arg[0], arg[1]), s2size(arg[2]));
	tb_append(&result, EOS);
	break;
    case UFNOT:
	value = !bools[0];
	break;
    case UFEQUAL:
	value = (nums[0] == nums[1]);
	break;
    case UFERROR:
	is_error = FALSE;
	value = (arg[0] == error_val);
	break;
    case UFLT:			/* FALLTHRU */
    case UFLESS:
	value = (nums[0] < nums[1]);
	break;
    case UFLEQ:
	value = (nums[0] <= nums[1]);
	break;
    case UFNEQ:
	value = (nums[0] != nums[1]);
	break;
    case UFGT:			/* FALLTHRU */
    case UFGREATER:
	value = (nums[0] > nums[1]);
	break;
    case UFGEQ:
	value = (nums[0] >= nums[1]);
	break;
    case UFSNEQ:
	value = (strcmp(arg[0], arg[1]) != 0);
	break;
    case UFSEQUAL:
	value = (strcmp(arg[0], arg[1]) == 0);
	break;
    case UFSLT:		/* FALLTHRU */
    case UFSLESS:
	value = (strcmp(arg[0], arg[1]) < 0);
	break;
    case UFSLEQ:
	value = (strcmp(arg[0], arg[1]) <= 0);
	break;
    case UFSGT:		/* FALLTHRU */
    case UFSGREAT:
	value = (strcmp(arg[0], arg[1]) > 0);
	break;
    case UFSGEQ:
	value = (strcmp(arg[0], arg[1]) >= 0);
	break;
    case UFINDIRECT:
	if ((sp = tokval(arg[0])) != error_val)
	    tb_scopy(&result, sp);
	else
	    is_error = TRUE;
	break;
    case UFAND:
	value = (bools[0] && bools[1]);
	break;
    case UFOR:
	value = (bools[0] || bools[1]);
	break;
    case UFLENGTH:
	value = (long) strlen(arg[0]);
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
	value = (long) arg[0][0];
	break;
    case UFCHR:
	tb_append(&result, (char) nums[0]);
	tb_append(&result, EOS);
	break;
    case UFGTKEY:
	tb_append(&result, (char) keystroke_raw8());
	tb_append(&result, EOS);
	break;
    case UFGTSEQ:
	(void) kcod2escape_seq(kbd_seq_nomap(), tb_values(result), result->tb_size);
	tb_setlen(&result, -1);
	break;
    case UFCMATCH:
	if ((exp = new_regexval(arg[0], TRUE)) != 0) {
	    int save_flag = ignorecase;
	    ignorecase = TRUE;
	    value = regexec(exp->reg, arg[1], (char *) 0, 0, -1);
	    ignorecase = save_flag;
	}
	break;
    case UFMATCH:
	if ((exp = new_regexval(arg[0], TRUE)) != 0)
	    value = regexec(exp->reg, arg[1], (char *) 0, 0, -1);
	break;
    case UFRANDOM:		/* FALLTHRU */
    case UFRND:
	if (!is_error)
	    value = rand() % absol(nums[0]);
	value++;		/* return 1 to N */
	break;
    case UFABS:
	value = absol(nums[0]);
	break;
    case UFSINDEX:
	value = ourstrstr(arg[0], arg[1], FALSE);
	break;
    case UFENV:
	if (!is_error)
	    tb_scopy(&result, safe_getenv(arg[0]));
	break;
    case UFBIND:
	if (!is_error) {
	    if ((sp = prc2engl(arg[0])) == error_val)
		is_error = TRUE;
	    else
		tb_scopy(&result, sp);
	}
	break;
    case UFDEFAULT:
	if (!is_error)
	    default_mode_value(&result, arg[0]);
	break;
    case UFREADABLE:		/* FALLTHRU */
    case UFRD:
	value = (!is_error &&
		 doglob(arg[0]) &&
		 cfg_locate(arg[0], FL_CDIR | FL_READABLE) != NULL);
	break;
    case UFWRITABLE:
	value = (!is_error &&
		 doglob(arg[0]) &&
		 cfg_locate(arg[0], FL_CDIR | FL_WRITEABLE) != NULL);
	break;
    case UFEXECABLE:
	value = (!is_error &&
		 doglob(arg[0]) &&
		 cfg_locate(arg[0], FL_CDIR | FL_EXECABLE) != NULL);
	break;
    case UFFILTER:
	value = FALSE;
#if OPT_FILTER
	if (flt_lookup(arg[0]))
	    value = TRUE;
#endif
	break;
    case UFFTIME:
	if (!is_error
	    && (bp = find_any_buffer(arg[0])) != 0
	    && !is_internalname(bp->b_fname))
	    value = file_modified(bp->b_fname);
	break;
    case UFLOCMODE:
    case UFGLOBMODE:
	if (!is_error) {
	    VALARGS vargs;
	    if (find_mode(curbp, arg[0],
			  (fnum == UFGLOBMODE), &vargs) != TRUE)
		break;
	    tb_scopy(&result, string_mode_val(&vargs));
	}
	break;
    case UFDQUERY:
	if (!is_error) {
	    if ((cp = user_reply(arg[0], arg[1])) != 0
		&& (cp != error_val))
		tb_scopy(&result, cp);
	    else
		is_error = TRUE;
	}
	break;
    case UFQPASSWD:
	if (!is_error) {
	    qpasswd = TRUE;
	    if ((cp = user_reply(arg[0], NULL)) != 0
		&& (cp != error_val))
		tb_scopy(&result, cp);
	    else
		is_error = TRUE;
	    qpasswd = FALSE;
	}
	break;
    case UFQUERY:
	if (!is_error) {
	    if ((cp = user_reply(arg[0], error_val)) != 0
		&& (cp != error_val))
		tb_scopy(&result, cp);
	    else
		is_error = TRUE;
	}
	break;
    case UFLOOKUP:
	if ((i = combine_choices(fsm_lookup_choices, arg[0])) > 0)
	    tb_scopy(&result, SL_TO_BSL(cfg_locate(arg[1], i)));
	break;
    case UFPATH:
	if (!is_error) {
	    switch (choice_to_code(fsm_path_choices, arg[0], strlen(arg[0]))) {
	    case PATH_END:
		cp = pathleaf(arg[1]);
		if ((cp = strchr(cp, '.')) != 0)
		    tb_scopy(&result, SL_TO_BSL(cp));
		break;
	    case PATH_FULL:
		if (tb_scopy(&juggle, arg[1])
		    && tb_alloc(&juggle, NFILEN)) {
		    lengthen_path(tb_values(juggle));
		    tb_setlen(&juggle, -1);
		    tb_scopy(&result, SL_TO_BSL(tb_values(juggle)));
		} else {
		    is_error = TRUE;
		}
		break;
	    case PATH_HEAD:
		path_head(&juggle, arg[1]);
		tb_setlen(&juggle, -1);
		tb_scopy(&result, SL_TO_BSL(tb_values(juggle)));
		break;
	    case PATH_ROOT:
		tb_scopy(&result, SL_TO_BSL(pathleaf(arg[1])));
		if (tb_values(result) != 0
		    && (cp = strchr(tb_values(result), '.')) != 0) {
		    *cp = EOS;
		    tb_setlen(&result, -1);
		}
		break;
	    case PATH_SHORT:
		if (tb_scopy(&juggle, arg[1])
		    && tb_alloc(&juggle, NFILEN)) {
		    shorten_path(tb_values(juggle), FALSE);
		    tb_setlen(&juggle, -1);
		    tb_scopy(&result, SL_TO_BSL(tb_values(juggle)));
		} else {
		    is_error = TRUE;
		}
		break;
	    case PATH_TAIL:
		tb_scopy(&result, pathleaf(arg[1]));
		break;
	    default:
		is_error = TRUE;
		break;
	    }
	}
	break;
    case UFPATHCAT:
	tb_alloc(&juggle, NFILEN);
	pathcat(tb_values(juggle), arg[0], arg[1]);
	tb_setlen(&juggle, -1);
	tb_scopy(&result, SL_TO_BSL(tb_values(juggle)));
	break;
    case UFPATHQUOTE:
	path_quote(&result, SL_TO_BSL(arg[0]));
	break;
    case UFSTIME:
	value = time((time_t *) 0);
	break;
    case UFSTOKEN:
	value = search_token(arg[0], arg[1], arg[2]);
	break;
    case UFTOKEN:
	extract_token(&result, arg[0], arg[1], arg[2]);
	break;
    case UFTRANS:
	translate_string(&result, arg[0], arg[1], arg[2]);
	break;
    case UFWORD:
	extract_token(&result, arg[0], "\t ", arg[1]);
	break;
    case UFREGISTER:
	if (!is_error) {
	    i = reg2index(*arg[0]);
	    if ((i = index2ukb(i)) < NKREGS
		&& i >= 0) {
		KILL *kp = kbs[i].kbufh;
		while (kp != 0) {
		    tb_bappend(&result,
			       (char *) (kp->d_chunk),
			       KbSize(i, kp));
		    kp = kp->d_next;
		}
		tb_append(&result, EOS);
		break;
	    }
	}
	/* FALLTHRU */
    case NFUNCS:
	TRACE(("unknown function #%d\n", fnum));
	is_error = TRUE;
	break;
    }

    if (ret_numeric)
	render_long(&result, value);
    else if (ret_boolean)
	render_boolean(&result, value);
    else
	tb_enquote(&result);

    TPRINTF(("-> %s%s\n",
	     is_error ? "*" : "",
	     is_error ? error_val : NONNULL(tb_values(result))));

    tb_free(&juggle);
    for (i = 0; i < nargs; i++) {
	tb_free(&args[i]);
    }
    returnString(is_error ? error_val : tb_values(result));
}

/* find a temp variable */
static UVAR *
lookup_tempvar(const char *name)
{
    UVAR *p;

    if (*name) {
	for (p = temp_vars; p != 0; p = p->next)
	    if (!vl_stricmp(name, p->u_name))
		return p;
    }
    return NULL;
}

/* find a state variable */
static int
lookup_statevar(const char *name)
{
    int vnum;			/* ordinal number of var referenced */
    if (!*name)
	return ILLEGAL_NUM;

    /* scan the list, looking for the referenced name */
    for (vnum = 0; statevars[vnum] != 0; vnum++)
	if (strcmp(name, statevars[vnum]) == 0)
	    return vnum;

    return ILLEGAL_NUM;
}

/*
 * Get a state variable's value.  This function may in turn be called by
 * the statevar_func[], so it has to manage a stack of results.
 */
static char *
get_statevar_val(int vnum)
{
    char *result = error_val;
    static TBUFF *buffer[9];
    static unsigned nested;
    int s;

    if (vnum != ILLEGAL_NUM) {
	unsigned old_level = nested;
	unsigned new_level = ((nested < TABLESIZE(buffer) - 1)
			      ? (nested + 1)
			      : nested);

	nested = new_level;
	tb_init(&buffer[nested], EOS);
	s = (*statevar_func[vnum]) (&buffer[nested], (const char *) NULL);

	if (s == TRUE) {
	    tb_append(&buffer[nested], EOS);	/* trailing null, just in case... */
	    result = tb_values(buffer[nested]);
	}
	nested = old_level;
    }
    return result;
}

static void
FindModeVar(char *var, VWRAP * vd)
{
    vd->v_num = lookup_statevar(var);
    if (vd->v_num != ILLEGAL_NUM)
	vd->v_type = VW_STATEVAR;
#if !SMALLER
    else {
	VALARGS args;
	if (is_mode_name(var, TRUE, &args) == TRUE) {
	    vd->v_type = VW_MODE;
	}
    }
#endif
}

/*
 * Find a variable's type and name
 */
static void
FindVar(char *var, VWRAP * vd)
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
    case '$':			/* check for legal state var */
	FindModeVar(var + 1, vd);
	break;

    case '%':			/* check for legal temp variable */
	vd->v_ptr = lookup_tempvar(var + 1);
	if (vd->v_ptr) {	/* existing */
	    vd->v_type = VW_TEMPVAR;
	} else {		/* new */
	    beginDisplay();
	    p = typealloc(UVAR);
	    if (p &&
		(p->u_name = strmalloc(var + 1)) != 0) {
		p->next = temp_vars;
		p->u_value = 0;
		temp_vars = vd->v_ptr = p;
		vd->v_type = VW_TEMPVAR;
		free_vars_cmpl();
	    }
	    endofDisplay();
	}
	break;

    case '&':			/* indirect operator? */
	if (vl_lookup_func(var) == UFINDIRECT) {
	    TBUFF *tok = 0;
	    /* grab token, and eval it */
	    execstr = get_token(execstr, &tok, eol_null, EOS, (int *) 0);
	    (void) vl_strncpy(var, tokval(tb_values(tok)), NLINE);
	    FindVar(var, vd);	/* recursive, but probably safe */
	    tb_free(&tok);
	}
	break;
    default:
	FindModeVar(var, vd);
	break;
    }
}

/*
 * Handles name-completion for variables.  Note that since the first character
 * can be either a '$' or '%', we'll postpone name-completion until we've seen
 * a '$'.
 */
static int
vars_complete(DONE_ARGS)
{
    int status;

    if (buf[0] == '$') {
	*pos -= 1;		/* account for leading '$', not in tables */
	status = kbd_complete(flags, c, buf + 1, pos,
			      (const char *) list_of_modes(), sizeof(char *));
	*pos += 1;
    } else if (buf[0] == '%') {
	*pos -= 1;		/* account for leading '%', not in tables */
	status = kbd_complete(KBD_MAYBEC, c, buf + 1, pos,
			      (const char *) init_vars_cmpl(), sizeof(char *));
	*pos += 1;
    } else {
	if (c != NAMEC)		/* cancel the unget */
	    unkeystroke(c);
	status = TRUE;
    }
    return status;
}

static int
PromptForVariableName(TBUFF **result)
{
    int status;
    static TBUFF *var;

    /* get the variable we want to set */
    if (var == 0)
	tb_scopy(&var, "");
    status = kbd_reply("Variable name: ", &var,
		       mode_eol, '=',
		       KBD_MAYBEC2 | KBD_NOEVAL | KBD_LOWERC,
		       vars_complete);
    *result = var;
    TRACE(("PromptForVariableName returns %s (%d)\n", tb_visible(*result), status));
    return status;
}

static int
PromptAndSet(const char *name, int f, int n)
{
    int status;
    VWRAP vd;
    char var[NLINE];
    char prompt[NLINE];

    TRACE((T_CALLED "PromptAndSet(%s, %d, %d)\n", name, f, n));

    /* look it up -- vd will describe the variable */
    FindVar(vl_strncpy(var, name, sizeof(var)), &vd);

    if (vd.v_type == VW_NOVAR) {
	mlforce("[Can't find variable '%s']", var);
	status = FALSE;
    } else if (vd.v_type == VW_MODE) {
	VALARGS args;
	(void) find_mode(curbp, var + 1, -TRUE, &args);
	set_end_string('=');
	status = adjvalueset(var + 1, FALSE, TRUE, -TRUE, &args);
    } else {
	static TBUFF *tmp;

	tb_init(&tmp, EOS);
	if (f == TRUE) {	/* new (numeric) value passed as arg */
	    render_int(&tmp, n);
	} else {		/* get it from user */
	    if (vd.v_type == VW_STATEVAR) {
		/* FIXME: this really should allow embedded nulls */
		tb_scopy(&tmp, get_statevar_val(vd.v_num));
	    } else {
		/* must be temp var */
		tb_scopy(&tmp, (vd.v_ptr->u_value) ? vd.v_ptr->u_value : "");
	    }
	    (void) lsprintf(prompt, "Value of %s: ", var);
	    status = mlreply2(prompt, &tmp);
	    if (status == ABORT)
		tb_error(&tmp);
	    else if (status != TRUE)
		returnCode(status);
	}

	status = SetVarValue(&vd, var, tb_values(tmp));

	if (status == ABORT) {
	    mlforce("[Variable %s is readonly]", var);
	} else if (status != TRUE) {
	    mlforce("[Cannot set %s to %s]", var, tb_values(tmp));
	}
    }

    returnCode(status);
}

static void
free_UVAR_value(UVAR * p)
{
    if (p->u_value != error_val
	&& p->u_value != 0) {
	beginDisplay();
	free(p->u_value);
	endofDisplay();
    }
    p->u_value = 0;
}

/*
 * Remove a temp variable.  That's the only type of variable that we can
 * remove.
 */
int
rmv_tempvar(const char *name)
{
    UVAR *p, *q;

    if (*name == '%')
	name++;

    for (p = temp_vars, q = 0; p != 0; q = p, p = p->next) {
	if (!vl_stricmp(p->u_name, name)) {
	    TRACE(("rmv_tempvar(%s) ok\n", name));
	    if (q != 0)
		q->next = p->next;
	    else
		temp_vars = p->next;
	    beginDisplay();
	    free_UVAR_value(p);
	    free(p->u_name);
	    free((char *) p);
	    free_vars_cmpl();
	    endofDisplay();
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

    TRACE(("set_state_variable(%s,%s)\n", name, value));

    /* it may not be "marked" yet -- unadorned names are assumed to
     *  be "state variables
     */
    if (toktyp(name) == TOK_LITSTR) {
	var[0] = '$';
	vl_strncpy(var + 1, name, sizeof(var) - 1);
    } else {
	vl_strncpy(var, name, sizeof(var));
    }

    if (value != NULL) {	/* value supplied */
	VWRAP vd;
	FindVar(var, &vd);
	status = SetVarValue(&vd, var, value);
    } else {			/* no value supplied:  interactive */
	status = PromptAndSet(var, FALSE, 0);
    }

    updatelistvariables();

    TRACE(("...set_state_variable ->%d\n", status));
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
    TBUFF *var;

    if ((status = PromptForVariableName(&var)) == TRUE) {
	status = PromptAndSet(tb_values(var), f, n);
    }
    updatelistvariables();
    return status;
}

/*
 * this is the externally bindable command function.
 * assign a variable a new value -- any type of variable.
 */
/* ARGSUSED */
int
unsetvar(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int status;
    TBUFF *var;

    if ((status = PromptForVariableName(&var)) == TRUE) {
	status = set_state_variable(tb_values(var), "");
    }
    updatelistvariables();
    return status;
}

/* int set_statevar_val(int vnum, const char *value);  */

static int
set_statevar_val(int vnum, const char *value)
{
    return (*statevar_func[vnum]) ((TBUFF **) 0, value);
}

/* figure out what type of variable we're setting, and do so */
static int
SetVarValue(VWRAP * var, const char *name, const char *value)
{
    int status;
    int savecmd;
    char *savestr;
    VALARGS args;

    if (isEmpty(name) || value == 0)
	return FALSE;

    status = TRUE;
    switch (var->v_type) {
    case VW_NOVAR:
	return FALSE;

    case VW_MODE:
	savestr = execstr;
	savecmd = clexec;
	execstr = TYPECAST(char, value);
	clexec = TRUE;
	(void) find_mode(curbp, name + 1, -TRUE, &args);
	set_end_string('=');
	status = adjvalueset(name + 1, FALSE, TRUE, -TRUE, &args);
	execstr = savestr;
	clexec = savecmd;
	break;

    case VW_TEMPVAR:
	beginDisplay();
	free_UVAR_value(var->v_ptr);
	if (value == error_val) {
	    var->v_ptr->u_value = error_val;
	} else if ((var->v_ptr->u_value = strmalloc(value)) == 0) {
	    status = FALSE;
	    var->v_ptr->u_value = error_val;
	}
	endofDisplay();
	break;

    case VW_STATEVAR:
	status = set_statevar_val(var->v_num, value);
	break;
    }

#if OPT_DEBUGMACROS
    if (tracemacros) {		/* we tracing macros? */
	if (var->v_type == TOK_STATEVAR)
	    mlforce("(((%s:&%s:%s)))",
		    render_boolean(rp, status),
		    statevars[var->v_num], value);
	else if (var->v_type == TOK_TEMPVAR)
	    mlforce("(((%s:%%%s:%s)))",
		    render_boolean(rp, status),
		    var->v_ptr->u_name, value);

	(void) update(TRUE);

	/* make the user hit a key */
	if (ABORTED(keystroke())) {
	    mlwarn("[Aborted]");
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
skip_cnumber(const char *s)
{
    while (isDigit(*s))
	s++;
    return s;
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
skip_number(char *s)
{
    while (isDigit(*s))
	s++;
    return s;
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

/*
 * Blanks and spaces are not synonymous
 */
char *
skip_space_tab(char *src)
{
    while (*src != EOS && isBlank(*src))
	++src;
    return src;
}

#if OPT_COLOR
void
set_ctrans(const char *thePalette)
{
    int n = 0, value;
    char *next;

    TRACE(("set_ctrans(%s)\n", thePalette));

    if (thePalette != 0) {
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
#if OPT_TRACE
	TRACE(("...ctrans, where not 1-1:\n"));
	for (n = 0; n < NCOLORS; n++) {
	    if (n != ctrans[n])
		TRACE(("\t[%d] = %d\n", n, ctrans[n]));
	}
#endif
    }
}
#endif

#if OPT_SHOW_COLORS
static void
show_attr(int color, const char *attr, const char *name)
{
    bprintf(" %c%d%sC", (char) CONTROL_A, (int) strlen(name), attr);
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
    int j, k, next;
    REGION region;

    bprintf("--- Color palette ");
    bpadc('-', term.cols - DOT.o);
    bputc('\n');

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
	next = DOT.o + 10;
	if (j >= 0)
	    bprintf("C%X", j);
	bpadc(' ', next - DOT.o);

	/*
	 * $palette values are in decimal, but it's useful to see the
	 * hexadecimal equivalents.
	 */
	next = DOT.o + 10;
	bprintf("%d", k);
	if (k > 9)
	    bprintf(" (%X)", k);
	bpadc(' ', next - DOT.o);

	show_attr(j, "", "Normal");
	show_attr(j, "B", "Bold");
	show_attr(j, "I", "Italic");
	show_attr(j, "U", "Underline");
	show_attr(j, "R", "Reverse");
    }
    bprintf("\n");		/* tweak to ensure we get final line */

    MK.l = lback(buf_head(curbp));
    MK.o = 0;
    DOT.l = lforw(buf_head(curbp));
    DOT.o = 0;

    if (getregion(&region) == TRUE)
	attribute_cntl_a_seqs_in_region(&region, FULLLINE);
}

/* ARGSUSED */
int
descolors(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return liststuff(PALETTE_COLORS_BufName,
		     FALSE, makecolorlist, 0, (void *) 0);
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
#endif /* OPT_UPBUFF */

#endif /* OPT_SHOW_COLORS */

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
    vile_refresh(FALSE, 0);
#endif
    return TRUE;
}
#endif

/* represent integer as string */
char *
render_int(TBUFF **rp, int i)
{
    char *p, *q;

    p = tb_values(tb_alloc(rp, 32));
    q = lsprintf(p, "%d", i);
    if (rp != 0 && q != 0 && p != 0)
	(*rp)->tb_used = (q - p + 1);
    return p;
}

/* represent long integer as string */
char *
render_long(TBUFF **rp, long i)
{
    char *p, *q;

    p = tb_values(tb_alloc(rp, 32));
    q = lsprintf(p, "%ld", i);
    if (rp != 0 && q != 0 && p != 0)
	(*rp)->tb_used = (q - p + 1);
    return p;
}

#if OPT_EVAL

/* represent boolean as string */
char *
render_boolean(TBUFF **rp, int val)
{
    static char *bools[] =
    {"FALSE", "TRUE"};
    return tb_values(tb_scopy(rp, bools[val ? 1 : 0]));
}

#if (SYS_WINNT||SYS_VMS)
/* unsigned to hex */
char *
render_hex(TBUFF **rp, unsigned i)
{
    char *p, *q;

    p = tb_values(tb_alloc(rp, 32));
    q = lsprintf(p, "%x", i);
    if (rp != 0 && q != 0 && p != 0)
	(*rp)->tb_used = (q - p + 1);
    return p;
}
#endif

/* pull an integer out of a string.  allow 'c' character constants as well */
long
scan_long(const char *s)
{
    char *ns;
    long l;

    if (s[0] == '\'' && s[2] == '\'' && s[3] == EOS)
	return s[1];

    l = strtol(s, &ns, 0);

    /* the endpointer better point at a 0, otherwise
     * it wasn't all digits */
    if (*s && !*ns)
	return l;

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
    return (strtol(val, 0, 0) != 0);
}

/* discriminate types of tokens */
int
toktyp(const char *tokn)
{
    if (isEmpty(tokn))
	return TOK_NULL;
    if (tokn[0] == DQUOTE)
	return TOK_QUOTSTR;
    if (tokn[0] == SQUOTE)
	return TOK_QUOTSTR;
#if ! SMALLER
    if (tokn[1] == EOS)
	return TOK_LITSTR;	/* only one character */
    switch (tokn[0]) {
    case '~':
	return TOK_DIRECTIVE;
    case '@':
	return TOK_QUERY;
    case '<':
	return TOK_BUFLINE;
    case '$':
	return TOK_STATEVAR;
    case '%':
	return TOK_TEMPVAR;
    case '&':
	return TOK_FUNCTION;
    case '*':
	return TOK_LABEL;

    default:
	return TOK_LITSTR;
    }
#else
    return TOK_LITSTR;
#endif
}

/*--------------------------------------------------------------------------*/

#if OPT_MACRO_ARGS

/* ARGSUSED */
static int
complete_integer(DONE_ARGS)
{
    char *tmp;

    (void) flags;
    (void) pos;
    (void) strtol(buf, &tmp, 0);
    if ((tmp != 0) && (tmp != buf) && (*tmp == 0) && isSpace(c)) {
	if (c != NAMEC)		/* put it back (cf: kbd_complete) */
	    unkeystroke(c);
	return TRUE;
    }
    return FALSE;
}

static int
complete_FSM(DONE_ARGS, const FSM_CHOICES * choices)
{
    int status;

    if (isDigit(*buf)) {	/* allow numbers for colors */
	status = complete_integer(PASS_DONE_ARGS);
    } else if (choices != 0) {
	status = kbd_complete(PASS_DONE_ARGS,
			      (const char *) choices,
			      sizeof(FSM_CHOICES));
    } else {
	status = FALSE;
    }

    return status;
}

static int
complete_boolean(DONE_ARGS)
{
    return complete_FSM(PASS_DONE_ARGS, fsm_bool_choices);
}

#if OPT_ENUM_MODES
static const FSM_CHOICES *complete_enum_ptr;

static int
complete_enum(DONE_ARGS)
{
    return complete_FSM(PASS_DONE_ARGS, complete_enum_ptr);
}
#endif

static int
complete_mode(DONE_ARGS)
{
    return kbd_complete(PASS_DONE_ARGS,
			(const char *) list_of_modes(),
			sizeof(char *));
}

static int
qs_vars_cmp(const void *a, const void *b)
{
    return vl_stricmp(*(const char *const *) a, (*(const char *const *) b));
}

static char **vars_cmpl_list;

static void
free_vars_cmpl(void)
{
    beginDisplay();
    FreeAndNull(vars_cmpl_list);
    endofDisplay();
}

static char **
init_vars_cmpl(void)
{
    int pass;
    UVAR *p;
    unsigned count = 0;

    if (vars_cmpl_list == 0) {
	for (pass = 0; pass < 2; pass++) {
	    count = 0;
	    for (p = temp_vars; p != 0; p = p->next) {
		if (pass != 0)
		    vars_cmpl_list[count] = p->u_name;
		count++;
	    }
	    if (pass == 0) {
		beginDisplay();
		vars_cmpl_list = typeallocn(char *, count + 1);
		endofDisplay();
	    }
	}

	if (vars_cmpl_list != 0) {
	    vars_cmpl_list[count] = 0;
	    qsort(vars_cmpl_list, count, sizeof(char *), qs_vars_cmp);
	}

    }
    return vars_cmpl_list;
}

static int
complete_vars(DONE_ARGS)
{
    int status = FALSE;
    char **nptr;

    if ((nptr = init_vars_cmpl()) != 0) {
	status = kbd_complete(PASS_DONE_ARGS,
			      (const char *) nptr,
			      sizeof(char *));
    }
    return status;
}

static int
read_argument(TBUFF **paramp, const PARAM_INFO * info)
{
    int status = TRUE;
    char *prompt;
    TBUFF *temp;
    int (*complete) (DONE_ARGS) = no_completion;
    int save_clexec;
    int save_isnamed;
    char *save_execstr;
    KBD_OPTIONS flags = 0;	/* no expansion, etc. */

    if (mac_tokval(paramp) == 0) {
	prompt = "String";
	if (info != 0) {
	    switch (info->pi_type) {
	    case PT_BOOL:
		prompt = "Boolean";
		complete = complete_boolean;
		break;
	    case PT_BUFFER:
		prompt = "Buffer";
		break;
	    case PT_DIR:
		prompt = "Directory";
		break;
#if OPT_ENUM_MODES
	    case PT_ENUM:
		prompt = "Enum";
		flags = KBD_NOEVAL | KBD_LOWERC;
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
		flags = KBD_NOEVAL | KBD_LOWERC;
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
		flags = KBD_NOEVAL | KBD_LOWERC | KBD_MAYBEC;
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

	save_clexec = clexec;
	save_execstr = execstr;
	save_isnamed = isnamedcmd;

	/*
	 * If we've run out of parameters on the command-line, force this
	 * to be interactive.
	 */
	if (execstr == 0 || !*skip_blanks(execstr)) {
	    clexec = FALSE;
	    isnamedcmd = TRUE;
	    execstr = "";
	}

	if (info->pi_type == PT_FILE) {
	    char fname[NFILEN];

	    status = mlreply_file(tb_values(temp), (TBUFF **) 0,
				  FILEC_UNKNOWN, fname);
	    if (status != ABORT)
		tb_scopy(paramp, fname);
	} else if (info->pi_type == PT_DIR) {
	    char fname[NFILEN];

	    status = mlreply_dir(tb_values(temp), (TBUFF **) 0, fname);
	    if (status != ABORT)
		tb_scopy(paramp, fname);
	} else if (info->pi_type == PT_BUFFER) {
	    char bname[NBUFN];

	    status = ask_for_bname(tb_values(temp), bname, sizeof(bname));
	    if (status != ABORT)
		tb_scopy(paramp, bname);
	} else {
	    status = kbd_reply(tb_values(temp),
			       paramp,	/* in/out buffer */
			       eol_history,
			       '\n',	/* expect a newline or return */
			       flags,	/* no expansion, etc. */
			       complete);
	}
	clexec = save_clexec;
	execstr = save_execstr;
	isnamedcmd = save_isnamed;

	tb_free(&temp);
    }
    return status;
}

/*
 * When first executing a macro, save the remainder of the argument string
 * as an array of strings.
 */
int
save_arguments(BUFFER *bp)
{
    PROC_ARGS *p;
    int num_args = 0;
    int max_args;
    char temp[NBUFN];
    const CMDFUNC *cmd = engl2fnc(strip_brackets(temp, bp->b_bname));
    int ok = TRUE;
    int fail = FALSE;
    int status;
    const char *cp;
    TBUFF **all_args;

    if (cmd != 0 && cmd->c_args != 0) {
	for (max_args = 0;
	     cmd->c_args[max_args].pi_type != PT_UNKNOWN;
	     max_args++) ;
    } else {
	max_args = 0;
    }

    TRACE((T_CALLED "save_arguments(%s) max_args=%d\n", bp->b_bname, max_args));

#define tb_allocn(n) typecallocn(TBUFF *, n)	/* FIXME: indent */

    beginDisplay();
    if ((p = typealloc(PROC_ARGS)) == 0) {
	status = no_memory("save_arguments");
    } else if ((all_args = tb_allocn(max_args + 1)) == 0) {
	status = no_memory("save_arguments");
	free(p);
    } else {
	status = TRUE;

	p->nxt_args = arg_stack;
	p->all_args = all_args;

	arg_stack = p;

	/*
	 * Remember the caller's $return variable, if any.
	 * Coming into a macro, we have no value in $return, but can leave $_
	 * as is.
	 */
	p->result = this_macro_result;
	this_macro_result = 0;

	tb_scopy(&(p->all_args[num_args]), bp->b_bname);
	if (p->all_args[num_args] == 0) {
	    fail = TRUE;
	} else {
	    ++num_args;

	    while (num_args < max_args + 1) {
		int copied;

		if (ok) {
		    status = read_argument(&(p->all_args[num_args]),
					   &(cmd->c_args[num_args - 1]));
		    if (status == ABORT) {
			break;
		    }
		    ok = (status == TRUE);
		}

		if (ok) {
		    cp = tokval(tb_values(p->all_args[num_args]));
		    if (cp != error_val) {
			tb_scopy(&(p->all_args[num_args]), cp);
			copied = TRUE;
		    } else {
			status = ABORT;
			break;
		    }
		} else {
		    tb_scopy(&(p->all_args[num_args]), "");
		    copied = TRUE;
		}

		if (copied && p->all_args[num_args] == 0) {
		    status = no_memory("save_arguments");
		    fail = TRUE;
		    break;
		}

		TPRINTF(("...ARG%d:%s\n",
			 num_args,
			 tb_values(p->all_args[num_args])));
		num_args++;
	    }

	    p->num_args = num_args - 1;
	}

	if (fail) {
	    arg_stack = p->nxt_args;
	    while (max_args >= 0) {
		tb_free(&(p->all_args[max_args--]));
	    }
	    free(p->all_args);
	    free(p);
	}
    }
    endofDisplay();

    updatelistvariables();
    returnCode(status);
}

/*
 * Pop the list of arguments off our stack.
 */
/* ARGSUSED */
void
restore_arguments(BUFFER *bp GCC_UNUSED)
{
    PROC_ARGS *p = arg_stack;

    TRACE(("restore_arguments(%s)\n", bp->b_bname));

    if (p != 0) {
	/*
	 * Restore the caller's $return variable.
	 */
	tb_free(&this_macro_result);
	this_macro_result = p->result;

	arg_stack = p->nxt_args;
	while (p->num_args >= 0) {
	    tb_free(&(p->all_args[p->num_args]));
	    p->num_args -= 1;
	}
	beginDisplay();
	free(p->all_args);
	free(p);
	endofDisplay();
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
    return NONNULL(result);
}

#endif /* OPT_MACRO_ARGS */

/*--------------------------------------------------------------------------*/

#if OPT_EVAL

/* the argument simply represents itself */
static const char *
simple_arg_eval(char *argp)
{
    return argp;
}

/* the returned value is the user's response to the specified prompt */
static const char *
query_arg_eval(char *argp)
{
    argp = user_reply(tokval(argp + 1), error_val);
    return argp ? argp : error_val;
}

/* the returned value is the next line from the named buffer */
static const char *
buffer_arg_eval(char *argp)
{
    argp = next_buffer_line(tokval(argp + 1));
    return argp ? argp : error_val;
}

/* expand it as a temp variable */
static const char *
tempvar_arg_eval(char *argp)
{
    static TBUFF *tmp;
    UVAR *p;
    char *result;
    if ((p = lookup_tempvar(argp + 1)) != 0) {
	tb_scopy(&tmp, p->u_value);
	result = tb_values(tmp);
    } else {
	result = error_val;
    }
    return NONNULL(result);
}

/* state variables are expanded.  if it's
 * not an statevar, perhaps it's a mode?
 */
static const char *
statevar_arg_eval(char *argp)
{
    int vnum;
    const char *result;

    if ((result = get_argument(++argp)) == error_val) {
	vnum = lookup_statevar(argp);
	if (vnum != ILLEGAL_NUM)
	    result = get_statevar_val(vnum);
#if !SMALLER
	else {
	    VALARGS args;
	    if (is_mode_name(argp, TRUE, &args) == TRUE)
		result = string_mode_val(&args);
	}
#endif
    }
    return result;
}

/* run a function to evalute it */
static const char *
function_arg_eval(char *argp)
{
    int fnum = vl_lookup_func(argp);
    return (fnum != ILLEGAL_NUM) ? run_func(fnum) : error_val;
}

/* goto targets evaluate to their line number in the buffer */
static const char *
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
static const char *
qstring_arg_eval(char *argp)
{
    return argp + 1;
}

static const char *
directive_arg_eval(char *argp)
{
    static TBUFF *tkbuf;
    char *result;
#if !SYS_UNIX
    /*
     * major hack alert -- the directives start with '~'.  on unix, we'd also
     * like to be able to expand ~user as a home directory.  handily, these two
     * uses don't conflict.  too much.
     *
     * For other systems, we can usually expand our own "home" directory.
     */
    if (strncmp("~/", argp, 2))
	return error_val;
#endif
    if (argp == 0)
	argp = "";
    tb_alloc(&tkbuf, NFILEN + strlen(argp));
    if (tb_values(tkbuf) != 0)
	result = lengthen_path(strcpy(tb_values(tkbuf), argp));
    else
	result = "";
    return result;
}

typedef const char *(ArgEvalFunc) (char *argp);

static ArgEvalFunc *eval_func[] =
{
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
const char *
tokval(char *tokn)
{
    const char *result;

#if OPT_EVAL
    {
	int toknum = toktyp(tokn);
	if (toknum < 0 || toknum > MAXTOKTYPE)
	    result = error_val;
	else if (toknum == TOK_DIRECTIVE || toknum == TOK_LABEL)
	    result = tokn;
	else
	    result = (*eval_func[toknum]) (tokn);
	TRACE2(("tokval(%s) = %s\n", TRACE_NULL(tokn), TRACE_NULL(result)));
    }
#else
    result = (toktyp(tokn) == TOK_QUOTSTR) ? tokn + 1 : tokn;
#endif
    return (result);
}

/*
 * Return true if the argument is one of the strings that we accept as a
 * synonym for "true".
 */
int
is_truem(const char *val)
{
    char temp[8];
    (void) mklower(strncpy0(temp, val, sizeof(temp)));
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
    char temp[8];
    (void) mklower(strncpy0(temp, val, sizeof(temp)));
#if OPT_BOOL_CHOICES
    return choice_to_code(fsm_bool_choices, temp, strlen(temp)) == FALSE;
#else
    return !strncmp(temp, "false", strlen(temp));
#endif
}

#if OPT_EVAL
/*
 * Check if the next token will be something that we can evaluate, i.e.,
 * a function.
 */
static int
can_evaluate(void)
{
    int result = FALSE;
    int type;
    char *save = execstr;
    TBUFF *tok = 0;

    if (mac_token(&tok)) {
	type = toktyp(tb_values(tok));
	result = (type == TOK_FUNCTION);
    }
    tb_free(&tok);
    execstr = save;
    return result;
}

int
evaluate(int f, int n)
{
    static PARAM_INFO info =
    {PT_STR, "Expression", 0};
    TBUFF *params = 0, *tok = 0, *cmd = 0;
    char *old, *tmp;
    int code = FALSE;

    if (read_argument(&params, &info) == TRUE) {
	if ((tmp = tb_values(params)) != error_val && tmp != 0) {
	    old = execstr;
	    execstr = tmp;
	    TRACE(("EVAL %s\n", execstr));
	    for (;;) {
		if (can_evaluate()) {
		    if ((tmp = mac_tokval(&tok)) != 0) {
			if (tb_length(cmd))
			    tb_sappend0(&cmd, " ");
			tb_sappend0(&cmd, tmp);
		    }
		} else if (mac_token(&tok)) {
		    if (tb_length(cmd))
			tb_sappend0(&cmd, " ");
		    tb_sappend0(&cmd, tb_values(tok));
		} else {
		    break;
		}
	    }
	    if ((tmp = tb_values(cmd)) != 0) {
		execstr = tmp;
		TRACE(("...docmd {%s}\n", execstr));
		code = docmd(tmp, TRUE, f, n);
	    }
	    execstr = old;
	}
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
static size_t
s2size(const char *s)
{
    int n = strtol(s, 0, 0);
    if (n < 0)
	n = 0;
    return n;
}

/* use this to return a pointer to the string after the n-1 first characters */
static const char *
s2offset(const char *s, const char *n)
{
    size_t len = strlen(s) + 1;
    size_t off = s2size(n);
    if (off > len)
	off = len;
    if (off == 0)
	off = 1;
    return s + (off - 1);
}

/*
 * Line Labels begin with a "*" as the first nonblank char, like:
 *
 *   *LBL01
 */
LINE *
label2lp(BUFFER *bp, const char *label)
{
    LINE *result = 0;

    if (bp != 0 && label != 0 && *label != EOS) {
	LINE *glp;
	size_t len = strlen(label);
	int need = (int) len + 1;
	int col;

	if (len > 1) {
	    for_each_line(glp, bp) {
		if (llength(glp) >= need) {
		    for (col = 0; col < llength(glp); ++col)
			if (!isSpace(glp->l_text[col]))
			    break;
		    if (llength(glp) >= need + col
			&& glp->l_text[col] == '*'
			&& !memcmp(&glp->l_text[col + 1], label, len)) {
			result = glp;
			break;
		    }
		}
	    }
	}
	TRACE(("label2lp(%s) ->%d\n", label, line_no(curbp, result)));
    }
    return result;
}

#endif

#if OPT_EVAL || !SMALLER
char *
mkupper(char *s)
{
    if (s != error_val && s != 0) {
	char *sp;

	for (sp = s; *sp; sp++)
	    if (isLower(*sp))
		*sp = (char) toUpper(*sp);

    }
    return s;
}
#endif

char *
mklower(char *s)
{
    if (s != error_val && s != 0) {
	char *sp;

	for (sp = s; *sp; sp++)
	    if (isUpper(*sp))
		*sp = (char) toLower(*sp);

    }
    return s;
}

char *
mktrimmed(char *str)
{				/* trim whitespace */
    char *base = str;
    char *dst = str;

    if (str != error_val && str != 0) {
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
    }
    return base;
}

int
absol(int x)
{				/* return absolute value of given integer */
    return (x < 0) ? -x : x;
}

#if !SMALLER
/*
 * Returns true if the buffer contains blanks or quotes which would confuse us
 * when parsing.
 */
int
must_quote_token(const char *values, size_t last)
{
    size_t n;

    if (last != 0 && values != 0) {
	for (n = 0; n < last; n++) {
	    int ch = CharOf(values[n]);
	    if (ch == SQUOTE
		|| ch == DQUOTE
		|| ch == BACKSLASH
		|| !isGraph(ch))
		return TRUE;
	}
    }
    return FALSE;
}

/*
 * Appends the buffer, with quotes
 */
void
append_quoted_token(TBUFF **dst, const char *values, size_t last)
{
    size_t n;

    TRACE(("append_quoted_token\n"));
    tb_append(dst, DQUOTE);
    for (n = 0; n < last; n++) {
	int ch = CharOf(values[n]);
	switch (ch) {
	case DQUOTE:
	case BACKSLASH:
	    tb_append(dst, BACKSLASH);
	    tb_append(dst, ch);
	    break;
	case '\b':
	    tb_sappend(dst, "\\b");
	    break;
	case '\t':
	    tb_sappend(dst, "\\t");
	    break;
	case '\r':
	    tb_sappend(dst, "\\r");
	    break;
	case '\n':
	    tb_sappend(dst, "\\n");
	    break;
	default:
	    /* as for other characters, including nonprinting ones, they are
	     * not a problem
	     */
	    tb_append(dst, ch);
	    break;
	}
    }
    tb_append(dst, DQUOTE);
}
#endif /* !SMALLER */

#if NO_LEAKS
void
vars_leaks(void)
{
#if OPT_EVAL
    free_vars_cmpl();
#endif
}
#endif
