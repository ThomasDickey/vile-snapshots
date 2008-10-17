/*
 * w32oo:  collection of Win32-specific functions that require a C++ (OO)
 *         compilation.
 *
 * Caveats
 * =======
 * - Due to a conflict between estruct.h and MS names, the Windows macros
 *   "FAILED" may not be used to test an OLE return code.  Use SUCCEEDED
 *   instead.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/w32oo.cpp,v 1.12 2008/10/17 00:44:06 tom Exp $
 */

#include "w32vile.h"

#include <ole2.h>
#include <shlobj.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "estruct.h"
#include "edef.h"
#include "dirstuff.h"

#if CC_TURBO
#include <dir.h>		/* for 'chdir()' */
#endif

#ifdef UNICODE
static int
vl_GetPathFromIDList(LPITEMIDLIST lp, char *bufferp)
{
    int rc = 0;
    W32_CHAR buffer[FILENAME_MAX];
    char *result;

    rc = SHGetPathFromIDList(lp, buffer);
    if (rc) {
	if ((result = asc_charstring(buffer)) != 0) {
	    strcpy(bufferp, result);
	    free (result);
	}
    }
    return rc;
}
#else
#define vl_GetPathFromIDList(listp, bufferp) SHGetPathFromIDList(listp, bufferp)
#endif

/* ---------------------------- Favorites --------------------------------- */

/*
 * Return path to favorites folder.  don't know of any other way to do this
 * than via COM ... a bit over the top, doncha' think?
 */
const char *
get_favorites(void)
{
    char         dir[FILENAME_MAX];
    static       char *path;
    LPITEMIDLIST pidl;
    HRESULT      hr;

    if (! path)
    {
#ifndef VILE_OLE
        hr = OleInitialize(NULL);
        if (! SUCCEEDED(hr))
        {
            disp_win32_error(hr, NULL);
            return (NULL);
        }
#endif
        pidl = NULL;
        hr   = SHGetSpecialFolderLocation(NULL, CSIDL_FAVORITES, &pidl);
        if (! SUCCEEDED(hr))
        {
            disp_win32_error(hr, NULL);
            CoTaskMemFree(pidl);     // API accepts NULL ptr
#ifndef VILE_OLE
            OleUninitialize();
#endif
            return (NULL);
        }

        if (! vl_GetPathFromIDList(pidl, dir)) {
            disp_win32_error(W32_SYS_ERROR, NULL);
        } else {
            bsl_to_sl_inplace(dir);  // Convert to canonical form
            path = strmalloc(dir);
            if (! path)
                no_memory("get_favorites()");
        }
        CoTaskMemFree(pidl);
#ifndef VILE_OLE
        OleUninitialize();
#endif
    }
    return (path);
}

/* --------------------------- Graphical CD ------------------------------- */

static const char *initial_browse_dir;

/*
 * Setting the initial browse folder for SHBrowseForFolder() is, *cough*,
 * insane.  Oh by the way, if a pidl isn't used to set the initial browse
 * folder, then initial paths like "\\some_unc_path" don't work.  I kid
 * you not.
 *
 * Much of this callback's code is cribbed from the 'Net.
 */
static int CALLBACK
BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
    HRESULT       hr;
    ULONG         len;
    LPITEMIDLIST  pidl;
    LPSHELLFOLDER pShellFolder;
    W32_CHAR      szDir[MAX_PATH];
    LPWSTR        wide_path;

    switch(uMsg)
    {
        case BFFM_INITIALIZED:
            // Set/initial the browse folder path.  This code is a hoot.
            len = MultiByteToWideChar(CP_ACP,
                                      MB_USEGLYPHCHARS|MB_PRECOMPOSED,
                                      initial_browse_dir,
                                      -1,
                                      0,
                                      0);
            wide_path = new WCHAR[len];
            MultiByteToWideChar(CP_ACP,
                                MB_USEGLYPHCHARS|MB_PRECOMPOSED,
                                initial_browse_dir,
                                -1,
                                wide_path,
                                len);
            pShellFolder = 0;
            hr = SHGetDesktopFolder(&pShellFolder);
            if (! SUCCEEDED(hr))
            {
                delete [] wide_path;
                disp_win32_error(hr, hwnd);
                break;
            }
            hr = pShellFolder->ParseDisplayName(0,
                                                0,
                                                wide_path,
                                                &len,
                                                &pidl,
                                                0);
            if (! SUCCEEDED(hr)) {
                disp_win32_error(hr, hwnd);
            } else {
                SendMessage(hwnd, BFFM_SETSELECTION, FALSE, (LPARAM) pidl);
                CoTaskMemFree(pidl);
            }
            delete [] wide_path;
            pShellFolder->Release();
            break;
        case BFFM_SELCHANGED:
           // Set the status window text to the currently selected path.
           if (SHGetPathFromIDList((LPITEMIDLIST) lp, szDir))
              SendMessage(hwnd, BFFM_SETSTATUSTEXT, 0, (LPARAM) szDir);
           break;
        default:
           break;
    }
    return (0);
}

