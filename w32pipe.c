/*
 * w32pipe:  win32 clone of npopen.c, utilizes native pipes (not temp files).
 *
 * Background
 * ==========
 * The techniques used in w32_npclose() and w32_inout_popen() are derived
 * from much trial and error and support "pipe" I/O in both a console and
 * GUI environment.  You may _think_ you have a better way of effecting the
 * functionality provided in this module and that may well be the case.
 * But be sure you test your new code with at least these versions of Win32:
 *
 *      win95 (original version), OSR2, NT 4.0
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
 * -- The original Win95 console shell (command.com) accesses the floppy
 *    drive each and every time a process communicates with it via a pipe
 *    and the OS R2 shell abruptly hangs under similar conditions.  By
 *    default, then, on a WinNT host, vile's pipes are implemented using
 *    native pipes (i.e., with the code in this module), while Win95 hosts
 *    fall back to temp file communication.  If the user's replacement
 *    Win95 shell does not exhibit communication problems similar to
 *    those described above (e.g., Thompson Toolkit Shell), vile may be
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
 * $Header: /users/source/archives/vile.vcs/RCS/w32pipe.c,v 1.21 2000/08/09 09:13:01 Ian.Jamison Exp $
 */

#include <windows.h>
#include <io.h>
#ifdef __BORLANDC__
#define __MSC
#include <sys/stat.h>
#endif
#include <share.h>
#include <process.h>
#include <assert.h>
#include <ctype.h>

#define HAVE_FCNTL_H 1

#include "estruct.h"
#include "edef.h"

#define BAD_FD          (-1)
#define BAD_PROC_HANDLE (INVALID_HANDLE_VALUE)
#define PIPESIZ         (4096)
#define SHELL_ERR_MSG   \
          "error: shell process \"%s\" failed, check COMSPEC env var\n"

static HANDLE proc_handle;
static char   *tmpin_name;

/* ------------------------------------------------------------------ */

static void
global_cleanup(void)
{
    if (tmpin_name)
    {
        (void) remove(tmpin_name);
        (void) free(tmpin_name);
        tmpin_name = NULL;
    }
    restore_console_title();
#if DISP_NTWIN
    if (global_g_val(GMDFORCE_CONSOLE))
        FreeConsole();
#endif
}



static HANDLE
exec_shell(char *cmd, HANDLE *handles, int hide_child)
{
    char                 *cmdstr;
#if DISP_NTWIN
    HWND                 fgnd;
    int                  force_console = global_g_val(GMDFORCE_CONSOLE);
#endif
    int                  freestr;
    PROCESS_INFORMATION  pi;
    STARTUPINFO          si;

    proc_handle = BAD_PROC_HANDLE;  /* in case of failure */
    TRACE(("exec_shell %s\n", cmd));
    if ((cmdstr = mk_shell_cmd_str(cmd, &freestr, TRUE)) == NULL)
    {
        /* heap exhausted! */

        mlforce("insufficient memory to invoke shell");

        /* Give user a chance to read message--more will surely follow. */
        Sleep(3000);
        return (proc_handle);
    }

    memset(&si, 0, sizeof(si));
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES;
    si.hStdInput   = handles[0];
    si.hStdOutput  = handles[1];
    si.hStdError   = handles[2];
#if DISP_NTWIN
    if (force_console)
    {
        if (hide_child)
            fgnd = GetForegroundWindow();
        AllocConsole();
    }
    else if (hide_child)
    {
        si.dwFlags     |= STARTF_USESHOWWINDOW;
        si.wShowWindow  = SW_HIDE;
    }
#endif
    TRACE(("CreateProcess %s\n", cmdstr));
    if (CreateProcess(NULL,
                      cmdstr,
                      NULL,
                      NULL,
                      TRUE,       /* Inherit handles */
#if DISP_NTWIN
                      force_console ? 0 : CREATE_NEW_CONSOLE,
#else
                      0,
#endif
                      NULL,
                      NULL,
                      &si,
                      &pi))
    {
        /* Success */

#if DISP_NTWIN
        if (force_console && hide_child)
            SetForegroundWindow(fgnd);
#endif
        CloseHandle(pi.hThread);
        proc_handle = pi.hProcess;
    }
    if (freestr)
        free(cmdstr);
    return (proc_handle);
}



