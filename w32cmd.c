/*
 * w32cmd:  collection of functions that add Win32-specific editor
 *          features (modulo the clipboard interface) to [win]vile.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/w32cmd.c,v 1.10 2001/01/03 23:17:58 cmorgan Exp $
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commdlg.h>
#include <direct.h>

#include "estruct.h"
#include "edef.h"
#include "nefunc.h"

#undef  RECT			     /* FIXME: symbol conflict */

/* --------------------------------------------------------------------- */
/* ----------------------- misc common routines ------------------------ */
/* --------------------------------------------------------------------- */

static DWORD
comm_dlg_error(void)
{
    DWORD errcode;

    if ((errcode = CommDlgExtendedError()) != 0)
        mlforce("[CommDlgExtendedError() returns err code %d]", errcode);
    return (errcode);
}

/* --------------------------------------------------------------------- */
/* -------- commdlg-based routines that open and save files ------------ */
/* --------------------------------------------------------------------- */

static OPENFILENAME ofn;
static int          ofn_initialized;

static void
ofn_init(void)
{
    static char         custFilter[128] = "All Files (*.*)\0*.*\0";
    static char         filter[]        =
"C/C++ Files (*.c;*.cpp;*.h;*.rc)\0*.c;*.cpp;*.h;*.rc\0Text Files (*.txt)\0*.txt\0\0";

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize       = sizeof(ofn);
    ofn.lpstrFilter       = filter;
    ofn.lpstrCustomFilter = custFilter;
    ofn.nMaxCustFilter    = sizeof(custFilter);
    ofn_initialized       = TRUE;
}



/* glob a canonical filename, and if a true directory, put it in win32 fmt */
static int
glob_and_validate_dir(const char *inputdir, char *outputdir)
{
#define NOT_A_DIR  "[\"%s\" is not a directory]"

    int  outlen, rc;
    char tmp[NFILEN];

    strcpy(outputdir, inputdir);
    if (! doglob(outputdir))      /* doglob updates outputdir */
        rc = FALSE;
    else
    {
        /*
         * GetOpenFileName()/GetSaveFileName() will accept bogus initial dir
         * values without reporting an error.  Handle that.
         */

        if (! is_directory(outputdir))
        {
            outlen = (term.cols - 1) - (sizeof(NOT_A_DIR) - 3);
            mlforce(NOT_A_DIR, path_trunc(outputdir, outlen, tmp, sizeof(tmp)));
            rc = FALSE;
        }
        else
        {
            strcpy(outputdir, sl_to_bsl(outputdir));
            rc = TRUE;
        }
    }
    return (rc);

#undef NOT_A_DIR
}



/*
 * FUNCTION
 *   commdlg_open_files(int chdir_allowed, const char *dir)
 *
 *   chdir_allowed - permissible for GetOpenFileName() to cd to the
 *                   folder that stores the file(s) the user opens.
 *
 *   dir           - path of directory to browse.  If NULL, use cwd.
 *
 * DESCRIPTION
 *   Use the Windows common dialog library to open one or more files.
 *
 * RETURNS
 *   Boolean, T -> all is well.  F -> error noted and reported.
 */

static int
commdlg_open_files(int chdir_allowed, const char *dir)
{
#define RET_BUF_SIZE_ (24 * 1024)

    int   chdir_mask, i, len, nfile, rc = TRUE, status;
    char  *filebuf, oldcwd[FILENAME_MAX], newcwd[FILENAME_MAX], *cp,
          validated_dir[FILENAME_MAX];

    if (dir)
    {
        if (! glob_and_validate_dir(dir, validated_dir))
            return (FALSE);
        else
            dir = validated_dir;

    }
    oldcwd[0]  = newcwd[0] = '\0';
    chdir_mask = (chdir_allowed) ? 0 : OFN_NOCHANGEDIR;
    filebuf    = malloc(RET_BUF_SIZE_);
    if (! filebuf)
        return (no_memory("commdlg_open_files()"));
    filebuf[0] = '\0';
    if (! ofn_initialized)
        ofn_init();
    (void) _getcwd(oldcwd, sizeof(oldcwd));
    ofn.lpstrInitialDir = (dir) ? dir : oldcwd;
    ofn.lpstrFile       = filebuf;
    ofn.nMaxFile        = RET_BUF_SIZE_;
    ofn.Flags           = (OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
                          OFN_ALLOWMULTISELECT | OFN_EXPLORER) | chdir_mask;
#if DISP_NTWIN
    ofn.hwndOwner       = winvile_hwnd();
#else
    ofn.hwndOwner       = GetForegroundWindow();
#endif
    status              = GetOpenFileName(&ofn);

#if DISP_NTCONS
    /* attempt to restore focus to the console editor */
    (void) SetForegroundWindow(ofn.hwndOwner);
#endif

    if (! status)
    {
        /* user CANCEL'd the dialog box (err code 0), else some other error. */

        if (comm_dlg_error() == 0)
        {
            /* canceled -- restore old cwd if necessary */

            if (chdir_allowed)
            {
                if (stricmp(newcwd, oldcwd) != 0)
                    (void) _chdir(oldcwd);
            }
        }
        else
            rc = FALSE;  /* Win32 error */
        free(filebuf);
        return (rc);
    }
    if (chdir_allowed)
    {
        /* tell editor cwd changed, if it did... */

        if (! _getcwd(newcwd, sizeof(newcwd)))
        {
            free(filebuf);
            mlerror("_getcwd() failed");
            return (FALSE);
        }
        if (stricmp(newcwd, oldcwd) != 0)
        {
            if (! set_directory(newcwd))
            {
                free(filebuf);
                return (FALSE);
            }
        }
    }

    /*
     * Got a list of one or more files in filebuf (each nul-terminated).
     * Make one pass through the list to count the number of files
     * returned.  If #files > 0, then first file in the list is actually
     * the returned files' folder (er, directory).
     */
    cp    = filebuf;
    nfile = 0;
    for_ever
    {
        len = strlen(cp);
        if (len == 0)
            break;
        nfile++;
        cp += len + 1;  /* skip filename and its terminator */
    }
    if (nfile)
    {
        BUFFER *bp, *first_bp;
        char   *dir, tmp[FILENAME_MAX * 2], *nxtfile;
        int    have_dir;

        if (nfile > 1)
        {
            /* first "file" in the list is actually a folder */

            have_dir = 1;
            dir      = filebuf;
            cp       = dir + strlen(dir) + 1;
            nfile--;
        }
        else
        {
            have_dir = 0;
            cp       = filebuf;
        }
        for (i = 0, first_bp = NULL; rc && i < nfile; i++)
        {
            if (have_dir)
            {
                sprintf(tmp, "%s\\%s", dir, cp);
                nxtfile = tmp;
            }
            else
                nxtfile = cp;
            if ((bp = getfile2bp(nxtfile, FALSE, FALSE)) == 0)
                rc = FALSE;
            else
            {
                bp->b_flag |= BFARGS;   /* treat this as an argument */
                if (! first_bp)
                    first_bp = bp;
                cp += strlen(cp) + 1;
            }
        }
        if (rc)
            rc = swbuffer(first_bp);  /* editor switches to 1st buffer */
    }

    /* Cleanup */
    free(filebuf);
    return (rc);

#undef RET_BUF_SIZE_
}