static int
graphical_cd(const char *dir)
{
    BROWSEINFO   bi;
    HWND         hwnd;
    LPITEMIDLIST pidl;
    int          rc = FALSE;
    char         bslbuf[FILENAME_MAX + 1], selected_folder[FILENAME_MAX + 1];

    TRACE((T_CALLED "graphical_cd(%s)\n", dir));

    // win32 functions don't like '/' as path delimiter.
    if (! w32_glob_and_validate_dir(dir, bslbuf))
        returnCode(rc);

    // pShellFolder->ParseDisplayName() will not accept local system folder
    // names that don't include a drive letter.  Yes, this is fatal brain
    // damage, but it's not like there are other supported, non-deprecated
    // alternatives.  Fortunately, lengthen_path() rides to the rescue.
    // Unfortunately, lengthen_path() reverts to '/' as a path delimiter.
    // Oh, what tangled webs we weave.
    initial_browse_dir = sl_to_bsl(lengthen_path(bslbuf));
    TRACE(("initial_browse_dir=%s\n", initial_browse_dir));

#if DISP_NTWIN
    hwnd = (HWND) winvile_hwnd();
#else
    hwnd = GetVileWindow();
#endif

#ifndef VILE_OLE
    HRESULT hr = OleInitialize(NULL);
    if (! SUCCEEDED(hr))
    {
        disp_win32_error(hr, hwnd);
        returnCode(rc);
    }
#endif
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = hwnd;
    bi.ulFlags   = BIF_STATUSTEXT;
    bi.lpfn      = BrowseCallbackProc;
    if ((pidl = SHBrowseForFolder(&bi)) != NULL)
    {
        if (! vl_GetPathFromIDList(pidl, selected_folder))
            disp_win32_error(NOERROR, hwnd);
        else
            rc = set_directory(selected_folder);
        CoTaskMemFree(pidl);
    }
    // else -- user cancel'd out of dialog box

#ifndef VILE_OLE
    OleUninitialize();
#endif
    returnCode(rc);
}

int
wincd(int f, int n)
{
    char         buf[NFILEN];
    static TBUFF *last;
    int          rc;

    TRACE((T_CALLED "wincd(%d,%d)\n", f, n));
    buf[0] = '\0';
    rc     = mlreply_dir("Initial Directory: ", &last, buf);
    if (rc == TRUE)
    {
        mktrimmed(buf);
        rc = graphical_cd(buf);
    }
    else if (rc == FALSE)
    {
        /* empty response */

        getcwd(buf, sizeof(buf));
        rc = graphical_cd(buf);
    }
    /* else rc == ABORT or SORTOFTRUE */
    returnCode(rc);
}

/* explicitly specify initial directory */
int
wincd_dir(const char *dir)
{
    char buf[NFILEN];

    TRACE((T_CALLED "wincd_dir(%s)\n"));
    if (dir == NULL)
    {
        getcwd(buf, sizeof(buf));
        dir = buf;
    }
    returnCode(graphical_cd(dir));
}

static int
filterExceptions(unsigned int code, struct _EXCEPTION_POINTERS *ep)
{
    if (code == EXCEPTION_INVALID_HANDLE)
    {
	return EXCEPTION_EXECUTE_HANDLER;
    }
    else
    {
	return EXCEPTION_CONTINUE_SEARCH;
    }
}

// filter out STATUS_INVALID_HANDLE aka EXCEPTION_INVALID_HANDLE
void
w32_close_handle(HANDLE handle)
{
    __try
    {
	(void) CloseHandle(handle);
    }
    __except(filterExceptions(GetExceptionCode(), GetExceptionInformation()))
    {
	TRACE(("error closing handle %#x\n", handle));
    }
}

#ifdef __cplusplus
} /* Extern "C" */
#endif
