/*
 * w32pipe:  win32 clone of npopen.c--utilizes native pipes for hosts
 *           and shells that can handle same, else falls back on temp files
 *           (but uses a much different algorithm than that of npopen.c).
 *
 * Background
 * ==========
 * The techniques used in native_npclose() and native_inout_popen() are
 * derived from much trial and error and support "pipe" I/O in both a
 * console and GUI environment.  You may _think_ you have a better way of
 * effecting the functionality provided in this module and that may well be
 * the case.  But be sure you test your new code with at least these
 * versions of Win32:
 *
 *      win95, win98, OSR2, NT 4.0, win2K
 *
 * For each HOST, be sure to test read pipes, write pipes, and filters (and
 * test repeatedly within the same vile session).
 *
 *
 * Acknowledgments
 * ===============
 * Until I read Steve Kirkendall's code for the Win32 version of elvis, I
 * did not realize that attempting to redirect stdin to a device is a
 * _not_ a good strategy.
 *
 *
 * Caveats
 * =======
 * -- The MSDN Knowledge Base has example code that uses anonymous pipes
 *    to redirect a spawned process's stdin, stdout, and stderr.  Don't go
 *    there.
 *
 * -- On a Win2K host, DuplicateHandle() failed.
 *
 * -- The original Win95 console shell (command.com) accesses the floppy
 *    drive each and every time a process communicates with it via a pipe
 *    and the OSR2 shell abruptly hangs under similar conditions.  By
 *    default, then, on a WinNT host, vile's pipes are implemented using
 *    native pipes (i.e., with the code in this module), while Win95 hosts
 *    fall back to temp file communication.  If the user's replacement
 *    Win95 shell does not exhibit communication problems similar to
 *    those described above (e.g., any 32-bit shell), vile may be
 *    forced to use native Win32 pipes by setting the global mode
 *    "w32pipes" (e.g., "se w32pipes").  For further details, including
 *    a description of "force-console" mode, refer to the file
 *    doc/w32modes.doc .
 *
 * -- This module's native pipes implementation exhibits various problems
 *    when a 16-bit console app is exec'd.  On a win95 host, the editor
 *    and shell generally hang.  WinNT does better, but winvile creates
 *    "background" shell windows that require manual closure.
 *
 * -- This module configures read pipes so that the exec'd app reads
 *    it's input from an empty file.  That's a necessity, not a bug.
 *    Consequently, if an attempt is made to read data from an app
 *    that itself reads input (why would you do that?), the app will
 *    appear to hang if it reopens stdin on the console (because vile's
 *    stdin is not available to the app--another necessity).  In this
 *    situation, kill the app by typing ^C (and then please apply for a
 *    QA position with a certain Redmond company).
 *
 * $Id: w32pipe.c,v 1.42 2024/07/06 22:32:20 tom Exp $
 */

#define HAVE_FCNTL_H 1

#include "estruct.h"
#include "edef.h"

#include <io.h>
#ifdef __BORLANDC__
#define __MSC
#include <sys/stat.h>
#endif
#include <share.h>
#include <process.h>
#include <ctype.h>

#define BAD_FD          (-1)
#define BAD_PROC_HANDLE (INVALID_HANDLE_VALUE)
#define PIPESIZ         (4096)
#define SHELL_ERR_MSG   \
          "[shell process \"%s\" failed, check $shell state var]"

static int native_inout_popen(FILE **fr, FILE **fw, char *cmd);
static void native_npclose(FILE *fp);
static int tmp_inout_popen(FILE **fr, FILE **fw, char *cmd);
static void tmp_npclose(FILE *fp);

static HANDLE proc_handle;
static char *tmpin_name;

/* ---------------- Cloned from npopen.c ---------------------------- */

FILE *
npopen(char *cmd, const char *type)
{
    FILE *ff = 0;

    if (*type == 'r')
	(void) inout_popen(&ff, (FILE **) 0, cmd);
    else if (*type == 'w')
	(void) inout_popen((FILE **) 0, &ff, cmd);
    return ff;
}

