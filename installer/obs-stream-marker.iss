; Inno Setup script - builds OBS-Stream-Marker-Setup-<version>.exe
; Build:  ISCC.exe installer\obs-stream-marker.iss   (after scripts\build.ps1 created .\dist)
#define AppName "OBS Stream Marker"
#define AppVersion "1.0.0"
#define Plugin "obs-stream-marker"

[Setup]
AppId={{6F1B7C3E-5A52-4C1E-9B7D-0B5A57E4A9C1}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=veekaxx
AppPublisherURL=https://instagram.com/veekaxx
DefaultDirName={commonappdata}\obs-studio\plugins\{#Plugin}
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\release
OutputBaseFilename=OBS-Stream-Marker-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
VersionInfoVersion={#AppVersion}

[Files]
Source: "..\dist\{#Plugin}\bin\64bit\*"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "..\dist\{#Plugin}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs

[Code]
function InitializeSetup(): Boolean;
var
  ResultCode: Integer;
begin
  Result := True;
  if Exec(ExpandConstant('{sys}\cmd.exe'), '/c tasklist /fi "imagename eq obs64.exe" | find /i "obs64.exe" >nul', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) and (ResultCode = 0) then
  begin
    MsgBox('OBS Studio is running. Please close OBS completely, then run this installer again.', mbError, MB_OK);
    Result := False;
  end;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then
    WizardForm.FinishedLabel.Caption := 'OBS Stream Marker was installed.' + #13#10#13#10 +
      'Start OBS and open  Docks > Stream Marker.' + #13#10#13#10 +
      'Made by veekaxx - instagram.com/veekaxx';
end;
