; $Id: winvile.iss,v 1.36 2025/01/26 10:11:48 tom Exp $
; vile:ts=2 sw=2 fk=8bit
;
; This installs winvile as "winvile-ole.exe", since that is the name I use when building the OLE flavor
; among several for winvile/vile on the win32 platform.  The build script sets $progname to "winvile-ole"
; to help winvile's "-Or" option to find the proper executable.  So this install script uses the same
; name for the executable.
;
; The shortcuts and context-menu entry add the version - to distinguish the install from hand-installed
; copies of the same program.  Note that they all share the same registry key, and the $exec-path variable
; is set from that.  It is used in finding the proper executable for the OLE interface.
;
; TODO:
;
;	make this able to change the type of settings (user-environment to/from application settings).
;
;	components' disablenouninstallwarning could be used to implement selective uninstall, but looks clumsy
;
;	the skipifdoesntexist flag on the [Run] does not appear to work.
;
; context-menu should require admin privilege to set?
;
;	install visvile (separate install, or component to add-on)
;	install vile.exe (separate install)
;
;	can I use GetPreviousData to obtain results from previous install?  (that would help with CurUninstallStep).

; This packager supports only #define's with strings (no C comments)
#include "..\patchlev.h"

#define myVer VILE_RELEASE + '.' + VILE_VERSION + VILE_PATCHLEVEL

#ifndef myAppName 
#define myAppName 'WinVile'
#endif

; something like "winvile", for filename

#ifndef myFullName 
#define myFullName myAppName
#endif

; something like "winvile-ole", for filename
#ifndef myRootName 
#define myRootName myFullName + '-ole'
#endif

#define myAppVer myAppName + ' ' + myVer
#if Ver < 0x5060100
#define mySendTo '{sendto}\' + myAppName + '.lnk'
#else
#define mySendTo '{usersendto}\' + myAppName + '.lnk'
#endif
#define MyQuickLaunch '{userappdata}\Microsoft\Internet Explorer\Quick Launch\' + myAppName + '.lnk'

[Setup]
#emit 'AppName=' + myAppName
#emit 'VersionInfoVersion=' + VILE_RELEASE + '.' + VILE_VERSION + '.' + Str(VILE_PATCHVALUE)
#emit 'VersionInfoDescription=Setup for "' + myAppName + ' - VI Like Emacs"
#emit 'AppVersion=' + myVer
#emit 'AppVerName=' + myAppVer
AppPublisher=Thomas E. Dickey
AppCopyright=© 1997-2024,2025, Thomas E. Dickey
AppPublisherURL=https://invisible-island.net/vile/
AppSupportURL=https://invisible-island.net/vile/
AppUpdatesURL=https://invisible-island.net/vile/
DefaultDirName={pf}\VI-Like-Emacs
OutputDir=iss-output
#emit 'OutputBaseFilename=' + myFullName + '-setup-' + VILE_RELEASE + '_' + VILE_VERSION + VILE_PATCHLEVEL
#emit 'DefaultGroupName=' + myAppName
AllowNoIcons=yes
LicenseFile=..\COPYING
Compression=lzma
SolidCompression=yes
ChangesEnvironment=yes
; SetupLogging=yes

#ifdef WIN64App
ArchitecturesInstallIn64BitMode=x64
#endif

[Components]
#emit 'Name: main; Description: The ' + myAppName + ' executable; types: full custom compact'
Name: help; Description: Vile's help-file; types: full custom compact
Name: docs; Description: Extra documentation; types: full custom
Name: macros; Description: Useful macros; types: full custom compact
Name: explorer; Description: Windows Explorer integration; types: full custom
#emit 'Name: filters; Description: External filter executables (' + myAppName + ' has most of these built-in); types: full custom'

