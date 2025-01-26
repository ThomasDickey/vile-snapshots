/*
 * vile -- "vi like emacs"
 *
 * this used to be MicroEMACS, a public domain program written by
 * dave g. conroy, further improved and modified by daniel m. lawrence.
 *
 * the original author of vile is paul fox.  tom dickey and kevin
 * buettner made huge contributions along the way, as did rick
 * sladkey and other people (see all of the CHANGES files for
 * details).  vile is now principally maintained by tom dickey.
 *
 * previous versions of vile were limited to non-commercial use due to
 * their inclusion of code from versions of MicroEMACS which were
 * restricted in that way.  in the current version of vile, every
 * attempt has been made to ensure that this later code has been
 * removed or rewritten, returning vile's original basis to freely
 * distributable status.  This version of vile is distributed under the
 * terms of the GNU Public License (see COPYING).
 *
 * Copyright (c) 1992-2024,2025 by Paul Fox and Thomas Dickey
 */

/*
 * $Id: main.c,v 1.757 2025/01/26 17:07:24 tom Exp $
 */

#define realdef			/* Make global definitions not external */

#include	"estruct.h"	/* global structures and defines */
#include	"edef.h"	/* global declarations */
#include	"nevars.h"
#include	"nefunc.h"
#include	"nefsms.h"

#include	<ctype.h>

#if OPT_LOCALE
#include	<locale.h>
#endif /* OPT_LOCALE */

#if defined(HAVE_SETGROUPS)
#include	<grp.h>
#endif

#if CC_NEWDOSCC
#include <io.h>
#if CC_DJGPP
#include <dpmi.h>
#include <go32.h>
#endif
#endif

#if SYS_VMS
#include <processes.h>
#endif

#if OPT_FILTER
#include <filters.h>
#endif

#if OPT_PERL
#if defined(DECL_ENVIRON)
extern char **environ;
#elif !defined(HAVE_ENVIRON)
static char **my_environ = 0;
#define environ my_environ
#endif
#endif

extern char *exec_pathname;
extern const char *const pathname[];	/* startup file path/name array */

/* if on a crippled system, try to survive: bigger stack */
#if SYS_MSDOS && CC_TURBO
unsigned _stklen = 24000U;
#endif

static int cmd_mouse_motion(const CMDFUNC * cfp);
static void get_executable_dir(void);
static void global_val_init(void);
static void main_loop(void);
static void make_startup_file(const char *name);
static void setup_sighandlers(int enabled);

#if OPT_MULTIBYTE
static void pre_init_ctype(void);
#endif

extern const int nametbl_size;
extern const int glbstbl_size;

/*--------------------------------------------------------------------------*/

/*
 * Get the argument parameter.  The 'param' points to the option flag.  Set it
 * to a 1-character string to force the next entry from argv[] to be fetched.
 */
static char *
get_argvalue(char *param, char *argv[], int *argcp)
{
    if (!*(++param)) {
	*argcp += 1;
	param = argv[*argcp];
    }
    if (param == NULL)
	print_usage(BADEXIT);
    return param;
}

#define	GetArgVal(param)	get_argvalue(param, argv, &carg)

/*--------------------------------------------------------------------------*/

static void
save_buffer_state(BUFFER **save_bp, UINT * save_flags)
{
    *save_bp = curbp;
    if (*save_bp) {
	*save_flags = (*save_bp)->b_flag;
	/* mark as modified, to prevent undispbuff() from clobbering */
	b_set_changed(*save_bp);
    }
}

static void
restore_buffer_state(BUFFER *save_bp, UINT save_flags)
{
    if (save_bp) {
	save_bp->b_flag = save_flags;
	swbuffer(save_bp);
    }
}

static void
hide_and_discard(BUFFER **bp)
{
    if (zotwp(*bp)) {
	zotbuf(*bp);
	*bp = NULL;
    }
}

static int
run_startup_commands(BUFFER **bp)
{
    BUFFER *cmds = *bp;
    int result = TRUE;
    BUFFER *save_bp = NULL;	/* saves curbp when doing startup commands */
    LINE *lp;
    UINT save_flags = 0;	/* saves curbp's flags when doing startup */

    TRACE((T_CALLED "run_startup_commands %s\n", cmds->b_bname));

    save_buffer_state(&save_bp, &save_flags);

    /* remove blank lines to make the trace simpler */
    for (lp = lforw(buf_head(cmds)); lp != buf_head(cmds);) {
	LINE *nlp = lforw(lp);

	if (lisreal(lp) && !llength(lp)) {
	    lremove2(cmds, lp);
	}
	lp = nlp;
    }

    /* don't want swbuffer to try to read it */
    cmds->b_active = TRUE;
    set_rdonly(cmds, cmds->b_fname, MDVIEW);

    /* go execute it! */
    if (dobuf(cmds, 1, -1) != TRUE) {
	result = FALSE;
    }
    /* if we managed to load a buffer, don't go back to the unnamed buffer */
    if (cmds != save_bp
	&& eql_bname(save_bp, UNNAMED_BufName)
	&& is_empty_buf(save_bp)
	&& save_bp->b_nwnd == 0
	&& !b_is_scratch(curbp)) {
	b_clr_changed(save_bp);
	(void) zotbuf(save_bp);
    } else {
	restore_buffer_state(save_bp, save_flags);
    }
    if (result) {		/* remove the now unneeded buffer */
	b_set_scratch(cmds);	/* make sure it will go */
	hide_and_discard(bp);
    }

    returnCode(result);
}

static int
run_and_discard(BUFFER **bp)
{
    int rc = run_startup_commands(bp);

    hide_and_discard(bp);
    return rc;
}

static void
add_cmdarg(BUFFER *bp, const char *cmd, const char *arg)
{
    TBUFF *tb = NULL;
    if (tb_scopy(&tb, arg) != NULL
	&& tb_enquote(&tb) != NULL) {
	b2printf(bp, cmd, tb_values(tb));
    }
    tb_free(&tb);
}

static void
setup_command(BUFFER *opts_bp, char *param)
{
    char *p1;
    char *p2;
    const CMDFUNC *cfp;

    /*
     * Check for special cases where a leading number should be treated
     * as a repeat-count (see docmd(), which does something similar).
     */
    param = skip_blanks(param);
    p1 = skip_number(param);
    if (p1 != param) {		/* we found a number */
	p2 = skip_blanks(p1);
	if (*p2 == '*' || *p2 == ':') {
	    b2printf(opts_bp, "%.*s ", (int) (p1 - param), param);
	    param = skip_blanks(p2 + 1);
	}
    }

    /*
     * As a special case, quote simple commands such as forward and reverse
     * search.
     */
    cfp = DefaultKeyBinding(CharOf(*param));
    if (isPunct(CharOf(*param))
	&& ((cfp == &f_forwsearch)
	    || (cfp == &f_backsearch)
#if OPT_SHELL
	    || (cfp == &f_capturecmd)
	    || (cfp == &f_operfilter)
#endif
	)) {
	add_cmdarg(opts_bp, "execute-named-command %s\n", param);
    } else {
	b2printf(opts_bp, "execute-named-command %s\n", param);
    }
}

static int
default_fill(void)
{
    /* this comes out to 70 on an 80 (or greater) column display */
    int fill;
    fill = (7 * term.cols) / 8;	/* must be done after vtinit() */
    if (fill > 70)
	fill = 70;
    return fill;
}

#if OPT_LOCALE
static char *
get_set_locale(const char *value)
{
    char *result = setlocale(LC_CTYPE, value);
    if (result != NULL)
	result = strmalloc(result);
    return result;
}

#ifdef HAVE_LANGINFO_CODESET
static void
set_posix_locale(void)
{
    TRACE(("...reset locale to POSIX!\n"));
    vl_real_enc.locale = get_set_locale("C");
    vl_get_encoding(&vl_real_enc.encoding, vl_real_enc.locale);
    vl_wide_enc.locale = NULL;
    vl_narrow_enc.locale = NULL;
}
#endif
#endif /* OPT_LOCALE */

/*
 * For command-line use, run the appropriate syntax filter on the current
 * buffer, writing the attributed text to the standard output.  This assumes
 * that if filters are built-in, that the appropriate one is available.
 */
static void
filter_to_stdio(FILE *fp)
{
    static char *my_macro;
    int s;

    TRACE(("SyntaxFilter-only:\n"));
    TRACE(("\tcurfname:%s\n", curbp->b_fname));

#if OPT_FILTER && OPT_MAJORMODE
    if (curbp->majr != NULL && flt_lookup(curbp->majr->shortname)) {
	const CMDFUNC *cmd = &f_operattrdirect;
	clexec = TRUE;
	filter_only = TRUE;
	gotobob(FALSE, 0);
	havemotion = &f_gotoeob;
	(void) CMD_U_FUNC(cmd) (FALSE, 0);
    } else
#endif
    {
	/*
	 * Rely upon the macro to construct the proper filter path.
	 */
	if (my_macro == NULL)
	    my_macro = strmalloc("HighlightFilter");

	/*
	 * If we used a built-in filter, its output is written as a side effect
	 * of calling flt_putc, etc.  Otherwise we have to write it ourselves
	 * now.
	 */
	s = docmd(my_macro, TRUE, FALSE, 0);
	if (s == TRUE) {
	    int nlines;
	    B_COUNT nchars;
	    REGION region;

	    gotoeob(FALSE, 1);
	    setmark();
	    gotobob(FALSE, 1);

	    regionshape = rgn_FULLLINE;
	    if (getregion(curbp, &region)) {

		ffp = fp;
		ffstatus = file_is_pipe;

		write_region(curbp, &region, TRUE, &nlines, &nchars);
	    }
	}
    }
}

/*--------------------------------------------------------------------------*/

