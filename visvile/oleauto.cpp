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

#define RESTORE_WDW     TRUE
#define NO_RESTORE_WDW  FALSE

// Registry configuration keys and subkeys
#define CD_KEYVAL          "ChdirDocPath"
#define CLOSE_DOC_KEYVAL   "CloseDoc"
#define ENABLE_KEYVAL      "Enable"
#define ROOT_CFG_KEY       "Software\\Winvile\\VisVile"
#define SYNC_ERRBUF_KEYVAL "SyncErrbuf"

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
            visvile_opts.sync_errbuf = TRUE;

    // All other options default to FALSE....

    // Now, read "old" config values from registry (if they exist).
    if (RegOpenKeyEx(HKEY_CURRENT_USER,
                     ROOT_CFG_KEY,
                     0, 
                     KEY_READ,
                     &hk) == ERROR_SUCCESS)
    {
        visvile_opts.cd_doc_dir   = get_reg_cfg_val(hk,
                                                    CD_KEYVAL,
                                                    visvile_opts.cd_doc_dir);
        visvile_opts.close_ds_doc = get_reg_cfg_val(hk,
                                                    CLOSE_DOC_KEYVAL,
                                                    visvile_opts.close_ds_doc);
        visvile_opts.enabled      = get_reg_cfg_val(hk,
                                                    ENABLE_KEYVAL,
                                                    visvile_opts.enabled);
        visvile_opts.sync_errbuf  = get_reg_cfg_val(hk,
                                                    SYNC_ERRBUF_KEYVAL,
                                                    visvile_opts.sync_errbuf);
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
    RegCloseKey(hk);
}



HRESULT
oleauto_init(void)
{
    get_addin_config();
    olebuf_len = ansibuf_len = 512;
    ansibuf    = (char *) malloc(ansibuf_len);
    olebuf     = (OLECHAR *) malloc(olebuf_len * sizeof(OLECHAR));
    pVile      = new CVile();
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
        delete pVile;
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



HRESULT
CVile::Connect(int restore_wdw, VARIANT_BOOL *in_insert_mode)
{
    HRESULT hr;

    *in_insert_mode = VARIANT_TRUE;   // Set a known, safe value.
    if (pVileAuto)
    {
        /*
         * We have a connection to a winvile instance (maybe).  Force
         * winvile application to become visible, which is important.  This
         * action also allows us to determine if the connection is still
         * intact.
         */

        hr = pVileAuto->put_Visible(TRUE);
        if (FAILED(hr))
        {
            /* We'll assume the user has closed our previous editor instance. */

            Disconnect();  // kills editor connection and nulls pVileAuto.
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
        hr = pVileAuto->put_Visible(TRUE);   /* Make the editor visible. */
        if (FAILED(hr))  
        {
            /* Certainly didn't expect that. */

            Disconnect();
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
    char         cmd[512], line[128];
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
            hr = pVileAuto->ForegroundWindow();  // Pop editor to front.
    }
    return (hr);
}



HRESULT
CVile::VileCmd(const char *cmd)
{
    BSTR         bcmd;
    HRESULT      hr;
    VARIANT_BOOL in_insert_mode;        // editor in insert mode?
    OLECHAR      *ole;
    char         *scratch = NULL;

    hr = Connect(RESTORE_WDW, &in_insert_mode);
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
        hr = pVileAuto->VileKeys(bcmd);
        SysFreeString(bcmd);  // don't need to free "ole", it points at a
                              // global, dynamic tmp buffer.
        if (scratch)
            free(scratch);
        if (SUCCEEDED(hr))
            hr = pVileAuto->ForegroundWindow();  // Pop editor to front.
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
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CConfigDlg, CDialog)
	//{{AFX_MSG_MAP(CConfigDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfigDlg message handlers

BOOL CConfigDlg::OnInitDialog()
{
    m_cd_doc_dir   = visvile_opts.cd_doc_dir;
    m_close_ds_doc = visvile_opts.close_ds_doc;
    m_enabled      = visvile_opts.enabled;
    m_sync_errbuf  = visvile_opts.sync_errbuf;
    return (CDialog::OnInitDialog());
}
