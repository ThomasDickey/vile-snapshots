/*
 * w32misc:  collection of unrelated, common win32 functions used by both
 *           the console and GUI flavors of the editor.
 *
 * $Id: w32misc.c,v 1.66 2024/07/06 22:32:20 tom Exp $
 */

#include "estruct.h"
#include "edef.h"
#include "nefunc.h"

#include <io.h>
#include <conio.h>
#include <stdio.h>
#include <errno.h>
#include <process.h>

#define CSHEXE           "csh.exe"
#define CSHEXE_LEN       (sizeof(CSHEXE) - 1)
#define HOST_95          0
#define HOST_NT          1
#define HOST_UNDEF       (-1)
#define SHEXE            "sh.exe"
#define SHEXE_LEN        (sizeof(SHEXE) - 1)
#define SHELL_C_LEN      (sizeof(" -c ") - 1)

static int host_type = HOST_UNDEF;	/* nt or 95? */
#if !DISP_NTWIN
static W32_CHAR saved_title[256];
#endif

/* ------------------------------------------------------------------ */

#if DISP_NTWIN
int
stdin_data_available(void)
{
    FILE *fp;
    int rc = 0;
    int fd1 = fileno(stdin);
    int fd2;

    if (fd1 >= 0) {
	if ((fd2 = dup(fd1)) >= 0) {
	    if ((fp = fdopen(fd2, "r")) != NULL) {
		fclose(fp);
		rc = 1;
	    }
	}
    }
    return (rc);
}

#define MAX_SIGS 6

typedef void (*SIGFUNC_TYPE) (int);

static SIGFUNC_TYPE saved_sigs[MAX_SIGS];

static void
ignore_signals(void)
{
    saved_sigs[0] = signal(SIGILL, SIG_IGN);
    saved_sigs[1] = signal(SIGFPE, SIG_IGN);
    saved_sigs[2] = signal(SIGSEGV, SIG_IGN);
    saved_sigs[3] = signal(SIGTERM, SIG_IGN);
    saved_sigs[4] = signal(SIGBREAK, SIG_IGN);
    saved_sigs[5] = signal(SIGABRT, SIG_IGN);
}

static void
restore_signals(void)
{
    (void) signal(SIGILL, saved_sigs[0]);
    (void) signal(SIGFPE, saved_sigs[1]);
    (void) signal(SIGSEGV, saved_sigs[2]);
    (void) signal(SIGTERM, saved_sigs[3]);
    (void) signal(SIGBREAK, saved_sigs[4]);
    (void) signal(SIGABRT, saved_sigs[5]);
}

#endif

