/*
 *	eval.c -- function and variable evaluation
 *	original by Daniel Lawrence
 *
 * $Id: eval.c,v 1.473 2025/01/26 14:08:10 tom Exp $
 */

#include	<estruct.h>
#include	<edef.h>
#include	<nevars.h>
#include	<nefsms.h>

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
static int SetVarValue(VWRAP * var, char *name, const char *value);
static void FindVar(char *var, VWRAP * vd);
static void free_vars_cmpl(void);
#endif

/*--------------------------------------------------------------------------*/
#if OPT_SHOW_CTYPE

static int show_ctypes_f;
static int show_ctypes_n;

#define same_string(a,b) (((a) && (b) && !strcmp(a,b)) || !((a) || (b)))

static void
one_ctype_row(VL_CTYPE2 * enc, int ch)
{
    UINT j;
    CHARTYPE test;
    CHARTYPE used;
    const char *s;

#if OPT_MULTIBYTE
    int rc;
    char temp[MAX_UTF8];
    int use_locale = same_string(enc->locale, vl_wide_enc.locale);
#else
    (void) enc;
#endif

    bprintf("\n%d\t", (int) ch);
    if ((ch == '\n') || (ch == '\t')) {		/* vtlistc() may not do these */
	bprintf("^%c", (int) ('@' | ch));
    } else {
#if OPT_MULTIBYTE
	if (use_locale
	    && (rc = vl_conv_to_utf8((UCHAR *) temp,
				     (UINT) ch,
				     sizeof(temp))) > 1) {
	    bputsn(temp, rc);
	} else
#endif
	    bputc(ch);
    }
    bputc('\t');

#if OPT_MULTIBYTE
    used = vl_ctype_bits(ch, use_locale);
#else
    used = vl_chartypes_[ch + 1];
#endif

    for (j = 0; j != vl_UNUSED; j++) {
	if ((s = choice_to_name(&fsm_charclass_blist, (int) j)) != NULL) {

	    test = (CHARTYPE) (1 << j);

	    if (j != 0 && *s != EOS)
		bputc(' ');
	    bprintf("%*s",
		    (int) strlen(s),
		    ((used & test)
		     ? s
		     : "-"));
	}
    }
}

/* list the current character-classes into the current buffer */
/* ARGSUSED */
static void
make_ctype_list(int dum1 GCC_UNUSED, void *ptr GCC_UNUSED)
{
    int first = (show_ctypes_n * 256);
    int last = first + N_chars;
    int i;
    VL_CTYPE2 *enc = &vl_real_enc;

#if OPT_MULTIBYTE
    make_local_b_val(curbp, VAL_FILE_ENCODING);
    if ((vl_encoding > enc_8BIT) && show_ctypes_f) {
	set_b_val(curbp, VAL_FILE_ENCODING, vl_encoding);
	enc = &vl_wide_enc;
    } else {
	/*
	 * Keep the buffer encoding 8-bit to make it work with the character
	 * classes for the narrow locale.
	 */
	set_b_val(curbp, VAL_FILE_ENCODING, enc_8BIT);
    }
#endif

#if defined(WMDSHOWCHAR) && OPT_MULTIBYTE
    /*
     * If "showchar" is set, and the current buffer is not this one, show
     * the row for the current character as well.  Slow, but as noted, useful
     * for debugging.
     */
    if (w_val(curwp, WMDSHOWCHAR)) {
	WINDOW *wp = recomputing_win();
	BUFFER *bp = recomputing_buf();

	if (wp != NULL && bp != NULL && (curbp != bp)) {
	    VL_CTYPE2 *my_enc = (b_is_utfXX(bp)
				 ? &vl_wide_enc
				 : &vl_real_enc);
	    BUFFER *savebp = curbp;
	    int savech;

	    bprintf("-- Current Character in %s, locale %s (%s) ",
		    bp->b_bname,
		    (my_enc->locale
		     ? my_enc->locale
		     : NONNULL(enc->locale)),
		    NONNULL(encoding2s(b_val(bp, VAL_FILE_ENCODING))));
	    bpadc('-', term.cols - DOT.o);
	    bputc('\n');

	    curbp = bp;
	    savech = char_at_mark(wp->w_dot);
	    curbp = savebp;

	    one_ctype_row(my_enc, savech);
	    bputc('\n');
	    bputc('\n');
	}
    }
#endif

    bprintf("--- Printable Characters for locale %s (%s) ",
	    NONNULL(enc->locale),
	    NONNULL(enc->encoding));
    bpadc('-', term.cols - DOT.o);
    bputc('\n');

    for (i = first; i < last; i++) {
	one_ctype_row(enc, i);
    }
}

/* ARGSUSED */
static int
show_CharClasses(BUFFER *bp)
{
    int rc;

    rc = liststuff(PRINTABLECHARS_BufName,
		   -TRUE, make_ctype_list, 0, (void *) 0);
    if ((bp = find_b_name(PRINTABLECHARS_BufName)) != NULL) {
	set_b_val(bp, VAL_TAB, 7);
    }
    return rc;
}

#if OPT_UPBUFF
void
update_char_classes(void)
{
    update_scratch(PRINTABLECHARS_BufName, show_CharClasses);
}
#endif

/* ARGSUSED */
int
desprint(int f, int n GCC_UNUSED)
{
    show_ctypes_f = f;
    show_ctypes_n = 0;		/* always page 0 with this command */
    return show_CharClasses(curbp);
}

#if OPT_MULTIBYTE
/*
 * Show just wide-characters, if they are available.  The 'f' parameter is only
 * true if 'n' is greater than zero, so a separate function from desprint() is
 * needed if we are allowing the user to select pages of printing information.
 */