int
softfork(void)			/* dummy function to make filter-region work */
{
    return 0;
}

int
inout_popen(FILE **fr, FILE **fw, char *cmd)
{
    TRACE(("inout_popen(fr=%p, fw=%p, cmd='%s')\n", fr, fw, cmd));

    ffstatus = file_is_pipe;
    fileeof = FALSE;
    append_libdir_to_path();
#ifdef GMDW32PIPES
    if (global_g_val(GMDW32PIPES))
	return (native_inout_popen(fr, fw, cmd));
#endif
    return (tmp_inout_popen(fr, fw, cmd));
}

void
npclose(FILE *fp)
{
#ifdef GMDW32PIPES
    if (global_g_val(GMDW32PIPES)) {
	native_npclose(fp);
	return;
    }
#endif
    tmp_npclose(fp);
}

/* ------------------- native pipe routines first ------------------- */

/*
 * when desperate to communicate an error, enable popup messages and
 * use mlforce().
 */
static void
lastditch_msg(char *msg)
{
#if OPT_POPUP_MSGS
    int save = global_g_val(GMDPOPUP_MSGS);

    set_global_g_val(GMDPOPUP_MSGS, TRUE);
#endif
    mlforce(msg);
#if OPT_POPUP_MSGS
    update(FALSE);
    popup_msgs();
    update(FALSE);
    set_global_g_val(GMDPOPUP_MSGS, save);
#endif
}

static void
common_cleanup(void)
{
    if (tmpin_name) {
	(void) remove(tmpin_name);
	(void) free(tmpin_name);
	tmpin_name = NULL;
    }
    restore_console_title();
}

#define close_fd(fd) if (fd != BAD_FD) { close(fd); fd = BAD_FD; }

static void
close_proc_handle(void)
{
    TRACE(("close_proc_handle %p\n", proc_handle));
    (void) w32_close_handle(proc_handle);
    proc_handle = BAD_PROC_HANDLE;
}

static HANDLE
exec_shell(char *cmd, HANDLE * handles, int hide_child GCC_UNUSED)
{
    W32_CHAR *w32_cmdstr = 0;
    char *cmdstr;
    int freestr;
    PROCESS_INFORMATION pi;
    STARTUPINFO si;

    proc_handle = BAD_PROC_HANDLE;	/* in case of failure */
    TRACE((T_CALLED "exec_shell %s\n", cmd));
    if ((cmdstr = mk_shell_cmd_str(cmd, &freestr, TRUE)) == NULL) {
	/* heap exhausted! */

	no_memory("exec_shell");

	/* Give user a chance to read message--more will surely follow. */
	Sleep(3000);
    } else if ((w32_cmdstr = w32_charstring(cmdstr)) == 0) {
	no_memory("exec_shell");
    } else {

	memset(&si, 0, sizeof(si));
	/* *INDENT-EQLS* */
	si.cb         = sizeof(si);
	si.dwFlags    = STARTF_USESTDHANDLES;
	si.hStdInput  = handles[0];
	si.hStdOutput = handles[1];
	si.hStdError  = handles[2];
#if DISP_NTWIN
	if (hide_child) {
	    si.dwFlags    |= STARTF_USESHOWWINDOW;
	    si.wShowWindow = SW_HIDE;
	}
#endif
	TRACE(("CreateProcess %s (pipe)\n", cmdstr));
	if (CreateProcess(NULL,
			  w32_cmdstr,
			  NULL,
			  NULL,
			  TRUE,	/* Inherit handles */
			  0,
			  NULL,
			  NULL,
			  &si,
			  &pi)) {
	    /* Success */

	    w32_close_handle(pi.hThread);
	    proc_handle = pi.hProcess;
	    TRACE(("...created proc_handle %p\n", proc_handle));
	}
    }
    FreeIfNeeded(cmdstr);
    FreeIfNeeded(w32_cmdstr);
    returnPtr(proc_handle);
}