static void
set_host(void)
{
    OSVERSIONINFO info;

    info.dwOSVersionInfoSize = sizeof(info);
#pragma warning(suppress: 28159)
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

/*
 * FUNCTION
 *   mk_shell_cmd_str(char *cmd, int *allocd_mem, int prepend_shc)
 *
 *   cmd         - command string to be be passed to a Win32 command
 *                 interpreter.
 *
 *   alloced_mem - Boolean, T -> returned string was allocated on heap.
 *
 *   prepend_shc - Boolean, T -> prepend "$SHELL -c" to cmd.
 *
 * DESCRIPTION
 *   If the user's shell is a unix lookalike, then a command passed to
 *   system() or CreateProcess() requires special preprocessing.
 *   This extra processing is required because the aforementioned
 *   functions pass a "raw", flat command line to the shell that is
 *   _not_ broken up into the following four canonical argv components:
 *
 *     argv[0] = name of shell
 *     argv[1] = name of shell
 *     argv[2] = -c
 *     argv[3] = cmd
 *     argv[4] = NULL
 *
 *   Put another way, a true execlp() does not exist in the win32 world and,
 *   therefore, cannot be called to effect sh -c "cmdstr".  Consequently,
 *   when a unix shell (executing under win32) receives a "raw" command line,
 *   the shell splits the raw command into words, performs its normal
 *   expansions (file globbing, variable substitution, etc.) and then
 *   removes all quote characters.  After that, the shell executes the
 *   command.  This scenario means that if the user tries the following
 *   command in vile:
 *
 *       :!echo 'word1    word2'
 *
 *   It is passed to the shell as:
 *
 *        sh -c echo 'word1    word2'
 *
 *   and is displayed by the shell as:
 *
 *        word1 word2
 *
 *   That's not a big deal, but consider this vile idiom:
 *
 *        ^X-!egrep -n 'word1 word2' *.c
 *
 *   Egrep receives the following command line from the shell:
 *
 *        egrep -n word1 word2 <glob'd file list>
 *
 *   Oops.  Word2 of the regular expression is now a filename.
 *
 * SOLUTIONS
 *   1) If user's shell is a unix lookalike and the command contains no
 *      single quote delimiters, enclose the entire command in single
 *      quotes.  This forces the shell to treat the command string
 *      as a single argument _before_ word splitting, expansions, and
 *      quote removal.  Otherwise,
 *
 *   2) If user's shell is a unix lookalike, enclose the command string in
 *      double quotes and escape every nonquoted double quote within the
 *      original string.  This is the same principle as 1) above, but uses
 *      a nestable delimiter.  This solution isn't foolproof.  Consider:
 *
 *          ^X-!echo '[#@$*]' \"special\" word
 *
 *      will be read into the error buffer as:
 *
 *          [#@$*] special word
 *
 *      This could be worked around by preceding a leading \" token with '
 *      and appending ' to its closing delimiter.  But that creates its
 *      own set of side effects.
 *
 * CAVEATS
 *   The workarounds are inappropriate for csh (variants) which don't
 *   support nested quotes.
 *
 * RETURNS
 *   Pointer to possibly modified string.  If modified, the command string
 *   was created on the heap and must be free'd by the client.  If
 *   storage can't be allocated, NULL is returned.
 */

char *
mk_shell_cmd_str(const char *cmd, int *allocd_mem, int prepend_shc)
{
    int alloc_len;
    int bourne_shell;		/* Boolean, T if user's shell has appearances
				 * of a Unix lookalike bourne shell (e.g.,
				 * sh, ksh, bash).
				 */
    size_t len;
    char *out_str, *cp, *shell, *shell_c = "/c";

    shell = get_shell();
    len = strlen(shell);
    bourne_shell = (len >= 2 &&
		    toLower(shell[len - 2]) == 's' &&
		    toLower(shell[len - 1]) == 'h')
	||
	(len >= SHEXE_LEN &&
	 stricmp(shell + len - SHEXE_LEN, SHEXE) == 0);
    if (bourne_shell) {
	shell_c = "-c";

	/* Now check for csh lookalike. */
	bourne_shell = !(
			    (len >= 3 &&
			     toLower(shell[len - 3]) == 'c')
			    ||
			    (len >= CSHEXE_LEN &&
			     stricmp(shell + len - CSHEXE_LEN, CSHEXE) == 0)
	    );
    }
    if (!bourne_shell) {
	/*
	 * MS-DOS shell or csh.  Do not bother quoting user's command
	 * string, since the former is oblivious to the notion of a unix
	 * shell's argument quoting and the latter does not support nested
	 * double quotes.
	 */

	if (prepend_shc) {
	    alloc_len = (int) (strlen(cmd) + strlen(shell) + SHELL_C_LEN + 1);
	    if ((out_str = typeallocn(char, alloc_len)) == NULL)
		  return (out_str);
	    *allocd_mem = TRUE;
	    sprintf(out_str, "%s %s %s", shell, shell_c, cmd);
	    return (out_str);
	} else {
	    *allocd_mem = FALSE;
	    return (char *) (cmd);
	}
    }

    /* Else apply solutions 1-2 above. */
    alloc_len = (int) strlen(cmd) * 2;	/* worst case -- every cmd byte quoted */
    if (prepend_shc)
	alloc_len += (int) strlen(shell) + SHELL_C_LEN;
    alloc_len += 3;		/* terminating nul + 2 quote chars     */
    if ((out_str = typeallocn(char, alloc_len)) == NULL) {
	errno = ENOMEM;
	return (out_str);
    }
    *allocd_mem = TRUE;

    cp = out_str;
    if (prepend_shc)
	cp += sprintf(cp, "%s %s ", shell, shell_c);
    if (strchr(cmd, '\'') == NULL) {
	/* No single quotes in command string.  Solution #1. */

	sprintf(cp, "'%s'", cmd);
	return (out_str);
    }

    /* Solution #2. */
    *cp++ = '"';
    while (*cmd) {
	if (*cmd == '\\') {
	    *cp++ = *cmd++;
	    if (*cmd) {
		/* Any quoted char is immune to further quoting. */

		*cp++ = *cmd++;
	    }
	} else if (*cmd != '"')
	    *cp++ = *cmd++;
	else {
	    /* bare '"' */

	    *cp++ = '\\';
	    *cp++ = *cmd++;
	}
    }
    *cp++ = '"';
    *cp = '\0';
    return (out_str);
}

/*
 * Semi-generic CreateProcess(). Refer to the w32_system()
 * DESCRIPTION below for the rationale behind "no_wait" argument.
 */
int
w32_CreateProcess(char *cmd, int no_wait)
{
    int rc = -1;
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    W32_CHAR *actual;

    TRACE((T_CALLED "w32_CreateProcess(%s, %d)\n", cmd, no_wait));

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!no_wait) {
	/* command requires a shell, so hookup console I/O */

	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    }

    if ((actual = w32_charstring(cmd)) != 0) {
	if (CreateProcess(NULL,
			  actual,
			  NULL,
			  NULL,
			  !no_wait,	/* Inherit handles */
			  0,
			  NULL,
			  NULL,
			  &si,
			  &pi)) {
	    /* Success */
	    if (!no_wait) {
		/* wait for shell process to exit */
		rc = w32_wait_handle(pi.hProcess);
	    }
	    (void) CloseHandle(pi.hProcess);
	    (void) CloseHandle(pi.hThread);
	} else {
	    /* Bummer */

	    mlforce("[unable to create win32 process]");
	}
	free(actual);
    } else {
	no_memory("w32_CreateProcess");
    }
    returnCode(rc);
}

/*
 * FUNCTION
 *   w32_system(const char *cmd)
 *
 *   cmd - command string to be be passed to a Win32 command interpreter.
 *
 * DESCRIPTION
 *   Executes our version of the system() call, taking care to ensure that
 *   the user's command string is properly quoted if get_shell() points to a
 *   bourne shell clone.  We use our version of system() rather than the
 *   vendor's so that vile's users may redefine their shell via the editor's
 *   $shell state variable.  If we didn't add this additional layer of
 *   indirection, users would be at the mercy of whatever env var the C
 *   compiler's runtime library chose to reference from within system()--in
 *   the case of the MS CRT, that would be $COMSPEC.
 *
 *   As an additional feature, "cmd" may be executed without running
 *   as a shell subprocess if the the first token of the command string
 *   is "start " (useful for launching windows apps that don't need a shell).
 *
 * RETURNS
 *   If memory allocation fails, -1.
 *   Else, whatever system() returns.
 */

