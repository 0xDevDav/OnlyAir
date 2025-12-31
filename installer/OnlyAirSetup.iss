; OnlyAir Professional Installer
; Virtual Camera & Virtual Audio Solution

#define MyAppName "OnlyAir"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "OnlyAir Software"
#define MyAppURL "https://github.com/0xDevDav/OnlyAir"
#define MyAppExeName "OnlyAir.exe"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=
OutputDir=output
OutputBaseFilename=OnlyAirSetup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes
PrivilegesRequired=admin
MinVersion=10.0
WizardStyle=modern
DisableWelcomePage=no
WizardImageStretch=no
SetupIconFile=..\resources\OnlyAir.ico
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"

[Messages]
english.WelcomeLabel2=This will install [name/ver] on your computer.%n%nOnlyAir creates virtual camera and audio devices to stream video files to applications like Zoom, Meet, Teams, and more.%n%nClick Next to continue.
italian.WelcomeLabel2=Questo installerà [name/ver] sul tuo computer.%n%nOnlyAir crea dispositivi virtuali per camera e audio per trasmettere video su applicazioni come Zoom, Meet, Teams e altre.%n%nClicca Avanti per continuare.

[CustomMessages]
; English
english.VirtualDevicesTitle=Virtual Devices
english.VirtualDevicesDesc=OnlyAir will install virtual camera and audio drivers
english.VirtualDevicesInfo=OnlyAir requires the following virtual devices:%n%nVIRTUAL CAMERA (Softcam)%nCreates a virtual webcam that applications like Zoom, Teams, Meet can use.%nThe driver will be automatically registered during installation.%n%nVIRTUAL AUDIO (VB-Cable)%nCreates a virtual audio cable to send audio to applications.%nVB-Cable is developed by VB-Audio Software (https://vb-audio.com/Cable/)%nand is free for personal use.%n%nBoth drivers will be installed/updated automatically.
english.RegisteringVCam=Registering Virtual Camera...
english.InstallingVBAudio=Installing VB-Cable Virtual Audio...
english.CreateDesktopShortcut=Create a &desktop shortcut
english.CreateStartMenuShortcut=Create a &Start Menu shortcut
english.ShortcutsGroup=Shortcuts:

; Italian
italian.VirtualDevicesTitle=Dispositivi Virtuali
italian.VirtualDevicesDesc=OnlyAir installerà i driver per camera e audio virtuali
italian.VirtualDevicesInfo=OnlyAir richiede i seguenti dispositivi virtuali:%n%nCAMERA VIRTUALE (Softcam)%nCrea una webcam virtuale che applicazioni come Zoom, Teams, Meet possono usare.%nIl driver verrà registrato automaticamente durante l'installazione.%n%nAUDIO VIRTUALE (VB-Cable)%nCrea un cavo audio virtuale per inviare l'audio alle applicazioni.%nVB-Cable è sviluppato da VB-Audio Software (https://vb-audio.com/Cable/)%ned è gratuito per uso personale.%n%nEntrambi i driver verranno installati automaticamente.
italian.RegisteringVCam=Registrazione Camera Virtuale...
italian.InstallingVBAudio=Installazione VB-Cable Audio Virtuale...
italian.CreateDesktopShortcut=Crea un collegamento sul &desktop
italian.CreateStartMenuShortcut=Crea un collegamento nel menu &Start
italian.ShortcutsGroup=Collegamenti:

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopShortcut}"; GroupDescription: "{cm:ShortcutsGroup}"
Name: "startmenuicon"; Description: "{cm:CreateStartMenuShortcut}"; GroupDescription: "{cm:ShortcutsGroup}"

[Files]
; Main application
Source: "..\build\bin\Release\OnlyAir.exe"; DestDir: "{app}"; Flags: ignoreversion

; Softcam DLL (Virtual Camera)
Source: "..\build\bin\Release\softcam.dll"; DestDir: "{app}"; Flags: ignoreversion

; FFmpeg DLLs
Source: "..\build\bin\Release\avcodec*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\avformat*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\avutil*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\avfilter*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\avdevice*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\swscale*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\swresample*.dll"; DestDir: "{app}"; Flags: ignoreversion

; Qt6 DLLs
Source: "..\build\bin\Release\Qt6Core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\Qt6Gui.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\Qt6Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\Qt6OpenGL.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\Qt6OpenGLWidgets.dll"; DestDir: "{app}"; Flags: ignoreversion

; Qt6 plugins
Source: "..\build\bin\Release\platforms\qwindows.dll"; DestDir: "{app}\platforms"; Flags: ignoreversion

; Application icon
Source: "..\resources\OnlyAir.ico"; DestDir: "{app}"; Flags: ignoreversion

; VB-Cable installer
Source: "..\third_party\vbcable\VBCABLE_Setup_x64.exe"; DestDir: "{tmp}"; Flags: ignoreversion deleteafterinstall

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\OnlyAir.ico"; Tasks: startmenuicon
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"; IconFilename: "{app}\OnlyAir.ico"; Tasks: startmenuicon
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\OnlyAir.ico"; Tasks: desktopicon

[Run]
; Register Softcam Virtual Camera
Filename: "regsvr32.exe"; Parameters: "/s ""{app}\softcam.dll"""; StatusMsg: "{cm:RegisteringVCam}"; Flags: runhidden waituntilterminated

; Install VB-Cable
Filename: "{tmp}\VBCABLE_Setup_x64.exe"; Parameters: "-i -h"; StatusMsg: "{cm:InstallingVBAudio}"; Flags: waituntilterminated

; Launch application
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Unregister Softcam Virtual Camera
Filename: "regsvr32.exe"; Parameters: "/u /s ""{app}\softcam.dll"""; Flags: runhidden waituntilterminated skipifdoesntexist; RunOnceId: "UnregisterSoftcam"

[Code]
procedure UnregisterExistingDriver;
var
  DllPath: String;
  ResultCode: Integer;
begin
  DllPath := ExpandConstant('{app}\softcam.dll');
  if FileExists(DllPath) then
  begin
    Exec('regsvr32.exe', '/u /s "' + DllPath + '"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;
end;

function InitializeSetup: Boolean;
begin
  Result := True;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
  begin
    UnregisterExistingDriver;
  end;
end;

procedure InitializeWizard;
var
  InfoPage: TWizardPage;
  InfoLabel: TNewStaticText;
begin
  InfoPage := CreateCustomPage(wpWelcome,
    CustomMessage('VirtualDevicesTitle'),
    CustomMessage('VirtualDevicesDesc'));

  InfoLabel := TNewStaticText.Create(InfoPage);
  InfoLabel.Parent := InfoPage.Surface;
  InfoLabel.Left := 0;
  InfoLabel.Top := 0;
  InfoLabel.Width := InfoPage.SurfaceWidth;
  InfoLabel.Height := 250;
  InfoLabel.WordWrap := True;
  InfoLabel.Caption := CustomMessage('VirtualDevicesInfo');
end;
