/*
 * oleauto.cpp - this module:
 *
 * 1) supplies several miscellaneous helper functions,
 * 2) manipulates the winvile OLE automation server, and
 * 3) implements a visvile configuration dialog box.
 */

#include "stdafx.h"
#include "resource.h"
#include <initguid.h>
#include "w32reg.h"    // located in the mainline vile development project
                       // directory, one level "up".
#include "oleauto.h"

#include <string.h>

#define RESTORE_WDW     TRUE
#define NO_RESTORE_WDW  FALSE

// Registry configuration keys and subkeys
#define CD_KEYVAL          "ChdirDocPath"
#define CLOSE_DOC_KEYVAL   "CloseDoc"
#define ENABLE_KEYVAL      "Enable"
#define KEY_REDIRECT       "RedirectWinvileKeys"
#define ROOT_CFG_KEY       "Software\\Winvile\\VisVile"
#define SYNC_ERRBUF_KEYVAL "SyncErrbuf"
#define WRITE_BUFFERS      "WriteModifiedBuffers"

static size_t   ansibuf_len,   /* scaled in bytes   */
                olebuf_len;    /* scaled in wchar_t */
static char     *ansibuf;
static OLECHAR  *olebuf;

static int      regupdate_required;

CVile           *pVile = NULL;
VISVILE_OPTS    visvile_opts;

/* ----------------------------------------------------------------- */

void
visvile_regdata_dirty(void)
{
    regupdate_required = TRUE;
}



/*
 * FUNCTION
 *   fmt_win32_error(HRESULT errcode, char **buf, ULONG buflen)
 *
 *   errcode - win32 error code for which message applies.
 *
 *   buf     - indirect pointer to buf to receive formatted message.  If *buf
 *             is NULL, the buffer is allocated on behalf of the client and
 *             must be free'd using LocalFree().
 *
 *   buflen  - length of buffer (specify 0 if *buf is NULL).
 *
 * DESCRIPTION
 *   Format system error reported by Win32 API.
 *
 * RETURNS
 *   *buf
 */

static char *
fmt_win32_error(HRESULT errcode, char **buf, ULONG buflen)
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



HRESULT
ReportLastError(HRESULT err)
{
    char *buf = NULL, tmp[512];

    if (err != REGDB_E_CLASSNOTREG)
    {
        fmt_win32_error(err, &buf, 0);
        sprintf(tmp,
           "Unexpected error encountered.\r\rError code:  %lx\rDescription: %s",
                err,
                buf);
    }
    else
    {
        strcpy(tmp,
               "Winvile OLE Automation registration lookup failed.\r\r"
               "The command 'winvile -Or' registers an OLE-aware "
               "version of the editor.");
    }

    ::MessageBox(NULL, tmp, PROGNAM, MB_OK|MB_ICONSTOP);
    if (buf)
        LocalFree(buf);
    return (err);
}



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



static DWORD
get_reg_cfg_val(HKEY key, char *value_name, DWORD def_value)
{
    DWORD val_type, result, result_size;

    val_type    = REG_DWORD;
    result_size = sizeof(result);
    if (RegQueryValueEx(key,
                        value_name,
                        NULL,
                        &val_type,
                        (BYTE *) &result,
                        &result_size) != ERROR_SUCCESS)
    {
        // Assume value missing -- use default.

        result = def_value;
        visvile_regdata_dirty();
    }
    return (result);
}



// Read configuration values from HKEY_CURRENT_USER\Software\Winvile\VisVile
static void
get_addin_config(void)
{
    HKEY hk;

    // Supply fallback, default VisVile configuration values.
    visvile_opts.enabled =
        visvile_opts.close_ds_doc =
            visvile_opts.write_buffers = TRUE;

    // All other options default to FALSE....

    // Now, read "old" config values from registry (if they exist).
    if (RegOpenKeyEx(HKEY_CURRENT_USER,
                     ROOT_CFG_KEY,
                     0,
                     KEY_READ,
                     &hk) == ERROR_SUCCESS)
    {
        visvile_opts.cd_doc_dir    = get_reg_cfg_val(hk,
                                                     CD_KEYVAL,
                                                     visvile_opts.cd_doc_dir);
        visvile_opts.close_ds_doc  = get_reg_cfg_val(hk,
                                                     CLOSE_DOC_KEYVAL,
                                                     visvile_opts.close_ds_doc);
        visvile_opts.enabled       = get_reg_cfg_val(hk,
                                                     ENABLE_KEYVAL,
                                                     visvile_opts.enabled);
        visvile_opts.sync_errbuf   = get_reg_cfg_val(hk,
                                                     SYNC_ERRBUF_KEYVAL,
                                                     visvile_opts.sync_errbuf);
        visvile_opts.write_buffers = get_reg_cfg_val(hk,
                                                     WRITE_BUFFERS,
                                                    visvile_opts.write_buffers);

        visvile_opts.key_redir     = pVile->DsHwnd() ?
              get_reg_cfg_val(hk, KEY_REDIRECT, visvile_opts.key_redir) : FALSE;
        RegCloseKey(hk);
    }
    else
    {
        /*
         * No configuration info stored in registry.  Force registry
         * update when visvile exits.
         */

        visvile_regdata_dirty();
    }
}



