/*
 * w32ole.cpp:  Winvile's OLE implementation (the editor currently
 *              supports only OLE Automation).
 *
 * Note:  A great deal of the code included in this file was copied from
 * the Microsoft platform sdk samples directory, specifically from:
 *
 *    samples\com\oleaut\hello\hello .
 *
 * However, this implementation handles the Release() methods quite
 * differently and so, I wouldn't use this code as a guide for writing
 * any other automation servers :-) .
 *
 * Caveats
 * =======
 * - Due to a conflict between estruct.h and MS names, the Windows macros
 *   "FAILED" may not be used to test an OLE return code.  Use SUCCEEDED
 *   instead.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/w32ole.cpp,v 1.5 1998/10/01 10:31:26 cmorgan Exp $
 */

#include <windows.h>
#include <ole2.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

extern "C"
{
    #include "estruct.h"
    #undef FAILED          /* Don't want this defn of FAILED */
    #include "edef.h"
}
#include "w32ole.h"
#include "w32reg.h"


static size_t   ansibuf_len,   /* scaled in bytes   */
                olebuf_len;    /* scaled in wchar_t */
static char     *ansibuf;
static DWORD    dwRegisterCF, dwRegisterActiveObj;
static vile_oa  *g_pvile_oa;
static int      oleauto_server;
static OLECHAR  *olebuf;

/* ------------------------ C Linkage Helpers ---------------------- */

extern "C"
{

int
oleauto_init(OLEAUTO_OPTIONS *opts)
{
    char           *dummy = NULL;
    HRESULT        hr;
    LPCLASSFACTORY pcf = NULL;

    olebuf_len = ansibuf_len = 512;
    ansibuf    = (char *) malloc(ansibuf_len);
    olebuf     = (OLECHAR *) malloc(olebuf_len * sizeof(OLECHAR));
    if (! (olebuf && ansibuf))
        return (FALSE);

    hr = OleInitialize(NULL);   // Intialize OLE
    if (! SUCCEEDED(hr))
    {
        disp_win32_error(hr, NULL);
        return (FALSE);
    }

    oleauto_server = TRUE;

    // Create an instance of the Vile Application object. Object is
    // created with refcount 0.
    if (! SUCCEEDED(vile_oa::Create(&g_pvile_oa, ! opts->invisible)))
        return (FALSE);

    // Expose class factory for application object.
    pcf = new vile_oa_cf;
    if (! pcf)
        return (FALSE);
    pcf->AddRef();
    hr = CoRegisterClassObject(CLSID_VileAuto,
                               pcf,
                               CLSCTX_LOCAL_SERVER,
                    (opts->multiple) ? REGCLS_SINGLEUSE : REGCLS_MULTIPLEUSE,
                               &dwRegisterCF);
    pcf->Release();
    if (! SUCCEEDED(hr))
    {
        disp_win32_error(hr, NULL);
        return (FALSE);
    }

    /*
     * Register Vile application object in the Running Object Table (ROT).
     * This allows controllers to connect to a running application object
     * instead of creating a new instance.  Use weak registration so that
     * the ROT releases it's reference when all external references are
     * released.  If strong registration is used, the ROT will not release
     * it's reference until RevokeActiveObject is called and so will keep
     * the object alive even after all external references have been
     * released.
     */
    hr = RegisterActiveObject(g_pvile_oa,
                              CLSID_VileAuto,
                              ACTIVEOBJECT_WEAK,
                              &dwRegisterActiveObj);
    if (! SUCCEEDED(hr))
    {
        disp_win32_error(hr, NULL);
        return (FALSE);
    }
    if (! opts->invisible)
    {
        hr = CoLockObjectExternal(g_pvile_oa, TRUE, TRUE);
        if (! SUCCEEDED(hr))
        {
            disp_win32_error(hr, NULL);
            return (FALSE);
        }
    }
    return (TRUE);
}

void
oleauto_exit(int code)
{
    if (ansibuf)
        free(ansibuf);
    if (olebuf)
        free(olebuf);
    if (oleauto_server)
    {
        if (g_pvile_oa)
        {
            /*
             * For a local server, CoDisconnectObject will disconnect the
             * object from external connections.  So the controller will
             * get an RPC error if it accesses the object after calling
             * Quit and the controller will not GP fault.
             */

            CoDisconnectObject(g_pvile_oa, 0);
            delete g_pvile_oa;
        }
        if (dwRegisterCF != 0)
            CoRevokeClassObject(dwRegisterCF);
        if (dwRegisterActiveObj != 0)
            RevokeActiveObject(dwRegisterActiveObj, NULL);
        OleUninitialize();
    }
    exit(code);
}

} /* Extern "C" */

