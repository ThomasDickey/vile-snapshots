; $Id: convile.nsi,v 1.38 2025/01/26 10:11:18 tom Exp $
; vile:fk=8bit
; Script originally generated with the Venis Install Wizard, but customized.
; The Inno Setup script is preferred; but this can be built via cross-compiling.

; Define the application name
!define APPNAME "ConVile"
!define EXENAME "convile.exe"

!define VERSION_MAJOR "9"
!define VERSION_MINOR "8"
!define VERSION_LEVEL "27"
!define VERSION_PATCH "za"
!define VERSION_BUILT "0"

!define SUBKEY "VI like Emacs"

!define INSTALL "${SUBKEY} (Console)"
!define VERSION "${VERSION_MAJOR}.${VERSION_MINOR}${VERSION_PATCH}"

; Main Install settings
Name "${INSTALL}"
InstallDir "c:\mingw"
InstallDirRegKey HKLM "Software\${SUBKEY}" "$INSTDIR\bin"
OutFile "NSIS-Output\${APPNAME}-${VERSION}-setup.exe"

CRCCheck on
SetCompressor /SOLID lzma

VIAddVersionKey ProductName "${SUBKEY}"
VIAddVersionKey CompanyName "Thomas E. Dickey"
VIAddVersionKey LegalCopyright "© 1997-2024,2025, Thomas E. Dickey"
VIAddVersionKey FileDescription "ConVile Installer (MinGW)"
VIAddVersionKey FileVersion "${VERSION}"
VIAddVersionKey ProductVersion "${VERSION}"
VIAddVersionKey Comments "This installer was built with NSIS and cross-compiling to MinGW."
VIAddVersionKey InternalName "setup-${APPNAME}-${VERSION}.exe"
VIProductVersion "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_LEVEL}.${VERSION_BUILT}"

; Modern interface settings
!include "MUI.nsh"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\${EXENAME}"

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
	SetOutPath "$INSTDIR\bin"
	File ".\bin\${EXENAME}"
	File ".\share\vile\vile.hlp"

	SetOutPath "$INSTDIR\share\vile"
	File /oname=README.txt "..\README" 

	CreateDirectory "$SMPROGRAMS\${INSTALL}"
	CreateShortCut "$SMPROGRAMS\${INSTALL}\${APPNAME}.lnk" "$INSTDIR\bin\${EXENAME}"
	CreateShortCut "$SMPROGRAMS\${INSTALL}\Uninstall.lnk" "$INSTDIR\uninstall.exe"

SectionEnd

Section "macros" Section2

	SectionIn 1 2

	; Set Section properties
	SetOverwrite on

	; Set Section Files and Shortcuts
	SetOutPath "$INSTDIR\share\vile"
	File ".\share\vile\*.rc"
	File ".\share\vile\*.keywords"

	!appendfile "${VILE_RC}" "source vileinit.rc$\n"
	!appendfile "${VILE_RC}" "set cs=black$\n"
	File "${VILE_RC}"
	!delfile "${VILE_RC}"

	SetOutPath "$INSTDIR\lib\vile"
	File ".\lib\vile\vile-manfilt.exe"

SectionEnd

Section "filters" Section3

	SectionIn 1

	; Set Section properties
	SetOverwrite on

	; Set Section Files and Shortcuts
	SetOutPath "$INSTDIR\lib\vile"
	File ".\lib\vile\*.exe"

SectionEnd

Section -FinishSection

	WriteRegStr HKLM "Software\${SUBKEY}" "" "$INSTDIR"
	WriteRegStr HKLM "Software\${SUBKEY}" "Environment" ""
	WriteRegStr HKLM "Software\${SUBKEY}\Environment" "VILE_HELP_FILE" "$INSTDIR\bin\vile.hlp"
	WriteRegStr HKLM "Software\${SUBKEY}\Environment" "VILE_LIBDIR_PATH" "$INSTDIR\lib\vile"
	WriteRegStr HKLM "Software\${SUBKEY}\Environment" "VILE_STARTUP_PATH" "$INSTDIR\share\vile"
	WriteRegStr HKLM "Software\${SUBKEY}\Environment" "VILE_STARTUP_FILE" "$INSTDIR\share\vile\${VILE_RC}"
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
	Delete "$SMPROGRAMS\${INSTALL}\${APPNAME}.lnk"
	Delete "$SMPROGRAMS\${INSTALL}\Uninstall.lnk"

	; Clean up application
	Delete "$INSTDIR\bin\${EXENAME}"
	Delete "$INSTDIR\bin\vile.hlp"
	Delete "$INSTDIR\share\vile\README.txt"
	Delete "$INSTDIR\share\vile\*.rc"
	Delete "$INSTDIR\share\vile\*.keywords"
	Delete "$INSTDIR\lib\vile\*.exe"

	; Remove remaining directories
	RMDir "$SMPROGRAMS\${INSTALL}"
	RMDir "$INSTDIR\share\vile"
	RMDir "$INSTDIR\share"
	RMDir "$INSTDIR\lib\vile"
	RMDir "$INSTDIR\lib"
	RMDir "$INSTDIR\bin"
	RMDir "$INSTDIR\"

SectionEnd

; eof