static void
set_reg_cfg_val(HKEY key, char *value_name, DWORD value)
{
    long rc;

    if ((rc = RegSetValueEx(key,
                            value_name,
                            NULL,
                            REG_DWORD,
                            (BYTE *) &value,
                            sizeof(value))) != ERROR_SUCCESS)
    {
        ReportLastError(rc);
    }
}



// Write configuration values to HKEY_CURRENT_USER\Software\Winvile\VisVile
static void
set_addin_config(void)
{
    HKEY hk;
    LONG rc;

    if ((rc = RegCreateKeyEx(HKEY_CURRENT_USER,
                             ROOT_CFG_KEY,
                             0,
                             NULL,
                             0,
                             KEY_ALL_ACCESS,
                             NULL,
                             &hk,
                             NULL)) != ERROR_SUCCESS)
    {
        ReportLastError(rc);
        return;
    }
    set_reg_cfg_val(hk, CD_KEYVAL,          visvile_opts.cd_doc_dir);
    set_reg_cfg_val(hk, CLOSE_DOC_KEYVAL,   visvile_opts.close_ds_doc);
    set_reg_cfg_val(hk, ENABLE_KEYVAL,      visvile_opts.enabled);
    set_reg_cfg_val(hk, SYNC_ERRBUF_KEYVAL, visvile_opts.sync_errbuf);
    set_reg_cfg_val(hk, WRITE_BUFFERS,      visvile_opts.write_buffers);
    set_reg_cfg_val(hk, KEY_REDIRECT,       visvile_opts.key_redir);
    RegCloseKey(hk);
}



HRESULT
oleauto_init(void)
{
    olebuf_len = ansibuf_len = 512;
    ansibuf    = (char *) malloc(ansibuf_len);
    olebuf     = (OLECHAR *) malloc(olebuf_len * sizeof(OLECHAR));

    /*
     * CVile's constructor argument extracts a handle to the DevStudio 5
     * frame using a sequence of win32 calls that was determined by trial
     * and error.  'Twill be interesting to see if this works with later
     * DevStudio versions.  [works with DevStudio 6.0]
     */
    pVile = new CVile(::GetParent(::GetActiveWindow()));
    get_addin_config();  // Execute this code _after_ creating pVile.
    if (! (olebuf && ansibuf && pVile))
        return (ReportLastError(E_OUTOFMEMORY));
    else
        return (S_OK);
}

void
oleauto_exit(void)
{
    if (regupdate_required)
        set_addin_config();
    if (pVile)
    {
        (void) pVile->key_redir_change(FALSE);  // key redirection killed
        delete pVile;
    }
    if (olebuf)
        free(olebuf);
    if (ansibuf)
        free(ansibuf);
}

/* ------------------------------ CVile ------------------------- */

void
CVile::Disconnect()
{
    if (pVileAuto)
    {
        (void) pVileAuto->Release();
        pVileAuto = NULL;
    }
}



bool
CVile::Connected()
{
    bool         connected = FALSE;
    VARIANT_BOOL visible;

    if (pVileAuto)
    {
        /*
         * We have a connection to a winvile instance (maybe).  Obtain
         * winvile application visibility state to verify connection.
         */

        connected = SUCCEEDED(pVileAuto->get_Visible(&visible));
    }
    return (connected);
}



// Enable or disable key redirection from winvile to DevStudio.
HRESULT
CVile::key_redir_change(int enable)
{
    HRESULT hr = S_OK;

    if (Connected())
    {
        hr = pVileAuto->WindowRedirect((enable) ? (DWORD) ds_hwnd : NULL);
        if (FAILED(hr))
            Disconnect(); // Certainly didn't expect that.
    }
    return (hr);
}



