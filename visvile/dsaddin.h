// DSAddIn.h : header file
//

#if !defined(AFX_DSADDIN_H__E4C5E2F6_36FF_11D2_B8E4_0020AF0F4354__INCLUDED_)
#define AFX_DSADDIN_H__E4C5E2F6_36FF_11D2_B8E4_0020AF0F4354__INCLUDED_

#include "commands.h"

// {E4C5E2E3-36FF-11D2-B8E4-0020AF0F4354}
DEFINE_GUID(CLSID_DSAddIn,
0xe4c5e2e3, 0x36ff, 0x11d2, 0xb8, 0xe4, 0, 0x20, 0xaf, 0xf, 0x43, 0x54);

/////////////////////////////////////////////////////////////////////////////
// CDSAddIn

class CDSAddIn : 
	public IDSAddIn,
	public CComObjectRoot,
	public CComCoClass<CDSAddIn, &CLSID_DSAddIn>
{
public:
	DECLARE_REGISTRY(CDSAddIn, "VisVile.DSAddIn.1",
		"VISVILE Developer Studio Add-in", IDS_VISVILE_LONGNAME,
		THREADFLAGS_BOTH)

	CDSAddIn() {}
	BEGIN_COM_MAP(CDSAddIn)
		COM_INTERFACE_ENTRY(IDSAddIn)
	END_COM_MAP()
	DECLARE_NOT_AGGREGATABLE(CDSAddIn)

// IDSAddIns
public:
	STDMETHOD(OnConnection)(THIS_ IApplication* pApp, VARIANT_BOOL bFirstTime,
		long dwCookie, VARIANT_BOOL* OnConnection);
	STDMETHOD(OnDisconnection)(THIS_ VARIANT_BOOL bLastTime);

protected:
	bool AddCommand(IApplication* pApp, char* MethodName, char* CmdName,
			 UINT StrResId, UINT GlyphIndex, VARIANT_BOOL bFirstTime);

protected:
	CCommandsObj* m_pCommands;
	DWORD m_dwCookie;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DSADDIN_H__E4C5E2F6_36FF_11D2_B8E4_0020AF0F4354__INCLUDED)
