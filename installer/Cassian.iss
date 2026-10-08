#ifndef PackageDir
  #error PackageDir must point to the prepared Windows package.
#endif
#ifndef OutputDir
  #define OutputDir ".."
#endif
#ifndef AppVersion
  #error AppVersion must come from the Cassian package version.
#endif

[Setup]
#ifdef SmokeTest
AppId={{239BAD5F-8A9D-4E14-BB81-CE61624D444E}
AppName=Cassian Installer Test
OutputBaseFilename=Cassian-Setup-Smoke
#else
AppId={{84DD593B-E4BB-44C4-992B-78587D84B2B3}
AppName=Cassian
OutputBaseFilename=Cassian-Setup
#endif
AppVersion={#AppVersion}
AppPublisher=Crazaloth
AppPublisherURL=https://github.com/CrazalothAI/Cassius-Guitar-DAW
AppSupportURL=https://github.com/CrazalothAI/Cassius-Guitar-DAW/issues
DefaultDirName={localappdata}\Programs\Cassian
DefaultGroupName=Cassian
PrivilegesRequired=lowest
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.17763
OutputDir={#OutputDir}
SetupIconFile={#PackageDir}\Cassian.ico
UninstallDisplayIcon={app}\Cassian.exe
UninstallDisplayName=Cassian
Compression=lzma2
SolidCompression=yes
#ifdef SignInstaller
SignTool=CassianSign
SignedUninstaller=yes
#endif
WizardStyle=modern
DisableProgramGroupPage=yes
CloseApplications=yes
RestartApplications=no
InfoAfterFile={#PackageDir}\QUICK-START.txt

[Types]
Name: "app"; Description: "Cassian app"
Name: "full"; Description: "Cassian app and VST3 plugin"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "app"; Description: "Cassian standalone app"; Types: app full custom; Flags: fixed
Name: "vst3"; Description: "VST3 plugin for a DAW (current user)"; Types: full

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#PackageDir}\Cassian.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "{#PackageDir}\QUICK-START.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageDir}\SOURCE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageDir}\THIRD_PARTY.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageDir}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageDir}\COPYRIGHT.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageDir}\USER-GUIDE.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageDir}\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion recursesubdirs createallsubdirs
#ifdef WithSoundBank
Source: "{#PackageDir}\Sounds\*"; DestDir: "{app}\Sounds"; Flags: ignoreversion recursesubdirs createallsubdirs
#endif
#ifdef SmokeTest
Source: "{#PackageDir}\Cassian.vst3\*"; DestDir: "{app}\VST3\Cassian.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
#else
Source: "{#PackageDir}\Cassian.vst3\*"; DestDir: "{localappdata}\Programs\Common\VST3\Cassian.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
#endif
Source: "{#PackageDir}\MicrosoftEdgeWebview2Setup.exe"; Flags: dontcopy

[Icons]
#ifdef SmokeTest
Name: "{app}\Cassian Test"; Filename: "{app}\Cassian.exe"; WorkingDir: "{app}"
#else
Name: "{userprograms}\Cassian"; Filename: "{app}\Cassian.exe"; WorkingDir: "{app}"; AppUserModelID: "Cassian.GuitarProcessor"
Name: "{userdesktop}\Cassian"; Filename: "{app}\Cassian.exe"; WorkingDir: "{app}"; Tasks: desktopicon; AppUserModelID: "Cassian.GuitarProcessor"
#endif

[Run]
Filename: "{app}\Cassian.exe"; Description: "Launch Cassian"; Flags: nowait postinstall skipifsilent

[Code]
function RuntimeInRegistry(Root: Integer; const Key: String): Boolean;
var
  Version: String;
begin
  Result := RegQueryStringValue(Root, Key, 'pv', Version) and (Version <> '') and (Version <> '0.0.0.0');
end;

function HasWebView2: Boolean;
var
  Key: String;
begin
  Key := 'Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}';
  Result := RuntimeInRegistry(HKCU32, Key) or RuntimeInRegistry(HKLM32, Key) or
    RuntimeInRegistry(HKCU64, Key) or RuntimeInRegistry(HKLM64, Key);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ExitCode: Integer;
begin
  Result := '';
  ExitCode := -1;
  if HasWebView2 then begin
    Log('Microsoft WebView2 Runtime detected.');
    exit;
  end;
  WizardForm.PreparingLabel.Caption := 'Installing Microsoft WebView2 Runtime. An internet connection is required.';
  ExtractTemporaryFile('MicrosoftEdgeWebview2Setup.exe');
  if not Exec(ExpandConstant('{tmp}\MicrosoftEdgeWebview2Setup.exe'), '/silent /install', '', SW_HIDE, ewWaitUntilTerminated, ExitCode) then
    Result := 'Could not start the Microsoft WebView2 installer. Please reconnect to the internet and try again.'
  else if (ExitCode <> 0) and (ExitCode <> 3010) then
    Result := 'Microsoft WebView2 installation failed (code ' + IntToStr(ExitCode) + '). Check your internet connection and try again.'
  else if not HasWebView2 then
    Result := 'Microsoft WebView2 is not ready. Install its Evergreen Runtime from https://developer.microsoft.com/microsoft-edge/webview2 and try again.';
  if ExitCode = 3010 then NeedsRestart := True;
end;
