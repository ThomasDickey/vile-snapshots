/* 
 * w32pipe:  win32 clone of npopen.  
 *
 * Acknowledgments:  The techniques used in npclose() and inout_popen()
 *                   are based directly on information supplied in the
 *                   MSDN Knowledge Base.
 *
 * Caveats:  These routines work well under WinNT 4.0 in all cases and may
 *           work under Win95 if a shell other than COMMAND.COM is selected
 *           as the console shell (via either $COMSPEC or $SHELL).  An 
 *           example of such a shell is the Thompson Toolkit shell.
 *
 *           In all other Win95 cases, especially when COMMAND.COM is the
 *           console shell, the routines will either hang (OS R2) or
 *           cause command.com to attempt to access the PC's floppy drive. 
 *           Seriously.  Since most Win95 users won't be using the Thompson
 *           Toolkit shell and definitely won't be pleased with the
 *           aforementioned behavior, these routines are called only when 
 *           the global boolean "w32pipes" is set in vile.rc .  By default,
 *           "w32pipes" is set on an NT host and reset on a Win95 host.
 *
 *           These routines have not been tested under WinNT 3.51 .
 * 
 * $Header: /users/source/archives/vile.vcs/RCS/w32pipe.c,v 1.3 1998/03/14 00:12:08 tom Exp $
 */

#include <windows.h>
#include <io.h>
#include <process.h>
#include <assert.h>

#define HAVE_FCNTL_H 1

#include "estruct.h"
#include "edef.h"

#define BAD_FD          (-1)
#define BAD_PROC_HANDLE (-1)
#define SHELL_ERR_MSG   \
          "error: shell process \"%s\" failed, check COMSPEC/SHELL env vars\n" 

static DWORD console_mode;
static int   proc_handle;
static char  *shell = NULL;
static char  *shell_c = "/c";
static char  orig_title[256];

/* ------------------------------------------------------------------ */

/*
 * Need to install an event handler before spawning a child so that
 * typing ^C in the child process does not cause the waiting vile 
 * process to exit.  Don't understand why this is necessary, but it
 * is required for win95 (at least).
 */
static BOOL WINAPI
event_handler(DWORD ctrl_type)
{
    switch (ctrl_type)
    {
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            imdying(1);
        break;
    }
    return (TRUE);
}

#ifndef DISP_NTWIN

/* Temporarily setup child's console input for typical line-oriented I/O. */
void 
push_console_mode(char *shell_cmd)
{
    HANDLE hConIn = GetStdHandle(STD_INPUT_HANDLE);

    GetConsoleTitle(orig_title, sizeof(orig_title));
    SetConsoleTitle(shell_cmd);
    (void) GetConsoleMode(hConIn, &console_mode);
    (void) SetConsoleMode(hConIn, 
                  ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT);
    SetConsoleCtrlHandler(event_handler, TRUE);
}



/* Put everything back the way it was. */
void
pop_console_mode(void)
{
    SetConsoleTitle(orig_title);
    SetConsoleCtrlHandler(event_handler, FALSE);
    (void) SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), console_mode);
}

#else /* DISP_NTWIN */
#define push_console_mode(shell_cmd) /* nothing */
#define pop_console_mode() /* nothing */
#endif


static int
exec_shell(char *cmd)
{
    char *argv[4];
    char *sep = "/c";

    if (shell == 0) 
        shell = get_shell();

    if (!strcmp(shell, "/bin/sh"))
        shell_c = "-c";

    argv[0] = shell;
    argv[1] = shell_c;
    argv[2] = cmd;
    argv[3] = NULL;

    TRACE(("exec_shell %s\n", shell));
    TRACE(("shell cmd: %s\n", cmd));

    return (proc_handle = spawnvpe(_P_NOWAIT, argv[0], argv, NULL));
}