static HANDLE
get_handle(int fd)
{
    HANDLE result;
    result = (HANDLE) _get_osfhandle(fd);
    return result;
}

static int
native_inout_popen(FILE **fr, FILE **fw, char *cmd)
{
    char buf[NFILEN + 128];
    HANDLE handles[3];
    int i, rc, rp[3], tmpin_fd, wp[3];

    TRACE((T_CALLED "native_inout_popen cmd=%s\n", cmd));

    /* *INDENT-EQLS* */
    proc_handle   = BAD_PROC_HANDLE;
    rp[0]         = rp[1] = rp[2] = wp[0] = wp[1] = wp[2] = BAD_FD;
    handles[0]    = handles[1] = handles[2] = INVALID_HANDLE_VALUE;
    tmpin_fd      = BAD_FD;
    tmpin_name    = NULL;
    set_console_title(cmd);

    if (is_win95()) {
	char *cmdp;

	/*
	 * If w32pipes is set on a win95 host, you don't ever want slowreadf()
	 * to periodically update the display while an intrinisic, high
	 * bandwidth DOS command is in progress.  Reason:  the win95 shell,
	 * command.com, will simply hang in the following scenario:
	 *
	 *    ^X!<high_bandwidth_cmd>
	 *
	 *         and
	 *
	 *    PIPESIZ < output of <high_bandwidth_cmd>
	 *
	 * I'm assuming that what's going on here is that command.com is
	 * written in ASM and is using very low level BIOS/DOS calls to
	 * effect output and, furthermore, that these low level calls don't
	 * block when the input consumer is busy.
	 */

	cmdp = skip_blanks(cmd);
	nowait_pipe_cmd = (strnicmp(cmdp, "dir", 3) == 0) ||
	    (strnicmp(cmdp, "type", 4) == 0);
    }
    do {
	if (fr) {
	    *fr = NULL;

	    /*
	     * Open (parent's) input pipe in TEXT mode, which will force
	     * translation of the child's CR/LF record delimiters to NL
	     * and keep the dreaded ^M chars from temporarily appearing
	     * in a vile buffer (ugly).
	     */
	    if (_pipe(rp, PIPESIZ, O_TEXT | O_NOINHERIT) == -1)
		break;
	    if ((rp[2] = _dup(rp[1])) == -1)
		break;
	    handles[2] = handles[1] = get_handle(rp[2]);
	    (void) close(rp[1]);
	    rp[1] = BAD_FD;
	    if (!fw) {
		/*
		 * This is a read pipe (only).  Connect child's stdin to
		 * an empty file.  Under no circumstances should the
		 * child's stdin be connected to a device (else lots of
		 * screwy things will occur).  In particular, connecting
		 * the child's stdin to the parent's stdin will cause
		 * aborts and hangs on the various Win32 hosts.  You've
		 * been warned.
		 */

		if ((tmpin_name = _tempnam(getenv("TEMP"), "vile")) == NULL)
		    break;
		if ((tmpin_fd = open(tmpin_name,
				     O_RDONLY | O_CREAT | O_TRUNC,
				     _S_IWRITE | _S_IREAD)) == BAD_FD) {
		    break;
		}
		handles[0] = get_handle(tmpin_fd);
	    }
	    if ((*fr = fdopen(rp[0], "r")) == 0)
		break;
	}
	if (fw) {
	    *fw = NULL;

	    /*
	     * Open (child's) output pipe in binary mode, which will
	     * prevent translation of the parent's CR/LF record delimiters
	     * to NL.  Apparently, many apps want those delimiters :-) .
	     */
	    if (_pipe(wp, PIPESIZ, O_BINARY | O_NOINHERIT) == -1)
		break;
	    if ((wp[2] = _dup(wp[0])) == -1)
		break;
	    handles[0] = get_handle(wp[2]);
	    (void) close(wp[0]);
	    wp[0] = BAD_FD;
	    if (!fr)
		handles[1] = handles[2] = GetStdHandle(STD_OUTPUT_HANDLE);
	    if ((*fw = fdopen(wp[1], "w")) == 0)
		break;
	}
	rc = (exec_shell(cmd,
			 handles,
			 fr != NULL	/* Child wdw hidden unless write pipe. */
	      ) == BAD_PROC_HANDLE) ? FALSE : TRUE;
	if (fw) {
	    if (!rc) {
		/* Shell process failed, put complaint in user's face. */

		sprintf(buf, SHELL_ERR_MSG, get_shell());
		lastditch_msg(buf);
	    }
	    w32_close_handle(handles[0]);
	}
	if (fr) {
	    if (!rc) {
		unsigned len;

		/*
		 * Shell process failed, put complaint in user's buffer.
		 * Can't write to handles[1] on a win2k host because the
		 * previously failed CreateProcess() call damaged the
		 * handle.
		 */
		len = (unsigned) (lsprintf(buf,
					   SHELL_ERR_MSG,
					   get_shell()) - buf);
		(void) write(rp[2], buf, len);
		(void) close(rp[2]);	/* in weird state; why not? */
	    }
	    w32_close_handle(handles[1]);
	    close_fd(tmpin_fd);
	}
	returnCode(rc);
    }
    while (FALSE);

    /* If we get here -- some operation has failed.  Clean up. */

    close_fd(wp[0]);
    close_fd(wp[1]);
    close_fd(wp[2]);
    close_fd(rp[0]);
    close_fd(rp[1]);
    close_fd(rp[2]);
    close_fd(tmpin_fd);
    for (i = 0; i < 3; i++) {
	if (handles[i] != INVALID_HANDLE_VALUE)
	    w32_close_handle(handles[i]);
    }
    common_cleanup();
    returnCode(FALSE);
}