int
MainProgram(int argc, char *argv[])
{
    static char dft_vileinit[] = "vileinit.rc";
    int tt_opened = 0;
    BUFFER *bp = NULL;
    int carg = 0;		/* current arg to scan */
    int literal = FALSE;	/* force args to be interpreted as filenames */
    char *vileinit = NULL;	/* the startup file or VILEINIT var */
    int startstat = TRUE;	/* result of running startup */
    BUFFER *havebp = NULL;	/* initial buffer to read */
    BUFFER *init_bp = NULL;	/* may contain startup commands */
    BUFFER *opts_bp = NULL;	/* may contain startup commands */
    char *havename = NULL;	/* name of first buffer in cmd line */
    const char *msg = NULL;
#if SYS_VMS
    char *init_descrip = NULL;
#endif
#ifdef VILE_OLE
    int ole_register = FALSE;
#endif
#if DISP_NTCONS
    int new_console = FALSE;
#endif
#if OPT_ENCRYPT
    char startkey[NKEYLEN];	/* initial encryption key */
    memset(startkey, 0, sizeof(startkey));
#endif

    /*
     * This is needed for a fallback case in locales.
     */
#if OPT_MULTIBYTE
    pre_init_ctype();
#endif

#if OPT_LOCALE
    {
	const char *env = "";
	char *old_locale = get_set_locale("");
	char *old_encoding = NULL;

	/*
	 * If the environment specifies a legal locale, old_locale will be
	 * nonnull.
	 */
	TRACE(("old_locale '%s'\n", NONNULL(old_locale)));
#if DISP_TERMCAP || DISP_CURSES || DISP_X11
	/*
	 * Infer a narrow 8-bit locale, if we can.  This is a special case,
	 * assumes that the name of the 8-bit locale can be found by stripping
	 * the "UTF-8" string, and also that both locales are installed.
	 */
	if (old_locale != NULL
	    && vl_is_utf8_encoding(vl_get_encoding(&old_encoding, old_locale))
	    && (((env = sys_getenv("LC_ALL")) != NULL && *env != 0) ||
		((env = sys_getenv("LC_CTYPE")) != NULL && *env != 0) ||
		((env = sys_getenv("LANG")) != NULL && *env != 0))) {
	    char *tmp;

	    TRACE(("Checking for UTF-8 suffix of '%s'\n", env));
	    if ((tmp = vl_narrowed(env)) != NULL) {
		vl_init_8bit(env, tmp);
		env = tmp;
	    } else {
		vl_init_8bit(env, env);
	    }
	} else {
	    env = old_locale;
	    vl_init_8bit(env, env);
	}
#elif DISP_NTWIN || DISP_NTCONS
#ifdef UNICODE
	vl_init_8bit("utf8", old_locale);
#else
	vl_init_8bit(VL_LOC_LATIN1, old_locale);
#endif
#endif
	/* update locale after stripping suffix */
	vl_real_enc.locale = get_set_locale(env);
	TRACE(("setlocale(%s) -> '%s'\n", NONNULL(env), NONNULL(vl_real_enc.locale)));

#ifdef HAVE_LANGINFO_CODESET
	/*
	 * If we have a valid narrow locale, get its encoding.  If that looks
	 * like an 8-bit encoding, we'll maintain both within vile.
	 *
	 * If we have no valid narrow locale, this may be because only the wide
	 * locale is installed (or stripping the suffix did not work due to
	 * unexpected naming convention).  Check if the wide locale's encoding
	 * uses UTF-8 so we'll know to use UTF-8 encoding, as well as construct
	 * character-class data for the 8bit from the wide character data.
	 *
	 * If neither scheme works, fallback to POSIX locale and its
	 * corresponding encoding.
	 */
	if (!okCTYPE2(vl_real_enc)) {
	    vl_get_encoding(&vl_real_enc.encoding, env);
	} else {
	    vl_get_encoding(&vl_real_enc.encoding, vl_real_enc.locale);
	}

	if (!okCTYPE2(vl_real_enc)
	    || isEmpty(vl_real_enc.encoding)) {
	    TRACE(("...checking original locale '%s'\n", NonNull(old_locale)));
	    if (vl_8bit_builtin()) {
		TRACE(("using built-in locale data\n"));
	    } else if (vl_is_utf8_encoding(old_encoding)) {
		TRACE(("original encoding is UTF-8\n"));
		vl_narrow_enc.locale = NULL;
	    } else {
		set_posix_locale();
	    }
	} else if (vl_is_utf8_encoding(vl_real_enc.encoding)) {
	    TRACE(("narrow encoding '%s' is already UTF-8!\n", NonNull(vl_real_enc.encoding)));
	    vl_narrow_enc.locale = NULL;
	} else if (!vl_is_8bit_encoding(vl_real_enc.encoding)) {
	    set_posix_locale();
	}
#endif
	if (vl_real_enc.locale == NULL)
	    vl_real_enc.locale = strmalloc("built-in");
	if (vl_real_enc.encoding == NULL)
	    vl_real_enc.encoding = strmalloc(VL_LOC_LATIN1);

	FreeIfNeeded(old_locale);
	FreeIfNeeded(old_encoding);
    }
#endif /* OPT_LOCALE */

#if OPT_PERL
    perl_init(&argc, &argv, &environ);
#endif

#if OPT_NAMEBST
    build_namebst(nametbl, 0, nametbl_size - 1, 0);
    build_namebst(glbstbl, 0, glbstbl_size - 1, GLOBOK);
#endif
    vl_ctype_init(global_g_val(GVAL_PRINT_LOW),
		  global_g_val(GVAL_PRINT_HIGH));
    global_val_init();		/* global buffer values */
    winit(FALSE);		/* command-line */
#if !SYS_UNIX
    vl_glob_opts = vl_GLOB_MIN;
    expand_wild_args(&argc, &argv);
#endif
    vl_glob_opts = vl_GLOB_ALL;
    prog_arg = strmalloc(argv[0]);	/* our only clue to exec-path */
#if SYS_MSDOS || SYS_OS2 || SYS_OS2_EMX || SYS_WINNT
    if (strchr(pathleaf(prog_arg), '.') == 0) {
	char *t = typeallocn(char, strlen(prog_arg) + 5);
	lsprintf(t, "%s.exe", prog_arg);
	free(prog_arg);
	prog_arg = t;
    }
#endif

    /*
     * Attempt to appease perl if we're running in a process which was setuid'd
     * or setgid'd.
     */
#if defined(HAVE_SETUID) && defined(HAVE_SETGID) && defined(HAVE_GETEGID) && defined(HAVE_GETEUID)
    if (geteuid() != getuid() || getegid() != getgid()) {
#if defined(HAVE_SETGROUPS)
	gid_t gid_list[2];
	gid_list[0] = getegid();
	IGNORE_RC(setgroups(1, gid_list));
#endif
	IGNORE_RC(setgid(getegid()));
	IGNORE_RC(setuid(geteuid()));
    }
#endif /* HAVE_SETUID, etc */

    get_executable_dir();

    if (strcmp(pathleaf(prog_arg), "view") == 0)
	set_global_b_val(MDREADONLY, TRUE);

#if DISP_X11
    if (argc != 2 || strcmp(argv[1], "-V") != 0) {
	if (x_preparse_args(&argc, &argv) != TRUE)
	    startstat = FALSE;
    }
#endif

    /*
     * Allow for I/O to the command-line before we initialize the screen
     * driver.
     *
     * FIXME: we only know how to do this for displays that open the
     * terminal in the same way for command-line and screen.
     */
    setup_sighandlers(TRUE);
#if OPT_DUMBTERM
    /* try to avoid staircasing if -F option goes directly to terminal */
    for (carg = 1; carg < argc; ++carg) {
	if (!strcmp(argv[carg], "-F")) {
	    filter_only = -TRUE;
	    break;
	}
    }
    if (!filter_only
	&& isatty(fileno(stdin))
	&& isatty(fileno(stdout))) {
	tt_opened = open_terminal(&dumb_term);
    } else
#endif
	tt_opened = open_terminal(&null_term);

    /*
     * Create buffers for storing command-line options.
     */
    if ((init_bp = bfind(VILEINIT_BufName, BFEXEC | BFINVS)) == NULL)
	tidy_exit(BADEXIT);

    if ((opts_bp = bfind(VILEOPTS_BufName, BFEXEC | BFINVS)) == NULL)
	tidy_exit(BADEXIT);

    /* Parse the passed in parameters */
    for (carg = 1; carg < argc; ++carg) {
	char *param = argv[carg];

	/* evaluate switches */
	if (*param == '-' && !literal) {
	    ++param;
	    /* all arguments following -- are interpreted as filenames */
	    if (!strcmp(param, "-")) {
		literal = TRUE;
		continue;
	    }
#if DISP_BORLAND || SYS_VMS
	    /* if it's a digit, it's probably a screen
	       resolution */
	    if (isDigit(*param)) {
#if DISP_BORLAND
		current_res_name = param;
#else
		if (strcmp(param, "132") == 0)
		    init_descrip = "WIDE";
		else if (strcmp(param, "80") == 0)
		    init_descrip = "NORMAL";
		else
		    print_usage(BADEXIT);
#endif
		continue;
	    } else
#endif /* screen resolution stuff */
		switch (*param) {
		case 'c':
#if DISP_X11
		    if (!strcmp(param, "class")) {
			++carg;
			break;
		    }
#endif
#if DISP_NTCONS
		    if (strcmp(param, "console") == 0) {
			/*
			 * start editor in a new console env if
			 * stdin is redirected.  if this option
			 * is not set when stdin is redirected,
			 * console vile's mouse features are
			 * unavailable (bug).
			 */
			new_console = TRUE;
			break;
		    }
#endif /* DISP_NTCONS */
		    setup_command(opts_bp, GetArgVal(param));
		    break;
#if OPT_EVAL || OPT_DEBUGMACROS
		case 'D':
		    /* must be outside [vileinit] and [vileopts] */
		    tracemacros = TRUE;
		    break;
#endif
		case 'e':	/* -e for Edit file */
		case 'E':
		    b2printf(opts_bp, "set noview\n");
		    break;
		case 'F':
		    filter_only = -TRUE;
		    break;
		case 'g':	/* -g for initial goto */
		case 'G':
		    add_cmdarg(opts_bp, "%s goto-line\n", GetArgVal(param));
		    break;
		case 'h':	/* -h for initial help */
		case 'H':
		    b2printf(opts_bp, "help\n");
		    break;

		case 'i':
		case 'I':
		    vileinit = dft_vileinit;
		    /*
		     * If the user has no startup file, make a simple
		     * one that points to this one.
		     */
		    if (cfg_locate(vileinit, LOCATE_SOURCE) != NULL
			&& cfg_locate(startup_file, LOCATE_SOURCE) == NULL)
			make_startup_file(vileinit);
		    add_cmdarg(init_bp, "source %s\n", vileinit);
		    break;

#if OPT_ENCRYPT
		case 'k':	/* -k<key> for code key */
		case 'K':
		    param = GetArgVal(param);
		    vl_make_encrypt_key(startkey, param);
		    break;
#endif
#ifdef VILE_OLE
		case 'O':
		    if (param[1] == 'r')
			ole_register = TRUE;
		    else
			print_usage(BADEXIT);
		    break;
#endif
		case 's':	/* -s <pattern> */
		case 'S':
		    add_cmdarg(opts_bp, "search-forward %s\n", GetArgVal(param));
		    break;
#if OPT_TAGS
		case 't':	/* -t for initial tag lookup */
		case 'T':
		    add_cmdarg(opts_bp, "tag %s\n", GetArgVal(param));
		    break;
#endif
		case 'U':
		    set_global_b_val(MDDOS, system_crlf = TRUE);
		    break;
		case 'u':
		    set_global_b_val(MDDOS, system_crlf = FALSE);
		    break;
		case 'v':	/* -v is view mode */
		    b2printf(opts_bp, "set view\n");
		    break;

		case 'R':	/* -R is readonly mode */
		    b2printf(opts_bp, "set readonly\n");
		    break;

		case 'V':
#if DISP_NTWIN
		    gui_version(prog_arg);
#else
		    (void) printf("%s\n", getversion());
#endif
		    tidy_exit(GOODEXIT);
		    /* NOTREACHED */

		case '?':
		    /* FALLTHRU */
		default:	/* unknown switch */
		    print_usage(GOODEXIT);
		}
	} else if (*param == '+' && !literal) {		/* alternate form of -g */
	    setup_command(opts_bp, GetArgVal(param));
	} else if (*param == '@' && !literal) {
	    vileinit = ++param;
	    add_cmdarg(init_bp, "source %s\n", param);
	} else if (*param != EOS) {

	    /* must be a filename */
#if OPT_ENCRYPT
	    cryptkey = (*startkey != EOS) ? startkey : NULL;
#endif
	    /* set up a buffer for this file */
	    bp = getfile2bp(param, FALSE, TRUE);
	    if (bp) {
		bp->b_flag |= BFARGS;	/* treat this as an argument */
		make_current(bp);	/* pull it to the front */
		if (!havebp) {
		    havebp = bp;
		    havename = param;
		}
	    }
#if OPT_ENCRYPT
	    cryptkey = NULL;
#endif
	}
    }

    /*
     * Do not try to pipe results (vile has no ex-mode, which would be used
     * in this case).
     */
#if DISP_TERMCAP || DISP_CURSES || DISP_ANSI
    if (!filter_only && !isatty(fileno(stdout))) {
	fprintf(stderr, "vile: ex mode is not implemented\n");
	ExitProgram(BADEXIT);
    }
#endif

#ifdef VILE_OLE
    if (ole_register) {
	/*
	 * Now that all command line options have been successfully
	 * parsed, register the OLE automation server and exit.
	 */

	ntwinio_oleauto_reg();
	/* NOT REACHED */
    }
#endif

    /* if stdin isn't a terminal, assume the user is trying to pipe a
     * file into a buffer.
     */
#if SYS_UNIX || SYS_VMS || SYS_MSDOS || SYS_OS2 || SYS_WINNT
#if DISP_NTWIN
    if (stdin_data_available())
#else
    if (!isatty(fileno(stdin)))
#endif
    {

#if !SYS_WINNT
#if !DISP_X11
#if SYS_UNIX
	const char *tty = NULL;
#else
	FILE *in;
#endif /* SYS_UNIX */
#endif /* DISP_X11 */
#endif /* !SYS_WINNT */
	BUFFER *lastbp = havebp;
	int nline = 0;
	int fd;

	bp = bfind(STDIN_BufName, BFARGS);
	make_current(bp);	/* pull it to the front */
	if (!havebp)
	    havebp = bp;
	fd = dup(fileno(stdin));
	ffp = (fd >= 0) ? fdopen(fd, "r") : NULL;
#if !DISP_X11
# if SYS_UNIX
# if defined(HAVE_TTYNAME)
	if (isatty(fileno(stdout)))
	    tty = ttyname(fileno(stdout));
	if (tty == NULL && isatty(fileno(stderr)))
	    tty = ttyname(fileno(stderr));
# endif
# if defined(HAVE_DEV_TTY)
	if (tty == NULL)
	    tty = "/dev/tty";
# endif
	if (tty == NULL) {
	    fprintf(stderr, "cannot open any terminal\n");
	    tidy_exit(BADEXIT);
	}
	/*
	 * Note: On Linux, the low-level close/dup operation
	 * doesn't work, since something hangs, apparently
	 * because substituting the file descriptor doesn't communicate
	 * properly up to the stdio routines.
	 */
	TRACE(("call freopen(%s) for stdin\n", tty));
	if ((freopen(tty, "r", stdin)) == NULL
	    || !isatty(fileno(stdin))) {
	    TRACE(("...failed to reopen stdin\n"));
	    fprintf(stderr, "cannot open a terminal (%s)\n", tty);
	    tidy_exit(BADEXIT);
	}
	TRACE(("...successfully reopened stdin\n"));
#else
# if SYS_WINNT
#  if DISP_NTCONS
	if (new_console) {
	    if (FreeConsole()) {
		if (!AllocConsole()) {
		    vl_fputs("unable to allocate new console\n", stderr);
		    tidy_exit(BADEXIT);
		}
	    } else {
		vl_fputs("unable to release existing console\n", stderr);
		tidy_exit(BADEXIT);
	    }
	}

	/*
	 * The editor must reopen the console, not fd 0.  If the console
	 * is not reopened, the nt console I/O routines die immediately
	 * when attempting to fetch a STDIN handle.
	 */
	freopen("con", "r", stdin);
#  endif
# else
#  if SYS_VMS
	fd = open("tt:", O_RDONLY, S_IREAD);	/* or sys$command */
#  else	/* e.g., DOS-based systems */
	fd = fileno(stderr);	/* this normally cannot be redirected */
#  endif
	if ((fd >= 0)
	    && (close(0) >= 0)
	    && (fd = dup(fd)) == 0
	    && (in = fdopen(fd, "r")) != 0) {
	    *stdin = *in;	/* FIXME: won't work on opaque FILE's */
	}
#  endif /* SYS_WINNT */
# endif	/* SYS_UNIX */
#endif /* DISP_X11 */

#if OPT_ENCRYPT
	if (*startkey != EOS) {
	    vl_strncpy(bp->b_cryptkey, startkey, sizeof(bp->b_cryptkey));
	    set_local_b_val(bp, MDCRYPT, TRUE);
	}
#endif
	if (ffp != NULL) {
	    (void) slowreadf(bp, &nline);
	    set_rdonly(bp, bp->b_fname, MDREADONLY);
	    (void) ffclose();
	}

	if (is_empty_buf(bp)) {
	    (void) zotbuf(bp);
	    curbp = havebp = lastbp;
	}
#if OPT_FINDERR
	else {
	    set_febuff(bp->b_bname);
	}
#endif
    }
#endif

    if (filter_only) {
#if OPT_COLOR_SCHEMES
	init_scheme();
#endif
    } else {
	if (!tt_opened)
	    setup_sighandlers(TRUE);
	(void) open_terminal((TERM *) 0);
	term.kopen();		/* open the keyboard */
	term.rev(FALSE);

	/*
	 * The terminal driver sets default colors and $palette during the
	 * open_terminal function.  Save that information as the 'default'
	 * color scheme.  We have to do this before reading .vilerc
	 */
#if OPT_COLOR_SCHEMES
	init_scheme();
#endif

	if (vtinit() != TRUE)	/* allocate display memory */
	    tidy_exit(BADEXIT);

	winit(TRUE);		/* windows */

#if OPT_MENUS
	if (delay_menus)
	    vlmenu_load(FALSE, 1);
#endif
    }

    set_global_b_val(VAL_FILL, default_fill());

    /* Create an unnamed buffer, so that the initialization-file will have
     * something to work on.  We don't pull in any of the command-line
     * filenames yet, because some of the initialization stuff has to be
     * triggered by switching buffers after reading the .vilerc file.
     *
     * If nothing modifies it, this buffer will be automatically removed
     * when we switch to the first file (i.e., havebp), because it is
     * empty (and presumably isn't named the same as an actual file).
     */
    bp = bfind(UNNAMED_BufName, 0);
    bp->b_active = TRUE;
#if OPT_DOSFILES
    /* an empty non-existent buffer defaults to line-style
       favored by the OS */
    set_local_b_val(bp, MDDOS, system_crlf);
#endif
    fix_cmode(bp, FALSE);
    swbuffer(bp);

    /*
     * Run the specified, or the system startup file here.  If vileinit is set,
     * it is the name of the user's command-line startup file, e.g.,
     *
     * 'vile @mycmds'
     *
     * If more than one startup file is named, remember the last.
     */
    if (!is_empty_buf(init_bp)) {
	startstat = run_and_discard(&init_bp);
	if (!startstat)
	    goto begin;

	if (!isEmpty(vileinit)) {
	    free(startup_file);
	    startup_file = strmalloc(vileinit);
	}
    } else {

	/* else vileinit is the contents of their VILEINIT variable */
	vileinit = vile_getenv("VILEINIT");
	if (vileinit != NULL) {	/* set... */
	    if (*vileinit) {	/* ...and not null */

		b2printf(init_bp, "%s", vileinit);

		startstat = run_and_discard(&init_bp);
		if (!startstat)
		    goto begin;
	    } else {
		init_bp = zotbuf2(init_bp);
	    }
	} else {		/* find and run .vilerc */
	    init_bp = zotbuf2(init_bp);
	    if (do_source(startup_file, 1, TRUE) != TRUE) {
		startstat = FALSE;
		goto begin;
	    }
	}
    }

    /*
     * before reading, double-check, since a startup-script may
     * have removed the first buffer.
     */
    if (havebp && find_bp(havebp)) {
	if (find_bp(bp) && is_empty_buf(bp) && !b_is_changed(bp))
	    b_set_scratch(bp);	/* remove the unnamed-buffer */
	if (swbuffer(havebp) == TRUE) {
#if OPT_MAJORMODE && OPT_HOOKS
	    /*
	     * Partial fix in case we edit .vilerc (vile.rc) or
	     * filters.rc, or related files that initialize syntax
	     * highlighting.
	     */
	    VALARGS args;
	    infer_majormode(curbp);
#if OPT_MODELINE
	    /* this may override the inferred majormode */
	    do_modelines(curbp);
#endif
	    if (find_mode(curbp, "vilemode", FALSE, &args) == TRUE)
		run_readhook();
#else
	    ;
#endif
	} else {
	    startstat = FALSE;
	}
	if (havename)
	    set_last_file_edited(havename);
	if (bp2any_wp(bp) && bp2any_wp(havebp))
	    zotwp(bp);
    }
    /*
     * Execute command-line options here, after reading the initial buffer.
     * That ensures that they override the init-file(s).
     */
    msg = "[Use ^A-h, ^X-h, or :help to get help]";
    if (!run_and_discard(&opts_bp))
	startstat = FALSE;

#if OPT_POPUP_MSGS
    purge_msgs();
#endif
    if (startstat == TRUE)	/* else there's probably an error message */
	mlforce("%s", msg);

  begin:
#if SYS_VMS
    if (init_descrip) {
	/*
	 * All terminal inits are complete.  Switch to new screen
	 * resolution specified from command line.
	 */
	(void) term.setdescrip(init_descrip);
    }
#endif
    if (filter_only) {
#ifndef filter_to_stdio

	/*
	 * Process files in the order they appear on the command-line.
	 */
	set_global_g_val(GMDABUFF, FALSE);
	sortlistbuffers();
	firstbuffer(FALSE, 0);

	do {
	    filter_only = TRUE;
	    filter_to_stdio(stdout);
	    filter_only = FALSE;	/* ...so nextbuffer doesn't write stdout */
	    fflush(stdout);
	} while (nextbuffer(FALSE, 0) == TRUE);
	ExitProgram(GOODEXIT);
#else
	fprintf(stderr,
		"The -F option is not supported in this configuration\n");
#endif

    } else {
	(void) update(FALSE);

#if DISP_NTWIN
	/* Draw winvile's main window, for the very first time. */
	winvile_start();
#endif

#if OPT_POPUP_MSGS
	if (global_g_val(GMDPOPUP_MSGS) && (startstat != TRUE)) {
	    bp = bfind(MESSAGES_BufName, BFINVS);
	    bsizes(bp);
	    TRACE(("Checking size of popup messages: %d\n", bp->b_linecount));
	    if (bp->b_linecount > 1) {
		int save = global_g_val(GMDPOPUP_MSGS);
		set_global_g_val(GMDPOPUP_MSGS, TRUE);
		popup_msgs();
		tb_init(&mlsave, EOS);
		set_global_g_val(GMDPOPUP_MSGS, save);
	    }
	}
	if (global_g_val(GMDPOPUP_MSGS) == -TRUE)
	    set_global_g_val(GMDPOPUP_MSGS, FALSE);
#endif

#ifdef GMDCD_ON_OPEN
	/* reset a one-shot, e.g., for winvile's drag/drop code */
	if (global_g_val(GMDCD_ON_OPEN) < 0) {
	    set_directory_from_file(curbp);
	    set_global_g_val(GMDCD_ON_OPEN, FALSE);
	}
#endif

	/*
	 * These should be gone, but may have persisted to this point if
	 * there was some problem with the scripting.  Try again.
	 */
	hide_and_discard(&init_bp);
	hide_and_discard(&opts_bp);

	/*
	 * Force the last message, if any, onto the status line.
	 */
	if (tb_length(mlsave)) {
	    tb_append(&mlsave, EOS);
	    mlforce("%.*s", (int) tb_length(mlsave), tb_values(mlsave));
	}

	/* process commands */
	main_loop();

	/* NOTREACHED */
    }
    return BADEXIT;
}

