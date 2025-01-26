/*
 * statevar.c -- the functions named var_XXXX() referred to by modetbl
 *	for getting and setting the values of the vile state variables,
 *	plus helper utility functions.
 *
 * $Id: statevar.c,v 1.173 2025/01/26 17:08:26 tom Exp $
 */

#include	<estruct.h>
#include	<edef.h>
#include	<nevars.h>
#include	<nefsms.h>
#include	<patchlev.h>

#if SYS_WINNT
#include	<process.h>
#endif

#define		WRITE_ONLY	"[write only]"

#if OPT_FINDPATH
static char *findpath;		/* $VILE_FINDPATH environment is "$findpath" state
				 * variable.
				 */
#endif

#if OPT_EVAL && OPT_SHELL
static char *shell;		/* $SHELL environment is "$shell" state variable */
static char *directory;		/* $TMP environment is "$directory" state variable */
#if DISP_X11
static char *x_display;		/* $DISPLAY environment is "$xdisplay" state variable */
static char *x_shell;		/* $XSHELL environment is "$xshell" state variable */
static char *x_shellflags;	/* flags separating "$xshell" from command */
#endif
#endif
#if OPT_PATHLOOKUP
static char *cdpath;		/* $CDPATH environment is "$cdpath" state variable. */
#endif

static const char *
DftEnv(const char *name, const char *dft)
{
    const char *result;

#if OPT_EVAL && OPT_SHELL
    name = safe_getenv(name);
    result = isEmpty(name) ? dft : name;
#else
    (void) name;
    result = dft;
#endif
    return result;
}

static void
SetEnv(char **namep, const char *value)
{
    char *newvalue;

    beginDisplay();
    if (*namep == NULL || strcmp(*namep, value)) {
	if ((newvalue = strmalloc(value)) != NULL) {
#if OPT_EVAL && OPT_SHELL
	    FreeIfNeeded(*namep);
#endif
	    *namep = newvalue;
	}
    }
    endofDisplay();
}

static int
any_ro_BOOL(TBUFF **rp, const char *vp, int value)
{
    if (rp) {
	render_boolean(rp, value);
	return TRUE;
    } else if (vp) {
	return ABORT;		/* read-only */
    } else {
	return FALSE;
    }
}

static int
any_rw_BOOL(TBUFF **rp, const char *vp, int *value)
{
    if (rp) {
	render_boolean(rp, *value);
	return TRUE;
    } else if (vp) {
	*value = scan_bool(vp);
	return TRUE;
    } else {
	return FALSE;
    }
}

static int
any_ro_INT(TBUFF **rp, const char *vp, int value)
{
    if (rp) {
	render_int(rp, value);
	return TRUE;
    } else if (vp) {
	return ABORT;		/* read-only */
    } else {
	return FALSE;
    }
}

static int
any_ro_ULONG(TBUFF **rp, const char *vp, ULONG value)
{
    if (rp) {
	render_ulong(rp, value);
	return TRUE;
    } else if (vp) {
	return ABORT;		/* read-only */
    } else {
	return FALSE;
    }
}

static int
any_rw_INT(TBUFF **rp, const char *vp, int *value)
{
    if (rp) {
	render_int(rp, *value);
	return TRUE;
    } else if (vp) {
	*value = scan_int(vp);
	return TRUE;
    } else {
	return FALSE;
    }
}

static int
any_ro_STR(TBUFF **rp, const char *vp, const char *value)
{
    if (rp) {
	if (value != NULL) {
	    tb_scopy(rp, value);
	    return TRUE;
	}
    } else if (vp) {
	return ABORT;		/* read-only */
    }
    return FALSE;
}

static int
any_rw_STR(TBUFF **rp, const char *vp, TBUFF **value)
{
    if (rp) {
	if (value != NULL && *value != NULL) {
	    tb_copy(rp, *value);
	    return TRUE;
	}
    } else if (vp) {
	tb_scopy(value, vp);
	return TRUE;
    }
    return FALSE;
}

static int
any_rw_EXPR(TBUFF **rp, const char *vp, TBUFF **value)
{
    if (rp) {
	if (value != NULL) {
	    tb_copy(rp, *value);
	    return TRUE;
	}
    } else if (vp) {
	regexp *exp = regcomp(vp, strlen(vp), TRUE);
	if (exp != NULL) {
	    beginDisplay();
	    free(exp);
	    endofDisplay();
	    tb_scopy(value, vp);
	    return TRUE;
	}
    }
    return FALSE;
}

static int
any_ro_TBUFF(TBUFF **rp, const char *vp, TBUFF **value)
{
    if (rp) {
	if (value != NULL) {
	    tb_copy(rp, *value);
	    return TRUE;
	}
    } else if (vp) {
	return ABORT;		/* read-only */
    }
    return FALSE;
}

static int
any_rw_TBUFF(TBUFF **rp, const char *vp, TBUFF **value)
{
    if (rp) {
	if (value != NULL) {
	    tb_copy(rp, *value);
	    return TRUE;
	}
    } else if (vp) {
	tb_scopy(value, vp);
	return TRUE;
    }
    return FALSE;
}

static int
any_rw_TXT(TBUFF **rp, const char *vp, char **value)
{
    if (rp) {
	tb_scopy(rp, *value);
	return TRUE;
    } else if (vp) {
	SetEnv(value, vp);
	return TRUE;
    } else {
	return FALSE;
    }
}

static int
any_CTYPE_MATCH(TBUFF **rp, const char *vp, CHARTYPE type)
{
    int whole_line = adjust_chartype(&type);

    if (rp) {
	(void) vl_ctype2tbuff(rp, type, whole_line);
	return TRUE;
    } else if (vp && valid_buffer(curbp)) {
	return lrepl_ctype(type, vp, (int) strlen(vp));
    } else {
	return FALSE;
    }
}

