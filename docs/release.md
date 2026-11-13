# Release

## Targets

1. **Zip:** extract into the Half-Life folder, restart Steam. Nothing else to install.
2. **Steam:** someday, with Valve's approval. Nothing rules it out.

Nothing of Valve's is shipped that the player already has. What's made from Valve's files is made on the player's
computer at the first start.

## What's shipped

* Ours: game code, settings, menus, expansion folders, our models and sprites.
* The Unified SDK's: configs, HUD, its recompiled models, node graphs.
* The models and sprites the game code needs from Opposing Force (a missing one is fatal).
* Uplink's maps (a free demo, not in the player's install).
* Never: Valve copies the engine finds anyway, the Unified SDK's tools, test maps and redistributable.

## First start

The client sets the folder up before the main menu, once (a few seconds of black screen). Code: `client/jhl_setup.cpp`.

1. Finds the player's games through the engine.
2. Converts their maps (`client/jhl_mapconvert.cpp`): the ContentInstaller's map upgrades ported to C++.
   `mapconvert_check` compares against the ContentInstaller: 215 of 220 identical.
3. Keeps the node graphs newer than the maps (every start), so maps don't rebuild them.
4. Copies Opposing Force's and Blue Shift's files and soundtracks (`config/mod/jhl_setup_files.txt`).
5. Copies Half-Life's menu art, then each game's own.
6. Writes default key bindings for a new folder.

A game the player doesn't own is skipped.

## Making a release

1. Windows: build `INSTALL` (installs to `_stage_release/windows/johnhalflife`, never the game).
2. Linux: `scripts/linux/build.sh` in Ubuntu 22.04 (or WSL). Installs to `~/jhl-build-linux/install`.
3. Package:
   ```
   python scripts/package_release.py --windows <staging>/windows --linux <linux staging> --unified HalfLifeUnified-V1.0.0.zip
   ```
   Writes `dist/johnhalflife-v<version>-<type>.zip`.
4. Licences: `config/licenses/` must list every shipped library. Update it when one changes.
5. Version, changelog line, tag ([versioning](README.md#versioning)).

**Testing:** add `--install <Half-Life folder>` to extract the zip into the game. Without `--linux` it makes a
Windows-only test zip.

The packager needs no Half-Life install: `scripts/release_unified_files.txt` and `scripts/npc_speeds.txt` list what
it needs (regenerate with `--make-list` and `restore_npc_speeds.py --make-table` if the Unified SDK changes).

## Linux build

* Built in Ubuntu 22.04: runs on 22.04 and newer. C++ library linked in.
* `client.so`, `server.so` and `libopenal-hlu.so.1` sit beside the `.dll`s.
* Setup, once:
  ```
  sudo dpkg --add-architecture i386 && sudo apt update
  sudo apt install -y build-essential g++-11-multilib libgl1-mesa-dev:i386 ninja-build git zip unzip curl pkg-config autoconf automake autoconf-archive libtool m4 bison flex gettext python3
  ```
  Plus CMake 3.23+ (Kitware's release in `~/tools`).
* `vgui.so` has no SONAME: linked as `-l:vgui.so`.

## Steam notes

* The Unified SDK's models merge Opposing Force and Blue Shift sequences: a Steam release couldn't ship them to
  players without the expansions.
* Uplink's maps are still Valve's.
* A Steam release may install outside the Half-Life folder; the first start already finds the games through the engine.