/* this is nothing but the main command loop */
static void
main_loop(void)
{
    const CMDFUNC *cfp = NULL;
    const CMDFUNC *last_cfp = NULL;
    const CMDFUNC *last_failed_motion_cfp = NULL;
    int s, c, f, n;

    for_ever {

#if OPT_WORKING && DOALLOC && (DISP_TERMCAP || DISP_CURSES)
	assert(allow_working_msg());
#endif
	hst_reset();

	/* vi doesn't let the cursor rest on the newline itself.  This
	   takes care of that. */
	/* if we're inserting, or will be inserting again, then
	   suppress.  this happens if we're using arrow keys
	   during insert */
	if (is_at_end_of_line(DOT) && (DOT.o > w_left_margin(curwp)) &&
	    !insertmode && !cmd_mouse_motion(cfp))
	    backchar(TRUE, 1);

	/* same goes for end-of-file -- I'm actually not sure if
	   this can ever happen, but I _am_ sure that it's
	   a lot safer not to let it... */
	if (is_header_line(DOT, curbp) && !is_empty_buf(curbp))
	    (void) backline(TRUE, 1);

	/* start recording for '.' command */
	dotcmdbegin();

	/* bring the screen up to date */
	s = update(FALSE);

	/* get a user command */
	kbd_mac_check();
	c = kbd_seq();

	/*
	 * Now that we have started a command, reset "$_".  If we did this at a
	 * lower level, we could not test for it in macros, etc.
	 */
#if OPT_EVAL
	tb_scopy(&last_macro_result, status2s(TRUE));
#endif

	/* reset the contents of the command/status line */
	if (kbd_length() > 0) {
	    mlerase();
	    if (s != SORTOFTRUE)	/* did nothing due to typeahead */
		(void) update(FALSE);
	}

	f = FALSE;
	n = 1;

#if VILE_SOMEDAY
/* insertion is too complicated to pop in
	and out of so glibly...   -pgf */
#ifdef insertmode
	/* FIXME: Paul and Tom should check this over. */
	if (insertmode != FALSE) {
	    if (!kbd_replaying(FALSE))
		mayneedundo();
	    unkeystroke(c);
	    insert(f, n);
	    dotcmdfinish();
	    continue;
	}
#endif /* insertmode */
#endif /* SOMEDAY */

	do_repeats(&c, &f, &n);

	kregflag = 0;

	/* flag the first time through for some commands -- e.g. subst
	   must know to not prompt for strings again, and pregion
	   must only restart the p-lines buffer once for each
	   command. */
	calledbefore = FALSE;

	/* convert key code to a function pointer */
	cfp = DefaultKeyBinding(c);

	if (cfp == &f_dotcmdplay &&
	    (last_cfp == &f_undo ||
	     last_cfp == &f_forwredo ||
	     last_cfp == &f_backundo ||
	     last_cfp == &f_inf_undo))
	    cfp = &f_inf_undo;

	s = execute(cfp, f, n);

	last_cfp = cfp;

	/* stop recording for '.' command */
	dotcmdfinish();

	/* if we had an error in playback, stop right away, since the remaining
	 * characters may be from the middle of a change-command which failed
	 * due to invalid motion.
	 */
	if ((s == FALSE) && (dotcmdactive == PLAY)) {
	    dotcmdactive = 0;
	}

	/* If this was a motion that failed, sound the alarm (like vi),
	 * but limit it to once, in case the user is holding down the
	 * autorepeat-key.
	 */
	if ((cfp != NULL)
	    && ((cfp->c_flags & MOTION) != 0)
	    && (s == FALSE)) {
	    if (cfp != last_failed_motion_cfp ||
		global_g_val(GMDMULTIBEEP)) {
		last_failed_motion_cfp = cfp;
		kbd_alarm();
	    }
	} else {
	    last_failed_motion_cfp = NULL;	/* avoid noise! */
	}

	attrib_matches();
	regionshape = rgn_EXACT;

	/*
	 * Some perl scripts may not reset this flag.
	 */
	vile_is_busy = FALSE;

    }
}