int
deswprint(int f, int n)
{
    if (isEmpty(vl_wide_enc.locale) || isEmpty(vl_narrow_enc.locale)) {
	show_ctypes_f = f;
	show_ctypes_n = 0;
    } else {
	show_ctypes_f = TRUE;
	show_ctypes_n = f ? n : 0;
    }
    return show_CharClasses(curbp);
}
#endif

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

    if (var == NULL)
	tb_scopy(&var, "");
    status = kbd_reply("Character class: ", &var,
		       eol_history, '=',
		       KBD_NOEVAL | KBD_LOWERC,
		       cclass_complete);
    return (status == TRUE)
	? choice_to_code(&fsm_charclass_blist,
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

    if (var == NULL)
	tb_scopy(&var, "");
    status = kbd_reply("Character pattern: ", &var,
		       eol_history, '=', KBD_NOEVAL, no_completion);
    return ((status == TRUE)
	    ? new_regexval(tb_values(var), TRUE)
	    : NULL);
}

static int
match_charclass_regexp(int ch, REGEXVAL * exp)
{
    char temp[2];
    temp[0] = (char) ch;

    return nregexec(exp->reg, temp, temp + 1, 0, 0, FALSE);
}

static int
update_charclasses(const char *tag, int count)
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
    int rc = FALSE;
    int ch;
    int code;
    int count;
    CHARTYPE ctype;
    REGEXVAL *exp;

    if ((code = get_charclass_code()) >= 0) {
	ctype = (CHARTYPE) (1 << code);
	if ((exp = get_charclass_regexp()) != NULL) {
	    for (count = 0, ch = 0; ch < N_chars; ch++) {
		if (!istype(ctype, ch)
		    && match_charclass_regexp(ch, exp)) {
		    vl_ctype_set(ch, ctype);
		    count++;
		}
	    }
	    free_regexval(exp);
	    rc = update_charclasses("Set", count);
	}
    }
    return rc;
}

/* ARGSUSED */
int
unset_charclass(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int rc = FALSE;
    int ch;
    int code;
    int count;
    CHARTYPE ctype;
    REGEXVAL *exp;

    if ((code = get_charclass_code()) >= 0) {
	ctype = (CHARTYPE) (1 << code);
	if ((exp = get_charclass_regexp()) != NULL) {
	    for (count = 0, ch = 0; ch < N_chars; ch++) {
		if (istype(ctype, ch)
		    && match_charclass_regexp(ch, exp)) {
		    vl_ctype_clr(ch, ctype);
		    count++;
		}
	    }
	    free_regexval(exp);
	    rc = update_charclasses("Unset", count);
	}
    }
    return rc;
}

void
rebuild_charclasses(int print_lo, int print_hi)
{
    vl_ctype_init(print_lo, print_hi);
    vl_ctype_apply();
    update_char_classes();
}

