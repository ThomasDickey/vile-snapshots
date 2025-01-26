/*	Spawn:	various DOS access commands
 *		for MicroEMACS
 *
 * $Id: spawn.c,v 1.221 2025/01/26 16:31:43 tom Exp $
 */

#ifdef _WIN32
# include    <process.h>
#endif

#include	"estruct.h"
#include	"edef.h"
#include	"nefunc.h"

#if SYS_UNIX && defined(SIGTSTP) && !DISP_X11
#define USE_UNIX_JOB_CTL 1
#else
#define USE_UNIX_JOB_CTL 0
#endif

#if SYS_VMS
#include <starlet.h>
#include <lib$routines.h>
extern int vms_system(char *);	/*FIXME: not the same as 'system()'? */
#endif

#if OPT_FINDPATH

typedef struct struct_findinfo {
    FINDCFG cfg;		/* findcfg mode info             */
    const char *dir_list;	/* findpath directory list       */
    int nonrecursive;		/* this find cmd is nonrecursive */
} FINDINFO;

static char *ck_find_cmd(char *cmd, int *allocd_storage, int prepend_bang);
static char *find_all_files(char *cmd, FINDINFO * pinfo, int prepend_bang);
static char *find_dirs_only(char *cmd, FINDINFO * pinfo, int prepend_bang);

static char *prev_findcmd;	/* last shell command created by the builtin
				 * find feature.  debug aid -- may go away
				 */
#endif

static int spawn1(int rerun, int pressret);

#if	SYS_VMS
#define EFN	0		/* Event flag.          */

#include	<ssdef.h>	/* Random headers.      */
#include	<stsdef.h>
#include	<descrip.h>
#include	<iodef.h>

#if DISP_X11
#define VMSVT_DATA		/* nothing */
#else
#define VMSVT_DATA extern
#endif

VMSVT_DATA int oldmode[3];
VMSVT_DATA int newmode[3];
VMSVT_DATA short iochan;
#endif

#if CC_NEWDOSCC && !CC_DJGPP	/* Typo, was NEWSDOSCC  */
#include	<process.h>
#endif

/*
 * Check all modification-times after executing a shell command
 */
#ifdef	MDCHK_MODTIME
#define	AfterShell()	check_visible_files_changed()
#else
#define	AfterShell()	TRUE
#endif

#if DISP_X11 && !SMALLER
static void
x_window_SHELL(const char *cmd)
{
    TBUFF *tmp = NULL;

#ifdef HAVE_WAITPID
    int pid;

    if ((pid = fork()) > 0) {
	waitpid(pid, NULL, 0);
    } else if (pid == 0) {
	if (fork() == 0) {
#endif
	    /*
	     * We would use the -display option of xterm, but that
	     * would get in the way of the user's ability to
	     * customize $xshell.
	     */
#ifdef HAVE_PUTENV
	    static char *display_env;
	    char *env = get_xdisplay();
	    if (display_env != NULL)
		free(display_env);
	    if ((display_env = typeallocn(char, 20 + strlen(env))) != NULL) {
		lsprintf(display_env, "DISPLAY=%s", env);
		putenv(display_env);
	    }
#endif

	    tmp = tb_scopy(&tmp, get_xshell());
	    if (cmd != NULL) {
		tb_unput(tmp);
		tmp = tb_sappend(&tmp, " ");
		tmp = tb_sappend(&tmp, get_xshellflags());
		tmp = tb_sappend(&tmp, " ");
		tmp = tb_sappend(&tmp, cmd);
		tmp = tb_append(&tmp, EOS);
	    }
	    if (tmp != NULL) {
		char *result = tb_values(tmp);
		TRACE(("executing '%s'\n", result));
		IGNORE_RC(system(result));
		tb_free(&tmp);
	    }
#ifdef HAVE_WAITPID
	}
	free_all_leaks();
	_exit(0);
    }
#endif
}
#endif

/*
 * Create a subjob with a copy of the command interpreter in it. When the
 * command interpreter exits, mark the screen as garbage so that you do a full
 * repaint. The message at the start in VMS puts out a newline.
 * Under some (unknown) condition, you don't get one free when DCL starts up.
 */
/* ARGSUSED */
int
spawncli(int f GCC_UNUSED, int n GCC_UNUSED)
{
#undef OK_SPAWN

#if	SYS_UNIX
#define	OK_SPAWN
    term.clean(TRUE);
    (void) file_stat(NULL, NULL);
    term.openup();
#if	DISP_X11 && !SMALLER
    (void) x_window_SHELL((char *) 0);
#else
    (void) system_SHELL((char *) 0);
#endif
    term.openup();
    term.unclean();

    term.open();
    term.kopen();
    sgarbf = TRUE;
    return AfterShell();
#endif /* SYS_UNIX */

#if	SYS_VMS
#define	OK_SPAWN
    mlforce("[Starting DCL]\r\n");
    kbd_flush();		/* Ignore "ttcol".      */
    sgarbf = TRUE;
    return vms_system(NULL);	/* NULL => DCL.         */
#endif

#if	SYS_MSDOS || SYS_OS2 || SYS_WINNT
#define	OK_SPAWN
    bottomleft();
    kbd_flush();
    term.kclose();
    {
	char *shell = get_shell();
#if SYS_OS2
	/*
	 * spawn it if we know it.  Some 3rd party command processors fail
	 * if they system themselves (eg 4OS2).  CCM 24-MAR-94
	 */
	spawnl(P_WAIT, shell, shell, NULL);
#else
#if SYS_WINNT
# if DISP_NTCONS
	w32_CreateProcess(shell, FALSE);
# else
	/*
	 * This is winvile, in which case the editor _might_ have
	 * been launched from the command line (which implies
	 * direct access to an existing Win32 console environment)
	 * or else was launched as a true Win32 app (has no console
	 * env).  The latter case requires that CreateProcess() is
	 * called in such a way that a console is guaranteed to be
	 * allocated in the editor's behalf.
	 *
	 * The net result of the following call to w32_CreateProcess()
	 * is that the spawned shell runs in its own process, while
	 * winvile regains control immediately (i.e., there is no
	 * wait for the spawned shell to exit).
	 */
	w32_CreateProcess(shell, TRUE);
# endif
#else
	system(shell);
#endif
#endif
    }
    term.kopen();
    sgarbf = TRUE;
    return AfterShell();
#endif

#ifndef	OK_SPAWN
    mlforce("[This version of vile cannot spawn an interactive shell]");
    return FALSE;
#endif
}