static int
wopen_common(int chdir_allowed)
{

    char         dir[NFILEN];
    static TBUFF *last;
    int          rc;

    dir[0] = '\0';
    rc     = mlreply_dir("Directory: ", &last, dir);
    if (rc == TRUE)
    {
        mktrimmed(dir);
        rc = commdlg_open_files(chdir_allowed, (dir[0]) ? dir : NULL);
    }
    else if (rc == FALSE)
    {
        /* empty response */

        rc = commdlg_open_files(chdir_allowed, NULL);
    }
    /* else rc == ABORT or SORTOFTRUE */
    return (rc);
}



int
winopen_nocd(int f, int n)
{
    return (wopen_common(FALSE));
}



int
winopen(int f, int n)
{
    return (wopen_common(TRUE));
}



/* explicitly specify initial directory */
int
winopen_dir(const char *dir)
{
    return (commdlg_open_files(TRUE, dir));
}



/*
 * FUNCTION
 *   commdlg_save_file(int chdir_allowed, char *dir)
 *
 *   chdir_allowed - permissible for GetSaveFileName() to cd to the
 *                   folder that stores the file the user saves.
 *
 *   dir           - path of directory to browse.  If NULL, use cwd.
 *
 * DESCRIPTION
 *   Use the Windows common dialog library to save the current buffer in
 *   a filename and folder of the user's choice.
 *
 * RETURNS
 *   Boolean, T -> all is well.  F -> error noted and reported.
 */

static int
commdlg_save_file(int chdir_allowed, const char *dir)
{
    int   chdir_mask, rc = TRUE, status;
    char  filebuf[FILENAME_MAX], validated_dir[FILENAME_MAX],
          oldcwd[FILENAME_MAX], newcwd[FILENAME_MAX], *fname, *leaf,
          scratch[FILENAME_MAX];

    if (dir)
    {
        if (! glob_and_validate_dir(dir, validated_dir))
            return (FALSE);
        else
            dir = validated_dir;

    }

    /*
     * The "Save As" dialog box is traditionally initialized with the
     * name of the current open file.  Handle that detail now.
     */
    fname      = curbp->b_fname;
    filebuf[0] = '\0';           /* copy the currently open filename here */
    leaf       = NULL;
    if (! is_internalname(fname))
    {
        strcpy(scratch, fname);
        if ((leaf = last_slash(scratch)) == NULL)
        {
            /* easy case -- fname is a pure leaf */

            leaf = scratch;
        }
        else
        {
            /* fname specifies a path */

            *leaf++ = '\0';
            if (! dir)
            {
                /* save dirspec from path */

                dir = sl_to_bsl(scratch);
            }
        }
        strcpy(filebuf, leaf);
    }
    oldcwd[0]  = newcwd[0] = '\0';
    chdir_mask = (chdir_allowed) ? 0 : OFN_NOCHANGEDIR;
    if (! ofn_initialized)
        ofn_init();
    (void) _getcwd(oldcwd, sizeof(oldcwd));
    ofn.lpstrInitialDir = (dir) ? dir : oldcwd;
    ofn.lpstrFile       = filebuf;
    ofn.nMaxFile        = sizeof(filebuf);
    ofn.Flags           = (OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
                          OFN_OVERWRITEPROMPT | OFN_EXPLORER) | chdir_mask;
#if DISP_NTWIN
    ofn.hwndOwner       = winvile_hwnd();
#else
    ofn.hwndOwner       = GetForegroundWindow();
#endif
    status              = GetSaveFileName(&ofn);

#if DISP_NTCONS
    /* attempt to restore focus to the console editor */
    (void) SetForegroundWindow(ofn.hwndOwner);
#endif

    if (! status)
    {
        /* user CANCEL'd the dialog box (err code 0), else some other error. */

        if (comm_dlg_error() == 0)
        {
            /* canceled -- restore old cwd if necessary */

            if (chdir_allowed)
            {
                if (stricmp(newcwd, oldcwd) != 0)
                    (void) _chdir(oldcwd);
            }
        }
        else
            rc = FALSE;  /* Win32 error */
        return (rc);
    }
    if (chdir_allowed)
    {
        /* tell editor cwd changed, if it did... */

        if (! _getcwd(newcwd, sizeof(newcwd)))
        {
            mlerror("_getcwd() failed");
            return (FALSE);
        }
        if (stricmp(newcwd, oldcwd) != 0)
        {
            if (! set_directory(newcwd))
                return (FALSE);
        }
    }

    /*
     * Now mimic these standard vile commands to change the buffer name
     * and write its contents to disk:
     *
     *       :f
     *       :w
     */
    /* ------------ begin :f ------------ */
#if OPT_LCKFILES
    if ( global_g_val(GMDUSEFILELOCK) ) {
        if (!b_val(curbp,MDLOCKED) && !b_val(curbp,MDVIEW))
            release_lock(curbp->b_fname);
        ch_fname(curbp, fname);
        make_global_b_val(curbp,MDLOCKED);
        make_global_b_val(curbp,VAL_LOCKER);
        grab_lck_file(curbp, fname);
    } else
#endif
     ch_fname(curbp, filebuf);
    curwp->w_flag |= WFMODE;
    updatelistbuffers();
    /* ------------ begin :w ------------ */
    return (filesave(0, 0));
}