int
w32_system(const char *cmd)
{
    char *cmdstr;
    int no_shell, freestr, rc;

    TRACE(("w32_system(%s)\n", cmd));

    no_shell = W32_SKIP_SHELL(cmd);
    if (no_shell) {
	/*
	 * Must strip off "start " prefix from command because this
	 * idiom is supported by Win95, but not by WinNT.
	 */

	if ((cmdstr = typeallocn(char, strlen(cmd) + 1)) == NULL) {
	    (void) no_memory("w32_system");
	    return (-1);
	}
	strcpy(cmdstr, cmd + W32_START_STR_LEN);
	freestr = TRUE;
    } else if ((cmdstr = mk_shell_cmd_str(cmd, &freestr, TRUE)) == NULL) {
	/* heap exhausted! */

	(void) no_memory("w32_system");
	return (-1);
    }
    set_console_title(cmd);
    rc = w32_CreateProcess(cmdstr, no_shell);
    if (freestr)
	free(cmdstr);
    restore_console_title();
    return (rc);
}

#if DISP_NTWIN

static int
get_console_handles(STARTUPINFO * psi, SECURITY_ATTRIBUTES * psa)
{
    CONSOLE_CURSOR_INFO cci;
    char shell[NFILEN];

    psi->hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    psi->dwFlags = STARTF_USESTDHANDLES;

    /* If it's possible to fetch data via hStdOutput, assume all is well. */
    if (GetConsoleCursorInfo(psi->hStdOutput, &cci)) {
	psi->hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	psi->hStdError = GetStdHandle(STD_ERROR_HANDLE);
	return (TRUE);
    }

    /*
     * Else assume that winvile was started from a shell and that stdout
     * and stdin (and maybe stderr) have been redirected to handles that
     * can't be accessed by the parent process.  This is known to occur
     * with the CYGWIN shell(s) on a WinNT host.  In this case, workaround
     * these issues:
     *
     * Numero Uno:  the stdin and stdout handles returned by GetStdHandle()
     *              are worthless.  Workaround:  get the real handles by
     *              opening CONIN$ and CONOUT$.  We don't look for a stderr
     *              handle because I couldn't find any mention of CONERR$
     *              in MSDN.  Additionally, the CYGWIN shells will create
     *              stderr if it doesn't exist.
     *
     * Numero Dos:  a dynamically created console in the context of a CYGWIN
     *              shell sets its foreground and background colors to the
     *              same value (usually black).  Not so easy to read the
     *              text :-( . Workaround: force contrasting colors (white
     *              text on black bgrnd).  Someday we ought to let the users
     *              choose the colors via a win32-specific mode.
     *
     * And it should be noted that the above techniques do not work on
     * a Win2K host using the latest version of bash.  Sigh.  So, if you
     * use bash as your shell, invoke winvile from the command line
     * and type:
     *
     *    :sh
     *
     * or
     *
     *    :!bash
     *
     * the spawned bash shell will hang.  No known workaround.  Does not
     * occur if winvile is launched by windows explorer.
     */
    if ((psi->hStdInput = CreateFile(W32_STRING("CONIN$"),
				     GENERIC_READ,
				     FILE_SHARE_READ,
				     psa,
				     OPEN_EXISTING,
				     FILE_ATTRIBUTE_NORMAL,
				     NULL)) == INVALID_HANDLE_VALUE) {
	mlforce("[std input handle creation failed]");
	return (FALSE);
    }
    if ((psi->hStdOutput = CreateFile(W32_STRING("CONOUT$"),
				      GENERIC_WRITE,
				      FILE_SHARE_WRITE,
				      psa,
				      OPEN_EXISTING,
				      FILE_ATTRIBUTE_NORMAL,
				      NULL)) == INVALID_HANDLE_VALUE) {
	mlforce("[std output handle creation failed]");
	return (FALSE);
    }

    /*
     * set some value for stderr...this seems to be important for
     * cygwin version circa 1.5.xx (so that error messages appear in the
     * spawned console when winvile is invoked from bash command line).
     *
     * What to choose depends on the user's $shell .
     */
    strcpy(shell, get_shell());
    (void) mklower(shell);
    if (strstr(shell, "cmd") || strstr(shell, "command"))
	psi->hStdError = psi->hStdOutput;
    else
	psi->hStdError = GetStdHandle(STD_ERROR_HANDLE);

    psi->dwFlags |= STARTF_USEFILLATTRIBUTE;
    psi->dwFillAttribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    return (TRUE);
}

/*
 * FUNCTION
 *   w32_system_winvile(const char *cmd, int *pressret)
 *
 *   cmd       - command string to be be passed to a Win32 command interpreter.
 *
 *   *pressret - Boolean, T -> display prompt and wait for response.  Value
 *               usually read-only, but will be set if the user's command
 *               is prefixed with W32_START_STR
 *
 * DESCRIPTION
 *   Executes a system() call in the context of a Win32 GUI application,
 *   taking care to ensure that the user's command string is properly
 *   quoted if get_shell() points to a bourne shell clone.
 *
 *   "In the context of a Win32 GUI application" means that:
 *
 *   a) the GUI requires explicit console allocation prior to exec'ing
 *      "cmd", and
 *   b) said console stays "up" until explicitly dismissed by the user if
 *      "*pressret" is TRUE.
 *
 * ACKNOWLEDGMENTS
 *   I had no idea a Win32 GUI app could exec a console command until I
 *   browsed the win32 gvim code.
 *
 * RETURNS
 *   If memory/console allocation fails, -1.
 *   Else whatever the executed command returns.
 */