static void
native_npclose(FILE *fp)
{
    (void) fflush(fp);
    (void) fclose(fp);
    if (proc_handle != BAD_PROC_HANDLE) {
	(void) w32_wait_handle(proc_handle);
	TRACE(("...CreateProcess finished waiting in native_npclose\n"));
	close_proc_handle();
    }
    common_cleanup();
}

/* ------------------- tmp file pipe routines last ------------------
 * --                                                              --
 * -- The key to making these routines work is recognizing that    --
 * -- under windows it's necessary to wait for the writer process  --
 * -- to finish and close tmp file A before the reader process     --
 * -- reads from A.                                                --
 * ------------------------------------------------------------------
 */

static HANDLE handles[3];
static int stdin_fd, stdout_fd;
static char *stdin_name, *stdout_name, *shcmd;

static void
tmp_cleanup(void)
{
    close_fd(stdin_fd);
    close_fd(stdout_fd);

    if (stdin_name) {
	(void) remove(stdin_name);
	(void) free(stdin_name);
	stdin_name = NULL;
    }
    if (stdout_name) {
	(void) remove(stdout_name);
	(void) free(stdout_name);
	stdout_name = NULL;
    }
    common_cleanup();
}

static int
tmp_inout_popen(FILE **fr, FILE **fw, char *cmd)
{
    char buf[NFILEN + 128];
    DWORD dummy, len;
    int rc, tmpin_fd;

    TRACE(("tmp_inout_popen cmd=%s\n", cmd));

    /* *INDENT-EQLS* */
    proc_handle = BAD_PROC_HANDLE;
    handles[0]  = handles[1] = handles[2] = INVALID_HANDLE_VALUE;
    tmpin_fd    = stdin_fd = stdout_fd = BAD_FD;
    tmpin_name  = stdin_name = stdout_name = NULL;
    set_console_title(cmd);

    do {
	if (fr) {
	    *fr = NULL;
	    if ((stdin_name = _tempnam(getenv("TEMP"), "vile")) == NULL)
		break;
	    if ((stdin_fd = open(stdin_name,
				 O_RDWR | O_CREAT | O_TRUNC | O_TEXT,
				 _S_IWRITE | _S_IREAD)) == BAD_FD) {
		break;
	    }
	    handles[2] = handles[1] = get_handle(stdin_fd);
	    if (!fw) {
		/*
		 * This is a read pipe (only).  Connect child's stdin to
		 * an empty file.  Under no circumstances should the
		 * child's stdin be connected to a device (else lots of
		 * screwy things will occur).  In particular, connecting
		 * the child's stdin to the parent's stdin will cause
		 * aborts and hangs on the various Win32 hosts.  You've
		 * been warned.
		 */

		if ((tmpin_name = _tempnam(getenv("TEMP"), "vile")) == NULL)
		    break;
		if ((tmpin_fd = open(tmpin_name,
				     O_RDONLY | O_CREAT | O_TRUNC,
				     _S_IWRITE | _S_IREAD)) == BAD_FD) {
		    break;
		}
		handles[0] = get_handle(tmpin_fd);
	    } else {
		/*
		 * Set up descriptor for filter operation.   Note the
		 * subtleties here:  exec'd shell is passed a descriptor
		 * to the temp file that's opened "w".  The editor
		 * receives a descriptor to the file that's opened "r".
		 */

		if ((*fr = fopen(stdin_name, "r")) == NULL)
		    break;
	    }
	}
	if (fw) {
	    *fw = NULL;

	    /* create a temp file to receive data from the editor */
	    if ((stdout_name = _tempnam(getenv("TEMP"), "vile")) == NULL)
		break;
	    if ((stdout_fd = open(stdout_name,
				  O_RDWR | O_CREAT | O_TRUNC | O_BINARY,
				  _S_IWRITE | _S_IREAD)) == BAD_FD) {
		break;
	    }
	    if ((*fw = fdopen(stdout_fd, "w")) == 0)
		break;

	    /*
	     * we're all set up, but can't exec "cmd" until the editor
	     * writes data to the temp file connected to stdout.
	     */
	    shcmd = cmd;	/* remember this */
	    return (TRUE);
	}

	/* This must be a read (only) pipe.  Appropriate to exec "cmd". */
	rc = (exec_shell(cmd,
			 handles,
			 TRUE	/* hide child wdw */
	      ) == BAD_PROC_HANDLE) ? FALSE : TRUE;

	if (!rc) {
	    /*
	     * Shell process failed, put complaint in user's buffer, which
	     * is currently proxied by a temp file that the editor will
	     * suck in shortly.
	     */
	    len = (DWORD) (lsprintf(buf, SHELL_ERR_MSG, get_shell()) - buf);
	    (void) WriteFile(handles[1], buf, len, &dummy, NULL);
	    FlushFileBuffers(handles[1]);
	} else {
	    /* wait for exec'd process to exit */

	    (void) w32_wait_handle(proc_handle);
	    TRACE(("...CreateProcess finished waiting in tmp_inout_popen\n"));
	    close_proc_handle();
	}

	if (fr) {
	    /*
	     * When closing descriptors shared between parent and child, order
	     * is quite important when $shell == command.com .  In this
	     * situation, the descriptors can't be closed until the exec'd
	     * process exits (I kid you not).
	     */
	    close_fd(stdin_fd);
	    (void) close(tmpin_fd);

	    /* let the editor consume the output of the read pipe */
	    if ((*fr = fopen(stdin_name, "r")) == NULL) {
		/*
		 * impossible to put error in user's buffer since that file
		 * descriptor is closed.
		 */
		sprintf(buf,
			"[error opening temp file \"%s\": %s]",
			stdin_name,
			strerror(errno));
		lastditch_msg(buf);
		break;
	    }
	}
	return (rc);
    }
    while (FALSE);

    /* If we get here -- some operation has failed.  Clean up. */
    tmp_cleanup();
    return (FALSE);
}

