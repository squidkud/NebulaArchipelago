# Archipelago (multiworld) avec Nebula — guide pas à pas

Ce guide explique comment compiler Nebula avec le support d'[Archipelago](https://archipelago.gg), puis jouer à Super
Mario Galaxy dans un multiworld. Il suppose que vous n'avez jamais compilé de programme : suivez les étapes dans l'ordre.

> **État du travail : écrit, mais jamais compilé sous Windows ni essayé dans le jeu.** Le client Archipelago a été testé
> contre un vrai serveur Archipelago (connexion, envoi des étoiles, réception des objets, but final), l'APWorld génère
> des parties sans erreur (plus de 30 graines essayées) et le code du jeu passe une vérification de syntaxe. En revanche,
> la compilation Windows et le comportement réel dans le jeu n'ont pas pu être vérifiés. Si une étape échoue ou si
> quelque chose ne se passe pas comme décrit, envoyez-moi le message d'erreur et le fichier `archipelago.log` (voir
> plus bas) : c'est ce qu'il faut pour corriger.

## Ce que fait (et ne fait pas) cette version

- **Chaque étoile a son nom** : « Good Egg Galaxy - Dino Piranha », « Honeyhive Galaxy - Bee Mario Takes Flight », etc.
  (les étoiles du jeu en anglais, dont les étoiles vertes et celles des comètes). Collecter une étoile envoie un
  objet à quelqu'un dans le multiworld. Les étoiles des comètes sont facultatives, de même que les étoiles à pièces
  violettes (après la fin du jeu) et la cache-cache de Luigi (expérimental).
- **Les étoiles sont des objets** : **Étoiles de Puissance, Étoiles Vertes et Grandes Étoiles** arrivent du multiworld
  (comme dans l'APWorld Dolphin). Le compteur d'étoiles du jeu, les étoiles demandées par chaque galaxie et les
  événements « Grande Étoile » / « Étoile Verte » lisent ces objets, pas les étoiles que vous avez ramassées.
- **Les Grandes Étoiles ouvrent les dômes** de l'Observatoire (la 1re est donnée au départ : elle ouvre le dôme de la
  Terrasse mais **ne compte pas** dans le compteur d'étoiles ; les 5 autres ouvrent la Fontaine, la Cuisine, la Chambre,
  la Salle des machines et le Jardin dans cet ordre et comptent comme des étoiles).
- **Démarrage à l'Observatoire** dès que vous choisissez une sauvegarde, sans avoir fini le jeu.
- **Luigi** est toujours disponible sur n'importe quelle sauvegarde (bouton Luigi au choix du fichier), sans option.
- Les autres objets sont des bonus : champignon 1-Up ou 50 Star Bits.
- **Les pop-ups disent d'où vient un objet** : « Power Star from Bob (Good Egg Galaxy - Dino Piranha) » (l'emplacement
  n'est affiché que si la ligne reste assez courte), « Sent … to Bob » pour ce que vous envoyez, « Found … » pour
  ce que vous trouvez pour vous-même.
- Une **logique** est fournie : les galaxies, les dômes et les galaxies cachées demandent les bons objets.
- **But** : prendre l'étoile du boss final, dans le Réacteur de la Galaxie de Bowser. Le but est envoyé dès que vous
  prenez l'étoile (sans attendre l'épilogue ni les crédits). Le nombre d'étoiles choisi dans le YAML sert seulement
  à ouvrir ce dernier niveau (Rosetta vous parle d'harmonie, puis il est ouvert) ; les Grandes Étoiles ne sont pas
  nécessaires.
- **Exigences d'étoiles** par galaxie et **coût des Lumas affamés** réglables (voir le tableau des options).
- **Mélange des galaxies** (`galaxy_shuffle`, `galaxy_shuffle_type`) : dans l'Observatoire, chaque petite galaxie
  garde ce qui l'ouvre (le nombre d'étoiles demandé, sa place dans le dôme), mais **montre, nomme et lance une autre
  galaxie**. Les étoiles de cette galaxie sont les emplacements de cette galaxie, où qu'elle se trouve. Détails dans
  « Options du mélange » plus bas. Après une étoile, vous revenez dans la salle où vous aviez choisi la galaxie (et
  non celle où elle se trouve dans le jeu normal). Expérimental : jamais essayé en jeu.
