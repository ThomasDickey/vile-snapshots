/*
 * w32misc:  collection of unrelated, common win32 functions used by both
 *           the console and GUI flavors of the editor.
 *
 * Caveats
 * =======
 * -- This code has not been tested with NT 3.51 .
 *
 * $Header: /users/source/archives/vile.vcs/RCS/w32misc.c,v 1.10 1998/09/23 22:21:14 tom Exp $
 */

#include <windows.h>
#include <io.h>
#include <conio.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <process.h>

#include "estruct.h"
#include "edef.h"
#include "nefunc.h"

#define CSHEXE      "csh.exe"
#define CSHEXE_LEN  (sizeof(CSHEXE) - 1)
#define HOST_95     0
#define HOST_NT     1
#define HOST_UNDEF  (-1)
#define SHEXE       "sh.exe"
#define SHEXE_LEN   (sizeof(SHEXE) - 1)
#define SHELL_C_LEN (sizeof(" -c ") - 1)

static int   host_type = HOST_UNDEF; /* nt or 95? */
#ifndef DISP_NTWIN
static char  saved_title[256];
#endif

/* ------------------------------------------------------------------ */

#if DISP_NTWIN
int
stdin_data_available(void)
{
    FILE *fp;
    int  rc = 0;

    if ((fp = fdopen(dup(fileno(stdin)), "r")) != NULL)
    {
        fclose(fp);
        rc = 1;
    }
    return (rc);
}
#endif


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
 *   therefore, cannnot be called to effect sh -c "cmdstr".  Consequently,
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
mk_shell_cmd_str(char *cmd, int *allocd_mem, int prepend_shc)
{
    int         alloc_len;
    static int  bourne_shell = 0; /* Boolean, T if user's shell has
                                   * appearances of a Unix lookalike
                                   * bourne shell (e.g., sh, ksh, bash).
                                   */
    char        *out_str, *cp;
    static char *shell = NULL, *shell_c = "/c";

    if (shell == NULL)
    {
        int len;

        shell        = get_shell();
        len          = strlen(shell);
        bourne_shell = (len >= 2 &&
                        tolower(shell[len - 2]) == 's' &&
                        tolower(shell[len - 1]) == 'h')
                               ||
                       (len >= SHEXE_LEN &&
                        stricmp(shell + len - SHEXE_LEN, SHEXE) == 0);
        if (bourne_shell)
        {
            shell_c = "-c";

            /* Now check for csh lookalike. */
            bourne_shell = ! (
                               (len >= 3 &&
                               tolower(shell[len - 3]) == 'c')
                                        ||
                               (len >= CSHEXE_LEN &&
                                stricmp(shell + len - CSHEXE_LEN, CSHEXE) == 0)
                             );
        }
    }
    if (! bourne_shell)
    {
        /*
         * MS-DOS shell or csh.  Do not bother quoting user's command
         * string, since the former is oblivious to the notion of a unix
         * shell's argument quoting and the latter does not support nested
         * double quotes.
         */

        if (prepend_shc)
        {
            alloc_len = strlen(cmd) + strlen(shell) + SHELL_C_LEN + 1;
            if ((out_str = malloc(alloc_len)) == NULL)
                return (out_str);
            *allocd_mem = TRUE;
            sprintf(out_str, "%s %s %s", shell, shell_c, cmd);
            return (out_str);
        }
        else
        {
            *allocd_mem = FALSE;
            return (cmd);
        }
    }

    /* Else apply solutions 1-2 above. */
    alloc_len = strlen(cmd) * 2; /* worst case -- every cmd byte quoted */
    if (prepend_shc)
        alloc_len += strlen(shell) + SHELL_C_LEN;
    alloc_len += 3;              /* terminating nul + 2 quote chars     */
    if ((out_str = malloc(alloc_len)) == NULL)
    {
        errno = ENOMEM;
        return (out_str);
    }
    *allocd_mem = TRUE;

    cp = out_str;
    if (prepend_shc)
        cp += sprintf(cp, "%s %s ", shell, shell_c);
    if (strchr(cmd, '\'') == NULL)
    {
        /* No single quotes in command string.  Solution #1. */

        sprintf(cp, "'%s'", cmd);
        return (out_str);
    }

    /* Solution #2. */
    *cp++ = '"';
    while (*cmd)
    {
        if (*cmd == '\\')
        {
            *cp++ = *cmd++;
            if (*cmd)
            {
                /* Any quoted char is immune to further quoting. */

                *cp++ = *cmd++;
            }
        }
        else if (*cmd != '"')
            *cp++ = *cmd++;
        else
        {
            /* bare '"' */

            *cp++ = '\\';
            *cp++ = *cmd++;
        }
    }
    *cp++ = '"';
    *cp   = '\0';
    return (out_str);
}