int
w32_system_winvile(const char *cmd, int *pressret)
{
#define PRESS_ANY_KEY "\n[Press any key to continue]"

    char *cmdstr;
    HWND hwnd;
    int no_shell = W32_SKIP_SHELL(cmd);
    int freestr;
    int close_disabled = FALSE;
    int rc = -1;
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa;
    STARTUPINFO si;

    W32_CHAR *w32_cmd = w32_charstring(cmd);
    W32_CHAR *w32_cmdstr = 0;

    TRACE((T_CALLED "w32_system_winvile(%s)\n", cmd));

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);

    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);

    if (no_shell) {
	/*
	 * Must strip off "start " prefix from command because this
	 * idiom is supported by Win95, but not by WinNT.
	 */

	if ((cmdstr = typeallocn(char, strlen(cmd) + 1)) == NULL) {
	    (void) no_memory("w32_system_winvile");
	    returnCode(rc);
	}
	strcpy(cmdstr, cmd + W32_START_STR_LEN);
	freestr = TRUE;
	*pressret = FALSE;	/* Not waiting for the launched cmd to exit. */
    } else {
	sa.bInheritHandle = TRUE;
	if ((cmdstr = mk_shell_cmd_str(cmd, &freestr, TRUE)) == NULL) {
	    /* heap exhausted! */

	    (void) no_memory("w32_system_winvile");
	    returnCode(rc);
	}
	if (!AllocConsole()) {
	    if (freestr)
		free(cmdstr);
	    mlforce("[console creation failed]");
	    returnCode(rc);
	}
	if (!get_console_handles(&si, &sa)) {
	    (void) FreeConsole();
	    if (freestr)
		free(cmdstr);
	    returnCode(rc);
	}
	SetConsoleTitle(w32_cmd);

	/* don't let signal in dynamic console kill winvile */
	ignore_signals();

	/*
	 * If the spawned console's close button is pressed, both the
	 * console _and_ winvile are killed.  Not good.  Try to disable
	 * console close button before shell process is started, while the
	 * console title is still intact.  Once the shell starts running,
	 * it is free to change the console's title, and many will do so
	 * (e.g., bash).
	 */
	Sleep(0);		/* yield processor so GDI can create console frame */
	if ((hwnd = FindWindow(NULL, w32_cmd)) != NULL) {
	    /*
	     * Disable console close button using code borrowed from
	     * Charles Petzold.
	     */

	    (void) EnableMenuItem(GetSystemMenu(hwnd, FALSE),
				  SC_CLOSE,
				  MF_GRAYED);
	    close_disabled = TRUE;

	    /*
	     * On a Win2K host, the console is often created as a
	     * background window, which means that the user must press
	     * ALT+TAB to bring it to the foreground to interact with the
	     * shell.  Ugh.  This call to SetForegroundWindow() should
	     * work, even on 2K / 98.
	     */
	    (void) SetForegroundWindow(hwnd);
	}
    }
    if ((w32_cmdstr = w32_charstring(cmdstr)) == 0) {
	(void) no_memory("w32_system_winvile");
	returnCode(rc);
    }
    if (CreateProcess(NULL,
		      w32_cmdstr,
		      &sa,
		      &sa,
		      !no_shell,	/* Inherit handles */
		      0,
		      NULL,
		      NULL,
		      &si,
		      &pi)) {
	/* Success */

	DWORD dummy;
	INPUT_RECORD ir;

	if (!close_disabled) {
	    int i;

	    /*
	     * If the spawned console's close button is pressed, both the
	     * console _and_ winvile are killed.  Not good.
	     */
	    for (i = 0; i < 5; i++) {
		if ((hwnd = FindWindow(NULL, w32_cmd)) != NULL) {
		    (void) EnableMenuItem(GetSystemMenu(hwnd, FALSE),
					  SC_CLOSE,
					  MF_GRAYED);
		    (void) SetForegroundWindow(hwnd);
		    break;
		}
		Sleep(200);
	    }
	}
	if (!no_shell) {
	    rc = w32_wait_handle(pi.hProcess);
	    restore_signals();
	    if (*pressret) {
		if (!WriteFile(si.hStdOutput,
			       PRESS_ANY_KEY,
			       sizeof(PRESS_ANY_KEY) - 1,
			       &dummy,
			       NULL)) {
		    mlforce("[dynamic console write failed]");
		    rc = -1;
		} else {
		    for_ever
		    {
			/* Wait for a single key of input from user. */

			if (!ReadConsoleInput(si.hStdInput, &ir, 1, &dummy)) {
			    mlforce("[dynamic console read failed]");
			    rc = -1;
			    break;
			}
			if (ir.EventType == KEY_EVENT &&
			    ir.Event.KeyEvent.bKeyDown) {
			    break;
			}
		    }
		}
	    }
	} else {
	    rc = 0;
	}
	(void) CloseHandle(pi.hProcess);
	(void) CloseHandle(pi.hThread);
    } else {
	/* Bummer */

	mlforce("[unable to create Win32 process]");
    }
    free(w32_cmd);
    free(w32_cmdstr);
    if (freestr)
	free(cmdstr);
    if (!no_shell)
	FreeConsole();
    returnCode(rc);
}
#endif /* DISP_NTWIN */

/*
 * FUNCTION
 *   w32_keybrd_reopen(int pressret)
 *
 *   pressret - Boolean, T -> display prompt and wait for response
 *
 * DESCRIPTION
 *   This is essentially the Win32 equivalent of the pressreturn() function
 *   in spawn.c, but differs in that it reopens the console keyboard _after_
 *   prompting the user to press return.  Order is important IF the user has
 *   configured his/her dos box such that the buffer size exceeds the
 *   window size.  In that scenario, if the ntconio.c routines gained
 *   control (via term.kopen) before the prompt, then the output of the
 *   previous shell command (e.g., :!dir) is immediately thrown away
 *   due to a screen context switch and the user has no chance to read the
 *   shell output.
 *
 *   This function prevents that scenario from occurring.
 *
 * APPLIES TO
 *   W32 console vile only.
 *
 * RETURNS
 *   None
 */