/* npflush is called for filter ops effected with temp files */
void
npflush(void)
{
    char buf[NFILEN + 128];
    int rc;

    /*
     * caller has filled and closed the write pipe data stream.  time to
     * exec a process.
     */

    if ((stdout_fd = open(stdout_name, O_RDONLY | O_BINARY)) == BAD_FD) {
	/* oh my, put complaint in user's face. */

	sprintf(buf, "[unable to open temp file \"%s\": %s]",
		stdout_name,
		strerror(errno));
	lastditch_msg(buf);
    } else {
	/* handles[1-2] were initialized by tmp_npopen_open() */

	handles[0] = get_handle(stdout_fd);
	rc = (exec_shell(shcmd,
			 handles,
			 TRUE	/* do hide child window */
	      ) == BAD_PROC_HANDLE) ? FALSE : TRUE;
	if (!rc) {
	    /* Shell process failed, put complaint in user's face. */

	    sprintf(buf, SHELL_ERR_MSG, get_shell());
	    lastditch_msg(buf);
	} else {
	    /* now wait for app to exit */

	    (void) w32_wait_handle(proc_handle);
	    TRACE(("...CreateProcess finished waiting in npflush\n"));
	    close_proc_handle();
	}

	/*
	 * When closing descriptors shared between parent and child, order
	 * is quite important when $shell == command.com .  In this
	 * situation, the descriptors can't be closed until the exec'd
	 * process exits.
	 */
	close_fd(stdout_fd);
    }
}

