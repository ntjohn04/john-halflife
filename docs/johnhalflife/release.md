# Release

The install targets, in order:

1. **Community zip:** download, extract into the Half-Life folder, start John Half-Life (as a Steam custom game).
   Nothing else to install or run.
2. **Steam release:** the same game as a Steam product, someday. It needs Valve's approval, so it isn't planned
   around yet, but nothing is built in a way that rules it out.

What a player's folder needs comes from three places: files we ship, files taken from the player's own games, and
files made from those on the player's computer. Nothing of Valve's is shipped that the player's games already
have; what's made from Valve's files is made on the player's computer, when the game first starts.

## What's in the folder now

Measured on the development install (2026-11-01), every file sorted by where it came from (the Unified SDK's
release zip `HalfLifeUnified-V1.0.0.zip`, its ContentInstaller, our build):

| Kind | Files | Size | Release |
|---|---|---|---|
| johnhalflife's (game code, settings, menu, expansion folders) | 16 + expansions | 9 MB | Ship |
| The Unified SDK's own: `cfg/` (248 JSON), campaigns, scripts, stats, titles, sentences and materials JSON, HUD sprites and JSON (66), `class_signs.wad`, `ravelin.wad` | ~330 | 2 MB | Ship |
| The Unified SDK's models, recompiled with sequences merged from the three games | 128 | 43 MB | Ship in the zip, as the Unified SDK does; see [Steam](#steam) |
| Byte-for-byte copies of Half-Life's files (sounds, models, sprites, gfx, events) | ~1,760 | 46 MB | Drop: the engine finds them in `valve` |
| Byte-for-byte copies of Opposing Force's and Blue Shift's (sounds, models, sprites, gfx, WADs, classes) | ~1,320 | 92 MB | Copy from the player's `gearbox` and `bshift` at first start (`jhl_setup_files.txt`) |
| ... of which the models and sprites the game code names (every map precaches every weapon, Opposing Force's too, and johnhalflife's effects use some of its sprites) | 80 | 7.6 MB | Also ship: a missing model is a fatal error, so without them Half-Life wouldn't start a map for a player without Opposing Force |
| Half-Life's, Opposing Force's and Blue Shift's maps, converted (the Unified SDK's map upgrades) | 220 | 379 MB | Convert at first start |
| Node graphs for them | 338 | 18 MB | Made by the engine as each map first loads |
| Opposing Force's and Blue Shift's soundtracks (`media/`) | 38 | 66 MB | Copy at first start |
| Made by our build from the player's games: Half-Life's shell, hornet splats in `decals.wad`, each game's own shared-name files, vanilla NPC speeds, the menu art | ~130 | | Make at first start |
| Uplink's maps (`hldemo1` to `3`), converted | 3 | | Open: Valve's maps, not in a player's install; see below |
| The Unified SDK's test maps (`test_*`, `zoo_*`, `hlu_technology_demonstration`) and the TFC WADs only `zoo_npcs` uses | 45 | 32 MB | Drop |
| The Unified SDK's tools, installer files, FGD, `VC_redist.x86.exe` | ~100 | 20 MB | Drop (link the C++ runtime statically, as Valve's DLLs need nothing installed) |
| MMod's files | 29 | 0.4 MB | Never shipped; each must be replaced or cleared first ([MMod assets](../../README.md#mmod-assets)) |
| The player's (saves, config, logs, spray) | | | Never shipped |

So the zip is about 55 MB: our files, the Unified SDK's own and its models.

## First start

The client DLL loads before the main menu. On the first start, and after an update changes what it makes, it sets
the folder up and writes a version stamp, so later starts skip it:

1. Finds the player's games through the engine (`valve`, and `gearbox` and `bshift` if installed) rather than
   assuming they're next to the mod.
2. Converts each game's maps into the mod folder (`client/jhl_mapconvert.cpp`, the maps in `jhl_setup_maps.txt`):
   the ContentInstaller's map upgrades (version 1.0.0) ported to C++, the entities rewritten and everything else in
   the BSP left as Valve's, Blue Shift's first two lumps put in Half-Life's order. `mapconvert_check` (a build target,
   `scripts/mapconvert_check.cpp`) checks it against the ContentInstaller's maps: 215 of 220 the same, byte for
   byte; the 5 others keep a few bytes the ContentInstaller's BSP reader lost (see the source).
