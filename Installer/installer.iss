; MediaBoxManager installer. Compile with Inno Setup 6.
#define AppName "MediaBoxManager"
#ifndef AppVersion
  #define AppVersion "1.1.1"
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
Name: "{commonappdata}\MediaBox"; Permissions: users-modify; Flags: uninsneveruninstall

[Files]
; Required entries prevent building a package with missing application binaries.
Source: "{#PackageDir}\bin\MediaBoxManager.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#PackageDir}\bin\MediaBoxPlayer.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#PackageDir}\bin\vcruntime140.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#PackageDir}\bin\vcruntime140_1.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#PackageDir}\bin\msvcp140.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
; Preserve the Qt deploy layout (bin, plugins, translations and bin/qt.conf).
; Runtime settings and logs never belong in an installer or its uninstall log.
Source: "{#PackageDir}\*"; DestDir: "{app}"; Excludes: "bin\MediaBoxManager.exe,bin\MediaBoxPlayer.exe,bin\vcruntime140.dll,bin\vcruntime140_1.dll,bin\msvcp140.dll,lampbox.exe,*.pdb,*.log,lampbox.conf,MediaBoxManager.conf,MediaBoxPlayer.conf,vc_redist*.exe"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "assets\setup.ico"; DestDir: "{app}"; Flags: ignoreversion

[InstallDelete]
; Replace the unversioned shortcuts created by earlier installer builds.
Type: files; Name: "{group}\{#AppName}.lnk"
Type: files; Name: "{autodesktop}\{#AppName}.lnk"

[Icons]
Name: "{group}\{#AppName} {#AppVersion}"; Filename: "{app}\bin\MediaBoxManager.exe"; WorkingDir: "{app}\bin"
Name: "{autodesktop}\{#AppName} {#AppVersion}"; Filename: "{app}\bin\MediaBoxManager.exe"; WorkingDir: "{app}\bin"

[Run]
Filename: "{app}\bin\MediaBoxManager.exe"; WorkingDir: "{app}\bin"; Description: "Запустить MediaBoxManager"; Flags: postinstall nowait skipifsilent