#if OPT_CURTOKENS
static int
any_REGEX_MATCH(TBUFF **rp, const char *vp, REGEXVAL * rexp)
{
    if (rp) {
	(void) vl_regex2tbuff(rp, rexp, vl_get_it_all);
	return TRUE;
    } else if (vp && valid_buffer(curbp)) {
	return lrepl_regex(rexp, vp, (int) strlen(vp));
    } else {
	return FALSE;
    }
}
#endif

#if OPT_HOOKS
static int
any_HOOK(TBUFF **rp, const char *vp, HOOK * hook)
{
    if (rp) {
	tb_scopy(rp, hook->proc);
	return TRUE;
    } else if (vp) {
	(void) strncpy0(hook->proc, vp, (size_t) NBUFN);
	return TRUE;
    } else {
	return FALSE;
    }
}
#endif

const char *
safe_getenv(const char *name)
{
    const char *result;
#if OPT_EVAL && OPT_SHELL
    char *v = getenv(name);
#if defined(_WIN32)
    if (v == 0)
	v = vile_getenv(name);
#endif
    result = NONNULL(v);
#else
    (void) name;
    result = "";
#endif
    return result;
}

#if OPT_EVAL && OPT_SHELL

#if SYS_OS2 || SYS_OS2_EMX
#  define SHELL_NAME "COMSPEC"
#  define SHELL_PATH "cmd.exe"
#else
#  if SYS_WINNT
#    define SHELL_NAME "COMSPEC"
#    define SHELL_PATH (is_winnt() ? "cmd.exe" : "command.com")
#  else
#    if SYS_MSDOS
#      define SHELL_NAME "COMSPEC"
#      define SHELL_PATH "command.com"
#    else
#      define SHELL_NAME "SHELL"
#      define SHELL_PATH "/bin/sh"
#    endif
#  endif
#endif

char *
get_shell(void)
{
    if (shell == NULL)
	SetEnv(&shell, DftEnv(SHELL_NAME, SHELL_PATH));
    return shell;
}
#endif

#if OPT_FINDPATH
char *
get_findpath(void)
{
    if (findpath == NULL)
	SetEnv(&findpath, DftEnv("VILE_FINDPATH", ""));
    return (findpath);
}
#endif

#if OPT_EVAL && DISP_X11 && OPT_SHELL
#ifndef DEFAULT_XSHELL
#define DEFAULT_XSHELL "xterm"
#endif
char *
get_xshell(void)
{
    if (x_shell == NULL)
	SetEnv(&x_shell, DftEnv("XSHELL", DEFAULT_XSHELL));
    return x_shell;
}

#ifndef DEFAULT_XSHELLFLAGS
#define DEFAULT_XSHELLFLAGS "-e"
#endif
char *
get_xshellflags(void)
{
    if (x_shellflags == NULL)
	SetEnv(&x_shellflags, DftEnv("XSHELLFLAGS", DEFAULT_XSHELLFLAGS));
    return x_shellflags;
}

char *
get_xdisplay(void)
{
    if (x_display == NULL)
	SetEnv(&x_display, DftEnv("DISPLAY", x_get_display_name()));
    return x_display;
}
#endif

#if OPT_EVAL && OPT_SHELL
char *
get_directory(void)
{
    if (directory == NULL)
	SetEnv(&directory, DftEnv("TMP", P_tmpdir));
    return directory;
}
#endif

static KILLREG *
default_kill(void)
{
    KILLREG *result = NULL;
    if (kbs[0].kbufh != NULL) {
	int n = index2ukb(0);
	result = kbs + n;
    }
    return result;
}

#if OPT_PATHLOOKUP
char *
get_cdpath(void)
{
    if (cdpath == NULL)
	SetEnv(&cdpath, DftEnv("CDPATH", ""));
    return cdpath;
}

int
var_CDPATH(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, get_cdpath());
	return TRUE;
    } else if (vp) {
	SetEnv(&cdpath, vp);
	return TRUE;
    } else {
	return FALSE;
    }
}
#endif

#if OPT_EVAL
/* Return comma-delimited list of "interesting" options. */
static char *
cfgopts(void)
{
    static const char *opts[] =
    {
#if !OPT_SHELL
	"noshell",
#endif
#if SYS_WINNT && defined(VILE_OLE)
	"oleauto",
#endif
#if OPT_HYPERTEXT
	"hypertext",
#endif
#if OPT_LOCALE
	"locale",
#endif
#if OPT_ICONV_FUNCS
	"iconv",
#endif
#if OPT_MULTIBYTE
	"multibyte",
#endif
#if OPT_PERL
	"perl",
#endif
#if DISP_ANSI
	"ansi",
#elif DISP_BORLAND
	"borland",
#elif DISP_CURSES
	"curses",
#elif DISP_NTCONS
	"ntcons",
#elif DISP_NTWIN
	"ntwin",
#elif DISP_TERMCAP
# if USE_TERMINFO
	"terminfo",
# else
	"termcap",
# endif
#elif DISP_VIO
	"os2vio",
#elif DISP_VMSVT
	"vmsvt",
#elif DISP_X11
# if ATHENA_WIDGETS
	"athena",
#  if defined(HAVE_LIB_XAW3DXFT)
	"xaw3dxft",
#  elif defined(HAVE_LIB_XAW3D)
	"xaw3d",
#  elif defined(HAVE_LIB_XAWPLUS)
	"xawplus",
#  elif defined(HAVE_LIB_NEXTAW)
	"nextaw",
#  elif defined(HAVE_LIB_XAW)
	"xaw",
#  else
	"other-xaw",
#  endif
# elif MOTIF_WIDGETS
	"motif",
# endif
# if defined(XRENDERFONT)
	"xft",
# endif
#endif
	NULL			/* End of list marker */
    };
    static TBUFF *optstring;

    if (optstring == NULL) {
	const char **lclopt;

	optstring = tb_init(&optstring, EOS);
	for (lclopt = opts; *lclopt; lclopt++) {
	    if (tb_length(optstring))
		optstring = tb_append(&optstring, ',');
	    optstring = tb_sappend(&optstring, *lclopt);
	}
	optstring = tb_append(&optstring, EOS);
    }
    return tb_values(optstring);
}
#endif