- **Choix de l'étoile** : quand vous choisissez une galaxie, **toutes** ses étoiles sont proposées dans le choix
  d'étoile : les normales, celles des comètes, celles de Luigi et les cachées (celles déjà prises sont allumées, les
  autres grisées), pas seulement la suivante. Seules les 4 premières « étoiles en plus » tiennent à l'écran.
- **Pas de textes du Luma noir** : les leçons du Butler dans les dômes (comment aller dans une galaxie, les Star Bits,
  « un nouveau boss a été découvert ») ne s'affichent plus.
- **Le spin** est donné dès le départ (le jeu le donne normalement avec le Luma de la Porte des Étoiles).
- **Galaxies de l'Observatoire** (`Observatory Specials`) : les 10 galaxies « secrètes » (Sweet Sweet, Sling Pod,
  Drip Drop, Bigmouth, Sand Spiral, Boo's Boneyard, Snow Cap et les trois galaxies des Étoiles Vertes) sont
  mélangées entre elles : la planète « ? » qui apparaît dans l'Observatoire garde ce qui la fait apparaître (Grande
  Étoile, étoile précise, Luma affamé) mais emmène vers une autre galaxie. Expérimental aussi.
- **Mélange des dômes** (`dome_shuffle`) : une salle prend la place d'une autre (la Salle des machines à la place de
  la Terrasse, la Cuisine à la place de la Chambre...). La porte qui s'ouvre avec la n-ième Grande Étoile mène à une
  autre salle ; les galaxies d'une salle restent dans leur salle. Le nom affiché sur la porte reste celui de la
  porte d'origine. Une fois dans une salle, ses galaxies s'ouvrent avec le seul nombre d'étoiles demandé (les
  conditions de Grande Étoile du jeu, propres à sa salle d'origine, sont ignorées). Expérimental aussi.
- **Couleur des étoiles selon l'objet** (`star_colors: true`, désactivé par défaut) : la couleur d'une étoile (dans le
  niveau et dans le choix des étoiles) dit la classe de l'objet qu'elle cache : **vert** = objet normal, **bleu** =
  utile, **or** = progression (il en faut pour avancer), **rouge** = piège. Les Grandes Étoiles ne changent pas. Le
  jeu demande au serveur ce que contient chaque lieu au moment de la connexion. Attention : c'est un petit spoiler
  voulu (on voit quelles étoiles comptent).
- **Boutique de Star Bits** (`shop_items`, de 1 à 20, 8 par défaut ; prix de `shop_min_price` (100 par défaut) à
  `shop_max_price` (2000 par défaut), les autres étagères en progression régulière) : le
  Luma jaune du début de l'Observatoire, celui qui ouvrait la page des succès de Nebula, ouvre à la place une
  boutique. Chaque étagère est un lieu du multiworld : elle contient un objet (de votre monde ou d'un autre, le nom
  et le joueur sont affichés une fois connecté) et coûte des Star Bits. Chaque étagère montre l'image de l'objet s'il
  est de votre jeu (pièges compris), sinon le logo Archipelago. Pas de curseur : manette/clavier, **A** demande
  « Acheter X pour N Star Bits ? » (Oui/Non, **B** annule), **B** ferme la boutique. À l'ouverture, les étagères non
  achetées sont déclarées en **hint** aux joueurs concernés. Il faut être connecté au serveur pour acheter ; un achat
  est mémorisé (par le serveur). Les prix ne sont pas pris en compte dans la logique. La boutique est obligatoire
  (de 1 à 20 étagères).