void
w32_keybrd_reopen(int pressret)
{
#if DISP_NTCONS
    int c;

    if (pressret) {
	vl_fputs("[Press return to continue]", stdout);
	fflush(stdout);

	/* loop for a CR, a space, or a : to do another named command */
	while ((c = _getch()) != '\r' &&
	       c != '\n' &&
	       c != ' ' &&
	       !ABORTED(c)) {
	    if (DefaultKeyBinding(c) == &f_namedcmd) {
		unkeystroke(c);
		break;
	    }
	}
	vl_putc('\n', stdout);
    }
    term.kopen();
    kbd_erase_to_end(0);
#else
    (void) pressret;
#endif
}

#if DISP_NTCONS
void
w32_set_console_title(const char *title)
{
    W32_CHAR *actual = w32_charstring(title);
    if (actual != 0) {
	SetConsoleTitle(actual);
	free(actual);
    }
}
#endif

/*
 * The code in ntconio.c that saves and restores console titles
 * didn't work reliably for pipe or shell operations.  It's moved here
 * now and called via the w32_system() wrapper or the w32pipe module.
 */
void
set_console_title(const char *title)
{
    (void) title;
#if !DISP_NTWIN
    GetConsoleTitle(saved_title, sizeof(saved_title));
    w32_set_console_title(title);
#endif
}

void
restore_console_title(void)
{
#if !DISP_NTWIN
    SetConsoleTitle(saved_title);
#endif
}

/*
 * FUNCTION
 *   fmt_win32_error(ULONG errcode, char **buf, ULONG buflen)
 *
 *   errcode - win32 error code for which message applies.  If errcode is
 *             W32_SYS_ERROR, GetLastError() will be invoked to obtain the
 *             error code.
 *
 *   buf     - indirect pointer to buf to receive formatted message.  If *buf
 *             is NULL, the buffer is allocated on behalf of the client and
 *             must be free'd using LocalFree().
 *
 *   buflen  - length of buffer (specify 0 if *buf is NULL).
 *
 * DESCRIPTION
 *   Format system error reported by Win32 API.
 *
 * RETURNS
 *   *buf
 */

char *
fmt_win32_error(ULONG errcode, char **buf, ULONG buflen)
{
    W32_CHAR *buffer = 0;
    int flags = FORMAT_MESSAGE_FROM_SYSTEM;

    if (*buf) {
	buffer = typeallocn(W32_CHAR, buflen);
    } else {
	flags |= FORMAT_MESSAGE_ALLOCATE_BUFFER;
    }
    if (buffer != NULL) {
	FormatMessage(flags,
		      NULL,
		      errcode == W32_SYS_ERROR ? GetLastError() : errcode,
		      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),	/* dflt language */
		      buffer,
		      buflen,
		      NULL);
	if (*buf) {
	    char *formatted = asc_charstring(buffer);
	    vl_strncpy(*buf, formatted, buflen);
	    free(formatted);
	    free(buffer);
	}
    }
    return (*buf);
}

W32_CHAR *
w32_prognam(void)
{
    static W32_CHAR *result;

    if (result == 0)
	result = w32_charstring(prognam);

    return result;
}

int
w32_message_box(HWND hwnd, const char *message, int code)
{
    int rc;
    W32_CHAR *buf = w32_charstring(message);

    rc = MessageBox(hwnd, buf, w32_prognam(), code);
    free(buf);
    return (rc);
}

/*
 * FUNCTION
 *   disp_win32_error(ULONG errcode, void *hwnd)
 *
 *   errcode - win32 error code for which message applies.  If errcode is
 *             W32_SYS_ERROR, GetLastError() will be invoked to obtain the
 *             error code.
 *
 *   hwnd    - specifies the window handle argument to MessageBox (can be NULL).
 *
 * DESCRIPTION
 *   Format system error reported by Win32 API and display in message box.
 *
 * RETURNS
 *   None
 */

void
disp_win32_error(ULONG errcode, void *hwnd)
{
    char *buf = NULL;

    fmt_win32_error(errcode, &buf, 0);
    w32_message_box(hwnd, buf, MB_OK | MB_ICONSTOP);
    LocalFree(buf);
}

#if DISP_NTWIN
/*
 * FUNCTION
 *   parse_font_str(const char *fontstr, FONTSTR_OPTIONS *results)
 *
 *   fontstr - a font specification string, see DESCRIPTION below.
 *
 *   results - Pointer to structure that returns data from a successfully
 *             parsed font string.
 *
 * DESCRIPTION
 *   Turns a font specification string into a LOGFONT data structure.
 *   Specification syntax is as follows:
 *
 *     <font>  :== [<face>,]<size>[,<style>]
 *
 *     <face>  :== font-name
 *     <size>  :== point size (integer)
 *     <style> :== { bold | italic | bold-italic }
 *
 *     ex:    Letter Gothic,8
 *     ex:    r_ansi,8,bold
 *
 *     Note 1:  If <style> unspecified, "normal" is assumed.
 *     Note 2:  if <face> contains a comma it should be escaped with '\'.
 *     Note 3:  if <face> is omitted, the current font is modified.
 *
 * RETURNS
 *   Boolean, T -> font syntax ok, else bogus syntax
 */

