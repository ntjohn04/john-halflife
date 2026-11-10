# John Half-Life

A mod for Half-Life, Opposing Force and Blue Shift, built on the Half-Life Unified SDK.
These pages record what John Half-Life changes compared with the stock SDK, and why; the settings and launch
options are on the [front page](../README.md). The Unified SDK's own documentation is in [unified-sdk](unified-sdk/README.md).

* [Camera and movement](camera_movement.md)
* [Weapons](weapons.md)
* [Physics](physics.md): Half-Life's game code acting at any frame rate as it did at 72 fps
* [Visuals](visuals.md): signs and panels flush with walls drawn steady; the software renderer's touches
* [Sound](sound.md): Half-Life's own mixing and room effects on the SDK's OpenAL sound system
* [Effects](effects.md): every visual effect (gibs, blood, explosions, beams, sparks...): vanilla's and ours
* [Changelog](CHANGELOG.md): every version and what it brought
* [Release](release.md): the install targets, what's shipped and what's made from the player's games
* [Tuning sheet](tuning.md): the numbers on the changes since `v0.7.0-prealpha` (shared systems, gargantua,
  tentacles, mounted guns), where each is and where to see it

## Debug commands

Console commands for finding problems; none changes anything in play.

* Hitch finder: `jhl_debug_hitch <ms>` (console, 0 off) reports every frame longer than that: the client's and the
  server's time by part (each engine call into them, user messages, sound loading, resampling and reverb changes,
  entity thinks and touches, by class), their slowest calls, and what the engine took beyond them. Printed to the
  console and added to `jhl_hitches.txt` in the game's folder; the 2 seconds after a map loads, and pauses, aren't
  counted (`src/game/shared/jhl_hitch.h`). A frame that saved the game says so: an autosave (`trigger_autosave`) or a
  quick save takes the engine 25-130 ms to write, in vanilla too (the three hitches of the 1.0 playthrough were
  all autosaves).
* Muzzle check: `jhl_debug_muzzle` (console) prints everything that keeps the muzzle light and smoke from being made:
  `jhl_fx_muzzleflash`, third person or a spectator state, a zoomed field of view against `default_fov`, the view
  model's muzzle point, `r_dynamic`, and whether a temporary entity can be had. For the rare times they've stopped.
* Blocked movers: `jhl_debug_blocked` (console, toggles) prints each time something blocks an elevator, door or train:
  the mover, its height and speed, and the blocker's health, place and footing, then the engine's push tests replayed
  (free before, how far it was pushed, stuck after), any rider raised out of the floor, and any NPC on a moving mover within 4 units of its floor not kept standing on it, with why (stuck, or over something else). For NPCs crushed or bouncing on lifts.
* NPC drawing: `jhl_debug_npcdraw 1` prints, 50 times a second for each NPC drawn, its height, its height before its
  last animation update, its sequence, frame and yaw, the smoothing fraction and the height it's drawn at. For telling a real bounce from a drawn one.
* NPC damage: `jhl_debug_damage` (console, toggles) prints each hit an NPC takes, alive or dead: the damage, its type
  and source, the health before, death and script state, movetype and footing, and when one is gibbed. For bodies
  gibbed when they shouldn't be.
