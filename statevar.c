/*
 * statevar.c -- the functions named var_XXXX() referred to by modetbl
 *	for getting and setting the values of the vile state variables,
 *	plus helper utility functions.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/statevar.c,v 1.33 2000/03/14 02:59:33 tom Exp $
 */

#include	"estruct.h"
#include	"edef.h"
#include	"nevars.h"
#include	"patchlev.h"

#if OPT_EVAL && OPT_SHELL
static char *shell;	/* $SHELL environment is "$shell" state variable */
static char *directory;	/* $TMP environment is "$directory" state variable */
#if DISP_X11
static char *x_display;	/* $DISPLAY environment is "$xdisplay" state variable */
static char *x_shell;	/* $XSHELL environment is "$xshell" state variable */
#endif
#endif
#if OPT_PATHLOOKUP
static char *cdpath;	/* $CDPATH environment is "$cdpath" state variable. */
#endif

#if OPT_HOOKS
static int
any_HOOK(TBUFF **rp, const char *vp, HOOK *hook)
{
	if (rp) {
		tb_scopy(rp, hook->proc);
		return TRUE;
	} else if (vp) {
		(void)strcpy(hook->proc, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}
#endif

char *
vile_getenv(char *s)
{
#if	OPT_EVAL && OPT_SHELL
	register char *v = getenv(s);
	return v ? v : "";
#else
	return "";
#endif
}

static char *
DftEnv(
register char	*name,
register char	*dft)
{
#if	OPT_EVAL && OPT_SHELL
	name = vile_getenv(name);
	return (*name == EOS) ? dft : name;
#else
	return dft;
#endif
}

static void
SetEnv(
char	**namep,
const char *value)
{
#if	OPT_EVAL && OPT_SHELL
	FreeIfNeeded(*namep);
#endif
	*namep = strmalloc(value);
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
	if (shell == 0)
		SetEnv(&shell, DftEnv(SHELL_NAME, SHELL_PATH));
	return shell;
}
#endif


#if OPT_EVAL && DISP_X11 && OPT_SHELL
char *
get_xshell(void)
{
	if (x_shell == 0)
		SetEnv(&x_shell, DftEnv("XSHELL", "xterm"));
	return x_shell;
}
#endif

#if OPT_EVAL && DISP_X11 && OPT_SHELL
char *
get_xdisplay(void)
{
	if (x_display == 0)
		SetEnv(&x_display, DftEnv("DISPLAY", x_get_display_name()));
	return x_display;
}
#endif

#if OPT_EVAL && OPT_SHELL
char *
get_directory(void)
{
	if (directory == 0)
		SetEnv(&directory, DftEnv("TMP", P_tmpdir));
	return directory;
}
#endif

/* access the default kill buffer */
static char *
getkill(TBUFF **rp)
{
	tb_init(rp, EOS);
	if (kbs[0].kbufh != 0) {
		int n = index2ukb(0);
		tb_bappend(rp, (char *)(kbs[n].kbufh->d_chunk),
				kbs[n].kused);
	}
	tb_append(rp, EOS);

	return tb_values(*rp);
}

#if OPT_PATHLOOKUP
char *
get_cdpath(void)
{
	if (cdpath == 0)
		SetEnv(&cdpath, DftEnv("CDPATH", ""));
	return cdpath;
}

int var_CDPATH(TBUFF **rp, const char *vp)
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
#if SYS_WINNT && defined(VILE_OLE)
	"oleauto",
#endif
#if OPT_PERL
	"perl",
#endif
#if DISP_TERMCAP
# if USE_TERMINFO
	"terminfo",
# else
	"termcap",
# endif
#endif
	NULL		 /* End of list marker */
    };
    static TBUFF *optstring;

    if (optstring == 0) {
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
int var_ABUFNAME(TBUFF **rp, const char *vp)
{
	BUFFER *bp;
	if (rp) {
		tb_scopy(rp, ((bp = find_alt()) != 0) ? bp->b_bname : "");
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {

		return FALSE;
	}
}

#if OPT_HOOKS
int var_AUTOCOLORHOOK(TBUFF **rp, const char *vp)
{
	return any_HOOK(rp, vp, &autocolorhook);
}
#endif

int var_BCHARS(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, curbp->b_bytecount);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_BFLAGS(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_init(rp, EOS);
		buffer_flags(tb_values(*rp), curbp);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_BLINES(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, curbp->b_linecount);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
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

int var_BRIGHTNESS(TBUFF **rp, const char *vp)
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
		return vile_refresh(FALSE,1);
	} else {
		return FALSE;
	}
}
#endif

#if OPT_HOOKS
int var_BUFHOOK(TBUFF **rp, const char *vp)
{
	return any_HOOK(rp, vp, &bufhook);
}
#endif

int var_CBUFNAME(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, curbp->b_bname);
		return TRUE;
	} else if (vp) {
		if (curbp) {
			set_bname(curbp, vp);
			curwp->w_flag |= WFMODE;
		}
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_BWINDOWS(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, curbp->b_nwnd);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

#if OPT_HOOKS
int var_CDHOOK(TBUFF **rp, const char *vp)
{
	return any_HOOK(rp, vp, &cdhook);
}
#endif

int var_CFGOPTS(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, cfgopts());
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_CFNAME(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, (curbp && curbp->b_fname) ? curbp->b_fname : "");
		return TRUE;
	} else if (vp) {
		if (curbp) {
			ch_fname(curbp, vp);
			curwp->w_flag |= WFMODE;
		}
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_CHAR(TBUFF **rp, const char *vp)
{
	if (rp) {
		if (curbp && !is_empty_buf(curbp)) {
			if (is_at_end_of_line(DOT))
				render_int(rp, '\n');
			else
				render_int(rp, char_at(DOT));
		} else {
			tb_scopy(rp, error_val);
		}
		return TRUE;
	} else if (vp) {
		if (curbp == NULL || b_val(curbp,MDVIEW)) {
			return rdonly();
		} else {
			int s, c;
			mayneedundo();
			(void)ldelete(1L, FALSE); /* delete 1 char */
			c = scan_int(vp);
			if (c == '\n')
				s = lnewline();
			else
				s = linsert(1, c);
			(void)backchar(FALSE, 1);
			return s;
		}
	} else {
		return FALSE;
	}
}

#if OPT_ENCRYPT
int var_CRYPTKEY(TBUFF **rp, const char *vp)
{
	if (rp) {
		return FALSE;  /* write-only */
	} else if (vp) {
		if (cryptkey != 0)
			free(cryptkey);
		cryptkey = (char *)malloc(NKEYLEN);
		vl_make_encrypt_key (cryptkey, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}
#endif

#if !SMALLER
int var_CURCHAR (TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, getcchar() + 1);
		return TRUE;
	} else if (vp) {
		return gotochr(TRUE, strtol(vp,0,0));
	} else {
		return FALSE;
	}
}
#endif

int var_CURCOL(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, getccol(FALSE) + 1);
		return TRUE;
	} else if (vp) {
		return gotocol(TRUE, strtol(vp,0,0));
	} else {
		return FALSE;
	}
}