/*
 * FUNCTION
 *   w32_system(const char *cmd)
 *
 *   cmd - command string to be be passed to a Win32 command interpreter.
 *
 * DESCRIPTION
 *   Executes a system() call, taking care to ensure that the user's
 *   command string is properly quoted if get_shell() points to a bourne
 *   shell clone.
 *
 * RETURNS
 *   If memory allocation fails, -1.
 *   Else, whatever system() returns.
 */

int
w32_system(const char *cmd)
{
    char *cmdstr;
    int  freestr, rc;

    if ((cmdstr = mk_shell_cmd_str((char *) cmd, &freestr, FALSE)) == NULL)
    {
        /* heap exhausted! */

        (void) no_memory("w32_system()");
        return (-1);
    }
    set_console_title(cmd);
    rc = system(cmdstr);
    if (freestr)
        free(cmdstr);
    restore_console_title();
    return (rc);
}



#if DISP_NTWIN
/*
 * FUNCTION
 *   w32_system_winvile(const char *cmd)
 *
 *   cmd - command string to be be passed to a Win32 command interpreter.
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
 *   b) said console stays "up" until explicitly dismissed by the user.
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
w32_system_winvile(const char *cmd)
{
#define PRESS_ANY_KEY "\n[Press any key to continue]"

    char                 *cmdstr;
    int                  freestr, rc = -1;
    PROCESS_INFORMATION  pi;
    STARTUPINFO          si;

    if ((cmdstr = mk_shell_cmd_str((char *) cmd, &freestr, TRUE)) == NULL)
    {
        /* heap exhausted! */

        (void) no_memory("w32_system_winvile()");
        return (rc);
    }
    if (! AllocConsole())
    {
        if (freestr)
            free(cmdstr);
        mlforce("console creation failed");
        return (rc);
    }
    SetConsoleTitle(cmd);
    memset(&si, 0, sizeof(si));
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES;
    si.hStdInput   = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput  = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError   = GetStdHandle(STD_ERROR_HANDLE);
    if (CreateProcess(NULL,
                      cmdstr,
                      NULL,
                      NULL,
                      TRUE,       /* Inherit handles */
                      0,
                      NULL,
                      NULL,
                      &si,
                      &pi))
    {
        /* Success */

        DWORD        dummy;
        INPUT_RECORD ir;

        (void) _cwait(&rc, (int) pi.hProcess, 0);
        (void) WriteFile(si.hStdOutput,
                         PRESS_ANY_KEY,
                         sizeof(PRESS_ANY_KEY) - 1,
                         &dummy,
                         NULL);
        for (;;)
        {
            /* Wait for a single key of input from user. */

            if (! ReadConsoleInput(si.hStdInput, &ir, 1, &dummy))
                break;      /* What?? */
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown)
                break;
        }
        (void) CloseHandle(pi.hProcess);
        (void) CloseHandle(pi.hThread);
    }
    else
    {
        /* Bummer */

        mlforce("unable to exec console command");
    }
    if (freestr)
        free(cmdstr);
    FreeConsole();
    return (rc);
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
 *   control (via TTkopen) before the prompt, then the output of the
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
#ifdef DISP_NTCONS
    int c;

    if (pressret)
    {
        fputs("[Press return to continue]", stdout);
        fflush(stdout);

        /* loop for a CR, a space, or a : to do another named command */
        while ((c = _getch()) != '\r' &&
                c != '\n' &&
                c != ' ' &&
                !ABORTED(c))
        {
            if (kcod2fnc(c) == &f_namedcmd)
            {
                unkeystroke(c);
                break;
            }
        }
    }
    TTkopen();
    kbd_erase_to_end(0);
