/*
 * w32cmd:  collection of functions that add Win32-specific editor
 *          features (modulo the clipboard interface) to [win]vile.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/w32cmd.c,v 1.9 2000/11/11 01:49:13 cmorgan Exp $
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <commdlg.h>
#include <direct.h>

#include "estruct.h"
#include "edef.h"
#include "nefunc.h"

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

    int   chdir_mask, i, len, nfile, rc = TRUE;
    DWORD errcode;
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
    if (! GetOpenFileName(&ofn))
    {
        /* user CANCEL'd the dialog box (errcode 0), else some other error. */

        if ((errcode = CommDlgExtendedError()) != 0)
        {
            mlforce("[CommDlgExtendedError() returns err code %d]", errcode);
            rc = FALSE;
        }
        else
        {
            /* canceled -- restore old cwd if necessary */

            if (chdir_allowed)
            {
                if (stricmp(newcwd, oldcwd) != 0)
                    (void) _chdir(oldcwd);
            }
        }
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
    int   chdir_mask, rc = TRUE;
    DWORD errcode;
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
    if (! GetSaveFileName(&ofn))
    {
        /* user CANCEL'd the dialog box (errcode 0), else some other error. */

        if ((errcode = CommDlgExtendedError()) != 0)
        {
            mlforce("[CommDlgExtendedError() returns err code %d]", errcode);
            rc = FALSE;
        }
        else
        {
            /* canceled -- restore old cwd if necessary */

            if (chdir_allowed)
            {
                if (stricmp(newcwd, oldcwd) != 0)
                    (void) _chdir(oldcwd);
            }
        }
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
/* - Note that a good deal of this implementation is based on chapter -- */
/* - 15 of Petzold's "Programming Windows 95". ------------------------- */
/* --------------------------------------------------------------------- */

#ifdef DISP_NTWIN
int
winpg_setup(int f, int n)
{
    mlforce("not implemented");
    return (TRUE);
}



int
winprint(int f, int n)
{
    mlforce("not implemented");
    return (TRUE);
}
#endif   /* DISP_NTWIN */