[Tasks]
Name: for_all_users; Description: Install for all users on this machine; GroupDescription: Configuration Settings; Components: explorer filters macros; Check: isGuru; Flags: unchecked
Name: register_vars; Description: Use registry for environment variables; GroupDescription: Configuration Settings; Components: filters macros; Flags: unchecked
Name: first_in_path; Description: Put install-location first in path; GroupDescription: Configuration Settings; Components: filters macros; Flags: unchecked
Name: use_wvwrap; Description: Add Context Menu Entry; GroupDescription: Windows Explorer; Components: explorer; Flags: unchecked
Name: use_sendto; Description: Add Send To Entry; GroupDescription: Windows Explorer; Components: explorer; Flags: unchecked
Name: desktopicon; Description: {cm:CreateDesktopIcon}; GroupDescription: {cm:AdditionalIcons}; Components: main; Flags: unchecked
Name: quicklaunchicon; Description: {cm:CreateQuickLaunchIcon}; GroupDescription: {cm:AdditionalIcons}; Components: main; Flags: unchecked

[Dirs]
Name: {app}\bin; Components: main explorer
Name: {app}\doc; Components: docs
Name: {app}\macros; Components: macros
Name: {app}\filters; Components: filters

[Files]
#emit 'Source: ..\bin\' + myRootName + '.exe; DestDir: {app}\bin; DestName: ' + myRootName + '.exe; Components: main; AfterInstall: myPostExecutable; Flags: ignoreversion'
Source: ..\bin\wvwrap.exe; DestDir: {app}\bin; DestName: wvwrap.exe; Components: explorer; AfterInstall: myPostExplorer; Flags: ignoreversion
Source: ..\vile.hlp; DestDir: {app}\bin; Components: docs help; AfterInstall: myPostHelpfile; Flags: ignoreversion
Source: ..\README; DestDir: {app}; DestName: README.txt; Components: docs main; Flags: isreadme
Source: ..\doc\*.html; Destdir: {app}\doc; Components: docs; Flags: ignoreversion
Source: ..\macros\*.rc; DestDir: {app}\macros; Components: macros; AfterInstall: myPostMacros; Flags: ignoreversion recursesubdirs
Source: ..\filters\*.rc; DestDir: {app}\macros; Components: macros; AfterInstall: myPostMacros; Flags: ignoreversion
Source: c:\vile\*.keywords; DestDir: {app}\macros; Components: macros; Flags: ignoreversion
Source: c:\vile\*.exe; DestDir: {app}\filters; Components: filters; AfterInstall: myPostFilters; Flags: ignoreversion
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
#emit 'Name: {group}\' + myAppName + '; Filename: {app}\bin\' + myRootName + '.exe; Components: main; Flags: createonlyiffileexists'
#emit 'Name: {group}\{cm:UninstallProgram,' + myAppName + '}; Filename: {uninstallexe}'
#emit 'Name: {userdesktop}\' + myAppName + '; Filename: {app}\bin\' + myRootName + '.exe; Components: main; Tasks: desktopicon; Flags: createonlyiffileexists'
#emit 'Name: {userappdata}\Microsoft\Internet Explorer\Quick Launch\' + myAppName + '; Filename: {app}\bin\' + myRootName + '.exe; Components: main; Tasks: quicklaunchicon; Flags: createonlyiffileexists'

[Run]
#emit 'Filename: {app}\bin\' + myRootName + '.exe; Description: {cm:LaunchProgram,' + myAppName + '}; Flags: nowait postinstall skipifsilent skipifdoesntexist'

[UninstallDelete]
Type: files; Name: {app}\macros\vile.rc
Type: dirifempty; Name: {app}\macros
Type: dirifempty; Name: {app}
#emit 'Type: files; Name: ' + mySendTo
#emit 'Type: files; Name: ' + myQuickLaunch

[Code]
#emit 'const MY_CONTEXT_MENU = ''Edit with ' + myAppName + ''';'
#emit 'const MY_EDITOR_APP = ''{app}\bin\' + myRootName + '.exe'';'

const MY_HELP_CONTENTS = 'Help - Contents';

const MY_REGISTER_OLE = 'Register OLE Server';
const MY_UNREGISTER_OLE = 'Unregister OLE Server';

function isGuru(): Boolean;
begin
	Result := isAdminLoggedOn();
end;

function environRootKey(): Integer;
begin
	Result := HKEY_CURRENT_USER;
end;

function appKey(): string;
begin
	Result := 'Software\VI Like Emacs';
end;

function envSubKey(): string;
begin
	Result := 'Environment';
end;

function appSubKey(): string;
begin
	Result := appKey() + '\' + envSubKey();
end;

function envSysKey(): string;
begin
	Result := 'System\CurrentControlSet\Control\Session Manager\Environment';