#endif
}



/*
 * The code in ntconio.c that saves and restores console titles
 * didn't work reliably for pipe or shell operations.  It's moved here
 * now and called via the w32_system() wrapper or the w32pipe module.
 */
void
set_console_title(const char *title)
{
#ifndef DISP_NTWIN
    GetConsoleTitle(saved_title, sizeof(saved_title));
    SetConsoleTitle(title);
#endif
}



void
restore_console_title(void)
{
#ifndef DISP_NTWIN
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
    int flags = FORMAT_MESSAGE_FROM_SYSTEM;

    if (! *buf)
        flags |= FORMAT_MESSAGE_ALLOCATE_BUFFER;
    FormatMessage(flags,
                  NULL,
                  errcode == W32_SYS_ERROR ? GetLastError() : errcode,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* dflt language */
                  (*buf) ? *buf : (LPTSTR) buf,
                  buflen,
                  NULL);
    return (*buf);
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
    MessageBox(hwnd, buf, prognam, MB_OK|MB_ICONSTOP);
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
parse_font_str(const char *fontstr, FONTSTR_OPTIONS *results)
{
    const char    *cp, *tmp;
    char          *endnum, *facep;
    unsigned long size;

    memset(results, 0, sizeof(*results));
    size  = 0;
    cp    = fontstr;
    while (*cp && isspace(*cp))
        cp++;

    /* Up first is either a font face or font size. */
    if (isdigit(*cp))
    {
        errno = 0;
        size  = strtoul(cp, &endnum, 10);
        if (errno != 0)
            return (FALSE);
        tmp = endnum;
        while (*tmp && isspace(*tmp))
            tmp++;
        if (*tmp != '\0')
        {
            if (*tmp != ',')
            {
                /* Not a 100% integer value, assume this is a font face. */

                size = 0;
            }
            else
                cp = tmp;      /* Valid point size. */
        }
        else
            cp = tmp;         /* Only a point size specified, nothing left. */
    }
    if (size == 0)
    {
        /* this must be a font face */

        facep = results->face;
        while (*cp)
        {
            if (*cp == ',')
            {
                cp++;
                break;
            }
            else if (*cp == '\\' && cp[1] == ',')
            {
                *facep++  = ',';
                cp       += 2;
            }
            else
                *facep++ = *cp++;
        }
        *facep = '\0';
        if (results->face[0] == '\0' || *cp == '\0')
            return (FALSE);
        else
        {
            /* Now pick up non-optional font size (that follows face). */

            errno = 0;
            size  = strtoul(cp, &endnum, 10);
            if (errno != 0 || size == 0)
                return (FALSE);
            cp = endnum;
        }
    }

    /* Now look for optional font style. */
    while (*cp && isspace(*cp))
        cp++;

    /* At this point, there are two allowable states:  delimiter or EOS. */
    if (*cp)
    {
        if (*cp++ == ',')
        {
            while (*cp && isspace(*cp))
                cp++;
            if (strncmp(cp, "bold-italic", sizeof("bold-italic") - 1) == 0)
                results->bold = results->italic = TRUE;
            else if (strncmp(cp, "italic", sizeof("italic") - 1) == 0)
                results->italic = TRUE;
            else if (strncmp(cp, "bold", sizeof("bold") - 1) == 0)
                results->bold = TRUE;
            else
                return (FALSE);
        }
        else
            return (FALSE);
    }
    results->size = size;
    return (TRUE);
}
#endif  /* DISP_NTWIN */
