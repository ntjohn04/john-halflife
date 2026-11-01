# johnhalflife

An MMod-inspired mod for Half-Life, Opposing Force and Blue Shift, built on the Half-Life Unified SDK.
These pages record what johnhalflife changes compared with the stock SDK, and why.

* [Camera and movement](camera_movement.md)
* [Weapons](weapons.md)
* [Physics](physics.md): Half-Life's game code acting at any frame rate as it did at 72 fps
* [Visuals](visuals.md): signs and panels flush with walls drawn steady; the software renderer's 16-bit color and rippling water
* [Sound](sound.md): Half-Life's own mixing and room effects on the SDK's OpenAL sound system
* [Effects plan](effects_plan.md): the map of visual effects to work through next
* [Effects test command](effects_test.md): `jhl_fx_test`, for designing particle effects
* [Tuning sheet](tuning.md): the numbers on the changes since `v0.7.0-prealpha` (shared systems, gargantua,
  tentacles, mounted guns), where each is and where to see it

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
| `jhl_visuals_flushfaces` | 1 | Signs, posters and panels built flush with walls drawn steady (they flickered in OpenGL and vanished in software) | [Visuals](visuals.md) |
| `jhl_visuals_16bitcolor` | 1 | The software renderer's 16-bit color: every frame cut to 565, as it drew | [Visuals](visuals.md#16-bit-color-jhl_visuals_16bitcolor) |
| `jhl_visuals_softwarewater` | 1 | The software renderer's water: its ripple simulation in place of OpenGL's sine wobble, and under water its 320 x 200 view and fog by depth | [Visuals](visuals.md#software-renderer-touches) |
| `jhl_visuals_renderfixes` | 1 | Models lit up to 4/3 as the world is; with texture filtering off, textures point sampled with mipmaps and the sky pixelated as the world is | [Visuals](visuals.md#software-renderer-touches) |
| `jhl_physics_fpsfix` | 1 | Pushables, +use walking, grenades and gibs sliding, debris settling and fire rates acting at any frame rate as at 72 fps | [Physics](physics.md) |
| `jhl_weapons_smoothrecoil` | 1 | Spring-damped recoil (0 = original punches) | [Weapons](weapons.md#recoil) |
| `jhl_fx_bulletimpacts` | 1 | Bullet impact sparks/dust and water splashes | [Weapons](weapons.md#bullet-impacts) |
| `jhl_fx_muzzleflash` | 1 | Muzzle light and smoke, RPG backblast | [Weapons](weapons.md#muzzle-light-and-smoke) |
| `jhl_fx_particlesystem` | 1 | The particle system changes (gibs, blood, explosions, debris, beams, rings) | [Effects plan](effects_plan.md#gibs-done) |
| `jhl_fx_tracers` | 1 | johnhalflife's tracers (with the particle system on): the M249's tracer rounds, every gun's faint streaks, NPCs' | [Weapons](weapons.md#tracers) |
| `jhl_hud_scale` | 1 | Sprite HUD scale (0 = automatic by screen height) | [HUD](#hud) |
| `jhl_snd_vanilla` | 1 | Half-Life's own mixing (22 kHz, its pitch grain, ear volumes, clipping) and room effects, in place of the SDK's OpenAL sound | [Sound](sound.md) |

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

## Recommended launch options

Set in Steam: right-click Half-Life (or the mod), Properties, Launch Options. These are the engine's own, read
once as it starts, before any of the mod's code runs, so the mod can't set them itself.

* **`-particles 8192`**: room for more of the engine's small particles (explosion specks, spark streaks, blood
  specks, tracer particles), which the effects use more of than vanilla. Without it, a chain of big explosions can
  run the engine's pool dry: the console says "not enough free particles", and a few specks don't appear for a
  moment. Harmless either way (nothing breaks; the explosions also thin their own out as they near it:
  [tuning sheet](tuning.md#explosion-particles-clientjhl_explosionscpp-particleshare)); this just keeps them all.
  Modern PCs don't feel it.
* **`-condebug`** (optional, for troubleshooting): writes the console to `qconsole.log` in the game folder as you
  play, so its last lines survive a crash. With the crash's details from Windows' event log, that usually shows
  what happened.

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

## Versions

The version is `project(HalfLifeMod VERSION ...)` in `CMakeLists.txt` and the release type is
`HalfLife_RELEASE_TYPE` (Pre-Alpha, Alpha, Beta, Release), both shown in the build info
(`cl_projectinfo_show 1`, or "SDK | Show build info" in John Settings; off by default). Milestones raise the
minor version and get a git tag, `v<version>-<release type>`; fixes between milestones raise the patch, and a released
fix is tagged the same way.

| Tag | What it covers |
|---|---|
| `v0.1.0-prealpha` | Camera and movement, weapons (recoil, accuracy, effects, sounds), John Settings and the menus on the 25th anniversary engine, separate game folders for Half-Life, Opposing Force, Blue Shift and Uplink |
| `v0.2.0-prealpha` | Gibs (counts, lifetime, bounce sounds, blood trails, per-NPC profiles, central blood splash with red and yellow chunks, blood pools) and hit blood (sizes, colors, exit spurts, melee bursts, headshot streams), `jhl_fx_test`, the `jhl_fx_particlesystem` toggle |
| `v0.3.0-prealpha` | Explosions: vanilla placement restored (Half-Life Updated sank them into surfaces), two-fireball pool with randomness, bigger RPG rockets and explosive breakables, helicopter deaths (varied falling fireballs, crash mushroom cloud), smoke (bigger puff, lingering cloud), client spark showers (by surface material), surface debris by material, ember specks, size-scaled lights, scorches, screen shake, underwater fireballs and bubbles; debris lasting 60 s with a shared budget (barrels, helicopters, gibs) |
| `v0.4.0-prealpha` | Beams and rings: the vortigaunt's strand pattern (a thin, wild, dimmer strand, endpoint and impact lights, sparks by what was hit) on vortigaunts, voltigores, the displacer and shock rifle, controllers (round animated balls) and the Nihilanth (zaps, death bolts, a finale of flashes and spark showers), map arcs, Gene Worm warp beams and electrified wires; a noisy torus on every ring (houndeye, helicopter crashes, displacer), houndeye light, shake and dust, the displacer's implosion; Xen spire rings layered three deep, drifting in and out of step; breakable shards lasting like gibs (crates throw wood, computers spark), varied spark bursts, armor ricochet sparks, blob particles and fewer non-metal spark showers on explosions; fixes: controller energy ball NaN velocity, gluon view drift after a map change |
| `v0.5.0-prealpha` | Weapons: tracers (the M249's red tracer rounds with a white-hot core and burning specks off walls; faint streaks on every gun, NPCs' too, from the muzzle; `jhl_fx_tracers`), the M249's muzzle smoke building with sustained fire, per-pellet shotgun smoke, the gluon gun's dragged, bending beam (vanilla colors restored) with scorches, leaking sparks, a fizzle on release and lights, the Gauss gun's visible charge (glow, arcs, unstable sparks), brighter layered beams, release bursts, afterimages of charged shots and impact and reflection lights, a crossbow bolt wake, melee sparks on metal, the tripmine light; corpse gibbing by bullet damage (headcrabs, shock roaches, houndeyes, pit drones, bullsquids, controllers), displacer specks; fixes: controller spark burst on corpse gib |
| `v0.6.0-prealpha` | Weapons, continued: hornets' twirling trails in five colours (calm and flourishing moods, straight on the strike), their spit, dart splats, blood on stings and a death light; the shock rifle's bolt as a glow and spark circling its path, tightening with range, half spinning each way, with a double-helix wake, purple fizzles or cyan arcs on impact and a muzzle light; projectiles aimed at the crosshair (shock rifle, RPG, hornet gun, spore rocket); the barnacle grapple's springy tongue (slack flying out, bowing as you move, twanging taut on latching, swallowing as it pulls), reeling back in on letting go, a bite of blood, a wet mark where it latches, blended view model transitions; fixes: shock rifle crash on firing, shock bolt helix on the grapple's tongue tip, grapple animation flicker from prediction |
| `v0.7.0-prealpha` | Weapons, finished: the spore launcher's lights, burst off the surface and a blast ring showing its reach; weapon particles carrying the player's movement; the RPG's fuller launch (backblast, billows, flame flashes), laser dot light and a laser line from its housing, the rocket's flickering motor glow and smoke-puff trail; the sniper rifle's trace with a hot start, bolt smoke, barrel flash and heavy impacts; small touches on the standard weapons (Glock smoke heat, revolver impacts, Desert Eagle barrel wisps, MP5 grenade launch, double-shot blast, melee dust); the hand grenade tumbling and rolling; the snark's death burst; explosion fill light; spent casings (the revolver's and sniper rifle's handed off from their animations, .50 AE and 5.56 shapes, the M249's casing every shot with links, lying along the gun then tumbling, Half-Life's brass on Half-Life's guns, shotgun shells on the pump from the receiver, brass lingering 6-10 s, silent under water); the gluon bend fixed at 0.25; fixes: Desert Eagle and M249 shells thrown from behind the player's head, sniper smoke stopping after a map change |
| `v0.8.0-prealpha` | Monsters and the world: the gargantua's flickering flame lights, embers and heat smoke, its stomp as a wave down a lane, eye light, stomp dust and lingering death plates; tentacle strikes throwing up what they hit by material, slams cratering; the pit worm's lit eye, beam core and scorches; the gene worm's eye lights, lit heart, smacks and acid clouds; the Gonarch's spit dripping white droplets; the ichthyosaur's bite bubbles bursting from its mouth; mounted guns' (`func_tank`) muzzle lights, flashes, smoke and brass, and NPC turrets' flashes and brass; laser turrets' fuller beams and recoil for whoever mans a turret; mortar fields' falling streaks, the Opposing Force mortar's launch and trail; chargers' soft light and sparks running dry; env_shooter pieces lingering 45 s; Opposing Force and Blue Shift view models getting the weapon effects; reload blending (Glock, revolver, Desert Eagle, shotgun); bounce sounds thinned; fixes: the Opposing Force mortar's oversized fireball, floating pieces over breakable floors (c4a2b's Gonarch), gibs stuck in each other, satchel switch sounds, MMod's satchel flap sound |
| `v0.9.0-prealpha` | Sound: Half-Life's own mixing and room effects on the SDK's OpenAL sound system (`jhl_snd_vanilla`), checked against recordings of vanilla: sounds at the engine's 22 kHz, pitched the engine's way (the faint grain of vanilla's sound), its ear volumes (sounds ahead and the player's own full in both ears), clipping rather than a limiter, and its room types in place of the SDK's EAX presets (reverb and echo per room set to vanilla's, water rooms muffling the whole mix); `cl_snd_channels`; fixes: sounds (gunfire, explosions) going silent when OpenAL ran out of sources |
| `v0.9.1-prealpha` | Sound fix: 22 kHz sounds (the guns, many effects) no longer muffled with `jhl_snd_vanilla`: converted to the device's rate flat to 10.5 kHz, as the system converts the engine's output (OpenAL's resamplers rolled off from 6 kHz) |
| `v0.10.0-prealpha` | Visuals: brush entities built flush with walls (signs, posters, panels, gears, rails: about 2,700 faces in Half-Life's maps) found from each map's BSP and drawn just out of the wall, so they no longer flicker in OpenGL or vanish in software (`jhl_visuals_flushfaces`) |
| `v0.11.0-prealpha` | Physics: Half-Life's game code acting at any frame rate as it did at 72 fps, its 1998 `fps_max` (`jhl_physics_fpsfix`): boxes and barrels pushed and pulled at the 72 fps rate, pulling no faster than pushing (it ran away, worse the higher the fps), walking with +use, grenades, satchels, gibs and spores sliding as far as they did, client gibs, debris and shells settling without extra bounces, trigger_camera drift, and guns keeping the time a shot waited (up to a frame was lost each shot) |

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