HRESULT
CVile::Connect(int restore_wdw, VARIANT_BOOL *in_insert_mode)
{
    HRESULT hr;

    *in_insert_mode = VARIANT_TRUE;   // Set a known, safe value.
    if (pVileAuto)
    {
        VARIANT_BOOL visible;

        /*
         * We have a connection to a winvile instance (maybe).  First obtain
         * winvile application visibility state, which provides a true test
         * of whether or not the connection exists.
         */
        hr = pVileAuto->get_Visible(&visible);
        if (FAILED(hr))
        {
            // Assume the user has closed our previous editor instance.

            Disconnect();  // kills editor connection and nulls pVileAuto.
        }
        else if (restore_wdw && ! visible)
        {
            hr = pVileAuto->put_Visible(TRUE);
            if (FAILED(hr))
                Disconnect(); // Certainly didn't expect that.
        }
    }
    if (! pVileAuto)
    {
        LPUNKNOWN punk;

        hr = CoCreateInstance(CLSID_VileAuto,
                              NULL,
                              CLSCTX_LOCAL_SERVER,
                              IID_IUnknown,
                              (void **) &punk);
        if (FAILED(hr))
            return (hr);
        hr = punk->QueryInterface(IID_IVileAuto, (void **) &pVileAuto);
        punk->Release();
        if (FAILED(hr))
            return (hr);
        if (restore_wdw)
        {
            hr = pVileAuto->put_Visible(TRUE);   // Make the editor visible.
            if (FAILED(hr))
                Disconnect(); // Certainly didn't expect that.
        }
        if (pVileAuto && visvile_opts.key_redir)
        {
            // Send Winvile the HWND that receives redirected keystrokes.

            hr  = pVileAuto->WindowRedirect((DWORD) ds_hwnd);
            if (FAILED(hr))
                Disconnect(); // Certainly didn't expect that.
        }
    }
    if (SUCCEEDED(hr))
    {
        if (restore_wdw)
        {
            VARIANT_BOOL minimized;

            /* Restore editor if minimized. */

            hr = pVileAuto->get_IsMinimized(&minimized);
            if (SUCCEEDED(hr) && minimized)
                hr = pVileAuto->Restore();
        }
        if (SUCCEEDED(hr))
        {
            // return editor's "mode" state to client

            hr = pVileAuto->get_InsertMode(in_insert_mode);
        }
    }
    return (hr);
}



HRESULT
CVile::FileOpen(BSTR filename, long lineno)
{
    BSTR         bcmd;
    char         cmd[1024], line[64], cd[512];
    HRESULT      hr;
    VARIANT_BOOL in_insert_mode;        // editor in insert mode?
    OLECHAR      *ole;

    hr = Connect(RESTORE_WDW, &in_insert_mode);
    if (SUCCEEDED(hr))
    {
        sprintf(cmd,
                "%s:e %S\n",
                (in_insert_mode) ? "\033" : "",   // prepend ESC if necessary
                filename);
        if (visvile_opts.cd_doc_dir)
        {
            char *tmp, *cp;

            if ((tmp = FROM_OLE_STRING(filename)) == NULL)
                return (E_OUTOFMEMORY);
            if ((cp = strrchr(tmp, '\\')) != NULL)
            {
                *cp = '\0';
                sprintf(cd, ":cd %s\n", tmp);
                *cp = '\\';
                strcat(cmd, cd);
            }
        }
        if (lineno > 1)
        {
            sprintf(line, ":%ld\n", lineno);
            strcat(cmd, line);
        }
        ole  = TO_OLE_STRING(cmd);
        bcmd = SysAllocString(ole);
        if (! (bcmd && ole))
            return (E_OUTOFMEMORY);
        hr = pVileAuto->VileKeys(bcmd);
        SysFreeString(bcmd);  // don't need to free "ole", it points at a
                              // global, dynamic tmp buffer.
        if (SUCCEEDED(hr))
            FgWindow();
    }
    return (hr);
}



void
CVile::FgWindow()
{
    HWND hwnd;

    // Set foreground window using a method that's compatible with win2k
    pVileAuto->get_MainHwnd((LONG *) &hwnd);
    (void) ::SetForegroundWindow(hwnd);
}