#if USE_UNIX_JOB_CTL
int
bktoshell(int f, int n)		/* suspend and wait to wake up */
{
    int forced = (f && n == SPECIAL_BANG_ARG);	/* then it was :stop! */

    /* take care of autowrite */
    if (!forced && writeall(f, n, FALSE, TRUE, TRUE, FALSE) != TRUE)
	return FALSE;

    beginDisplay();
    term.clean(TRUE);
    (void) file_stat(NULL, NULL);

/* #define simulate_job_control_for_debug */
# ifdef simulate_job_control_for_debug
    rtfrmshell(SIGCONT);
    return TRUE;
# else
    (void) signal_pg(SIGTSTP);
    return TRUE;
# endif
}
#else
/* ARGSUSED */
int
bktoshell(int f GCC_UNUSED, int n GCC_UNUSED)
{
    mlforce("[Job control unavailable]");
    return FALSE;
}
#endif /* SIGTSTP */

/*ARGSUSED*/
SIGT
rtfrmshell(int ACTUAL_SIG_ARGS GCC_UNUSED)
{
#if USE_UNIX_JOB_CTL
    TRACE(("entering rtfrmshell...\n"));
    endofDisplay();
    term.openup();
    term.open();
    term.kopen();
    term.unclean();
    sgarbf = TRUE;
    setup_handler(SIGCONT, rtfrmshell);		/* suspend & restart */
    (void) update(TRUE);
#endif
#if defined(MDCHK_MODTIME)
    (void) check_visible_files_changed();
#endif
    SIGRET;
}

void
pressreturn(void)
{
    int c;
    int osgarbf;
    int old_reading;

    if (!(quiet || clhide)) {
	osgarbf = sgarbf;
	sgarbf = FALSE;
	mlforce("[Press return to continue]");
	sgarbf = osgarbf;
	/* loop for a CR, a space, or a : to do another named command */
	old_reading = read_msgline(TRUE);
	while ((c = keystroke()) != '\r' &&
	       c != '\n' &&
	       c != ' ' &&
	       !ABORTED(c)) {
	    if (DefaultKeyBinding(c) == &f_namedcmd) {
		unkeystroke(c);
		break;
	    }
	}
	kbd_erase_to_end(0);
	read_msgline(old_reading);
    }
}

/* ARGSUSED */
int
respawn(int f, int n GCC_UNUSED)
{
    return spawn1(TRUE, !f);
}

/* ARGSUSED */
int
vl_spawn(int f, int n GCC_UNUSED)
{
    return spawn1(FALSE, !f);
}

#define COMMON_SH_PROMPT (SYS_UNIX || SYS_VMS || SYS_MSDOS || SYS_OS2 || SYS_WINNT)

#if COMMON_SH_PROMPT
/*
 * Common function for prompting for shell/pipe command, and for recording the
 * last shell/pipe command so that we can support "!!" convention.
 *
 * Note that for 'capturecmd()', we must retain a leading "!".
 */
static int
ShellPrompt(TBUFF **holds,
	    char *result,
	    int rerun)		/* TRUE/FALSE: spawn, -TRUE: capturecmd */
{
    int s;
    size_t len;
    static const char bang[] = SHPIPE_LEFT;
    BUFFER *bp;
    int cb = any_changed_buf(&bp), fix = (rerun != -TRUE);
    char save[NLINE], temp[NLINE], line[NLINE + 1];

    if ((len = tb_length(*holds)) != 0) {
	(void) strncpy(save, tb_values(*holds), len);
    }
    save[len] = EOS;

    /* if it doesn't start with '!', or if that's all it is */
    if (!isShellOrPipe(save) || save[1] == EOS)
	(void) strcpy(save, bang);

    (void) strcpy(line, save);
    if (rerun != TRUE) {
	if (cb != 0) {
	    if (cb > 1) {
		(void) lsprintf(temp,
				"Warning: %d modified buffers: %s",
				cb, bang);
	    } else {
		(void) lsprintf(temp,
				"Warning: buffer \"%s\" is modified: %s",
				bp->b_bname, bang);
	    }
	} else {
	    (void) lsprintf(temp, "%s%s",
			    rerun == -TRUE ? "" : ": ", bang);
	}

	if ((s = mlreply_no_bs(temp, line + 1, NLINE - 1)) != TRUE)
	    return s;
    }
    if (line[1] == EOS)
	return FALSE;

    *holds = tb_scopy(holds, line);
    (void) strcpy(result, line + fix);
    return TRUE;
}
#endif

/*
 * Run a one-liner in a subjob. When the command returns, wait for a single
 * character to be typed, then mark the screen as garbage so a full repaint is
 * done.
 */
/* the #ifdefs have been totally separated, for readability */
static int
spawn1(int rerun, int pressret)
{
#if COMMON_SH_PROMPT
    int s;
    char line[NLINE];		/* command line send to shell */

    if ((s = ShellPrompt(&tb_save_shell[0], line, rerun)) != TRUE)
	return s;
#endif /* COMMON_SH_PROMPT */

    /* take care of autowrite */
    if (writeall(FALSE, 1, FALSE, TRUE, TRUE, FALSE) != TRUE)
	return FALSE;

#if SYS_UNIX
#if DISP_X11
    (void) pressret;
#if defined(HAVE_WAITPID) && !SMALLER
    (void) x_window_SHELL(line);
#else
    (void) system_SHELL(line);
#endif
#else
    term.clean(TRUE);
    (void) file_stat(NULL, NULL);

    (void) system_SHELL(line);

    term.unclean();
    if (pressret)
	pressreturn();
    term.open();
    term.kopen();
    term.flush();
    sgarbf = TRUE;
#endif /* DISP_X11 */
    return AfterShell();
#endif /* SYS_UNIX */

#if	SYS_VMS
    kbd_flush();
    s = vms_system(line);	/* Run the command.     */
    if (pressret) {
	term.putch('\r');
	term.putch('\n');
	term.flush();
	pressreturn();
    }
    sgarbf = TRUE;
    return (s);
#endif
#if	SYS_MSDOS || SYS_OS2 || SYS_WINNT
    kbd_erase_to_end(0);
    kbd_flush();
    term.kclose();
#if SYS_WINNT
# if DISP_NTWIN
    w32_system_winvile(line, &pressret);
# else
    if (W32_SKIP_SHELL(line))
	pressret = FALSE;
    w32_system(line);
# endif
    w32_keybrd_reopen(pressret);
#else
    system(line);
    term.open();
    term.kopen();
    /* wait for return here if we are interactive */
    if (pressret) {
	pressreturn();
    }
#endif
#if DISP_NTCONS
    ntcons_reopen();
#endif
    sgarbf = TRUE;
    return AfterShell();
#endif
}

/*
 * Pipe a one line command into a window
 */