/* attempt to locate the executable that contains our code.
* leave its directory name in exec_pathname and shorten prog_arg
* to the simple filename (no path).
*/
static void
get_executable_dir(void)
{
#if SYS_UNIX || SYS_VMS || SYS_MSDOS || SYS_OS2
    char temp[NFILEN];
    char *s, *t;

    if (last_slash(prog_arg) == NULL) {
	/* If there are no slashes, we can guess where we came from,
	 */
	if ((s = cfg_locate(prog_arg, FL_PATH | FL_EXECABLE | FL_INSECURE))
	    != NULL)
	    s = strmalloc(s);
    } else {
	/* if there _are_ slashes, then argv[0] was either
	 * absolute or relative. lengthen_path figures it out.
	 */
	s = strmalloc(lengthen_path(vl_strncpy(temp, prog_arg, sizeof(temp))));
    }
    if (s == NULL)
	return;

    t = pathleaf(s);
    if (t != s && t != NULL) {
	/*
	 * On a unix host, 't' points past slash.  On a VMS host,
	 * 't' points to first char after the last ':' or ']' in
	 * the exe's path.
	 */
	free(prog_arg);
	prog_arg = strmalloc(t);
	*t = EOS;
	exec_pathname = strmalloc(lengthen_path(vl_strncpy(temp, s, sizeof(temp))));
#ifndef VILE_STARTUP_PATH
	/* workaround for DJGPP, to add executable's directory to startup path */
	if (vile_getenv("VILE_STARTUP_PATH") == 0) {
	    append_to_path_list(&startup_path, exec_pathname);
	}
#endif
    }
    free(s);
#elif SYS_WINNT
    static HKEY rootkeys[] =
    {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE};

    int j;
    HKEY hkey;
    char buffer[256];

    for (j = 0; j < (int) TABLESIZE(rootkeys); ++j) {
	if (RegOpenKeyEx(rootkeys[j],
			 VILE_SUBKEY,
			 0,
			 KEY_READ,
			 &hkey) == ERROR_SUCCESS) {
	    if (w32_get_reg_sz(hkey, "", buffer, sizeof(buffer)) == ERROR_SUCCESS) {

		/*
		 * If the (Default) key has a value, use it for $exec-path
		 * We need $exec-path to make "winvile -Or" work, since an
		 * install is not guaranteed to put winvile.exe in %PATH%.
		 */
		exec_pathname = strmalloc(buffer);
		(void) RegCloseKey(hkey);
		break;
	    }

	    (void) RegCloseKey(hkey);
	}
    }
#endif
}

void
tidy_exit(int code)
{
#if OPT_PERL
    perl_exit();
#endif
#if DISP_NTWIN
    winvile_cleanup();
#endif
#if DISP_NTCONS
    term.close();
#endif
#if DISP_X11
    term.close();		/* need this if $xshell left subprocesses */
#endif
    term.clean(TRUE);
#ifdef SIGHUP
    setup_handler(SIGHUP, SIG_IGN);
#endif
    ExitProgram(code);
}

#ifndef strmalloc
char *
strmalloc(const char *src)
{
    char *dst = NULL;
    if (src != NULL) {
	beginDisplay();
	dst = castalloc(char, strlen(src) + 1);
	if (dst != NULL)
	    (void) strcpy(dst, src);
	endofDisplay();
    }
    return dst;
}
#endif

int
no_memory(const char *s)
{
    mlforce("[%s] %s", out_of_mem, s);
    return FALSE;
}

#define setIntValue(vp, value) vp->v.i = value
#define setPatValue(vp, value) vp->v.r = new_regexval(value, TRUE)
#define setTxtValue(vp, value) vp->v.p = strmalloc(value)

#define setINT(name,value)  case name: setIntValue(d,value); break
#define setPAT(name,value)  case name: setPatValue(d,value); break
#define setTXT(name,value)  case name: setTxtValue(d,value); break

#define DFT_FENCE_BEGIN "/\\*"
#define DFT_FENCE_END   "\\*/"

#define DFT_FENCE_IF    "^\\s*#\\s*if"
#define DFT_FENCE_ELIF  "^\\s*#\\s*elif\\>"
#define DFT_FENCE_ELSE  "^\\s*#\\s*else\\>"
#define DFT_FENCE_FI    "^\\s*#\\s*endif\\>"

#define DFT_BUFNAME_EXPR "\\(\\[\\([!]\\?[[:print:]]\\+\\)\\]\\)\\|\\([[:ident:].-]\\+\\)"
	/*
	 * The $filename-expr expression is used with a trailing '*' to repeat
	 * the last field.  Otherwise it is complete.
	 *
	 * Limitations - the expressions are used in the error finder, which
	 * has to choose the first pattern which matches a filename embedded
	 * in other text.  UNIX-style pathnames could have embedded blanks,
	 * but it's not common.  It is common on win32, but is not often a
	 * problem with grep, etc., since the full pathname of the file is not
	 * usually given.
	 */
#if OPT_MSDOS_PATH
#define DFT_FILENAME_EXPR "\\(\\([[:alpha:]]:\\)\\|\\([\\\\/]\\{2\\}\\)[[:ident:]]\\+\\)\\?[[:file:]]"
#elif OPT_VMS_PATH
#define DFT_FILENAME_EXPR "[-/\\w.;\\[\\]<>$:]"
#else /* UNIX-style */
#define DFT_FILENAME_EXPR "[[:file:]]"
#endif

#define DFT_IDENTIFIER_EXPR    "\\w\\+"

#if OPT_MULTIBYTE
/* default value assumes POSIX locale, since it is used before vl_ctyle init */
#define DFT_LATIN1_EXPR	"^\\(\
aa\\|\
af\\|\
br\\|\
de\\|\
en\\|\
es\\|\
fr\\|\
fi\\|\
ga\\|\
gb\\|\
gl\\|\
gv\\|\
id\\|\
it\\|\
kl\\|\
ms\\|\
nl\\|\
nn\\|\
om\\|\
pt\\|\
so\\|\
sv\\|\
tl\\)\
_[A-Za-z]\\+$\
"
#endif

			/* where do paragraphs start? */
#define DFT_PARAGRAPHS  "^\\.[ILPQ]P\\>\\|^\\.P\\>\\|\
^\\.LI\\>\\|^\\.[plinb]p\\>\\|^\\.\\?\\s*$"

			/* where do comments start and end */
#define DFT_COMMENTS    "^\\s*/\\?\\(\\s*[#*>/]\\)\\+/\\?\\s*$"

#define DFT_CMT_PREFIX  "^\\s*\\(\\(\\s*[#*>]\\)\\|\\(///*\\)\\)\\+"

			/* where do sections start? */
#define DFT_SECTIONS    "^[{\014]\\|^\\.[NS]H\\>\\|^\\.HU\\?\\>\\|\
^\\.[us]h\\>\\|^+c\\>"

			/* where do sentences start? */
#define DFT_SENTENCES   "[.!?][])\"']* \\?$\\|[.!?][])\"']*  \\|\
^\\.[ILPQ]P\\>\\|^\\.P\\>\\|^\\.LI\\>\\|^\\.[plinb]p\\>\\|^\\.\\?\\s*$"

#if SYS_MSDOS			/* actually, 16-bit ints */
#define DFT_TIMEOUTUSERVAL 30000
#else
#define DFT_TIMEOUTUSERVAL 60000
#endif

#if OPT_MSDOS_PATH
#define DFT_BACKUPSTYLE ".bak"
#else
#define DFT_BACKUPSTYLE "off"
#endif

#if SYS_VMS
#define DFT_CSUFFIX "\\.\\(\\([CHIS]\\)\\|CC\\|CXX\\|HXX\\)\\(;[0-9]*\\)\\?$"
#endif
#if SYS_MSDOS
#define DFT_CSUFFIX "\\.\\(\\([chis]\\)\\|cc\\|cpp\\|cxx\\|hxx\\)$"
#endif
#ifndef DFT_CSUFFIX		/* UNIX or OS2/HPFS (mixed-case names) */
#define DFT_CSUFFIX "\\.\\(\\([CchisS]\\)\\|CC\\|cc\\|cpp\\|cxx\\|hxx\\)$"
#endif

#define DFT_STRIPSUFFIX "\\(\\.orig\\|~\\)$"

#define DFT_FENCE_CHARS "{}()[]"
#define DFT_CINDENT_CHARS ":#" DFT_FENCE_CHARS

#if SYS_VMS
#define DFT_WILDCARD "*.c,*.h,*.com,*.mms"
#endif
#if SYS_UNIX
#define DFT_WILDCARD "*.[chly] *.def *.cc *.cpp *.xs"
#endif
#ifndef DFT_WILDCARD
#define DFT_WILDCARD "*.c *.h *.def *.cc *.cpp *.cs *.xs"
#endif

/*
 * For the given class/mode, initialize the VAL struct to the default value
 * for that mode.
 */
