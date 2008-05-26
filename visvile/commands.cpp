// Commands.cpp : implementation file
//

#include "stdafx.h"
#include "VisVile.h"
#include "Commands.h"

#include <initguid.h>
#include "oleauto.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <direct.h>
#include <io.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static char workspace_cwd[FILENAME_MAX + 1];

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

static void
save_workspace_cwd(void)
{
    if (_getcwd(workspace_cwd, sizeof(workspace_cwd)) == NULL)
        workspace_cwd[0] = '\0';  // I suppose this might happen
}

static HRESULT
fetch_workspace_logname(CCommands *cmdp, char *file)
{
    BSTR                bstr_prjname;
    HRESULT             hr;
    CComPtr <IDispatch> pDispProj;

    hr = cmdp->GetApplicationObject()->get_ActiveProject(&pDispProj);
    if (FAILED(hr))
        return (ReportLastError(hr));
    CComQIPtr < IGenericProject, &IID_IGenericProject > pProj(pDispProj);
    if (! pProj)
    {
        ::MessageBox(NULL,
         "Unexpected OLE error:  Unable to access current project object",
                     PROGNAM,
                     MB_OK|MB_ICONSTOP);
        return (E_UNEXPECTED);
    }
    hr  = pProj->get_Name(&bstr_prjname);
    if (FAILED(hr))
        return (ReportLastError(hr));
    sprintf(file, "%S.plg", bstr_prjname);
    SysFreeString(bstr_prjname);
    if (workspace_cwd[0] == '\0')
        save_workspace_cwd();  // as a side effect, update workspace cwd
                               // when necessary (should not be necessary).
    return (hr);
}

static HRESULT
unexpected_docu_error(void)
{
    ::MessageBox(NULL,
     "Unexpected OLE error:  VisVile->DevStudio document connection failed.",
                 PROGNAM,
                 MB_OK|MB_ICONSTOP);
    return (E_UNEXPECTED);
}

static HRESULT
openfile(IDispatch *theDocument, OPENFILE_OPTS *opts)
{
    BSTR    filename;
    HRESULT hr;
    long    lineno = -1;

    if (opts->honor_addin_state && ! visvile_opts.enabled)
        return (S_OK);

    // Process file _if_ a Text Document
    CComQIPtr < IGenericDocument, &IID_IGenericDocument > pGenDoc(theDocument);
    if (! pGenDoc)
        return ((opts->ck_document_error) ? unexpected_docu_error() : S_OK);
    else
    {
        BSTR doc_type;
        int  not_text;

        hr = pGenDoc->get_Type(&doc_type);
        if (FAILED(hr))
            return (ReportLastError(hr));
        not_text = (wcscmp(doc_type, L"Text") != 0);
        SysFreeString(doc_type);
        if (not_text)
            return (S_OK);
    }

    // Okay, get the text document object
    CComQIPtr < ITextDocument, &IID_ITextDocument > pDoc(theDocument);
    if (! pDoc)
        return ((opts->ck_document_error) ? unexpected_docu_error() : S_OK);

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
    HRESULT hr = S_OK;

    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    if (visvile_opts.enabled)
    {
        if (visvile_opts.write_buffers)
        {
            /*
             * The following cryptic vile command flushes all modified
             * buffers to disk and does not prompt for user response.
             *
             * Note:  because DevStudio decides what files need to be
             * rebuilt _before_ this event is fired, the "write_buffers"
             * option isn't quite as useful as it might seem.  As a
             * workaround, press the "build" button/key (F7) twice :-) .
             * See ../doc/visvile.doc for further details.
             */

            hr = pVile->VileCmd("1:ww\n", FALSE, FALSE);
            if (FAILED(hr))
                ReportLastError(hr);
        }
        if (visvile_opts.sync_errbuf)
        {
            char logpath[FILENAME_MAX + 1], file[FILENAME_MAX + 1];

            /*
             * The build log (activated from the Tools -> Options -> Build
             * dialog box) may be accessed using this path:
             *
             * <workspace_cwd>\\<workspace_logname>
             */

            hr = fetch_workspace_logname(m_pCommands, file);
            if (SUCCEEDED(hr))
            {
                sprintf(logpath, "%s\\%s", workspace_cwd, file);

                /*
                 * Nuke old logfile to ensure that the "wait-file" winvile
                 * command invoked by BuildFinish() below picks up a fresh
                 * logfile.
                 */
                (void) remove(logpath);
            }
        }
    }
    return (hr);
}

HRESULT
CCommands::XApplicationEvents::BuildFinish(long nNumErrors, long nNumWarnings)
{
    HRESULT hr = S_OK;

    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    if (visvile_opts.enabled &&
                        visvile_opts.sync_errbuf &&
                                    (nNumErrors || nNumWarnings))
    {
        char cmd[FILENAME_MAX * 5 + 1],
             path[FILENAME_MAX + 1],
             file[FILENAME_MAX + 1];

        /*
         * Suck the current active project's build log into winvile's error
         * buffer.  If the build log doesn't exist, the user will be annoyed
         * by an editor that pops up an empty buffer :-) .
         *
         * The build log (activated from the Tools -> Options -> Build dialog
         * box) may be accessed using this path:
         *
         *    <workspace_cwd>\\<workspace_logname>
         */
        hr = fetch_workspace_logname(m_pCommands, file);
        if (SUCCEEDED(hr))
        {
            sprintf(path, "%s\\%s", workspace_cwd, file);
            sprintf(cmd,
 ":e [History]\n:kill %s\n:cd %s\n:wait-file %s\n:view %s\n:error-buffer %s\n",
                    file,
                    workspace_cwd,
                    path,
                    path,
                    file);
            pVile->VileCmd(cmd, TRUE, TRUE);
        }
    }
    return (hr);
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
    save_workspace_cwd();
    return (S_OK);
}

HRESULT CCommands::XApplicationEvents::WorkspaceClose()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    workspace_cwd[0] = '\0';     // Erase saved cwd.
    return (S_OK);
}

HRESULT CCommands::XApplicationEvents::NewWorkspace()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    save_workspace_cwd();
    return (S_OK);
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
    HRESULT    hr = S_OK;

    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    m_pApplication->EnableModeless(VARIANT_FALSE);
    if (Dlg.DoModal() == IDOK)
    {
        visvile_regdata_dirty();   // Be sloppy & assume user changed cfg.
        visvile_opts.cd_doc_dir    = Dlg.m_cd_doc_dir;
        visvile_opts.close_ds_doc  = Dlg.m_close_ds_doc;
        visvile_opts.enabled       = Dlg.m_enabled;
        visvile_opts.sync_errbuf   = Dlg.m_sync_errbuf;
        visvile_opts.write_buffers = Dlg.m_write_buffers;
        if (visvile_opts.key_redir != (DWORD) Dlg.m_key_redir)
        {
            // Immediately update key redirection state with winvile,
            // assuming editor is connected as OLE automation server.

            hr = pVile->key_redir_change(Dlg.m_key_redir);
            if (FAILED(hr))
                (void) ReportLastError(hr);
        }
        visvile_opts.key_redir     = Dlg.m_key_redir;
    }
    m_pApplication->EnableModeless(VARIANT_TRUE);
    return (hr);
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