end;

// Set the environment variable ValueName.
procedure addVarToEnv(const RootKey: Integer; const SubKeyName, ValueName, toAdd: String);
var
	Updated : string;
begin
	Updated := ExpandConstant(toAdd);
	RegWriteStringValue(RootKey, SubKeyName, ValueName, Updated);
	Log('Added ' + toAdd + ' to ' + ValueName);
	// MsgBox('addDirToEnv: ' #13#13 + ValueName + '="' + Updated + '"', mbInformation, MB_OK)
end;

// Remove the given environment variable ValueName.
function removeVarFromEnv(const RootKey: Integer; const SubKeyName, ValueName: String): Boolean;
var
	Current : string;
begin
	Result := False;
	if RegQueryStringValue(RootKey, SubKeyName, ValueName, Current) then
	begin
		RegDeleteValue(RootKey, SubKeyName, ValueName);
		Result := True;
		Log('Removed ' + ValueName);
		// MsgBox('removeDirFromEnv: ' #13#13 + ValueName + '="' + Current + '"', mbInformation, MB_OK)
	end;
end;

// Find a directory in a semicolon-separated list.
// Returns the index in the list, or -1 if not found.
function findDirInText(const InText, toFind: String): Integer;
var
	first, last, count : Integer;
	OnLeft, OnRight : String;
	found : Boolean;
begin
	first := 1;
	count := 0;
	found := False;
	Log('findDirInText{' + InText + '} {' + toFind + '}');
	OnRight := Copy(InText, first, Length(InText));
	while Not found And (first <= Length(OnRight)) do
		begin
		count := count + 1;
		last := Pos(';', OnRight);

		if last <= 0 then
			last := Length(OnRight) + 1;

		if first < last then
			begin
			OnLeft := copy(OnRight, first, last - first);
			Log('...compare {' + OnLeft + '}');
			if CompareText(ToFind, OnLeft) = 0 then
				found := True;
				result := count;
			end;

		first := last + 1;
		if first <= Length(OnRight) then
			begin
			OnRight := copy(OnRight, first, Length(OnRight) + 1 - first);
			first := 1;
			end;

		end;
		Log('->' + IntToStr(result));
end;

// Add the given string to the front of the environment variable ValueName,
// unless it is already there.
procedure addDirToEnv(const RootKey: Integer; const SubKeyName, ValueName, toAdd: String);
var
	Current : string;
	Updated : string;
	Actual  : string;
	Lookup  : integer;
begin
	Updated := ExpandConstant(toAdd);
	if RegQueryStringValue(RootKey, SubKeyName, ValueName, Current) then
		begin
		// Successfully read the value
		Actual := Updated;
		if Length(Current) >= Length(Actual) then
			begin
			Lookup := findDirInText(Current, Actual);

			if isTaskSelected('first_in_path') then
				if Lookup > 1 then
					begin
					Log('Overriding to put first in path, found index ' + IntToStr(Lookup));
					Lookup := 0;
					end;

			if Lookup > 0 then
				begin
				Log('Directory ' + toAdd + ' is already in ' + ValueName)
				Updated := '';
				end
			else
				begin
				if Length(Current) > 0 then
					Updated := Updated + ';' + Current
				end;
			end
		else
			begin
			if Length(Current) > 0 then
				Updated := Updated + ';' + Current
			end;
		end;
	if Length(Updated) > 0 then
	begin
		RegWriteStringValue(RootKey, SubKeyName, ValueName, Updated);
		Log('Added ' + toAdd + ' to ' + ValueName);
		// MsgBox('addDirToEnv: ' #13#13 + ValueName + '="' + Updated + '"', mbInformation, MB_OK)
	end;
end;

// Remove the given string from the environment variable ValueName, assuming
// it is at the front of the variable, and (like the PATH variable) delimited
// by a semicolon from any other directory(s).
function removeDirFromEnv(const RootKey: Integer; const SubKeyName, ValueName, toRemove: String): Boolean;
var
	Current : string;
	Updated : string;
	Actual  : string;