int
w32_inout_popen(FILE **fr, FILE **fw, char *cmd)
{
    HANDLE handles[3];
    int    i, rc, rp[3], tmpin_fd, wp[3];

    TRACE(("w32_inout_popen cmd=%s\n", cmd));
    proc_handle  = BAD_PROC_HANDLE;
    rp[0] = rp[1] = rp[2] = wp[0] = wp[1] = wp[2] = BAD_FD;
    handles[0]   = handles[1] = handles[2] = INVALID_HANDLE_VALUE;
    tmpin_fd     = BAD_FD;
    tmpin_name   = NULL;
    set_console_title(cmd);
    if (is_win95())
    {
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

        for (cmdp = cmd; *cmdp && isspace(*cmdp); cmdp++)
            ;
        nowait_pipe_cmd = (strnicmp(cmdp, "dir", 3) == 0)  ||
                          (strnicmp(cmdp, "type", 4) == 0);
    }
    do
    {
        if (fr)
        {
            *fr = NULL;

            /*
             * Open (parent's) input pipe in TEXT mode, which will force
             * translation of the child's CR/LF record delimiters to NL
             * and keep the dreaded ^M chars from temporarily appearing
             * in a vile buffer (ugly).
             */
            if (_pipe(rp, PIPESIZ, O_TEXT|O_NOINHERIT) == -1)
                break;

#ifdef DUP_HANDLE_BROKEN_ON_WIN2K
            if (! DuplicateHandle(GetCurrentProcess(),
                                  (HANDLE) _get_osfhandle(rp[1]),
                                  GetCurrentProcess(),
                                  handles + 1,
                                  0,
                                  TRUE,
                                  DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
            {
                break;
            }
#else
            if ((rp[2] = _dup(rp[1])) == -1)
                break;
            handles[1] = (HANDLE)_get_osfhandle(rp[2]);
#endif

            handles[2] = handles[1];
            (void) close(rp[1]);
            rp[1] = BAD_FD;
            if (! fw)
            {
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
                                     O_RDONLY|O_CREAT|O_TRUNC,
                                     _S_IWRITE|_S_IREAD)) == BAD_FD)
                {
                    break;
                }
                handles[0] = (HANDLE) _get_osfhandle(tmpin_fd);
            }
            if (! (*fr = fdopen(rp[0], "r")))
                break;
        }
        if (fw)
        {
            *fw = NULL;

            /*
             * Open (child's) output pipe in binary mode, which will
             * prevent translation of the parent's CR/LF record delimiters
             * to NL.  Apparently, many apps want those delimiters :-) .
             */
            if (_pipe(wp, PIPESIZ, O_BINARY|O_NOINHERIT) == -1)
                break;

#ifdef DUP_HANDLE_BROKEN_ON_WIN2K
            if (! DuplicateHandle(GetCurrentProcess(),
                                  (HANDLE) _get_osfhandle(wp[0]),
                                  GetCurrentProcess(),
                                  handles + 0,
                                  0,
                                  TRUE,
                                  DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
            {
                break;
            }
#else
            if ((wp[2] = _dup(wp[0])) == -1)
                break;
            handles[0] = (HANDLE)_get_osfhandle(wp[2]);
#endif

            (void) close(wp[0]);
            wp[0] = BAD_FD;
            if (! fr)
                handles[1] = handles[2] = GetStdHandle(STD_OUTPUT_HANDLE);
            if (! (*fw = fdopen(wp[1], "w")))
                break;
        }
        rc = (exec_shell(cmd,
                         handles,
                         fr != NULL  /* Child wdw hidden unless write pipe. */
                         ) == BAD_PROC_HANDLE) ? FALSE : TRUE;
        if (fw)
        {
            if (! rc)
            {
                 /* Shell process failed, put complaint in user's face. */

                putc('\n', stdout);
                printf(SHELL_ERR_MSG, get_shell());
                fflush(stdout);
            }
            CloseHandle(handles[0]);
        }
        if (fr)
        {
            if (! rc)
            {
                char  buf[200];
                DWORD dummy, len;

                /* Shell process failed, put complaint in user's buffer. */

                len = lsprintf(buf, SHELL_ERR_MSG, get_shell()) - buf;
                (void) WriteFile(handles[1], buf, len, &dummy, NULL);
                FlushFileBuffers(handles[1]);
            }
            CloseHandle(handles[1]);
            if (tmpin_fd != BAD_FD)
                close(tmpin_fd);
        }
        return (rc);
    }
    while (FALSE);

    /* If we get here -- some operation has failed.  Clean up. */

    if (wp[0] != BAD_FD)
        close(wp[0]);
    if (wp[1] != BAD_FD)
        close(wp[1]);
    if (wp[2] != BAD_FD)
        close(wp[2]);
    if (rp[0] != BAD_FD)
        close(rp[0]);
    if (rp[1] != BAD_FD)
        close(rp[1]);
    if (rp[2] != BAD_FD)
        close(rp[2]);
    if (tmpin_fd != BAD_FD)
        close(tmpin_fd);
    for (i = 0; i < 3; i++)
    {
        if (handles[i] != INVALID_HANDLE_VALUE)
            CloseHandle(handles[i]);
    }
    global_cleanup();
    return (FALSE);
}



void
w32_npclose(FILE *fp)
{
    int term_status;

    (void) fflush(fp);
    (void) fclose(fp);
    if (proc_handle != BAD_PROC_HANDLE)
    {
        (void) _cwait(&term_status, (int) proc_handle, 0);
        (void) CloseHandle(proc_handle);
        proc_handle = BAD_PROC_HANDLE;
    }
    global_cleanup();
}
