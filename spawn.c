/*	Spawn:	various DOS access commands
 *		for MicroEMACS
 *
 * $Header: /users/source/archives/vile.vcs/RCS/spawn.c,v 1.143 1999/04/13 23:29:34 pgf Exp $
 *
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
extern	int	vms_system(char *);	/*FIXME: not the same as 'system()'?*/
#endif

static	int spawn1(int rerun, int pressret);



#if	SYS_VMS
#define EFN	0				/* Event flag.		*/

#include	<ssdef.h>			/* Random headers.	*/
#include	<stsdef.h>
#include	<descrip.h>
#include	<iodef.h>

extern  int	oldmode[3];			/* In "termio.c"	*/
extern  int	newmode[3];			/* In "termio.c"	*/
extern  short	iochan;				/* In "termio.c"	*/
#endif

#if CC_NEWDOSCC && !CC_DJGPP			/* Typo, was NEWSDOSCC	*/
#include	<process.h>
#endif

/*
 * Check all modification-times after executing a shell command
 */
#ifdef	MDCHK_MODTIME
#define	AfterShell()	check_visible_modtimes()
#else
#define	AfterShell()	TRUE
#endif

#if DISP_X11 && !SMALLER
static void x_window_SHELL(const char *cmd)
{
	TBUFF *tmp = 0;

	/*
	 * We would use the -display option of xterm, but that would get in the
	 * way of the user's ability to customize $xshell.
	 */
#if HAVE_PUTENV
	static char *display_env;
	char *env = get_xdisplay();
	if (display_env != 0)
		free(display_env);
	display_env = typeallocn(char,20+strlen(env));
	lsprintf(display_env, "DISPLAY=%s", env);
	putenv(display_env);
#endif

	tmp = tb_scopy(&tmp, get_xshell());
	if (cmd != 0) {
		tb_unput(tmp);
		tmp = tb_sappend(&tmp, " -e ");
		tmp = tb_sappend(&tmp, cmd);
		tmp = tb_append(&tmp, EOS);
	}
	TRACE(("executing '%s'\n", tb_values(tmp)));
	(void)system(tb_values(tmp));
	tb_free(&tmp);
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
	ttclean(TRUE);
	kbd_openup();
#if	DISP_X11 && !SMALLER
	(void)x_window_SHELL((char *)0);
#else
	(void)system_SHELL((char *)0);
#endif
	kbd_openup();
	ttunclean();
	sgarbf = TRUE;
	return AfterShell();
#endif /* SYS_UNIX */


#if	SYS_VMS
#define	OK_SPAWN
	mlforce("[Starting DCL]\r\n");
	kbd_flush();				/* Ignore "ttcol".	*/
	sgarbf = TRUE;
	return vms_system(NULL);		/* NULL => DCL.		*/
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
 *	spawn it if we know it.  Some 3rd party command processors fail
 *	if they system themselves (eg 4OS2).  CCM 24-MAR-94
 */
		spawnl( P_WAIT, shell, shell, NULL);
#else
#if SYS_WINNT
		w32_system(shell);
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
	int forced = (f && n == SPECIAL_BANG_ARG); /* then it was :stop! */

	/* take care of autowrite */
	if (!forced && writeall(f,n,FALSE,TRUE,TRUE) != TRUE)
		return FALSE;

	beginDisplay();
	ttclean(TRUE);

/* #define simulate_job_control_for_debug */
# ifdef simulate_job_control_for_debug
	rtfrmshell(SIGCONT);
	return TRUE;
# else
	(void)signal_pg(SIGTSTP);

	/*
	 * Next four lines duplicate spawncli() actions following return
	 * from shell.	Adding lines 1-3 ensure that vile properly redraws
	 * its screen when TERM type is vt220 or vt320 and host is linux.
	 */
	kbd_openup();
	ttunclean();
	sgarbf = TRUE;
	return AfterShell();
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
	endofDisplay();
	ttunclean();
	sgarbf = TRUE;
#  if SYS_APOLLO
	(void)term.getch();		/* have to skip a character */
	ttunclean();		/* ...so that I can finally suppress echo */
#  endif
	term.kopen();
	setup_handler(SIGCONT,rtfrmshell); /* suspend & restart */
	(void)update(TRUE);
#endif
#ifdef	MDCHK_MODTIME
	(void)check_visible_modtimes();
#endif
	SIGRET;
}