void
init_mode_value(struct VAL *d, MODECLASS v_class, int v_which)
{
    static const char expand_chars[] =
    {
	EXPC_THIS,
	EXPC_THAT,
	EXPC_SHELL,
	EXPC_TOKEN,
	EXPC_RPAT,
	0};

    switch (v_class) {
    case UNI_MODE:
	switch (v_which) {
	    setINT(GMDABUFF, TRUE);	/* auto-buffer */
	    setINT(GMDALTTABPOS, FALSE);	/* emacs-style tab positioning */
	    setINT(GMDERRORBELLS, TRUE);	/* alarms are noticeable */
	    setINT(GMDEXPAND_PATH, FALSE);
	    setINT(GMDFILENAME_IC, DFT_FILE_IC);
	    setINT(GMDIMPLYBUFF, FALSE);	/* imply-buffer */
	    setINT(GMDINSEXEC, FALSE);	/* allow ^F/^B, etc., to be interpreted during insert mode */
	    setINT(GMDMAPLONGER, FALSE);	/* favor longer maps */
	    setINT(GMDMULTIBEEP, TRUE);		/* multiple beeps for multiple motion failures */
	    setINT(GMDPIN_TAGSTACK, FALSE);	/* tagstack push/pop ops are pinned to curwin */
	    setINT(GMDREMAP, TRUE);	/* allow remapping by default */
	    setINT(GMDREMAPFIRST, FALSE);	/* should 1st char of a map be remapped? */
	    setINT(GMDRONLYRONLY, FALSE);	/* Set readonly-on-readonly */
	    setINT(GMDRONLYVIEW, FALSE);	/* Set view-on-readonly */
	    setINT(GMDSAMEBANGS, FALSE);	/* use same "!!" data for ^X-! */
	    setINT(GMDSMOOTH_SCROLL, FALSE);
	    setINT(GMDUNIQ_BUFS, FALSE);	/* only one buffer per file */
	    setINT(GMDWARNRENAME, TRUE);	/* warn before renaming a buffer */
	    setINT(GMDWARNREREAD, TRUE);	/* warn before rereading a buffer */
	    setINT(GMDWARNUNREAD, TRUE);	/* warn if quitting without looking at all buffers */
	    setINT(GVAL_MAPLENGTH, 1200);
	    setINT(GVAL_MINI_HILITE, VAREV);	/* reverse hilite */
	    setINT(GVAL_PRINT_HIGH, 0);
	    setINT(GVAL_PRINT_LOW, 0);
	    setINT(GVAL_REPORT, 5);	/* report changes */
	    setINT(GVAL_TIMEOUTUSERVAL, 30000);		/* how long to wait for user seq */
	    setINT(GVAL_TIMEOUTVAL, 500);	/* how long to wait for ESC seq */
	    setTXT(GVAL_EXPAND_CHARS, expand_chars);
#ifdef GMDALL_VERSIONS
	    setINT(GMDALL_VERSIONS, FALSE);
#endif
#ifdef GMDCBRD_ECHO
	    setINT(GMDCBRD_ECHO, TRUE);
#endif
#ifdef GMDCD_ON_OPEN
	    setINT(GMDCD_ON_OPEN, cd_on_open);
#endif
#ifdef GMDDIRC
	    setINT(GMDDIRC, FALSE);	/* directory-completion */
#endif
#ifdef GMDFLASH
	    setINT(GMDFLASH, FALSE);	/* beeps beep by default */
#endif
#ifdef FORCE_CONSOLE_RESURRECTED
# ifdef GMDFORCE_CONSOLE	/* deprecated (unused) */
	    setINT(GMDFORCE_CONSOLE, is_win95());
# endif
#endif
#ifdef GMDGLOB
	    setINT(GMDGLOB, TRUE);
#endif
#ifdef GMDHEAPSIZE
	    setINT(GMDHEAPSIZE, TRUE);	/* show heap usage */
#endif
#ifdef GMDHISTORY
	    setINT(GMDHISTORY, TRUE);
#endif
#ifdef GMDMOUSE
	    setINT(GMDMOUSE, FALSE);
#endif
#ifdef GMDPOPUP_CHOICES
	    setINT(GMDPOPUP_CHOICES, TRUE);
#endif
#ifdef GMDPOPUP_POSITIONS
	    setINT(GMDPOPUP_POSITIONS, TRUE);
#endif
#ifdef GMDPOPUPMENU
	    setINT(GMDPOPUPMENU, TRUE);		/* enable popup menu */
#endif
#ifdef GMDPOPUP_MSGS
	    setINT(GMDPOPUP_MSGS, -TRUE);	/* popup-msgs */
#endif
#ifdef GMDRESOLVE_LINKS
	    setINT(GMDRESOLVE_LINKS, FALSE);	/* set noresolve-links by default in case we've got NFS problems */
#endif
#ifdef GMDSWAPTITLE
	    setINT(GMDSWAPTITLE, FALSE);
#endif
#ifdef GMDUSEFILELOCK
	    setINT(GMDUSEFILELOCK, FALSE);	/* Use filelocks */
#endif
#ifdef GMDW32PIPES
	    setINT(GMDW32PIPES, is_winnt());	/* use native pipes?  */
#endif
#ifdef GMDWARNBLANKS
	    setINT(GMDWARNBLANKS, FALSE);	/* if filenames have blanks */
#endif
#ifdef GMDWORKING
	    setINT(GMDWORKING, TRUE);	/* we put up "working..." */
#endif
#ifdef GMDXTERM_FKEYS
	    setINT(GMDXTERM_FKEYS, FALSE);	/* function-key modifiers */
#endif
#ifdef GMDXTERM_MOUSE
	    setINT(GMDXTERM_MOUSE, FALSE);	/* mouse-clicking */
#endif
#ifdef GMDXTERM_TITLE
	    setINT(GMDXTERM_TITLE, FALSE);	/* xterm window-title */
#endif
#ifdef GVAL_BCOLOR
	    setINT(GVAL_BCOLOR, C_BLACK);	/* background color */
#endif
#ifdef GVAL_CCOLOR
	    setINT(GVAL_CCOLOR, ENUM_UNKNOWN);	/* cursor color */
#endif
#ifdef GVAL_CHK_ACCESS
	    setINT(GVAL_CHK_ACCESS, FL_CDIR);	/* access-check */
#endif
#ifdef GVAL_COLOR_SCHEME
	    setPAT(GVAL_COLOR_SCHEME, NULL);	/* default scheme is 0 */
#endif
#ifdef GVAL_CSUFFIXES
	    setPAT(GVAL_CSUFFIXES, DFT_CSUFFIX);
#endif
#ifdef GVAL_FCOLOR
	    setINT(GVAL_FCOLOR, C_WHITE);	/* foreground color */
#endif
#ifdef GVAL_FINDCFG
	    setTXT(GVAL_FINDCFG, "");
#endif
#ifdef GVAL_FOR_BUFFERS
	    setINT(GVAL_FOR_BUFFERS, FB_MIXED);
#endif
#ifdef GVAL_GLOB
	    setTXT(GVAL_GLOB, "!echo %s");
#endif
#ifdef GVAL_ICURSOR
	    setTXT(GVAL_ICURSOR, "0");	/* no insertion cursor */
#endif
#ifdef GVAL_KEEP_POS
	    setINT(GVAL_KEEP_POS, KPOS_VILE);
#endif
#ifdef GVAL_MCOLOR
	    setINT(GVAL_MCOLOR, VAREV);		/* show in reverse */
#endif
#ifdef GVAL_POPUP_CHOICES
	    setINT(GVAL_POPUP_CHOICES, POPUP_CHOICES_DELAYED);
#endif
#ifdef GVAL_POPUP_POSITIONS
	    setINT(GVAL_POPUP_POSITIONS, POPUP_POSITIONS_NOTDOT);	/* default */
#endif
#ifdef GVAL_READER_POLICY
	    setINT(GVAL_READER_POLICY, RP_BOTH);
#endif
#ifdef GVAL_REDIRECT_KEYS
	    setTXT(GVAL_REDIRECT_KEYS,
		   "F5::S,F10::S,F11::S,F7::F,F5:C:,F9::Y");
#endif
#ifdef GVAL_RECENT_FILES
	    setINT(GVAL_RECENT_FILES, 0);
#endif
#ifdef GVAL_RECENT_FLDRS
	    setINT(GVAL_RECENT_FLDRS, 0);
#endif
#ifdef GVAL_SCROLLPAUSE
	    setINT(GVAL_SCROLLPAUSE, 0);
#endif
#ifdef GVAL_VIDEO
	    setINT(GVAL_VIDEO, 0);
#endif
#ifdef GVAL_VTFLASH
	    setINT(GVAL_VTFLASH, VTFLASH_OFF);	/* hardwired flash off */
#endif
	default:
	    setIntValue(d, 0);
	    break;
	}
	break;
    case BUF_MODE:
	switch (v_which) {
	    setINT(MDAIND, FALSE);	/* auto-indent */
	    setINT(MDASAVE, FALSE);	/* auto-save */
	    setINT(MDAUTOWRITE, FALSE);		/* auto-write */
	    setINT(MDBACKLIMIT, TRUE);	/* limit backspacing to insert point */
	    setINT(MDCINDENT, FALSE);	/* C-style indent */
	    setINT(MDDOS, system_crlf);
	    setINT(MDIGNCASE, FALSE);	/* exact matches */
	    setINT(MDINS_RECTS, FALSE);		/* insert-mode for rectangles */
	    setINT(MDLOADING, FALSE);	/* asynchronously loading a file */
	    setINT(MDMAGIC, TRUE);	/* magic searches */
	    setINT(MDMETAINSBIND, TRUE);	/* honor meta-bindings when in insert mode */
	    setINT(MDNEWLINE, TRUE);	/* trailing-newline */
	    setINT(MDREADONLY, FALSE);	/* readonly */
	    setINT(MDSHOWMAT, FALSE);	/* show-match */
	    setINT(MDSHOWMODE, TRUE);	/* show-mode */
	    setINT(MDSPACESENT, TRUE);	/* add two spaces after each sentence */
	    setINT(MDTABINSERT, TRUE);	/* allow tab insertion */
	    setINT(MDTAGSRELTIV, FALSE);	/* path relative tag lookups */
	    setINT(MDTAGWORD, FALSE);	/* tag entire word */
	    setINT(MDTERSE, FALSE);	/* terse messaging */
	    setINT(MDUNDOABLE, TRUE);	/* undo stack active */
	    setINT(MDUNDO_DOS_TRIM, FALSE);	/* undo dos trimming */
	    setINT(MDVIEW, FALSE);	/* view-only */
	    setINT(MDWRAPSCAN, TRUE);	/* scan wrap */
	    setINT(MDWRAPWORDS, FALSE);		/* wrap */
	    setINT(MDYANKMOTION, TRUE);		/* yank-motion */
	    setINT(VAL_ASAVECNT, 256);	/* autosave count */
	    setINT(VAL_FILL, default_fill());	/* column for paragraph reformat */
	    setINT(VAL_PERCENT_CRLF, 50);	/* threshold for crlf conversion */
	    setINT(VAL_RECORD_SEP, RS_SYS_DEFAULT);
	    setINT(VAL_SHOW_FORMAT, SF_FOREIGN);
	    setINT(VAL_SWIDTH, 8);	/* shiftwidth */
	    setINT(VAL_TAB, 8);	/* tab stop */
	    setINT(VAL_TAGLEN, 0);	/* significant tag length */
	    setINT(VAL_UNDOLIM, 10);	/* undo limit */
	    setINT(VAL_WRAPMARGIN, 0);	/* wrap-margin */
	    setPAT(VAL_CMT_PREFIX, DFT_CMT_PREFIX);
	    setPAT(VAL_COMMENTS, DFT_COMMENTS);
	    setPAT(VAL_FENCE_BEGIN, DFT_FENCE_BEGIN);
	    setPAT(VAL_FENCE_ELIF, DFT_FENCE_ELIF);
	    setPAT(VAL_FENCE_ELSE, DFT_FENCE_ELSE);
	    setPAT(VAL_FENCE_END, DFT_FENCE_END);
	    setPAT(VAL_FENCE_FI, DFT_FENCE_FI);
	    setPAT(VAL_FENCE_IF, DFT_FENCE_IF);
	    setPAT(VAL_PARAGRAPHS, DFT_PARAGRAPHS);
	    setPAT(VAL_SECTIONS, DFT_SECTIONS);
	    setPAT(VAL_SENTENCES, DFT_SENTENCES);
	    setTXT(VAL_CINDENT_CHARS, DFT_CINDENT_CHARS);	/* C-style indent flags */
	    setTXT(VAL_FENCES, "{}()[]");	/* fences */
	    setTXT(VAL_TAGS, "tags");	/* tags filename */
#if OPT_CURTOKENS
	    setINT(VAL_CURSOR_TOKENS, CT_REGEX);
	    setPAT(VAL_BUFNAME_EXPR, DFT_BUFNAME_EXPR);
	    setPAT(VAL_IDENTIFIER_EXPR, DFT_IDENTIFIER_EXPR);
	    setPAT(VAL_PATHNAME_EXPR, DFT_FILENAME_EXPR "\\+");
#endif
#ifdef MDCHK_MODTIME
	    setINT(MDCHK_MODTIME, FALSE);	/* modtime-check */
#endif
#ifdef MDCMOD
	    setINT(MDCMOD, FALSE);	/* C mode */
#endif
#ifdef MDCRYPT
	    setINT(MDCRYPT, FALSE);	/* crypt */
#endif
#ifdef MDFILTERMSGS
	    setINT(MDFILTERMSGS, FALSE);	/* syntax-filter messages */
#endif
#ifdef MDHILITE
	    setINT(MDHILITE, TRUE);	/* syntax-highlighting */
#endif
#ifdef MDHILITEOVERLAP
	    setINT(MDHILITEOVERLAP, TRUE);	/* overlap visual-matches */
#endif
#ifdef MDLOCKED
	    setINT(MDLOCKED, FALSE);	/* LOCKED */
#endif
#ifdef MDMODELINE
	    setINT(MDMODELINE, FALSE);
#endif
#ifdef MDREUSE_POS
	    setINT(MDREUSE_POS, FALSE);		/* reuse position */
#endif
#ifdef MDTAGIGNORECASE
	    setINT(MDTAGIGNORECASE, FALSE);	/* ignore case in tags */
#endif
#ifdef MDUPBUFF
	    setINT(MDUPBUFF, TRUE);	/* animated */
#endif
#ifdef VAL_AUTOCOLOR
	    setINT(VAL_AUTOCOLOR, 0);	/* auto syntax coloring timeout */
#endif
#ifdef VAL_BACKUPSTYLE
	    setTXT(VAL_BACKUPSTYLE, DFT_BACKUPSTYLE);
#endif
#ifdef VAL_BYTEORDER_MARK
	    setINT(VAL_BYTEORDER_MARK, ENUM_UNKNOWN);	/* byteorder-mark NONE */
#endif
#ifdef VAL_C_SWIDTH
	    setINT(VAL_C_SWIDTH, 8);	/* C file shiftwidth */
#endif
#ifdef VAL_C_TAB
	    setINT(VAL_C_TAB, 8);	/* C file tab stop */
#endif
#ifdef VAL_FENCE_LIMIT
	    setINT(VAL_FENCE_LIMIT, 10);	/* fences iteration timeout */
#endif
#ifdef VAL_FILE_ENCODING
	    setINT(VAL_FILE_ENCODING, enc_DEFAULT);
#endif
#ifdef VAL_FILTERNAME
	    setTXT(VAL_FILTERNAME, "");
#endif
#ifdef VAL_HILITEMATCH
	    setINT(VAL_HILITEMATCH, 0);		/* no hilite */
#endif
#ifdef VAL_LOCKER
	    setTXT(VAL_LOCKER, "");	/* Name locker */
#endif
#ifdef VAL_MODELINES
	    setINT(VAL_MODELINES, 5);
#endif
#ifdef VAL_PERCENT_AUTOCOLOR
	    setINT(VAL_PERCENT_AUTOCOLOR, 50);
#endif
#ifdef VAL_PERCENT_UTF8
	    setINT(VAL_PERCENT_UTF8, 90);
#endif
#ifdef VAL_RECORD_ATTRS
	    setINT(VAL_RECORD_ATTRS, 0);
#endif
#ifdef VAL_RECORD_FORMAT
	    setINT(VAL_RECORD_FORMAT, FAB$C_UDF);
#endif
#ifdef VAL_RECORD_LENGTH
	    setINT(VAL_RECORD_LENGTH, 0);
#endif
#ifdef VAL_STRIPSUFFIX
	    setPAT(VAL_STRIPSUFFIX, DFT_STRIPSUFFIX);
#endif
	default:
	    setIntValue(d, 0);
	    break;
	}
#if OPT_CURTOKENS
	switch (v_which) {
	case VAL_BUFNAME_EXPR:
	case VAL_PATHNAME_EXPR:
	    set_buf_fname_expr((d == &global_b_values.bv[v_which])
			       ? curbp
			       : NULL);
	    break;
	}
#endif
	break;
    case WIN_MODE:
	switch (v_which) {
	    setINT(WMDLIST, FALSE);	/* list-mode */
	    setINT(WMDHORSCROLL, TRUE);		/* horizontal scrolling */
	    setINT(WMDNUMBER, FALSE);	/* number */
	    setINT(WMDNONPRINTOCTAL, FALSE);	/* unprintable-as-octal */
	    setINT(WVAL_SIDEWAYS, 0);	/* list-mode */
#ifdef WMDLINEWRAP
	    setINT(WMDLINEBREAK, FALSE);	/* linebreak */
	    setINT(WMDLINEWRAP, FALSE);		/* linewrap */
#endif
#ifdef WMDRULER
	    setINT(WMDRULER, FALSE);	/* ruler */
#endif
#ifdef WMDSHOWCHAR
	    setINT(WMDSHOWCHAR, FALSE);		/* showchar */
#endif
#ifdef WMDSHOWVARS
	    setINT(WMDSHOWVARS, FALSE);		/* showvariables */
#endif
#ifdef WMDTERSELECT
	    setINT(WMDTERSELECT, TRUE);		/* terse selections */
#endif
#ifdef WMDUNICODE_AS_HEX
	    setINT(WMDUNICODE_AS_HEX, FALSE);	/* unicode-as-hex */
#endif
	default:
	    setIntValue(d, 0);
	    break;
	}
	break;
    default:
	setIntValue(d, 0);
	break;
    }
}