static void
tmp_npclose(FILE *fp)
{
    char buf[NFILEN + 128];
    int rc;

    (void) fflush(fp);
    (void) fclose(fp);

    if (stdout_fd != BAD_FD) {
	/*
	 * write pipe, but not a filter.  Editor has written data to temp
	 * file, time now to exec "cmd" and hook its stdin to the file.
	 *
	 * It should be noted that exec'ing a process in the npclose()
	 * phase of a write pipe is not exactly keeping in spirit with
	 * the control flow in file.c :-) .  However, the strategy used
	 * here ensures that the launched process reads a temp file that
	 * is completey flushed to disk.  The only direct drawback with
	 * this approach is that when the exec'd process exits, the user
	 * does not receive a "[press return to continue]" prompt from
	 * file.c .  But, cough, we can work around that problem :-) .
	 */

	if ((stdout_fd = open(stdout_name, O_RDONLY | O_BINARY)) == BAD_FD) {
	    /* oh my, put complaint in user's face. */

	    sprintf(buf, "[unable to open temp file \"%s\": %s]",
		    stdout_name,
		    strerror(errno));
	    lastditch_msg(buf);
	} else {
	    handles[0] = get_handle(stdout_fd);
	    handles[1] = handles[2] = GetStdHandle(STD_OUTPUT_HANDLE);
	    rc = (exec_shell(shcmd,
			     handles,
			     FALSE	/* don't hide child window */
		  ) == BAD_PROC_HANDLE) ? FALSE : TRUE;
	    if (!rc) {
		/* Shell process failed, put complaint in user's face. */

		sprintf(buf, SHELL_ERR_MSG, get_shell());
		lastditch_msg(buf);
	    } else {
		/* now wait for app to exit */

		(void) w32_wait_handle(proc_handle);
		TRACE(("...CreateProcess finished waiting in tmp_npclose\n"));
		close_proc_handle();
	    }

	    /*
	     * When closing descriptors shared between parent and child,
	     * order is quite important when $shell == command.com .  In
	     * this situation, the descriptors can't be closed until the
	     * exec'd process exits.
	     */
	    close_fd(stdout_fd);
	}
	pressreturn();		/* cough */
	sgarbf = TRUE;
    }
    tmp_cleanup();
}
