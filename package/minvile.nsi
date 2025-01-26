; $Id: minvile.nsi,v 1.45 2025/01/26 10:11:34 tom Exp $
; vile:fk=8bit
; Script originally generated with the Venis Install Wizard, but customized.
; The Inno Setup script is preferred; but this can be built via cross-compiling.

; Define the application name
!define APPNAME "MinVile"
!define EXENAME "minvile.exe"

!define VERSION_MAJOR "9"
!define VERSION_MINOR "8"
!define VERSION_LEVEL "27"
!define VERSION_PATCH "za"
!define VERSION_BUILT "0"

!define SUBKEY "VI like Emacs"

!define INSTALL "${SUBKEY} (GUI)"
!define VERSION "${VERSION_MAJOR}.${VERSION_MINOR}${VERSION_PATCH}"

; Main Install settings
Name "${INSTALL}"
InstallDir "$PROGRAMFILES\${SUBKEY}"
InstallDirRegKey HKLM "Software\${SUBKEY}" "$INSTDIR\bin"
OutFile "NSIS-Output\${APPNAME}-${VERSION}-setup.exe"

CRCCheck on
SetCompressor /SOLID lzma

VIAddVersionKey ProductName "${SUBKEY}"
VIAddVersionKey CompanyName "Thomas E. Dickey"
VIAddVersionKey LegalCopyright "© 1997-2024,2025, Thomas E. Dickey"
VIAddVersionKey FileDescription "WinVile Installer (MinGW)"
VIAddVersionKey FileVersion "${VERSION}"
VIAddVersionKey ProductVersion "${VERSION}"
VIAddVersionKey Comments "This installer was built with NSIS and cross-compiling to MinGW."
VIAddVersionKey InternalName "setup-${APPNAME}-${VERSION}.exe"
VIProductVersion "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_LEVEL}.${VERSION_BUILT}"

; Modern interface settings
!include "MUI.nsh"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\${EXENAME}"

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

!define VILE_RC "vile.rc"

InstType "Full"		; SectionIn 1
InstType "Typical"	; SectionIn 2
InstType "Minimal"	; SectionIn 3

Section "${APPNAME}" Section1

	SectionIn 1 2 3

	; Set Section properties
	SetOverwrite on

	; Set Section Files and Shortcuts
	SetOutPath "$INSTDIR"
	File /oname=README.txt "..\README" 
	File ".\bin\${EXENAME}"
	File ".\share\vile\vile.hlp"
	CreateShortCut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\${EXENAME}"
	CreateShortCut "$SENDTO\${APPNAME}.lnk" "$INSTDIR\${EXENAME}"
	CreateDirectory "$SMPROGRAMS\${INSTALL}"
	CreateShortCut "$SMPROGRAMS\${INSTALL}\${APPNAME}.lnk" "$INSTDIR\${EXENAME}"
	CreateShortCut "$SMPROGRAMS\${INSTALL}\Uninstall.lnk" "$INSTDIR\uninstall.exe"

SectionEnd

Section "macros" Section2

	SectionIn 1 2

	; Set Section properties
	SetOverwrite on

	; Set Section Files and Shortcuts
	SetOutPath "$INSTDIR\macros"
	File ".\share\vile\*.rc"
	File ".\share\vile\*.keywords"

	!appendfile "${VILE_RC}" "source vileinit.rc$\n"
	!appendfile "${VILE_RC}" "set cs=black$\n"
	File "${VILE_RC}"
	!delfile "${VILE_RC}"

	SetOutPath "$INSTDIR\filters"
	File ".\lib\vile\vile-manfilt.exe"

SectionEnd

Section "filters" Section3

	SectionIn 1

	; Set Section properties
	SetOverwrite on

	; Set Section Files and Shortcuts
	SetOutPath "$INSTDIR\filters"
	File ".\lib\vile\*.exe"

SectionEnd

Section -FinishSection
	WriteRegStr HKLM "Software\${SUBKEY}" "" "$INSTDIR"
	WriteRegStr HKLM "Software\${SUBKEY}" "Environment" ""
	WriteRegStr HKLM "Software\${SUBKEY}\Environment" "VILE_HELP_FILE" "$INSTDIR\vile.hlp"
	WriteRegStr HKLM "Software\${SUBKEY}\Environment" "VILE_LIBDIR_PATH" "$INSTDIR\filters"
	WriteRegStr HKLM "Software\${SUBKEY}\Environment" "VILE_STARTUP_PATH" "$INSTDIR\macros"
	WriteRegStr HKLM "Software\${SUBKEY}\Environment" "VILE_STARTUP_FILE" "$INSTDIR\macros\${VILE_RC}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${INSTALL}" "DisplayName" "${INSTALL} ${VERSION}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${INSTALL}" "UninstallString" "$INSTDIR\uninstall.exe"
	WriteUninstaller "$INSTDIR\uninstall.exe"

SectionEnd

; Modern install component descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${Section1} "${SUBKEY}"
	!insertmacro MUI_DESCRIPTION_TEXT ${Section2} "Initialization script and useful macros"
	!insertmacro MUI_DESCRIPTION_TEXT ${Section3} "Syntax (highlighting) filters.  Most are compiled-in, but some are useful as stand-alone programs."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;Uninstall section
Section Uninstall

	;Remove from registry...
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${INSTALL}"
	DeleteRegKey HKLM "SOFTWARE\${SUBKEY}"

	; Delete self
	Delete "$INSTDIR\uninstall.exe"

	; Delete Shortcuts
	Delete "$DESKTOP\${APPNAME}.lnk"
	Delete "$SENDTO\${APPNAME}.lnk"
	Delete "$SMPROGRAMS\${INSTALL}\${APPNAME}.lnk"
	Delete "$SMPROGRAMS\${INSTALL}\Uninstall.lnk"

	; Clean up application
	Delete "$INSTDIR\${EXENAME}"
	Delete "$INSTDIR\vile.hlp"
	Delete "$INSTDIR\README.txt"
	Delete "$INSTDIR\macros\*.rc"
	Delete "$INSTDIR\macros\*.keywords"
	Delete "$INSTDIR\filters\*.exe"

	; Remove remaining directories
	RMDir "$SMPROGRAMS\${INSTALL}"
	RMDir "$INSTDIR\macros"
	RMDir "$INSTDIR\filters"
	RMDir "$INSTDIR\bin"
	RMDir "$INSTDIR\"

SectionEnd

; eof