/* ARGSUSED */
int
capturecmd(int f, int n)
{
    int s;
#if SYS_UNIX || SYS_MSDOS || SYS_VMS || SYS_OS2 || SYS_WINNT
    int allocd_storage;
    BUFFER *bp;			/* pointer to buffer to zot */
    char line[NLINE];		/* command line send to shell */
    char *final_cmd;		/* possibly edited command line */

    /* get the command to pipe in */
    hst_init('!');
    s = ShellPrompt(&tb_save_shell[!global_g_val(GMDSAMEBANGS)], line, -TRUE);
    hst_flush();

    /* prompt ok? */
    if (s != TRUE)
	return s;

#if OPT_FINDPATH
    if ((final_cmd = ck_find_cmd(line, &allocd_storage, TRUE)) == NULL)
	return (FALSE);
#else
    allocd_storage = FALSE;
    final_cmd = line;
#endif

    if ((s = writeall(f, n, FALSE, FALSE, TRUE, FALSE)) == TRUE) {
	if ((s = ((bp = bfind(OUTPUT_BufName, 0)) != NULL)) == TRUE) {
	    W_VALUES *save_wvals = save_window_modes(bp);

	    if ((s = popupbuff(bp)) == TRUE) {
		ch_fname(bp, final_cmd);
		bp->b_active = FALSE;	/* force a re-read */
		if ((s = swbuffer_lfl(bp, FALSE, FALSE)) == TRUE)
		    set_rdonly(bp, line, MDVIEW);
	    }
	    restore_window_modes(bp, save_wvals);
	}
    }
#if OPT_FINDPATH
    if (allocd_storage)
	(void) free(final_cmd);
#endif

#else /* ! SYS_UNIX */

    WINDOW *wp;			/* pointer to new window */
    BUFFER *bp;			/* pointer to buffer to zot */
    static char oline[NLINE];	/* command line send to shell */
    char line[NLINE];		/* command line send to shell */
    WINDOW *ocurwp;		/* save the current window during delete */

    static char filnam[NSTRING] = "command";

    /* get the command to pipe in */
    if ((s = mlreply("cmd: <", oline, NLINE)) != TRUE)
	return (s);

    (void) strcpy(line, oline);

    /* get rid of the command output buffer if it exists */
    if ((bp = find_b_name(OUTPUT_BufName)) != NULL) {
	/* try to make sure we are off screen */
	ocurwp = NULL;
	for_each_window(wp) {
	    if (wp->w_bufp == bp) {
		if (curwp != wp) {
		    ocurwp = curwp;
		    curwp = wp;
		}
		delwind(FALSE, 1);
		if (ocurwp != NULL)
		    curwp = ocurwp;
		break;
	    }
	}
	if (zotbuf(bp) != TRUE)
	    return (FALSE);
    }

    if (s != TRUE)
	return (s);

    /* split the current window to make room for the command output */
    if ((s = splitwind(FALSE, 1)) != TRUE)
	return (s);

    /* and read the stuff in */
    if ((s = getfile(filnam, FALSE)) != TRUE)
	return (s);

    /* overwrite its buffer name for consistency */
    set_bname(curbp, OUTPUT_BufName);

    /* make this window in VIEW mode, update buffer's mode lines */
    set_local_b_val(curwp->w_bufp, MDVIEW, TRUE);
    curwp->w_flag |= WFMODE;

#if OPT_FINDERR
    set_febuff(OUTPUT_BufName);
#endif

    /* and get rid of the temporary file */
    unlink(filnam);
    s = AfterShell();
#endif /* SYS_UNIX */
    return (s);
}

#if SYS_UNIX || SYS_MSDOS || (SYS_OS2 && CC_CSETPP) || SYS_WINNT
/*
 * write_kreg_to_pipe() exists to facilitate execution of a Win32 thread.
 * All other host operating systems are simply victims.
 */
static void
write_kreg_to_pipe(void *writefp)
{
    FILE *fw;
    KILL *kp;			/* pointer into kill register */

    fw = (FILE *) writefp;
    kregcirculate(FALSE);
    kp = kbs[ukb].kbufh;
    while (kp != NULL) {
	IGNORE_RC(fwrite((char *) kp->d_chunk,
			 (size_t) 1,
			 (size_t) KbSize(ukb, kp),
			 fw));
	kp = kp->d_next;
    }
#if SYS_UNIX && ! TEST_DOS_PIPES
    (void) fflush(fw);
    (void) fclose(fw);
    ExitProgram(GOODEXIT);
    /* NOTREACHED */
#else
# if SYS_WINNT
    /*
     * If this function is invoked by a thread, then that thread (not
     * the parent process) must close write pipe.  We generalize this
     * function so that all Win32 execution environments (threaded or
     * not) use the same code.
     */
    (void) fflush(fw);
    (void) fclose(fw);
    if (!global_g_val(GMDW32PIPES))
	npflush();
# else
    npflush();			/* fake multi-processing */
# endif
#endif
}

#if OPT_SELECTIONS
/*
 * A corresponding function to write the current region from DOT to MK to a
 * pipe.
 */
static void
write_region_to_pipe(void *writefp)
{
    FILE *fw = (FILE *) writefp;
    LINE *last = setup_region();
    LINE *lp = DOT.l;

    while (lp != last) {
	IGNORE_RC(fwrite((char *) lvalue(lp),
			 sizeof(char),
			   (size_t) llength(lp),
			 fw));
	vl_putc('\n', fw);
	lp = lforw(lp);
    }
#if SYS_UNIX && ! TEST_DOS_PIPES
    (void) fflush(fw);
    (void) fclose(fw);
    ExitProgram(GOODEXIT);
    /* NOTREACHED */
#else
# if SYS_WINNT
    /*
     * If this function is invoked by a thread, then that thread (not
     * the parent process) must close write pipe.  We generalize this
     * function so that all Win32 execution environments (threaded or
     * not) use the same code.
     */
    (void) fflush(fw);
    (void) fclose(fw);
    if (!global_g_val(GMDW32PIPES))
	npflush();
# else
    npflush();			/* fake multi-processing */
# endif
#endif
}
#endif
#endif /* OPT_SELECTIONS */

