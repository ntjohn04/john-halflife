# Release

The release is a zip: extract into the Half-Life folder, restart Steam. Nothing of Valve's the player already has is
shipped; what's made from Valve's files is made at the first start.

## What's shipped

* Ours: game code, settings, menus, expansion folders, our models and sprites.
* The Unified SDK's: configs, HUD, its recompiled models, node graphs.
* The models and sprites the game code needs from Opposing Force.
* Uplink's maps.

## First start

Once, before the main menu (a few seconds of black screen). Code: `client/jhl_setup.cpp`.

1. Finds the player's games through the engine.
2. Converts their maps (`client/jhl_mapconvert.cpp`): the ContentInstaller's map upgrades in C++.
   `mapconvert_check` compares against it: 215 of 220 identical.
3. Keeps the node graphs newer than the maps (every start), so maps don't rebuild them.
4. Copies Opposing Force's and Blue Shift's files and soundtracks (`config/mod/jhl_setup_files.txt`).
5. Copies Half-Life's menu art, then each game's own.
6. Writes default key bindings for a new folder.

Games the player doesn't own are skipped.

## Making a release

1. Windows: build `INSTALL` (to `_stage_release/windows/johnhalflife`, never the game).
2. Linux: `scripts/linux/build.sh` in Ubuntu 22.04 (or WSL), to `~/jhl-build-linux/install`.
3. Package:
   ```
   python scripts/package_release.py --windows <staging>/windows --linux <linux staging> --unified HalfLifeUnified-V1.0.0.zip
   ```
   Writes `dist/johnhalflife-v<version>-<type>.zip`.
4. Licences: `config/licenses/` lists every shipped library; update it when one changes.
5. Version, changelog line, tag ([versioning](README.md#versioning)).

* **Testing:** `--install <Half-Life folder>` extracts the zip into the game. Without `--linux`: a Windows-only test
  zip.
* The packager needs no Half-Life install: `scripts/release_unified_files.txt` and `scripts/npc_speeds.txt` list
  what it needs. Regenerate with `--make-list` and `restore_npc_speeds.py --make-table` if the Unified SDK changes.

## Linux build

* Built in Ubuntu 22.04, runs on 22.04 and newer. C++ library linked in.
* `client.so`, `server.so` and `libopenal-hlu.so.1` sit beside the `.dll`s.
* Setup, once:
  ```
  sudo dpkg --add-architecture i386 && sudo apt update
  sudo apt install -y build-essential g++-11-multilib libgl1-mesa-dev:i386 ninja-build git zip unzip curl pkg-config autoconf automake autoconf-archive libtool m4 bison flex gettext python3
  ```
  Plus CMake 3.23+ (Kitware's release in `~/tools`).
* `vgui.so` has no SONAME: linked as `-l:vgui.so`.

## Steam notes

* The Unified SDK's models merge Opposing Force and Blue Shift sequences, so a Steam release couldn't ship them to
  players without the expansions.
* Uplink's maps are still Valve's.
* A Steam install may sit outside the Half-Life folder; the first start already finds the games through the engine.
