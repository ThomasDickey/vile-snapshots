/*
 * wvwrap.cpp:  A WinVile WRAPper .
 *
 * Originally written by Ed Henderson.
 *
 * This wrapper may be used to open one or more files via a right mouse
 * click in the Windows Explorer.  For more details, please read
 * doc/oleauto.doc .
 *
 * Note:  A great deal of the code included in this file is copied
 * (almost verbatim) from other vile modules.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/wvwrap.cpp,v 1.1 1999/05/17 02:31:35 cmorgan Exp $
 */

#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <string.h>

#include <initguid.h>
#include "w32reg.h"
#include "w32ole.h"

static size_t   olebuf_len;    /* scaled in wchar_t */
static OLECHAR  *olebuf;
static char     **argv;
static int      argc;

//--------------------------------------------------------------

/* from w32misc.c */
static char *
fmt_win32_error(ULONG errcode, char **buf, ULONG buflen)
{
    int flags = FORMAT_MESSAGE_FROM_SYSTEM;

    if (! *buf)
        flags |= FORMAT_MESSAGE_ALLOCATE_BUFFER;
    FormatMessage(flags,
                  NULL,
                  errcode,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* dflt language */
                  (*buf) ? *buf : (LPTSTR) buf,
                  buflen,
                  NULL);
    return (*buf);
}



static int
nomem(void)
{
    char buf[512], *tmp;

    tmp = buf;
    fmt_win32_error(E_OUTOFMEMORY, &tmp, 0);
    MessageBox(NULL, buf, "wvwrap", MB_OK|MB_ICONSTOP);
    return (1);
}



/* from ntwinio.c */
static void
make_argv(char *cmdline)
{
    int maxargs = (strlen(cmdline) + 1) / 2;
    char *ptr;

    argv = (char **) malloc(maxargs * sizeof(*argv));
    if (! argv)
        exit(nomem());

    for (ptr = cmdline; *ptr != '\0';)
    {
        char delim = ' ';

        while (*ptr == ' ')
            ptr++;

        if (*ptr == '\''
         || *ptr == '"'
         || *ptr == ' ') {
            delim = *ptr++;
        }
        argv[argc++] = ptr;
        if (argc+1 >= maxargs) {
            break;
        }
        while (*ptr != delim && *ptr != '\0')
            ptr++;
        if (*ptr == delim)
            *ptr++ = '\0';
    }
}



#if (! defined(_UNICODE) || defined(UNICODE))
/*
 * Quick & Dirty Unicode conversion routine.  Routine uses a
 * dynamic buffer to hold the converted string so it may be any arbitrary
 * size.  However, the same dynamic buffer is reused when the routine is
 * called a second time.  So make sure that the converted string is
 * used/copied before the conversion routine is called again.
 *
 *
 * from w32ole.cpp
 */
static OLECHAR *
ConvertToUnicode(char *szA)
{
    size_t len;

    len = strlen(szA) + 1;
    if (len > olebuf_len)
    {
        if (olebuf)
            free(olebuf);
        olebuf_len = olebuf_len * 2 + len;
        if ((olebuf = (OLECHAR *) malloc(olebuf_len * sizeof(OLECHAR))) == NULL)
            return (olebuf);  /* We're gonna' die */
    }
    mbstowcs(olebuf, szA, len);
    return (olebuf);
}
#endif



int WINAPI
WinMain( HINSTANCE hInstance,      // handle to current instance
         HINSTANCE hPrevInstance,  // handle to previous instance
         LPSTR     lpCmdLine,      // pointer to command line
         int       nCmdShow        // show state of window
  )
{
    BSTR      bstr;
    HRESULT   hr;
    char      *lclbuf = NULL, tmp[512];
    OLECHAR   *olestr;
    LPUNKNOWN punk;
    IVileAuto *pVileAuto;

    make_argv(lpCmdLine);

    olebuf_len = 512;
    olebuf     = (OLECHAR *) malloc(olebuf_len * sizeof(OLECHAR));
    if (! olebuf)
        return (nomem());

    hr = CoInitialize(NULL); // Fire up COM.
    if (FAILED(hr))
    {
        fmt_win32_error(hr, &lclbuf, 0);
        sprintf(tmp,
                "ERROR: CoInitialize() failed, errcode: %#lx, desc: %s",
                hr,
                lclbuf);
        MessageBox(NULL, tmp, "wvwrap", MB_OK|MB_ICONSTOP);
        return (1);
    }
    hr = CoCreateInstance(CLSID_VileAuto,
                          NULL,
                          CLSCTX_LOCAL_SERVER,
                          IID_IUnknown,
                          (void **) &punk);
    if (FAILED(hr))
    {
        fmt_win32_error(hr, &lclbuf, 0);
        sprintf(tmp,
                "ERROR: CoInitialize() failed, errcode: %#lx, desc: %s\n"
                "Try registering winvile via its -Or switch.",
                hr,
                lclbuf);
        MessageBox(NULL, tmp, "wvwrap", MB_OK|MB_ICONSTOP);
        return (1);
    }
    hr = punk->QueryInterface(IID_IVileAuto, (void **) &pVileAuto);
    punk->Release();
    if (FAILED(hr))
    {
        fmt_win32_error(hr, &lclbuf, 0);
        sprintf(tmp,
                "ERROR: QueryInterface() failed, errcode: %#lx, desc: %s\n\n",
                hr,
                lclbuf);
        MessageBox(NULL, tmp, "wvwrap", MB_OK|MB_ICONSTOP);
        return (1);
    }
    if (argc > 0)
    {
        char *cp;

        /*
         * When wvwrap starts up (and subsequently launches winvile), the
         * editor's CWD is set to a path deep within the bowels of Windows.
         * Not very useful.  Change CWD to the directory of the first
         * file on the commandline.
         */
        cp = strrchr(*argv, '\\');
        if (cp)
        {
            *cp = '\0';
            sprintf(tmp, ":cd %s\n", *argv);
            *cp = '\\';
            olestr = TO_OLE_STRING(tmp);
            bstr   = SysAllocString(olestr);
            if (! (bstr && olestr))
                return (nomem());
            pVileAuto->VileKeys(bstr);
            SysFreeString(bstr);
        }
        while (argc--)
        {
            olestr = TO_OLE_STRING(*argv++);
            bstr   = SysAllocString(olestr);
            if (! (bstr && olestr))
                return (nomem());
            pVileAuto->Open(bstr);
            SysFreeString(bstr);
        }
    }
    pVileAuto->Release();
    CoUninitialize(); // shut down COM
    return (0);
}