* Texture materials: `jhl_texture [material]` prints the texture under the crosshair and the materials bullet impacts
  use for it, and adds it to `jhl_textures.txt` with the material given, for corrections to
  `config/mod/sound/jhl_impact_materials.json` ([weapons](weapons.md#bullet-impacts)).
* [Effects test command](effects_test.md): `jhl_fx_test`, for designing particle effects

## The settings menu

Options → Multiplayer → **John Settings...** lists the settings (on the [front page](../README.md#settings)) plus a few vanilla ones (crosshair
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
player only games (`type "singleplayer_only"` in `liblist.gam`) show that list as a tab in place of
Multiplayer; johnhalflife keeps multiplayer, so it sits behind a button. A dedicated settings window is
possible with client VGUI2 (see BugfixedHL-Rebased) but was judged not worth the port for now.

### Blood and gibs (`jhl_gore`)
Valve's content lock only sets `violence_hblood`, `violence_hgibs`, `violence_ablood` and `violence_agibs` to 0
(and back to 1 when unlocked, after checking the password); nothing re-applies it at startup. `jhl_gore`
stands in for all four so the dialog can show one checkbox: at startup it's set from them (on only if all four
are on), and changing it sets all four. It isn't saved itself; the four are.

### Recommended settings
`src/game/client/jhl_settings.cpp`

The recommended values are the defaults: every setting listed in the dialog, and only those (`DialogSettings`), with
the same values as `user.scr`'s defaults and the game code's own (`crosshair_scale`, `default_fov`). Notable ones:
`crosshair 1`, `crosshair_scale 0` (automatic), `default_fov 100`, `sv_aim 0` (auto-aim), `fps_override 1` with `fps_max 240`,
`gl_vsync 0`,
`m_rawinput 1`, blood and gibs on (the content lock isn't used).

* **First launch:** all of them, once, a few frames after start so the saved config has loaded, then remembered in
  `jhl_recommended_version`; the engine's own settings (frame rate, vsync, the crosshair, auto-aim) have its defaults
  otherwise. Raising `RecommendedSettingsVersion` applies the list once more.
* **Reset:** the `jhl_recommended` command, or the "Reset all to recommended settings" box in the dialog.
* **Debug tools** (`jhl_debug_*`, above) aren't settings: not saved, off at every launch, and left out of the list.

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
* Menu files used unchanged are copied from the player's own Valve install on the first start, so they match their
  version of the game and are never shipped (`client/jhl_setup.cpp`): Half-Life's main menu, with its hint beside
  each button as WON had
  (`GameMenu.res`, shared by all four, so the Unified SDK's Campaign Select is gone; its map,
  `hlu_campaignselect`, still works from the console), background layouts and tiles, logo and icon, then the
  folder's own game's (`gearbox`, `bshift`) over them. The icon is also each folder's `game_big.tga`, the one
  Steam's library shows for a mod: Half-Life's red lambda for John Half-Life and Uplink, Opposing Force's green,
  Blue Shift's blue (Steam reads it when it starts).
* **The main menu's text, as the WON versions had it** (`scripts/make_menu_strings.py` writes each game's
  `resource/gameui_english.txt`): labels in Title Case in Half-Life and in capitals in the expansions (Options kept,
  not WON's Configuration), a hint beside each button, Hazard Course for the Training Room; Opposing Force's drill
  sergeant ("You going AWOL on me, son?" to quit, its hints, Boot Camp). From the community's WON menu restorations,
  which agree. English only.
* **The expansions' menu buttons in their own fonts and colours:** Opposing Force's in Stardos Stencil Bold, the gold
  of its logo; Blue Shift's in Courier Prime Bold, a typewriter's like its logo, ice blue (both SIL Open Font
  License, shipped in each folder's `resource/fonts`). WON's buttons were pictures, none of which survive, so these
  are chosen by eye; Blue Shift's follows both restorations' Courier New. The first start writes each folder's
  `resource/TrackerScheme.res` from Half-Life's with only the buttons' fonts (`MenuLarge` and its glow,
  `MenuBlurLarge`), their colours (`InGameDesktop`) and the font file (`CustomFontFiles`) changed; the hints keep
  Half-Life's.
* **Intro videos:** the engine plays the list in each game folder's own `media/StartupVids.txt` (`-novid` skips
  it, a key the video playing). Their sound isn't the video's: it plays `sound/UI/valve_sound.wav` as each starts,
  so each game has one video and that file is its soundtrack. John Half-Life and Uplink play Half-Life's intro (the
  first start copying the player's `valve/media/valve.webm`; Valve's sound is found in `valve`). Blue Shift plays its
  WON intros, Sierra Studios then Gearbox Software and Valve, as one, and Opposing Force Gearbox's and Valve's, both
  shipped (`config/expansions/*/media/intro.webm` and `sound/UI/valve_sound.wav`, made from Blue Shift's own AVIs), so
  neither needs the other installed.
* John Half-Life and Uplink use Half-Life's art and set the 25th anniversary `liblist.gam` keys
  `hd_background "1"` (the 21:9 background in `HD_BackgroundLayout.txt`: 105 tiles of 256x256 at
  `resolution 3840 1600`) and `animated_title "1"`, like Valve's `valve` folder. Without them, or without the
  layout in the mod's own folder, the 25th anniversary main menu was black.
* **Opposing Force's and Blue Shift's backgrounds are their WON versions' art, widescreen:** the 800x600 picture
  without its logos, its sides carried on to 21:9 (FLUX.1 Fill) and all of it upscaled to Half-Life's 3840x1600
  (4xNomos8kDAT). `scripts/make_menu_backgrounds.py` cuts a picture into the tiles
  (`resource/background/jhl_21_9_*.tga`, names the first start's copies of the games' own 800x600 backgrounds never
  touch) and writes `HD_BackgroundLayout.txt`; the pictures, in versions with the WON logos, only the game's own and
  none, are made outside the repository.
* **And Half-Life's animated title, with their names:** `animated_title "1"`, the engine (GameUI.dll) animating the
  pictures it finds in the folder, `resource/logo*.tga`, exactly as Half-Life's: OPPOSING FORCE in its buttons'
  stencil and gold, blue shift in lower case as its logo, in its typewriter face and neon blue (the big blurred names
  behind them paler, in capitals), each of Valve's ten pictures remade at its size, its letters' place, blur and
  opacity (`scripts/make_menu_titles.py`). As in Half-Life, the title hides the small logo above the buttons
  (`game_menu.tga`); it's always centred near the top, sized by its picture.
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
  folder from the player's install on the first start (`client/jhl_setup.cpp`).

## The Unified SDK's models

Compared with vanilla's (2026-11, LD models), the Unified SDK's models keep the same textures (re-encoded, or
the same art on re-laid-out sheets), render flags and view model sequence order. Renamed or split sequences are
matched by its upgraded maps. They add sequences, bodygroups and hitboxes (neck boxes count as head; Opposing
Force's ally grunts and black ops can take headshots, which Gearbox's models didn't allow); these are kept.

* **Walking and running speed:** recompiled from decompiled sources, the models' linear movement came out
  (frames - 1) / frames of vanilla's, so NPCs moved 2 to 6% slower. `scripts/restore_npc_speeds.py` puts the
  vanilla values back into the installed models at install time (93 sequences; Rosenberg's take the scientist's
  values, whose animations his model uses), from the player's own games.
* **Rosenberg:** the one character whose face was high-definition among low-definition ones (Blue Shift's own
  Rosenberg head, 128x128, kept by the Unified SDK). John Half-Life builds its own `models/rosenberg.mdl`: the
  Unified SDK's with a community low-definition head (Paynamia's, CC BY-NC 4.0; `assets/README.md`).
* **Blue Shift's helmet and vest:** Gearbox made only one of each, high-definition beside Barney's low-definition
  helmet and vest. John Half-Life ships community low-definition ones built from low-definition Barney, unchanged:
  Evy Houstuvk's vest and Paynamia's helmet (`config/mod/models`, credited in `config/licenses/CREDITS.txt`). Files
  John Half-Life ships itself are left out of the first start's copying (`scripts/make_setup_files.py`). Their HUD
  pickup icons are Blue Shift's, as drawn.
* **Harold (Blue Shift, ba_yard3):** the wounded scientist was the glasses scientist, as any other. John Half-Life
  gives him his own head: that one bald and without glasses (`assets/README.md`), on two models of his own, pointed
  at by the first start's map conversion (`BaYard3HaroldModel`). Every other scientist is as before.
* **Blue Shift's viewmodel arms:** Gearbox gave every low-definition viewmodel high-definition arm textures (260x247),
  and its high-definition ones low-definition arms (68x62). The release packager swaps in the small pair, the same art
  laid out alike, from the Unified SDK's own high-definition Blue Shift handgun (`scripts/lowdef_arms.py`, on the 23
  viewmodels with arms).
* **Blue Shift's chair:** the one chair model, used in one scene, was a high-definition one beside the brush-built
  chairs everywhere else; John Half-Life ships Evy Houstuvk's, from the prefab those are made from (CC BY-NC-ND 4.0,
  unchanged). The toolbox and flashlight props are left as Gearbox made them.
* **Glass shards:** `glassgibs.mdl`'s texture is named `chome_glass`, so it never got the chrome flag a texture named
  chrome gets; the first start writes a copy of the player's with the flag set (`jhl_setup.cpp`).
* **Opposing Force's of3a1b:** its chapter title was Half-Life's `C4A1TITLE`, copied from c4a1 and shown as "XEN" (vanilla
  Opposing Force has no such title); the first start's map conversion removes it (`Of3a1bRemoveChapterTitle`), the
  chapter's title showing at of3a1.
* **Left as is:** Blue Shift's scientists use Half-Life's `scientist.mdl` and its frame rates; vanilla Blue
  Shift's walked and ran slower (a run at 207 units/s, Half-Life's 293). The Unified SDK's
  `models/bshift/scientist.mdl` is unused and mixes the two games' rates, so restoring them would take another
  copied config (a model replacement) and an untried model in every Blue Shift scene, for a difference hard to see.
* Settings and saves are per folder, as with the real games.
* The engine adds the `fallback_dir` folder under the search path ID `GAME_FALLBACK`. The Unified SDK reads its
  config files (model/sound replacement, skill, server config, map cycle) from `GAMECONFIG`, the mod's own folder
  only, to keep downloaded content out, so `FileSystem_LoadFileIntoBuffer` also tries `GAME_FALLBACK` when a
  `GAMECONFIG` read finds nothing. Without it, the expansions' view models stayed Half-Life's.

## Networking

Five user messages were added; everything else is client side or reuses existing sounds.

| Message | Size | Sent by | Purpose |
|---|---|---|---|
| `SmoothPunch` | 20 bytes | Server, on jump and landing | Spring-damped view jolt |
| `BulletHit` | 10 bytes | Server, `DecalGunshot` (NPCs, mounted guns) | Bullet impact effects |
| `WaterSplash` | 6 bytes | Server, NPC bullets entering water | Water splash |
| `BloodPool` | 3 bytes | Server, a bleeding NPC dying without gibbing (PVS) | Blood pool under the body |
| `JhlMusic` | Varies | Server, the player's HUD starting (single player) | The music as saved ([Sound](sound.md#music-in-saves)) |

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
minor version and get a git tag, `v<version>-<release type>`, and a row in the [changelog](CHANGELOG.md); fixes between milestones raise the patch, and a released
fix is tagged the same way.

## Model slots

The engine has 512 model slots a map, and the map's brush entities (doors, walls, glass: `*1`, `*2`...) take theirs
first: of4a4 has 330, and with every weapon of both games precached it used 508 of them with johnhalflife's effect
sprites (vanilla Opposing Force used about as many). Past 512 a model isn't precached, and its first use ends the map
(`Host_Error: no precache`); the console says when it happens, and `jhl_debug_precache [list]` counts them. The client
can't load a model the server didn't precache (`CL_LoadModel` gives -1), so the sprites and models only
johnhalflife's effects use are precached last in `W_Precache`, most-seen first, only while the map has room for its
own models and 10 to spare (43; 33 come after them on of4a4, and followers crossing a level change bring theirs):
op4ctf_crash gets none, of4a4 6 of 13, op4ctf_power most, and those
effects draw their vanilla part only, the console saying so as the map loads. The client asks for them with `JhlEffectModel` (`src/game/client/jhl_models.h`),
0 where they aren't precached. Anything new precached on every map: check `jhl_debug_precache` on of4a4.

## Saves

Don't add a data map (`DECLARE_DATAMAP`/`BEGIN_DATAMAP`) to an entity class that doesn't have one, for example
to use a think function: saves made before the change no longer restore it. Restoring reads one block per class
in the hierarchy, starting with the most derived, and stops at the first one it doesn't find, so none of the
inherited fields are restored. A weapon restored that way has no owner and crashed the game when equipped.
Keep transient timers out of save data instead (as `CHandGrenade::ItemPostFrame` does), or accept breaking
older saves.
