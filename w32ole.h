/*
 * w32ole.h:      Winvile's OLE definitions (the editor currently
 *                supports only OLE Automation).
 *
 * Preconditions: include <windows.h> and <ole2.h> prior to including this file.
 *
 * Note:          a great deal of the code included in this file was copied
 *                from the Microsoft platform sdk samples directory,
 *                specifically from:
 *
 *                samples\com\oleaut\hello\hello .
 *
 * $Id: w32ole.h,v 1.11 2020/01/17 22:36:27 tom Exp $
 */

#ifndef W32OLE_H
#define W32OLE_H

#if defined(_UNICODE) || defined(UNICODE)
    #define FROM_OLE_STRING(str) asc_charstring(str)
    #define TO_OLE_STRING(str) w32_charstring(str)
#else
    #define FROM_OLE_STRING(str) ConvertToAnsi(str)
    static char* ConvertToAnsi(OLECHAR *szW);
    #define TO_OLE_STRING(str) ConvertToUnicode(str)
    static OLECHAR* ConvertToUnicode(const char *szA);
#endif

#include "winviletlb.h"

extern HRESULT LoadTypeInfo(ITypeInfo **pptinfo, REFCLSID clsid);

class vile_oa : public IVileAuto    // oa -> ole automation
{
public:
    // IUnknown methods
    STDMETHOD(QueryInterface)(REFIID riid, LPVOID * ppvObj);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

    // IDispatch methods
    STDMETHOD(GetTypeInfoCount)(UINT * pctinfo);
    STDMETHOD(GetTypeInfo)(
      UINT itinfo,
      LCID lcid,
      ITypeInfo * * pptinfo);
    STDMETHOD(GetIDsOfNames)(
      REFIID riid,
      OLECHAR * * rgszNames,
      UINT cNames,
      LCID lcid,
      DISPID * rgdispid);
    STDMETHOD(Invoke)(
      DISPID dispidMember,
      REFIID riid,
      LCID lcid,
      WORD wFlags,
      DISPPARAMS * pdispparams,
      VARIANT * pvarResult,
      EXCEPINFO * pexcepinfo,
      UINT * puArgErr);

    // IVileAuto methods
    STDMETHOD(get_Application)(IVileAuto **ppvile);
    STDMETHOD(get_FullName)(BSTR *pbstr);
    STDMETHOD(get_InsertMode)(VARIANT_BOOL *pbool);
    STDMETHOD(get_GlobMode)(VARIANT_BOOL *pbool);
    STDMETHOD(get_IsMinimized)(VARIANT_BOOL *pbool);
    STDMETHOD(get_MainHwnd)(LONG *phwnd);
    STDMETHOD(get_Name)(BSTR *pbstr);
    STDMETHOD(get_Parent)(IVileAuto **ppvile);
    STDMETHOD(put_Visible)(VARIANT_BOOL bVisible);
    STDMETHOD(get_Visible)(VARIANT_BOOL *pbool);

    // Note that ForegroundWindow() is not very useful on Win2K or XP...
    STDMETHOD(ForegroundWindow)();
    STDMETHOD(Minimize)();
    STDMETHOD(Quit)();
    STDMETHOD(Restore)();
    STDMETHOD(VileKeys)(BSTR keys);
    STDMETHOD(WindowRedirect)(DWORD hwnd);


    // vile_oa methods
    static HRESULT Create(vile_oa **ppvile, BOOL visible);
                                    // Creates and initializes Vile object
    vile_oa();
    ~vile_oa();

public:
    BSTR m_bstrName;               // Name of application.

private:
    LPTYPEINFO m_ptinfo;           // Vile application type information
    BSTR m_bstrFullName;           // Full name of application.
    VARIANT_BOOL m_bVisible;       // Editor visible?
    ULONG m_cRef;                  // Reference count
    HWND m_hwnd;                   // Winvile's window handle.
};

class vile_oa_cf : public IClassFactory
{
public:
    // IUnknown methods
    STDMETHOD(QueryInterface)(REFIID riid, LPVOID * ppvObj);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

    // IClassFactory methods
    STDMETHOD(CreateInstance)(IUnknown * punkOuter, REFIID riid,
                              void * * ppv);
    STDMETHOD(LockServer)(BOOL fLock);

    vile_oa_cf();

private:
    ULONG m_cRef;                   // Reference count
};

#endif