#if OPT_EVAL
int
var_ABUFNAME(TBUFF **rp, const char *vp)
{
    BUFFER *bp;
    return any_ro_STR(rp, vp, ((bp = find_alt()) != NULL) ? bp->b_bname : "");
}

#if OPT_HOOKS
int
var_AUTOCOLORHOOK(TBUFF **rp, const char *vp)
{
    return any_HOOK(rp, vp, &autocolorhook);
}
#endif

int
var_BCHARS(TBUFF **rp, const char *vp)
{
    bsizes(curbp);
    return (valid_buffer(curbp)
	    ? any_ro_ULONG(rp, vp, curbp->b_bytecount)
	    : FALSE);
}

int
var_BCHANGED(TBUFF **rp, const char *vp)
{
    return any_ro_BOOL(rp, vp, valid_buffer(curbp) && b_is_changed(curbp));
}

int
var_FCHANGED(TBUFF **rp, const char *vp)
{
    return any_ro_BOOL(rp, vp, eval_fchanged(curbp));
}

int
var_BFLAGS(TBUFF **rp, const char *vp)
{
    char temp[80];

    if (rp) {
	buffer_flags(temp, curbp);
	tb_scopy(rp, temp);
	return TRUE;
    } else if (vp) {
	return ABORT;		/* read-only */
    } else {
	return FALSE;
    }
}

int
var_BLINES(TBUFF **rp, const char *vp)
{
    bsizes(curbp);
    return (valid_buffer(curbp)
	    ? any_ro_INT(rp, vp, curbp->b_linecount)
	    : FALSE);
}

#if DISP_NTWIN
static int
parse_rgb(const char **src, int dft)
{
    char *dst = 0;
    int result;

    *src = skip_cblanks(*src);
    result = strtol(*src, &dst, 0);
    if (dst != 0)
	*src = dst;
    if (dst == 0 || (*dst != 0 && !isSpace(*dst)) || result <= 0 || result > 255)
	result = dft;
    return result;
}

int
var_BRIGHTNESS(TBUFF **rp, const char *vp)
{
    char temp[80];

    if (rp) {
	lsprintf(temp, "%d %d %d", rgb_gray, rgb_normal, rgb_bright);
	tb_scopy(rp, temp);
	return TRUE;
    } else if (vp) {
	rgb_gray = parse_rgb(&vp, 140);
	rgb_normal = parse_rgb(&vp, 180);
	rgb_bright = parse_rgb(&vp, 255);
	return vile_refresh(FALSE, 1);
    } else {
	return FALSE;
    }
}
#endif

#if OPT_HOOKS
int
var_BUFHOOK(TBUFF **rp, const char *vp)
{
    return any_HOOK(rp, vp, &bufhook);
}
#endif

#if OPT_CURTOKENS
int
var_BUF_FNAME_EXPR(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, (valid_buffer(curbp)
			       ? get_buf_fname_expr(curbp)->pat
			       : ""));
}

int
var_BUFNAME(TBUFF **rp, const char *vp)
{
    return any_REGEX_MATCH(rp, vp, (valid_buffer(curbp)
				    ? b_val_rexp(curbp, VAL_BUFNAME_EXPR)
				    : (REGEXVAL *) 0));
}
#endif

int
var_CBUFNAME(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, valid_buffer(curbp) ? curbp->b_bname : "");
	return TRUE;
    } else if (vp) {
	if (valid_buffer(curbp)) {
	    set_bname(curbp, vp);
	    curwp->w_flag |= WFMODE;
	}
	return TRUE;
    } else {
	return FALSE;
    }
}

int
var_BWINDOWS(TBUFF **rp, const char *vp)
{
    return (valid_buffer(curbp)
	    ? any_ro_INT(rp, vp, (int) curbp->b_nwnd)
	    : FALSE);
}

#if OPT_HOOKS
int
var_CDHOOK(TBUFF **rp, const char *vp)
{
    return any_HOOK(rp, vp, &cdhook);
}
#endif

int
var_CFGOPTS(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, cfgopts());
}

int
var_CFNAME(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, ((valid_buffer(curbp) && curbp->b_fname)
		      ? curbp->b_fname
		      : ""));
	return TRUE;
    } else if (vp) {
	if (valid_buffer(curbp)) {
	    ch_fname(curbp, vp);
	    curwp->w_flag |= WFMODE;
	}
	return TRUE;
    } else {
	return FALSE;
    }
}

int
var_CHAR(TBUFF **rp, const char *vp)
{
    int status = FALSE;

    if (rp) {
	if (valid_buffer(curbp) && !is_empty_buf(curbp)) {
	    render_int(rp, CharAtDot());
	} else {
	    tb_scopy(rp, error_val);
	}
	status = TRUE;
    } else if (vp) {
	if ((status = check_editable(curbp)) == TRUE) {
	    int c;
	    mayneedundo();
	    (void) ldel_chars(1L, FALSE);	/* delete 1 char */
	    c = scan_int(vp);
	    if ((status = bputc(c)) == TRUE)
		(void) backchar(FALSE, 1);
	}
    }
    return status;
}