int
parse_font_str(const char *fontstr, FONTSTR_OPTIONS * results)
{
    const char *cp, *tmp;
    char *endnum, *facep;
    ULONG size;

    memset(results, 0, sizeof(*results));
    size = 0;
    cp = skip_cblanks(fontstr);

    /* Up first is either a font face or font size. */
    if (isDigit(*cp)) {
	errno = 0;
	size = strtoul(cp, &endnum, 10);
	if (errno != 0)
	    return (FALSE);
	tmp = skip_cblanks(endnum);
	if (*tmp != '\0') {
	    if (*tmp != ',') {
		/* Not a 100% integer value, assume this is a font face. */
		size = 0;
	    } else
		cp = tmp;	/* Valid point size. */
	} else
	    cp = tmp;		/* Only a point size specified, nothing left. */
    }
    if (size == 0) {
	/* this must be a font face */

	facep = results->face;
	while (*cp) {
	    if (*cp == ',') {
		cp++;
		break;
	    } else if (*cp == '\\' && cp[1] == ',') {
		*facep++ = ',';
		cp += 2;
	    } else
		*facep++ = *cp++;
	}
	*facep = '\0';
	if (results->face[0] == '\0' || *cp == '\0') {
	    return (FALSE);
	} else {
	    /* Now pick up non-optional font size (that follows face). */

	    errno = 0;
	    size = strtoul(cp, &endnum, 10);
	    if (errno != 0 || size == 0)
		return (FALSE);
	    cp = endnum;
	}
    }

    /* Now look for optional font style. */
    cp = skip_cblanks(cp);

    /* At this point, there are two allowable states:  delimiter or EOS. */
    if (*cp) {
	if (*cp++ == ',') {
	    cp = skip_cblanks(cp);
	    if (strncmp(cp, "bold-italic", sizeof("bold-italic") - 1) == 0)
		results->bold = results->italic = TRUE;
	    else if (strncmp(cp, "italic", sizeof("italic") - 1) == 0)
		results->italic = TRUE;
	    else if (strncmp(cp, "bold", sizeof("bold") - 1) == 0)
		results->bold = TRUE;
	    else
		return (FALSE);
	} else
	    return (FALSE);
    }
    results->size = size;
    TRACE(("parse_font_str(face=\"%s\", size=%ld, style=\"%s%s\")\n",
	   results->face,
	   results->size,
	   results->bold ? "Bold" : "",
	   results->italic ? "Italic" : ""));
    return (TRUE);
}
#endif /* DISP_NTWIN */

/* return current window title in dynamic buffer */
char *
w32_wdw_title(void)
{
    static char *buffer;
    static W32_CHAR *buf;
    static DWORD bufsize;
    int nchars = 0;
    char *result = 0;

    if (!buf) {
	bufsize = 128;
	buf = typeallocn(W32_CHAR, bufsize);
    }
    while (buf != 0) {
#if DISP_NTWIN
	nchars = GetWindowText(winvile_hwnd(), buf, bufsize);
#else
	nchars = GetConsoleTitle(buf, bufsize);
#endif
	if (nchars >= ((int) bufsize - 1)) {
	    /* Enlarge buffer and try again. */

	    bufsize *= 2;
	    safe_typereallocn(W32_CHAR, buf, bufsize);
	} else {
	    break;
	}
    }
    if (nchars && buf) {
	FreeIfNeeded(result);
	buffer = asc_charstring(buf);
	result = buffer;
    } else {
	result = error_val;
    }
    return (result);
}

/*
 * Delete current text selection and optionally copy same to Windows
 * clipboard.
 */
int
w32_del_selection(int copy_to_cbrd)
{
    AREGION delrp;
    MARK savedot;
    REGIONSHAPE shape;
    int status;

    /* first, go to beginning of selection, if it exists. */
    if (!sel_motion(FALSE, FALSE))
	return (FALSE);

    if (!sel_yank(0))		/* copy selection to unnamed register */
	return (FALSE);

    if (!get_selection_buffer_and_region(&delrp)) {
	/* no selected region.  hmmm -- should not happen */

	return (FALSE);
    }
    shape = delrp.ar_shape;
    savedot = DOT;

    /*
     * Region to delete is now accessible via delrp.  Mimic the code
     * executed by operdel().  The actual code executed depends upon
     * whether or not the region is rectangular.
     */
    if (shape == rgn_RECTANGLE) {
	DORGNLINES dorgn;
	int save;

	save = FALSE;
	dorgn = get_do_lines_rgn();
	DOT = delrp.ar_region.r_orig;

	/* setup dorgn() aka do_lines_in_region() */
	haveregion = &delrp.ar_region;
	regionshape = shape;
	status = dorgn(kill_line, &save, FALSE);
	haveregion = NULL;
    } else {
	lines_deleted = 0;
	DOT = delrp.ar_region.r_orig;
	status = ldel_bytes(delrp.ar_region.r_size, FALSE);
#if OPT_SELECTIONS
	find_release_attr(curbp, &delrp.ar_region);
#endif
    }
    restore_dot(savedot);
    if (status) {
	if (copy_to_cbrd) {
	    /*
	     * cbrdcpy_unnamed() reports number of lines copied, which is
	     * same as number of lines deleted.
	     */
	    status = cbrdcpy_unnamed(FALSE, FALSE);
	} else {
	    /* No data copied to clipboard, report number of lines deleted. */

	    status = TRUE;
	    if (shape == rgn_RECTANGLE) {
		if (do_report(klines + (kchars != 0))) {
		    mlwrite("[%d line%s, %d character%s killed]",
			    klines,
			    PLURAL(klines),
			    kchars,
			    PLURAL(kchars));
		}
	    } else {
		if (do_report(lines_deleted))
		    mlwrite("[%d lines deleted]", lines_deleted);
	    }
	}
    }
    return (status);
}

#ifdef UNICODE
#define PASS_WCHAR 1
#endif