- **Transformations en objets** (`transformation_items: true`, désactivé par défaut) : le Champignon Abeille, le
  Champignon Boo, le Champignon Ressort, la Fleur de Feu, la Fleur de Glace et le vol (Étoile Rouge) deviennent des
  objets du multiworld (« Bee Mushroom », « Boo Mushroom », « Spring Mushroom », « Fire Flower », « Ice Flower »,
  « Flying (Red Star) »). Ramasser le bonus dans une galaxie **ne fait rien** tant qu'on n'a pas reçu l'objet. Mario
  arc-en-ciel n'est pas concerné. La logique connaît les étoiles dont toute la mission est une transformation
  (Honeyhive, Honeyclimb, Boo's Boneyard, la première étoile de Dusty Dune, Cake Lane, Freezeflame ; liste dans
  `Forms.py`, écrite de mémoire) ; les six objets restent placés tôt, car une autre étoile peut en demander un
  (revenez plus tard). On peut aussi en donner au départ avec `start_inventory`.
- **Sauvegarde automatique** : en mode Archipelago, le jeu sauvegarde seul au retour de chaque étoile, sans question
  ni fenêtre : un petit « Sauvegarde… » apparaît en bas à gauche pendant trois secondes (on continue de jouer), puis le
  bruit de sauvegarde terminée.
- **Cheat console** : un objet envoyé depuis la console du serveur (`/send`) s'affiche comme les autres avec la mention
  « Cheat console ».
- **Luma goinfre** : il apparaît dès que son dôme s'ouvre, même si vous êtes déjà dans l'Observatoire.
- **Pas de comètes aléatoires** : les comètes qui passent sur les galaxies sont supprimées ; les étoiles de comète se
  choisissent directement dans le choix des étoiles.
- **Grande Étoile d'un boss** : pas de cinématique de retour à l'Observatoire ; on revient au dôme comme après une
  étoile normale.
- **Pièges** (`trap_percentage`, de 0 à 100, 0 par défaut) : part des objets « remplissage » de votre monde qui sont
  des pièges :
  - **Fire Trap** (Mario prend feu), **Shock Trap** (décharge électrique), **Freeze Trap** (Mario est gelé) et
    **Spike Trap** (un coup qui fait mal) : chacun coûte un point de santé, donc un piège peut faire perdre une vie ;
  - **Star Bit Thief** : on vous prend jusqu'à 50 Star Bits ;
  - **Quake Trap** : la caméra tremble pendant 10 secondes ;
  - **Confusion Trap** : le stick est inversé pendant 20 secondes.

  `trap_types` choisit lesquels peuvent tomber (tous par défaut), par ex. `trap_types: [Quake Trap, Confusion Trap]`.
  Un piège qui ne peut pas démarrer (Mario déjà blessé, en cinématique...) attend le bon moment ; il ne s'applique
  qu'en jeu, jamais pendant un menu.
- **DeathLink** (`death_link: true`) : chaque vie perdue est envoyée aux autres joueurs qui ont DeathLink, et leurs
  morts font mourir Mario ici. Une mort venue d'un DeathLink n'est pas renvoyée (pas de boucle).
- **Étoile arc-en-ciel** (`rainbow_star_percentage`, de 0 à 100, 0 par défaut) : part des objets « remplissage »
  qui sont des Rainbow Stars (Mario devient arc-en-ciel, invincible, un moment). C'est le seul objet bonus.
