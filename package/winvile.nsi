; $Id: winvile.nsi,v 1.6 2010/08/08 17:56:43 tom Exp $
; Script generated with the Venis Install Wizard

; Define your application name
!define APPNAME "WinVile"
!define APPNAMEANDVERSION "WinVile 9.8"

; Main Install settings
Name "${APPNAMEANDVERSION}"
InstallDir "$PROGRAMFILES\WinVile"
InstallDirRegKey HKLM "Software\${APPNAME}" "NSIS install_dir"
OutFile "NSIS-Output\OutFile.exe"

; Modern interface settings
!include "MUI.nsh"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\winvile-ole.exe"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\COPYING"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Set languages (first is default language)
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_RESERVEFILE_LANGDLL

InstType "Full"		; SectionIn 1
InstType "Typical"	; SectionIn 2
InstType "Minimal"	; SectionIn 3

Section "WinVile" Section1

	SectionIn 1 2 3

	; Set Section properties
	SetOverwrite on

	; Set Section Files and Shortcuts
	SetOutPath "$INSTDIR\"
	File "..\bin\winvile-ole.exe"
	File "..\vile.hlp"
	CreateShortCut "$DESKTOP\WinVile.lnk" "$INSTDIR\winvile-ole.exe"
	CreateDirectory "$SMPROGRAMS\WinVile"
	CreateShortCut "$SMPROGRAMS\WinVile\WinVile.lnk" "$INSTDIR\winvile-ole.exe"
	CreateShortCut "$SMPROGRAMS\WinVile\Uninstall.lnk" "$INSTDIR\uninstall.exe"

SectionEnd

Section "macros" Section2

	SectionIn 1 2

	; Set Section properties
	SetOverwrite on

	; Set Section Files and Shortcuts
	SetOutPath "$INSTDIR\macros"
	File "..\filters\*.rc"
	File "..\macros\*.rc"

SectionEnd

Section "filters" Section3

	SectionIn 1

	; Set Section properties
	SetOverwrite on

	; Set Section Files and Shortcuts
	SetOutPath "$INSTDIR\filters"
	File "c:\vile\*.exe"

SectionEnd

Section -FinishSection

	WriteRegStr HKLM "Software\${APPNAME}" "" "$INSTDIR"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" "$INSTDIR\uninstall.exe"
	WriteUninstaller "$INSTDIR\uninstall.exe"

SectionEnd

; Modern install component descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${Section1} "VI Like Emacs"
	!insertmacro MUI_DESCRIPTION_TEXT ${Section2} "Initialization script and useful macros"
	!insertmacro MUI_DESCRIPTION_TEXT ${Section3} "Syntax (highlighting) filters.  Most are compiled-in, but some are useful as stand-along programs."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;Uninstall section
Section Uninstall

	;Remove from registry...
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
	DeleteRegKey HKLM "SOFTWARE\${APPNAME}"

	; Delete self
	Delete "$INSTDIR\uninstall.exe"

	; Delete Shortcuts
	Delete "$DESKTOP\WinVile.lnk"
	Delete "$SMPROGRAMS\WinVile\WinVile.lnk"
	Delete "$SMPROGRAMS\WinVile\Uninstall.lnk"

	; Clean up WinVile
	Delete "$INSTDIR\winvile-ole.exe"
	Delete "$INSTDIR\vile.hlp"
	Delete "$INSTDIR\macros\*.rc"
	Delete "$INSTDIR\filters\*.exe"

	; Remove remaining directories
	RMDir "$SMPROGRAMS\WinVile"
	RMDir "$INSTDIR\macros"
	RMDir "$INSTDIR\filters"
	RMDir "$INSTDIR\"

SectionEnd

; eof