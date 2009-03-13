/* oleauto.h - declarations for oleauto.cpp */

#ifndef OLEAUTO_H
#define OLEAUTO_H

#define PROGNAM "VisVile"

#include "w32ole.h"    // located one directory "up".

typedef struct visvile_opts_struct
{
    DWORD cd_doc_dir;    // Boolean, T -> winvile cd's to directory of opened
                         //               text document.
    DWORD close_ds_doc;  // Boolean, T -> close DevStudio copy of document
                         //               just opened by winvile.
    DWORD enabled;       // Boolean, T -> add-in enabled?
    DWORD sync_errbuf;   // Boolean, T -> after a build completes with errors
                         //               or warnings, attempt to open the
                         //               build log file (if it exists) in
                         //               winvile's error buffer.
    DWORD write_buffers; // Boolean, T -> write modified buffers to disk
                         //               prior to build.
    DWORD key_redir;     // Boolean, T -> redirect selected Winvile keystrokes
                         //               to DevStudio.
} VISVILE_OPTS;

/////////////////////////////////////////////////////////////////////////////

// Class to control the winvile ole automation server.
class CVile
{
    private:
        IVileAuto *pVileAuto;
        HWND      ds_hwnd;     // Handle to DevStudio window, NULL if
                               // handle can't be determined.

    public:
        CVile(HWND ds_handle)  { pVileAuto = NULL; ds_hwnd = ds_handle; }
        ~CVile() { Disconnect(); }
        HRESULT  Connect(int restore_wdw, VARIANT_BOOL *in_insert_mode);
        bool     Connected();
                        // Is visvile connected to winvile?
        void     Disconnect();
        HWND     DsHwnd() { return ds_hwnd; }
        HRESULT  FileOpen(BSTR filename, long lineno);
                        // A negative lineno is the same as BOF.
        void     FgWindow();
                        // pop winvile to foreground
        HRESULT  VileCmd(const char *cmd,
                         bool       restore_wdw,
                         bool       force_connection = TRUE);
                        // Run an arbitrary command.
        HRESULT  key_redir_change(int enable);
                        // Enable/disable key redirection.
};


/////////////////////////////////////////////////////////////////////////////
// CConfigDlg dialog

class CConfigDlg : public CDialog
{
// Construction
public:
	virtual BOOL OnInitDialog();
	CConfigDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CConfigDlg)
	enum { IDD = IDD_DIALOG1 };
	BOOL	m_enabled;
	BOOL	m_sync_errbuf;
	BOOL	m_close_ds_doc;
	BOOL	m_cd_doc_dir;
	BOOL	m_write_buffers;
	BOOL	m_key_redir;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConfigDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
    void update_dlg_control_states(bool dialog_up);
    void update_dlg_control(int ctrl_id, bool dialog_up);

	// Generated message map functions
	//{{AFX_MSG(CConfigDlg)
	afx_msg void OnEnabled();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


// *************************** Generic externs **********************

extern VISVILE_OPTS visvile_opts;

extern void         oleauto_exit(void);
extern HRESULT      oleauto_init(void);
extern CVile        *pVile;
extern HRESULT      ReportLastError(HRESULT err);
extern void         visvile_regdata_dirty(void);

#endif
