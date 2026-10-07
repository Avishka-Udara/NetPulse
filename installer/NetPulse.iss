#define AppVersion "1.1.1"
[Setup]
AppId=NetPulse.AvishkaUdara
AppName=NetPulse
AppVersion={#AppVersion}
AppPublisher=Avishka Udara
AppPublisherURL=https://github.com/avishka-Udara
DefaultDirName={localappdata}\Programs\NetPulse
DefaultGroupName=NetPulse
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
LicenseFile=..\LICENSE
OutputDir=..\dist
OutputBaseFilename=NetPulse-{#AppVersion}-windows-x64-setup
SetupIconFile=..\src\netpulse.ico
UninstallDisplayIcon={app}\NetPulse.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
AppMutex=Local\NetPulse.Desktop.Singleton
CloseApplications=yes
RestartApplications=no
[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked
[Files]
Source: "..\build\NetPulse.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\config.ini"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion
[Icons]
Name: "{group}\NetPulse"; Filename: "{app}\NetPulse.exe"
Name: "{autodesktop}\NetPulse"; Filename: "{app}\NetPulse.exe"; Tasks: desktopicon
[Run]
Filename: "{app}\NetPulse.exe"; Description: "Launch NetPulse"; Flags: nowait postinstall skipifsilent
[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var Command: String;
begin
  if CurUninstallStep = usUninstall then
    if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'NetPulse', Command) then
      if Pos('"' + ExpandConstant('{app}\NetPulse.exe') + '"', Command) = 1 then
        RegDeleteValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'NetPulse');
end;
