// Commands.cpp : implementation file
//

#include "stdafx.h"
#include "VisVile.h"
#include "Commands.h"

#include <initguid.h>
#include "oleauto.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

typedef struct openfile_opts_struct
{
    BOOL goto_line_number;         /* T -> extract document line number from
                                    * current docu selection.
                                    */
    BOOL ck_document_error;        /* T -> If can't open document, report
                                    * error, else assume that error indicates
                                    * attempt to open non-TEXT document.
                                    */
    BOOL honor_addin_state;        /* T -> If addin disabled, do nothing. */
    BOOL never_close;              /* T -> regardless of current configuration,
                                    * don't close the devstudio editor's copy
                                    * of the open document.
                                    */

} OPENFILE_OPTS;

/////////////////////////////////////////////////////////////////////////////
// CCommands

CCommands::CCommands()
{
    m_pApplication          = NULL;
    m_pApplicationEventsObj = NULL;
    m_pDebuggerEventsObj    = NULL;
}

CCommands::~CCommands()
{
    if (m_pApplication)
        m_pApplication->Release();
}

BOOL CCommands::SetApplicationObject(IApplication* pApplication)
{
    // This function assumes pApplication has already been AddRef'd
    //  for us, which CDSAddIn did in its QueryInterface call
    //  just before it called us.
    m_pApplication = pApplication;

    // Create Application event handlers
    XApplicationEventsObj::CreateInstance(&m_pApplicationEventsObj);
    if (m_pApplicationEventsObj)
    {
        m_pApplicationEventsObj->AddRef();
        m_pApplicationEventsObj->Connect(m_pApplication);
        m_pApplicationEventsObj->m_pCommands = this;
    }
    else
        return (FALSE);

#ifdef WANT_DEBUGGER_EVENTS
    // Create Debugger event handler
    // FIXME -- stupid code, assumes no errors
    CComPtr<IDispatch> pDebugger;
    if (SUCCEEDED(m_pApplication->get_Debugger(&pDebugger)) 
        && pDebugger != NULL)
    {
        XDebuggerEventsObj::CreateInstance(&m_pDebuggerEventsObj);
        m_pDebuggerEventsObj->AddRef();
        m_pDebuggerEventsObj->Connect(pDebugger);
        m_pDebuggerEventsObj->m_pCommands = this;
    }
#endif
    return (TRUE);
}

void CCommands::UnadviseFromEvents()
{
    if (m_pApplicationEventsObj)
    {
        m_pApplicationEventsObj->Disconnect(m_pApplication);
        m_pApplicationEventsObj->Release();
        m_pApplicationEventsObj = NULL;
    }

#ifdef WANT_DEBUGGER_EVENTS
    if (m_pDebuggerEventsObj != NULL)
    {
        // Since we were able to connect to the Debugger events, we
        //  should be able to access the Debugger object again to
        //  unadvise from its events (thus the VERIFY_OK below--see stdafx.h).
        CComPtr<IDispatch> pDebugger;
        VERIFY_OK(m_pApplication->get_Debugger(&pDebugger));
        ASSERT (pDebugger != NULL);
        m_pDebuggerEventsObj->Disconnect(pDebugger);
        m_pDebuggerEventsObj->Release();
        m_pDebuggerEventsObj = NULL;
    }
#endif
}


/////////////////////////////////////////////////////////////////////////////
// Event handlers

static HRESULT
openfile(IDispatch *theDocument, OPENFILE_OPTS *opts)
{
    BSTR    filename;
    HRESULT hr;
    long    lineno = -1;

    if (opts->honor_addin_state && ! visvile_opts.enabled)
        return (S_OK);

    // First get the filename

    // Get the document object
    CComQIPtr < ITextDocument, &IID_ITextDocument > pDoc (theDocument);
    if (! pDoc)
    {
        if (opts->ck_document_error)
        {
            ::MessageBox(NULL, 
     "Unexpected OLE error:  VisVile->DevStudio document connection failed.",
                         PROGNAM, 
                         MB_OK|MB_ICONSTOP);
            return (E_UNEXPECTED);
        }
        else
            return (S_OK);    /* Assume not a text document */
        
    }

    // Get the document name
    hr = pDoc->get_FullName(&filename);
    if (FAILED(hr))
        return (ReportLastError(hr));

    if (opts->goto_line_number)
    {
        LPDISPATCH pDispSel;

        // Get a selection object dispatch pointer
        if (SUCCEEDED (pDoc->get_Selection(&pDispSel)))
        {
            // Get the selection object
            CComQIPtr < ITextSelection, &IID_ITextSelection > pSel (pDispSel);

            if (pSel) // Get the selection line number
                pSel->get_CurrentLine(&lineno);

            pDispSel->Release();
        }
        // Else winvile will open file at BOF.
    }

    if (! opts->never_close && visvile_opts.close_ds_doc)
    {
        // Close the document in developer studio
        CComVariant vSaveChanges = dsSaveChangesPrompt;
        DsSaveStatus Saved;

        pDoc->Close(vSaveChanges, &Saved);
    }

    // Open the file in Winvile and position to the selected line
    hr = pVile->FileOpen(filename, lineno);
    SysFreeString(filename);
    if (FAILED(hr))
        ReportLastError(hr);
    return (hr);
}