#if OPT_ENCRYPT
int
var_CRYPTKEY(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, WRITE_ONLY);
	return TRUE;
    } else if (vp) {
	beginDisplay();
	FreeIfNeeded(cryptkey);
	cryptkey = typeallocn(char, NKEYLEN);
	endofDisplay();
	if (cryptkey == NULL)
	    return no_memory("var_CRYPTKEY");
	vl_make_encrypt_key(cryptkey, vp);
	return TRUE;
    } else {
	return FALSE;
    }
}
#endif

#if !SMALLER
int
var_CURCHAR(TBUFF **rp, const char *vp)
{
    if (rp) {
	render_ulong(rp, vl_getcchar() + 1);
	return TRUE;
    } else if (vp && valid_buffer(curbp)) {
	return gotochr(TRUE, (int) strtol(vp, NULL, 0));
    } else {
	return FALSE;
    }
}

int
var_EMPTY_LINES(TBUFF **rp, const char *vp)
{
    if (rp) {
	render_int(rp, var_empty_lines);
	return TRUE;
    } else if (vp) {
	var_empty_lines = (int) strtol(vp, NULL, 0);
	if (var_empty_lines <= 0)
	    var_empty_lines = 1;
	return TRUE;
    } else {
	return FALSE;
    }
}
#endif

int
var_CURCOL(TBUFF **rp, const char *vp)
{
    if (rp) {
	render_int(rp, getccol(FALSE) + 1);
	return TRUE;
    } else if (vp && valid_buffer(curbp)) {
	return gotocol(TRUE, (int) strtol(vp, NULL, 0));
    } else {
	return FALSE;
    }
}

int
var_CURLINE(TBUFF **rp, const char *vp)
{
    if (rp) {
	render_int(rp, getcline());
	return TRUE;
    } else if (vp && valid_buffer(curbp)) {
	return gotoline(TRUE, (int) strtol(vp, NULL, 0));
    } else {
	return FALSE;
    }
}

#if OPT_SHELL
int
var_CWD(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, current_directory(FALSE));
	return TRUE;
    } else if (vp) {
	return set_directory(vp);
    } else {
	return FALSE;
    }
}
#endif

int
var_CWLINE(TBUFF **rp, const char *vp)
{
    if (rp) {
	render_int(rp, getlinerow());
	return TRUE;
    } else if (vp && valid_buffer(curbp)) {
	return forwline(TRUE, (int) strtol(vp, NULL, 0) - getlinerow());
    } else {
	return FALSE;
    }
}

int
var_DEBUG(TBUFF **rp, const char *vp)
{
    return any_rw_BOOL(rp, vp, &tracemacros);
}

#if OPT_SHELL
int
var_DIRECTORY(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, get_directory());
	return TRUE;
    } else if (vp) {
	SetEnv(&directory, vp);
	return TRUE;
    } else {
	return FALSE;
    }
}
#endif

int
var_CMD_COUNT(TBUFF **rp, const char *vp)
{
    return any_ro_INT(rp, vp, cmd_count);
}

#if OPT_PROCEDURES
int
var_CMD_MOTION(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, fnc2engl(cmd_motion));
}
#endif

int
var_DISCMD(TBUFF **rp, const char *vp)
{
    return any_rw_BOOL(rp, vp, &vl_msgs);
}

int
var_DISINP(TBUFF **rp, const char *vp)
{
    return any_rw_BOOL(rp, vp, &vl_echo);
}

#if OPT_TRACE
int
var_GOAL_COLUMN(TBUFF **rp, const char *vp)
{
    return any_rw_INT(rp, vp, &curgoal);
}
#endif

#if OPT_LOCALE
int
var_ENCODING(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, vl_real_enc.encoding);
}
#endif

int
var_EOC(TBUFF **rp, const char *vp)
{
    return any_ro_BOOL(rp, vp, ev_end_of_cmd ? 1 : 0);
}

#if OPT_FINDERR
int
var_FILENAME_EXPR(TBUFF **rp, const char *vp)
{
    int code = any_rw_EXPR(rp, vp, &filename_expr);
    if (rp != NULL && code == TRUE) {
	free_err_exps(curbp);
	update_err_regex();
    }
    return code;
}

int
var_ERROR_EXPR(TBUFF **rp, const char *vp)
{
    return any_rw_EXPR(rp, vp, &error_expr);
}

int
var_ERROR_MATCH(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, tb_values(error_match));
	return TRUE;
    } else if (vp) {
	tb_scopy(&error_match, vp);
	return TRUE;
    }
    return FALSE;
}

int
var_ERROR_BUFFER(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, get_febuff());
	return TRUE;
    } else if (vp) {
	set_febuff(vp);
	return TRUE;
    } else {
	return FALSE;
    }
}

int
var_ERROR_TABSTOP(TBUFF **rp, const char *vp)
{
    return any_rw_INT(rp, vp, &error_tabstop);
}
#endif

int
var_EXEC_PATH(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, exec_pathname);
}

#if OPT_MSDOS_PATH || SYS_VMS
#define the_EXEC_SUFFIX ".exe"
#else
#define the_EXEC_SUFFIX ""
#endif

int
var_EXEC_SUFFIX(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, the_EXEC_SUFFIX);
}

#if OPT_HOOKS
int
var_EXITHOOK(TBUFF **rp, const char *vp)
{
    return any_HOOK(rp, vp, &exithook);
}
#endif

#if SYS_WINNT
int
var_FAVORITES(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, get_favorites());
}
#endif

#if OPT_FINDPATH
int
var_FINDPATH(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, get_findpath());
	return TRUE;
    } else if (vp) {
	SetEnv(&findpath, vp);
	return TRUE;
    } else {
	return FALSE;
    }
}