/*
 * FUNCTION
 *   filterregion(void)
 *
 * DESCRIPTION
 *   Run a region through an external filter, replace region with the
 *   filter's output.
 *
 *   Architecturally, filterregion() is designed like so:
 *
 *       ------------  write pipe    ------------
 *       | vile     |--------------->| a filter |
 *       |          |                |          |
 *       |          |<---------------| ex:  fmt |
 *       ------------  read pipe     ------------
 *
 *   The idea here is to exec a filter (say, fmt) and then SIMULTANEOUSLY
 *   pump a potentially big wad of data down the write pipe while, AT THE
 *   SAME TIME, reading the filter's output.
 *
 *   The words in caps are the key, as they illustrate a need for separate
 *   vile processes:  a writer and a reader.  This is accomplished on a
 *   Unix host via the function softfork(), which calls fork().
 *
 *   For all other OSes, softfork() is a stub, which means that the
 *   entire filter operation runs single threaded.  This is a problem.
 *   Consider the following scenario on a host that doesn't support fork():
 *
 *   1) vile's !<region> command is used to exec vile-c-filt
 *   2) vile begins pushing a large <region> (say, 100 KB) down the
 *      write pipe
 *   3) the filter responds by pushing back an even larger response
 *   4) since vile hasn't finished the write pipe operation, the editor
 *      does not read any data from the read pipe.
 *   5) eventually, vile's read pipe buffer limit is reached and the
 *      filter blocks.
 *   6) when the filter blocks, it can't read data from vile and so the
 *      editor's write pipe buffer limit will trip as well, blocking vile.
 *
 *   That's process deadlock.
 *
 *   One workaround on a non-Unix host is the use of temp files, which
 *   uses this algorithm:
 *
 *      vile writes region to temp file,
 *      vile execs filter with stdin connected to temp file,
 *      vile reads filter's response.
 *
 *   This mechanism can be effected without error by a single process.
 *
 *   An alternative workaround simply creates two threads that simulate the
 *   effects of fork().  The first thread fills the write pipe, while the
 *   second thread consumes the read pipe.
 *
 * RETURNS
 *   Boolean, T -> all is well, F -> failure
 */

int
filterregion(void)
{
    int s = FALSE;
/* FIXME work on this for OS2, need inout_popen support, or named pipe? */
#if SYS_UNIX || SYS_MSDOS || (SYS_OS2 && CC_CSETPP) || SYS_WINNT
    static char oline[NLINE];	/* command line send to shell */
    char line[NLINE];		/* command line send to shell */
    FILE *fr, *fw;

    TRACE((T_CALLED "filterregion\n"));

    /* get the filter name and its args */
    if ((s = mlreply_no_bs("!", oline, NLINE)) != TRUE)
	returnCode(s);

    (void) strcpy(line, oline);

    if ((s = inout_popen(&fr, &fw, line)) != TRUE) {
	mlforce("[Couldn't open pipe or command]");
    } else {
	if ((s = begin_kill()) == TRUE) {
	    if (!softfork()) {
#if !(SYS_WINNT && defined(GMDW32PIPES))
		write_kreg_to_pipe(fw);
#else
		/* This is a Win32 environment with compiled Win32 pipe
		 * support.
		 */
		if (global_g_val(GMDW32PIPES)) {
		    long rc = (long) _beginthread(write_kreg_to_pipe, 0, fw);

		    /*
		     * w32pipes mode enabled -- create child thread to blast
		     * region to write pipe.
		     */
		    if (rc == -1) {
			mlforce("[Can't create Win32 write pipe]");
			(void) fclose(fw);
			(void) npclose(fr);
			returnCode(FALSE);
		    }
		} else {
		    /*
		     * Single-threaded parent process writes region to pseudo
		     * write pipe (temp file).
		     */
		    write_kreg_to_pipe(fw);
		}
#endif
	    }
#if ! ((SYS_OS2 && CC_CSETPP) || SYS_WINNT)
	    if (fw != NULL)
		(void) fclose(fw);
#endif
	    DOT.l = lback(DOT.l);
	    s = ifile((char *) 0, TRUE, fr);
	    npclose(fr);
	    (void) firstnonwhite(FALSE, 1);
	    (void) setmark();
	    end_kill();
	} else {
	    fclose(fw);
	    npclose(fr);
	}
    }
#else
    TRACE((T_CALLED "filterregion (stub)\n"));
    mlforce("[Region filtering not available -- try buffer filtering]");
#endif
    returnCode(s);
}

#if OPT_SELECTIONS
/*
 * Like filterregion, but opens a stream reading from a filter's output when
 * we will not modify the current buffer (e.g., syntax highlighting).
 */
int
open_region_filter(void)
{
/* FIXME work on this for OS2, need inout_popen support, or named pipe? */
#if SYS_UNIX || SYS_MSDOS || (SYS_OS2 && CC_CSETPP) || SYS_WINNT
    static char oline[NLINE];	/* command line send to shell */
    char line[NLINE];		/* command line send to shell */
    FILE *fr, *fw;
#endif
    int s;

    TRACE((T_CALLED "open_region_filter\n"));

#if SYS_UNIX || SYS_MSDOS || (SYS_OS2 && CC_CSETPP) || SYS_WINNT
    /* get the filter name and its args */
    if ((s = mlreply_no_bs("!", oline, NLINE)) == TRUE) {
	(void) strcpy(line, oline);
	if ((s = inout_popen(&fr, &fw, line)) == TRUE) {
	    if (!softfork()) {
#if !(SYS_WINNT && defined(GMDW32PIPES))
		write_region_to_pipe(fw);
#else
		/* This is a Win32 environment with compiled Win32 pipe
		 * support.
		 */
		if (global_g_val(GMDW32PIPES)) {
		    ULONG code = (ULONG) _beginthread(write_region_to_pipe,
						      0,
						      fw);

		    /*
		     * w32pipes mode enabled -- create child thread to blast
		     * region to write pipe.
		     */
		    if (code == (ULONG) (-1)) {
			mlforce("[Can't create Win32 write pipe]");
			(void) fclose(fw);
			(void) npclose(fr);
			s = FALSE;
		    }
		} else {
		    /*
		     * Single-threaded parent process writes region to pseudo
		     * write pipe (temp file).
		     */
		    write_region_to_pipe(fw);
		}
#endif
	    }
	    if (s == TRUE) {
#if ! ((SYS_OS2 && CC_CSETPP) || SYS_WINNT)
		(void) fclose(fw);
#endif
		ffp = fr;
		count_fline = 0;
	    }
	} else {
	    mlforce("[Couldn't open pipe or command]");
	}
    }
#else
    mlforce("[Region filtering not available -- try buffer filtering]");
    s = FALSE;
#endif
    returnCode(s);
}
#endif /* OPT_SELECTIONS */

/*
 * filter a buffer through an external DOS program
 * this is obsolete, the filterregion code is better.
 */
