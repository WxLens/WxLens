; WxLens Windows installer (docs/ROADMAP.md's "Release prep" section: Inno Setup, with a
; portable ZIP alongside). Builds one installer from the Release windeployqt output.
;
; Usage (from repo root, after a normal Release build has run windeployqt):
;   "C:\Users\<you>\AppData\Local\Programs\Inno Setup 6\ISCC.exe" tools\installer\windows\wxlens.iss
;
; Override the build output location or version without editing this file:
;   ISCC.exe /DBuildDir=..\build-somewhere\Release /DMyAppVersion=0.2.0 tools\installer\windows\wxlens.iss
;
; BuildDir is resolved relative to THIS SCRIPT'S directory (tools\installer\windows), not the
; repo root or the current working directory - Inno's #define paths are script-relative.

#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\..\build-release-vs2026\Release"
#endif

#define MyAppName "WxLens"
#define MyAppPublisher "WxLens"
#define MyAppURL "https://github.com/WxLens/WxLens"
#define MyAppExeName "wxlens-app.exe"

[Setup]
; Fixed, do not regenerate: Inno uses this GUID to recognize upgrades/uninstalls of the same
; product across versions. Generated once for WxLens; keep it stable forever.
AppId={{8E6C6C1A-6C7B-4C6E-9C7A-3E9C6E4C6C1A}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
VersionInfoVersion={#MyAppVersion}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
; Per-machine (needs elevation) is the default, but a user without admin rights can still
; install for themselves alone - required for a clean install on a locked-down machine.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile=..\..\..\LICENSE.txt
SetupIconFile=..\..\..\app\res\branding\wxlens.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
OutputDir=..\..\..\dist
OutputBaseFilename=WxLens-{#MyAppVersion}-windows-x64-setup

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; The whole windeployqt output tree, minus what end users never need: debug symbols (.pdb,
; hundreds of MB - see tools/installer/windows/README.md), the GTest binaries that happen to
; land in the same output directory, and their .ilk/.exp/.lib build-time siblings.
Source: "{#BuildDir}\bin\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; \
    Excludes: "*.pdb,*.ilk,*-test.exe,test_mln_quick.exe"

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Qt/QML cache the app writes under its own install dir (aotstats etc. stay under bin/qmlcache
; inside the build tree, not here, so this is just a safety net for anything dropped alongside
; the exe at runtime). User settings/palettes live under %LOCALAPPDATA%\WxLens and are
; deliberately NOT removed - uninstalling the app must not delete a user's saved places/palettes.
Type: filesandordirs; Name: "{app}\qmlcache"
