# Oblivion Remastered Engine Fixes

An [OBSE64](https://www.nexusmods.com/oblivionremastered/mods/282) plugin for The Elder Scrolls IV:
Oblivion Remastered that fixes bugs in the game's engine that mods run into: four crashes, and
four smaller problems that make some mods misbehave. It adds nothing to your saves, and works with
any mods, or none.

## What it fixes

- **Picking up an item a mod placed in the world crashed the game**. Remastered gives objects 
  placed by mods no physics, and the game's pick-up code assumed they had some.
- **Loading a save could crash after you removed a mod that casts spells.** If one of the mod's
  spells was in flight when you saved, loading that save without the mod crashed the game.
  Such a spell is now dropped as the save loads.
- **An item that removes itself in its own script crashed the game.** Many older mods give
  characters a hidden item, a "token", that does its work in a script and then removes itself;
  Crime has witnesses crashed this way when a witness died. The removal now waits until the script
  has finished.
  characters a hidden item, a "token", that does its work in a script and then removes itself. 
  The removal now waits until the script has finished.
- **"[NL]" in front of names and messages from mods.** Text the Remastered has no translation for,
  such as the names of places and items a mod adds or the messages older mods show, appeared with
  "[NL]" in front. It now shows as written.
- **Spells a mod removed stayed listed in the open magic menu** until it was opened again. The list
  now updates on changes.
- **Stealing could add a fraction of a gold to your bounty.** Bounty changes are now rounded to
  whole gold, which mods that count bounty in whole gold expect. The game shows bounty in whole gold anyway.

## Requirements

- Oblivion Remastered on Steam, game version 1.512.105. Like OBSE64, it doesn't support the
  Microsoft Store or Game Pass version.
- [OBSE64](https://www.nexusmods.com/oblivionremastered/mods/282) for that game version.

## Installing

With a mod manager, install the zip as you would any OBSE64 plugin.

By hand, extract the zip into the game's `OblivionRemastered\Binaries\Win64` folder, so that the
plugin ends up at:

```
...\steamapps\common\Oblivion Remastered\OblivionRemastered\Binaries\Win64\OBSE\Plugins\RemasteredEngineFixes.dll
```

Then start the game with OBSE64, as usual. To check that it's working, open
`Documents\My Games\Oblivion Remastered\OBSE\Logs\RemasteredEngineFixes.log`: it lists each fix as
it's applied.

To uninstall, delete `RemasteredEngineFixes.dll`. Your saves don't depend on it.

## Game updates

The plugin changes the game's code at fixed places, which move when the game is updated, so it only
works with the version it was made for: OBSE64 won't load it with any other. Each fix also checks
the game's code before changing it, and leaves it alone, saying so in the log, if it isn't what it
expects.

## Building

You need Visual Studio 2022 or later with the "Desktop development with C++" workload, and a copy of
[OBSE64's source](https://github.com/ianpatt/obse64), for its plugin header. From this folder:

```
cmake -S . -B build -A x64 -DOBSE64_DIR=<path to OBSE64's source>
cmake --build build --config Release
cmake --build build --config Release --target release_zip
```

The plugin is built as `build\Release\RemasteredEngineFixes.dll`, and the release zip, with the
plugin in `OBSE\Plugins`, as `build\RemasteredEngineFixes-<version>.zip`.

Each fix is in its own file in `src`, which starts by explaining the bug and how it's fixed.

## License

MIT; see [LICENSE](LICENSE). Thanks to Ian Patterson for OBSE64.