static int
wsave_common(int chdir_allowed)
{

    char         dir[NFILEN];
    static TBUFF *last;
    int          rc;

    dir[0] = '\0';
    rc     = mlreply_dir("Directory: ", &last, dir);
    if (rc == TRUE)
    {
        mktrimmed(dir);
        rc = commdlg_save_file(chdir_allowed, (dir[0]) ? dir : NULL);
    }
    else if (rc == FALSE)
    {
        /* empty user response */

        rc = commdlg_save_file(chdir_allowed, NULL);
    }
    /* else rc == ABORT or SORTOFTRUE */
    return (rc);
}



int
winsave(int f, int n)
{
    return (wsave_common(TRUE));
}


int
winsave_nocd(int f, int n)
{
    return (wsave_common(FALSE));
}



/* explicitly specify initial directory */
int
winsave_dir(const char *dir)
{
    return (commdlg_save_file(TRUE, dir));
}

/* --------------------------------------------------------------------- */
/* ------------------ delete current text selection -------------------- */
/* --------------------------------------------------------------------- */
int
windeltxtsel(int f, int n)  /* bound to Alt+Delete */
{
    return (w32_del_selection(FALSE));
}

/* --------------------------------------------------------------------- */
/* ------------------------- printing support -------------------------- */
/* --------------------------------------------------------------------- */
/* - Note that a good deal of this implementation is based on chapters - */
/* - 4 & 15 of Petzold's "Programming Windows 95". --------------------- */
/* --------------------------------------------------------------------- */

#ifdef DISP_NTWIN

static int  printing_aborted,  /* T -> user aborted print job.     */
            print_low, print_high, print_tabstop,
            print_number,      /* T -> line numbering is in effect */
            pgsetup_chgd,      /* T -> user invoked page setup dlg
                                *      and did not cancel out
                                */
            printdlg_chgd,     /* T -> user invoked print dialog and
                                *      did not cancel out
                                */
            spooler_failure;

static PAGESETUPDLG *pgsetup;
static PRINTDLG     *pd;

static RECT working_margin;    /* user's preferred margin (or a default),
                                * scaled in device coordinates (pixels).
                                */

static HWND hDlgCancelPrint;

/*
 * GetLastError() doesn't return a legitimate Win32 error if the spooler
 * encounters problems.  How inconvenient.
 */
static void
display_spooler_error(HWND hwnd)
{
#define ERRPREFIX "Spooler Error: "

    static const char *spooler_errs[] =
    {
        "general error",
        "canceled from program",
        "canceled by user",
        "out of disk space",
        "out of memory",
    };
    char       buf[256], *cp;
    const char *str;
    long       errcode,
               max_err = (sizeof(spooler_errs) / sizeof(spooler_errs));

    errcode         = GetLastError();
    spooler_failure = TRUE;
    if ((errcode & SP_NOTREPORTED) == 0)
        return;
    errcode *= -1;
    strcpy(buf, ERRPREFIX);
    cp = buf + (sizeof(ERRPREFIX) - 1);
    if (errcode >= max_err || errcode < 0)
        sprintf(cp, "unknown error code (%ld)", errcode);
    else
        strcat(cp, spooler_errs[errcode]);
    MessageBox(hwnd, buf, prognam, MB_ICONSTOP | MB_OK);

#undef ERRPREFIX
}