begin
	Result := False;
	if RegQueryStringValue(RootKey, SubKeyName, ValueName, Current) then
	begin
		// Successfully read the value
		Actual := ExpandConstant(toRemove);
		if Length(Current) >= Length(Actual) then
		begin
			Updated := Copy(Current, 1, Length(Actual));
			if CompareStr(Updated, Actual) = 0 then
			begin
				Updated := Copy(Current, Length(Actual) + 1, Length(Current));
				Actual  := Copy(Updated, 1, 1);
				if CompareStr(Actual, ';') = 0 then
				begin
					Updated := Copy(Updated, 2, Length(Updated));
				end;
				if Length(Updated) = 0 then
					RegDeleteValue(RootKey, SubKeyName, ValueName)
				else
					RegWriteStringValue(RootKey, SubKeyName, ValueName, Updated);
				Result := True;
				Log('Removed ' + toRemove + ' from ' + ValueName);
				// MsgBox('removeDirFromEnv: ' #13#13 + ValueName + '="' + Updated + '"', mbInformation, MB_OK)
			end;
		end;
	end;
end;

function selectedVarsRootKey(): Integer;
begin
	if isTaskSelected('for_all_users') then
		Result := HKEY_LOCAL_MACHINE
	else
		Result := HKEY_CURRENT_USER;
end;

function selectedVarsSubKey(): String;
begin
	if isTaskSelected('for_all_users') then
	begin
		if isTaskSelected('register_vars') then
			Result := appSubKey()
		else
			Result := envSysKey();
	end else
	begin
		if isTaskSelected('register_vars') then
			Result := appSubKey()
		else
			Result := envSubKey();
	end;
end;

procedure addAnyDirectory(const ValueName, newValue: String);
begin
	addDirToEnv(selectedVarsRootKey(), selectedVarsSubKey(), ValueName, NewValue);
end;

procedure addAnyVariable(const ValueName, newValue: String);
begin
	addVarToEnv(selectedVarsRootKey(), selectedVarsSubKey(), ValueName, NewValue);
end;

procedure addHelpFile(const Value: String);
begin
	addAnyVariable('VILE_HELP_FILE', Value);
end;

procedure addLibraryPath(const Value: String);
begin
	addAnyDirectory('VILE_LIBDIR_PATH', Value);
end;

procedure addStartupPath(const Value: String);
begin
	addAnyDirectory('VILE_STARTUP_PATH', Value);
end;

procedure removeAnyDirVar(const ValueName, NewValue: String);
begin
	removeDirFromEnv(HKEY_CURRENT_USER, envSubKey(), ValueName, NewValue);
	removeDirFromEnv(HKEY_CURRENT_USER, appSubKey(), ValueName, NewValue);
	removeDirFromEnv(HKEY_LOCAL_MACHINE, appSubKey(), ValueName, NewValue);
	removeDirFromEnv(HKEY_LOCAL_MACHINE, envSysKey(), ValueName, NewValue);
end;

// FIXME: should only remove if it matches the installer's value
procedure removeAnyVariable(const ValueName: String);
begin
	removeVarFromEnv(HKEY_CURRENT_USER, envSubKey(), ValueName);
	removeVarFromEnv(HKEY_CURRENT_USER, appSubKey(), ValueName);
	removeVarFromEnv(HKEY_LOCAL_MACHINE, appSubKey(), ValueName);
	removeVarFromEnv(HKEY_LOCAL_MACHINE, envSysKey(), ValueName);
end;