/* debug aid -- this may go away */
int
var_FINDCMD(TBUFF **rp, const char *vp)
{
    char *cmd = last_findcmd();

    return (any_ro_STR(rp, vp, NONNULL(cmd)));
}
#endif

int
var_FONT(TBUFF **rp, const char *vp)
{
    int result = FALSE;

#if SYS_WINNT && DISP_NTWIN
    if (rp) {
	tb_scopy(rp, ntwinio_current_font());
	result = TRUE;
    } else if (vp) {
	result = ntwinio_font_frm_str(vp, FALSE);
    }
#elif DISP_X11
    if (rp) {
	tb_scopy(rp, x_current_fontname());
	result = TRUE;
    } else if (vp) {
	result = x_setfont(vp);
    }
#else
    result = any_ro_STR(rp, vp, "fixed");
#endif

    return result;
}

int
var_FWD_SEARCH(TBUFF **rp, const char *vp)
{
    return any_rw_BOOL(rp, vp, &last_srch_direc);
}

int
var_HELPFILE(TBUFF **rp, const char *vp)
{
    return any_rw_TXT(rp, vp, &helpfile);
}

#if DISP_X11
int
var_ICONNAME(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, x_get_icon_name());
	return TRUE;
    } else if (vp) {
	x_set_icon_name(vp);
	return TRUE;
    } else {
	return FALSE;
    }
}
#endif

#if OPT_CURTOKENS
int
var_IDENTIFIER(TBUFF **rp, const char *vp)
{
    return any_REGEX_MATCH(rp, vp, (valid_buffer(curbp)
				    ? b_val_rexp(curbp, VAL_IDENTIFIER_EXPR)
				    : (REGEXVAL *) 0));
}
#endif

int
var_KBDMACRO(TBUFF **rp, const char *vp)
{
    if (rp) {
	get_kbd_macro(rp);
	return TRUE;
    } else if (vp) {
	return ABORT;		/* read-only */
    } else {
	return FALSE;
    }
}

int
var_KILL(TBUFF **rp, const char *vp)
{
    if (rp) {
	KILLREG *kr = default_kill();
	KILL *kb;
	int limit = kill_limit;
	int used;

	tb_init(rp, EOS);
	if ((kr != NULL) && (kb = kr->kbufh) != NULL) {
	    while (kb->d_next != NULL) {
		if ((used = KBLOCK) > limit)
		    used = limit;
		tb_bappend(rp, (char *) (kb->d_chunk), (size_t) used);
		if ((limit -= used) <= 0)
		    break;
		kb = kb->d_next;
	    }
	    if (limit > 0) {
		if ((used = (int) kr->kused) > limit)
		    used = limit;
		tb_bappend(rp, (char *) (kb->d_chunk), (size_t) used);
	    }
	}
	tb_append(rp, EOS);
	return TRUE;
    } else if (vp) {
	return ABORT;		/* read-only */
    } else {
	return FALSE;
    }
}

int
var_KILL_SIZE(TBUFF **rp, const char *vp)
{
    if (rp) {
	KILLREG *kr = default_kill();
	KILL *kb;
	int result = 0;
	if ((kr != NULL) && (kb = kr->kbufh) != NULL) {
	    while (kb->d_next != NULL) {
		result += KBLOCK;
		kb = kb->d_next;
	    }
	    result += (int) kr->kused;
	    return any_ro_INT(rp, vp, result);
	} else {
	    return FALSE;
	}
    } else if (vp) {
	return ABORT;		/* read-only */
    } else {
	return FALSE;
    }
}

int
var_KILL_LIMIT(TBUFF **rp, const char *vp)
{
    return any_rw_INT(rp, vp, &kill_limit);
}

int
var_LASTKEY(TBUFF **rp, const char *vp)
{
    return any_rw_INT(rp, vp, &lastkey);
}

int
var_LCOLS(TBUFF **rp, const char *vp)
{
    if (rp) {
	int col;
#ifdef WMDLINEWRAP
	/* temporarily set linewrap to avoid having the sideways value
	 * subtracted from the returned column.
	 */
	int save_break = w_val(curwp, WMDLINEBREAK);
	int save_wrap = w_val(curwp, WMDLINEWRAP);
	w_val(curwp, WMDLINEBREAK) = 1;
	w_val(curwp, WMDLINEWRAP) = 1;
#endif
	col = offs2col(curwp, DOT.l, llength(DOT.l));
#ifdef WMDLINEWRAP
	w_val(curwp, WMDLINEBREAK) = save_break;
	w_val(curwp, WMDLINEWRAP) = save_wrap;
#endif
	render_int(rp, col);
	return TRUE;
    } else if (vp) {
	return ABORT;		/* read-only */
    } else {
	return FALSE;
    }
}

int
var_LIBDIR_PATH(TBUFF **rp, const char *vp)
{
    return any_rw_TXT(rp, vp, &libdir_path);
}

int
var_LINE(TBUFF **rp, const char *vp)
{
    return any_CTYPE_MATCH(rp, vp, (CHARTYPE) 0);
}

int
var_LLENGTH(TBUFF **rp, const char *vp)
{
    return valid_buffer(curbp) ? any_ro_INT(rp, vp, llength(DOT.l)) : FALSE;
}

#if OPT_LOCALE
int
var_LOCALE(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, vl_real_enc.locale);
}
#endif

#if OPT_MAJORMODE
int
var_MAJORMODE(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, ((valid_buffer(curbp) && curbp->majr != NULL)
			       ? curbp->majr->shortname
			       : ""));
}