/* ARGSUSED */
int
vile_filter(int f GCC_UNUSED, int n GCC_UNUSED)
{
#if !(SYS_UNIX||SYS_MSDOS || (SYS_OS2 && CC_CSETPP))	/* filterregion up above is better */
    int s;			/* return status from CLI */
    BUFFER *bp;			/* pointer to buffer to zot */
    static char oline[NLINE];	/* command line send to shell */
    char line[NLINE];		/* command line send to shell */
    char tnam[NFILEN];		/* place to store real file name */
    static char bname1[] = "fltinp";
#if	SYS_UNIX
    char *t;
#endif

    static char filnam1[] = "fltinp";
    static char filnam2[] = "fltout";

#if	SYS_VMS
    mlforce("[Not available under VMS]");
    return (FALSE);
#endif
    /* get the filter name and its args */
    if ((s = mlreply("cmd: |", oline, NLINE)) != TRUE)
	return (s);
    (void) strcpy(line, oline);

    /* setup the proper file names */
    bp = curbp;
    (void) strcpy(tnam, bp->b_fname);	/* save the original name */
    ch_fname(bp, bname1);	/* set it to our new one */

    /* write it out, checking for errors */
    if (writeout(filnam1, curbp, TRUE, TRUE) != TRUE) {
	mlforce("[Cannot write filter file]");
	ch_fname(bp, tnam);
	return (FALSE);
    }
#if	SYS_MSDOS || SYS_OS2 || SYS_WINNT
    (void) strcat(line, " <fltinp >fltout");
    bottomleft();
    term.kclose();
    system(line);
    term.kopen();
    sgarbf = TRUE;
    s = TRUE;
#endif
#if	SYS_UNIX
    bottomleft();
    term.clean(TRUE);
    (void) file_stat(0, 0);
    if ((t = strchr(line, '|')) != 0) {
	char temp[NLINE];
	(void) strcpy(temp, t);
	(void) strcat(strcpy(t, " <fltinp"), temp);
    } else {
	(void) strcat(line, " <fltinp");
    }
    (void) strcat(line, " >fltout");
    system(line);
    term.unclean();
    term.flush();
    sgarbf = TRUE;
    s = TRUE;
#endif

    /* on failure, escape gracefully */
    if (s != TRUE || ((s = readin(filnam2, FALSE, curbp, TRUE)) != TRUE)) {
	mlforce("[Execution failed]");
	ch_fname(bp, tnam);
	unlink(filnam1);
	unlink(filnam2);
	return (s);
    }

    ch_fname(bp, tnam);		/* restore name */

    b_set_changed(bp);		/* flag it as changed */
    nounmodifiable(bp);		/* and it can never be "un-changed" */

    /* and get rid of the temporary file */
    unlink(filnam1);
    unlink(filnam2);
    return AfterShell();
#else
    mlforce("[Buffer filtering not available -- use filter operator]");
    return FALSE;
#endif
}

#if	SYS_VMS
/*
 * Run a command. The "cmd" is a pointer to a command string, or NULL if you
 * want to run a copy of DCL in the subjob (this is how the standard routine
 * lib$spawn works. You have to do weird stuff with the terminal on the way in
 * and the way out, because DCL does not want the channel to be in raw mode.
 */
int
vms_system(char *cmd)
{
    struct dsc$descriptor cdsc;
    struct dsc$descriptor *cdscp;
    long status;
    long substatus;
    long iosb[2];

    status = sys$qiow(EFN, iochan, IO$_SETMODE, iosb, 0, 0,
		      oldmode, sizeof(oldmode), 0, 0, 0, 0);
    if (status != SS$_NORMAL || (iosb[0] & 0xFFFF) != SS$_NORMAL)
	return (FALSE);
    cdscp = NULL;		/* Assume DCL.          */
    if (cmd != NULL) {		/* Build descriptor.    */
	cdsc.dsc$a_pointer = cmd;
	cdsc.dsc$w_length = strlen(cmd);
	cdsc.dsc$b_dtype = DSC$K_DTYPE_T;
	cdsc.dsc$b_class = DSC$K_CLASS_S;
	cdscp = &cdsc;
    }
    status = lib$spawn(cdscp, 0, 0, 0, 0, 0, &substatus, 0, 0, 0);
    if (status != SS$_NORMAL)
	substatus = status;
    status = sys$qiow(EFN, iochan, IO$_SETMODE, iosb, 0, 0,
		      newmode, sizeof(newmode), 0, 0, 0, 0);
    if (status != SS$_NORMAL || (iosb[0] & 0xFFFF) != SS$_NORMAL)
	return (FALSE);
    if ((substatus & STS$M_SUCCESS) == 0)	/* Command failed.      */
	return (FALSE);
    return AfterShell();
}
#endif

#ifdef HAVE_PUTENV
int
set_envvar(int f GCC_UNUSED, int n GCC_UNUSED)
{
    static TBUFF *var, *val;

    char *both;
    int rc;

    if ((rc = mlreply2("Environment variable: ", &var)) != ABORT) {
	if ((rc = mlreply2("Value: ", &val)) != ABORT) {
	    size_t len_var = tb_length(var);
	    size_t len_val = tb_length(val);

	    if (len_var != 0
		&& len_val != 0
		&& (both = typeallocn(char, len_var + len_val + 1)) != NULL) {
		lsprintf(both, "%s=%s", tb_values(var), tb_values(val));
		rc = (putenv(both) == 0);	/* this will leak.  i think it has to. */
	    } else {
		rc = no_memory("set_envvar");
	    }
	}
    }

    return rc;
}
#endif

#if OPT_FINDPATH

/*
 * FUNCTION
 *   parse_findcfg_mode(FINDCFG *pcfg, char *inputstr)
 *
 *   pcfg     - returned by reference -- what was found in inputstr
 *
 *   inputstr - what to parse
 *
 * DESCRIPTION
 *   find-cfg mode is a string that supports this syntax:
 *
 *      "[<recursive_token>][,<nonrecursive_token>[,<option>...]]"
 *
 *   where:
 *     <recursive_token>    := an ascii char that triggers a recursive find,
 *                             may not be taken from the character set defined
 *                             by isalpha().  To use ',' as a token, escape it
 *                             with '\'.
 *     <nonrecursive_token> := an ascii char that triggers a nonrecursive find,
 *                             may not be taken from the character set defined
 *                             by isalpha().  To use ',' as a token, escape it
 *                             with '\'.
 *
 *     <option>             := <dirs_only>|<follow>
 *
 *     <dirs_only>          := d
 *
 *     <follow>             := f
 *
 *   Example usage:
 *     se find-cfg="$,@"  ; '$' -> recursive find, '@' -> nonrecursive find
 *     se find-cfg="$,,d" ; '$' -> recursive find, the find operation should
 *                        ; only look for directories.
 *
 *   Note that an empty string disables find-cfg mode.
 *
 * RETURNS
 *   Boolean, T -> all is well
 */