- **Mélange de la musique** (`music_shuffle: true`) : chaque musique de niveau ou de boss joue une autre musique (même
  multiworld = même mélange). Les jingles (fanfare d'étoile, game over), les chansons des cinématiques et tous les
  bruitages ne changent pas.
- **Mélange des ennemis** (`enemy_shuffle: true`, désactivé par défaut, expérimental) : les ennemis « simples » des niveaux
  (Goombas, Bob-ombs, Octogombas, Boos de feu, Plantes Piranha, Taupes, Chauves-souris) sont remplacés par un autre type,
  toujours de la même façon pour un même multiworld. Jamais les boss ni les autres créatures, ni un ennemi lié à une
  mission (« tuez-les tous »), un chemin ou un interrupteur. Pas testé en jeu.
- **Mélange « Full »** (`galaxy_shuffle_type: full`) : tout ce que vous avez mis dans `galaxy_shuffle` est dans UN seul
  tas, n'importe quelle galaxie à n'importe quelle place : un dôme peut n'avoir aucun boss ou en avoir trois, et les
  galaxies de l'Observatoire (Sweet Sweet...) peuvent se trouver dans les dômes, où la place garde son
  propre modèle de miniature (mais le nom, les étoiles et le lancement sont ceux de la galaxie qui s'y trouve). Une galaxie de dôme peut aussi se retrouver sur une
  planète « ? » de l'Observatoire.
- **Galaxies de dôme toujours visibles** : plus de « ??? » dans une salle tant qu'un événement de l'histoire ou une
  étoile précise du jeu manque. Les quatre galaxies qui demandaient une étoile précise (Buoy Base, Honeyclimb,
  Bonefin, Matter Splatter) s'ouvrent maintenant avec le seul nombre d'étoiles.
- **Logo Archipelago** : les fenêtres qui annoncent une étoile reçue ou la connexion montrent le logo Archipelago à la
  place de l'étoile.
- **Pas encore faits** : couleurs de Mario/Luigi (`mario_colors`), `hide_galaxy`, le mode `by_sphere` des Lumas. Ce
  sont des morceaux qui touchent à l'affichage et aux modèles du jeu : je préfère ne pas promettre ce que je ne peux
  pas contrôler. Ne mettez pas ces options dans votre YAML.
- **Un fichier de sauvegarde neuf par multiworld** : une sauvegarde n'est pas liée à une partie Archipelago.

## 1. Installer les outils (une seule fois)

Tout se fait sous Windows 10/11. Installez, dans cet ordre :

1. **Visual Studio 2022 Community** (gratuit) — [visualstudio.microsoft.com](https://visualstudio.microsoft.com). Dans
   l'installateur, cochez la charge de travail **« Développement Desktop en C++ »** (elle inclut le SDK Windows).
2. **LLVM / clang 19 ou plus récent** — [github.com/llvm/llvm-project/releases](https://github.com/llvm/llvm-project/releases),
   fichier `LLVM-xx.x.x-win64.exe`. Installez-le dans le dossier proposé (`C:\Program Files\LLVM`) : le script de
   compilation l'attend là.
3. **CMake** (3.24 ou plus) — [cmake.org/download](https://cmake.org/download). À l'installation, choisissez
   « Add CMake to the system PATH ».
4. **Ninja** — [github.com/ninja-build/ninja/releases](https://github.com/ninja-build/ninja/releases), fichier
   `ninja-win.zip`. Dézippez `ninja.exe` dans un dossier qui est dans le PATH (par exemple `C:\Windows`, ou ajoutez son
   dossier au PATH).
5. **Rust (stable, MSVC)** — [rustup.rs](https://rustup.rs). Il sert à l'outil qui lit votre ISO.
6. **.NET 8 SDK** — [dotnet.microsoft.com/download/dotnet/8.0](https://dotnet.microsoft.com/download/dotnet/8.0).
7. **Git** — [git-scm.com](https://git-scm.com/download/win).

Fermez puis rouvrez PowerShell après les installations pour que le PATH soit pris en compte.

## 2. Récupérer le code

Dans PowerShell (menu Démarrer → « PowerShell »), choisissez un dossier de travail court, par exemple `C:\dev` :

```powershell
mkdir C:\dev
cd C:\dev
git clone https://github.com/keepvaibin/nebula.git
cd nebula
```

Puis appliquez les modifications Archipelago. Deux cas :

- **Vous avez reçu un fichier `nebula-archipelago-complet.patch`** : partez d'un dossier `nebula` tout neuf (si vous
  aviez déjà une version modifiée, supprimez le dossier `C:\dev\nebula` et refaites les commandes `git clone` ci-dessus),
  copiez le fichier dedans, puis `git apply --whitespace=nowarn nebula-archipelago-complet.patch`
- **Vous avez reçu un dossier ou un zip `nebula-archipelago-files`** : copiez son contenu par-dessus le dossier `nebula`
  (en acceptant de remplacer les fichiers existants).

Si Nebula a changé entre-temps, `git apply` peut refuser : dans ce cas, donnez-moi le message d'erreur.

## 3. Préparer le jeu (une seule fois)

Votre copie de Super Mario Galaxy (version américaine, identifiant `RMGE01`) est lue par l'outil d'import. Adaptez le
chemin vers votre fichier :

À **chaque nouvelle fenêtre PowerShell**, commencez par ces deux lignes (la première autorise les scripts pour cette
fenêtre seulement, la seconde charge le compilateur de Visual Studio ; sans elles, `cmake` répond « No CMAKE_C_COMPILER
could be found » et `build.ps1` est bloqué) :

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
. .\tools\enter_msvc_environment.ps1
```

Puis :

```powershell
cargo build --release --locked -p nebula-import
.\target\release\nebula-import.exe identify "D:\Jeux\SMG.iso"
```

La commande doit répondre `RMGE01`. Ensuite :

```powershell
.\target\release\nebula-import.exe import "D:\Jeux\SMG.iso" --output C:\dev\nebula-content
```

Cela crée le dossier `C:\dev\nebula-content` (avec `game.pak`).

## 4. Compiler

Toujours dans le même PowerShell (celui où vous avez tapé les deux lignes ci-dessus), dans `C:\dev\nebula` :

```powershell
$cmake = "C:\Program Files\CMake\bin\cmake.exe"
Test-Path $cmake        # doit répondre True
& $cmake -S . -B build\host -G Ninja -DCMAKE_BUILD_TYPE=Release
& $cmake --build build\host --target NebulaNative
.\native\build.ps1 -Petari native\petari -Build build\game
```

On appelle CMake par son chemin complet parce que devkitPro en installe un autre, qui passe avant dans le `PATH` et
casse la compilation avec Visual Studio (message « The C compiler ... is not able to compile a simple test program »,
chemins en `/c/...`). Si ce message est apparu, supprimez d'abord le dossier de ce premier essai :
`Remove-Item -Recurse -Force build\host`.

La première commande de `cmake` télécharge des dépendances : il faut Internet et un peu de patience. À la fin, vous
devez avoir :

- `build\host\NebulaNative.exe`
- `build\game\smg_game.dll`

Pour vérifier le client Archipelago avant de lancer le jeu (facultatif) :

```powershell
& $cmake --build build\host
& "C:\Program Files\CMake\bin\ctest.exe" --test-dir build\host --output-on-failure
```

## 5. Rassembler et lancer

Mettez `NebulaNative.exe` et `smg_game.dll` dans **le même dossier** (par exemple `C:\dev\nebula-run`). Les deux doivent
venir de cette compilation : un `NebulaNative.exe` d'une autre version ne marchera pas avec ce `smg_game.dll`.

```powershell
mkdir C:\dev\nebula-run
copy build\host\NebulaNative.exe C:\dev\nebula-run
copy build\game\smg_game.dll C:\dev\nebula-run
```

Le lancement (après l'étape 7) :

```powershell
cd C:\dev\nebula-run
.\NebulaNative.exe --pak C:\dev\nebula-content\game.pak --nand "$env:LOCALAPPDATA\Nebula\saves"
```

## 6. Générer le multiworld

1. Installez [Archipelago](https://github.com/ArchipelagoMW/Archipelago/releases) (le `Setup.Archipelago.exe`).
2. Copiez `super_mario_galaxy_pc_port.apworld` (fourni, ou à créer en zippant le dossier `apworld\super_mario_galaxy_pc_port` en
   `super_mario_galaxy_pc_port.zip` puis en renommant l'extension en `.apworld`) dans le dossier `custom_worlds` d'Archipelago.
3. Lancez le « Archipelago Launcher » → **Generate Template Options** : un dossier de modèles s'ouvre. Copiez
   `Super Mario Galaxy PC Port.yaml`, renommez le `name:` à l'intérieur (c'est votre nom de joueur, par exemple `Mario`), et
   mettez-le dans le dossier `Players` d'Archipelago.
4. Lancez **Generate** dans le Launcher : il crée un fichier `AP_xxxxx.zip` dans le dossier `output`.
5. Hébergez-le : soit via [archipelago.gg/uploads](https://archipelago.gg/uploads) (en ligne, le plus simple), soit avec
   « Host » dans le Launcher en local. Notez l'adresse du serveur, par exemple `archipelago.gg:38281`.

Options du YAML (toutes facultatives) :

| Option | Effet |
| --- | --- |
| `stars_to_finish` (10 à 100, défaut 50) | nombre d'étoiles pour ouvrir le Réacteur de Bowser |
| `power_star_percentage` (50 à 100, défaut 80) | part des lieux qui contiennent une Étoile de Puissance (le minimum nécessaire pour ouvrir toutes les galaxies est toujours ajouté) |
| `include_comet_stars` (défaut oui) | les étoiles des comètes sont des lieux |
| `include_purple_coin_stars` (défaut non) | les étoiles à pièces violettes (après la fin) et la Galaxie du Fantôme du cimetière |
| `include_luigi_stars` (défaut oui) | les étoiles de la cache-cache de Luigi : sans lettre, Luigi est dans la galaxie dès que vous choisissez cette étoile |
| `one_up_percentage` (0 à 100, défaut 50) | part de 1-Up parmi les bonus (le reste : 50 Star Bits) |
| `star_requirements` (`vanilla` ou `dome_offsets`, défaut `vanilla`) | `dome_offsets` : les étoiles demandées par chaque galaxie des dômes viennent des options `dome_*_counts` |
| `dome_one_counts` … `dome_six_counts` | pour chaque « orbite » (First à Fifth Orbit) : combien d'étoiles de plus que l'orbite précédente (0 à 8, 10 pour les dômes 2 à 5, 20 pour le dôme 6) ; la 1re orbite d'un dôme s'ajoute au plus grand nombre du dôme précédent ; le total est plafonné par `stars_to_finish`. Accepte `random` et `random-range-1-5` |
| `observatory_hungry_luma_behavior` (`vanilla`, `choice`, `progressive_values_low`, `progressive_values_high`) | `vanilla` : coûts du jeu ; `choice` : exactement `observatory_luma_counts` ; `progressive_*` : les coûts ne font que croître (jusqu'à 5000 ou 9999 Star Bits) |
| `shop_items`, `shop_min_price`, `shop_max_price` | la boutique de Star Bits de l'Observatoire (voir plus haut) : nombre d'étagères (1 à 20, 8 par défaut), prix de la moins chère (100 par défaut) et de la plus chère (2000 par défaut), les autres suivent en progression régulière |
| `transformation_items` (défaut non) | les 6 transformations deviennent des objets : sans l'objet, le bonus ramassé ne fait rien |
| `observatory_luma_counts` | Star Bits demandés par chaque Luma affamé (Dome 1 à 6 Hungry Luma, Gateway Hungry Luma = Galaxie du Fantôme du cimetière), 0 à 9999. Accepte `random` |

Les Star Bits des Lumas ne sont pas pris en compte dans la logique (on suppose qu'on en ramasse assez).

## 7. Dire à Nebula où se connecter

**Le plus simple** : lancez le script `tools\archipelago_setup.ps1` (dans PowerShell, depuis `C:\dev\nebula` :
`.\tools\archipelago_setup.ps1`). Il vous pose 3 questions (adresse, nom du joueur, mot de passe) et écrit le fichier
`%LOCALAPPDATA%\Nebula\archipelago.ini` avec le bon nom (si PowerShell refuse de lancer le script, tapez d'abord
`Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass`). Vous pouvez aussi le créer à la main avec le Bloc-notes :

```
server=archipelago.gg:38281
slot=Mario
password=
```

- `server` : l'adresse donnée par l'hébergement, par exemple `archipelago.gg:58396` (le port change à chaque salle : copiez-le depuis la page de la salle). Sans préfixe, Nebula essaie d'abord la connexion chiffrée (`wss://`, ce qu'exige archipelago.gg) puis non chiffrée (`ws://`, pour un serveur lancé sur votre PC). Vous pouvez aussi écrire le préfixe vous-même.
- `slot` : exactement le `name:` de votre YAML.
- `password` : vide si la salle n'en a pas.
- Pour jouer sans Archipelago : renommez ou supprimez ce fichier (ou ajoutez la ligne `enabled=0`).
- Il n'est pas encore possible de saisir l'adresse dans le jeu lui-même : la connexion se règle uniquement par ce fichier.

Attention : le fichier doit s'appeler **`archipelago.ini`** et non `archipelago.ini.txt` (le Bloc-notes ajoute
parfois `.txt` ; activez « Extensions de noms de fichiers » dans l'Explorateur pour le voir). Nebula accepte quand
même `.ini.txt`, mais l'idéal est le bon nom.

Lancez ensuite le jeu (étape 5) et **créez une nouvelle sauvegarde** : vous arrivez à l'Observatoire.

### Savoir si ça se connecte

- Un message **« Connected to Archipelago as <votre nom> »** s'affiche en haut de l'écran (même fenêtre que les
  succès). Ensuite, chaque objet trouvé ou reçu s'affiche (« Found … », « … from <joueur> »).
- Si rien ne s'affiche, ouvrez **`%LOCALAPPDATA%\Nebula\archipelago.log`** : ce fichier dit si `archipelago.ini` a été
  trouvé, où Nebula l'a cherché, à quelle adresse il se connecte et pourquoi la connexion échoue (mauvais nom de slot,
  mauvais mot de passe, mauvaise adresse, jeu différent…). Envoyez-le moi en cas de doute.
- Les objets (1-Up, Star Bits) arrivent toutes les quelques secondes de jeu, quand Mario peut bouger librement ; les
  étoiles comptent tout de suite.

## Le client et le tracker

Dans le **Launcher d'Archipelago**, l'APWorld ajoute un bouton **« Super Mario Galaxy PC Port Client »**. Le jeu
parle au serveur tout seul (via Nebula et `archipelago.ini`) ; ce client ne fait que **suivre la même partie**, à
côté du jeu. Connectez-le avec la même adresse et le même nom de joueur (`/connect adresse:port` dans sa fenêtre, ou
les champs en haut). Commandes :

- `/tracker` : le compteur d'étoiles (Puissance + Vertes + Grandes), les Grandes Étoiles, et chaque galaxie avec ses
  étoiles faites et si le nombre d'étoiles demandé est atteint ;
- `/stars good egg` : les étoiles des galaxies dont le nom contient ce texte (`[x]` = faite) ;
- `/places` : quelle galaxie montre chaque miniature, quand les galaxies sont mélangées.

Si le **Universal Tracker** est installé (module séparé d'Archipelago), le client utilise aussi sa fenêtre : la
logique est reconstruite à partir des données de la partie (mélanges, ordre des dômes, étoiles demandées).
Je n'ai pas pu ouvrir la fenêtre graphique ici (pas d'écran) : les commandes et la reconstruction de la logique sont
vérifiées en test, l'affichage dans le Launcher ne l'est pas.

## Options du mélange

À mettre dans votre YAML, comme les autres options :

```yaml
  galaxy_shuffle:          # liste ; vide = pas de mélange
    - Dome Majors          # les 15 grandes galaxies (Good Egg, Honeyhive, Space Junk...)
    - Dome Minors          # les 9 petites (Loopdeeloop, Flipswitch, Rolling Green...)
    - Bosses               # les 5 galaxies de Bowser / Bowser Jr. (échangées entre elles seulement)
    - Observatory Specials # les 10 galaxies atteintes depuis l'Observatoire même (Sweet Sweet, Sling Pod, Drip Drop,
                           # Bigmouth, Sand Spiral, Boo's Boneyard, Snow Cap, Rolling Gizmo, Loopdeeswoop, Bubble Blast)
    # - Full               # les quatre à la fois
  galaxy_shuffle_type: by_type   # by_type | major_separate | full
  dome_shuffle: true
```

- `by_type` : les grandes entre elles, les petites entre elles, les boss entre eux.
- `major_separate` : les grandes entre elles, les autres entre elles (les boss restent entre boss).
- `full` : tout dans un seul tas (un dôme peut avoir aucun boss ou trois ; les galaxies de l'Observatoire peuvent
  être dans les dômes, la place gardant son propre modèle).
- Avec `dome_shuffle`, les étoiles demandées par une salle suivent **l'ordre d'ouverture** des salles ; le jeu utilise
  alors exactement les nombres de l'APWorld.
- Le générateur écarte les mélanges où une galaxie demande une étoile cachée derrière elle-même, ou qui laissent trop
  peu de place dans les premières galaxies (sinon le remplissage de l'APWorld échoue).

## Limites connues

- **Jamais compilé sous Windows, jamais testé en jeu** (voir en haut).
- **Connexion chiffrée (wss://)** : ajoutée avec le chiffrement intégré à Windows, mais pas encore vérifiée avec archipelago.gg. Si ça échoue, `archipelago.log` donne la raison ; en repli, lancez le serveur sur votre PC (Host) avec `server=localhost:38281`.
- **Départ à l'Observatoire** : les cinématiques d'introduction de l'histoire sont sautées, et certaines scènes du
  type « vous venez d'obtenir une Grande Étoile » peuvent ne pas se jouer, puisque les étoiles viennent des objets.
- **Étoiles des comètes** : elles se choisissent directement dans le choix des étoiles (plus de comètes aléatoires). Les étoiles à pièces
  violettes ne sont accessibles qu'après la fin du jeu.
- **Cache-cache de Luigi** : les étoiles de Luigi se choisissent comme les autres ; Luigi apparaît dans la galaxie dès
  que vous choisissez son étoile. Le jeu peut quand même lui faire quitter l'Observatoire et envoyer sa lettre : ça ne
  change rien à ces étoiles.
- **Mélange des galaxies/dômes** : jamais essayé en jeu. Points à surveiller : la carte des galaxies du menu pause garde l'ancienne
  disposition ; le mélange des dômes change la salle derrière chaque porte, mais pas les cinématiques ni les noms écrits sur les portes.
  Si quelque chose ne va pas, envoyez `archipelago.log` (il note « galaxy shuffle: … » et « dome shuffle: … »).
- **Couleurs de Mario/Luigi, `hide_galaxy`, `star_colors` : pas faits.**
- Les objets 1-Up / Star Bits ne sont donnés que dans la partie jouable (pas pendant une cinématique, un menu ou une
  mort).
- Un fichier de sauvegarde n'est pas lié à un multiworld : utilisez une sauvegarde neuve pour chaque partie, sinon les
  étoiles déjà obtenues sont envoyées d'un coup.
- Seuls le build normal (`NebulaNative`), le build Vulkan et l'hôte SDL embarquent le client ; l'hôte « headless » ne le
  fait pas.

## En cas de problème

- Le jeu ne démarre pas / message de version : `NebulaNative.exe` et `smg_game.dll` ne viennent pas de la même
  compilation.
- Aucun message à l'écran : lisez `%LOCALAPPDATA%\Nebula\archipelago.log` (voir plus haut). `archipelago.ini` n'est
  pas au bon endroit, s'appelle `archipelago.ini.txt` ou n'a pas de ligne `server` ou `slot`.
- « no slot called … » / « password » : `slot` ou `password` ne correspondent pas à la salle ; « game » : le YAML n'a
  pas été généré avec « Super Mario Galaxy PC Port ».
- Les étoiles ne comptent pas : vérifiez que le message « Connected » s'affiche avant de jouer, et envoyez-moi le
  fichier `archipelago.log`.
- `No CMAKE_C_COMPILER could be found` ou « l'exécution de scripts est désactivée » : refaites les deux lignes du début
  de l'étape 3 dans cette fenêtre, supprimez le dossier `build\host` puis relancez.
- Une étape de compilation échoue : copiez les 20 dernières lignes de l'erreur.