void
pressreturn(void)
{
	int c;

	mlforce("[Press return to continue]");
	/* loop for a CR, a space, or a : to do another named command */
	while ((c = keystroke()) != '\r' &&
			c != '\n' &&
			c != ' ' &&
			!ABORTED(c)) {
		if (kcod2fnc(c) == &f_namedcmd) {
			unkeystroke(c);
			break;
		}
	}
	kbd_erase_to_end(0);
}

/* ARGSUSED */
int
respawn(int f, int n GCC_UNUSED)
{
	return spawn1(TRUE, !f);
}

/* ARGSUSED */
int
spawn(int f, int n GCC_UNUSED)
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
ShellPrompt(
TBUFF	**holds,
char	*result,
int	rerun)		/* TRUE/FALSE: spawn, -TRUE: capturecmd */
{
	register int	s;
	register SIZE_T	len;
	static	const char bang[] = SHPIPE_LEFT;
	BUFFER *bp;
	int	cb	= any_changed_buf(&bp),
		fix	= (rerun != -TRUE);
	char	save[NLINE],
		temp[NLINE],
		line[NLINE+1];

	if ((len = tb_length(*holds)) != 0) {
		(void)strncpy(save, tb_values(*holds), len);
	}
	save[len] = EOS;

	/* if it doesn't start with '!', or if that's all it is */
	if (!isShellOrPipe(save) || save[1] == EOS)
		(void)strcpy(save, bang);

	(void)strcpy(line, save);
	if (rerun != TRUE) {
		if (cb != 0) {
		    if (cb > 1) {
			(void)lsprintf(temp,
				"Warning: %d modified buffers: %s",
				cb, bang);
		    } else {
			(void)lsprintf(temp,
				"Warning: buffer \"%s\" is modified: %s",
				bp->b_bname, bang);
		    }
		} else {
			(void)lsprintf(temp, "%s%s",
				rerun == -TRUE ? "" : ": ", bang);
		}

		if ((s = mlreply_no_bs(temp, line+1, NLINE)) != TRUE)
			return s;
	}
	if (line[1] == EOS)
		return FALSE;

	*holds = tb_scopy(holds, line);
	(void)strcpy(result, line+fix);
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
#if DISP_IBMPC
	int	closed;
#endif
#if COMMON_SH_PROMPT
	register int	s;
	char	line[NLINE];	/* command line send to shell */

	if ((s = ShellPrompt(&tb_save_shell[0], line, rerun)) != TRUE)
		return s;
#endif	/* COMMON_SH_PROMPT */

	/* take care of autowrite */
	if (writeall(FALSE,1,FALSE,TRUE,TRUE) != TRUE)
		return FALSE;

#if SYS_UNIX
#if DISP_X11
	/*FIXME (void)x_window_SHELL(line); */
	(void)system_SHELL(line);
#else
	ttclean(TRUE);
	(void)system_SHELL(line);
	term.flush();
	ttunclean();
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
	s = vms_system(line);		/* Run the command.	*/
	if (pressret) {
		term.putch('\r');
		term.putch('\n');
		term.flush();
		pressreturn();
	}
	sgarbf = TRUE;
	return (s);
#endif
#if	SYS_WIN31
	mlforce("[Not in Windows 3.1]");
	return FALSE;
#endif
#if	SYS_MSDOS || SYS_OS2 || SYS_WINNT
	kbd_erase_to_end(0);
	kbd_flush();
	term.kclose();
#if	DISP_IBMPC
	/* If we don't reset to 80x25, parts of the shell-output will go
	 * astray.
	 */
	closed = term.cols != 80 || term.rows != 25;
	if (closed)
		term.close();
#endif
#if SYS_WINNT
# if DISP_NTWIN
	w32_system_winvile(line, pressret);
# else
	w32_system(line);
# endif
	w32_keybrd_reopen(pressret);
#else
	system(line);
	term.kopen();
	/* if we are interactive, pause here */
	if (pressret) {
		pressreturn();
	}
#endif
#if	DISP_IBMPC
	/* Reopen the display _after_ the prompt, to keep the shell-output
	 * in the same type of screen as the prompt.
	 */
	if (closed)
		term.open();
#endif
#if DISP_NTCONS
	ntcons_reopen();
#endif
	sgarbf = TRUE;
	return AfterShell();
#endif
}