int var_CURLINE(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, getcline());
		return TRUE;
	} else if (vp) {
		return gotoline(TRUE, strtol(vp,0,0));
	} else {
		return FALSE;
	}
}

#if OPT_SHELL
int var_CWD(TBUFF **rp, const char *vp)
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

int var_CWLINE(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, getlinerow());
		return TRUE;
	} else if (vp) {
		return forwline(TRUE, strtol(vp,0,0) - getlinerow());
	} else {
		return FALSE;
	}
}

int var_DEBUG(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_boolean(rp, tracemacros);
		return TRUE;
	} else if (vp) {
		tracemacros = scan_bool(vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

#if OPT_SHELL
int var_DIRECTORY(TBUFF **rp, const char *vp)
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

int var_DISCMD(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_boolean(rp, !no_msgs);
		return TRUE;
	} else if (vp) {
		no_msgs = !scan_bool(vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_DISINP(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_boolean(rp, !no_echo);
		return TRUE;
	} else if (vp) {
		no_echo = !scan_bool(vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_EOC(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_boolean(rp, ev_end_of_cmd ? 1 : 0);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

#if OPT_FINDERR
int var_FILENAME_EXPR(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, tb_values(filename_expr));
		return TRUE;
	} else if (vp) {
		regexp *exp = regcomp(vp, TRUE);
		if (exp != 0) {
			free(exp);
			tb_scopy(&filename_expr, vp);
			return TRUE;
		}
	}
	return FALSE;
}

int var_ERROR_BUFFER(TBUFF **rp, const char *vp)
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
#endif

int var_EXEC_PATH(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, exec_pathname);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_EXEC_SUFFIX(TBUFF **rp, const char *vp)
{
	if (rp) {
#if OPT_MSDOS_PATH || SYS_VMS
		tb_scopy(rp, ".exe");
#else
		tb_scopy(rp, "");
#endif
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

#if OPT_HOOKS
int var_EXITHOOK(TBUFF **rp, const char *vp)
{
	return any_HOOK(rp, vp, &exithook);
}
#endif

#if DISP_X11||DISP_NTWIN
int var_FONT(TBUFF **rp, const char *vp)
{
#if SYS_WINNT && defined(DISP_NTWIN)
	if (rp) {
		tb_scopy(rp, ntwinio_current_font());
		return TRUE;
	} else if (vp) {
		return ntwinio_font_frm_str(vp, FALSE);
	} else {
		return FALSE;
	}
#endif
#if DISP_X11
	if (rp) {
		tb_scopy(rp, x_current_fontname());
		return TRUE;
	} else if (vp) {
		return x_setfont(vp);
	} else {
		return FALSE;
	}
#endif
}
#endif

int var_FWD_SEARCH(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_boolean(rp, last_srch_direc == FORWARD);
		return TRUE;
	} else if (vp) {
		last_srch_direc = scan_bool(vp) ? FORWARD : REVERSE;
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_HELPFILE(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, helpfile);
		return TRUE;
	} else if (vp) {
		SetEnv(&helpfile, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

#if DISP_X11
int var_ICONNAM(TBUFF **rp, const char *vp)
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

int var_IDENTIF(TBUFF **rp, const char *vp)
{
	if (rp) {
		lgrabtext(rp, vl_ident);
		return TRUE;
	} else if (vp) {
		return lrepltext(vl_ident, vp);
	} else {
		return FALSE;
	}
}

int var_KBDMACRO(TBUFF **rp, const char *vp)
{
	if (rp) {
		get_kbd_macro(rp);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_KILL(TBUFF **rp, const char *vp)
{
	if (rp) {
		getkill(rp);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_LASTKEY(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, lastkey);
		return TRUE;
	} else if (vp) {
		lastkey = scan_int(vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_LCOLS(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, offs2col(curwp, DOT.l, llength(DOT.l)));
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_LIBDIR_PATH(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, libdir_path);
		return TRUE;
	} else if (vp) {
		SetEnv(&libdir_path, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_LINE(TBUFF **rp, const char *vp)
{
	if (rp) {
		lgrabtext(rp, (CHARTYPE)0);
		return TRUE;
	} else if (vp) {
		return lrepltext((CHARTYPE)0, vp);
	} else {
		return FALSE;
	}
}

int var_LLENGTH(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, llength(DOT.l));
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

#if OPT_MAJORMODE
int var_MAJORMODE(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, (curbp->majr != 0) ? curbp->majr->name : "");
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}
#endif

int var_MATCH(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, tb_length(tb_matched_pat)
					? tb_values(tb_matched_pat) : "");
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_MODE(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, current_modename());
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

#if OPT_MLFORMAT
int var_MLFORMAT(TBUFF **rp, const char *vp)
{
	if (rp) {
		if (modeline_format == 0)
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

int var_MODIFIED(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_boolean(rp, curbp && b_is_changed(curbp));
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

#if OPT_COLOR
int var_NCOLORS(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, ncolors);
		return TRUE;
	} else if (vp) {
		return set_colors(strtol(vp,0,0));
	}
	return FALSE;
}
#else
int var_NCOLORS(TBUFF **rp GCC_UNUSED, const char *vp GCC_UNUSED)
{
	return FALSE;
}
#endif

int var_NTILDES(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, ntildes);
		return TRUE;
	} else if (vp) {
		ntildes = strtol(vp,0,0);
		if (ntildes > 100)
			ntildes = 100;
		return TRUE;
	} else {
		return FALSE;
	}
}

#if OPT_SHELL
int var_OCWD(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, previous_directory());
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}
#endif

int var_OS(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, opersys);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_PAGELEN(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, term.rows);
		return TRUE;
	} else if (vp) {
#if DISP_X11 || DISP_NTWIN
		gui_resize(term.cols, strtol(vp,0,0));
		return TRUE;
#else
		return newlength(TRUE, strtol(vp,0,0));
#endif
	} else {
		return FALSE;
	}
}

int var_PATHSEP(TBUFF **rp, const char *vp)
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
int var_POSFORMAT(TBUFF **rp, const char *vp)
{
	if (rp) {
		if (position_format == 0)
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

int var_CURWIDTH(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, term.cols);
		return TRUE;
	} else if (vp) {
#if DISP_X11 || DISP_NTWIN
		gui_resize(strtol(vp,0,0), term.rows);
		return TRUE;
#else
		return newwidth(TRUE, strtol(vp,0,0));
#endif
	} else {
		return FALSE;
	}
}

int var_PALETTE(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, tb_length(tb_curpalette)
					? tb_values(tb_curpalette) : "");
		return TRUE;
	} else if (vp) {
		return set_palette(vp);
	} else {
		return FALSE;
	}
}

int var_PATCHLEVEL(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, PATCHLEVEL);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_PATHNAME(TBUFF **rp, const char *vp)
{
	if (rp) {
		lgrabtext(rp, vl_pathn);
		return TRUE;
	} else if (vp) {
		return lrepltext(vl_pathn, vp);
	} else {
		return FALSE;
	}
}

int var_PENDING(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_boolean(rp, keystroke_avail());
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_PROCESSID(TBUFF **rp, const char *vp)
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
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_PROGNAME(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, prognam);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_QIDENTIF(TBUFF **rp, const char *vp)
{
	if (rp) {
		lgrabtext(rp, vl_qident);
		return TRUE;
	} else if (vp) {
		return lrepltext(vl_qident, vp);
	} else {
		return FALSE;
	}
}

#if OPT_HOOKS
int var_RDHOOK(TBUFF **rp, const char *vp)
{
	return any_HOOK(rp, vp, &readhook);
}
#endif

int var_REPLACE(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, tb_values(replacepat));
		return TRUE;
	} else if (vp) {
		tb_scopy(&replacepat, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_SEARCH(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, searchpat);
		return TRUE;
	} else if (vp) {
		(void)strcpy(searchpat, vp);
		FreeIfNeeded(gregexp);
		gregexp = regcomp(searchpat, b_val(curbp, MDMAGIC));
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_SEED(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, seed);
		return TRUE;
	} else if (vp) {
		seed = strtol(vp,0,0);
		srand(seed);
		return TRUE;
	} else {
		return FALSE;
	}
}

#if OPT_SHELL
int var_SHELL(TBUFF **rp, const char *vp)
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

int var_SRES(TBUFF **rp, const char *vp)
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

int var_STARTUP_FILE(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, startup_file);
		return TRUE;
	} else if (vp) {
		SetEnv(&startup_file, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_STARTUP_PATH(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, startup_path);
		return TRUE;
	} else if (vp) {
		SetEnv(&startup_path, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

static int lastcmdstatus = TRUE;

void setcmdstatus(int s)
{
	lastcmdstatus = s;
}

int var_STATUS(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_boolean(rp, lastcmdstatus);
		return TRUE;
	} else if (vp) {
		lastcmdstatus = scan_bool(vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

#if SYS_WINNT
int var_TITLE(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, w32_wdw_title());
		return TRUE;
	} else if (vp) {
		term.set_title(vp);
		return TRUE;
	} else {
		return FALSE;
	}
}
#endif

#if DISP_X11
int var_TITLE(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, x_get_window_name());
		return TRUE;
	} else if (vp) {
		term.set_title(vp);
		return TRUE;
	} else {
		return FALSE;
	}
}
#endif

int var_TPAUSE(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, term.pausecount);
		return TRUE;
	} else if (vp) {
		term.pausecount = strtol(vp,0,0);
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_VERSION(TBUFF **rp, const char *vp)
{
	if (rp) {
		tb_scopy(rp, version);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_WLINES(TBUFF **rp, const char *vp)
{
	if (rp) {
		render_int(rp, curwp->w_ntrows);
		return TRUE;
	} else if (vp) {
		return resize(TRUE, strtol(vp,0,0));
	} else {
		return FALSE;
	}
}

int var_WORD(TBUFF **rp, const char *vp)
{
	if (rp) {
		lgrabtext(rp, vl_nonspace);
		return TRUE;
	} else if (vp) {
		return lrepltext(vl_nonspace, vp);
	} else {
		return FALSE;
	}
}

#if OPT_HOOKS
int var_WRHOOK(TBUFF **rp, const char *vp)
{
	return any_HOOK(rp, vp, &writehook);
}
#endif

#if DISP_X11 && OPT_SHELL
int var_XDISPLAY(TBUFF **rp, const char *vp)
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

int var_XSHELL(TBUFF **rp, const char *vp)
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
#endif
#endif

#if NO_LEAKS
void
ev_leaks(void)
{
#if OPT_EVAL
	register UVAR *p;
	while ((p = temp_vars) != 0)
		rmv_tempvar(p->u_name);

	FreeAndNull(shell);
	FreeAndNull(directory);
#if DISP_X11
	FreeAndNull(x_display);
	FreeAndNull(x_shell);
#endif
#endif
}
#endif	/* NO_LEAKS */