/* slam a string into the editor's input buffer */
int
w32_keybrd_write(W32_CHAR * data)
{
#if DISP_NTCONS
    HANDLE hstdin;
    INPUT_RECORD ir;
    DWORD unused;
#else
    HANDLE hwvile;
#endif
    int rc;
#ifndef PASS_WCHAR
    int n, nbytes;
    UCHAR buffer[8];
#endif

    rc = TRUE;
#if DISP_NTCONS
    hstdin = GetStdHandle(STD_INPUT_HANDLE);
    memset(&ir, 0, sizeof(ir));
    ir.EventType = KEY_EVENT;
    ir.Event.KeyEvent.bKeyDown = TRUE;
#else
    hwvile = winvile_hwnd();
#endif

    while (*data && rc) {
#ifdef PASS_WCHAR
#if DISP_NTCONS
	ir.Event.KeyEvent.uChar.UnicodeChar = *data;
	rc = WriteConsoleInput(hstdin, &ir, 1, &unused);
#else
	rc = PostMessage(hwvile, WM_CHAR, *data, 0);
#endif
#else /* !PASS_WCHAR */
#ifdef UNICODE
	nbytes = vl_conv_to_utf8(buffer, *data, 8);
#else
	buffer[0] = *data;
	nbytes = 1;
#endif
	for (n = 0; n < nbytes; ++n) {
#if DISP_NTCONS
	    ir.Event.KeyEvent.uChar.AsciiChar = buffer[n];
	    rc = WriteConsoleInput(hstdin, &ir, 1, &unused);
#else
	    rc = PostMessage(hwvile, WM_CHAR, buffer[n], 0);
#endif
	}
#endif /* PASS_WCHAR */
	data++;
    }
    return (rc);
}

#if DISP_NTWIN

/* Center a child window (usually a dialog box) over a parent. */
void
w32_center_window(HWND child_hwnd, HWND parent_hwnd)
{
    int w, h;
    RECT crect;			/* child rect */
    RECT prect;			/* parent rect */

    GetWindowRect(parent_hwnd, &prect);
    GetWindowRect(child_hwnd, &crect);
    w = crect.right - crect.left;
    h = crect.bottom - crect.top;
    MoveWindow(child_hwnd,
	       prect.left + ((prect.right - prect.left) / 2 - w / 2),
	       prect.top + ((prect.bottom - prect.top) / 2 - h / 2),
	       w,
	       h,
	       TRUE);
}
#endif

/*
 * If necessary, add/remove World/Everyone write permission to/from a file acl.
 *
 * Much of this function's logic is cribbed from here:
 *
 *     http://www.devx.com/cplus/Article/16711/1954?pf=true
 *
 * This function is not 100% robust.  It will not handle the case of:
 *
 *     + a file that does not include World/Everyone ACE
 *     + a file that includes one or more DENIED World/Everyone ACEs
 *
 * However, this function provides enough logic to make an NTFS-based,
 * cygwin-generated, readonly file, writable.  Which is good enough for
 * the purpose at hand.
 */
static int
add_remove_write_acl(const char *filename, int add_acl, DWORD * prev_access_mask)
{
#define WRITABLE_MASK (FILE_WRITE_DATA | FILE_APPEND_DATA)

    BOOL bDaclPresent, bDaclDefaulted;
    W32_CHAR *w32_bslfn = 0;
    char *bslfn, *msg = NULL;
    DWORD dwSizeNeeded;
    int i, rc = FALSE;
    PSID pAceSID, pWorldSID;
    PACL pacl = NULL;
    ACCESS_ALLOWED_ACE *pAllowed;
    BYTE *pSecDescriptorBuf = 0;

    SID_IDENTIFIER_AUTHORITY SIDAuthWorld =
    {
	SECURITY_WORLD_SID_AUTHORITY
    };

    /* does the file exist? */
    bslfn = sl_to_bsl(filename);
    if (access(bslfn, 0) != 0)
	return (rc);
    if ((w32_bslfn = w32_charstring(bslfn)) == 0)
	return (rc);

    dwSizeNeeded = 0;
    (void) GetFileSecurity(w32_bslfn,
			   DACL_SECURITY_INFORMATION,
			   NULL,
			   0,
			   &dwSizeNeeded);

    if (dwSizeNeeded == 0) {
	fmt_win32_error(W32_SYS_ERROR, &msg, 0);
	mlforce("[GetFileSecurity: %s]", mktrimmed(msg));
	LocalFree(msg);
    } else if ((pSecDescriptorBuf = malloc(sizeof(BYTE) * dwSizeNeeded)) == NULL) {
	rc = no_memory("add_remove_write_acl");
    } else if (!GetFileSecurity(w32_bslfn,
				DACL_SECURITY_INFORMATION,
				pSecDescriptorBuf,
				dwSizeNeeded,
				&dwSizeNeeded)) {
	fmt_win32_error(W32_SYS_ERROR, &msg, 0);
	mlforce("[GetFileSecurity: %s]", mktrimmed(msg));
	LocalFree(msg);
    }

    /* Get DACL from Security Descriptor */
    else if (!GetSecurityDescriptorDacl((SECURITY_DESCRIPTOR *) pSecDescriptorBuf,
					&bDaclPresent,
					&pacl,
					&bDaclDefaulted)) {
	fmt_win32_error(W32_SYS_ERROR, &msg, 0);
	mlforce("[GetSecurityDescriptorDacl: %s]", mktrimmed(msg));
	LocalFree(msg);
    }

    /* Check if DACL present in security descriptor */
    else if (!bDaclPresent || pacl == NULL) {
	/*
	 * Nothing to manipulate, perhaps a non-NTFS file.  Regardless, a
	 * NULL discretionary ACL implicitly allows all access to an object
	 * (sez docu for GetSecurityDescriptorDacl).
	 */
	;
    }

    /* Create a well-known SID for "Everyone/World" (code courtesy of MSDN). */
    else if (!AllocateAndInitializeSid(&SIDAuthWorld,
				       1,
				       SECURITY_WORLD_RID,
				       0, 0, 0, 0, 0, 0, 0,
				       &pWorldSID)) {
	fmt_win32_error(W32_SYS_ERROR, &msg, 0);
	mlforce("[AllocateAndInitializeSid: %s]", mktrimmed(msg));
	LocalFree(msg);
    } else {
	for (i = 0, pAllowed = NULL; i < pacl->AceCount; i++) {
	    ACE_HEADER *phdr;

	    if (GetAce(pacl, i, (LPVOID *) & phdr)) {
		if (phdr->AceType == ACCESS_ALLOWED_ACE_TYPE) {
		    pAllowed = (ACCESS_ALLOWED_ACE *) phdr;
		    pAceSID = (SID *) & (pAllowed->SidStart);
		    if (EqualSid(pWorldSID, pAceSID))
			break;
		}
	    }
	}
	if (i < pacl->AceCount) {
	    /* success */

	    int mkchange = FALSE;

	    if (add_acl) {
		if ((pAllowed->Mask & WRITABLE_MASK) != WRITABLE_MASK) {
		    /* world ACE does not have "write" permissions...add them */

		    *prev_access_mask = pAllowed->Mask;
		    mkchange = TRUE;
		    pAllowed->Mask |= FILE_GENERIC_WRITE;
		}
	    } else {
		/* restore previous world ACE mask for this file */

		pAllowed->Mask = *prev_access_mask;
		mkchange = TRUE;
	    }
	    if (mkchange) {
		rc = SetFileSecurity(w32_bslfn,
				     DACL_SECURITY_INFORMATION,
				     pSecDescriptorBuf);

		if (!rc) {
		    DWORD err = GetLastError();
		    if (!(add_acl && err == ERROR_ACCESS_DENIED)) {
			fmt_win32_error(err, &msg, 0);
			mlforce("[SetFileSecurity: %s]", mktrimmed(msg));
			LocalFree(msg);
		    }
		    /*
		     * Else tried adding write permissions and privs are
		     * insufficient.  Report no error...whatever action the
		     * client is attempting will soon fail and an error
		     * will be reported at that time.
		     */
		}
	    }
	}
	/* Else no World ACE, so add it...someday...maybe...when really bored? */

	FreeSid(pWorldSID);
    }
    FreeIfNeeded(w32_bslfn);
    FreeIfNeeded(pSecDescriptorBuf);
    return (rc);

#undef WRITABLE_MASK
}