/* Obtain/adjust default info about the printed page. */
static int
handle_page_dflts(HWND hwnd)
{
    RECT m;

    if (pgsetup == NULL)
    {
        if ((pgsetup = calloc(1, sizeof(*pgsetup))) == NULL)
            return (no_memory("get_page_dflts"));
        pgsetup->Flags       = PSD_RETURNDEFAULT|PSD_DEFAULTMINMARGINS;
        pgsetup->lStructSize = sizeof(*pgsetup);

        /* ask system to return current settings without showing dlg box */
        if (! PageSetupDlg(pgsetup))
        {
            if (comm_dlg_error() == 0)
            {
                /* uh-oh */

                mlforce("BUG: PageSetupDlg() fails default data call");
            }
            return (FALSE);
        }

        /*
         * Now, configure the actual default margins used by the program,
         * which are either 0.5 inches or 125 mm, as appropriate for the
         * desktop env.
         */
        if (pgsetup->Flags & PSD_INHUNDREDTHSOFMILLIMETERS)
            m.bottom = m.top = m.left = m.right = 1250;
        else
            m.bottom = m.top = m.left = m.right = 500;
        pgsetup->rtMargin  = m;
        pgsetup->Flags    &= ~PSD_RETURNDEFAULT;
        pgsetup->Flags    |= PSD_MARGINS;
        pgsetup->hwndOwner = hwnd;
    }
    return (TRUE);
}



/*
 * Handles two chores:
 *
 *   1) temporarily converts all margin values to thousands of inches, if
 *      required, prior to rationalizing.
 *
 *   2) rationalizes user's margin settings with respect to the printer's
 *      unprintable page border (typically quite small--1/4" for a LJ4Plus).
 *
 * CAVEATS:  call handle_page_dflts() before this function.
 */
static void
rationalize_margins(int xdpi,   /* dots (pixels) per inch in x axis */
                    int ydpi,   /* dots (pixels) per inch in y axis */
                    int xoffs,  /* unprintable x offset, scaled in pixels */
                    int yoffs   /* unprintable y offset, scaled in pixels */)
{
    RECT m;

    m = pgsetup->rtMargin;   /* user's preferred margins, scaled in
                              * 0.001in or 0.01mm .
                              */
    if (pgsetup->Flags & PSD_INHUNDREDTHSOFMILLIMETERS)
    {
        /* convert to thousandths of inches */

        m.top    = (int) (1000.0 * m.top / 2500.0);
        m.bottom = (int) (1000.0 * m.bottom / 2500.0);
        m.right  = (int) (1000.0 * m.right / 2500.0);
        m.left   = (int) (1000.0 * m.left / 2500.0);
    }

    /* now, convert preferred margins to device coordinates */
    working_margin.left   = (int) (m.left * xdpi / 1000.0);
    working_margin.right  = (int) (m.right * xdpi / 1000.0);
    working_margin.top    = (int) (m.top * ydpi / 1000.0);
    working_margin.bottom = (int) (m.bottom * ydpi / 1000.0);

    /*
     * Many printers define a minimum margin -- printing not allowed there.
     * Deduct that white space from user's pref.  This white space will
     * be gained back when the viewport is adjusted during printing.
     *
     * If can't deduct it all, deduct the max.
     */
    working_margin.top =
        (working_margin.top > yoffs) ? working_margin.top - yoffs : 0;
    working_margin.bottom =
        (working_margin.bottom > yoffs) ? working_margin.bottom - yoffs : 0;
    working_margin.left =
        (working_margin.left > xoffs) ? working_margin.left - xoffs : 0;
    working_margin.right =
        (working_margin.right > xoffs) ? working_margin.right - xoffs : 0;
}



