; AJT JOY16 all-in-one Windows installer (Inno Setup 6).
; The build workflow passes /DAppVersion, /DAppDir (Electron app folder), /DVst3Dir (AJT JOY16.vst3 folder) and /DVst3FxDir (AJT JOY16 FX.vst3 folder).
#define AppName "AJT JOY16"
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
AppId={{8E2B9B8C-6F2C-4F7A-9C3E-1A6D4E2F7B16}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=AJITO SOUNDS
DefaultDirName={autopf}\AJITO SOUNDS\AJT JOY16
DefaultGroupName=AJT JOY16
DisableProgramGroupPage=yes
OutputBaseFilename=AJT-JOY16-Installer-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\AJT JOY16.exe

[Languages]
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "すべて（アプリと VST3 プラグイン）"
Name: "custom"; Description: "選んで入れる"; Flags: iscustom

[Components]
Name: "app"; Description: "AJT JOY16 アプリ（オリジナル / Advanced）"; Types: full custom
Name: "vst3"; Description: "VST3 プラグイン（DAW 用、C:\Program Files\Common Files\VST3）"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Components: app

[Files]
Source: "{#AppDir}\*"; DestDir: "{app}"; Components: app; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#Vst3Dir}\*"; DestDir: "{commoncf64}\VST3\AJT JOY16.vst3"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#Vst3FxDir}\*"; DestDir: "{commoncf64}\VST3\AJT JOY16 FX.vst3"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\AJT JOY16"; Filename: "{app}\AJT JOY16.exe"; Components: app
Name: "{group}\{cm:UninstallProgram,AJT JOY16}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\AJT JOY16"; Filename: "{app}\AJT JOY16.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\AJT JOY16.exe"; Description: "{cm:LaunchProgram,AJT JOY16}"; Flags: nowait postinstall skipifsilent; Components: app
