# johnhalflife

An MMod-inspired mod for Half-Life, Opposing Force and Blue Shift, built on the Half-Life Unified SDK.
These pages record what johnhalflife changes compared with the stock SDK, and why.

* [Camera and movement](camera_movement.md)
* [Weapons](weapons.md)
* [Effects plan](effects_plan.md): the map of visual effects to work through next
* [Effects test command](effects_test.md): `jhl_fx_test`, for designing particle effects

## Principles

* **Written from specs, not from MMod's code.** MMod's binaries were reverse engineered into behaviour
  specs (`re/specs/` in the workspace), and the code here was written from those specs.
* **Reserved.** Each feature is a toned-down take on MMod's version. Vanilla visuals and Valve's models stay.
* **Never worse than vanilla.** Sustained play (accuracy, recoil, readability) is never worse than the
  original game.
* **Vanilla assets first.** MMod files are used only where nothing stock fits. They are never committed;
  see [MMod assets](#mmod-assets).
* **Every feature can be switched off.** Each has a `jhl_*` client setting (default 1, saved in
  `config.cfg`), so it can be compared with vanilla in game.

## Settings

All are client settings; set them in the console.

| Setting | Default | Turns on/off | Page |
|---|---|---|---|
| `jhl_camera_bob` | 1 | Dual-rate view bob and figure-eight weapon sway | [Camera](camera_movement.md#view-bob-and-weapon-sway) |
| `jhl_camera_viewroll` | 1 | Stronger strafe roll | [Camera](camera_movement.md#strafe-roll) |
| `jhl_camera_verticallag` | 1 | Eye lift while moving upward | [Camera](camera_movement.md#vertical-lag) |
| `jhl_camera_jumplandjolt` | 1 | Jump and landing view jolts | [Camera](camera_movement.md#jump-and-landing-jolts) |
| `jhl_visuals_weaponlag` | 1 | Weapon lags behind when turning | [Camera](camera_movement.md#weapon-lag-and-wall-push) |
| `jhl_visuals_weaponwallpush` | 1 | Weapon pulls back near walls | [Camera](camera_movement.md#weapon-lag-and-wall-push) |
| `jhl_weapons_smoothrecoil` | 1 | Spring-damped recoil (0 = original punches) | [Weapons](weapons.md#recoil) |
| `jhl_fx_bulletimpacts` | 1 | Bullet impact sparks/dust and water splashes | [Weapons](weapons.md#bullet-impacts) |
| `jhl_fx_muzzleflash` | 1 | Muzzle light and smoke, RPG backblast | [Weapons](weapons.md#muzzle-light-and-smoke) |
| `jhl_fx_particlesystem` | 1 | The particle system changes (gibs so far) | [Effects plan](effects_plan.md#gibs-done) |
| `jhl_hud_scale` | 1 | Sprite HUD scale (0 = automatic by screen height) | [HUD](#hud) |

Changed defaults of existing settings: `cl_rollangle` 0.65 and `cl_rollspeed` 300 (see
[strafe roll](camera_movement.md#strafe-roll)).

### Settings menu
Options → Multiplayer → **John Settings...** lists the settings above plus a few vanilla ones (crosshair
scale, field of view, frame rate limit, vertical sync, raw mouse input). It's driven by `config/mod/user.scr`,
installed with the build together with the menu files in `config/mod/resource/`:

* `gameui_english.txt`: the Unified SDK's copy (it has every 25th anniversary string), with the dialog titled
  "John Settings", the Lock tab titled "Content", "Enable original models (recommended)" and the software
  renderer "(not recommended)".
* `OptionsSubMultiplayer.res`: the 25th anniversary layout, with the Advanced... button renamed.
* `OptionsSubAdvanced.res`: the 25th anniversary layout of the Lock tab, which now also holds Valve's addon
  content, original models and detail texture options; the content lock is hidden and its label points to the
  settings.
* These options aren't settings but engine commands (`_setrenderer`, `_sethdmodels`, `_setaddons_folder`)
  stored in the registry key shared by every GoldSrc game (`HKCU\Software\Valve\Half-Life\Settings`), applied
  on restart. So they can't be listed in John Settings, and the mod leaves their values alone rather than
  change them for vanilla too.

The two `.res` files are copies of Valve's for a specific version of the game (now the 25th anniversary one).
If Valve changes those tabs again, rebase them on the new files rather than keep the old layout: a layout that
doesn't mention one of the tab's controls leaves it at a default position (a stray checkbox).

**Don't use `LIST` (dropdown) entries unless the values are exactly 0, 1, 2, ... in order.** GameUI treats a
list's stored value as an item index: when the dialog opens it reads the setting's current value as the
index, and on OK it sends the value of that item, or `0.0` if the index is past the end. A list with other
values corrupts the setting a little more on every OK (this set `fps_max 0` once). `BOOL` and `NUMBER`
entries read and write the real value; `NUMBER` is clamped to its min and max (-1 = no limit).

The engine's options tabs are fixed, and only the `user.scr` list can hold mod settings: a tab's layout file
can move or hide its controls, but which setting a control changes is decided in Valve's code. Single
player only games (`type "singleplayer_only"` in `liblist.gam`, as MMod) show that list as a tab in place of
Multiplayer; johnhalflife keeps multiplayer, so it sits behind a button. A dedicated settings window is
possible with client VGUI2 (see BugfixedHL-Rebased) but was judged not worth the port for now.

### Blood and gibs
`jhl_gore` (in the dialog as "Vanilla | Blood and gibs")

Valve's content lock only sets `violence_hblood`, `violence_hgibs`, `violence_ablood` and `violence_agibs` to 0
(and back to 1 when unlocked, after checking the password); nothing re-applies it at startup. `jhl_gore`
stands in for all four so the dialog can show one checkbox: at startup it's set from them (on only if all four
are on), and changing it sets all four. It isn't saved itself; the four are.

### Recommended settings
`src/game/client/jhl_settings.cpp`

* **First launch:** `crosshair_scale 2`, `m_rawinput 1`, and blood and gibs on (the content lock isn't
  used). Applied once, a few frames after start so the saved config has loaded, then remembered in
  `jhl_recommended_version`. Raising `RecommendedSettingsVersion` applies a changed list once more.
* **Reset:** the `jhl_recommended` command, or the "Reset all to recommended settings" box in the dialog,
  sets every setting listed in the dialog, and only those, to its recommended value (`DialogSettings`,
  matching the defaults in `user.scr`). Notable values: `crosshair_scale 2`, `default_fov 105`,
  `fps_override 1` with `fps_max 240`, `m_rawinput 1`.

## HUD

### HUD scale
`jhl_hud_scale` (0 = automatic by screen height)

The sprite HUD is laid out on a virtual screen (real size divided by the scale) and drawn scaled. Engine
text keeps its size; VGUI and the spectator HUD stay in real pixels. Crosshair size stays on
`crosshair_scale`. The crosshair itself is vanilla.

## Expansions

Each game is its own mod folder with its own name, main menu, New Game / Hazard Course and saves, the way Valve
ships Opposing Force and Blue Shift on top of Half-Life. The expansion folders are thin: a `liblist.gam` with
`fallback_dir "johnhalflife"` (all maps, models, sounds and settings come from the base folder), the menu, and
a copy of the game code.

| Folder | Name | Start / training map | Source |
|---|---|---|---|
| `johnhalflife` | John Half-Life | `c0a0` / `t0a0` | `config/liblist.gam.in`, `config/mod/` |
| `johnhalflife_op4` | John Half-Life: Opposing Force | `of0a0` / `ofboot0` | `config/expansions/op4/` |
| `johnhalflife_bs` | John Half-Life: Blue Shift | `ba_tram1` / `ba_hazard1` | `config/expansions/bs/` |
| `johnhalflife_uplink` | John Half-Life: Uplink | `hldemo1` / `t0a0` | `config/expansions/uplink/` |

* The build installs the game code, `delta.lst` and the settings files into each folder.
* Menu files used unchanged are copied from the player's own Valve install at install time, so they match their
  version of the game and are never committed (`cmake/JhlInstallValveMenu.cmake`): Half-Life's main menu,
  without its hint text next to each button
  (`GameMenu.res`, shared by all four, so the Unified SDK's Campaign Select is gone; its map,
  `hlu_campaignselect`, still works from the console), background layouts and tiles, logo and icon, then the
  folder's own game's (`gearbox`, `bshift`) over them.
* John Half-Life and Uplink use Half-Life's art and set the 25th anniversary `liblist.gam` keys
  `hd_background "1"` (the 21:9 background in `HD_BackgroundLayout.txt`: 105 tiles of 256x256 at
  `resolution 3840 1600`) and `animated_title "1"`, like Valve's `valve` folder. Opposing Force and Blue Shift
  don't, like `gearbox` and `bshift`. Without them, or without the layout in the mod's own folder, the 25th
  anniversary main menu was black.
* Blue Shift keeps multiplayer, unlike vanilla, so John Settings stays in the same place.
* Settings and saves are per folder, as with the real games.
* The engine adds the `fallback_dir` folder under the search path ID `GAME_FALLBACK`. The Unified SDK reads its
  config files (model/sound replacement, skill, server config, map cycle) from `GAMECONFIG`, the mod's own folder
  only, to keep downloaded content out, so `FileSystem_LoadFileIntoBuffer` also tries `GAME_FALLBACK` when a
  `GAMECONFIG` read finds nothing. Without it, the expansions' view models stayed Half-Life's.

## MMod assets

`mmod_port/` tracks every MMod file the mod uses (29 files): `assets.json` lists them with SHA-256 hashes,
`ASSETS.md` is the generated report, and `port_assets.py install` copies them from a local MMod
installation into the mod directory. Each must be replaced, removed or cleared with its author before
anything is published. In short:

* **Movement:** jump, landing, long jump and wood footstep sounds, and MMod's texture material table.
* **Weapon switch:** 15 draw sounds.
* **Grenade pin:** the pin pull sound.
* **Water splash:** the ripple ring sprite (optional; the splash works without it).

## Networking

Three user messages were added; everything else is client side or reuses existing sounds.

| Message | Size | Sent by | Purpose |
|---|---|---|---|
| `SmoothPunch` | 20 bytes | Server, on jump and landing | Spring-damped view jolt |
| `BulletHit` | 10 bytes | Server, `DecalGunshot` (NPCs, mounted guns) | Bullet impact effects |
| `WaterSplash` | 6 bytes | Server, NPC bullets entering water | Water splash |

User message names must be 11 characters or fewer; a longer name fails to register and sending it is
fatal ("bogus message type (0)").

Weapon prediction also syncs the current spread through `weapon_data_t.fuser4` (added to `delta.lst`).

## Versions

The version is `project(HalfLifeMod VERSION ...)` in `CMakeLists.txt` and the release type is
`HalfLife_RELEASE_TYPE` (Pre-Alpha, Alpha, Beta, Release), both shown in the build info
(`cl_projectinfo_show 1`, or "SDK | Show build info" in John Settings; off by default). Milestones raise the
minor version and get a git tag, `v<version>-<release type>`; fixes between milestones raise the patch.

| Tag | What it covers |
|---|---|
| `v0.1.0-prealpha` | Camera and movement, weapons (recoil, accuracy, effects, sounds), John Settings and the menus on the 25th anniversary engine, separate game folders for Half-Life, Opposing Force, Blue Shift and Uplink |

## Saves

Don't add a data map (`DECLARE_DATAMAP`/`BEGIN_DATAMAP`) to an entity class that doesn't have one, for example
to use a think function: saves made before the change no longer restore it. Restoring reads one block per class
in the hierarchy, starting with the most derived, and stops at the first one it doesn't find, so none of the
inherited fields are restored. A weapon restored that way has no owner and crashed the game when equipped.
Keep transient timers out of save data instead (as `CHandGrenade::ItemPostFrame` does), or accept breaking
older saves.

## Open items

* Opposing Force weapons without an MMod counterpart still use the crowbar's switch sound as a placeholder.
* HUD work is next.