static BOOL CALLBACK
printer_abort_proc(HDC hPrintDC, int errcode)
{
    HWND hwnd = winvile_hwnd();
    MSG  msg;

    while (! printing_aborted && PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (hDlgCancelPrint || ! IsDialogMessage(hDlgCancelPrint, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (! printing_aborted);
}



static BOOL CALLBACK
printer_dlg_proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
            w32_center_window(hDlg, winvile_hwnd());
            EnableMenuItem(GetSystemMenu(hDlg, FALSE), SC_CLOSE, MF_GRAYED);
            return (TRUE);
            break;
        case WM_COMMAND:       /* user cancels printing */
            printing_aborted = TRUE;
            EnableWindow(GetParent(hDlg), TRUE);
            DestroyWindow(hDlg);
            hDlgCancelPrint = NULL;
            return (TRUE);
            break;
    }
    return (FALSE);
}



static void
winprint_cleanup(HFONT hfont_prev)
{
    if (hfont_prev)
        (void) DeleteObject(SelectObject(pd->hDC, hfont_prev));
    (void) EnableWindow(pd->hwndOwner, TRUE);
    (void) DeleteDC(pd->hDC);
}



static int
winprint_startpage(PRINTDLG *pd, HFONT hprint_font)
{
    /* FIXME -- emit header */

    /* Petzold tests "< 0", API ref sez, "<= 0".  I trust Petzold */
    if (StartPage(pd->hDC) < 0)
    {
        display_spooler_error(pd->hwndOwner);
        return (FALSE);
    }
    else
    {
        /*
         * There are some printer drivers that reset the font in the
         * printer's device context following each call to StartPage().
         * Guess how long it took me to figure that out?
         *
         * Example driver:  HP LaserJet4 Plus.
         */

        SelectObject(pd->hDC, hprint_font);

        /* Likewise the Viewport.  Sure, why not. */
        SetViewportOrgEx(pd->hDC,
                         working_margin.left,
                         working_margin.top,
                         NULL);
    }
    return (TRUE);
}



static int
winprint_endpage(PRINTDLG      *pd,
                 unsigned long pagenum,
                 int           xchar,
                 int           ychar,
                 int           mcpl,
                 unsigned long ypos)
{
    char     buf[256];
    unsigned buflen;
    UINT     txtmode;

    /* FIXME -- footer is hardwired as page number */
    /*
     * FIXME -- footer floats with margins.  blech.  need to hardwire footer
     * locn on last physical printing line of document.
     */

    txtmode = (GetTextAlign(pd->hDC) & ~TA_LEFT);
    buflen  = sprintf(buf, "%lu", pagenum);
    txtmode = SetTextAlign(pd->hDC, txtmode | TA_CENTER);
    TextOut(pd->hDC, xchar * mcpl / 2, ychar * ypos, buf, buflen);
    (void) SetTextAlign(pd->hDC, txtmode);
    /* Petzold tests "< 0", API ref sez, "<= 0".  I trust Petzold */
    if (EndPage(pd->hDC) < 0)
    {
        display_spooler_error(pd->hwndOwner);
        return (FALSE);
    }
    return (TRUE);
}



/*
 * FUNCTION
 *   winprint_fmttxt(unsigned long *dst,
 *                   unsigned long *src,
 *                   unsigned long srclen,
 *                   int           mcpl,
 *                   unsigned long line_num)
 *
 *   dst      - formatted output data
 *   src      - vile buffer LINE
 *   srclen   - length of LINE
 *   mcpl     - max chars per (printable) line.
 *   line_num - cur line number.  If 0, the data being output is wrapped
 *              a line number should not be emitted.
 *
 * DESCRIPTION
 *   Format a vile buffer LINE -- turning unprintable data into printable
 *   data and properly expanding tabs.
 *
 * RETURNS
 *   Number of src chars consumed.
 */

static unsigned long
winprint_fmttxt(unsigned char *dst,
                unsigned char *src,
                unsigned long srclen,
                int           mcpl,
                unsigned long line_num)
{
    unsigned      c;
    unsigned long i, nout, nconsumed, orig_srclen = srclen, line_num_offset;

    nout = nconsumed = line_num_offset = 0;
    if (print_number && line_num > 0)
    {
        nout = sprintf(dst, "%6lu  ", line_num);
        if (nout >= mcpl)
        {
            /* absurd printer output width -- skip line numbering */

            nout = 0;
        }
        else
        {
            dst += nout;
            line_num_offset = nout;
        }
    }

    /*
     * add a simple pre-formatting optimization:  if the current input is all
     * whitespace (or empty), handle that case now.
     */
    i = 0;
    while (i < srclen && isspace(src[i]))
        i++;
    if (i == srclen)
    {
        /* input line is empty or all white space */

        *dst++ = ' ';
        *dst   = '\0';
        return (srclen);  /* collapsed entire input line to single byte */
    }

    while (srclen--)
    {
        c = *src;
        if (c == _TAB_)
        {
            unsigned long nspaces;

            nspaces = print_tabstop - (nout - line_num_offset) % print_tabstop;
            if (nout + nspaces >= mcpl)
                break;
            nout += nspaces;
            for (i = 0; i < nspaces; i++)
                *dst++ = ' ';
        }
        else if  (c < _SPC_)  /* assumes ASCII char set  */
        {
            if (nout + 2 >= mcpl)
                break;
            nout  += 2;       /* account for '^' meta char     */
            *dst++ = '^';
            *dst++ = ctrldigits[c];
        }
        else if (c > _TILDE_ && (! PASS_HIGH(c)))   /* assumes ASCII char set */
        {
            if (nout + 4 >= mcpl)
                break;
            nout  += 4;                      /* account for '\xdd' meta chars */
            *dst++ = '\\';
            *dst++ = 'x';
            *dst++ = hexdigits[(c & 0xf0) >> 4];
            *dst++ = hexdigits[c & 0xf];
        }
        else
        {
            if (nout + 1 >= mcpl)
                break;
            nout++;
            *dst++ = c;
        }
        src++;
        nconsumed++;
    }
    *dst = '\0';

    if (srclen > 0)
    {
        /*
         * Still more data in the current input line that needs to be
         * formatted, but there's no more room left on the current output
         * row.  Add a simple post-formatting optimization:  if the
         * remainder of the current input line is all whitespace, return a
         * value that will flag the calling routine that this line has been
         * completely formatted (no use to wrap this line just to ouput
         * whitespace).
         */

        i = 0;
        while (i < srclen && isspace(src[i]))
            i++;
        if (i == srclen)
        {
            /* remainder of input line is all white space */

            nconsumed = orig_srclen;
        }
    }
    return (nconsumed);
}



static int
winprint_selection(PRINTDLG *pd,
                   int      ychar,  /* max character height         */
                   int      mcpl,   /* max printable chars per line */
                   int      mlpp    /* max lines per page           */
                   )
{
    int i;

    mlforce("[unimplemented]");
    return (TRUE);
}



/*
 * returns TRUE if all is well, else FALSE if a print spooling function
 * failed or some other error was detected.
 */
static int
winprint_curbuffer(PRINTDLG *pd,
                   HFONT    hprint_font,
                   int      xchar,  /* avg char width                 */
                   int      ychar,  /* max character height           */
                   int      mcpl,   /* max (printable) chars per line */
                   int      mlpp    /* max lines per page             */
                   )
{
    unsigned char  *printbuf;
    LINEPTR        lp;
    int            rc, isempty_line, endpg_req;
    unsigned long  hdrh,      /* header height, scaled in lines      */
                   footerh,   /* footer height, scaled in lines      */
                   footerpos, /* y page coord that triggers a footer */
                   vile_llen,
                   outlen,
                   pagenum,
                   plines,    /* running count of lines printed */
                   vlines,    /* count of vile lines in buffer, can be
                               * different from plines if text is wrapped.
                               */
                   ypos;      /* vertical position of print cursor on current
                               * page
                               */

    print_tabstop = tabstop_val(curbp);
    print_number  = (nu_width(curwp) > 0);

    /* "+32" conservatively accounts for line numbering prefix space */
    if ((printbuf = malloc(mcpl + 32)) == NULL)
        return (no_memory("winprint_curbuffer"));

    if (! winprint_startpage(pd, hprint_font))
        return (FALSE);
    endpg_req = rc = TRUE;
    lp        = lforw(buf_head(curbp));
    plines    = ypos = 0;
    pagenum   = vlines = 1;

    /* FIXME -- changes when true headers and footers added */
    /* FIXME -- these computations cause hdr/footer to float with
     * margins.  wrong approach.
     */
    hdrh      = 0;
    footerh   = 2;
    footerpos = mlpp - (1 + footerh);
    /* end FIXME */

    /* FIXME -- add collation and copies support */
    while (rc && (! printing_aborted))
    {
        if (lp == buf_head(curbp))
            break;               /* end of buffer */

        /*
         * printing a line of text is not as simple as it seems, since
         * we must take care to wrap buffer data that exceeds mcpl.
         */
        vile_llen    = llength(lp);
        isempty_line = (vile_llen == 0);
        for (outlen = 0; isempty_line || (outlen < vile_llen); plines++)
        {
            if (plines % mlpp == 0 && (! endpg_req))
            {
                ypos = 0;
                if (! winprint_startpage(pd, hprint_font))
                {
                    rc = FALSE;
                    break;
                }
                plines    += hdrh;
                endpg_req  = TRUE;
            }
            if (isempty_line)
            {
                isempty_line = FALSE;

                if (print_number)
                {
                    /* winprint_fmttxt() handles line number formatting */

                    (void) winprint_fmttxt(printbuf, " ", 1, mcpl, vlines);
                }
                else
                {
                    printbuf[0] = ' ';
                    printbuf[1] = '\0';
                }
            }
            else
            {
                outlen += winprint_fmttxt(
                                   printbuf,
                                   (unsigned char *) (lp->l_text + outlen),
                                   vile_llen - outlen,
                                   mcpl,
                                   (outlen == 0) ? vlines : 0
                                         );
            }
            TextOut(pd->hDC, 0, ychar * ypos, printbuf, strlen(printbuf));
            ypos++;
            if (plines % mlpp == footerpos)
            {
                endpg_req = FALSE;
                if (! winprint_endpage(pd,
                                       pagenum++,
                                       xchar,
                                       ychar,
                                       mcpl,
                                       mlpp - 1))
                {
                    rc = FALSE;
                    break;
                }
                plines += footerh;
            }
        }
        lp = lforw(lp);
        vlines++;
    }
    (void) free(printbuf);
    if (rc && endpg_req)
    {
        rc = winprint_endpage(pd,
                              pagenum,
                              xchar,
                              ychar,
                              mcpl,
                              mlpp - 1);
    }
    return (rc);
}



/*
 * Map winvile's display font to, hopefully, a suitable printer font.
 * Some of the code in this function is borrowed from Petzold.
 */
static HFONT
get_printing_font(HDC hdc, HWND hwnd)
{
    char            *curfont;
    HFONT           hfont;
    LOGFONT         logfont;
    char            font_mapper_face[LF_FACESIZE + 1],
                    msg[LF_FACESIZE * 2 + 128];
    POINT           pt;
    FONTSTR_OPTIONS str_rslts;
    TEXTMETRIC      tm;
    double          ydpi;

    curfont = ntwinio_current_font();
    if (! parse_font_str(curfont, &str_rslts))
    {
        mlforce("BUG: Invalid internal font string");
        return (NULL);
    }
    SaveDC(hdc);
    SetGraphicsMode(hdc, GM_ADVANCED);
    ModifyWorldTransform(hdc, NULL, MWT_IDENTITY);
    SetViewportOrgEx(hdc, 0, 0, NULL);
    SetWindowOrgEx(hdc, 0, 0, NULL);
    ydpi = GetDeviceCaps(hdc, LOGPIXELSY);
    pt.x = 0;
    pt.y = str_rslts.size * ydpi / 72;
    DPtoLP(hdc, &pt, 1);

    /* Build up LOGFONT data structure. */
    memset(&logfont, 0, sizeof(logfont));
    logfont.lfWeight = (str_rslts.bold) ? FW_BOLD : FW_NORMAL;
    logfont.lfHeight = -pt.y;
    if (str_rslts.italic)
        logfont.lfItalic = TRUE;
    logfont.lfCharSet        = DEFAULT_CHARSET;
    logfont.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    strncpy(logfont.lfFaceName, str_rslts.face, LF_FACESIZE - 1);
    logfont.lfFaceName[LF_FACESIZE - 1] = '\0';
    if ((hfont = CreateFontIndirect(&logfont)) == NULL)
    {
        disp_win32_error(W32_SYS_ERROR, hwnd);
        RestoreDC(hdc, -1);
        return (NULL);
    }

    /* font must be fixed pitch -- if not we bail */
    (void) SelectObject(hdc, hfont);
    GetTextFace(hdc, sizeof(font_mapper_face), font_mapper_face);
    GetTextMetrics(hdc, &tm);
    if ((tm.tmPitchAndFamily & TMPF_FIXED_PITCH) != 0)
    {
        /* Misnamed constant! */

        RestoreDC(hdc, -1);
        DeleteObject(hfont);
        sprintf(msg,
            "Fontmapper selected proportional printing font (%s), aborting...",
                font_mapper_face);
        MessageBox(hwnd, msg, prognam, MB_ICONSTOP | MB_OK);
        return (NULL);
    }

    if (stricmp(font_mapper_face, str_rslts.face) != 0)
    {
        /*
         * Notify user that fontmapper substituted a font for the printer
         * context that does not match the display context.  This is an
         * informational message only -- printing can proceed.
         */

        sprintf(msg,
                "info: Fontmapper substituted \"%s\" for \"%s\"",
                font_mapper_face,
                str_rslts.face);
        mlforce(msg);
    }
    RestoreDC(hdc, -1);
    return (hfont);
}



/*
 * this feature doesn't support printing by "pages" because vile doesn't
 * rigorously support this concept (what's the page height? cur window?).
 */
int
winprint(int f, int n)
{
    AREGION         ar;
    char            buf[NFILEN + 64];
    DOCINFO         di;
    HDC             hdc;
    HFONT           hfont, old_hfont;
    HWND            hwnd;
    int             horzres,
                    mcpl,   /* max chars per line            */
                    mlpp,   /* max lines per page            */
                    rc = TRUE,
                    vertres,
                    xchar,  /* avg char width (pixels)       */
                    xdpi,   /* dots per inch, x axis         */
                    xoffs,
                    xwidth, /* physical page width (pixels)  */
                    ychar,  /* max character height (pixels) */
                    ydpi,   /* dots per inch, y axis         */
                    yoffs,
                    ywidth; /* physical page height (pixels) */
    TEXTMETRIC      tm;

    hwnd = winvile_hwnd();
    if (! pd)
    {
        if ((pd = calloc(1, sizeof(*pd))) == NULL)
            return (no_memory("winprint"));
        pd->lStructSize = sizeof(*pd);
        pd->nCopies     = 1;
        pd->Flags       = PD_ALLPAGES|PD_RETURNDC|PD_NOPAGENUMS;
    }
    pd->hwndOwner = hwnd;

    /* configure print dialog box for current selection, if any */
    if (get_selection_buffer_and_region(&ar) == NULL)
        pd->Flags |= PD_NOSELECTION;
    else
    {
        pd->Flags |= PD_SELECTION;
        pd->Flags &= ~PD_NOSELECTION;
    }

    /*
     * FIXME -------------------------------------------------
     * disable the features that aren't implemented
     * FIXME -------------------------------------------------
     */
    pd->Flags |= (PD_NOSELECTION|PD_USEDEVMODECOPIESANDCOLLATE);
    /*
     * end FIXME ---------------------------------------------
     */

    if (! handle_page_dflts(hwnd))
        return (FALSE);

    if (pgsetup_chgd)
    {
        unsigned long nbytes;
        DEVMODE       *pdm_print, *pdm_setup;

        /*
         * Force printer to use parameters specified in page setup dialog.
         * This is a rather subtle implementation issue (i.e., undocumented).
         *
         * Famous last words:
         *
         *    "Wouldn't it be enlightening to spend an afternoon or so and add
         *    win32 printing to an app."
         *
         * Sure, you betcha'.
         */

        if (pd->hDevMode)
        {
            GlobalFree(pd->hDevMode);
            pd->hDevMode = NULL;
        }
        if (pd->hDevNames)
        {
            /* this is stale -- force PrintDlg to reload it */

            GlobalFree(pd->hDevNames);
            pd->hDevNames = NULL;
        }
        pdm_setup = GlobalLock(pgsetup->hDevMode);
        nbytes    = pdm_setup->dmSize + pdm_setup->dmDriverExtra;
        if ((pd->hDevMode = GlobalAlloc(GMEM_MOVEABLE, nbytes)) == NULL)
        {
            GlobalUnlock(pgsetup->hDevMode);
            return (no_memory("winprint"));
        }
        pdm_print = GlobalLock(pd->hDevMode);
        memcpy(pdm_print, pdm_setup, nbytes);
        GlobalUnlock(pgsetup->hDevMode);
        GlobalUnlock(pd->hDevMode);
        pgsetup_chgd = FALSE;
    }

    /* Up goes the canonical win32 print dialog */
    if (! PrintDlg(pd))
    {
        /* user cancel'd dialog box or some error detected. */

        if (comm_dlg_error() != 0)
        {
            /* legit error */

            rc = FALSE;
        }
        return (rc);
    }
    printdlg_chgd    = TRUE;
    printing_aborted = spooler_failure = FALSE;
    print_high       = global_g_val(GVAL_PRINT_HIGH);
    print_low        = global_g_val(GVAL_PRINT_LOW);

    /* compute some printing parameters */
    hdc = pd->hDC;
    SetMapMode(hdc, MM_TEXT);
    if ((hfont = get_printing_font(hdc, hwnd)) == NULL)
    {
        winprint_cleanup(NULL);
        return (FALSE);
    }
    old_hfont = SelectObject(hdc, hfont);
    xdpi      = GetDeviceCaps(hdc, LOGPIXELSX);
    ydpi      = GetDeviceCaps(hdc, LOGPIXELSY);
#if 0
    /*
     * Can't use the next two lines because I've seen at least one printer
     * driver return HORZRES/VERTRES data that's inconsistent with
     * PHYSICALOFFSETX/PHYSICALOFFSETY (HP DJ 870C).
     *
     * So, just stick with the PHYSICAL* constants .
     */
    horzres   = GetDeviceCaps(hdc, HORZRES);
    vertres   = GetDeviceCaps(hdc, VERTRES);
#endif
    xoffs     = GetDeviceCaps(hdc, PHYSICALOFFSETX);
    yoffs     = GetDeviceCaps(hdc, PHYSICALOFFSETY);
    xwidth    = GetDeviceCaps(hdc, PHYSICALWIDTH);
    ywidth    = GetDeviceCaps(hdc, PHYSICALHEIGHT);
    horzres   = xwidth - 2 * xoffs;
    vertres   = ywidth - 2 * yoffs;
    if (horzres < 0 || vertres < 0)
    {
        MessageBox(hwnd,
                   "Printer driver's physical dimensions not rationale",
                   prognam,
                   MB_ICONSTOP | MB_OK);
        winprint_cleanup(old_hfont);
        return (FALSE);
    }
    rationalize_margins(xdpi, ydpi, xoffs, yoffs);
    (void) GetTextMetrics(hdc, &tm);
    ychar = tm.tmHeight + tm.tmExternalLeading;
    xchar = tm.tmAveCharWidth;
    mcpl  = (horzres - working_margin.left - working_margin.right) / xchar;
    if (mcpl <= 0)
    {
        MessageBox(hwnd,
                   "Left/Right margins too wide",
                   prognam,
                   MB_ICONSTOP | MB_OK);
        winprint_cleanup(old_hfont);
        return (FALSE);
    }
    mlpp  = (vertres - working_margin.top - working_margin.bottom) / ychar;
    if (mlpp <= 0)
    {
        MessageBox(hwnd,
                   "Top/Bottom margins too wide",
                   prognam,
                   MB_ICONSTOP | MB_OK);
        winprint_cleanup(old_hfont);
        return (FALSE);
    }

    /* disable access to winvile message queue during printing */
    (void) EnableWindow(hwnd, FALSE);

    /* up goes the _modeless_ 'abort printing' dialog box */
    hDlgCancelPrint = CreateDialog((HINSTANCE) GetWindowLong(
                                                    hwnd,
                                                    GWL_HINSTANCE
                                                             ),
                                   "PrintCancelDlg",
                                   hwnd,
                                   printer_dlg_proc);
    if (! hDlgCancelPrint)
    {
        disp_win32_error(W32_SYS_ERROR, hwnd);
        winprint_cleanup(old_hfont);
        return (FALSE);
    }
    (void) SetAbortProc(hdc, printer_abort_proc);

    /* create a job name for spooler */
    memset(&di, 0, sizeof(di));
    di.cbSize = sizeof(di);
    if (pd->Flags & PD_SELECTION)
    {
        sprintf(buf, "%s (selection)", curbp->b_bname);
        di.lpszDocName = buf;
    }
    else
        di.lpszDocName = curbp->b_bname;
    if (StartDoc(hdc, &di) <= 0)
    {
        display_spooler_error(hwnd);
        winprint_cleanup(old_hfont);
        return (FALSE);
    }
    if (pd->Flags & PD_SELECTION)
        rc = winprint_selection(pd, ychar, mcpl, mlpp);
    else
        rc = winprint_curbuffer(pd, hfont, xchar, ychar, mcpl, mlpp);
    if (! spooler_failure)
    {
        /*
         * don't close document if spooler failed -- causes a fault
         * on Win9x hosts.
         */

        (void) EndDoc(hdc);
    }
    if (! printing_aborted)
    {
        /*
         * order is important here:  enable winvile's message queue before
         * destroying the cancel printing dlg, else winvile loses focus
         */

        (void) EnableWindow(hwnd, TRUE);
        (void) DestroyWindow(hDlgCancelPrint);
    }
    (void) DeleteObject(SelectObject(hdc, old_hfont));
    (void) DeleteDC(hdc);
    return (rc);
}



int
winpg_setup(int f, int n)
{
    RECT margins;
    int  rc = TRUE;

    if (! handle_page_dflts(winvile_hwnd()))
        return (FALSE);
    if (printdlg_chgd)
    {
        unsigned long nbytes;
        DEVMODE       *pdm_print, *pdm_setup;

        /*
         * Force PageSetupDlg() to use parameters specified by last
         * invocation of PrinterDlg().
         */
        if (pgsetup->hDevMode)
        {
            GlobalFree(pgsetup->hDevMode);
            pgsetup->hDevMode = NULL;
        }
        if (pgsetup->hDevNames)
        {
            /* stale by defn -- force PrintSetupDlg to reload it */

            GlobalFree(pgsetup->hDevNames);
            pgsetup->hDevNames = NULL;
        }
        pdm_print = GlobalLock(pd->hDevMode);
        nbytes    = pdm_print->dmSize + pdm_print->dmDriverExtra;
        if ((pgsetup->hDevMode = GlobalAlloc(GMEM_MOVEABLE, nbytes)) == NULL)
        {
            GlobalUnlock(pd->hDevMode);
            return (no_memory("winpg_setup"));
        }
        pdm_setup = GlobalLock(pgsetup->hDevMode);
        memcpy(pdm_setup, pdm_print, nbytes);
        GlobalUnlock(pgsetup->hDevMode);
        GlobalUnlock(pd->hDevMode);
        printdlg_chgd = FALSE;
    }
    if (! PageSetupDlg(pgsetup))
    {
        /* user cancel'd dialog box or some error detected. */

        if (comm_dlg_error() != 0)
        {
            /* legit error -- user did not cancel dialog box */

            rc = FALSE;
        }
    }
    pgsetup_chgd = TRUE;
    return (rc);
}
#endif   /* DISP_NTWIN */
