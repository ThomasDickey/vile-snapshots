// AddInMod.cpp : implementation file
//

#include "stdafx.h"
#include "VisVile.h"
#include "DSAddIn.h"
#include "Commands.h"
#include "oleauto.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// This is called when the user first loads the add-in, and on start-up
//  of each subsequent Developer Studio session
STDMETHODIMP CDSAddIn::OnConnection(IApplication* pApp, VARIANT_BOOL bFirstTime,
		long dwCookie, VARIANT_BOOL* OnConnection)
{
    HRESULT hr;

	AFX_MANAGE_STATE(AfxGetStaticModuleState());
    *OnConnection = VARIANT_FALSE;
    hr            = oleauto_init();
    if (FAILED(hr))
        return (hr);
	
	// Store info passed to us
	IApplication* pApplication = NULL;
	hr = pApp->QueryInterface(IID_IApplication, (void**) &pApplication);
    if (FAILED(hr))
        return (ReportLastError(hr));

	m_dwCookie = dwCookie;

	// Create command dispatch, send info back to DevStudio
	CCommandsObj::CreateInstance(&m_pCommands);
    if (! m_pCommands)
    {
        ::MessageBox(NULL, 
               "Unexpected OLE error:  CCommandsObj::CreateInstance() failed.",
                     PROGNAM, 
                     MB_OK|MB_ICONSTOP);
		return (E_UNEXPECTED);
    }
    m_pCommands->AddRef();

	// The QueryInterface above AddRef'd the Application object.  It will
	//  be Release'd in CCommand's destructor.
	if (! m_pCommands->SetApplicationObject(pApplication))
    {
        ::MessageBox(NULL, 
            "Unexpected OLE error:  CCommands::SetApplicationObject() failed.",
                     PROGNAM, 
                     MB_OK|MB_ICONSTOP);
		return (E_UNEXPECTED);
    }
    hr = pApplication->SetAddInInfo((long) AfxGetInstanceHandle(),
                                    (LPDISPATCH) m_pCommands, 
                                    IDR_TOOLBAR_MEDIUM, 
                                    IDR_TOOLBAR_LARGE, 
                                    m_dwCookie);
    if (FAILED(hr))
        return (ReportLastError(hr));

	// Inform DevStudio of the commands we implement
	if (! AddCommand(pApplication, 
                     "VisVileConfig", 
                     "VisVileConfigCmd",
			         IDS_CMD_CONFIG, 
                     0,
                     bFirstTime))
    {
		return (E_UNEXPECTED);
    }
	if (! AddCommand(pApplication, 
                     "VisVileEnable", 
                     "VisVileEnableCmd",
			         IDS_CMD_ENABLE, 
                     1,
                     bFirstTime))
    {
		return (E_UNEXPECTED);
    }
	if (! AddCommand(pApplication, 
                     "VisVileDisable", 
                     "VisVileDisableCmd",
			         IDS_CMD_DISABLE, 
                     2,
                     bFirstTime))
    {
		return (E_UNEXPECTED);
    }
	if (! AddCommand(pApplication,
                     "VisVileOpenDoc", 
                     "VisVileOpenDocCmd",
			         IDS_CMD_LOAD, 
                     3,
                     bFirstTime))
    {
		return (E_UNEXPECTED);
    }
	*OnConnection = VARIANT_TRUE;
	return (S_OK);
}

// This is called on shut-down, and also when the user unloads the add-in
STDMETHODIMP CDSAddIn::OnDisconnection(VARIANT_BOOL bLastTime)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	m_pCommands->UnadviseFromEvents();
	m_pCommands->Release();
	m_pCommands = NULL;
    oleauto_exit();
	return (S_OK);
}

// Add a command to DevStudio
// Creates a toolbar button for the command also.
// 'MethodName' is the name of the methode specified in the .odl file
// 'StrResId' the resource id of the descriptive string
// 'GlyphIndex' the image index into the command buttons bitmap
// Return true on success
//
bool CDSAddIn::AddCommand(IApplication *pApp, 
                          char         *MethodName, 
                          char         *CmdName,
			              UINT         StrResId, 
                          UINT         GlyphIndex,
                          VARIANT_BOOL bFirstTime)
{
	VARIANT_BOOL bRet;
	CString      CmdString, CmdText;

	CmdText.LoadString (StrResId);
	CmdString = CmdName;
	CmdString += CmdText;

	CComBSTR bszCmdString (CmdString);
	CComBSTR bszMethod (MethodName);
	CComBSTR bszCmdName (CmdName);

	pApp->AddCommand(bszCmdString, bszMethod, GlyphIndex, m_dwCookie, &bRet);
    if (bRet == VARIANT_FALSE)
    {
        CString tmp;

        tmp  = "Duplicate command installation rejected\r\rDuplicate name: ";
        tmp += MethodName;
        ::MessageBox(NULL, 
                     tmp,
                     PROGNAM, 
                     MB_OK|MB_ICONSTOP);
    }

	// Add toolbar buttons only if this is the first time the add-in
	//  is being loaded.  Toolbar buttons are automatically remembered
	//  by Developer Studio from session to session, so we should only
	//  add the toolbar buttons once.
	if (bFirstTime == VARIANT_TRUE)
	{
		HRESULT hr;

		hr = pApp->AddCommandBarButton(dsGlyph, bszCmdName, m_dwCookie);
        if (FAILED(hr))
        {
            (void) ReportLastError(hr);
            bRet = VARIANT_FALSE;
        }
	}
    return ((bRet == VARIANT_TRUE) ? TRUE : FALSE);
}