int
parse_findcfg_mode(FINDCFG * pcfg, char *inputstr)
{
    char *cp;
    int i, rc = TRUE;

    memset(pcfg, 0, sizeof(*pcfg));
    pcfg->disabled = TRUE;	/* an assumption */
    cp = mktrimmed(inputstr);
    if (*cp == EOS)
	return (rc);

    /* handle first 2 tokens */
    for (i = 0; i < 2 && *cp; i++) {
	if (*cp != ',') {
	    if (isAlpha(*cp)) {
		mlforce("[alphanumeric tokens not allowed]");
		return (FALSE);
	    }
	    if (*cp == '\\' && cp[1] == ',')
		cp++;		/* skip escape char */
	    if (i == 0)
		pcfg->recur_token = *cp++;
	    else
		pcfg->nonrecur_token = *cp++;
	    cp = skip_blanks(cp);
	    if (*cp) {
		if (*cp != ',') {
		    mlforce("[invalid find-cfg syntax]");
		    return (FALSE);
		}
	    } else {
		pcfg->disabled = (!rc);
		return (rc);	/* end of string, all done */
	    }
	}
	cp++;			/* skip token delimiter */
	cp = skip_blanks(cp);
	if (*cp == EOS) {
	    mlforce("[invalid find-cfg syntax]");
	    return (FALSE);
	}
    }

    /* options? */
    if (*cp) {
	while (*cp) {
	    if (*cp == 'd')
		pcfg->dirs_only = TRUE;
	    else if (*cp == 'f')
		pcfg->follow = TRUE;
	    else {
		mlforce("[invalid find-cfg syntax]");
		rc = FALSE;
		break;
	    }
	    cp++;
	}
    }
    pcfg->disabled = (!rc);
    return (rc);
}

static void
free_vector(char ***vec, size_t vec_elements)
{
    char **base;
    ULONG i;

    base = *vec;
    for (i = 0; i < vec_elements; i++, base++)
	(void) free(*base);
    (void) free(*vec);
}

/*
 * Cruise through the user's shell command, stripping out unquoted tokens
 * that include wildcard characters.   Save each token in a vector.  Return
 * the remains of the shell command to the caller (or NULL if an error occurs).
 */
static char *
extract_wildcards(char *cmd, char ***vec, size_t *vecidx, const char *fnname)
{
    char **temp;
    char **base;
    char *cp;
    char *anchor;
    char buf[NFILEN * 2];
    int delim;
    size_t idx, len;

    idx = 0;
    len = 32;
    cp = cmd;
    base = castalloc(char *, len * sizeof(char *));
    if (base == NULL) {
	(void) no_memory(fnname);
	return (NULL);
    }
    while (*cp) {
	cp = anchor = skip_blanks(cp);
	if (*cp == '\'' || *cp == '"') {
	    delim = *cp++;
	    while (*cp) {
		if (*cp == '\\' && cp[1] == delim) {
		    cp += 2;
		} else if (*cp == delim) {
		    cp++;
		    break;
		} else {
		    cp++;
		}
	    }
	} else {
	    cp++;
	    while (*cp && (!isSpace(*cp)))
		cp++;
	    len = (size_t) (cp - anchor);
	    strncpy(buf, anchor, len);
	    buf[len] = EOS;
	    if (string_has_wildcards(buf)) {
		memset(anchor, ' ', len);	/* blank out wildcard in cmd */
		if (idx >= len) {
		    len *= 2;
		    temp = castrealloc(char *, base, sizeof(*base));
		    if (temp == NULL) {
			free_vector(&base, idx);
			(void) no_memory(fnname);
			return (NULL);
		    } else {
			base = temp;
		    }
		}
		base[idx] = castalloc(char, len + 1);
		if (base[idx] == NULL) {
		    free_vector(&base, idx);
		    (void) no_memory(fnname);
		    return (NULL);
		}
		strcpy(base[idx++], buf);
	    }
	}
    }

    /* all done */
    *vec = base;
    *vecidx = idx;
    return (cmd);
}

static const char *
determine_quoted_delimiter(void)
{
    const char *qdelim;

#if SYS_UNIX
    qdelim = "'";
#else
    {
	char *cp, buf[NFILEN];
	int unix_shell, shell_len;

	strcpy(buf, get_shell());
	cp = strrchr(buf, '.');
	if (cp)
	    *cp = EOS;		/* trim file suffix */
	shell_len = (int) strlen(buf);
	unix_shell = (shell_len >= 2 &&
		      toLower(buf[shell_len - 2]) == 's' &&
		      toLower(buf[shell_len - 1]) == 'h');
	if (unix_shell)
	    qdelim = "'";
	else {
	    /*
	     * Assume a DOS-based shell, which generally honors double
	     * quotes as argument delimiters (not single quotes).
	     */

	    qdelim = "\"";
	}
    }
#endif
    return (qdelim);
}

char *
last_findcmd(void)
{
    return (prev_findcmd);
}

/*
 * Add a string to the end of a cmd string, lengthening same if necessary.
 * Does not terminate cmd string.
 */
static int
add_token_to_cmd(char **cmd,
		 size_t *cmdidx,
		 size_t *cmdlen,
		 const char *token,
		 const char *funcname)
{
    int rc = TRUE;
    char *tmp;
    size_t toklen = strlen(token);

    if ((tmp = *cmd) != NULL) {
	if (*cmdidx + toklen + 2 > *cmdlen) {
	    *cmdlen *= 2;
	    tmp = castrealloc(char, tmp, *cmdlen);
	    if (tmp == NULL) {
		(void) free(*cmd);
		*cmd = NULL;
	    } else {
		*cmd = tmp;
	    }
	}
    }
    if (*cmd != NULL) {
	strcpy(*cmd + *cmdidx, token);
	*cmdidx += toklen;
	(*cmd)[*cmdidx] = ' ';
	(*cmdidx)++;
	rc = TRUE;
    } else {
	rc = no_memory(funcname);
    }
    return (rc);
}

/*
 * FUNCTION
 *   ck_find_cmd(char *cmd, int *allocd_storage, int prepend_bang)
 *
 *   cmd            - user's original shell command.
 *
 *   allocd_storage - by ref, T -> the returned cmd string was allocated
 *                    on the heap.  Caller must free.
 *
 *   prepend_bang   - Boolean, T -> that if a modified shell command is
 *                    created, prepend it with '!'.
 *
 * DESCRIPTION
 *   If the user has enabled find-cfg mode and if the user's shell
 *   command contains a token that indicates that a find operation should
 *   be initiated, modify the user's shell command appropriately.
 *
 *   Example:
 *     :setv $findpath="."
 *     :se find-cfg="$"
 *     ^X-!$egrep -n FIXME *.[ch]
 *
 *   Resultant shell command on a Unix host (somewhat simplified):
 *     !find . -name '*.[ch]' -print | xargs egrep -n FIXME
 *
 *   Note that this example does not include the syntax used to filter out
 *   CVS/RCS directories and tags files.
 *
 * RETURNS
 *   Pointer to original user cmd if no find operation required, a newly
 *   synthesized cmd string, or NULL (failure case).
 */
