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
 * $Header: /users/source/archives/vile.vcs/RCS/w32oo.cpp,v 1.1 2000/09/25 07:08:57 cmorgan Exp $
 */

#include <windows.h>
#include <ole2.h>
#include <shlobj.h>
#include <stdio.h>

extern "C" {

#include "estruct.h"
#undef FAILED          /* Don't want this defn of FAILED */
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
    LPMALLOC     pMalloc = NULL;
    static       char *path;
    LPITEMIDLIST pidl = NULL;
    int          rc;
    HRESULT      hr;

    if (! path)
    {
        pMalloc = NULL;
        pidl    = NULL;

#ifndef VILE_OLE
        hr = OleInitialize(NULL);   /* Intialize OLE */
        if (! SUCCEEDED(hr))
        {
            disp_win32_error(hr, NULL);
            return (NULL);
        }
#endif
        hr = SHGetMalloc(&pMalloc);
        if (! SUCCEEDED(hr))
        {
            disp_win32_error(hr, NULL);
#ifndef VILE_OLE
            OleUninitialize();
#endif
            return (NULL);
        }
        hr = SHGetSpecialFolderLocation(NULL, CSIDL_FAVORITES, &pidl);
        if (! SUCCEEDED(hr))
        {
            disp_win32_error(hr, NULL);
            pMalloc->Release();
#ifndef VILE_OLE
            OleUninitialize();
#endif
            return (NULL);
        }

        if (! SHGetPathFromIDList(pidl, dir))
        {
            disp_win32_error(W32_SYS_ERROR, NULL);
            pMalloc->Release();
#ifndef VILE_OLE
            OleUninitialize();
#endif
            return (NULL);
        }
        bsl_to_sl_inplace(dir);  /* Convert to canonical form */
        path = strmalloc(dir);
        if (pidl)
            pMalloc->Free(pidl);
        pMalloc->Release();

#ifndef VILE_OLE
        OleUninitialize();
#endif
        if (! path)
            no_memory("get_favorites()");

    }
    return (path);
}

} /* Extern "C" */