/* ----------------------- C++ Helper Functions --------------------- */

/*
 * Quick & Dirty ANSI/Unicode conversion routines.  These routines use a
 * dynamic buffer to hold the converted string so it may be any arbitrary
 * size.  However, the same dynamic buffer is reused when the routine is
 * called a second time.  So make sure that the converted string is
 * used/copied before the conversion routine is called again.
 *
 */
#if (! defined(_UNICODE) || defined(UNICODE))
char *
ConvertToAnsi(OLECHAR *szW)
{
    size_t len;

    len = wcstombs(NULL, szW, 1) + 1;
    if (len > ansibuf_len)
    {
        if (ansibuf)
            free(ansibuf);
        ansibuf_len = ansibuf_len * 2 + len;
        if ((ansibuf = (char *) malloc(ansibuf_len)) == NULL)
            return (ansibuf);  /* We're gonna' die */
    }
    wcstombs(ansibuf, szW, len);
    return (ansibuf);
}

OLECHAR *
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

HRESULT
LoadTypeInfo(ITypeInfo **pptinfo, REFCLSID clsid)
{
    HRESULT hr;
    LPTYPELIB ptlib = NULL;
    LPTYPEINFO ptinfo = NULL;

    *pptinfo = NULL;

    hr = LoadRegTypeLib(LIBID_VileAuto, VTL_MAJOR, VTL_MINOR, 0x09, &ptlib);
    if (! SUCCEEDED(hr))
        return hr;

    // Get type information for interface of the object.
    hr = ptlib->GetTypeInfoOfGuid(clsid, &ptinfo);
    ptlib->Release();
    if (! SUCCEEDED(hr))
        return hr;

    *pptinfo = ptinfo;
    return NOERROR;
}

/* ------------------------ Class Factory -------------------------- */

vile_oa_cf::vile_oa_cf(void)
{
    m_cRef = 0;
}

STDMETHODIMP
vile_oa_cf::QueryInterface(REFIID iid, void **ppv)
{
    *ppv = NULL;

    if (iid == IID_IUnknown || iid == IID_IClassFactory)
        *ppv = this;
    else
        return E_NOINTERFACE;

    AddRef();
    return NOERROR;
}

STDMETHODIMP_(ULONG)
vile_oa_cf::AddRef(void)
{
    return ++m_cRef;
}

STDMETHODIMP_(ULONG)
vile_oa_cf::Release(void)
{
    if(--m_cRef == 0)
    {
        delete this;
        return 0;
    }
    return m_cRef;
}

STDMETHODIMP
vile_oa_cf::CreateInstance(IUnknown *punkOuter, REFIID riid, void **ppv)
{
    HRESULT hr;

    *ppv = NULL;

    // This implementation doesn't allow aggregation
    if (punkOuter)
        return CLASS_E_NOAGGREGATION;

    /*
     * If this is a REGCLS_SINGLEUSE class factory, CreateInstance will be
     * called at most once.  The global application object has already been
     * created when CreateInstance is called.  A REGCLS_MULTIPLEUSE class
     * factory's CreateInstance would be called multiple times and would
     * create a new object each time.
     */
    hr = g_pvile_oa->QueryInterface(riid, ppv);
    if (!  SUCCEEDED(hr))
    {
        g_pvile_oa->Quit();
        return hr;
    }
    return (NOERROR);
}

STDMETHODIMP
vile_oa_cf::LockServer(BOOL fLock)
{
    HRESULT hr;

    hr = CoLockObjectExternal(g_pvile_oa, fLock, TRUE);
    if (! SUCCEEDED(hr))
        disp_win32_error(hr, NULL);
    return (hr);
}

/* ------------------------------- vile_oa ------------------------------ */

HRESULT
vile_oa::Create(vile_oa **ppvile, BOOL visible)
{
    HRESULT hr;
    vile_oa *pvile = NULL;
    OLECHAR *tmp;

    *ppvile = NULL;

    // Create Application object
    pvile = new vile_oa();
    if (pvile == NULL)
        return E_OUTOFMEMORY;

    pvile->m_hwnd     = (HWND) winvile_hwnd();
    pvile->m_bVisible = (visible) ? VARIANT_TRUE : VARIANT_FALSE;

    // Name
    tmp = TO_OLE_STRING(prognam);
    if (! (tmp && (pvile->m_bstrName = SysAllocString(tmp))))
        return E_OUTOFMEMORY;

    // Load type information from type library.
    hr = LoadTypeInfo(&pvile->m_ptinfo, IID_IVileAuto);
    if (! SUCCEEDED(hr))
    {
        MessageBox(pvile->m_hwnd,
"Cannot load type library.\r\r Register type info using winvile's '-Or' command line switch",
                   prognam,
                   MB_OK | MB_ICONSTOP);
        return (hr);
    }

    *ppvile = pvile;
    return NOERROR;
}