3. Copies Opposing Force's and Blue Shift's files and soundtracks.
4. Copies Half-Life's menu art (its main menu without the hint text, background, logo, icon) into each folder,
   then the folder's own Valve game's over it.

What's made from files we ship is made by the build and shipped as made: the hornet splats added to the Unified
SDK's `decals.wad` (`scripts/make_hornet_decals.py`) and the vanilla speeds in its models
(`scripts/restore_npc_speeds.py`).

It runs before the game's window has anything on it: a black screen for a few seconds (7 on an SSD), once; its
messages go to the console when it's up. A window saying what it's doing was tried and left out: it could only be a
plain Windows one, on one platform, nothing like the game's look. The install notes say the first start takes a
moment.

A game the player doesn't own is skipped, and its campaign isn't offered. The C++ conversion is checked against
the ContentInstaller's: the same maps, byte for byte. Each upgrade is reviewed against 1998 as it's ported (keep
the bug fixes, revert the drift).

## Making a release

One zip for Windows and Linux, made by hand when there's one to make (no automated builds):

1. Windows: build, then install to a staging folder (not the game):
   `cmake --install <build folder> --config Release --prefix <staging>/windows/johnhalflife`
2. Linux (Ubuntu 22.04, e.g. WSL): `scripts/linux/build.sh` (installs to `~/jhl-build-linux/install`).
3. `python scripts/package_release.py --windows <staging>/windows --linux <Linux staging> --unified
   HalfLifeUnified-V1.0.0.zip` (from Windows, WSL's Linux staging is
   `\\wsl.localhost\Ubuntu-22.04\home\<user>\jhl-build-linux\install`). It writes
   `dist/johnhalflife-v<version>-<release type>.zip`.
4. Version, changelog row and tag as for any version ([Versioning](README.md#versioning)); upload the zip to the
   GitHub release.

The packager needs no Half-Life install: the Unified SDK's files it ships are listed in
`scripts/release_unified_files.txt` and the vanilla NPC speeds in `scripts/npc_speeds.txt` (names and numbers only,
made once on a machine with the games: `package_release.py --make-list`, `restore_npc_speeds.py --make-table`; again
if the Unified SDK's release changes).

v0.12.0's zip: 87 MB (183 MB extracted, 649 files). Most of it is the game code, a copy in each of the four game
folders, Windows' and Linux's; then the Unified SDK's models (50 MB).

## Linux build

The release has the Linux game code next to the Windows one (`client.so`, `server.so` and `libopenal-hlu.so.1` beside
the `.dll`s; `liblist.gam` names both). It's built in Ubuntu 22.04, so it needs glibc 2.34 at most and runs on 22.04
and newer; the C++ library is linked in. `scripts/linux/build.sh` builds it and installs it to a staging folder
(`~/jhl-build-linux/install`; the install sets the client's RUNPATH to `$ORIGIN`, so it finds OpenAL beside it).

Once, in Ubuntu 22.04 (WSL works: `wsl --install -d Ubuntu-22.04`):

```
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install -y build-essential g++-11-multilib libgl1-mesa-dev:i386 cmake ninja-build git zip unzip curl     pkg-config autoconf automake autoconf-archive libtool m4 bison flex gettext python3
```

and CMake 3.23 or newer (22.04 has 3.22): Kitware's release unpacked in `~/tools` is what the script uses.

* `vgui.so` (Valve's, next to `hl_linux`) has no SONAME: linked by its path, the client recorded that path and couldn't
  have found it; it's linked as `-l:vgui.so` (`src/game/client/CMakeLists.txt`).
* Every game folder's client needs `libopenal-hlu.so.1`; the install linked it only in the base folder.

## Steam

* The Unified SDK's models are Valve's, recompiled with Opposing Force's and Blue Shift's sequences merged in. A
  Steam release can't give those to players who don't own the expansions; it would need them made on the
  player's computer (from their games, or as patches on them) or Valve's agreement.
* Uplink was a free demo, but its maps are still Valve's.
* A Steam release may install outside the Half-Life folder, which is why the first start finds the games through
  the engine.