#if SYS_UNIX || SYS_MSDOS || SYS_VMS || SYS_OS2 || SYS_WINNT
/*
 * Pipe a one line command into a window
 */
/* ARGSUSED */
int
capturecmd(int f, int n)
{
	register BUFFER *bp;	/* pointer to buffer to zot */
	register int	s;
	char line[NLINE];	/* command line send to shell */

	/* get the command to pipe in */
	hst_init('!');
	s = ShellPrompt(&tb_save_shell[!global_g_val(GMDSAMEBANGS)],
		line, -TRUE);
	hst_flush();

	/* prompt ok? */
	if (s != TRUE)
		return s;

	/* take care of autowrite */
	if (writeall(f,n,FALSE,FALSE,TRUE) != TRUE)
		return FALSE;

	if ((s = ((bp = bfind(OUTPUT_BufName, 0)) != NULL)) != TRUE)
		return s;
	if ((s = popupbuff(bp)) != TRUE)
		return s;
	ch_fname(bp,line);
	bp->b_active = FALSE; /* force a re-read */
	if ((s = swbuffer_lfl(bp,FALSE)) != TRUE)
		return s;
	set_rdonly(bp, line, MDVIEW);

	return (s);
}

#else /* ! SYS_UNIX */

/*
 * Pipe a one line command into a window
 */
int
capturecmd(int f, int n)
{
	register int	s;	/* return status from CLI */
	register WINDOW *wp;	/* pointer to new window */
	register BUFFER *bp;	/* pointer to buffer to zot */
	static char oline[NLINE];	/* command line send to shell */
	char	line[NLINE];	/* command line send to shell */
	WINDOW *ocurwp;		/* save the current window during delete */

	static char filnam[NSTRING] = "command";

	/* get the command to pipe in */
	if ((s=mlreply("cmd: <", oline, NLINE)) != TRUE)
		return(s);

	(void)strcpy(line,oline);

	/* get rid of the command output buffer if it exists */
	if ((bp=find_b_name(OUTPUT_BufName)) != NULL) {
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
			return(FALSE);
	}

	if (s != TRUE)
		return(s);

	/* split the current window to make room for the command output */
	if (splitwind(FALSE, 1) == FALSE)
		return(FALSE);

	/* and read the stuff in */
	if (getfile(filnam, FALSE) == FALSE)
		return(FALSE);

	/* overwrite its buffer name for consistency */
	set_bname(curbp, OUTPUT_BufName);

	/* make this window in VIEW mode, update buffer's mode lines */
	make_local_b_val(curwp->w_bufp,MDVIEW);
	set_b_val(curwp->w_bufp,MDVIEW,TRUE);
	curwp->w_flag |= WFMODE;

#if OPT_FINDERR
	set_febuff(OUTPUT_BufName);
#endif

	/* and get rid of the temporary file */
	unlink(filnam);
	return AfterShell();
}
#endif /* SYS_UNIX */

#if SYS_UNIX || SYS_MSDOS || (SYS_OS2 && CC_CSETPP) || SYS_WINNT
/*
 * write_kreg_to_pipe() exists to facilitate execution of a Win32 thread.
 * All other host operating systems are simply victims.
 */