vile_oa::vile_oa()
{
    m_bstrFullName = NULL;
    m_bstrName     = NULL;
    m_ptinfo       = NULL;
    m_cRef         = 0;
}

vile_oa::~vile_oa()
{
    if (m_bstrFullName) SysFreeString(m_bstrFullName);
    if (m_bstrName)     SysFreeString(m_bstrName);
    if (m_ptinfo)       m_ptinfo->Release();
}

STDMETHODIMP
vile_oa::QueryInterface(REFIID iid, void **ppv)
{
    *ppv = NULL;

    if (iid == IID_IUnknown || iid == IID_IDispatch || iid == IID_IVileAuto)
        *ppv = this;
    else
        return E_NOINTERFACE;

    AddRef();
    return NOERROR;
}


STDMETHODIMP_(ULONG)
vile_oa::AddRef(void)
{
    return (++m_cRef);
}

STDMETHODIMP_(ULONG)
vile_oa::Release(void)
{
    if (--m_cRef == 0)
        PostQuitMessage(0);
    return (m_cRef);
}

STDMETHODIMP
vile_oa::GetTypeInfoCount(UINT *pctinfo)
{
    *pctinfo = 1;
    return NOERROR;
}

STDMETHODIMP
vile_oa::GetTypeInfo(
      UINT itinfo,
      LCID lcid,
      ITypeInfo **pptinfo)
{
    *pptinfo = NULL;

    if(itinfo != 0)
        return DISP_E_BADINDEX;

    m_ptinfo->AddRef();
    *pptinfo = m_ptinfo;

    return NOERROR;
}

STDMETHODIMP
vile_oa::GetIDsOfNames(
      REFIID riid,
      OLECHAR **rgszNames,
      UINT cNames,
      LCID lcid,
      DISPID *rgdispid)
{
    return DispGetIDsOfNames(m_ptinfo, rgszNames, cNames, rgdispid);
}

STDMETHODIMP
vile_oa::Invoke(
      DISPID dispidMember,
      REFIID riid,
      LCID lcid,
      WORD wFlags,
      DISPPARAMS *pdispparams,
      VARIANT *pvarResult,
      EXCEPINFO *pexcepinfo,
      UINT *puArgErr)
{
    return DispInvoke(
        this, m_ptinfo,
        dispidMember, wFlags, pdispparams,
        pvarResult, pexcepinfo, puArgErr);
}

STDMETHODIMP
vile_oa::get_Application(IVileAuto **ppvile)
{
    HRESULT hr;

    *ppvile = NULL;

    hr = QueryInterface(IID_IDispatch, (void **)ppvile);
    return ((SUCCEEDED(hr)) ? NOERROR : E_UNEXPECTED);
}

STDMETHODIMP
vile_oa::get_FullName(BSTR *pbstr)
{
    if (! m_bstrFullName)
    {
        char    *cp, key[NFILEN];
        HKEY    hk;
        long    rc;
        DWORD   regtype = REG_SZ;
        OLECHAR *tmp;
        BYTE    value[NFILEN * 2];
        ULONG   val_len;

        *pbstr  = NULL;
        val_len = sizeof(value);

        /* Extract server path from registry. */
        sprintf(key, "CLSID\\%s\\LocalServer32", CLSID_VILEAUTO_KEY);
        if (RegOpenKeyEx(HKEY_CLASSES_ROOT,
                         key,
                         0,
                         KEY_QUERY_VALUE,
                         &hk) != ERROR_SUCCESS)
        {
            disp_win32_error(W32_SYS_ERROR, m_hwnd);
            return (E_UNEXPECTED);
        }
        rc = RegQueryValueEx(hk, NULL, NULL, &regtype, value, &val_len);
        RegCloseKey(hk);
        if (rc != ERROR_SUCCESS)
        {
            disp_win32_error(W32_SYS_ERROR, m_hwnd);
            return (E_UNEXPECTED);
        }
        if ((cp = strchr((char *) value, ' ')) != NULL)
            *cp = '\0';
        tmp = TO_OLE_STRING((char *) value);
        if (! (tmp && (m_bstrFullName = SysAllocString(tmp))))
            return (E_OUTOFMEMORY);
    }
    *pbstr = SysAllocString(m_bstrFullName);
    return ((*pbstr) ? NOERROR : E_OUTOFMEMORY);
}

STDMETHODIMP
vile_oa::get_Name(BSTR *pbstr)
{
    *pbstr = SysAllocString(m_bstrName);
    return ((*pbstr) ? NOERROR : E_OUTOFMEMORY);
}