static char *
ck_find_cmd(char *cmd, int *allocd_storage, int prepend_bang)
{
    char *cp, *cmdcopy;
    FINDINFO info;

    *allocd_storage = FALSE;
    if (!parse_findcfg_mode(&info.cfg, global_g_val_ptr(GVAL_FINDCFG))) {
	return (NULL);		/* bogus find-cfg value noted */
    }
    if (info.cfg.disabled)
	return (cmd);		/* find-cfg mode disabled */

    /*
     * don't munge vile's copy of the user's command line -- scrogs the
     * [Output] buffer name.
     */
    if ((cmdcopy = strmalloc(cmd)) == NULL) {
	(void) no_memory("ck_find_cmd");
	return (NULL);
    }
    cp = skip_blanks(cmdcopy);
    if (*cp == SHPIPE_LEFT[0])
	cp++;
    cp = skip_blanks(cp);
    if (*cp) {
	info.nonrecursive = (*cp == info.cfg.nonrecur_token);

	/* if specifying recursive or nonrecursive find syntax ... */
	if (info.nonrecursive || *cp == info.cfg.recur_token) {
	    /*
	     * user wants [non]recursive find syntax added to shell
	     * command.
	     */

	    info.dir_list = get_findpath();
	    if (info.dir_list[0] == EOS)
		info.dir_list = ".";
	    if (info.cfg.dirs_only)
		cmd = find_dirs_only(cp + 1, &info, prepend_bang);
	    else
		cmd = find_all_files(cp + 1, &info, prepend_bang);
	    if (cmd) {
		/* keep a record of the cmd that's about to be spawned */

		if (prev_findcmd)
		    (void) free(prev_findcmd);
		if ((prev_findcmd = strmalloc(cmd)) == NULL) {
		    (void) free(cmd);
		    cmd = NULL;
		    no_memory("ck_find_cmd");
		} else {
		    *allocd_storage = TRUE;
		}
	    }
	}
    }
    (void) free(cmdcopy);
    return (cmd);
}

#if SYS_UNIX
#define EGREP_OPT_CASELESS ""
#define EGREP_TAG_CASELESS "[Tt][Aa][Gg][Ss]"
#else
#define EGREP_OPT_CASELESS "i"
#define EGREP_TAG_CASELESS "tags"
#endif

/*
 * FUNCTION
 *   find_dirs_only(char *cmd, FINDINFO *pinfo, int prepend_bang)
 *
 *   cmd          - user's input command, stripped of leading '!' and
 *                  find-cfg tokens.
 *
 *   pinfo        - contains info about the user's various "find" parameters.
 *
 *   prepend_bang - Boolean, T -> prepend '!' to beginning of shell cmd
 *                  synthesized by this routine.
 *
 * DESCRIPTION
 *   The task:
 *
 *   - construct a shell command that looks like so:
 *
 *     [!]find <$findpath_dir_list> -type d -print | \
 *          egrep -v <RE that elides CVS & RCS dirs> | xargs cmdstr
 *
 *   - if executing on a host that supports case insensitive file names,
 *     modify the above shell command to include option modifiers that as
 *     required.
 *
 *   - if a nonrecursive find is requested, add an appropriate option
 *     (i.e., -maxdepth 1) to the find cmdline (again, this syntax used may
 *     only be supported by the GNU version of find).
 *
 * RETURNS
 *   Pointer to newly formulated shell command (allocated on the heap) or
 *   NULL (failure).
 */
static char *
find_dirs_only(char *cmd, FINDINFO * pinfo, int prepend_bang)
{
    size_t i, outidx, outlen;
    const char *path, *fnname;
    char *rslt, buf[NFILEN * 2];
    const char *qdelim;
    int first = TRUE;

    fnname = "find_dirs_only";
    outlen = sizeof(buf);
    outidx = 0;
    rslt = castalloc(char, outlen);
    if (!rslt) {
	(void) no_memory(fnname);
	return (NULL);
    }
    if (prepend_bang) {
	*rslt = SHPIPE_LEFT[0];
	rslt[1] = EOS;
	outidx = 1;
    } else
	*rslt = '\0';
    strcat(rslt, "find ");
    outidx += sizeof("find ") - 1;
    path = pinfo->dir_list;

    /* add directory list to find command */
    while ((path = parse_pathlist(path, buf, &first)) != NULL) {
	if (!add_token_to_cmd(&rslt, &outidx, &outlen, buf, fnname))
	    return (NULL);
    }

    /* worry about nonrecursive find */
    if (pinfo->nonrecursive) {
	if (!add_token_to_cmd(&rslt, &outidx, &outlen, "-maxdepth 1", fnname))
	    return (NULL);
    }

    /* follow symbolic links? */
    if (pinfo->cfg.follow) {
	if (!add_token_to_cmd(&rslt, &outidx, &outlen, "-follow", fnname))
	    return (NULL);
    }

    /* terminate find string with "-type d -print" */
    if (!add_token_to_cmd(&rslt, &outidx, &outlen, "-type d -print", fnname))
	return (NULL);

    qdelim = determine_quoted_delimiter();

    /*
     * filter out RCS/CVS directories and tags files.  we make the
     * assumption that the user's "find" creates filenames with
     * '/' as a path delimiter.
     *
     * ========================== FIXME ==================================
     * Note that this regular expression is simplistic and could be better
     * if it ensured that "RCS/CVS" are preceded by '/' or nothing.
     * ========================== FIXME ==================================
     */
    sprintf(buf,
	    "| egrep -v" EGREP_OPT_CASELESS " %s(RCS|CVS)/%s",
	    qdelim,
	    qdelim);
    if (!add_token_to_cmd(&rslt, &outidx, &outlen, buf, fnname)) {
	rslt = NULL;
    } else if (!add_token_to_cmd(&rslt, &outidx, &outlen, "| xargs", fnname)) {
	rslt = NULL;
    } else if (!add_token_to_cmd(&rslt, &outidx, &outlen, cmd, fnname)) {
	rslt = NULL;
    } else if (rslt != NULL) {
	rslt[outidx] = EOS;	/* terminate cmd string */
	if (outidx != 0) {
	    char *cp;

	    i = --outidx;
	    cp = rslt + outidx;
	    while (isSpace(*cp) && i != 0) {
		cp--;
		i--;
	    }
	    if (cp != rslt + outidx) {
		/* white space found at end of string, trim it. */

		cp[1] = EOS;
	    }
	}
    }
    return (rslt);
}

