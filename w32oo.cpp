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
 * $Header: /users/source/archives/vile.vcs/RCS/w32oo.cpp,v 1.5 2005/01/19 20:45:50 cmorgan Exp $
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

/* ------------------------------------------------------------------------ */

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

        if (! SHGetPathFromIDList(pidl, dir))
            disp_win32_error(W32_SYS_ERROR, NULL);
        else
        {
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

#ifdef __cplusplus
} /* Extern "C" */
#endif