/*
 * If necessary, modify a file's World/Everyone ACE to include file "write"
 * permissions.  This function returns TRUE if an ACE change was made.  In
 * addition, if the ACE is changed, "old_mask_access" is initialized with the
 * prior value of the file's World/Everyone ACCESS_MASK.  This value is used
 * by w32_remove_write_acl() to restore the file's original ACCESS_MASK.
 */
int
w32_add_write_acl(const char *filename, ULONG * old_access_mask)
{
    if (is_win95())
	return (FALSE);		/* no such win9x feature */
    else
	return (add_remove_write_acl(filename, TRUE, old_access_mask));
}

/*
 * Bookend of w32_add_write_acl().  "orig_access_mask" is the value used to
 * restore the file's World/Everyone ACE ACCESS_MASK to its original value.
 */
int
w32_remove_write_acl(const char *filename, ULONG orig_access_mask)
{
    if (is_win95())
	return (FALSE);		/* no such win9x feature */
    else
	return (add_remove_write_acl(filename, FALSE, &orig_access_mask));
}

#ifdef UNICODE
/*
 * Use this via "w32_charstring()" to convert literal ASCII strings to Unicode.
 */
W32_CHAR *
w32_ansi_to_ucs2(const char *source, int sourcelen)
{
    W32_CHAR *target = 0;

    if (source != 0) {
	ULONG len = MultiByteToWideChar(CP_ACP,
					MB_USEGLYPHCHARS | MB_PRECOMPOSED,
					source,
					sourcelen,
					0,
					0);
	if (len != 0) {
	    target = typecallocn(W32_CHAR, len + 1);

	    (void) MultiByteToWideChar(CP_ACP,
				       MB_USEGLYPHCHARS | MB_PRECOMPOSED,
				       source,
				       sourcelen,
				       target,
				       len);
	}
    }

    return target;
}

/* WINVER >= _0x0500 */
#ifndef WC_NO_BEST_FIT_CHARS
#define WC_NO_BEST_FIT_CHARS 0
#endif

/*
 * Use this via "asc_charstring()" to convert Unicode to ASCII strings.
 */
char *
w32_ucs2_to_ansi(const W32_CHAR * source, int sourcelen)
{
    char *target = 0;

    if (source != 0) {
	ULONG len = WideCharToMultiByte(CP_ACP,
					WC_NO_BEST_FIT_CHARS,
					source,
					sourcelen,
					0,
					0,
					NULL,
					NULL);
	if (len) {
	    target = typecallocn(char, len + 1);

	    (void) WideCharToMultiByte(CP_ACP,
				       WC_NO_BEST_FIT_CHARS,
				       source,
				       sourcelen,
				       target,
				       len,
				       NULL,
				       NULL);
	}
    }

    return target;
}
#endif

void *
binmalloc(void *source, int length)
{
    void *target = malloc(length);
    if (target != 0)
	memcpy(target, source, length);
    return target;
}

/* replaces
 * (void) cwait(&rc, (CWAIT_PARAM_TYPE) handle, 0);
 */
int
w32_wait_handle(HANDLE handle)
{
    DWORD exitcode = (DWORD) (-1);	/* if GetExitCodeProcess() fails */

    (void) WaitForSingleObject(handle, INFINITE);
    (void) GetExitCodeProcess(handle, &exitcode);
    return (int) exitcode;
}