// Application events

HRESULT CCommands::XApplicationEvents::BeforeBuildStart()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return S_OK;
}

HRESULT CCommands::XApplicationEvents::BuildFinish(long nNumErrors, long nNumWarnings)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return S_OK;
}

HRESULT CCommands::XApplicationEvents::BeforeApplicationShutDown()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return S_OK;
}

HRESULT
CCommands::XApplicationEvents::DocumentOpen(IDispatch* theDocument)
{
    OPENFILE_OPTS opts;

    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    opts.goto_line_number  = TRUE;
    opts.ck_document_error = TRUE;
    opts.honor_addin_state = TRUE;
    opts.never_close       = FALSE;
    return (openfile(theDocument, &opts));
}

HRESULT CCommands::XApplicationEvents::BeforeDocumentClose(IDispatch* theDocument)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return S_OK;
}

HRESULT CCommands::XApplicationEvents::DocumentSave(IDispatch* theDocument)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return S_OK;
}

HRESULT CCommands::XApplicationEvents::NewDocument(IDispatch* theDocument)
{
    OPENFILE_OPTS opts;

    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    opts.goto_line_number  = FALSE;
    opts.ck_document_error = TRUE;
    opts.honor_addin_state = TRUE;
    opts.never_close       = FALSE;
    return (openfile(theDocument, &opts));
}

HRESULT CCommands::XApplicationEvents::WindowActivate(IDispatch* theWindow)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return S_OK;
}

HRESULT CCommands::XApplicationEvents::WindowDeactivate(IDispatch* theWindow)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return S_OK;
}

HRESULT CCommands::XApplicationEvents::WorkspaceOpen()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return S_OK;
}

HRESULT CCommands::XApplicationEvents::WorkspaceClose()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return S_OK;
}

HRESULT CCommands::XApplicationEvents::NewWorkspace()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return S_OK;
}

// Debugger event
HRESULT CCommands::XDebuggerEvents::BreakpointHit(IDispatch* pBreakpoint)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return S_OK;
}


/////////////////////////////////////////////////////////////////////////////
// CCommands methods

STDMETHODIMP CCommands::VisVileConfig() 
{
    CConfigDlg Dlg;

    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    m_pApplication->EnableModeless(VARIANT_FALSE);
    if (Dlg.DoModal() == IDOK)
    {
        visvile_regdata_dirty();   // Be sloppy & assume user changed cfg.
        visvile_opts.cd_doc_dir   = Dlg.m_cd_doc_dir;
        visvile_opts.close_ds_doc = Dlg.m_close_ds_doc;
        visvile_opts.enabled      = Dlg.m_enabled;
        visvile_opts.sync_errbuf  = Dlg.m_sync_errbuf;
    }
    m_pApplication->EnableModeless(VARIANT_TRUE);
    return S_OK;
}

STDMETHODIMP CCommands::VisVileEnable() 
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    if (! visvile_opts.enabled)
    {
        visvile_opts.enabled = TRUE;
        visvile_regdata_dirty();
    }
    return S_OK;
}

STDMETHODIMP CCommands::VisVileDisable() 
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    if (visvile_opts.enabled)
    {
        visvile_opts.enabled = FALSE;
        visvile_regdata_dirty();
    }
    return S_OK;
}

STDMETHODIMP CCommands::VisVileOpenDoc() 
{
    HRESULT       hr;
    OPENFILE_OPTS opts;

    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    // Define dispatch pointers for document and selection objects
    CComPtr < IDispatch > pDispDoc;

    // Get a document object dispatch pointer
    hr = m_pApplication->get_ActiveDocument(&pDispDoc);
    if (FAILED(hr))
        return (ReportLastError(hr));

    opts.goto_line_number  = TRUE;
    opts.ck_document_error = FALSE;
    opts.honor_addin_state = FALSE;
    opts.never_close       = TRUE;
    return (openfile(pDispDoc, &opts));
}
