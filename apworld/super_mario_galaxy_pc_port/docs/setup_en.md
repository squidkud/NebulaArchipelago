# Super Mario Galaxy PC Port Multiworld Setup Guide

## What you need

- Super Mario Galaxy (USA, RMGE01) installed with Nebula, built with Archipelago support (see `docs/ARCHIPELAGO.md`
  in the Nebula repository).
- [Archipelago](https://github.com/ArchipelagoMW/Archipelago/releases), to generate and host the multiworld.

## Installing the APWorld

Put `super_mario_galaxy_pc_port.apworld` in the `custom_worlds` folder of your Archipelago install (or double-click it).

## Making your config file

Open the Archipelago Launcher, choose "Generate Template Options", edit `Super Mario Galaxy PC Port.yaml` (the name under
`name:` is your slot name), and give it to whoever generates the multiworld.

## Connecting the game

Create `archipelago.ini` (not `archipelago.ini.txt`) in Nebula's settings folder (`%LOCALAPPDATA%\Nebula`) with:

```
server=archipelago.gg:38281
slot=YourSlotName
password=
```

Start Nebula and a new save file. A message "Connected to Archipelago as ..." appears on screen when it works; the file
`archipelago.log` in the same folder says what happened if it does not. Delete or rename `archipelago.ini` to play
without it.