int
var_MAJORMODEHOOK(TBUFF **rp, const char *vp)
{
    return any_HOOK(rp, vp, &majormodehook);
}
#endif

#if OPT_MODELINE
int
var_MODELINE_LIMIT(TBUFF **rp, const char *vp)
{
    return any_rw_INT(rp, vp, &modeline_limit);
}
#endif

int
var_MATCH(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp,
		      (tb_length(tb_matched_pat)
		       ? tb_values(tb_matched_pat)
		       : ""));
}

#if OPT_MENUS
int
var_MENU_FILE(TBUFF **rp, const char *vp)
{
    return any_rw_TXT(rp, vp, &menu_file);
}
#endif

int
var_MODE(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, current_modename());
}

#if OPT_MLFORMAT
int
var_MLFORMAT(TBUFF **rp, const char *vp)
{
    if (rp) {
	if (modeline_format == NULL)
	    mlforce("BUG: modeline_format uninitialized");
	else
	    tb_scopy(rp, modeline_format);
	return TRUE;
    } else if (vp) {
	SetEnv(&modeline_format, vp);
	upmode();
	return TRUE;
    } else {
	return FALSE;
    }
}
#endif

int
var_MODIFIED(TBUFF **rp, const char *vp)
{
    return any_ro_BOOL(rp, vp, valid_buffer(curbp) && b_is_changed(curbp));
}

#if OPT_COLOR
int
var_NCOLORS(TBUFF **rp, const char *vp)
{
    if (rp) {
	render_int(rp, ncolors);
	return TRUE;
    } else if (vp) {
	return set_colors((int) strtol(vp, NULL, 0));
    }
    return FALSE;
}
#else
int
var_NCOLORS(TBUFF **rp GCC_UNUSED, const char *vp GCC_UNUSED)
{
    return FALSE;
}
#endif

int
var_NTILDES(TBUFF **rp, const char *vp)
{
    if (rp) {
	render_int(rp, ntildes);
	return TRUE;
    } else if (vp) {
	ntildes = (int) strtol(vp, NULL, 0);
	if (ntildes > 100)
	    ntildes = 100;
	return TRUE;
    } else {
	return FALSE;
    }
}

#if OPT_SHELL
int
var_OCWD(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, previous_directory());
}
#endif

int
var_OS(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, opersys);
}

int
var_PAGELEN(TBUFF **rp, const char *vp)
{
    int result = FALSE;

    if (rp) {
	render_int(rp, term.rows);
	result = TRUE;
    } else if (vp) {
#if DISP_X11 || DISP_NTWIN
	gui_resize(term.cols, (int) strtol(vp, NULL, 0));
	result = TRUE;
#else
	result = newlength(TRUE, (int) strtol(vp, NULL, 0));
#endif
    }
    return result;
}

/* separator for lists of pathnames */
int
var_PATHCHR(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_append(rp, vl_pathchr);
	tb_append(rp, EOS);
	return TRUE;
    } else if (vp) {
	if (strlen(vp) == 1) {
	    vl_pathchr = *vp;
	    return TRUE;
	}
    }
    return FALSE;
}

/* separator for levels of pathnames */
int
var_PATHSEP(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_append(rp, vl_pathsep);
	tb_append(rp, EOS);
	return TRUE;
    } else if (vp) {
	if (strlen(vp) == 1) {
	    vl_pathsep = *vp;
	    return TRUE;
	}
    }
    return FALSE;
}

#if OPT_POSFORMAT
int
var_POSFORMAT(TBUFF **rp, const char *vp)
{
    if (rp) {
	if (position_format == NULL)
	    mlforce("BUG: position_format uninitialized");
	else
	    tb_scopy(rp, position_format);
	return TRUE;
    } else if (vp) {
	SetEnv(&position_format, vp);
	return TRUE;
    } else {
	return FALSE;
    }
}
#endif

int
var_CURWIDTH(TBUFF **rp, const char *vp)
{
    int result = FALSE;

    if (rp) {
	render_int(rp, term.cols);
	result = TRUE;
    } else if (vp) {
#if DISP_X11 || DISP_NTWIN
	gui_resize((int) strtol(vp, NULL, 0), term.rows);
	result = TRUE;
#else
	result = newwidth(TRUE, (int) strtol(vp, NULL, 0));
#endif
    }
    return result;
}

int
var_PALETTE(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, (tb_length(tb_curpalette)
		      ? tb_values(tb_curpalette)
		      : ""));
	return TRUE;
    } else if (vp) {
	return set_palette(vp);
    } else {
	return FALSE;
    }
}

int
var_PATCHLEVEL(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, VILE_PATCHLEVEL);
}

#if OPT_CURTOKENS
int
var_PATHNAME(TBUFF **rp, const char *vp)
{
    return any_REGEX_MATCH(rp, vp, (valid_buffer(curbp)
				    ? b_val_rexp(curbp, VAL_PATHNAME_EXPR)
				    : (REGEXVAL *) 0));
}
#endif

int
var_PENDING(TBUFF **rp, const char *vp)
{
    return any_ro_BOOL(rp, vp, keystroke_avail());
}

int
var_PROCESSID(TBUFF **rp, const char *vp)
{
    if (rp) {
#if SYS_UNIX
	render_int(rp, getpid());
#else
# if (SYS_WINNT || SYS_VMS)
	/* translate pid to hex, because:
	 *    a) most Win32 PID's are huge,
	 *    b) VMS PID's are traditionally displayed in hex.
	 */
	render_hex(rp, getpid());
# endif
#endif
	return TRUE;
    } else if (vp) {
	return ABORT;		/* read-only */
    } else {
	return FALSE;
    }
}

int
var_PROGNAME(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, prognam);
}

