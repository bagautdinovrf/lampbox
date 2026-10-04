; MediaBoxManager installer. Compile with Inno Setup 6.
#define AppName "MediaBoxManager"
#ifdef AppVersion
  #error AppVersion must be read from VERSION.txt; remove the /DAppVersion override.
#endif
#define VersionFile FileOpen(SourcePath + "..\VERSION.txt")
#if !VersionFile
  #error Cannot read the root VERSION.txt.
#endif
#define AppVersion Trim(FileRead(VersionFile))
#expr FileClose(VersionFile)
#if AppVersion == ""
  #error The root VERSION.txt is empty.
#endif
#ifndef PackageDir
  #define PackageDir "..\agent_build\deploy\Release"
#endif
#ifndef OutputDir
  #define OutputDir "bin"
#endif

[Setup]
AppId={{E8152E63-5345-40DD-9BE1-ACF3F93E35BE}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Руслан Багаутдинов
AppSupportURL=mailto:bagautdinovrf@ya.ru
DefaultDirName={autopf}\MediaBox
DisableDirPage=yes
UsePreviousAppDir=no
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableWelcomePage=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
MinVersion=10.0
LicenseFile=eula.rtf
OutputDir={#OutputDir}
OutputBaseFilename={#AppName}-{#AppVersion}-Setup
SetupIconFile=assets\setup.ico
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\setup.ico
VersionInfoProductName={#AppName}
VersionInfoCompany=Руслан Багаутдинов
VersionInfoDescription=Установщик {#AppName}
VersionInfoVersion={#AppVersion}
WizardStyle=modern
WizardImageFile=assets\wizard-large.bmp
WizardSmallImageFile=assets\wizard-small.bmp
WizardImageStretch=yes
Compression=lzma2
SolidCompression=yes
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[LangOptions]
DialogFontName=Segoe UI
DialogFontSize=9
WelcomeFontName=Segoe UI
WelcomeFontSize=14

[Dirs]
; Do not inherit broad Users permissions into the private player directories.
; Root permissions are restricted in ConfigureSharedDataAccess below.
Name: "{commonappdata}\MediaBox"; Flags: uninsneveruninstall
Name: "{commonappdata}\MediaBox\MediaBoxManager"; Permissions: users-modify; Flags: uninsneveruninstall
Name: "{commonappdata}\MediaBox\media"; Permissions: users-modify; Flags: uninsneveruninstall

[Files]
; Required entries prevent building a package with missing application binaries.
Source: "{#PackageDir}\bin\MediaBoxManager.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#PackageDir}\bin\MediaBoxPlayer.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#PackageDir}\bin\MediaBoxVPlayer.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#PackageDir}\bin\vcruntime140.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#PackageDir}\bin\vcruntime140_1.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#PackageDir}\bin\msvcp140.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
; Preserve the Qt deploy layout (bin, plugins, translations and bin/qt.conf).
Source: "{#PackageDir}\bin\qt.conf"; DestDir: "{app}\bin"; Flags: ignoreversion skipifsourcedoesntexist
; Runtime settings and logs never belong in an installer or its uninstall log.
Source: "{#PackageDir}\*"; DestDir: "{app}"; Excludes: "*.exe,*.conf,bin\vcruntime140.dll,bin\vcruntime140_1.dll,bin\msvcp140.dll,*.pdb,*.log,control.token,windows.json,project.json,project.json.pending,project.json.lock,schedule-project.json,schedule-project.json.lock,active.json,*.sqlite,*.sqlite-wal,*.sqlite-shm,runtimes\*,snapshots\*,publications\*"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "assets\setup.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName} {#AppVersion}"; Filename: "{app}\bin\MediaBoxManager.exe"; WorkingDir: "{app}\bin"
Name: "{autodesktop}\{#AppName} {#AppVersion}"; Filename: "{app}\bin\MediaBoxManager.exe"; WorkingDir: "{app}\bin"
Name: "{group}\MediaBoxVPlayer {#AppVersion}"; Filename: "{app}\bin\MediaBoxVPlayer.exe"; WorkingDir: "{app}\bin"
Name: "{autodesktop}\MediaBoxVPlayer {#AppVersion}"; Filename: "{app}\bin\MediaBoxVPlayer.exe"; WorkingDir: "{app}\bin"

[Run]
Filename: "{app}\bin\MediaBoxManager.exe"; WorkingDir: "{app}\bin"; Description: "Запустить MediaBoxManager"; Flags: postinstall nowait skipifsilent

[Code]
procedure ConfigureSharedDataAccess;
var
  ResultCode: Integer;
  Arguments: String;
begin
  { Set explicit shared-root permissions for the Users group.
    These rights allow reading and creating files/directories on the root, but
    do not include Delete/DeleteChild and do not propagate to either player. Keep other
    principals' ACL entries and the service's protected directory untouched. }
  Arguments := '"' + ExpandConstant('{commonappdata}\MediaBox') +
    '" /grant:r "*S-1-5-32-545:(RD,WD,AD,REA,X,RA,RC,S)"';
  ResultCode := -1;
  if not Exec(ExpandConstant('{sys}\icacls.exe'), Arguments, '',
    SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    RaiseException('Не удалось настроить права общего каталога MediaBox.');
  if ResultCode <> 0 then
    RaiseException('Не удалось настроить права общего каталога MediaBox. Код: ' + IntToStr(ResultCode));
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    ConfigureSharedDataAccess;
end;