int
w32_inout_popen(FILE **fr, FILE **fw, char *cmd)
{
    int  rc, rp[2], wp[2], stdout_orig, stdin_orig, stderr_orig;

    proc_handle = BAD_PROC_HANDLE;
    stdout_orig = stdin_orig = stderr_orig = BAD_FD;
    rp[0]       = rp[1]      = wp[0]       = wp[1] = BAD_FD;
    push_console_mode(cmd);
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
            if (_pipe(rp, BUFSIZ, O_TEXT|O_NOINHERIT) == -1)
                break;
            stdout_orig = _dup(fileno(stdout));
            stderr_orig = _dup(fileno(stderr));
            if (stdout_orig == BAD_FD || stderr_orig == BAD_FD)
                break;
            if (_dup2(rp[1], fileno(stdout)) != 0)
                break;
            if (_dup2(rp[1], fileno(stderr)) != 0)
                break;
            close(rp[1]);
            rp[1] = BAD_FD;   /* Mark as unused */
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
            if (_pipe(wp, BUFSIZ, O_BINARY|O_NOINHERIT) == -1)
                break;
            if ((stdin_orig = _dup(fileno(stdin))) == -1)
                break;
            if (_dup2(wp[0], fileno(stdin)) != 0)
                break;
            close(wp[0]);
            wp[0] = BAD_FD;   /* Mark as unused */
            if (! (*fw  = fdopen(wp[1], "w")))
                break;
        }
        rc = (exec_shell(cmd) == BAD_PROC_HANDLE) ? FALSE : TRUE;
        if (fr)
        {
            if (! rc)
            {
                /* Shell process failed, put complaint in user's buffer. */

                printf(SHELL_ERR_MSG, shell);
                fflush(stdout);
            }
            if (_dup2(stdout_orig, fileno(stdout)) != 0)
            {
                /* Can't reopen stdout -- time to tidy up and die. */

                imdying(0);
            }
            if (_dup2(stderr_orig, fileno(stderr)) != 0)
            {
                /* Lost stderr -- bummer */

                imdying(0);
            }
            close(stdout_orig);
            close(stderr_orig);
        }
        if (fw)
        {
            if (! rc)
            {
                 /* Shell process failed, put complaint in user's face. */

                fputc('\n', stdout);
                printf(SHELL_ERR_MSG, shell);
                fflush(stdout);
            }
            if (_dup2(stdin_orig, fileno(stdin)) != 0)
            {
                /* Lost stdin -- no way to continue */

                imdying(0);
            }
            close(stdin_orig);
        }
        return (rc);
    }
    while (FALSE);

    /* If we get here -- some operation has failed.  Clean up. */

    if (fw)
    {
        if (wp[1] != BAD_FD)
            close(wp[1]);
        if (wp[0] != BAD_FD)
            close(wp[0]);
        if (stdin_orig != BAD_FD)
        {
            if (_dup2(stdin_orig, fileno(stdin)) != 0)
            {
                /* Lost stdin -- no way to continue */

                imdying(0);
            }
            close(stdin_orig);
        }
    }
    if (fr)
    {
        if (rp[1] != BAD_FD)
            close(rp[1]);
        if (rp[0] != BAD_FD)
            close(rp[0]);
        if (stderr_orig != BAD_FD)
        {
            if (_dup2(stderr_orig, fileno(stderr)) != 0)
            {
                /* Lost stderr -- no way to continue */

                imdying(0);
            }
            close(stderr_orig);
        }
        if (stdout_orig != BAD_FD)
        {
            if (_dup2(stdout_orig, fileno(stdout)) != 0)
            {
                /* Lost stdout -- no way to continue */

                imdying(0);
            }
            close(stdout_orig);
        }
    }
    pop_console_mode();
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
        (void) _cwait(&term_status, proc_handle, 0);
        proc_handle = BAD_PROC_HANDLE;
    }
    pop_console_mode();
}

#define     HOST_95    0
#define     HOST_NT    1
#define     HOST_UNDEF (-1)
static int  host_type = HOST_UNDEF; /* nt or 95? */

static void
set_host(void)
{
    OSVERSIONINFO info;

    info.dwOSVersionInfoSize = sizeof(info);
    GetVersionEx(&info);
    host_type = (info.dwPlatformId == VER_PLATFORM_WIN32_NT) ?
                HOST_NT : HOST_95;
}


int
is_winnt(void)
{
    if (host_type == HOST_UNDEF)
        set_host();
    return (host_type == HOST_NT);
}

int
is_win95(void)
{
    if (host_type == HOST_UNDEF)
        set_host();
    return (host_type == HOST_95);
}
