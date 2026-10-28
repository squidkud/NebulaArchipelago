; Nebula Archipelago: the installer (Inno Setup 6, https://jrsoftware.org/isinfo.php).
;
; An ordinary installer: it copies the program's files, asks for the player's Super Mario Galaxy image (the game itself is
; never part of it), packs that image into game.pak with nebula-import, optionally writes archipelago.ini, and makes the
; Start menu entries and the uninstaller. It does not copy itself anywhere, run a command interpreter, or delete itself.
;
; Build:  ISCC.exe /DAppVersion=1.2.3 /DStageDir=C:\path\to\stage NebulaArchipelago.iss      (installer\build-archipelago-release.ps1)
; The stage folder holds Nebula.exe, NebulaSettings.exe, NebulaNative.exe, smg_game.dll, the .tsv tables if any, LICENSE,
; THIRD-PARTY-NOTICES.md, README.md, tools\nebula-import.exe and apworld\*.apworld.
;
; Layout (what the launcher, Nebula.exe, expects): {app}\versions\<version>\ holds the program, {app}\content\game.pak the
; packed game. Saves and settings stay in %LOCALAPPDATA%\Nebula, where earlier Nebula installs kept them: an update, an
; uninstall and a reinstall leave them alone.

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
; The packed game is about 3.2 GB; the image is read in place and packed beside the program.
ExtraDiskSpaceRequired=5368709120
VersionInfoVersion={#AppVersion}
VersionInfoCompany=squidkud
VersionInfoDescription=Nebula Archipelago Setup
VersionInfoProductName=Nebula Archipelago

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[CustomMessages]
english.GamePageTitle=Your Super Mario Galaxy
english.GamePageDescription=Nebula Archipelago needs your own copy of the game (USA, RMGE01, revision 0).
english.GamePageSubCaption=Choose your disc image (ISO, RVZ, WBFS...) or paste the path of an extracted game folder. The image is only read; it is not copied or sent anywhere.
english.GamePageFilter=Disc images|*.iso;*.rvz;*.wbfs;*.ciso;*.gcz;*.wia;*.gcm|All files|*.*
english.GamePageNone=Choose your Super Mario Galaxy image first.
english.GamePageMissing=This file or folder does not exist.
english.GamePageRejected=This is not a supported Super Mario Galaxy (USA, RMGE01, revision 0). Choose another image.
english.ApPageTitle=Archipelago connection
english.ApPageDescription=Optional: you can fill this in later (see the README).
english.ApPageSubCaption=Server address (for example archipelago.gg:38281), your player name (the "name:" of your YAML) and the room password if there is one. Leave the first two empty to skip.
english.ApServer=Server address:
english.ApSlot=Player name:
english.ApPassword=Password (empty if none):
english.PackingGame=Packing the game files (5 to 10 minutes, please wait)...
english.PackFailed=The game could not be packed from the image you chose. Run Setup again and choose another image.
english.ShortcutSettings=Nebula Archipelago Settings
english.ShortcutApworld=APWorld for Archipelago (folder)
english.ShortcutIni=Open the Archipelago settings folder
english.TaskDesktop=Create a desktop shortcut
english.RunGame=Start Nebula Archipelago
french.GamePageTitle=Votre Super Mario Galaxy
french.GamePageDescription=Nebula Archipelago a besoin de votre propre copie du jeu (version USA, RMGE01, révision 0).
french.GamePageSubCaption=Choisissez votre image de disque (ISO, RVZ, WBFS...) ou collez le chemin d'un dossier de jeu extrait. L'image est seulement lue : elle n'est ni copiée ni envoyée.
french.GamePageFilter=Images de disque|*.iso;*.rvz;*.wbfs;*.ciso;*.gcz;*.wia;*.gcm|Tous les fichiers|*.*
french.GamePageNone=Choisissez d'abord votre image de Super Mario Galaxy.
french.GamePageMissing=Ce fichier ou ce dossier n'existe pas.
french.GamePageRejected=Ce n'est pas un Super Mario Galaxy pris en charge (USA, RMGE01, révision 0). Choisissez une autre image.
french.ApPageTitle=Connexion Archipelago
french.ApPageDescription=Facultatif : vous pouvez le remplir plus tard (voir le README).
french.ApPageSubCaption=Adresse du serveur (par exemple archipelago.gg:38281), votre nom de joueur (le "name:" de votre YAML) et le mot de passe de la salle s'il y en a un. Laissez les deux premiers vides pour passer.
french.ApServer=Adresse du serveur :
french.ApSlot=Nom du joueur :
french.ApPassword=Mot de passe (vide s'il n'y en a pas) :
french.PackingGame=Préparation des fichiers du jeu (5 à 10 minutes, patientez)...
french.PackFailed=Les fichiers du jeu n'ont pas pu être préparés à partir de l'image choisie. Relancez l'installation et choisissez une autre image.
french.ShortcutSettings=Réglages de Nebula Archipelago
french.ShortcutApworld=APWorld pour Archipelago (dossier)
french.ShortcutIni=Ouvrir le dossier des réglages Archipelago
french.TaskDesktop=Créer un raccourci sur le bureau
french.RunGame=Lancer Nebula Archipelago

[Tasks]
Name: "desktopicon"; Description: "{cm:TaskDesktop}"; Flags: unchecked

[Files]
; The program, in a folder named after its version (the launcher finds the content two folders up).
Source: "{#StageDir}\*"; DestDir: "{app}\versions\{#AppVersion}"; Flags: ignoreversion recursesubdirs createallsubdirs
; The same importer, unpacked to the temporary folder before the install to check the chosen image.
Source: "{#StageDir}\tools\nebula-import.exe"; DestDir: "{tmp}"; Flags: dontcopy

[InstallDelete]
; An update replaces the old version folders (the packed game in {app}\content stays).
Type: filesandordirs; Name: "{app}\versions\*"

[Icons]
Name: "{autoprograms}\Nebula Archipelago"; Filename: "{app}\versions\{#AppVersion}\Nebula.exe"; WorkingDir: "{app}\versions\{#AppVersion}"
Name: "{autoprograms}\{cm:ShortcutSettings}"; Filename: "{app}\versions\{#AppVersion}\NebulaSettings.exe"; WorkingDir: "{app}\versions\{#AppVersion}"
Name: "{autoprograms}\{cm:ShortcutApworld}"; Filename: "{app}\versions\{#AppVersion}\apworld"
Name: "{autoprograms}\{cm:ShortcutIni}"; Filename: "{localappdata}\Nebula"
Name: "{autodesktop}\Nebula Archipelago"; Filename: "{app}\versions\{#AppVersion}\Nebula.exe"; WorkingDir: "{app}\versions\{#AppVersion}"; Tasks: desktopicon

[Run]
; Also after a silent install (the launcher's update runs this installer with /SILENT): the launcher starts again.
Filename: "{app}\versions\{#AppVersion}\Nebula.exe"; Description: "{cm:RunGame}"; WorkingDir: "{app}\versions\{#AppVersion}"; Flags: nowait postinstall

[UninstallDelete]
; The packed game made by Setup (not in the file list). Saves and settings in %LOCALAPPDATA%\Nebula are not touched.
Type: filesandordirs; Name: "{app}\content"
Type: filesandordirs; Name: "{app}\staging"
Type: filesandordirs; Name: "{app}\versions"
Type: dirifempty; Name: "{app}"

[Code]
var
  GamePage: TInputFileWizardPage;
  ArchipelagoPage: TInputQueryWizardPage;

function ContentPak(): String;
begin
  Result := AddBackslash(WizardDirValue) + 'content\game.pak';
end;

{ A packed game from an earlier install (or version) is kept: an update does not ask for the image again. }
function ContentIsPacked(): Boolean;
begin
  Result := FileExists(ContentPak());
end;

function IniPath(): String;
begin
  Result := ExpandConstant('{localappdata}\Nebula\archipelago.ini');
end;

procedure InitializeWizard();
begin
  GamePage := CreateInputFilePage(wpSelectDir, CustomMessage('GamePageTitle'), CustomMessage('GamePageDescription'),
    CustomMessage('GamePageSubCaption'));
  GamePage.Add('', CustomMessage('GamePageFilter'), '');

  ArchipelagoPage := CreateInputQueryPage(GamePage.ID, CustomMessage('ApPageTitle'), CustomMessage('ApPageDescription'),
    CustomMessage('ApPageSubCaption'));
  ArchipelagoPage.Add(CustomMessage('ApServer'), False);
  ArchipelagoPage.Add(CustomMessage('ApSlot'), False);
  ArchipelagoPage.Add(CustomMessage('ApPassword'), True);
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if PageID = GamePage.ID then
    Result := ContentIsPacked()
  else if PageID = ArchipelagoPage.ID then
    Result := FileExists(IniPath());
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  Input: String;
  Code: Integer;
begin
  Result := True;
  if CurPageID = GamePage.ID then
  begin
    Input := Trim(GamePage.Values[0]);
    if Input = '' then
    begin
      MsgBox(CustomMessage('GamePageNone'), mbError, MB_OK);
      Result := False;
    end
    else if not (FileExists(Input) or DirExists(Input)) then
    begin
      MsgBox(CustomMessage('GamePageMissing'), mbError, MB_OK);
      Result := False;
    end
    else
    begin
      { nebula-import identify exits with 0 only for the supported game (USA, RMGE01, revision 0). }
      ExtractTemporaryFile('nebula-import.exe');
      if not Exec(ExpandConstant('{tmp}\nebula-import.exe'), 'identify "' + Input + '"', '', SW_HIDE, ewWaitUntilTerminated, Code)
         or (Code <> 0) then
      begin
        MsgBox(CustomMessage('GamePageRejected'), mbError, MB_OK);
        Result := False;
      end;
    end;
  end;
end;

procedure WriteArchipelagoIni();
var
  Server, Slot, Password, Text: String;
begin
  if FileExists(IniPath()) then Exit;
  Server := Trim(ArchipelagoPage.Values[0]);
  Slot := Trim(ArchipelagoPage.Values[1]);
  Password := ArchipelagoPage.Values[2];
  if (Server = '') or (Slot = '') then Exit;
  ForceDirectories(ExtractFilePath(IniPath()));
  Text := 'server=' + Server + #13#10 + 'slot=' + Slot + #13#10 + 'password=' + Password + #13#10;
  SaveStringToFile(IniPath(), Text, False);
end;

{ The APWorld goes where Archipelago looks for custom worlds, when that folder is there and can be written. A copy is
  always kept in the program folder (the Start menu shortcut "APWorld for Archipelago"). }
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

procedure PackGame();
var
  Importer, Output, Input: String;
  Code: Integer;
begin
  if GamePage = nil then Exit;
  Input := Trim(GamePage.Values[0]);
  if Input = '' then Exit;
  Importer := ExpandConstant('{app}\versions\{#AppVersion}\tools\nebula-import.exe');
  Output := ExpandConstant('{app}\content');
  WizardForm.StatusLabel.Caption := CustomMessage('PackingGame');
  WizardForm.ProgressGauge.Style := npbstMarquee;
  DelTree(Output, True, True, True);
  if Exec(Importer, 'import "' + Input + '" --output "' + Output + '"', '', SW_HIDE, ewWaitUntilTerminated, Code) and (Code = 0)
     and FileExists(Output + '\game.pak') then
    SaveStringToFile(Output + '\content.json', '{"formatVersion":1}', False)
  else
  begin
    DelTree(Output, True, True, True);
    SuppressibleMsgBox(CustomMessage('PackFailed'), mbError, MB_OK, IDOK);
  end;
  WizardForm.ProgressGauge.Style := npbstNormal;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    CopyApworld();
    if not WizardSilent then
    begin
      WriteArchipelagoIni();
      if not FileExists(ExpandConstant('{app}\content\game.pak')) then PackGame();
    end;
  end;
end;