procedure AddShellCommand(const Description, Command: String);
begin
	RegWriteStringValue(HKEY_CLASSES_ROOT, '*\shell\' + Description + '\command', '', ExpandConstant(command));
end;

procedure removeShellCommand(const Description: String);
begin
	RegDeleteKeyIncludingSubkeys(HKEY_CLASSES_ROOT, '*\shell\' + Description);
end;

// This is called after installing the executable.
procedure myPostExecutable();
var
	Keypath : String;
	Value   : String;
begin
	Keypath := appKey();
	Value := ExpandConstant('{app}\bin');
	Log('Setting registry key "' + Keypath + '" to "' + Value + '"');
	if not RegWriteStringValue(selectedVarsRootKey(), Keypath, '', value) then
		Log('Failed to set key');
end;

// This is called once per installed-file.
procedure myPostFilters();
begin
	addLibraryPath('{app}\filters');
end;

// This is called once per installed-file.
procedure myPostHelpfile();
begin
	addHelpFile('{app}\bin\vile.hlp');
end;

// This is called once per installed-file.
procedure myPostMacros();
var
	ThisFile : string;
	InitFile : string;
begin
	// MsgBox('myPostMacros: tasks <' + WizardSelectedTasks(false) + '>', mbInformation, MB_OK);
	addStartupPath('{app}\macros');
	ThisFile := CurrentFileName();
	// MsgBox('myPostMacros: <' + ThisFile + '>', mbInformation, MB_OK);
	if CompareStr(ThisFile, '{app}\macros\vileinit.rc') = 0 then
	begin
		InitFile := ExpandConstant('{app}\macros\vile.rc');
		SaveStringToFile(InitFile, 'source vileinit.rc' + #13#10, False);
		SaveStringToFile(InitFile, 'set cs=black' + #13#10, True);
	end;
end;

// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/programmersguide/shell_basics/shell_basics_extending/context.asp
procedure AddContextMenu();
begin
	AddShellCommand(MY_CONTEXT_MENU, '{app}\bin\wvwrap.exe "%L"');
end;

// http://www.delphidabbler.com/articles?article=12
procedure AddSendTo();
begin
	CreateShellLink(
#emit 'ExpandConstant(''' + mySendTo + '''),'
#emit '''SendTo link for ' + myAppName + ''','
		ExpandConstant(MY_EDITOR_APP),    // program
		'-i',                             // option(s) will be followed by pathname
		'',                               // no target directory
		'',                               // no icon filename
		-1,                               // no icon index
		SW_SHOWNORMAL);
end;

procedure AddQuickLaunch();
begin
	CreateShellLink(
#emit 'ExpandConstant(''' + MyQuickLaunch + '''),'
#emit '''Quick Launch link for ' + myAppName + ''','
		ExpandConstant(MY_EDITOR_APP),    // program
		'-i',                             // option(s) will be followed by pathname
		'',                               // no target directory
		'',                               // no icon filename
		-1,                               // no icon index
		SW_SHOWNORMAL);
end;

function myToolsDir(): String;
begin
	Result := ExpandConstant('{group}\Tools');
end;

function myToolsLink(const Description: String): String;
begin
	Result := myToolsDir() + '\' + Description + '.lnk';
end;

// use shortcuts for invoking registering so we will "see" the right path.
procedure AddToolLink(const Description, Params: String);
begin
	CreateShellLink(
		ExpandConstant(myToolsLink(Description)),
		Description,
		ExpandConstant(MY_EDITOR_APP),    // program
		Params,                           //
		ExpandConstant('{app}\bin'),      // target directory contains executable
		'',                               // no icon filename
		-1,                               // no icon index
		SW_HIDE);
end;

procedure RemoveToolLink(const Description: String);
var
	ToRemove : String;
begin
	ToRemove := myToolsLink(Description);
	if FileExists(ToRemove) then
		begin
		Log('Deleting link: ' + ToRemove);
		if not DeleteFile(ToRemove) then
			Log('Failed to remove ' + ToRemove);
		end;
end;

function myDocsDir(): String;
begin
	Result := ExpandConstant('{group}');
end;

function myDocsLink(const Description: String): String;
begin
	Result := myDocsDir() + '\' + Description + '.lnk';
end;

// use shortcuts so we will "see" the right path.
procedure AddDocsLink(const Description, Params: String);
begin
	CreateShellLink(
		ExpandConstant(myDocsLink(Description)),
		Description,
		ExpandConstant('{app}\doc\' + Params), // document
		'',                               //
		ExpandConstant('{app}\doc'),      // target directory contains documents
		'',                               // no icon filename
		-1,                               // no icon index
		SW_HIDE);
end;

procedure RemoveDocsLink(const Description: String);
var
	ToRemove : String;
begin
	ToRemove := myDocsLink(Description);
	if FileExists(ToRemove) then
		begin
		Log('Deleting link: ' + ToRemove);
		if not DeleteFile(ToRemove) then
			Log('Failed to remove ' + ToRemove);
		end;
end;

function CleanupMyKey(const theRootKey: Integer): Boolean;
var
	Path : String;
	Value : String;
begin
	Result := False;
	if RegQueryStringValue(theRootKey, appKey(), '', Value) then
		begin
			if Value <> '' then
				begin
				Result := True;
				Log('Deleting value of "' + appKey() + '" = "' + Value + '"');
				if not RegDeleteValue(theRootKey, appKey(), '') then
					Log('Failed to delete value');

				Path := appKey() + '\Environment';
				Log('Checking for subkey "' + Path + '"');
				if RegValueExists(theRootKey, Path, '') then
					begin
					if RegDeleteKeyIncludingSubkeys(theRootKey, Path) then
						Log('Deleted key "' + Path + '"')
					else
						Log('Failed to delete key "' + Path + '"');
					end;

				if RegDeleteKeyIfEmpty(theRootKey, appKey()) then
					Log('Deleted key "' + appKey() + '"')
				else
					Log('Failed to delete key "' + appKey() + '"');
				end
		end
end;
			
procedure RegisterMyOLE();
var
	Filename : String;
	ResultCode: Integer;
begin
	Filename := ExpandConstant(MY_EDITOR_APP);
	Log('Registering OLE server ' + Filename);
	if not Exec(Filename, '-Or', '', SW_HIDE,
		 ewWaitUntilTerminated, ResultCode) then
	begin
		// handle failure if necessary; ResultCode contains the error code
		Log('** FAILED!');
	end;
end;

procedure UnregisterMyOLE();
var
	Filename : String;
	ResultCode: Integer;
begin
	Filename := ExpandConstant(MY_EDITOR_APP);
	Log('Unregistering OLE server ' + Filename);
	if not Exec(Filename, '-Ou', '', SW_HIDE,
		 ewWaitUntilTerminated, ResultCode) then
	begin
		// handle failure if necessary; ResultCode contains the error code
		Log('** FAILED!');
	end;
end;

procedure myPostExplorer();
begin
	if isTaskSelected('use_wvwrap') then
		begin
		if ForceDirectories(myToolsDir()) then
			begin
			AddToolLink(MY_REGISTER_OLE, '-Or');
			AddToolLink(MY_UNREGISTER_OLE, '-Ou');
			AddDocsLink(MY_HELP_CONTENTS, 'vile-toc.html');
			end
		else
			MsgBox('Cannot create:' #13#13 '    ' + myToolsDir(), mbInformation, MB_OK);
		RegisterMyOLE();
		AddContextMenu();
		end;

	if isTaskSelected('use_sendto') then
		begin
		AddSendTo();
		end;

	if isTaskSelected('quicklaunchicon') then
		begin
		AddQuickLaunch();
		Log('** added Quick-launch link');
		end;
end;

// On uninstall, we do not know which registry setting was selected during install, so we remove all.
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
	case CurUninstallStep of
	usUninstall:
		begin
		// MsgBox('CurUninstallStepChanged:' #13#13 'Uninstall is about to start.', mbInformation, MB_OK)
		// ...insert code to perform pre-uninstall tasks here...
		UnregisterMyOLE();
		end;
	usPostUninstall:
		begin
		removeAnyVariable('VILE_HELP_FILE');
		removeAnyDirVar('VILE_LIBDIR_PATH', '{app}\filters');
		removeAnyDirVar('VILE_STARTUP_PATH', '{app}\macros');
		removeShellCommand(MY_CONTEXT_MENU);
		
		if DirExists(myToolsDir()) then
			begin
			removeDocsLink(MY_HELP_CONTENTS);
			Log('Removing ' + myToolsDir());
			removeToolLink(MY_REGISTER_OLE);
			removeToolLink(MY_UNREGISTER_OLE);
			RemoveDir(myToolsDir());
			RemoveDir(ExpandConstant('{group}'));  // since this happens after the start-menu is removed
			end;

		{
			If we don't find the $exec-path in the current user, try the local machine.
			The setup program cannot pass the all-users flag to the uninstaller, so we
			have to try both.
		}
		Log('Checking current-user registry key');
		if not CleanupMyKey(HKEY_CURRENT_USER) then
			begin
			Log('Checking local-machine registry key');
			CleanupMyKey(HKEY_LOCAL_MACHINE);
			end;
		
		// MsgBox('CurUninstallStepChanged:' #13#13 'Uninstall just finished.', mbInformation, MB_OK);
		end;
	end;
end;