/*
 * FUNCTION
 *   find_all_files(char *cmd, FINDINFO *pinfo, int prepend_bang)
 *
 *   cmd          - user's input command, stripped of leading '!' and
 *                  find-cfg tokens.
 *
 *   pinfo        - contains info about the user's various "find" parameters.
 *
 *   prepend_bang - Boolean, T -> prepend '!' to beginning of shell cmd
 *                  synthesized by this routine.
 *
 * DESCRIPTION
 *   The task:
 *
 *   - scan all unquoted tokens in the user's command line.
 *
 *   - remove each unqoted token that includes shell wildcard chars.  Call the
 *     remainder of the user's input the "cmdstr".  Call the removed tokens
 *     the "wildvec".
 *
 *   - construct a shell command that looks like so:
 *
 *     [!]find <$findpath_dir_list> '(' -name wildvec[0] -o \
 *             -name wildvec[1] ... -o -name wildvec[n-1] ')' -print | \
 *          egrep -v <RE that elides CVS & RCS dirs and tags files> | \
 *          xargs cmdstr
 *
 *   - if executing on a host that supports case insensitive file names,
 *     modify the above shell command to include option modifiers that as
 *     required.  Note that the find options used in this case (i.e., -iname)
 *     may only be supported by the GNU version of find.
 *
 *   - if a nonrecursive find is requested, add an appropriate option
 *     (i.e., -maxdepth 1) to the find cmdline (again, this syntax used may
 *     only be supported by the GNU version of find).
 *
 * RETURNS
 *   Pointer to newly formulated shell command (allocated on the heap) or
 *   NULL (failure).
 */
static char *
find_all_files(char *cmd, FINDINFO * pinfo, int prepend_bang)
{
    size_t i, outidx, outlen, vecidx;
    const char *path, *fnname;
    char *xargstr, **vec, *rslt, buf[NFILEN * 2];
    const char *qdelim;
    int first = TRUE;

    fnname = "find_all_files";
    if ((xargstr = extract_wildcards(cmd, &vec, &vecidx, fnname)) == NULL)
	return (NULL);
    if (vecidx == 0) {
	/* No wild cards were found on the command line.  No sense
	 * continuing.  Why?  With no wild cards, the find command
	 * will search for all files by default, this may or may not
	 * be what the user wants.  It's certainly not what the user
	 * wants if s/he types this by mistake:
	 *
	 *    !<token>egrep -n FIXME 8.c
	 *
	 * which is an obvious slip of the fingers on the keyboard.
	 * If the user really wants to examine all files, s/he can type:
	 *
	 *    !<token>egrep -n FIXME *
	 */

	free_vector(&vec, vecidx);
	mlforce("[unless \"d\" option is set, shell command must include at least one wildcard]");
	return (NULL);
    }
    outlen = sizeof(buf);
    outidx = 0;
    rslt = castalloc(char, outlen);
    if (!rslt) {
	free_vector(&vec, vecidx);
	(void) no_memory(fnname);
	return (NULL);
    }
    if (prepend_bang) {
	*rslt = SHPIPE_LEFT[0];
	rslt[1] = EOS;
	outidx = 1;
    } else
	*rslt = '\0';
    strcat(rslt, "find ");
    outidx += sizeof("find ") - 1;
    path = pinfo->dir_list;

    /* add directory list to find command */
    while ((path = parse_pathlist(path, buf, &first)) != NULL) {
	if (!add_token_to_cmd(&rslt, &outidx, &outlen, buf, fnname)) {
	    free_vector(&vec, vecidx);
	    return (NULL);
	}
    }

    /* worry about nonrecursive find */
    if (pinfo->nonrecursive) {
	if (!add_token_to_cmd(&rslt, &outidx, &outlen, "-maxdepth 1", fnname)) {
	    free_vector(&vec, vecidx);
	    return (NULL);
	}
    }

    /* follow symbolic links? */
    if (pinfo->cfg.follow) {
	if (!add_token_to_cmd(&rslt, &outidx, &outlen, "-follow", fnname)) {
	    free_vector(&vec, vecidx);
	    return (NULL);
	}
    }

    if (vecidx > 1) {
	if (!add_token_to_cmd(&rslt, &outidx, &outlen, "'('", fnname)) {
	    free_vector(&vec, vecidx);
	    return (NULL);
	}
    }

    qdelim = determine_quoted_delimiter();

    /* add wildcards (if any) to find command */
    for (i = 0; i < vecidx; i++) {
	sprintf(buf,
		"%s-" EGREP_OPT_CASELESS "name %s%s%s",
		(i != 0) ? "-o " : "",
		qdelim,
		vec[i],
		qdelim);
	if (!add_token_to_cmd(&rslt, &outidx, &outlen, buf, fnname)) {
	    free_vector(&vec, vecidx);
	    return (NULL);
	}
    }
    free_vector(&vec, vecidx);	/* don't need this anymore */

    if (vecidx > 1) {
	if (!add_token_to_cmd(&rslt, &outidx, &outlen, "')'", fnname))
	    return (NULL);
    }

    /* terminate find string with "-print" (not needed for GNU find) */
    if (!add_token_to_cmd(&rslt, &outidx, &outlen, "-print", fnname))
	return (NULL);

    /*
     * filter out RCS/CVS directories and tags files.  we make the
     * assumption that the user's "find" creates filenames with
     * '/' as a path delimiter.
     *
     * ========================== FIXME ==================================
     * Note that this regular expression is simplistic and could be better
     * if it ensured that "RCS/CVS" are preceded by '/' or nothing.
     * ========================== FIXME ==================================
     */
    sprintf(buf,
	    "| egrep -v" EGREP_OPT_CASELESS
	    " %s((RCS|CVS)/|/" EGREP_TAG_CASELESS "$)%s",
	    qdelim,
	    qdelim);
    if (!add_token_to_cmd(&rslt, &outidx, &outlen, buf, fnname)) {
	rslt = NULL;
    } else if (!add_token_to_cmd(&rslt, &outidx, &outlen, "| xargs", fnname)) {
	rslt = NULL;
    } else if (!add_token_to_cmd(&rslt, &outidx, &outlen, xargstr, fnname)) {
	rslt = NULL;
    } else if (rslt != NULL) {
	rslt[outidx] = EOS;	/* terminate cmd string */
	if (outidx != 0) {
	    char *cp;

	    i = --outidx;
	    cp = rslt + outidx;
	    while (isSpace(*cp) && i != 0) {
		cp--;
		i--;
	    }
	    if (cp != rslt + outidx) {
		/* white space found at end of string, trim it. */

		cp[1] = EOS;
	    }
	}
    }
    return (rslt);
}

#endif /* OPT_FINDPATH */
