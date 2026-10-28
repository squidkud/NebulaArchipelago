; Nebula Archipelago: the installer (Inno Setup 6, https://jrsoftware.org/isinfo.php).
;
; An ordinary installer: it copies the program's files and makes the Start menu entry and the uninstaller. It asks for
; nothing: the launcher (Nebula.exe) asks for the player's Super Mario Galaxy image, packs it with nebula-import, edits
; archipelago.ini and looks for updates. It does not copy itself anywhere, run a command interpreter, or delete itself.
;
; Build:  ISCC.exe /DAppVersion=1.2.3 /DStageDir=C:\path\to\stage NebulaArchipelago.iss      (installer\build-archipelago-release.ps1)
; The stage folder holds Nebula.exe (the launcher), NebulaNative.exe, smg_game.dll, the .tsv tables if any, LICENSE,
; THIRD-PARTY-NOTICES.md, README.md, tools\nebula-import.exe and apworld\*.apworld.
;
; Layout (what the launcher expects): {app}\versions\<version>\ holds the program, {app}\content\game.pak the packed
; game (made by the launcher). Saves and settings stay in %LOCALAPPDATA%\Nebula, where earlier Nebula installs kept them:
; an update, an uninstall and a reinstall leave them alone.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef StageDir
  #error StageDir must be defined: ISCC /DStageDir=<folder>
#endif

[Setup]
AppId={{7D3B4C0E-5A1F-4E7B-9C62-2F8A6E1B90D4}
AppName=Nebula Archipelago
AppVersion={#AppVersion}
AppVerName=Nebula Archipelago {#AppVersion}
AppPublisher=squidkud
AppPublisherURL=https://github.com/squidkud/NebulaArchipelago
AppSupportURL=https://github.com/squidkud/NebulaArchipelago/issues
AppUpdatesURL=https://github.com/squidkud/NebulaArchipelago/releases
DefaultDirName={localappdata}\Programs\Nebula Archipelago
DefaultGroupName=Nebula Archipelago
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17134
OutputDir=..\..\dist
OutputBaseFilename=NebulaArchipelago-Setup
SetupIconFile=..\assets\nebula.ico
UninstallDisplayIcon={app}\versions\{#AppVersion}\Nebula.exe
LicenseFile=..\..\LICENSE
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
VersionInfoVersion={#AppVersion}
VersionInfoCompany=squidkud
VersionInfoDescription=Nebula Archipelago Setup
VersionInfoProductName=Nebula Archipelago

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[CustomMessages]
english.TaskDesktop=Create a desktop shortcut
english.RunGame=Start Nebula Archipelago
french.TaskDesktop=Créer un raccourci sur le bureau
french.RunGame=Lancer Nebula Archipelago

[Tasks]
Name: "desktopicon"; Description: "{cm:TaskDesktop}"; Flags: unchecked

[Files]
; The program, in a folder named after its version (the launcher finds the content two folders up).
Source: "{#StageDir}\*"; DestDir: "{app}\versions\{#AppVersion}"; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
; An update replaces the old version folders (the packed game in {app}\content stays).
Type: filesandordirs; Name: "{app}\versions\*"

[Icons]
Name: "{autoprograms}\Nebula Archipelago"; Filename: "{app}\versions\{#AppVersion}\Nebula.exe"; WorkingDir: "{app}\versions\{#AppVersion}"
Name: "{autodesktop}\Nebula Archipelago"; Filename: "{app}\versions\{#AppVersion}\Nebula.exe"; WorkingDir: "{app}\versions\{#AppVersion}"; Tasks: desktopicon

[Run]
; Also after a silent install (the launcher's update runs this installer with /SILENT): the launcher starts again.
Filename: "{app}\versions\{#AppVersion}\Nebula.exe"; Description: "{cm:RunGame}"; WorkingDir: "{app}\versions\{#AppVersion}"; Flags: nowait postinstall

[UninstallDelete]
; The packed game made by the launcher (not in the file list). Saves and settings in %LOCALAPPDATA%\Nebula are not touched.
Type: filesandordirs; Name: "{app}\content"
Type: filesandordirs; Name: "{app}\staging"
Type: filesandordirs; Name: "{app}\versions"
Type: dirifempty; Name: "{app}"

[Code]
{ The APWorld goes where Archipelago looks for custom worlds, when that folder is there and can be written. The launcher's
  Archipelago tab can also copy it (or open its folder) at any time. }
procedure CopyApworld();
var
  Source, Folder, Name: String;
  Found: TFindRec;
begin
  Folder := ExpandConstant('{commonappdata}\Archipelago\custom_worlds');
  if not DirExists(Folder) then Exit;
  Source := ExpandConstant('{app}\versions\{#AppVersion}\apworld\');
  if FindFirst(Source + '*.apworld', Found) then
  begin
    try
      repeat
        Name := Found.Name;
        FileCopy(Source + Name, AddBackslash(Folder) + Name, False);
      until not FindNext(Found);
    finally
      FindClose(Found);
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then CopyApworld();
end;
