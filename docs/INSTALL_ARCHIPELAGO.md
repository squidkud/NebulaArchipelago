# Nebula Archipelago — install / installation

## English

1. Download `NebulaArchipelago-Setup.exe` from the [latest release](https://github.com/squidkud/NebulaArchipelago/releases/latest).
   Its SHA-256 is next to it (`NebulaArchipelago-Setup.exe.sha256`); compare it with
   `Get-FileHash NebulaArchipelago-Setup.exe` in PowerShell if you want to check the download.
2. Run it. It installs for your user only (no administrator rights) in `%LOCALAPPDATA%\Programs\Nebula Archipelago`.
3. When it asks, choose **your own** Super Mario Galaxy image (USA, RMGE01, revision 0: ISO, RVZ, WBFS... or an extracted
   folder). The image is only read; the game is never included in or sent by the installer.
4. Optional: fill in the Archipelago server, your player name and the room password (you can do it later, see below).
5. Install the APWorld in Archipelago: the Start menu entry **APWorld for Archipelago (folder)** opens the folder that holds
   `super_mario_galaxy_pc_port.apworld`. Put that file in Archipelago's `custom_worlds` folder (the installer already
   copies it there when it can).
6. Start **Nebula Archipelago** from the Start menu. The launcher offers new versions at each start (and in Advanced >
   Check for updates).

Saves and settings stay in `%LOCALAPPDATA%\Nebula`: updating, uninstalling and reinstalling never touch them.

**archipelago.ini** (in `%LOCALAPPDATA%\Nebula`), three lines:

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
   `%LOCALAPPDATA%\Programs\Nebula Archipelago`.
3. Quand il le demande, choisissez **votre propre** image de Super Mario Galaxy (USA, RMGE01, révision 0 : ISO, RVZ,
   WBFS... ou un dossier extrait). L'image est seulement lue ; le jeu n'est jamais fourni ni envoyé par l'installateur.
4. Facultatif : adresse du serveur Archipelago, nom du joueur et mot de passe de la salle (modifiable plus tard).
5. Installez l'APWorld dans Archipelago : l'entrée du menu Démarrer **APWorld for Archipelago (dossier)** ouvre le dossier
   qui contient `super_mario_galaxy_pc_port.apworld`. Mettez ce fichier dans le dossier `custom_worlds` d'Archipelago
   (l'installateur l'y copie déjà quand il le peut).
6. Lancez **Nebula Archipelago** depuis le menu Démarrer. Le lanceur propose les nouvelles versions à chaque démarrage
   (et dans Avancé > Check for updates).

Les sauvegardes et réglages restent dans `%LOCALAPPDATA%\Nebula` : mise à jour, désinstallation et réinstallation n'y
touchent jamais.

**archipelago.ini** (dans `%LOCALAPPDATA%\Nebula`), trois lignes :

```
server=archipelago.gg:38281
slot=VotreNomDeJoueur
password=
```

Windows peut afficher « Éditeur inconnu » pour un installateur non signé : choisissez **Informations complémentaires**
puis **Exécuter quand même**. Windows Defender peut aussi signaler à tort un programme récent ; voir
[SIGNING.md](SIGNING.md).