STDMETHODIMP
vile_oa::get_Parent(IVileAuto **ppvile)
{
    HRESULT hr;

    *ppvile = NULL;

    hr = QueryInterface(IID_IDispatch, (void **)ppvile);
    return ((SUCCEEDED(hr)) ? NOERROR : E_UNEXPECTED);
}

STDMETHODIMP
vile_oa::Quit()
{
    PostQuitMessage(0);
    return NOERROR;
}

STDMETHODIMP
vile_oa::VileKeys(BSTR keys)
{
    HRESULT hr   = NOERROR;
    char    *msg = FROM_OLE_STRING(keys);

    while (*msg)
    {
        if (! PostMessage(m_hwnd, WM_CHAR, *msg, 0))
        {
            disp_win32_error(W32_SYS_ERROR, m_hwnd);
            hr = E_UNEXPECTED;
            break;
        }
        msg++;
    }
    return (hr);
}

STDMETHODIMP
vile_oa::put_Visible(VARIANT_BOOL bVisible)
{
    if (bVisible != m_bVisible)
    {
        HRESULT hr;

        m_bVisible = bVisible;
        hr = CoLockObjectExternal(this, (m_bVisible) ? TRUE : FALSE, TRUE);
        if (! SUCCEEDED(hr))
        {
            disp_win32_error(hr, m_hwnd);
            return (hr);
        }
        ::ShowWindow(m_hwnd, bVisible ? SW_SHOW : SW_HIDE);
    }
    return (NOERROR);
}

STDMETHODIMP
vile_oa::get_Visible(VARIANT_BOOL *pbool)
{
    *pbool = m_bVisible;
    return NOERROR;
}

STDMETHODIMP
vile_oa::get_IsMinimized(VARIANT_BOOL *pbool)
{
    WINDOWPLACEMENT wp;

    wp.length = sizeof(wp);
    if (! GetWindowPlacement(m_hwnd, &wp))
        return (GetLastError());
    *pbool = (wp.showCmd == SW_MINIMIZE ||
              wp.showCmd == SW_SHOWMINIMIZED ||
              wp.showCmd == SW_SHOWMINNOACTIVE) ? VARIANT_TRUE : VARIANT_FALSE;
    return NOERROR;
}

STDMETHODIMP
vile_oa::get_InsertMode(VARIANT_BOOL *pbool)
{
    *pbool = (insertmode) ? VARIANT_TRUE : VARIANT_FALSE;
    return NOERROR;
}

STDMETHODIMP
vile_oa::ForegroundWindow()
{
    if (! m_bVisible)
        put_Visible(TRUE);   /* Force window to be visible, first. */
    ::SetForegroundWindow(m_hwnd);
    return NOERROR;
}

STDMETHODIMP
vile_oa::Minimize()
{
    ::ShowWindow(m_hwnd, SW_MINIMIZE);
    if (! m_bVisible)
    {
        /*
         * Minimizing an invisible window will make it visible, so keep the
         * m_bVisible bookkeeping up-to-date.
         */

        put_Visible(TRUE);
    }
    return NOERROR;
}

STDMETHODIMP
vile_oa::Restore()
{
    ::ShowWindow(m_hwnd, SW_RESTORE);
    if (! m_bVisible)
    {
        /*
         * Restoring an invisible window will make it visible, so keep the
         * m_bVisible bookkeeping up-to-date.
         */

        put_Visible(TRUE);
    }
    return NOERROR;
}

/* ----------- C Linkage To OLE-specific Editor Functions ---------------- */

extern "C"
{

/*
 * Wait for a file to spring into existence with > 0 bytes of data.  This
 * editor function is used to work around a bug in DevStudio 5, which
 * creates its build log _after_ notifying visvile that a build is complete.
 */
int
waitfile(int f, int n)
{
#define SLEEP_INCR 100

    char         fname[NFILEN];  /* wait for this file */
    DWORD        how_long, curr_sleep;
    int          s;              /* status return */
    struct _stat statdata;

    fname[0] = '\0';
    how_long = (f) ? n : 2000;  /* If wait period not specified as argument,
                                 * wait up to 2 seconds.
                                 */
    if ((s = mlreply_no_opts("Wait for file: ", fname, sizeof(fname))) == TRUE)
    {
        curr_sleep = 0;
        for (;;)
        {
            if (_stat(fname, &statdata) == 0)
            {
                if (statdata.st_size > 0)
                    break;
            }
            Sleep(SLEEP_INCR);
            curr_sleep += SLEEP_INCR;
            if (curr_sleep > how_long)
            {
                mlwrite("timed out waiting for \"%s\"", fname);
                break;
            }
        }
    }
    return (s);
}

} /* Extern "C" */