HRESULT
CVile::VileCmd(
    const char *cmd,
    bool       restore_wdw,     // T -> make editor visible and pop its window
                                //      on top.
    bool       force_connection // T -> force connection to editor if
                                //      necessary.
                                // F -> don't execute command if editor
                                //      disconnected.
              )
{
    BSTR         bcmd;
    HRESULT      hr;
    VARIANT_BOOL in_insert_mode;        // editor in insert mode?
    OLECHAR      *ole;
    char         *scratch = NULL;

    if (! force_connection && ! Connected())
    {
        // The command should not be executed.

        return (S_OK);
    }

    hr = Connect(restore_wdw ? RESTORE_WDW : NO_RESTORE_WDW, &in_insert_mode);
    if (SUCCEEDED(hr))
    {
        if (in_insert_mode)
        {
            // Prepend ESC to command.

            if ((scratch = (char *) malloc(strlen(cmd) + 2)) == NULL)
                return (E_OUTOFMEMORY);
            scratch[0] = '\033';   // ESC
            strcpy(scratch + 1, cmd);
        }
        ole  = TO_OLE_STRING((scratch) ? (char *) scratch : (char *) cmd);
        bcmd = SysAllocString(ole);
        if (! (bcmd && ole))
            return (E_OUTOFMEMORY);
        if (restore_wdw)
        {
            /*
             * Pop editor to the front.  Note that there's a subtle
             * sequencing problem here.  Some command strings will require
             * restoration of the editor's window _before_ the string is
             * executed.  Why?  The string may be attempting to open a file
             * that's created as a side effect of a DevStudio operation
             * that _follows_ execution of "cmd".  In that situation,
             * visvile can't be waiting for the editor to finish execting
             * "cmd" before directing the editor to pop to the front.  Why?
             * 'Cause until visvile returns control to DevStudio, DevStudio
             * won't create the target file.  Nasty, eh?  Sure wasted an
             * evening of my life.
             */

            FgWindow(); // Pop editor to front.
        }
        hr = pVileAuto->VileKeys(bcmd);
        SysFreeString(bcmd);  // don't need to free "ole", it points at a
                              // global, dynamic tmp buffer.
        if (scratch)
            free(scratch);
    }
    return (hr);
}


/////////////////////////////////////////////////////////////////////////////
// CConfigDlg dialog


CConfigDlg::CConfigDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CConfigDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConfigDlg)
	m_enabled = FALSE;
	m_sync_errbuf = FALSE;
	m_close_ds_doc = FALSE;
	m_cd_doc_dir = FALSE;
	m_write_buffers = FALSE;
	m_key_redir = FALSE;
	//}}AFX_DATA_INIT
}


void CConfigDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfigDlg)
	DDX_Check(pDX, IDC_ENABLED, m_enabled);
	DDX_Check(pDX, IDC_ERRBUF, m_sync_errbuf);
	DDX_Check(pDX, IDC_CLOSE_DOC, m_close_ds_doc);
	DDX_Check(pDX, IDC_CHDIR, m_cd_doc_dir);
	DDX_Check(pDX, IDC_WRITE_BUFFERS, m_write_buffers);
	DDX_Check(pDX, IDC_KEY_REDIR, m_key_redir);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CConfigDlg, CDialog)
	//{{AFX_MSG_MAP(CConfigDlg)
	ON_BN_CLICKED(IDC_ENABLED, OnEnabled)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfigDlg message handlers

BOOL CConfigDlg::OnInitDialog()
{
    m_cd_doc_dir    = visvile_opts.cd_doc_dir;
    m_close_ds_doc  = visvile_opts.close_ds_doc;
    m_enabled       = visvile_opts.enabled;
    m_sync_errbuf   = visvile_opts.sync_errbuf;
    m_write_buffers = visvile_opts.write_buffers;
    m_key_redir     = visvile_opts.key_redir;

    /*
     * Now, depending on value of m_enabled, manually enable/disable all
     * other controls.  I'm doing this because I'm learning MFC/Windows
     * programming, not because I particularly care if the dialog box
     * responds to user input or not.
     */
    update_dlg_control_states(FALSE);
    return (CDialog::OnInitDialog()); }

void
CConfigDlg::OnEnabled()
{
    // Called when user clicks the Enabled check box.
    update_dlg_control_states(TRUE);
}

void
CConfigDlg::update_dlg_control_states(bool dialog_up)
{
    if (dialog_up)
        UpdateData(TRUE);     // copy current dlg data into member vars.
    update_dlg_control(IDC_CLOSE_DOC, dialog_up);
    update_dlg_control(IDC_CHDIR, dialog_up);
    update_dlg_control(IDC_WRITE_BUFFERS, dialog_up);
    update_dlg_control(IDC_ERRBUF, dialog_up);
    update_dlg_control(IDC_KEY_REDIR, dialog_up);
}

void
CConfigDlg::update_dlg_control(int ctrl_id, bool dialog_up)
{
    CWnd *hctrl;

    if ((hctrl = GetDlgItem(ctrl_id)) != NULL)
    {
        if (ctrl_id != IDC_KEY_REDIR)
            hctrl->EnableWindow(m_enabled);
        else
        {
            /*
             * The enable key redirect feature is _always_ enabled so long
             * as the DevStudio window handle is not NULL.  Keeping the
             * redirect feature enabled at all times permits the winvile
             * user to redirect keys to DevStudio even if the add-in is
             * disabled (this is a good thing).
             */

            hctrl->EnableWindow(pVile->DsHwnd() != NULL);
        }
        if (dialog_up)
            hctrl->UpdateWindow();
    }
}