int
var_PROMPT(TBUFF **rp, const char *vp)
{
    return any_rw_STR(rp, vp, &prompt_string);
}

int
var_QIDENTIFIER(TBUFF **rp, const char *vp)
{
    return any_CTYPE_MATCH(rp, vp, vl_qident);
}

#if OPT_HOOKS
int
var_RDHOOK(TBUFF **rp, const char *vp)
{
    return any_HOOK(rp, vp, &readhook);
}
#endif

/*
 * Note that replacepat is stored without a trailing null.
 */
int
var_REPLACE(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_copy(rp, replacepat);
	return TRUE;
    } else if (vp) {
	(void) tb_init(&replacepat, EOS);
	(void) tb_sappend(&replacepat, vp);
	return TRUE;
    } else {
	return FALSE;
    }
}

/*
 * Note that searchpat is stored without a trailing null.
 */
int
var_SEARCH(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_copy(rp, searchpat);
	return TRUE;
    } else if (vp && valid_buffer(curbp)) {
	(void) tb_init(&searchpat, EOS);
	(void) tb_sappend(&searchpat, vp);
	beginDisplay();
	FreeIfNeeded(gregexp);
	endofDisplay();
	gregexp = regcomp(tb_values(searchpat),
			  tb_length(searchpat),
			  b_val(curbp, MDMAGIC));
	return TRUE;
    } else {
	return FALSE;
    }
}

int
var_SEED(TBUFF **rp, const char *vp)
{
    if (rp) {
	render_int(rp, vl_seed);
	return TRUE;
    } else if (vp) {
	vl_seed = (int) strtol(vp, NULL, 0);
	srand((UINT) vl_seed);
	return TRUE;
    } else {
	return FALSE;
    }
}

#if OPT_SHELL
int
var_LOOK_IN_CWD(TBUFF **rp, const char *vp)
{
    return any_rw_BOOL(rp, vp, &look_in_cwd);
}

int
var_LOOK_IN_HOME(TBUFF **rp, const char *vp)
{
    return any_rw_BOOL(rp, vp, &look_in_home);
}

int
var_SHELL(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, get_shell());
	return TRUE;
    } else if (vp) {
	SetEnv(&shell, vp);
	return TRUE;
    } else {
	return FALSE;
    }
}
#endif

int
var_SRES(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, screen_desc);
	return TRUE;
    } else if (vp) {
	return term.setdescrip(vp);
    } else {
	return FALSE;
    }
}

int
var_STARTUP_FILE(TBUFF **rp, const char *vp)
{
    return any_rw_TXT(rp, vp, &startup_file);
}

int
var_STARTUP_PATH(TBUFF **rp, const char *vp)
{
    return any_rw_TXT(rp, vp, &startup_path);
}

static int lastcmdstatus = TRUE;

void
setcmdstatus(int s)
{
    lastcmdstatus = s;
}

int
var_STATUS(TBUFF **rp, const char *vp)
{
    return any_ro_BOOL(rp, vp, lastcmdstatus);
}

#if OPT_EVAL
int
var__STATUS(TBUFF **rp, const char *vp)
{
    return any_ro_TBUFF(rp, vp, &last_macro_result);
}

int
var_RETURN(TBUFF **rp, const char *vp)
{
    return any_rw_TBUFF(rp, vp, &this_macro_result);
}
#endif /* OPT_EVAL */

#if OPT_MULTIBYTE
int
var_BUF_ENCODING(TBUFF **rp, const char *vp GCC_UNUSED)
{
    if (rp) {
	int code = (valid_buffer(curbp)
		    ? b_val(curbp, VAL_FILE_ENCODING)
		    : enc_AUTO);
	const char *value = NULL;

	switch ((ENC_CHOICES) (code)) {
	case enc_POSIX:
	    value = "US-ASCII";
	    break;
	case enc_AUTO:
	    /* FALLTHRU */
	case enc_8BIT:
	    if (okCTYPE2(vl_narrow_enc)) {
		value = vl_narrow_enc.encoding;
	    } else {
		value = vl_real_enc.encoding;
	    }
	    break;
	case enc_LOCALE:
	    if (okCTYPE2(vl_wide_enc)) {
		value = vl_wide_enc.encoding;
	    } else {
		value = vl_real_enc.encoding;
	    }
	    break;
	case enc_UTF8:
	    value = "UTF-8";
	    break;
	case enc_UTF16:
	    value = "UTF-16";
	    break;
	case enc_UTF32:
	    value = "UTF-32";
	    break;
	}
	tb_scopy(rp, value);
	return TRUE;
    } else {
	return FALSE;
    }
}

int
var_CMD_ENCODING(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, encoding2s(cmd_encoding));
	return TRUE;
    } else if (vp) {
	int choice = choice_to_code(&fsm_cmd_encoding_blist, vp, strlen(vp));
	cmd_encoding = (ENC_CHOICES) choice;
	set_local_b_val(bminip, VAL_FILE_ENCODING, vl_encoding);
	return TRUE;
    }
    return FALSE;
}

int
var_KBD_ENCODING(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, encoding2s(kbd_encoding));
	return TRUE;
    } else if (vp) {
#if OPT_VL_OPEN_MBTERM
	int choice = choice_to_code(&fsm_kbd_encoding_blist, vp, strlen(vp));
	kbd_encoding = (ENC_CHOICES) choice;
	/* We can use 8-bit keyboard in UTF-8 mode, but have not setup
	 * tables to use UTF-8 keyboard in 8-bit mode (though it is feasible
	 * to implement this).
	 */
	if (kbd_encoding > term.get_enc())
	    kbd_encoding = term.get_enc();
	return TRUE;
#else
	/* treat as readonly value */
	;
#endif
    }
    return FALSE;
}

