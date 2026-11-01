# johnhalflife

An MMod-inspired mod for Half-Life, Opposing Force and Blue Shift, built on the Half-Life Unified SDK.
These pages record what johnhalflife changes compared with the stock SDK, and why; the settings, launch options
and versions are on the [front page](../../README.md).

* [Camera and movement](camera_movement.md)
* [Weapons](weapons.md)
* [Physics](physics.md): Half-Life's game code acting at any frame rate as it did at 72 fps
* [Visuals](visuals.md): signs and panels flush with walls drawn steady; the software renderer's touches
* [Sound](sound.md): Half-Life's own mixing and room effects on the SDK's OpenAL sound system
* [Effects plan](effects_plan.md): the plan the effects (v0.2.0 to v0.8.0) were built from, with what each does
* [Effects test command](effects_test.md): `jhl_fx_test`, for designing particle effects
* [Tuning sheet](tuning.md): the numbers on the changes since `v0.7.0-prealpha` (shared systems, gargantua,
  tentacles, mounted guns), where each is and where to see it

## The settings menu

Options → Multiplayer → **John Settings...** lists the settings (on the [front page](../../README.md#settings)) plus a few vanilla ones (crosshair
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

### Blood and gibs (`jhl_gore`)
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
* Each game's own skill values load after `skill.json` (Half-Life's): `cfg/skill_blueshift.json` (batteries
  give 20 armor, as in `bshift/skill.cfg`) and `cfg/skill_opposingforce.json` (`func_tank_target_allies 1`:
  only Opposing Force's mounted guns also fire at the player's allies, the Unified SDK had it for every game).
  They're listed in `config/mod/cfg/BlueShiftConfig.json` and `OpposingForceConfig.json`, copies of the Unified
  SDK's files (assets repository, `cfgsrc/cfg`) with a `Skill` section added; if those change upstream, copy
  them again.
* Files the three games each have their own version of under one name (two scientist lines, Barney's
  "icanhear", `misc/talk.wav`, `common/null.wav`, the RPG's laser dot, `640_logo.spr`) were left by the Unified
  SDK's installer as Opposing Force's or Blue Shift's for every game. The build copies each game's own into its
  folder from the player's install (`cmake/JhlInstallGameFiles.cmake`).

## The Unified SDK's models

Compared with vanilla's (2026-11, LD models), the Unified SDK's models keep the same textures (re-encoded, or
the same art on re-laid-out sheets), render flags and view model sequence order. Renamed or split sequences are
matched by its upgraded maps. They add sequences, bodygroups and hitboxes (neck boxes count as head; Opposing
Force's ally grunts and black ops can take headshots, which Gearbox's models didn't allow); these are kept.

* **Walking and running speed:** recompiled from decompiled sources, the models' linear movement came out
  (frames - 1) / frames of vanilla's, so NPCs moved 2 to 6% slower. `scripts/restore_npc_speeds.py` puts the
  vanilla values back into the installed models at install time (88 sequences), from the player's own games.
* **Open:** Blue Shift's scientists use Half-Life's `scientist.mdl` and its frame rates; vanilla Blue Shift's
  walked and ran slower (a run at 207 units/s, Half-Life's 293). The Unified SDK's `models/bshift/scientist.mdl`
  is unused and mixes the two games' rates.
* Settings and saves are per folder, as with the real games.
* The engine adds the `fallback_dir` folder under the search path ID `GAME_FALLBACK`. The Unified SDK reads its
  config files (model/sound replacement, skill, server config, map cycle) from `GAMECONFIG`, the mod's own folder
  only, to keep downloaded content out, so `FileSystem_LoadFileIntoBuffer` also tries `GAME_FALLBACK` when a
  `GAMECONFIG` read finds nothing. Without it, the expansions' view models stayed Half-Life's.

## MMod assets (the checklist)

`mmod_port/` tracks every MMod file the mod uses (29 files): `assets.json` lists them with SHA-256 hashes,
`ASSETS.md` is the generated report, and `port_assets.py install` copies them from a local MMod
installation into the mod directory. Each must be replaced, removed or cleared with its author before
anything is published.

## Networking

Four user messages were added; everything else is client side or reuses existing sounds.

| Message | Size | Sent by | Purpose |
|---|---|---|---|
| `SmoothPunch` | 20 bytes | Server, on jump and landing | Spring-damped view jolt |
| `BulletHit` | 10 bytes | Server, `DecalGunshot` (NPCs, mounted guns) | Bullet impact effects |
| `WaterSplash` | 6 bytes | Server, NPC bullets entering water | Water splash |
| `BloodPool` | 3 bytes | Server, a bleeding NPC dying without gibbing (PVS) | Blood pool under the body |

User message names must be 11 characters or fewer; a longer name fails to register and sending it is
fatal ("bogus message type (0)").

Weapon prediction also syncs the current spread through `weapon_data_t.fuser4` (added to `delta.lst`).

johnhalflife also sends its own kinds of temp entity through the SDK's `TempEntity` message (the engine's numbers,
its own contents, parsed in `client/entity.cpp` `MsgFunc_TempEntity`), in place of or alongside the engine's:

| Kind | Sent by | Purpose |
|---|---|---|
| `TE_EXPLOSION` | Explosions (flag bits 32 breakable, 64 sparks, 128 rocket) | johnhalflife's fireballs, smoke, sparks, debris |
| `TE_BREAKMODEL` | Breakables, vehicle debris | Debris lasting 60 s, by material |
| `TE_SPARKS` | `UTIL_Sparks`, computers, set pieces | Spark bursts and showers |
| `TE_ARMOR_RICOCHET` | Armor ricochets | The flash with a few sparks |
| `TE_BEAMTORUS` | Ring effects (houndeye, helicopter crashes, displacer) | The rings' noisy torus strand |
| `TE_BEAMRING` | c4a1's and of3a1b's spire ring `env_beam`s | The spire rings' strands, kept on their trains |
| `TE_SMOKE` | Houndeye blasts | Dust puffs along the floor |
| `TE_SPRITETRAIL` | The displacer ball as it goes | Its falling specks |
| `TE_TRACER` | NPCs' tracers (as often as vanilla's) | The vanilla tracer and johnhalflife's streak, from the muzzle |
| `TE_BEAMDISK` | The spore launcher's burst | Its blast ring, facing the view |
| `TE_BLOODSPRITE` | A snark bursting | A small blood puff and blood chunks |
| `TE_FIREFIELD` | A gargantua's flames, each tenth of a second | Where each hand's lands: flickering lights, embers, heat smoke |
| `TE_LAVASPLASH` | A gargantua's stomp, each move | The stretch it covered: its head's light, the ground kicked up |
| `TE_LARGEFUNNEL` | A tentacle's tap, slam or killing swing | What the floor throws up, by its material |
| `TE_USERTRACER` | A mortar shell 0.35 s before it lands | Its streak falling onto where it lands |
| `TE_MULTIGUNSHOT` | A `func_tank` shot | Its muzzle light, flash and smoke by kind; a machine gun's brass; a laser's lights |
| `TE_BUBBLES` | An ichthyosaur's bite | Bubbles bursting out from its mouth, slowing and rising |

## Versioning

The version is `project(HalfLifeMod VERSION ...)` in `CMakeLists.txt` and the release type is
`HalfLife_RELEASE_TYPE` (Pre-Alpha, Alpha, Beta, Release), both shown in the build info
(`cl_projectinfo_show 1`, or "SDK | Show build info" in John Settings; off by default). Milestones raise the
minor version and get a git tag, `v<version>-<release type>`; fixes between milestones raise the patch, and a released
fix is tagged the same way.

## Saves

Don't add a data map (`DECLARE_DATAMAP`/`BEGIN_DATAMAP`) to an entity class that doesn't have one, for example
to use a think function: saves made before the change no longer restore it. Restoring reads one block per class
in the hierarchy, starting with the most derived, and stops at the first one it doesn't find, so none of the
inherited fields are restored. A weapon restored that way has no owner and crashed the game when equipped.
Keep transient timers out of save data instead (as `CHandGrenade::ItemPostFrame` does), or accept breaking
older saves.
