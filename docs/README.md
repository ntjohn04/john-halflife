# John Half-Life docs

What John Half-Life changes and how. Settings and install: [front page](../README.md).
The Unified SDK's own docs: [unified-sdk/](unified-sdk/README.md).

* [Camera and movement](camera_movement.md)
* [Weapons](weapons.md)
* [Effects](effects.md)
* [Visuals](visuals.md)
* [Physics](physics.md)
* [Sound](sound.md)
* [Models](models.md)
* [Release](release.md)
* [Changelog](CHANGELOG.md)

## Debug commands

None changes anything in play.

* `jhl_debug_hitch <ms>`: reports every frame longer than that, to the console and `jhl_hitches.txt`.
* `jhl_debug_muzzle`: diagnoses any problems with muzzle effects (unconfirmed issue).
* `jhl_debug_blocked`: what blocks lifts, doors and trains.
* `jhl_debug_npcdraw`: NPC heights and animation, for telling real bounces from drawn ones.
* `jhl_debug_damage`: every hit an NPC takes.
* `jhl_debug_precache [list]`: model slots used on this map.
* `jhl_debug_waterlight`: the water light around the view.
* `jhl_texture [material]`: the texture under the crosshair and its impact material; logs to `jhl_textures.txt`.
* `jhl_fx_test <effect> [key=value ...]`: plays an effect where you aim.

## John Settings

* Options → Multiplayer → **John Settings...**, from `config/mod/user.scr`.
* Menu files in `config/mod/resource/`: copies of the 25th anniversary layouts.
* Never use `LIST` entries unless the values are 0, 1, 2... in order: GameUI stores the index.
* `jhl_gore` stands in for the content lock's four `violence_*` settings.
* Recommended settings (`jhl_settings.cpp`): the dialog's defaults, applied on first launch and by
  `jhl_recommended`. Raise `RecommendedSettingsVersion` to apply a changed list once more.
* Engine options stored in the registry (renderer, HD models, addons) can't be John Settings.

## Game folders

* `johnhalflife` is the base. The expansion folders are thin: `liblist.gam` with `fallback_dir "johnhalflife"`,
  their menu, and the game code.
* Sources: `config/mod/` and `config/expansions/{op4,bs,uplink}/`.
* Each has its own name, icon (`scripts/make_icons.py`), WON-style menu text (`scripts/make_menu_strings.py`),
  intro video, background and animated title.
* Each game's own skill values load after `skill.json`: `cfg/skill_blueshift.json`, `cfg/skill_opposingforce.json`.
* The Unified SDK reads configs from the mod folder only; John Half-Life also tries `GAME_FALLBACK`, so the
  expansions find the base folder's.

## Networking

New user messages (names must be 11 characters or fewer, or sending them is fatal):

| Message | Purpose |
|---|---|
| `SmoothPunch` | Jump and landing jolt |
| `BulletHit` | NPC and mounted gun bullet impacts |
| `WaterSplash` | NPC bullets entering water |
| `BloodPool` | Blood pool under a body |
| `JhlMusic` | Music from a save |

* Weapon spread is predicted through `weapon_data_t.fuser4` (in `delta.lst`).
* Many effects go through the Unified SDK's `TempEntity` message with John Half-Life's own contents, parsed in
  `client/entity.cpp` (`MsgFunc_TempEntity`).

## Versioning

* Version: `project(... VERSION ...)` in `CMakeLists.txt`. Release type: `HalfLife_RELEASE_TYPE`.
* Features raise the minor version, fixes the patch. Each version gets a tag (`v<version>-<type>`) and a changelog
  line.
* `cl_projectinfo_show 1` shows the build info.

## Model slots

* The engine allows 512 models a map, brush entities included.
* John Half-Life's effect sprites and models are precached last in `W_Precache`, only while there's room.
* The client asks for them with `JhlEffectModel` (0 if not precached). Effects then draw their vanilla part only.
