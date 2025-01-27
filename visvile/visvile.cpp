// VisVile.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include <initguid.h>
#include "VisVile.h"
#include "DSAddIn.h"
#include "Commands.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
	OBJECT_ENTRY(CLSID_DSAddIn, CDSAddIn)
END_OBJECT_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVisVileApp

class CVisVileApp : public CWinApp
{
public:
	CVisVileApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVisVileApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CVisVileApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
// CVisVileApp

BEGIN_MESSAGE_MAP(CVisVileApp, CWinApp)
	//{{AFX_MSG_MAP(CVisVileApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// The one and only CVisVileApp object

CVisVileApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CVisVileApp construction

CVisVileApp::CVisVileApp()
{
}

/////////////////////////////////////////////////////////////////////////////
// CVisVileApp initialization

BOOL CVisVileApp::InitInstance()
{
    _Module.Init(ObjectMap, m_hInstance);
    return CWinApp::InitInstance();
}

int CVisVileApp::ExitInstance()
{
    _Module.Term();
    return CWinApp::ExitInstance();
}

/////////////////////////////////////////////////////////////////////////////
// Special entry points required for inproc servers

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return _Module.GetClassObject(rclsid, riid, ppv);
}

STDAPI DllCanUnloadNow(void)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return (AfxDllCanUnloadNow()==S_OK && _Module.GetLockCount()==0) ? S_OK : S_FALSE;
}

// by exporting DllRegisterServer, you can use regsvr32.exe
STDAPI DllRegisterServer(void)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    HRESULT hRes = S_OK;

    // Registers object, typelib and all interfaces in typelib
    hRes = _Module.RegisterServer(TRUE);
    if (FAILED(hRes))
        return hRes;

    // Register description of this add-in object in its own
    //  "/Description" subkey.
    _ATL_OBJMAP_ENTRY* pEntry = _Module.m_pObjMap;
    CRegKey key;
    LONG lRes = key.Open(HKEY_CLASSES_ROOT, _T("CLSID"));
    if (lRes == ERROR_SUCCESS)
    {
        USES_CONVERSION;
        LPOLESTR lpOleStr;
        StringFromCLSID(*pEntry->pclsid, &lpOleStr);
        LPTSTR lpsz = OLE2T(lpOleStr);

        lRes = key.Open(key, lpsz);
        if (lRes == ERROR_SUCCESS)
        {
            CString strDescription;
            strDescription.LoadString(IDS_VISVILE_DESCRIPTION);
            key.SetKeyValue(_T("Description"), strDescription);
        }
        CoTaskMemFree(lpOleStr);
    }
    if (lRes != ERROR_SUCCESS)
        hRes = HRESULT_FROM_WIN32(lRes);

    return hRes;
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI DllUnregisterServer(void)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    HRESULT hRes = S_OK;
    _Module.UnregisterServer();
    return hRes;
}