/* ARGSUSED */
int
reset_charclasses(int f GCC_UNUSED, int n GCC_UNUSED)
{
    vl_ctype_init(global_g_val(GVAL_PRINT_LOW),
		  global_g_val(GVAL_PRINT_HIGH));
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
    if (isErrorVal(arg)) {
	k = 0;
    } else {
	k = vl_chartypes_[CharOf(*arg) + 1];
	if (*arg) {
	    while (*++arg) {
		k &= vl_chartypes_[CharOf(*arg) + 1];
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
	if (((CHARTYPE) (1 << j) & k) != 0
	    && (s = choice_to_name(&fsm_charclass_blist, (int) j)) != NULL) {
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
static unsigned vars_to_list;

/* list the current vars into the current buffer */
/* ARGSUSED */
static void
makevarslist(int dum1 GCC_UNUSED, void *ptr)
{
    UVAR *p;
    int j, k;
    int first = TRUE;

    if (vars_to_list & 1) {
	bprintf("--- State variables ");
	bpadc('-', term.cols - DOT.o);
	bputc('\n');

	for (p = (UVAR *) ptr; p->u_name != NULL; ++p) {
	    bprintf("\n$%s = ", p->u_name);
	    bputsn_xcolor(p->u_value,
			  (int) strlen(p->u_value),
			  ((p->u_type == VALTYPE_ENUM ||
			    p->u_type == VALTYPE_BOOL ||
			    p->u_type == VALTYPE_MAJOR)
			   ? XCOLOR_ENUM
			   : ((p->u_type == VALTYPE_REGEX)
			      ? XCOLOR_REGEX
			      : (p->u_type == VALTYPE_STRING
				 ? XCOLOR_STRING
				 : (p->u_type == VALTYPE_INT
				    ? XCOLOR_NUMBER
				    : (p->u_type == (char) VALTYPE_UNKNOWN
				       ? XCOLOR_WARNING
				       : XCOLOR_NONE))))));
	}
	first = FALSE;
    }

    if (vars_to_list & 2) {
	if (arg_stack != NULL && ((k = arg_stack->num_args) != 0)) {
	    if (!first)
		bputc('\n');
	    bprintf("--- %s parameters ",
		    tb_values(arg_stack->all_args[0]));
	    bpadc('-', term.cols - DOT.o);
	    for (j = 1; j <= k; j++) {
		bprintf("\n$%d = ", j);
		bputsn_xcolor(tb_values(arg_stack->all_args[j]),
			      (int) tb_length(arg_stack->all_args[j]),
			      XCOLOR_STRING);
	    }
	    first = FALSE;
	}
    }
    if (vars_to_list & 4) {
	if (temp_vars != NULL) {
	    if (!first)
		bputc('\n');
	    for (p = temp_vars, j = 0; p != NULL; p = p->next) {
		if (!j++) {
		    bprintf("--- Temporary variables ");
		    bpadc('-', term.cols - DOT.o);
		}
		bprintf("\n%%%s = ", p->u_name);
		if (p->u_value) {
		    bputsn_xcolor(p->u_value,
				  (int) strlen(p->u_value),
				  XCOLOR_STRING);
		}
	    }
	}
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
get_listvalue(const char *name, int showall, int *typep)
{
    const char *result = NULL;
    VALARGS args;
    int s;
    int vnum;

    *typep = VALTYPE_STRING;
    if ((s = is_mode_name(name, showall, &args)) == TRUE) {
	result = string_mode_val(&args);
	*typep = args.names->type;
    } else if (s == SORTOFTRUE) {
	vnum = vl_lookup_statevar(name);
	if (vnum != ILLEGAL_NUM) {
	    result = get_statevar_val(vnum);
	    *typep = statevar_funcs[vnum].type;
	}
    }
    return result;
}
#endif /* OPT_SHOW_EVAL */

#if OPT_SHOW_EVAL

static int show_vars_f;
static int show_vars_n;

/* ARGSUSED */
static int
show_VariableList(BUFFER *bp GCC_UNUSED)
{
    WINDOW *wp = curwp;
    int s;
    size_t t;
    char *v;
    const char *vv;
    const char *const *Names;
    int showall;
    int type;
    int rc = FALSE;
    UVAR *list = NULL;

    TRACE((T_CALLED "show_VariableList()\n"));

    Names = show_vars_f ? list_of_modes() : statevars;
    if (Names != NULL) {
	showall = show_vars_f ? (show_vars_n > 1) : FALSE;

	beginDisplay();
	/* count the variables */
	for (t = 0; Names[t] != NULL; t++) {
	    ;
	}
	++t;			/* guarantee a null entry on the end */

	if ((list = typecallocn(UVAR, t)) != NULL) {
	    /* collect data for state-variables, since some depend on window */
	    for (s = 0; Names[s] != NULL; s++) {
		list[s].u_name = (char *) Names[s];
		list[s].u_type = VALTYPE_STRING;
		if ((vv = get_listvalue(Names[s], showall, &type)) != NULL) {
		    list[s].u_type = (char) type;
		    if (isErrorVal(vv))
			list[s].u_type = VALTYPE_UNKNOWN;
		} else {
		    vv = "";
		}
		list[s].u_value = v = strmalloc(vv);
		t = strlen(v);
		v += (t + 1);
		if ((t != 0) && (vv[t - 1] == '\n')) {
		    v[-2] = EOS;	/* suppress trailing newline */
		}
	    }

	    rc = liststuff(VARIABLES_BufName, FALSE,
			   makevarslist, 0, (void *) list);
	    /* back to the buffer whose modes we just listed */
	    swbuffer(wp->w_bufp);

	    for (s = 0; Names[s] != NULL; s++) {
		free(list[s].u_value);
	    }
	    FreeIfNeeded(list);
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
    vars_to_list = 7;
    return show_VariableList(curbp);
}
#endif /* OPT_SHOW_EVAL */

#if OPT_EVAL
/* ARGSUSED */
static void
describe_dlr_vars(int dum1 GCC_UNUSED, void *ptr GCC_UNUSED)
{
    int j;
    char temp[NSTRING];
    const char *what;

    for (j = 0; statevar_funcs[j].func != NULL; ++j) {
	if (j)
	    bputc('\n');
	lsprintf(temp, "\"%s\"", statevars[j]);
	bputsn_xcolor(temp, -1, XCOLOR_STRING);
	bputc('\n');
	switch (statevar_funcs[j].type) {
	case VALTYPE_INT:
	    what = "integer";
	    break;
	case VALTYPE_STRING:
	    what = "string";
	    break;
	case VALTYPE_BOOL:
	    what = "bool";
	    break;
	case VALTYPE_REGEX:
	    what = "regex";
	    break;
	case VALTYPE_ENUM:
	    what = "enum";
	    break;
	case VALTYPE_MAJOR:
	    what = "majormode";
	    break;
	default:
	    what = "?";
	}
	bprintf("  ( %s )\n", what);
	bprintf("  ( %s )", statevar_funcs[j].help);
    }
}

int
des_dlr_vars(int f GCC_UNUSED, int n GCC_UNUSED)
{
    show_vars_f = f;
    show_vars_n = n;
    return liststuff(DLR_VARIABLES_BufName, FALSE,
		     describe_dlr_vars, 0, (void *) 0);
}

int
list_dlr_vars(int f GCC_UNUSED, int n GCC_UNUSED)
{
    show_vars_f = f;
    show_vars_n = n;
    vars_to_list = 1;
    return show_VariableList(curbp);
}

int
list_user_vars(int f GCC_UNUSED, int n GCC_UNUSED)
{
    show_vars_f = f;
    show_vars_n = n;
    vars_to_list = 4;
    return show_VariableList(curbp);
}
#endif

#define UF_PARAMS(code) \
	     ((code & NUM) \
	      ? "numeric" \
	      : ((code & BOOL) \
		 ? "boolean" \
		 : "string"))

#define UF_RETURN(code) \
	     ((code & NRET) \
	      ? "numeric" \
	      : ((code & BRET) \
		 ? "boolean" \
		 : "string"))

#if OPT_EVAL
/* ARGSUSED */
static void
list_amp_funcs(int dum1 GCC_UNUSED, void *ptr GCC_UNUSED)
{
    int n;
    char temp[NSTRING];

    bprintf("--- &functions ");
    bpadc('-', term.cols - DOT.o);

    for (n = 0; vl_ufuncs[n].f_name != NULL; ++n) {
	int nparams = (int) (vl_ufuncs[n].f_code & NARGMASK);
	const char *t_params = UF_PARAMS(vl_ufuncs[n].f_code);
	const char *t_return = UF_RETURN(vl_ufuncs[n].f_code);

	if (n)
	    bputc('\n');
	bputc('\n');
	lsprintf(temp, "\"&%s\"", vl_ufuncs[n].f_name);
	bputsn_xcolor(temp, -1, XCOLOR_STRING);
	bputc('\n');
#if OPT_ONLINEHELP
	bprintf("  ( %s )\n", vl_ufuncs[n].f_help);
#endif
	bprintf("  ( %d %s parameter%s )\n", nparams, t_params, PLURAL(nparams));
	bprintf("  ( returns %s )", t_return);
    }
}

int
des_amp_funcs(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return liststuff(AMP_FUNCTIONS_BufName, FALSE,
		     list_amp_funcs, 0, (void *) 0);
}
#endif

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

    if (name[0] == '&'
	&& name[1] != EOS) {

	mklower(vl_strncpy(downcased, name + 1, sizeof(downcased)));
	fnum = blist_pmatch(&blist_ufuncs, downcased, -1);
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

    if (tb_scopy(result, string) != NULL) {
	if ((s = tb_values(*result)) != NULL) {
	    for (; *s != EOS; ++s) {
		if ((t = vl_index(from, *s)) != NULL
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
    if (isLegalVal(temp)) {
	*pathleaf(temp) = EOS;
	path_trim(temp);
    }
    return temp;
}

/*
 * If the given argument contains non-path characters, quote it.  Otherwise
 * simply copy it.
 */
void
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
    char *s;

    FindVar(name, &vd);
    switch (vd.v_type) {
    case VW_STATEVAR:
	s = init_state_value(vd.v_num);
	tb_scopy(result, s);
	free(s);
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
		free_val(args.names, &mode);
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
    char *result = NULL;
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
	    result = error_val;
	} else {
	    if (strip) {
		if (((const char *) previous) != newvalue) {
		    TBUFF *fix = NULL;
		    tb_scopy(&fix, newvalue);
		    tb_free(tok);
		    *tok = fix;
		}
		tb_dequote(tok);
	    } else if (((const char *) previous) != newvalue) {
		tb_scopy(tok, newvalue);
	    }
	    result = tb_values(*tok);
	}
    } else {
	tb_free(tok);
    }
    return (result);
}

/*
 * Prompt for completion of the given "value", selecting possible completions
 * from a given "space".
 *
 * The "space" parameter can be (any unique abbreviation for):
 * buffer, command, directory, filename, tags
 *
 * Returns false (and reports message) in any of these cases:
 * a) no match for space parameter
 * b) user cancels the prompt.
 *
 * FIXME: there are other interesting spaces, such as environment, as well as
 * vile's Majormode, mode, variable
 */
static int
get_completion(TBUFF **result, const char *space, const char *value)
{
    int code = FALSE;
    int save_msgs = vl_msgs;
    int save_clexec = clexec;
    int save_isname = isnamedcmd;
    char fname[NFILEN];

    TRACE((T_CALLED "get_completion(%s:%s)\n", NonNull(space), NonNull(value)));

    clexec = FALSE;		/* this is an interactive feature */
    vl_msgs = TRUE;

    if (!isEmpty(space) && !isErrorVal(space)) {
	size_t len = strlen(space);
	KBD_OPTIONS kbd_namec = ((NAMEC != ' ') ? 0 : KBD_MAYBEC);

	fname[0] = EOS;
	tb_scopy(result, value);

	if (!strncmp(space, "buffer", len)) {
	    KBD_OPTIONS kbd_flags = (KBD_NORMAL
				     | kbd_namec);
	    code = kbd_reply("Buffer: ", result, eol_history, '\n',
			     kbd_flags, bname_complete);
	} else if (!strncmp(space, "command", len)) {
	    KBD_OPTIONS kbd_flags = (KBD_EXPCMD
				     | KBD_NULLOK
				     | kbd_namec);
	    code = kbd_reply("Command: ", result, eol_command, ' ',
			     kbd_flags, cmd_complete);
	} else if (!strncmp(space, "directory", len)) {
	    isnamedcmd = TRUE;	/* do not read from screen... */
	    code = mlreply_dir("Directory: ", result, fname);
	    tb_scopy(result, fname);
	} else if (!strncmp(space, "filename", len)) {
	    isnamedcmd = TRUE;	/* do not read from screen... */
	    code = mlreply_file("Filename: ", result,
				FILEC_READ | FILEC_PROMPT, fname);
	    tb_scopy(result, fname);
	} else if (!strncmp(space, "register", len)) {
	    code = kbd_reply("Register name: ", result, eol_history, '\n',
			     regs_kbd_options(), regs_completion);
	} else if (!strncmp(space, "tags", len)) {
	    code = kbd_reply("Tag name: ", result, eol_history, '\n',
			     tags_kbd_options(), tags_completion);
	} else {
	    code = FALSE;
	}
    }

    isnamedcmd = save_isname;
    vl_msgs = save_msgs;
    clexec = save_clexec;
    returnCode(code);
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
	result = (find_b_name(value) != NULL);
    } else if (!strncmp(classname, COLOR_CLASSNAME, len)) {
#if OPT_COLOR_CHOICES
	result = (choice_to_code(&fsm_color_blist, value, len) != ENUM_ILLEGAL);
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

/*
 * Implements "&gtmotion".
 */
static int
ufunc_get_motion(TBUFF **result, const char *name)
{
    int rc = FALSE;
    int key = kbd_seq();
    const BINDINGS *bs = vl_get_binding(name);
    const CMDFUNC *cmd = kcod2fnc(bs, key);

    if (cmd != NULL && (cmd->c_flags & MOTION)) {
	tb_sappend0(result, fnc2engl(cmd));
	rc = TRUE;
    }
    return rc;
}

int
eval_fchanged(BUFFER *bp)
{
    time_t current;
    int changed;

    if (valid_buffer(bp)
	&& get_modtime(bp, &current)) {
	changed = (current != bp->b_modtime);
    } else {
	changed = FALSE;
    }
    return changed;
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
    TBUFF *juggle = NULL;
    char *arg[MAXARGS];		/* function arguments */
    char *cp;
    const char *sp;
    int args_numeric, args_boolean, ret_numeric, ret_boolean;
    int bools[MAXARGS];
    int i, nargs;
    int is_error = FALSE;
    long value = 0;
    long nums[MAXARGS];
    int old_reading = read_msgline(TRUE);

    TRACE((T_CALLED "run_func(%d:%s)\n", fnum, vl_ufuncs[fnum].f_name));

    nargs = (int) (vl_ufuncs[fnum].f_code & NARGMASK);
    args_numeric = (int) (vl_ufuncs[fnum].f_code & NUM);
    args_boolean = (int) (vl_ufuncs[fnum].f_code & BOOL);

    ret_numeric = (int) (vl_ufuncs[fnum].f_code & NRET);
    ret_boolean = (int) (vl_ufuncs[fnum].f_code & BRET);

    TPRINTF(("** evaluate '%s' (0x%x), %d %s args returning %s\n",
	     vl_ufuncs[fnum].f_name,
	     vl_ufuncs[fnum].f_code,
	     nargs,
	     UF_PARAMS(vl_ufuncs[fnum].f_code),
	     UF_RETURN(vl_ufuncs[fnum].f_code)));

    for (i = 0; i < MAXARGS; ++i) {
	arg[i] = error_val;
	args[i] = NULL;
	nums[i] = 0;
	bools[i] = 0;
    }

    /* fetch required arguments */
    for (i = 0; i < nargs; i++) {
	if ((arg[i] = dequoted_parameter(&args[i])) == NULL
	    || isErrorVal(arg[i])
	    || isTB_ERRS(args[i])) {
	    arg[i] = error_val;
	    is_error = TRUE;
	}
	tb_init(&result, EOS);	/* in case mac_tokval() called us */
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
	value = isErrorVal(arg[0]);
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
	if (isLegalExp(sp, tokval(arg[0])))
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
    case UFGET_KEY:
    case UFGTKEY:
	tb_append(&result, (char) keystroke_raw8());
	tb_append(&result, EOS);
	break;
    case UFGET_MOTION:
    case UFGTMOTION:
	if (!ufunc_get_motion(&result, arg[0]))
	    is_error = TRUE;
	break;
    case UFGET_SEQ:
    case UFGTSEQ:
	(void) kcod2escape_seq(kbd_seq_nomap(), tb_values(result), result->tb_size);
	tb_setlen(&result, -1);
	break;
    case UFGET_COMPLETE:
	if (get_completion(&result, arg[0], arg[1]) != TRUE)
	    is_error = TRUE;
	break;
    case UFCMATCH:
	if ((exp = new_regexval(arg[0], TRUE)) != NULL) {
	    value = nregexec(exp->reg, arg[1], (char *) 0, 0, -1, TRUE);
	}
	break;
    case UFMATCH:
	if ((exp = new_regexval(arg[0], TRUE)) != NULL)
	    value = nregexec(exp->reg, arg[1], (char *) 0, 0, -1, FALSE);
	break;
    case UFRANDOM:		/* FALLTHRU */
    case UFRND:
	if (nums[0] < 1)
	    is_error = TRUE;
	if (!is_error)
	    value = rand() % nums[0] + 1;	/* return 1 to N */
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
	    if (isErrorVal(sp = prc2engl(arg[0])))
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
	    && (bp = find_any_buffer(arg[0])) != NULL
	    && !is_internalname(bp->b_fname))
	    value = (long) file_modified(bp->b_fname);
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
	    if (isLegalExp(cp, user_reply(arg[0], arg[1])))
		tb_scopy(&result, cp);
	    else
		is_error = TRUE;
	}
	break;
    case UFQPASSWD:
	if (!is_error) {
	    qpasswd = TRUE;
	    if (isLegalExp(cp, user_reply(arg[0], NULL)))
		tb_scopy(&result, cp);
	    else
		is_error = TRUE;
	    qpasswd = FALSE;
	}
	break;
    case UFQUERY:
	if (!is_error) {
	    if (isLegalExp(cp, user_reply(arg[0], error_val)))
		tb_scopy(&result, cp);
	    else
		is_error = TRUE;
	}
	break;
    case UFLOOKUP:
	if ((i = combine_choices(&fsm_lookup_blist, arg[0])) > 0)
	    tb_scopy(&result, SL_TO_BSL(cfg_locate(arg[1], (UINT) i)));
	break;
    case UFPATH:
	if (!is_error) {
	    switch (choice_to_code(&fsm_path_blist, arg[0], strlen(arg[0]))) {
	    case PATH_END:
		cp = pathleaf(arg[1]);
		if ((cp = strchr(cp, '.')) != NULL)
		    tb_scopy(&result, SL_TO_BSL(cp));
		break;
	    case PATH_FULL:
		if (tb_scopy(&juggle, arg[1])
		    && tb_alloc(&juggle, (size_t) NFILEN)) {
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
		if (tb_values(result) != NULL
		    && (cp = strchr(tb_values(result), '.')) != NULL) {
		    *cp = EOS;
		    tb_setlen(&result, -1);
		}
		break;
	    case PATH_SHORT:
		if (tb_scopy(&juggle, arg[1])
		    && tb_alloc(&juggle, (size_t) NFILEN)) {
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
	tb_alloc(&juggle, (size_t) NFILEN);
	pathcat(tb_values(juggle), arg[0], arg[1]);
	tb_setlen(&juggle, -1);
	tb_scopy(&result, SL_TO_BSL(tb_values(juggle)));
	break;
    case UFPATHQUOTE:
	path_quote(&result, SL_TO_BSL(arg[0]));
	break;
    case UFSTIME:
	value = (long) time((time_t *) 0);
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
		&& i >= 0
		&& i < NKREGS) {
		KILL *kp = kbs[i].kbufh;
		while (kp != NULL) {
		    tb_bappend(&result,
			       (char *) (kp->d_chunk),
			       (size_t) KbSize(i, kp));
		    kp = kp->d_next;
		}
		tb_append(&result, EOS);
	    }
	}
	break;
    case UFREGEX_ESCAPE:
	if (!is_error) {
	    for (sp = arg[0]; *sp != 0; ++sp) {
		int ch = CharOf(*sp);
		if (strchr("\\.[]*^$~", ch) != NULL) {
		    tb_append(&result, BACKSLASH);
		}
		tb_append(&result, ch);
	    }
	    tb_append(&result, EOS);
	}
	break;
    case UFBCHANGED:
	if (!is_error
	    && (bp = find_any_buffer(arg[0])) != NULL) {
	    value = b_is_changed(bp);
	} else {
	    is_error = TRUE;
	}
	break;
    case UFFCHANGED:
	if (!is_error
	    && (bp = find_any_buffer(arg[0])) != NULL) {
	    value = eval_fchanged(bp);
	} else {
	    is_error = TRUE;
	}
	break;
    case NFUNCS:
	TRACE(("unknown function #%d\n", fnum));
	is_error = TRUE;
	break;
    }

    if (is_error) {
	tb_error(&result);
    } else {
	if (ret_numeric)
	    render_long(&result, value);
	else if (ret_boolean)
	    render_boolean(&result, (int) value);
	else
	    tb_enquote(&result);
    }

    TRACE2(("actual:%lu:%s\n", tb_length(result), tb_visible(result)));
    TPRINTF(("-> %s%s\n",
	     is_error ? "*" : "",
	     NONNULL(tb_values(result))));

    tb_free(&juggle);
    for (i = 0; i < nargs; i++) {
	tb_free(&args[i]);
    }
    read_msgline(old_reading);
    returnString(tb_values(result));
}

/* find a temp variable */
static UVAR *
lookup_tempvar(const char *name)
{
    UVAR *p;

    if (*name) {
	for (p = temp_vars; p != NULL; p = p->next)
	    if (!vl_stricmp(name, p->u_name))
		return p;
    }
    return NULL;
}

/* find a state variable */
int
vl_lookup_statevar(const char *name)
{
    return blist_match(&blist_statevars, name);
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
	s = (*statevar_funcs[vnum].func) (&buffer[nested], (const char *) NULL);

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
    vd->v_num = vl_lookup_statevar(var);
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

static char *
VarName(char *var)
{
    char *result;

    if (var && var[0] && var[1]) {
	switch (*var) {
	case '$':
	case '%':
	case '&':
	    result = var + 1;
	    break;
	default:
	    result = var;
	    break;
	}
    } else {
	result = var;
    }
    return result;
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
    vd->v_ptr = NULL;

    if (!var[1]) {
	vd->v_type = VW_NOVAR;
	return;
    }

    switch (var[0]) {
    case '$':			/* check for legal state var */
	FindModeVar(VarName(var), vd);
	break;

    case '%':			/* check for legal temp variable */
	vd->v_ptr = lookup_tempvar(VarName(var));
	if (vd->v_ptr) {	/* existing */
	    vd->v_type = VW_TEMPVAR;
	} else {		/* new */
	    beginDisplay();
	    if ((p = typealloc(UVAR)) != NULL) {
		if ((p->u_name = strmalloc(VarName(var))) != NULL) {
		    p->next = temp_vars;
		    p->u_value = NULL;
		    temp_vars = vd->v_ptr = p;
		    vd->v_type = VW_TEMPVAR;
		    free_vars_cmpl();
		} else {
		    free(p);
		}
	    }
	    endofDisplay();
	}
	break;

    case '&':			/* indirect operator? */
	if (vl_lookup_func(var) == UFINDIRECT) {
	    TBUFF *tok = NULL;
	    /* grab token, and eval it */
	    execstr = get_token(execstr, &tok, eol_null, EOS, (int *) 0);
	    (void) vl_strncpy(var, tokval(tb_values(tok)), (size_t) NLINE);
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
    if (var == NULL)
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
	if (find_mode(curbp, VarName(var), -TRUE, &args)) {
	    set_end_string('=');
	    status = adjvalueset(VarName(var), FALSE, TRUE, -TRUE, &args);
	} else {
	    mlforce("[Can't find mode '%s']", VarName(var));
	    status = FALSE;
	}
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
	    status = kbd_string2(prompt, &tmp, '\n', KBD_NORMAL,
				 (*name == '$' && name_to_choices(name + 1))
				 ? fsm_complete
				 : no_completion);
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
    if (isLegalVal(p->u_value)) {
	beginDisplay();
	free(p->u_value);
	endofDisplay();
    }
    p->u_value = NULL;
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

    for (p = temp_vars, q = NULL; p != NULL; q = p, p = p->next) {
	if (!vl_stricmp(p->u_name, name)) {
	    TRACE(("rmv_tempvar(%s) ok\n", name));
	    if (q != NULL)
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
    return (*statevar_funcs[vnum].func) ((TBUFF **) 0, value);
}

/* figure out what type of variable we're setting, and do so */
static int
SetVarValue(VWRAP * var, char *name, const char *value)
{
    int status;
    int savecmd;
    char *savestr;
    VALARGS args;

    if (isEmpty(name) || value == NULL)
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
	if (find_mode(curbp, VarName(name), -TRUE, &args)) {
	    set_end_string('=');
	    status = adjvalueset(VarName(name), FALSE, TRUE, -TRUE, &args);
	} else {
	    status = FALSE;
	}
	execstr = savestr;
	clexec = savecmd;
	break;

    case VW_TEMPVAR:
	beginDisplay();
	free_UVAR_value(var->v_ptr);
	if (isErrorVal(value)) {
	    var->v_ptr->u_value = error_val;
	} else if ((var->v_ptr->u_value = strmalloc(value)) == NULL) {
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
int
set_ctrans(const char *thePalette)
{
    int changed = 0;
    int n = 0, value;
    char *next;

    TRACE(("set_ctrans(%s)\n", thePalette));

    if (thePalette != NULL) {
	while (*thePalette != EOS) {
	    thePalette = skip_cblanks(thePalette);
	    value = (int) strtol(thePalette, &next, 0);
	    if (next == thePalette)
		break;
	    thePalette = next;
	    if (ctrans[n] != value) {
		ctrans[n] = value;
		++changed;
	    }
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
    return changed;
}
#else
int
set_ctrans(const char *thePalette)
{
    return 0;
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
    MK.o = b_left_margin(curbp);
    DOT.l = lforw(buf_head(curbp));
    DOT.o = b_left_margin(curbp);

    if (getregion(curbp, &region) == TRUE)
	attribute_cntl_a_seqs_in_region(&region, rgn_FULLLINE);
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

    p = tb_values(tb_alloc(rp, (size_t) 32));
    q = lsprintf(p, "%d", i);
    if (*rp != NULL && q != NULL && p != NULL)
	(*rp)->tb_used = (size_t) (q - p + 1);
    return p;
}

/* represent long integer as string */
char *
render_long(TBUFF **rp, long i)
{
    char *p, *q;

    p = tb_values(tb_alloc(rp, (size_t) 32));
    q = lsprintf(p, "%ld", i);
    if (*rp != NULL && q != NULL && p != NULL)
	(*rp)->tb_used = (size_t) (q - p + 1);
    return p;
}

#if OPT_EVAL

/* represent boolean as string */
char *
render_boolean(TBUFF **rp, int val)
{
    static const char *bools[] =
    {
	"FALSE", "TRUE"
    };
    return tb_values(tb_scopy(rp, bools[val ? 1 : 0]));
}

/* represent long integer as string */
char *
render_ulong(TBUFF **rp, ULONG i)
{
    char *p, *q;

    p = tb_values(tb_alloc(rp, (size_t) 32));
    q = lsprintf(p, "%lu", i);
    if (*rp != NULL && q != NULL && p != NULL)
	(*rp)->tb_used = (size_t) (q - p + 1);
    return p;
}

#if (SYS_WINNT||SYS_VMS)
/* unsigned to hex */
char *
render_hex(TBUFF **rp, unsigned i)
{
    char *p, *q;

    p = tb_values(tb_alloc(rp, 32));
    q = lsprintf(p, "%x", i);
    if (*rp != 0 && q != 0 && p != 0)
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
    return (strtol(val, NULL, 0) != 0);
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
    IGNORE_RC(strtol(buf, &tmp, 0));
    if ((tmp != NULL) && (tmp != buf) && (*tmp == 0) && isSpace(c)) {
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
    } else if (choices != NULL) {
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

    if (vars_cmpl_list == NULL) {
	for (pass = 0; pass < 2; pass++) {
	    count = 0;
	    for (p = temp_vars; p != NULL; p = p->next) {
		if (pass != 0)
		    vars_cmpl_list[count] = p->u_name;
		count++;
	    }
	    if (pass == 0) {
		beginDisplay();
		vars_cmpl_list = typeallocn(char *, (size_t) count + 1);
		endofDisplay();
		if (vars_cmpl_list == NULL)
		    break;
	    }
	}

	if (vars_cmpl_list != NULL) {
	    vars_cmpl_list[count] = NULL;
	    qsort(vars_cmpl_list, (size_t) count, sizeof(char *), qs_vars_cmp);
	}

    }
    return vars_cmpl_list;
}

static int
complete_vars(DONE_ARGS)
{
    int status = FALSE;
    char **nptr;

    if ((nptr = init_vars_cmpl()) != NULL) {
	status = kbd_complete(PASS_DONE_ARGS,
			      (const char *) nptr,
			      sizeof(char *));
    }
    return status;
}

static int
read_argument(TBUFF **paramp, const PARAM_INFO * info)
{
    static char empty_string[] = "";

    int status = TRUE;
    const char *prompt;
    TBUFF *temp;
    int (*complete) (DONE_ARGS) = no_completion;
    int save_clexec;
    int save_isnamed;
    char *save_execstr;
    char fname[NFILEN];
    char bname[NBUFN];
    KBD_OPTIONS flags = 0;	/* no expansion, etc. */

    if (mac_tokval(paramp) == NULL) {
	prompt = "String";
	if (info != NULL) {
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
		complete_enum_ptr = info->pi_choice->choices;
		break;
#endif
	    case PT_FILE:
		prompt = "Filename";
		break;
	    case PT_INT:
		prompt = "Integer";
		complete = complete_integer;
		break;
	    case PT_REG:
		prompt = "Register";
		flags = KBD_MAYBEC;
		complete = regs_completion;
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
	    if (info->pi_text != NULL)
		prompt = info->pi_text;
	}

	temp = NULL;
	tb_scopy(&temp, prompt);
	tb_sappend0(&temp, ": ");

	save_clexec = clexec;
	save_execstr = execstr;
	save_isnamed = isnamedcmd;

	/*
	 * If we've run out of parameters on the command-line, force this
	 * to be interactive.
	 */
	if (execstr == NULL || !*skip_blanks(execstr)) {
	    clexec = FALSE;
	    isnamedcmd = TRUE;
	    execstr = empty_string;
	}

	if (info != NULL) {
	    switch (info->pi_type) {
	    case PT_FILE:
		status = mlreply_file(tb_values(temp), (TBUFF **) 0,
				      FILEC_UNKNOWN, fname);
		if (status != ABORT)
		    tb_scopy(paramp, fname);
		break;
	    case PT_DIR:
		status = mlreply_dir(tb_values(temp), (TBUFF **) 0, fname);
		if (status != ABORT)
		    tb_scopy(paramp, fname);
		break;
	    case PT_BUFFER:
		status = ask_for_bname(tb_values(temp), bname, sizeof(bname));
		if (status != ABORT)
		    tb_scopy(paramp, bname);
		break;
	    default:
		status = kbd_reply(tb_values(temp),
				   paramp,	/* in/out buffer */
				   eol_history,
				   '\n',	/* expect a newline or return */
				   flags,	/* no expansion, etc. */
				   complete);
		if (status == TRUE)
		    tb_prequote(paramp);
		break;
	    }
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

    if (cmd != NULL && cmd->c_args != NULL) {
	for (max_args = 0;
	     cmd->c_args[max_args].pi_type != PT_UNKNOWN;
	     max_args++) ;
    } else {
	max_args = 0;
    }

    TRACE((T_CALLED "save_arguments(%s) max_args=%d\n", bp->b_bname, max_args));

#define tb_allocn(n) typecallocn(TBUFF *, n)	/* FIXME: indent */

    beginDisplay();
    if ((p = typealloc(PROC_ARGS)) == NULL) {
	status = no_memory("save_arguments");
    } else if ((all_args = tb_allocn((size_t) max_args + 1)) == NULL) {
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
	this_macro_result = NULL;

	tb_scopy(&(p->all_args[num_args]), bp->b_bname);
	if (p->all_args[num_args] == NULL) {
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
		    if (isLegalVal(cp)) {
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

		if (copied && p->all_args[num_args] == NULL) {
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

    if (p != NULL) {
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
static const char *
get_argument(const char *name)
{
    static TBUFF *value;
    static char empty[] = "";
    int num;
    char *str;
    char *result = error_val;

    if (arg_stack != NULL) {
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
simple_arg_eval(const char *argp)
{
    return argp;
}

/* the returned value is the user's response to the specified prompt */
static const char *
query_arg_eval(const char *argp)
{
    argp = user_reply(tokval(argp + 1), error_val);
    return argp ? argp : error_val;
}

/* the returned value is the next line from the named buffer */
static const char *
buffer_arg_eval(const char *argp)
{
    argp = next_buffer_line(tokval(argp + 1));
    return argp ? argp : error_val;
}

/* expand it as a temp variable */
static const char *
tempvar_arg_eval(const char *argp)
{
    static TBUFF *tmp;
    UVAR *p;
    const char *result;
    if ((p = lookup_tempvar(argp + 1)) != NULL) {
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
statevar_arg_eval(const char *argp)
{
    int vnum;
    const char *result;

    if (isErrorVal(result = get_argument(++argp))) {
	vnum = vl_lookup_statevar(argp);
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

/* run a function to evaluate it */
static const char *
function_arg_eval(const char *argp)
{
    int fnum = vl_lookup_func(argp);
    return (fnum != ILLEGAL_NUM) ? run_func(fnum) : error_val;
}

/* goto targets evaluate to their line number in the buffer */
static const char *
label_arg_eval(const char *argp)
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
qstring_arg_eval(const char *argp)
{
    return argp + 1;
}

static const char *
directive_arg_eval(const char *argp)
{
    static TBUFF *tkbuf;
    const char *result;
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
    if (argp == NULL)
	argp = "";
    tb_alloc(&tkbuf, NFILEN + strlen(argp));
    if (tb_values(tkbuf) != NULL)
	result = lengthen_path(strcpy(tb_values(tkbuf), argp));
    else
	result = "";
    return result;
}

typedef const char *(ArgEvalFunc) (const char *argp);

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
tokval(const char *tokn)
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

const char *
status2s(int val)
{
    const char *result = error_val;
    switch (val) {
    case ABORT:
	result = "ABORT";
	break;
    case FALSE:
	result = "FALSE";
	break;
    case SORTOFTRUE:
	result = "SORTOFTRUE";
	break;
    case TRUE:
	result = "TRUE";
	break;
    }
    return result;
}

int
s2status(const char *val)
{
    int result = VL_ERROR;

    if (val != error_val) {
	if (is_truem(val)) {
	    result = TRUE;
	} else if (is_falsem(val)) {
	    result = FALSE;
	} else {
	    char temp[12];
	    char *next = NULL;
	    long test = strtol(val, &next, 0);
	    size_t sz = strlen(val);
	    if ((const char *) next != val && isEmpty(next)) {
		result = (int) test;
	    } else if (sz < sizeof(temp)) {
		(void) mklower(strncpy0(temp, val, sizeof(temp)));
		if (!strncmp(temp, "abort", sz)) {
		    result = ABORT;
		} else if (!strncmp(temp, "error", sz)) {
		    result = VL_ERROR;
		} else if (!strncmp(temp, "sortoftrue", sz)) {
		    result = SORTOFTRUE;
		}
	    }
	}
    }
    return result;
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
    return choice_to_code(&fsm_bool_blist, temp, strlen(temp)) == TRUE;
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
    return choice_to_code(&fsm_bool_blist, temp, strlen(temp)) == FALSE;
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
    TBUFF *tok = NULL;

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
    static char Expression[] = "Expression";
    static PARAM_INFO info =
    {
	PT_STR, Expression, NULL
    };
    TBUFF *params = NULL, *tok = NULL, *cmd = NULL;
    char *old, *tmp;
    int code = FALSE;

    if (read_argument(&params, &info) == TRUE) {
	if (isLegalExp(tmp, tb_values(params))) {
	    old = execstr;
	    execstr = tmp;
	    TRACE(("EVAL %s\n", execstr));
	    for (;;) {
		if (can_evaluate()) {
		    if ((tmp = mac_unquotedarg(&tok)) != NULL) {
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
	    if ((tmp = tb_values(cmd)) != NULL) {
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
    long n = strtol(s, NULL, 0);
    if (n < 0)
	n = 0;
    return (size_t) n;
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
    LINE *result = NULL;

    if (bp != NULL && label != NULL && *label != EOS) {
	LINE *glp;
	size_t len = strlen(label);
	int need = (int) len + 1;
	int col;

	if (len > 1) {
	    for_each_line(glp, bp) {
		if (llength(glp) >= need) {
		    for (col = 0; col < llength(glp); ++col)
			if (!isSpace(lvalue(glp)[col]))
			    break;
		    if (llength(glp) >= need + col
			&& lvalue(glp)[col] == '*'
			&& !memcmp(&lvalue(glp)[col + 1], label, len)) {
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
    if (isLegalVal(s)) {
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
    if (isLegalVal(s)) {
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

    if (isLegalVal(str)) {
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

long
absol(long x)
{				/* return absolute value of given number */
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

    if (last != 0 && values != NULL) {
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