int
var_TERM_ENCODING(TBUFF **rp, const char *vp GCC_UNUSED)
{
    if (rp) {
	tb_scopy(rp, encoding2s(term.get_enc()));
	return TRUE;
    } else {
	return FALSE;
    }
}

int
var_LATIN1_EXPR(TBUFF **rp, const char *vp)
{
    int rc = any_rw_EXPR(rp, vp, &latin1_expr);
    if (rc && !rp) {
	char *wide = vl_wide_enc.locale;
	char *narrow = vl_narrow_enc.locale;
	if (isEmpty(narrow))
	    narrow = vl_narrowed(wide);
	if (isEmpty(wide))
	    wide = narrow;
	vl_init_8bit(wide, narrow);
    }
    return rc;
}
#endif

int
var_TERM_COLS(TBUFF **rp, const char *vp GCC_UNUSED)
{
    return any_ro_INT(rp, vp, term.cols);
}

int
var_TERM_LINES(TBUFF **rp, const char *vp GCC_UNUSED)
{
    return any_ro_INT(rp, vp, term.rows);
}

int
var_TERM_RESIZES(TBUFF **rp, const char *vp GCC_UNUSED)
{
    int result = FALSE;
#if DISP_X11 || DISP_NTWIN || DISP_NTCONS
    result = TRUE;
#elif (DISP_TERMCAP || DISP_CURSES) && defined(SIGWINCH)
    result = TRUE;
#endif

    return any_ro_BOOL(rp, vp, result);
}

#if OPT_TITLE
int
var_TITLE(TBUFF **rp, const char *vp)
{
    int result = FALSE;
    if (rp) {
#if DISP_X11
	tb_scopy(rp, x_get_window_name());
	result = TRUE;
#elif SYS_WINNT
	tb_scopy(rp, w32_wdw_title());
	result = TRUE;
#endif
    } else if (vp) {
#if DISP_X11 || SYS_WINNT
	tb_scopy(&request_title, vp);
	result = TRUE;
#else
	result = ABORT;		/* read-only */
#endif
    }
    return result;
}

#if OPT_MULTIBYTE
int
var_TITLE_ENCODING(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, encoding2s(title_encoding));
	return TRUE;
    } else if (vp) {
	int choice = choice_to_code(&fsm_title_encoding_blist, vp, strlen(vp));
	title_encoding = (ENC_CHOICES) choice;
	if (auto_set_title) {
	    term.set_title(tb_values(current_title));
	}
	return TRUE;
    }
    return FALSE;
}
#endif

int
var_TITLEFORMAT(TBUFF **rp, const char *vp)
{
    int code = any_rw_STR(rp, vp, &title_format);
    if (!rp && vp)
	set_editor_title();
    return code;
}
#endif /* OPT_TITLE */

int
var_VERSION(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, version);
}

int
var_WITHPREFIX(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, tb_values(with_prefix));
}

int
var_WLINES(TBUFF **rp, const char *vp)
{
    if (rp) {
	render_int(rp, curwp->w_ntrows);
	return TRUE;
    } else if (vp && curwp) {
	return resize(TRUE, (int) strtol(vp, NULL, 0));
    } else {
	return FALSE;
    }
}

int
var_WORD(TBUFF **rp, const char *vp)
{
    return any_CTYPE_MATCH(rp, vp, vl_nonspace);
}

int
var_GET_AT_DOT(TBUFF **rp, const char *vp)
{
    return any_rw_BOOL(rp, vp, &vl_get_at_dot);
}

int
var_GET_IT_ALL(TBUFF **rp, const char *vp)
{
    return any_rw_BOOL(rp, vp, &vl_get_it_all);
}

int
var_GET_LENGTH(TBUFF **rp, const char *vp)
{
    return any_ro_INT(rp, vp, vl_get_length);
}

int
var_GET_OFFSET(TBUFF **rp, const char *vp)
{
    return any_ro_INT(rp, vp, vl_get_offset);
}

int
var_SYSTEM_NAME(TBUFF **rp, const char *vp)
{
    return any_ro_STR(rp, vp, opersys);
}

int
var_SYSTEM_CRLF(TBUFF **rp, const char *vp)
{
    return any_rw_BOOL(rp, vp, &system_crlf);
}

#if OPT_HOOKS
int
var_WRHOOK(TBUFF **rp, const char *vp)
{
    return any_HOOK(rp, vp, &writehook);
}
#endif

#if DISP_X11 && OPT_SHELL
int
var_XDISPLAY(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, get_xdisplay());
	return TRUE;
    } else if (vp) {
	SetEnv(&x_display, vp);
	return TRUE;
    } else {
	return FALSE;
    }
}

int
var_XSHELL(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, get_xshell());
	return TRUE;
    } else if (vp) {
	SetEnv(&x_shell, vp);
	return TRUE;
    } else {
	return FALSE;
    }
}

int
var_XSHELLFLAGS(TBUFF **rp, const char *vp)
{
    if (rp) {
	tb_scopy(rp, get_xshellflags());
	return TRUE;
    } else if (vp) {
	SetEnv(&x_shellflags, vp);
	return TRUE;
    } else {
	return FALSE;
    }
}
#endif
#endif

#if NO_LEAKS
void
ev_leaks(void)
{
#if OPT_EVAL
    UVAR *p;
    while ((p = temp_vars) != NULL)
	rmv_tempvar(p->u_name);

#if OPT_EVAL && OPT_SHELL
    FreeAndNull(shell);
    FreeAndNull(directory);
#if DISP_X11
    FreeAndNull(x_display);
    FreeAndNull(x_shell);
#endif
#endif
#endif
}
#endif /* NO_LEAKS */