static void
write_kreg_to_pipe(void *writefp)
{
    FILE *fw;
    KILL *kp;       /* pointer into kill register */

    fw = (FILE *)writefp;
    kregcirculate(FALSE);
    kp = kbs[ukb].kbufh;
    while (kp != NULL)
    {
	fwrite((char *)kp->d_chunk, 1, (SIZE_T)KbSize(ukb,kp), fw);
	kp = kp->d_next;
    }
#if SYS_UNIX && ! TEST_DOS_PIPES
    (void)fflush(fw);
    (void)fclose(fw);
    ExitProgram (GOODEXIT);
    /* NOTREACHED */
#else
    npflush();  /* fake multi-processing */
#endif
#if SYS_WINNT
    /*
     * If this function is invoked by a thread, then that thread (not
     * the parent process) must close write pipe.  We generalize this
     * function so that all Win32 execution environments (threaded or
     * not) use the same code.
     */
    (void)fflush(fw);
    (void)fclose(fw);
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
    FILE *fw = (FILE *)writefp;
    LINEPTR last = setup_region();
    LINEPTR lp = DOT.l;

    while (lp != last) {
	fwrite((char *)lp->l_text, sizeof(char), (SIZE_T)llength(lp), fw);
	fputc('\n', fw);
	lp = lforw(lp);
    }
#if SYS_UNIX && ! TEST_DOS_PIPES
    (void)fflush(fw);
    (void)fclose(fw);
    ExitProgram (GOODEXIT);
    /* NOTREACHED */
#else
    npflush();  /* fake multi-processing */
#endif
#if SYS_WINNT
    /*
     * If this function is invoked by a thread, then that thread (not
     * the parent process) must close write pipe.  We generalize this
     * function so that all Win32 execution environments (threaded or
     * not) use the same code.
     */
    (void)fflush(fw);
    (void)fclose(fw);
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
/* FIXME work on this for OS2, need inout_popen support, or named pipe? */
#if SYS_UNIX || SYS_MSDOS || (SYS_OS2 && CC_CSETPP) || SYS_WINNT
    static char oline[NLINE];   /* command line send to shell */
    char    line[NLINE];    /* command line send to shell */
    FILE *fr, *fw;
    int s;

    /* get the filter name and its args */
    if ((s=mlreply_no_bs("!", oline, NLINE)) != TRUE)
	return(s);
    (void)strcpy(line,oline);
    if ((s = inout_popen(&fr, &fw, line)) != TRUE) {
	mlforce("[Couldn't open pipe or command]");
	return s;
    }

    if ((s = begin_kill()) != TRUE)
    	return s;

    if (!softfork())
    {
#if !(SYS_WINNT && defined(GMDW32PIPES))
	write_kreg_to_pipe(fw);
#else
	/* This is a Win32 environment with compiled Win32 pipe support. */
	if (global_g_val(GMDW32PIPES))
	{
	    /*
	     * w32pipes mode enabled -- create child thread to blast
	     * region to write pipe.
	     */

	    if (_beginthread(write_kreg_to_pipe, 0, fw) == -1)
	    {
		mlforce("[Can't create Win32 write pipe]");
		(void) fclose(fw);
		(void) npclose(fr);
		return (FALSE);
	    }
	}
	else
	{
	    /*
	     * Single-threaded parent process writes region to pseudo
	     * write pipe (temp file).
	     */

	    write_kreg_to_pipe(fw);
	}
#endif
    }
#if ! ((SYS_OS2 && CC_CSETPP) || SYS_WINNT)
    (void)fclose(fw);
#endif
    DOT.l = lback(DOT.l);
    s = ifile((char *)0,TRUE,fr);
    npclose(fr);
    (void)firstnonwhite(FALSE,1);
    (void)setmark();
    end_kill();
    return s;
#else
    mlforce("[Region filtering not available -- try buffer filtering]");
    return FALSE;
#endif
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
    static char oline[NLINE];   /* command line send to shell */
    char    line[NLINE];    /* command line send to shell */
    FILE *fr, *fw;
    int s;

    /* get the filter name and its args */
    if ((s=mlreply_no_bs("!", oline, NLINE)) != TRUE)
	return(s);
    (void)strcpy(line,oline);
    if ((s = inout_popen(&fr, &fw, line)) != TRUE) {
	mlforce("[Couldn't open pipe or command]");
	return s;
    }

    if (!softfork())
    {
#if !(SYS_WINNT && defined(GMDW32PIPES))
	write_region_to_pipe(fw);
#else
	/* This is a Win32 environment with compiled Win32 pipe support. */
	if (global_g_val(GMDW32PIPES))
	{
	    /*
	     * w32pipes mode enabled -- create child thread to blast
	     * region to write pipe.
	     */

	    if (_beginthread(write_region_to_pipe, 0, fw) == -1)
	    {
		mlforce("[Can't create Win32 write pipe]");
		(void) fclose(fw);
		(void) npclose(fr);
		return (FALSE);
	    }
	}
	else
	{
	    /*
	     * Single-threaded parent process writes region to pseudo
	     * write pipe (temp file).
	     */

	    write_region_to_pipe(fw);
	}
#endif
    }
#if ! ((SYS_OS2 && CC_CSETPP) || SYS_WINNT)
    (void)fclose(fw);
#endif
    ffp = fr;
    return s;
#else
    mlforce("[Region filtering not available -- try buffer filtering]");
    return FALSE;
#endif
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
#if !(SYS_UNIX||SYS_MSDOS || (SYS_OS2 && CC_CSETPP)) /* filterregion up above is better */
	register int	s;	/* return status from CLI */
	register BUFFER *bp;	/* pointer to buffer to zot */
	static char oline[NLINE];	/* command line send to shell */
	char	line[NLINE];	/* command line send to shell */
	char tnam[NFILEN];	/* place to store real file name */
	static char bname1[] = "fltinp";
#if	SYS_UNIX
	char	*t;
#endif

	static char filnam1[] = "fltinp";
	static char filnam2[] = "fltout";

#if	SYS_VMS
	mlforce("[Not available under VMS]");
	return(FALSE);
#endif
	/* get the filter name and its args */
	if ((s=mlreply("cmd: |", oline, NLINE)) != TRUE)
		return(s);
	(void)strcpy(line,oline);

	/* setup the proper file names */
	bp = curbp;
	(void)strcpy(tnam, bp->b_fname);/* save the original name */
	ch_fname(bp, bname1);		/* set it to our new one */

	/* write it out, checking for errors */
	if (writeout(filnam1,curbp,TRUE,TRUE) != TRUE) {
		mlforce("[Cannot write filter file]");
		ch_fname(bp, tnam);
		return(FALSE);
	}

#if	SYS_MSDOS || SYS_OS2 || SYS_WINNT
	(void)strcat(line," <fltinp >fltout");
	bottomleft();
	term.kclose();
	system(line);
	term.kopen();
	sgarbf = TRUE;
	s = TRUE;
#endif
#if	SYS_UNIX
	bottomleft();
	ttclean(TRUE);
	if ((t = strchr(line, '|')) != 0) {
		char	temp[NLINE];
		(void)strcpy(temp, t);
		(void)strcat(strcpy(t, " <fltinp"), temp);
	} else {
		(void)strcat(line, " <fltinp");
	}
	(void)strcat(line," >fltout");
	system(line);
	ttunclean();
	term.flush();
	sgarbf = TRUE;
	s = TRUE;
#endif

	/* on failure, escape gracefully */
	if (s != TRUE || (readin(filnam2,FALSE,curbp,TRUE) == FALSE)) {
		mlforce("[Execution failed]");
		ch_fname(bp, tnam);
		unlink(filnam1);
		unlink(filnam2);
		return(s);
	}

	ch_fname(bp, tnam); /* restore name */

	b_set_changed(bp);	/* flag it as changed */
	nounmodifiable(bp);	/* and it can never be "un-changed" */

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
 * lib$spawn works. You have to do wierd stuff with the terminal on the way in
 * and the way out, because DCL does not want the channel to be in raw mode.
 */
int
vms_system(register char *cmd)
{
	struct  dsc$descriptor  cdsc;
	struct  dsc$descriptor  *cdscp;
	long	status;
	long	substatus;
	long	iosb[2];

	status = sys$qiow(EFN, iochan, IO$_SETMODE, iosb, 0, 0,
			  oldmode, sizeof(oldmode), 0, 0, 0, 0);
	if (status!=SS$_NORMAL || (iosb[0]&0xFFFF)!=SS$_NORMAL)
		return (FALSE);
	cdscp = NULL;				/* Assume DCL.		*/
	if (cmd != NULL) {			/* Build descriptor.	*/
		cdsc.dsc$a_pointer = cmd;
		cdsc.dsc$w_length  = strlen(cmd);
		cdsc.dsc$b_dtype   = DSC$K_DTYPE_T;
		cdsc.dsc$b_class   = DSC$K_CLASS_S;
		cdscp = &cdsc;
	}
	status = lib$spawn(cdscp, 0, 0, 0, 0, 0, &substatus, 0, 0, 0);
	if (status != SS$_NORMAL)
		substatus = status;
	status = sys$qiow(EFN, iochan, IO$_SETMODE, iosb, 0, 0,
			  newmode, sizeof(newmode), 0, 0, 0, 0);
	if (status!=SS$_NORMAL || (iosb[0]&0xFFFF)!=SS$_NORMAL)
		return (FALSE);
	if ((substatus&STS$M_SUCCESS) == 0)	/* Command failed.	*/
		return (FALSE);
	return AfterShell();
}
#endif