#define DFT_HELP_FILE "vile.hlp"

#define DFT_MENU_FILE ".vilemenu"

#ifdef VILE_LIBDIR_PATH
#define DFT_LIBDIR_PATH VILE_LIBDIR_PATH
#else
#define DFT_LIBDIR_PATH ""
#endif

#define DFT_MLFORMAT \
"%-%i%- %b %m:: :%f:is : :%=%F: : :%l:(:,:%c::) :%p::% :%C:char ::%S%-%-%|"

#define DFT_POSFORMAT \
"Line %{$curline}d of %{$blines}d,\
 Col %{$curcol}d of %{$lcols}d,\
 Char %{$curchar}d of %{$bchars}d\
 (%P%%) char is 0x%{$char}x or 0%{$char}o"

static const char *
default_help_file(void)
{
    const char *result;
    if ((result = vile_getenv("VILE_HELP_FILE")) == NULL)
	result = DFT_HELP_FILE;
    return result;
}

static const char *
default_libdir_path(void)
{
    const char *result;
    if ((result = vile_getenv("VILE_LIBDIR_PATH")) == NULL)
	result = DFT_LIBDIR_PATH;
    return result;
}

#if OPT_MENUS
static char *
default_menu_file(void)
{
    static char default_menu[] = DFT_MENU_FILE;
    char temp[NSTRING];
    char *menurc = vile_getenv("VILE_MENU");

    if (isEmpty(menurc)) {
	sprintf(temp, "%.*s_MENU", (int) (sizeof(temp) - 6), prognam);
	mkupper(temp);
	menurc = vile_getenv(temp);
	if (isEmpty(menurc)) {
	    menurc = default_menu;
	}
    }

    return menurc;
}
#endif

static const char *
default_startup_file(void)
{
    const char *result;
    if ((result = vile_getenv("VILE_STARTUP_FILE")) == NULL)
	result = DFT_STARTUP_FILE;
    return result;
}

static char *
default_startup_path(void)
{
    char *result = NULL;
    char *s;

    if ((s = vile_getenv("VILE_STARTUP_PATH")) != NULL) {
	append_to_path_list(&result, s);
    } else {
#if SYS_MSDOS || SYS_OS2 || SYS_WINNT
	append_to_path_list(&result, "/sys/public");
	append_to_path_list(&result, "/usr/bin");
	append_to_path_list(&result, "/bin");
	append_to_path_list(&result, "/");
#elif SYS_VMS
	append_to_path_list(&result, "sys$login");
	append_to_path_list(&result, "sys$sysdevice:[vmstools]");
	append_to_path_list(&result, "sys$library");
#elif defined(VILE_STARTUP_PATH)
	append_to_path_list(&result, VILE_STARTUP_PATH);
#else
	append_to_path_list(&result, "/usr/local/lib");
	append_to_path_list(&result, "/usr/local");
	append_to_path_list(&result, "/usr/lib");
#endif /* SYS_MSDOS... */
    }
#ifdef HELP_LOC
    append_to_path_list(&result, HELP_LOC);
#endif
    return result;
}

#if OPT_MULTIBYTE
static void
pre_init_ctype(void)
{
    tb_scopy(&latin1_expr, DFT_LATIN1_EXPR);
}
#endif

#if OPT_EVAL
/*
 * Returns a string representing the default/initial value of the given
 * state-variable.
 */
char *
init_state_value(int which)
{
    const char *value = NULL;
    char *result = NULL;

    switch (which) {
#if OPT_FINDERR
    case VAR_FILENAME_EXPR:
	value = DFT_FILENAME_EXPR;
	break;
#endif
    case VAR_FONT:
	value = default_font;
	break;
    case VAR_HELPFILE:
	value = default_help_file();
	break;
#if OPT_MULTIBYTE
    case VAR_LATIN1_EXPR:
	value = DFT_LATIN1_EXPR;
	break;
#endif
    case VAR_LIBDIR_PATH:
	value = default_libdir_path();
	break;
#if OPT_SHELL
    case VAR_LOOK_IN_CWD:
	/* FALLTHRU */
    case VAR_LOOK_IN_HOME:
	value = "true";
	break;
#endif
#if OPT_MENUS
    case VAR_MENU_FILE:
	value = default_menu_file();
	break;
#endif
#if OPT_MLFORMAT
    case VAR_MLFORMAT:
	value = DFT_MLFORMAT;
	break;
#endif
#if OPT_POSFORMAT
    case VAR_POSFORMAT:
	value = DFT_POSFORMAT;
	break;
#endif
    case VAR_PROMPT:
	value = ": ";
	break;
    case VAR_REPLACE:
	value = "";
	break;
    case VAR_STARTUP_FILE:
	value = default_startup_file();
	break;
    case VAR_STARTUP_PATH:
	result = default_startup_path();
	break;
#if !SMALLER
    case VAR_EMPTY_LINES:
	value = "1";
	break;
#endif
    }
    return (value != NULL) ? strmalloc(value) : result;
}
#endif /* OPT_EVAL */

#if OPT_REBIND
/*
 * Cache the length of kbindtbl[].
 */
static size_t
len_kbindtbl(void)
{
    static size_t result = 0;
    if (result == 0)
	while (kbindtbl[result].k_cmd != NULL)
	    result++;
    return result + 1;
}

/*
 * At startup, construct copies of kbindtbl[] for the alternate bindings, so
 * we can have separate bindings of special keys from the default bindings.
 */
static void
copy_kbindtbl(BINDINGS * dst)
{
    KBIND *result;
    size_t len = len_kbindtbl();

    if (dst->kb_special == kbindtbl) {
	if ((result = typecallocn(KBIND, len)) == NULL) {
	    (void) no_memory("copy_kbindtbl");
	    return;
	}
	dst->kb_special = dst->kb_extra = result;
    } else {
	result = dst->kb_special;
    }
    while (len-- != 0) {
	result[len] = kbindtbl[len];
    }
}
#endif

#if OPT_FILTER && defined(WIN32) && !SYS_MINGW
extern void flt_array(void);
#endif

/*
 * Initialize compiled-in state variable values.
 */
static void
state_val_init(void)
{
#if OPT_EVAL
    int i;

    /*
     * Note that not all state variables can be initialized explicitly, many
     * are either read-only, or have too many side-effects to set blindly.
     */
    for (i = 0; i < Num_StateVars; i++) {
	char *s;
	if ((s = init_state_value(i)) != NULL) {
	    set_state_variable(statevars[i], s);
	    free(s);
	}
    }
#else
    helpfile = default_help_file();
    startup_file = strmalloc(default_startup_file());
    startup_path = default_startup_path();
    libdir_path = default_libdir_path();
#if OPT_MENUS
    menu_file = default_menu_file();
#endif
#if OPT_MLFORMAT
    modeline_format = strmalloc(DFT_MLFORMAT);
#endif
#if OPT_MULTIBYTE
    tb_scopy(&latin1_expr, DFT_LATIN1_EXPR);
#endif
#if OPT_POSFORMAT
    position_format = strmalloc(DFT_POSFORMAT);
#endif
#if OPT_FINDERR
    tb_scopy(&filename_expr, DFT_FILENAME_EXPR);
#endif
#endif
}

static void
global_val_init(void)
{
    int i;

    TRACE((T_CALLED "global_val_init()\n"));
#if OPT_FILTER && defined(WIN32) && !SYS_MINGW
    flt_array();
#endif
    /* set up so the global value pointers point at the global
       values.  we never actually use the global pointers
       directly, but when buffers get a copy of the
       global_b_values structure, the pointers will still point
       back at the global values, which is what we want */
    for (i = 0; i <= NUM_G_VALUES; i++) {
	make_local_val(global_g_values.gv, i);
	init_mode_value(&global_g_values.gv[i], UNI_MODE, i);
    }

    for (i = 0; i <= NUM_B_VALUES; i++) {
	make_local_val(global_b_values.bv, i);
	init_mode_value(&global_b_values.bv[i], BUF_MODE, i);
    }

    for (i = 0; i <= NUM_W_VALUES; i++) {
	make_local_val(global_w_values.wv, i);
	init_mode_value(&global_w_values.wv[i], WIN_MODE, i);
    }
    state_val_init();

#if OPT_MAJORMODE
    /*
     * Built-in majormodes (see 'modetbl')
     */
    alloc_mode("c", TRUE);
    alloc_mode("vile", TRUE);

    set_submode_val("c", VAL_SWIDTH, 8);	/* C file shiftwidth */
    set_submode_val("c", VAL_TAB, 8);	/* C file tab stop */
    set_submode_val("c", MDCINDENT, TRUE);	/* C-style indent */
    set_submode_txt("c", VAL_CINDENT_CHARS, DFT_CINDENT_CHARS);		/* C-style indent flags */

    set_majormode_rexp("c", MVAL_MODE_SUFFIXES, DFT_CSUFFIX);
#ifdef MVAL_MODE_WILDCARD
    set_majormode_rexp("c", MVAL_MODE_WILDCARD, DFT_WILDCARD);
#endif
#endif /* OPT_MAJORMODE */

    /*
     * Initialize the "normal" bindings for insert and command mode to
     * the motion-commands that are safe if insert-exec mode is enabled.
     */
#if OPT_REBIND
    copy_kbindtbl(&sel_bindings);
    copy_kbindtbl(&ins_bindings);
    copy_kbindtbl(&cmd_bindings);
#endif
    for (i = 0; i < N_chars; i++) {
	const CMDFUNC *cfp;

	sel_bindings.kb_normal[i] = dft_bindings.kb_normal[i];
	ins_bindings.kb_normal[i] = NULL;
	cmd_bindings.kb_normal[i] = dft_bindings.kb_normal[i];
	if (i < 32
	    && (cfp = dft_bindings.kb_normal[i]) != NULL
	    && (cfp->c_flags & (GOAL | MOTION)) != 0) {
	    ins_bindings.kb_normal[i] =
		cmd_bindings.kb_normal[i] = cfp;
	}
    }
    returnVoid();
}

#if SYS_UNIX || SYS_MSDOS || SYS_OS2 || SYS_WINNT || SYS_VMS

/* ARGSUSED */
SIGT
catchintr(int ACTUAL_SIG_ARGS)
{
    am_interrupted = TRUE;
#if SYS_MSDOS || SYS_OS2 || SYS_WINNT
    sgarbf = TRUE;		/* there's probably a ^C on the screen. */
#endif
    setup_handler(SIGINT, catchintr);
    if (im_waiting(-1))
	longjmp(read_jmp_buf, signo);
    SIGRET;
}
#endif

#ifndef interrupted		/* i.e. unless it's a macro */
int
interrupted(void)
{
#if SYS_MSDOS && CC_DJGPP

    if (_go32_was_ctrl_break_hit() != 0) {
	while (keystroke_avail())
	    (void) keystroke();
	return TRUE;
    }
    if (was_ctrl_c_hit() != 0) {
	while (keystroke_avail())
	    (void) keystroke();
	return TRUE;
    }

    if (am_interrupted)
	return TRUE;
    return FALSE;
#endif
}
#endif

