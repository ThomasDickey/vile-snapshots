/*
 * statevar.c -- the functions named var_XXXX() referred to by modetbl
 *	for getting and setting the values of the vile state variables,
 *	plus helper utility functions.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/statevar.c,v 1.7 1999/04/04 23:05:22 cmorgan Exp $
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

#if OPT_PROCEDURES
static int
any_HOOK(char *rp, const char *vp, HOOK *hook)
{
	if (rp) {
		strcpy(rp, hook->proc);
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
		SetEnv(&x_display, DftEnv("DISPLAY", "unix:0"));
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

/* access the default kill buffer.  result buffer must be at least NSTRING */
static char *
getkill(char *rp)
{
	if (!kbs[0].kbufh) {
		*rp = EOS;
	} else {
		(void)strncpy(rp, (char *)(kbs[0].kbufh->d_chunk),
				kbs[0].kused);
		rp[NSTRING-1] = EOS;
	}

	return(rp);
}

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
    return (tb_values(optstring));
}
#endif

#if OPT_EVAL
int var_ABUFNAME(char *rp, const char *vp)
{
	BUFFER *bp;
	if (rp) {
		strcpy(rp, ((bp = find_alt()) != 0) ? bp->b_bname : "");
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {

		return FALSE;
	}
}

int var_BCHARS(char *rp, const char *vp)
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

int var_BLINES(char *rp, const char *vp)
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

#if OPT_PROCEDURES
int var_BUFHOOK(char *rp, const char *vp)
{
	return any_HOOK(rp, vp, &bufhook);
}
#endif

int var_CBUFNAME(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, curbp->b_bname);
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

#if OPT_PROCEDURES
int var_CDHOOK(char *rp, const char *vp)
{
	return any_HOOK(rp, vp, &cdhook);
}
#endif

int var_CFGOPTS(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, cfgopts());
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_CFNAME(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, (curbp && curbp->b_fname) ? curbp->b_fname : "");
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

int var_CURCHAR(char *rp, const char *vp)
{
	if (rp) {
		if (curbp && !is_empty_buf(curbp)) {
			if (is_at_end_of_line(DOT))
				render_int(rp, '\n');
			else
				render_int(rp, char_at(DOT));
		} else {
			strcpy(rp, errorm);
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
int var_CRYPTKEY(char *rp, const char *vp)
{
	if (rp) {
		return FALSE;  /* write-only */
	} else if (vp) {
		if (cryptkey != 0)
			free(cryptkey);
		cryptkey = malloc(NKEYLEN);
		vl_make_encrypt_key (cryptkey, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}
#endif

int var_CURCOL(char *rp, const char *vp)
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

int var_CURLINE(char *rp, const char *vp)
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
int var_CWD(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, current_directory(FALSE));
		return TRUE;
	} else if (vp) {
		return set_directory(vp);
	} else {
		return FALSE;
	}
}
#endif

int var_CWLINE(char *rp, const char *vp)
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

int var_DEBUG(char *rp, const char *vp)
{
	if (rp) {
		render_boolean(rp, macbug);
		return TRUE;
	} else if (vp) {
		macbug = scan_bool(vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

#if OPT_SHELL
int var_DIRECTORY(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, get_directory());
		return TRUE;
	} else if (vp) {
		SetEnv(&directory, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}
#endif

int var_DISCMD(char *rp, const char *vp)
{
	if (rp) {
		render_boolean(rp, discmd);
		return TRUE;
	} else if (vp) {
		discmd = scan_bool(vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_DISINP(char *rp, const char *vp)
{
	if (rp) {
		render_boolean(rp, disinp);
		return TRUE;
	} else if (vp) {
		disinp = scan_bool(vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_EOC(char *rp, const char *vp)
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

#if OPT_PROCEDURES
int var_EXITHOOK(char *rp, const char *vp)
{
	return any_HOOK(rp, vp, &exithook);
}
#endif

int var_FLICKER(char *rp, const char *vp)
{
	if (rp) {
		render_boolean(rp, flickcode);
		return TRUE;
	} else if (vp) {
		flickcode = scan_bool(vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

#if DISP_X11||DISP_NTWIN
int var_FONT(char *rp, const char *vp)
{
#if SYS_WINNT && defined(DISP_NTWIN)
	if (rp) {
		strcpy(rp, ntwinio_current_font());
		return TRUE;
	} else if (vp) {
		return ntwinio_font_frm_str(vp, FALSE);
	} else {
		return FALSE;
	}
#endif
#if DISP_X11
	if (rp) {
		strcpy(rp, x_current_fontname());
		return TRUE;
	} else if (vp) {
		return x_setfont(vp);
	} else {
		return FALSE;
	}
#endif
}
#endif

int var_FWD_SEARCH(char *rp, const char *vp)
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

int var_HELPFILE(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, helpfile);
		return TRUE;
	} else if (vp) {
		SetEnv(&helpfile, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

#if DISP_X11
int var_ICONNAM(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, x_get_icon_name());
		return TRUE;
	} else if (vp) {
		x_set_icon_name(vp);
		return TRUE;
	} else {
		return FALSE;
	}
}
#endif

int var_IDENTIF(char *rp, const char *vp)
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

int var_KILL(char *rp, const char *vp)
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

int var_LASTKEY(char *rp, const char *vp)
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

int var_LIBDIR_PATH(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, libdir_path);
		return TRUE;
	} else if (vp) {
		SetEnv(&libdir_path, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_LINE(char *rp, const char *vp)
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

int var_LLENGTH(char *rp, const char *vp)
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
int var_MAJORMODE(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, (curbp->majr != 0) ? curbp->majr->name : "");
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}
#endif

int var_MATCH(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, tb_length(tb_matched_pat)
					? tb_values(tb_matched_pat) : "");
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_MODE(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, current_modename());
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

#if OPT_MLFORMAT
int var_MLFORMAT(char *rp, const char *vp)
{
	if (rp) {
		if (modeline_format == 0)
			mlforce("BUG: modeline_format uninitialized");
		else
		    	strcpy(rp, modeline_format);
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

int var_MODIFIED(char *rp, const char *vp)
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

int var_NCOLORS(char *rp, const char *vp)
{
#if OPT_COLOR
	if (rp) {
		render_int(rp, ncolors);
		return TRUE;
	} else if (vp) {
		return set_ncolors(strtol(vp,0,0));
	}
#endif
	return FALSE;
}

int var_NTILDES(char *rp, const char *vp)
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
int var_OCWD(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, previous_directory());
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}
#endif

int var_OS(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, opersys);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_PAGELEN(char *rp, const char *vp)
{
	if (rp) {
		render_int(rp, term.t_nrow);
		return TRUE;
	} else if (vp) {
#if DISP_X11 || DISP_NTWIN
		gui_resize(term.t_ncol, strtol(vp,0,0));
		return TRUE;
#else
		return newlength(TRUE, strtol(vp,0,0));
#endif
	} else {
		return FALSE;
	}
}

int var_CURWIDTH(char *rp, const char *vp)
{
	if (rp) {
		render_int(rp, term.t_ncol);
		return TRUE;
	} else if (vp) {
#if DISP_X11 || DISP_NTWIN
		gui_resize(strtol(vp,0,0), term.t_nrow);
		return TRUE;
#else
		return newwidth(TRUE, strtol(vp,0,0));
#endif
	} else {
		return FALSE;
	}
}

int var_PALETTE(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, tb_values(tb_curpalette));
		return TRUE;
	} else if (vp) {
		return set_palette(vp);
	} else {
		return FALSE;
	}
}

int var_PATCHLEVEL(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, PATCHLEVEL);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_PATHNAME(char *rp, const char *vp)
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

int var_PENDING(char *rp, const char *vp)
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

int var_PROCESSID(char *rp, const char *vp)
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

int var_PROGNAME(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, prognam);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_QIDENTIF(char *rp, const char *vp)
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

#if OPT_PROCEDURES
int var_RDHOOK(char *rp, const char *vp)
{
	return any_HOOK(rp, vp, &readhook);
}
#endif

int var_REPLACE(char *rp, const char *vp)
{
	if (rp) {
	    	strcpy(rp, rpat);
		return TRUE;
	} else if (vp) {
		(void)strcpy(rpat, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_SEARCH(char *rp, const char *vp)
{
	if (rp) {
	    	strcpy(rp, pat);
		return TRUE;
	} else if (vp) {
		(void)strcpy(pat, vp);
		FreeIfNeeded(gregexp);
		gregexp = regcomp(pat, b_val(curbp, MDMAGIC));
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_SEED(char *rp, const char *vp)
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
int var_SHELL(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, get_shell());
		return TRUE;
	} else if (vp) {
		SetEnv(&shell, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}
#endif

int var_SRES(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, sres);
		return TRUE;
	} else if (vp) {
		return TTrez(vp);
	} else {
		return FALSE;
	}
}

int var_STARTUP_FILE(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, startup_file);
		return TRUE;
	} else if (vp) {
		SetEnv(&startup_file, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_STARTUP_PATH(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, startup_path);
		return TRUE;
	} else if (vp) {
		SetEnv(&startup_path, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_STATUS(char *rp, const char *vp)
{
	if (rp) {
		render_boolean(rp, cmdstatus);
		return TRUE;
	} else if (vp) {
		cmdstatus = scan_bool(vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

#if SYS_WINNT
int var_TITLE(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, w32_wdw_title());
	} else if (vp) {
		TTtitle((char *) vp);
		return TRUE;
	} else {
		return FALSE;
	}
}
#endif

#if DISP_X11
int var_TITLE(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, x_get_window_name());
		return TRUE;
	} else if (vp) {
		x_set_window_name(vp);
		return TRUE;
	} else {
		return FALSE;
	}
}
#endif

int var_TPAUSE(char *rp, const char *vp)
{
	if (rp) {
		render_int(rp, term.t_pause);
		return TRUE;
	} else if (vp) {
		term.t_pause = strtol(vp,0,0);
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_VERSION(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, version);
		return TRUE;
	} else if (vp) {
		return ABORT;  /* read-only */
	} else {
		return FALSE;
	}
}

int var_WLINES(char *rp, const char *vp)
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

int var_WORD(char *rp, const char *vp)
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

#if OPT_PROCEDURES
int var_WRHOOK(char *rp, const char *vp)
{
	return any_HOOK(rp, vp, &writehook);
}
#endif

#if DISP_X11 && OPT_SHELL
int var_XDISPLAY(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, get_xdisplay());
		return TRUE;
	} else if (vp) {
		SetEnv(&x_display, vp);
		return TRUE;
	} else {
		return FALSE;
	}
}

int var_XSHELL(char *rp, const char *vp)
{
	if (rp) {
		strcpy(rp, get_xshell());
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
