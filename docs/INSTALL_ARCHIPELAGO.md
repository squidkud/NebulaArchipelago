# Nebula Archipelago — install / installation

## English

1. Download `NebulaArchipelago-Setup.exe` from the [latest release](https://github.com/squidkud/NebulaArchipelago/releases/latest).
   Its SHA-256 is next to it (`NebulaArchipelago-Setup.exe.sha256`); compare it with
   `Get-FileHash NebulaArchipelago-Setup.exe` in PowerShell if you want to check the download.
2. Run it. It installs for your user only (no administrator rights) in `%LOCALAPPDATA%\Programs\Nebula Archipelago`, and
   asks nothing.
3. Start **Nebula Archipelago** from the Start menu. It is the only program to open: the game starts from its **Save and
   play** button. Its tabs:
   - **Archipelago**: choose **your own** Super Mario Galaxy image (USA, RMGE01, revision 0: ISO, RVZ, WBFS... or an
     extracted folder); it is packed once (5 to 10 minutes) and only read, never sent anywhere. Then type the server
     address, your player name and the room password: they are written to `archipelago.ini`. The same tab copies the
     APWorld into Archipelago (or opens its folder).
   - **Display, Graphics, Controls, Audio, General, Mods**: the game's settings.
   - **Updates**: the launcher looks for a newer release by itself each time it opens (you can turn that off here) and
     installs it, after checking its SHA-256, when you ask.

Saves and settings stay in `%LOCALAPPDATA%\Nebula`: updating, uninstalling and reinstalling never touch them.

**archipelago.ini** (in `%LOCALAPPDATA%\Nebula`; the Archipelago tab writes it, you can also edit it by hand), three lines:

```
server=archipelago.gg:38281
slot=YourPlayerName
password=
```

Windows may show "Unknown publisher" for an unsigned installer: choose **More info**, then **Run anyway**. Windows
Defender may also wrongly flag a new program; if it does, see [SIGNING.md](SIGNING.md).

## Français

1. Téléchargez `NebulaArchipelago-Setup.exe` depuis la [dernière version](https://github.com/squidkud/NebulaArchipelago/releases/latest).
   Son empreinte SHA-256 est à côté (`NebulaArchipelago-Setup.exe.sha256`) ; pour vérifier le téléchargement, comparez-la
   avec le résultat de `Get-FileHash NebulaArchipelago-Setup.exe` dans PowerShell.
2. Lancez-le. Il s'installe pour votre utilisateur seulement (sans droits administrateur), dans
   `%LOCALAPPDATA%\Programs\Nebula Archipelago`, et ne pose aucune question.
3. Lancez **Nebula Archipelago** depuis le menu Démarrer. C'est le seul programme à ouvrir : le jeu démarre avec le bouton
   **Save and play**. Ses onglets :
   - **Archipelago** : choisissez **votre propre** image de Super Mario Galaxy (USA, RMGE01, révision 0 : ISO, RVZ, WBFS...
     ou un dossier extrait) ; elle est préparée une seule fois (5 à 10 minutes), seulement lue, jamais envoyée. Saisissez
     ensuite l'adresse du serveur, votre nom de joueur et le mot de passe de la salle : ils sont écrits dans
     `archipelago.ini`. Le même onglet copie l'APWorld dans Archipelago (ou ouvre son dossier).
   - **Display, Graphics, Controls, Audio, General, Mods** : les réglages du jeu.
   - **Updates** : le lanceur cherche tout seul une nouvelle version à chaque ouverture (désactivable ici) et l'installe,
     après vérification de son SHA-256, quand vous le demandez.

Les sauvegardes et réglages restent dans `%LOCALAPPDATA%\Nebula` : mise à jour, désinstallation et réinstallation n'y
touchent jamais.

**archipelago.ini** (dans `%LOCALAPPDATA%\Nebula` ; l'onglet Archipelago l'écrit, vous pouvez aussi le modifier à la main), trois lignes :

```
server=archipelago.gg:38281
slot=VotreNomDeJoueur
password=
```

Windows peut afficher « Éditeur inconnu » pour un installateur non signé : choisissez **Informations complémentaires**
puis **Exécuter quand même**. Windows Defender peut aussi signaler à tort un programme récent ; voir
[SIGNING.md](SIGNING.md).