void
not_interrupted(void)
{
    am_interrupted = FALSE;
#if SYS_MSDOS
# if CC_DJGPP
    (void) _go32_was_ctrl_break_hit();	/* flush any pending kbd ctrl-breaks */
    (void) was_ctrl_c_hit();	/* flush any pending kbd ctrl-breaks */
# endif
#endif
}

#if SYS_MSDOS
# if CC_WATCOM
int
wat_crit_handler(unsigned deverror, unsigned errcode, unsigned *devhdr)
{
    _hardresume((int) _HARDERR_FAIL);
    return (int) _HARDERR_FAIL;
}

#else
void
dos_crit_handler(void)
{
#  if ! CC_DJGPP
    _hardresume(_HARDERR_FAIL);
#  endif
}

#  endif
#endif

#define DATA(sig, func) { sig, func, SIG_DFL }

static struct {
    int signo;
    SIGNAL_HANDLER handler;
    SIGNAL_HANDLER original;
} my_sighandlers[] = {

    DATA(0, NULL),		/* just for VMS */
#if SYS_UNIX || SYS_MSDOS || SYS_OS2 || SYS_WINNT
#if defined(SIGINT)
	DATA(SIGINT, catchintr),
#endif
#endif
#if SYS_UNIX
#if defined(SIGHUP)
	DATA(SIGHUP, imdying),
#endif
#if defined(SIGBUS)
	DATA(SIGBUS, imdying),
#endif
#if defined(SIGILL)
	DATA(SIGILL, imdying),
#endif
#if defined(SIGFPE)
	DATA(SIGFPE, imdying),
#endif
#if defined(SIGSYS)
	DATA(SIGSYS, imdying),
#endif
	DATA(SIGSEGV, imdying),
	DATA(SIGTERM, imdying),
#ifdef SIGQUIT
#ifdef VILE_DEBUG
	DATA(SIGQUIT, imdying),
#else
	DATA(SIGQUIT, SIG_IGN),
#endif
#endif
#ifdef SIGPIPE
	DATA(SIGPIPE, SIG_IGN),
#endif
#if defined(SIGWINCH) && ! DISP_X11
	DATA(SIGWINCH, sizesignal),
#endif
#endif /* SYS_UNIX */
};

#undef DATA

/*
 * Setup signal handlers, if 'enabled'.  Otherwise reset to default all of the
 * ones that pointed to imdying().  The latter is used only in ExitProgram() to
 * ensure that abort() can really generate a core dump.
 */
static void
setup_sighandlers(int enabled)
{
    static int save_original = TRUE;
    unsigned n;

    for (n = 1; n < TABLESIZE(my_sighandlers); ++n) {
	if (enabled) {
	    SIGNAL_HANDLER old = setup_handler(my_sighandlers[n].signo,
					       my_sighandlers[n].handler);
	    if (save_original)
		my_sighandlers[n].original = old;
	} else if (my_sighandlers[n].handler == imdying) {
	    setup_handler(my_sighandlers[n].signo, SIG_DFL);
	}
    }

    if (enabled)
	save_original = FALSE;

#if SYS_MSDOS
# if CC_DJGPP
    _go32_want_ctrl_break(TRUE);
    setcbrk(FALSE);
    want_ctrl_c(TRUE);
    hard_error_catch_setup();
# else
#  if CC_WATCOM
    {
	/* clean up Warning from Watcom C */
	void *ptrfunc = wat_crit_handler;
	_harderr(ptrfunc);
    }
#  else	/* CC_TURBO */
    _harderr(dos_crit_handler);
#  endif
# endif
#endif
}

SIGNAL_HANDLER
original_sighandler(int sig)
{
    SIGNAL_HANDLER result = SIG_DFL;
    unsigned n;

    for (n = 1; n < TABLESIZE(my_sighandlers); ++n) {
	if (sig == my_sighandlers[n].signo) {
	    result = my_sighandlers[n].original;
	    break;
	}
    }
    return result;
}

static void
siguninit(void)
{
#if SYS_MSDOS
# if CC_DJGPP
    _go32_want_ctrl_break(FALSE);
    want_ctrl_c(FALSE);
    hard_error_teardown();
    setcbrk(TRUE);
# endif
#endif
}

/* do number processing if needed */
static void
do_num_proc(int *cp, int *fp, int *np)
{
    int c, f, n;
    int mflag;
    int oldn;

    c = *cp;

    if (isCntrl(c) || !is8Bits(c))
	return;

    f = *fp;
    n = *np;
    oldn = need_a_count(f, n, 1);
    n = 1;

    if (isDigit(c) && c != '0') {
	n = 0;			/* start with a zero default */
	f = TRUE;		/* there is a # arg */
	mflag = 1;		/* current minus flag */
	while ((isDigit(c) && is8Bits(c)) || (c == '-')) {
	    if (c == '-') {
		/* already hit a minus or digit? */
		if ((mflag == -1) || (n != 0))
		    break;
		mflag = -1;
	    } else {
		n = n * 10 + (c - '0');
	    }
	    if ((n == 0) && (mflag == -1))	/* lonely - */
		mlwrite("arg:");
	    else
		mlwrite("arg: %d", n * mflag);

	    c = kbd_seq();	/* get the next key */
	}
	n = n * mflag;		/* figure in the sign */
    }
    *cp = c;
    *fp = f;
    *np = n * oldn;
}

/* do emacs ^U-style repeat argument processing -- vile binds this to 'K' */
static void
do_rept_arg_proc(int *cp, int *fp, int *np)
{
    int c, f, n;
    int mflag;
    int oldn;
    c = *cp;

    if (c != reptc)
	return;

    f = *fp;
    n = *np;

    oldn = need_a_count(f, n, 1);

    n = 4;			/* start with a 4 */
    f = TRUE;			/* there is a # arg */
    mflag = 0;			/* that can be discarded. */
    mlwrite("arg: %d", n);
    for (;;) {
	c = kbd_seq();
	if (!(isDigit(c) || (c == reptc) || (c == '-')))
	    break;
	if (c == reptc)
	    n = n * 4;

	/*
	 * If dash, and start of argument string, set arg.
	 * to -1.  Otherwise, insert it.
	 */
	else if (c == '-') {
	    if (mflag)
		break;
	    n = 0;
	    mflag = -1;
	}
	/*
	 * If first digit entered, replace previous argument
	 * with digit and set sign.  Otherwise, append to arg.
	 */
	else {
	    if (!mflag) {
		n = 0;
		mflag = 1;
	    }
	    n = 10 * n + c - '0';
	}
	mlwrite("arg: %d", (mflag >= 0) ? n : (n ? -n : -1));
    }
    /*
     * Make arguments preceded by a minus sign negative and change
     * the special argument "^U -" to an effective "^U -1".
     */
    if (mflag == -1) {
	if (n == 0)
	    n++;
	n = -n;
    }

    *cp = c;
    *fp = f;
    *np = n * oldn;
}

/* handle all repeat counts */
void
do_repeats(int *cp, int *fp, int *np)
{
    do_num_proc(cp, fp, np);
    do_rept_arg_proc(cp, fp, np);
    if (dotcmdactive == PLAY) {
	if (dotcmdarg)		/* then repeats are done by dotcmdcnt */
	    *np = 1;
    } else {
	/* then we want to cancel any dotcmdcnt repeats */
	if (*fp)
	    dotcmdarg = FALSE;
    }
}

/* the vi ZZ command -- write all, then quit */
int
zzquit(int f, int n)
{
    int thiscmd;
    int cnt;
    BUFFER *bp;

    thiscmd = lastcmd;
    cnt = any_changed_buf(&bp);
    if (cnt) {
	if (cnt > 1) {
	    mlprompt("Will write %d buffers.  %s ", cnt,
		     clexec ? s_NULL : "Repeat command to continue.");
	} else {
	    mlprompt("Will write buffer \"%s\".  %s ",
		     bp->b_bname,
		     clexec ? s_NULL : "Repeat command to continue.");
	}
	if (!clexec && !isnamedcmd) {
	    if (thiscmd != kbd_seq())
		return FALSE;
	}

	if (writeall(f, n, FALSE, TRUE, FALSE, FALSE) != TRUE) {
	    return FALSE;
	}

    } else if (!clexec && !isnamedcmd) {
	/* consume the next char. anyway */
	if (thiscmd != kbd_seq())
	    return FALSE;
    }
    return quit(f, n);
}

/*
 * attempt to write all changed buffers, and quit
 */
int
quickexit(int f, int n)
{
    int status;
    if ((status = writeall(f, n, FALSE, TRUE, FALSE, FALSE)) == TRUE)
	status = quithard(f, n);	/* conditionally quit */
    return status;
}

/* Force quit by giving argument */
/* ARGSUSED */
int
quithard(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return quit(TRUE, 1);
}

/*
 * Quit command. If an argument, always quit. Otherwise confirm if a buffer
 * has been changed and not written out.
 */
/* ARGSUSED */
int
quit(int f, int n GCC_UNUSED)
{
    int cnt;
    BUFFER *bp;
    const char *sadj, *sverb;

    run_a_hook(&exithook);

    if (f == FALSE) {
	cnt = any_changed_buf(&bp);
	sadj = "modified";
	sverb = "Write";
	if (cnt == 0 && global_g_val(GMDWARNUNREAD)) {
	    cnt = any_unread_buf(&bp);
	    sadj = "unread";
	    sverb = "Look at";
	}
	if (cnt != 0) {
	    if (cnt == 1)
		mlforce(
			   "Buffer \"%s\" is %s.  %s it, or use :q!",
			   bp->b_bname, sadj, sverb);
	    else
		mlforce(
			   "There are %d %s buffers.  %s them, or use :q!",
			   cnt, sadj, sverb);
	    return FALSE;
	}
    }
#if OPT_LCKFILES
    /* Release all placed locks */
    if (global_g_val(GMDUSEFILELOCK)) {
	for_each_buffer(bp) {
	    if (bp->b_active) {
		if (!b_val(curbp, MDLOCKED) &&
		    !b_val(curbp, MDVIEW))
		    release_lock(bp->b_fname);
	    }
	}
    }
#endif
    siguninit();
#if OPT_WORKING
    setup_handler(SIGALRM, SIG_IGN);
#endif
    tidy_exit(GOODEXIT);
    /* NOTREACHED */
    return FALSE;
}

/* ARGSUSED */
int
writequit(int f GCC_UNUSED, int n)
{
    int s;
    s = filesave(FALSE, n);
    if (s != TRUE)
	return s;
    return quit(FALSE, n);
}

/*
 * Abort.
 * Beep the beeper. Kill off any keyboard macro, etc., that is in progress.
 * Sometimes called as a routine, to do general aborting of stuff.
 */
/* ARGSUSED */
int
esc_func(int f GCC_UNUSED, int n GCC_UNUSED)
{
    dotcmdactive = 0;
    regionshape = rgn_EXACT;
    doingopcmd = FALSE;
    do_sweep(FALSE);
    sweephack = FALSE;
    opcmd = 0;
    mlwarn("[Aborted]");
    return ABORT;
}

/* tell the user that this command is illegal while we are in
   VIEW (read-only) mode				*/

int
rdonly(void)
{
    mlwarn("[No changes are allowed while in \"view\" mode]");
    return FALSE;
}

/* ARGSUSED */
int
unimpl(int f GCC_UNUSED, int n GCC_UNUSED)
{
    mlwarn("[Sorry, that vi command is unimplemented in vile ]");
    return FALSE;
}

int
opercopy(int f, int n)
{
    return unimpl(f, n);
}

int
opermove(int f, int n)
{
    return unimpl(f, n);
}

int
opertransf(int f, int n)
{
    return unimpl(f, n);
}

int
operglobals(int f, int n)
{
    return unimpl(f, n);
}

int
opervglobals(int f, int n)
{
    return unimpl(f, n);
}

int
vl_source(int f, int n)
{
    return unimpl(f, n);
}

int
visual(int f, int n)
{
    return unimpl(f, n);
}

int
ex(int f, int n)
{
    return unimpl(f, n);
}

/* ARGSUSED */
/* user function that only returns true */
int
nullproc(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return TRUE;
}

/* dummy functions for binding to key sequence prefixes */
/* ARGSUSED */
int
cntl_a_func(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return TRUE;
}

/* ARGSUSED */
int
cntl_x_func(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return TRUE;
}

/* ARGSUSED */
int
poundc_func(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return TRUE;
}

/* ARGSUSED */
int
reptc_func(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return TRUE;
}

/*----------------------------------------------------------------------------*/

#if OPT_HEAPSIZE

/* track overall heap usage */

#undef	realloc
#undef	malloc
#undef	free

