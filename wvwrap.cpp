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
 * $Header: /users/source/archives/vile.vcs/RCS/wvwrap.cpp,v 1.10 2006/01/04 22:53:39 cmorgan Exp $
 */

#include "w32vile.h"

#include <objbase.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <initguid.h>
#include "w32reg.h"
#include "w32ole.h"

#define	typeallocn(cast,ntypes)		(cast *)malloc((ntypes)*sizeof(cast))
#define	typereallocn(cast,ptr,ntypes)	(cast *)realloc((char *)(ptr),\
							(ntypes)*sizeof(cast))

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
    fmt_win32_error((ULONG) E_OUTOFMEMORY, &tmp, 0);
    MessageBox(NULL, buf, "wvwrap", MB_OK|MB_ICONSTOP);
    return (1);
}



/* from ntwinio.c */
static void
make_argv(char *cmdline)
{
    int maxargs = (strlen(cmdline) + 1) / 2;
    char *ptr;

    argv = typeallocn(char *, maxargs);
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
        if ((olebuf = typeallocn(OLECHAR, olebuf_len)) == NULL)
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
    BSTR         bstr;
    size_t       dynbuf_len, dynbuf_idx;
    HRESULT      hr;
    HWND         hwnd;
    VARIANT_BOOL insert_mode, minimized;
    char         *lclbuf = NULL, tmp[512], *dynbuf;
    OLECHAR      *olestr;
    LPUNKNOWN    punk;
    IVileAuto    *pVileAuto;

    make_argv(lpCmdLine);

    olebuf_len = 4096;
    dynbuf_len = 4096;
    dynbuf_idx = 0;
    olebuf     = typeallocn(OLECHAR, olebuf_len);
    dynbuf     = typeallocn(char, dynbuf_len);
    if (! (olebuf && dynbuf))
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
    pVileAuto->put_Visible(VARIANT_TRUE);  // May not be necessary
    pVileAuto->get_InsertMode(&insert_mode);
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
            int add_delim, offset = 0;

            *cp = '\0';
            if (cp == *argv)
            {
                /* filename is of the form:  \<leaf> .  handle this. */

                strcpy(dynbuf, (insert_mode) ? "\033" : "");
                strcat(dynbuf, ":cd \\\n");
            }
            else
            {
                 /*
                  * To add insult to injury, Windows Explorer has a habit
                  * of passing 8.3 filenames to wvwrap (noted on a win2K
                  * system and a FAT32 partition).  If the folder portion
                  * of the file's path happens to be in 8.3 format (i.e.,
                  * a tilde included in the folder name), then 8.3 folder
                  * names appear in winvile's Recent Folders list.
                  * Needless to say, it's no fun trying to decipher 8.3
                  * folder names.
                  */

                char folder[FILENAME_MAX], *fp;

                if (GetLongPathName(*argv, folder, sizeof(folder)) > 0)
                    fp = folder;
                else
                    fp = *argv;

                if (insert_mode)
                {
                    dynbuf[0] = '\033';
                    offset    = 1;
                }
                add_delim = (isalpha(fp[0]) && fp[1] == ':' && fp[2] == '\0');

                /*
                 * With regard to the following code, note that the
                 * original file might be in the form "<drive>:\leaf", in
                 * which case *argv now points at "<drive>:" .  Recall that
                 * cd'ing to <drive>:  on a DOS/WIN32 host has special
                 * semantics (which we don't want).
                 */
                sprintf(dynbuf + offset,
                        ":cd %s%s\n",
                        fp,
                        (add_delim) ? "\\" : "");
            }
            dynbuf_idx = strlen(dynbuf);
            *cp        = '\\';
        }
        while (argc--)
        {
            size_t len = strlen(*argv);

            if (dynbuf_idx + len + sizeof(":e \n") >= dynbuf_len)
            {
                dynbuf_len *= 2 + len + sizeof(":e \n");
                dynbuf      = typereallocn(char, dynbuf, dynbuf_len);
                if (! dynbuf)
                    return (nomem());
            }
            dynbuf_idx += sprintf(dynbuf + dynbuf_idx, ":e %s\n", *argv++);
        }
        olestr = TO_OLE_STRING(dynbuf);
        bstr   = SysAllocString(olestr);
        if (! (bstr && olestr))
            return (nomem());
        pVileAuto->VileKeys(bstr);
        SysFreeString(bstr);
    }

    // Set foreground window using a method that's compatible with win2k
    pVileAuto->get_MainHwnd((LONG *) &hwnd);
    (void) SetForegroundWindow(hwnd);

    // If editor minimized, restore window
    hr = pVileAuto->get_IsMinimized(&minimized);
    if (SUCCEEDED(hr) && minimized)
        hr = pVileAuto->Restore();
    pVileAuto->Release();
    CoUninitialize(); // shut down COM
    return (0);
}
