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
} VISVILE_OPTS;

/////////////////////////////////////////////////////////////////////////////

// Class to control the winvile ole automation server.
class CVile
{
    private:
        IVileAuto *pVileAuto;

    public:
        CVile()  { pVileAuto = NULL; }
        ~CVile() { Disconnect(); }
        HRESULT  Connect(int restore_wdw, VARIANT_BOOL *in_insert_mode);
        void     Disconnect();
        HRESULT  FileOpen(BSTR filename, long lineno);
                        // A negative lineno is the same as BOF.
        HRESULT  VileCmd(const char *cmd);
                        // Run an arbitrary command.
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
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConfigDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CConfigDlg)
		// NOTE: the ClassWizard will add member functions here
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