long currentheap;		/* current heap usage */

typedef struct {
    size_t length;
    unsigned magic;
} HEAPSIZE;

#define MAGIC_HEAP 0x12345678

/* display the amount of HEAP currently malloc'ed */
static void
display_heap_usage(void)
{
    beginDisplay();
    if (global_g_val(GMDHEAPSIZE)) {
	char membuf[20];
	int saverow = ttrow;
	int savecol = ttcol;

	if (saverow >= 0 && saverow < term.rows
	    && savecol >= 0 && savecol < term.cols) {
	    (void) lsprintf(membuf, "[%ld]", currentheap);
	    kbd_overlay(membuf);
	    kbd_flush();
	    movecursor(saverow, savecol);
	}
    }
    endofDisplay();
}

static char *
old_ramsize(char *mp)
{
    HEAPSIZE *p = (HEAPSIZE *) (mp - sizeof(HEAPSIZE));

    if (p->magic == MAGIC_HEAP) {
	mp = (char *) p;
	currentheap -= p->length;
    }
    return mp;
}

static char *
new_ramsize(char *mp, unsigned nbytes)
{
    HEAPSIZE *p = (HEAPSIZE *) mp;
    if (p != 0) {
	p->length = nbytes;
	p->magic = MAGIC_HEAP;
	currentheap += nbytes;
	mp = (char *) ((long) p + sizeof(HEAPSIZE));
    }
    return mp;
}

/* track reallocs */
char *
track_realloc(char *p, unsigned size)
{
    if (!p)
	return track_malloc(size);

    size += sizeof(HEAPSIZE);
    p = new_ramsize(realloc(old_ramsize(p), size), size);
    display_heap_usage();
    return p;
}

/* track mallocs */
char *
track_malloc(unsigned size)
{
    char *p;

    size += sizeof(HEAPSIZE);
    if ((p = typeallocn(char, size)) != 0) {
	(void) memset(p, 0, size);	/* so we can use for calloc */
	p = new_ramsize(p, size);
	display_heap_usage();
    }

    return p;
}

/* track free'd memory */
void
track_free(char *p)
{
    if (!p)
	return;
    free(old_ramsize(p));
    display_heap_usage();
}
#endif /* OPT_HEAPSIZE */

#if SYS_MSDOS
int
showmemory(int f, int n)
{
#if CC_TURBO
    extern long coreleft(void);
    mlforce("Memory left: %ld bytes", coreleft());
#elif CC_WATCOM
    mlforce("Watcom C doesn't provide a very useful 'memory-left' call.");
#elif CC_DJGPP
    mlforce("Memory left: %ld Kb virtual, %ld Kb physical",
	    _go32_dpmi_remaining_virtual_memory() / 1024,
	    _go32_dpmi_remaining_physical_memory() / 1024);
#endif
    return TRUE;
}
#endif /* SYS_MSDOS */

char *
strncpy0(char *dest, const char *src, size_t destlen)
{
    if (dest != NULL && src != NULL && destlen != 0) {
	(void) strncpy(dest, src, destlen);
	dest[destlen - 1] = EOS;
    }
    return dest;
}

/*
 * This is probably more efficient for copying short strings into a large
 * fixed-size buffer, because strncpy always zero-pads the destination to
 * the given length.
 */
char *
vl_strncpy(char *dest, const char *src, size_t destlen)
{
    size_t n;

    if ((src != NULL) && (destlen != 0)) {
	for (n = 0; n < destlen; ++n) {
	    if ((dest[n] = src[n]) == EOS)
		break;
	}
	dest[destlen - 1] = EOS;
    }
    return dest;
}

char *
vl_strncat(char *dest, const char *src, size_t destlen)
{
    size_t oldlen = strlen(dest);

    if ((destlen != 0) && (oldlen < destlen)) {
	vl_strncpy(dest + oldlen, src, destlen - oldlen);
    }
    return dest;
}

/*
 * Wrapper for environment variables that tell vile where to find its
 * components.
 */
char *
vile_getenv(const char *name)
{
    char *result = getenv(name);
#if defined(_WIN32) || defined(_WIN64)
    if (result == 0) {
	static HKEY rootkeys[] =
	{HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE};

	int j;
	HKEY hkey;
	char buffer[256];

	for (j = 0; j < (int) TABLESIZE(rootkeys); ++j) {
	    if (RegOpenKeyEx(rootkeys[j],
			     VILE_SUBKEY W32_STRING("\\Environment"),
			     0,
			     KEY_READ,
			     &hkey) == ERROR_SUCCESS) {
		if (w32_get_reg_sz(hkey, name, buffer, sizeof(buffer)) == ERROR_SUCCESS) {

		    result = strmalloc(buffer);
		    (void) RegCloseKey(hkey);
		    break;
		}

		(void) RegCloseKey(hkey);
	    }
	}
    }
#endif
    TPRINTF(("vile getenv %s=%s\n", name, NonNull(result)));
    return result;
}

/*
 * Wrapper for all generic/system environment variables, so that we can trace.
 */
char *
sys_getenv(const char *name)
{
    char *result = getenv(name);
    TPRINTF(("system getenv %s=%s\n", name, NonNull(result)));
    return result;
}

SIGNAL_HANDLER
setup_handler(int sig, SIGNAL_HANDLER disp)
{
    SIGNAL_HANDLER result;
#if defined(SA_RESTART)
    /* several systems (SCO, SunOS) have sigaction without SA_RESTART */
    /*
     * Redefine signal in terms of sigaction for systems which have the
     * SA_RESTART flag defined through <signal.h>
     *
     * This definition of signal will cause system calls to get restarted for a
     * more BSD-ish behavior.  This will allow us to use the OPT_WORKING
     * feature for such systems.
     */
    struct sigaction act, oact;

    act.sa_handler = disp;
    sigemptyset(&act.sa_mask);
#ifdef SA_NODEFER		/* don't rely on it.  if it's not there, signals
				   probably aren't deferred anyway. */
    act.sa_flags = SA_RESTART | SA_NODEFER;
#else
    act.sa_flags = SA_RESTART;
#endif

    if (sigaction(sig, &act, &oact) == 0)
	result = oact.sa_handler;
    else
	result = SIG_ERR;
#else /* !SA_RESTART */
    result = signal(sig, disp);
#endif
    return result;
}

/* put us in a new process group, on command.  we don't do this all the
* time since it interferes with suspending xvile on some systems with some
* shells.  but we _want_ it other times, to better isolate us from signals,
* and isolate those around us (like buggy window/display managers) from
* _our_ signals.  so we punt, and leave it up to the user.
*/
/* ARGSUSED */
int
newprocessgroup(int f GCC_UNUSED, int n GCC_UNUSED)
{
#if DISP_X11

    int pid;

    if (f) {
#if SYS_VMS
	pid = vfork();		/* VAX C and DEC C have a 'vfork()' */
#else
	pid = fork();
#endif

	if (pid > 0)
	    ExitProgram(GOODEXIT);
	else if (pid < 0) {
	    vl_fputs("cannot fork\n", stderr);
	    tidy_exit(BADEXIT);
	}
    }
# if !SYS_VMS
#  ifdef HAVE_SETSID
    (void) setsid();
#  else
#   ifdef HAVE_BSD_SETPGRP
    (void) setpgrp(0, 0);
#   else
    (void) setpgrp();
#   endif /* HAVE_BSD_SETPGRP */
#  endif /* HAVE_SETSID */
# endif	/* SYS_VMS */
#endif /* DISP_X11 */
    return TRUE;
}

static int
cmd_mouse_motion(const CMDFUNC * cfp)
{
    int result = FALSE;
#if (OPT_XTERM || DISP_X11)
    if (cfp != NULL
	&& CMD_U_FUNC(cfp) == mouse_motion)
	result = TRUE;
#else
    (void) cfp;
#endif
    return result;
}

static void
make_startup_file(const char *name)
{
    FILE *fp;
    char temp[NFILEN];

    if (startup_file != NULL
	&& strcmp(pathcat(temp, home_dir(), startup_file), startup_file)
	&& ((fp = fopen(temp, "w")) != NULL)) {
	fprintf(fp, "; generated by %s -I\n", prog_arg);
	fprintf(fp, "source %s\n", name);
	fclose(fp);
    }
}

#ifndef SIGTERM
#define SIGTERM 9
#endif

#ifndef valid_buffer
int
valid_buffer(BUFFER *test)
{
    beginDisplay();
    if (test != NULL && test != bminip) {
	BUFFER *bp;
	int valid = FALSE;

	for_each_buffer(bp) {
	    if (bp == test) {
		valid = TRUE;
		break;
	    }
	}
	if (!valid)
	    imdying(SIGTERM);
    }
    endofDisplay();

    return (test != NULL);
}
#endif

#ifndef valid_window
int
valid_window(WINDOW *test)
{
    beginDisplay();
    if (test != NULL && test != wminip && test != wnullp) {
	WINDOW *wp;
	int valid = FALSE;

	for_each_window(wp) {
	    if (wp == test) {
		valid = TRUE;
		break;
	    }
	}
	if (!valid)
	    imdying(SIGTERM);
    }
    endofDisplay();

    return (test != NULL);
}
#endif

#ifndef valid_line_bp
int
valid_line_bp(LINE *test, BUFFER *bp)
{
    beginDisplay();
    if (test != NULL) {
	int valid = FALSE;
	LINE *lp;

	if (test == buf_head(bp)) {
	    valid = TRUE;
	} else {
	    for_each_line(lp, bp) {
		if (lp == test) {
		    valid = TRUE;
		    break;
		}
	    }
	}
	if (!valid)
	    imdying(SIGTERM);
    }
    endofDisplay();

    return (test != NULL);
}
#endif

#ifndef valid_line_wp
int
valid_line_wp(LINE *test, WINDOW *wp)
{
    return (test != NULL)
	&& valid_window(wp)
	&& valid_buffer(wp->w_bufp)
	&& valid_line_bp(test, wp->w_bufp);
}
#endif

void
exit_program(int code)
{
    free_all_leaks();
    exit(code);
}

#ifdef VILE_ERROR_ABORT
#undef ExitProgram		/* in case it is oleauto_exit() */

void
ExitProgram(int code)
{
    char *env;
    if (code != GOODEXIT
	&& (env = vile_getenv("VILE_ERROR_ABORT")) != 0
	&& *env != '\0') {
	setup_sighandlers(FALSE);
	abort();
    }
    exit_program(code);
}
#endif

#if NO_LEAKS
void
free_all_leaks(void)
{
    beginDisplay();		/* ...this may take a while... */

    TRACE((T_CALLED "free_all_leaks\n"));

    /* free all of the global data structures */
    onel_leaks();
    path_leaks();
    kbs_leaks();
    bind_leaks();
    map_leaks();
    tags_leaks();
    wp_leaks();
    bp_leaks();
#if !SMALLER
    ev_leaks();
#endif
    mode_leaks();
    vars_leaks();
    fileio_leaks();
#if DISP_X11
    x11_leaks();
#endif

    free_local_vals(g_valnames, global_g_values.gv, global_g_values.gv);
    free_local_vals(b_valnames, global_b_values.bv, global_b_values.bv);
    free_local_vals(w_valnames, global_w_values.wv, global_w_values.wv);

    FreeAndNull(prog_arg);
    FreeAndNull(libdir_path);
    FreeAndNull(startup_path);
    FreeAndNull(gregexp);
    tb_free(&tb_matched_pat);
#if OPT_LOCALE
    FreeAndNull(vl_real_enc.locale);
    FreeAndNull(vl_real_enc.encoding);
#endif /* OPT_LOCALE */
#if OPT_MLFORMAT
    FreeAndNull(modeline_format);
#endif
#if OPT_POSFORMAT
    FreeAndNull(position_format);
#endif
    FreeAndNull(helpfile);
    FreeAndNull(startup_file);
#if SYS_UNIX
    if (exec_pathname != NULL
	&& strcmp(exec_pathname, "."))
	FreeAndNull(exec_pathname);
#endif
#if OPT_CURTOKENS
    free_regexval(buf_fname_expr.v.r);
#endif
#if OPT_FILTER
    flt_leaks();
    filters_leaks();
#endif
    vl_ctype_leaks();
    /* do these last, e.g., for tb_matched_pat */
    itb_leaks();
    tb_leaks();

    /* only do this for drivers which run inside a terminal */
#if DISP_TERMCAP || DISP_CURSES
    term.close();
#endif

#if OPT_LOCALE
    eightbit_leaks();
#endif
    vt_leaks();

    /* whatever is left over must be a leak */
#if OPT_TRACE
    show_alloc();
    show_elapsed();
    trace_leaks();
#endif
#if DISP_TERMCAP
    tcap_leaks();
#elif DISP_CURSES
    curses_leaks();
#endif
}
#endif /* NO_LEAKS */
