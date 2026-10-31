# Nebula Archipelago

**Super Mario Galaxy multiworld for [Archipelago](https://archipelago.gg), on PC.**
Every star is a check that sends an item to the multiworld, and the stars you play with are the ones the other players
send you.

[Download](https://github.com/squidkud/NebulaArchipelago/releases/latest) ·
[Install guide](docs/INSTALL_ARCHIPELAGO.md)

> ### Made entirely by AI
> **This whole mod was written by an AI: Claude, by Anthropic, working from start to finish with the person who runs
> this repository.** The APWorld, the game hooks, the Archipelago client, the launcher, the installer, the release
> pipeline and these docs were all written by Claude. The project owner is not a programmer: they described what they
> wanted, played the builds and reported what broke, and Claude did all the code.
> Because of that, treat it as a hobby project: **much of it has not been tested in depth in the game** (the shuffles
> most of all), and bugs are to be expected. Please report them in the
> [issues](https://github.com/squidkud/NebulaArchipelago/issues).

> ### It comes from Nebula
> Nebula Archipelago is a fork of **[Nebula](https://github.com/keepvaibin/nebula)** by keepvaibin, a native PC port
> of Super Mario Galaxy built on the [Petari](https://github.com/SMGCommunity/Petari) decompilation. Everything that
> makes the game run on PC (the renderer, audio, input, the game code itself) is Nebula's and Petari's work, not
> ours. This repository only adds Archipelago on top. If you just want to play the game on PC without Archipelago,
> go to [Nebula](https://github.com/keepvaibin/nebula). Not affiliated with or endorsed by Nintendo; the project
> contains no Nintendo code or assets, and you need your own copy of the game.

## What you get

**The basics**
- Each star of the game is a location ("Good Egg Galaxy - Dino Piranha (Mario)"), and it sends an item to the multiworld.
  The star counter, the galaxies' requirements and the Grand Star events read the items you have received.
- You start in the Comet Observatory as soon as you pick a save file. Grand Stars open the domes.
- Choose who plays with `character`: Mario, Luigi or both (the game forces that character at the file select).
- Item pop-ups show the item's picture and where it was found. The star's colour tells what it hides.

**Goals**
- `bowser`: take the star of the final boss in Bowser's Galaxy Reactor.
- `bowser_and_120_stars`: the final boss plus every star that exists before the ending.
- `242_stars`: every star of Mario and Luigi, up to *Grand Finale Galaxy - The Star Festival* (the 121st star).

**Items and traps**
- Power Stars, Green Stars and Grand Stars, 1-Up Mushrooms, Star Bits, packs of Star Bits, Full Heals and Rainbow Stars.
- Seven traps (Fire, Shock, Freeze, Spike, Star Bit Thief, Quake, Confusion) and DeathLink.
- Optional: the six transformations (Bee, Boo, Spring, Fire, Ice, Flying Star) become items, and the logic knows which
  star needs which form.

**The Observatory shop**
- The Luma in the Observatory opens a Star Bit shop: each shelf is a location, shown with the item's picture and whose
  item it is.

**Shuffles**
- Galaxies on the Observatory's miniatures, the domes themselves, music, skies and enemies (the last two are
  experimental).

**Tools**
- A client for the Archipelago launcher (`/tracker`, `/stars`, `/places`), Universal Tracker support, and a
  [PopTracker](https://poptracker.github.io/) pack with automatic tracking.
- A launcher with **Archipelago**, **Updates** and **Advanced** tabs; it updates itself from GitHub.
- A complete YAML template, with the options grouped in sections.

## Install and play

1. Download `NebulaArchipelago-Setup.exe` from the [latest release](https://github.com/squidkud/NebulaArchipelago/releases/latest)
   and run it (the installer is not code-signed, so Windows may show "Unknown publisher": choose **More info**, then
   **Run anyway**).
2. Start **Nebula Archipelago**, open the **Archipelago** tab, choose your own Super Mario Galaxy image (North America,
   RMGE01, revision 0), then type the server address, your player name and the room password.
3. Install `super_mario_galaxy_pc_port.apworld` (from the same release) in Archipelago, fill in the YAML template
   attached to the release and generate the multiworld.
4. Press **Save and play**. Use a new save file for each multiworld.

Details, tabs and troubleshooting: [docs/INSTALL_ARCHIPELAGO.md](docs/INSTALL_ARCHIPELAGO.md).

## Known limits

- Mostly untested in the game (see the note at the top).
- Mario's moves (Spin, Wall Jump...) are not part of the logic yet.
- Use the matching APWorld version for your game version; a multiworld already generated keeps the APWorld it was
  generated with.

## For developers

Building from source: [docs/BUILDING.md](docs/BUILDING.md). How Nebula works inside: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
The Archipelago code is in `apworld/` (the APWorld), `runtime/` and `native/petari/src/Game/System/NebulaArchipelago.cpp`
(the game side) and `installer/` (launcher and installer).

## License and credits

GNU General Public License v3.0 only ([LICENSE](LICENSE)), the same as Nebula. Third-party code is listed in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) and [docs/PROVENANCE.md](docs/PROVENANCE.md).

Built on [Nebula](https://github.com/keepvaibin/nebula) (keepvaibin), the [Petari](https://github.com/SMGCommunity/Petari)
decompilation (SMGCommunity) and [Archipelago](https://archipelago.gg). Written by Claude (Anthropic).
"Super Mario Galaxy", "Wii" and "Nintendo" are trademarks of Nintendo.
