# John Half-Life

A mod for Half-Life, Opposing Force and Blue Shift, built on the
[Half-Life Unified SDK](https://github.com/twhl-community/halflife-unified-sdk). It keeps Valve's look and
models and adds reserved changes to the camera, movement, weapon feel and effects; every feature can be
switched off in the console to compare with vanilla.

Status: pre-alpha, the first public release in preparation ([changelog](docs/CHANGELOG.md)).

## Install

Needs Half-Life on Steam, on Windows or Linux. Opposing Force and Blue Shift are optional: their campaigns appear if
you have them. macOS isn't supported (Half-Life itself doesn't run on current macOS).

1. Extract the zip into your Half-Life folder (in Steam: right-click Half-Life → Manage → Browse local files), so
   the `johnhalflife` folders sit next to `valve`.
2. Restart Steam: John Half-Life (and its Opposing Force, Blue Shift and Uplink games) appear in your library.
3. Play. The first start shows a black screen for a few seconds while it sets itself up from your games.

To update, extract the new zip over the old one.

## Principles

* **Reserved.** Each feature is a toned-down change. Vanilla visuals and Valve's models stay.
* **Never worse than vanilla.** Sustained play (accuracy, recoil, readability) is never worse than the
  original game.
* **Vanilla assets first.** New files only where nothing stock fits.
* **Every feature can be switched off.** Each has a `jhl_*` client setting (default 1, saved in
  `config.cfg`), so it can be compared with vanilla in game.

## Settings

All are client settings: in John Settings (below) or the console.

| Setting | Default | Turns on/off | Page |
|---|---|---|---|
| `jhl_camera_bob` | 1 | Dual-rate view bob and figure-eight weapon sway | [Camera](docs/camera_movement.md#view-bob-and-weapon-sway) |
| `jhl_camera_viewroll` | 1 | Stronger strafe roll | [Camera](docs/camera_movement.md#strafe-roll) |
| `jhl_camera_verticallag` | 1 | Eye lift while moving upward | [Camera](docs/camera_movement.md#vertical-lag) |
| `jhl_camera_jumplandjolt` | 1 | Jump and landing view jolts, and a sound for the long jump module's jump | [Camera](docs/camera_movement.md#jump-and-landing-jolts) |
| `jhl_visuals_weaponlag` | 1 | Weapon lags behind when turning | [Camera](docs/camera_movement.md#weapon-lag-and-wall-push) |
| `jhl_visuals_weaponwallpush` | 1 | Weapon pulls back near walls | [Camera](docs/camera_movement.md#weapon-lag-and-wall-push) |
| `jhl_visuals_firstpersonbody` | 1 | Your own body seen looking down: Gordon (in his lab coat before the suit), Barney or Adrian, posed by your movement | [Visuals](docs/visuals.md#first-person-body-jhl_visuals_firstpersonbody-default-1) |
| `jhl_visuals_flushfaces` | 1 | Signs, posters and panels built flush with walls, and doors at rest open or closed, drawn steady (they flickered in OpenGL and vanished in software) | [Visuals](docs/visuals.md) |
| `jhl_visuals_16bitcolor` | 1 | The software renderer's 16-bit color: every frame cut to 565, as it drew | [Visuals](docs/visuals.md#16-bit-color-jhl_visuals_16bitcolor) |
| `jhl_visuals_softwarewater` | 1 | The software renderer's water: its ripple simulation in place of OpenGL's sine wobble, and under water its 320 x 200 view and fog by depth | [Visuals](docs/visuals.md#software-renderer-touches) |
| `jhl_visuals_renderfixes` | 1 | Models lit up to 4/3 as the world is; with texture filtering off, textures point sampled with mipmaps and the sky pixelated as the world is | [Visuals](docs/visuals.md#software-renderer-touches) |
| `jhl_physics_fpsfix` | 1 | Pushables, +use walking, grenades and gibs sliding, debris settling and fire rates acting at any frame rate as at 72 fps | [Physics](docs/physics.md) |
| `jhl_physics_wallfix` | 1 | No extra speed walking at an angle into a wall, or climbing ladders with two keys or at an angle (0: vanilla's wall running and fast climbing) | [Physics](docs/physics.md#no-extra-speed-walking-into-a-wall-or-climbing-jhl_physics_wallfix-default-1) |
| `jhl_input_walkfix` | 1 | Walking (`+speed`) diagonally no faster than straight (vanilla: 1.41 times as fast) | |
| `jhl_input_walktoggle` | 1 | The walk key (`+speed`) toggles walking instead of walking while held; same binding | |
| `jhl_weapons_smoothrecoil` | 1 | Spring-damped recoil (0 = original punches) | [Weapons](docs/weapons.md#recoil) |
| `jhl_weapons_holster` | 1 | Slot 8 holsters: an empty box in the weapon menu that puts the weapon away (`lastinv` takes it out again) | [Weapons](docs/weapons.md#holster) |
| `jhl_fx_bulletimpacts` | 1 | Bullet impact sparks/dust and water splashes | [Weapons](docs/weapons.md#bullet-impacts) |
| `jhl_fx_muzzleflash` | 1 | Muzzle light and smoke, RPG backblast | [Weapons](docs/weapons.md#muzzle-light-and-smoke) |
| `jhl_fx_particlesystem` | 1 | The particle system changes (gibs, blood, explosions, debris, beams, rings) | [Effects](docs/effects.md#gibs-done) |
| `jhl_fx_tracers` | 1 | johnhalflife's tracers (with the particle system on): the M249's tracer rounds, every gun's faint streaks, NPCs' | [Weapons](docs/weapons.md#tracers) |
| `jhl_fx_keepcorpses` | 1 | Corpses vanilla fades a few seconds after death (monsters from a spawner, such as Race X portals and grunt drop-offs, and those the map flags to fade) stay like any other body. Up to 40 at once, the oldest fading past that; a spawner whose spot a kept corpse lies on fades it and spawns anyway | [Effects](docs/effects.md#kept-corpses) |
| `jhl_hud_scale` | 0 | Sprite HUD scale: 0 automatic (Valve's half-size HUD at 800x600 and below, 1 up to 1439 tall, 2 from 1440, 3 from 2160), 0.5 half size, 1, 2, 3... that size | [HUD](#hud-scale) |
| `jhl_hud_minimal` | 1 | A minimal HUD: the weapon menu without its ammo bars (the small green, red when empty, bars under each weapon), and the flashlight icon fading away while it's off and fully charged (back at once when on or charging) | [HUD](#hud-scale) |
| `jhl_hud_campaigncrosshair` | 1 | The crosshair in the campaign's HUD colour: Blue Shift's blue (vanilla Blue Shift kept Half-Life's orange with its blue HUD); Half-Life's, Uplink's and Opposing Force's already match | [HUD](#hud-scale) |
| `jhl_snd_vanilla` | 1 | Half-Life's own mixing (22 kHz, its pitch grain, ear volumes, clipping) and room effects, in place of the SDK's OpenAL sound | [Sound](docs/sound.md) |
| `jhl_snd_musicsaves` | 1 | Loading a save plays the music that was playing when it was saved, from that point (0: loading stops the music, as vanilla) | [Sound](docs/sound.md#music-in-saves) |

Changed defaults of existing settings: `gamma` 1.92 and `brightness` 0 (the engine's are 2.2 and 0, Steam's before
the 25th anniversary update 2.5 and 1; gamma goes from 1.8, darkest and most contrast, to 3.0), `cl_rollangle` 0.65
and `cl_rollspeed` 300 (see
[strafe roll](docs/camera_movement.md#strafe-roll)).

Options → Multiplayer → **John Settings...** lists the settings above plus a few vanilla ones (crosshair
scale, field of view, frame rate limit, vertical sync, raw mouse input), with **Reset all to recommended
settings** (also the `jhl_recommended` command). Every johnhalflife setting is on by default, as recommended;
"Vanilla | Blood and gibs" (`jhl_gore`) stands in for the content lock's four violence settings.

The first launch and the reset also set a few vanilla settings not in the dialog: ones Options → Video already has,
and technical ones only changed by mistake. **Texture filtering off** (`r_prefertexturefiltering 0`: point sampled, as
software drew; with `jhl_visuals_renderfixes`, mipmapped as software was), the original models rather than the HD
ones (`_sethdmodels 0`, Valve's default since the 25th anniversary update; Half-Life's own setting, shared with the
other games), and at the engine's defaults: widescreen field of view (`gl_widescreen_yfov 1`), anisotropic filtering
(`gl_ansio 16`), `gl_overbright`, `gl_use_shaders`, `gl_dither`, `gl_spriteblend`, `gl_fog` (all 1), detail textures
off (`r_detailtextures 0`), `texgamma` 2.0, `lightgamma` 2.5, and prediction (`cl_lw`, `cl_lc` 1).

## Recommended launch options and display

Set in Steam: right-click Half-Life (or the mod), Properties, Launch Options. These are the engine's own, read
once as it starts, before any of the mod's code runs, so the mod can't set them itself.

* **`-particles 8192`**: room for more of the engine's small particles (explosion specks, spark streaks, blood
  specks, tracer particles), which the effects use more of than vanilla. Without it, a chain of big explosions can
  run the engine's pool dry: the console says "not enough free particles", and a few specks don't appear for a
  moment. Harmless either way (nothing breaks; the explosions also thin their own out as they near it:
  [tuning sheet](docs/tuning.md#explosion-particles-clientjhl_explosionscpp-particleshare)); this just keeps them all.
  Modern PCs don't feel it.
* **`-condebug`** (optional, for troubleshooting): writes the console to `qconsole.log` in the game folder as you
  play, so its last lines survive a crash. With the crash's details from Windows' event log, that usually shows
  what happened.

### For the 1998 look: rough, pixelated rendering

The 25th anniversary engine smooths what 1998 drew sharp. For pixels as they were, at a low resolution such as
640 × 480:

* **`-nofbo`**: the monitor itself switches to the chosen resolution, as the software renderer did. Without it,
  OpenGL draws into an off-screen image at that resolution and stretches it to the desktop with smoothing, which is
  most of the blur at low resolutions. The smoothing is hard-coded in the engine's present (it's set again every
  frame), so this launch option is the only way around it. The catch is the mode switch: alt-tab is slow, and in a
  window the game is drawn at its resolution, unscaled. The console shows the mode it picked ("Setting closest
  display mode"); it keeps the desktop's refresh rate where the monitor offers it, and `-freq 144` (for example)
  asks for another.
* **`-nomsaa`**: no anti-aliasing. OpenGL multisamples the view by default, with no setting; software had none.
  With `-nofbo` there's no anti-aliasing either way. With `developer 1`, the console says how many samples the view
  is drawn with.
* **Texture filtering off** (Options → Video): textures point sampled, blocky up close. With
  `jhl_visuals_renderfixes`, mipmapped as software was, so distant textures don't sparkle
  ([Visuals](docs/visuals.md#software-renderer-touches)). On by default, as Half-Life's OpenGL renderer
  always drew.

If the picture's still soft with `-nofbo`, the monitor or graphics card is smoothing the stretch to the screen:
turn on integer scaling in the graphics driver's settings (NVIDIA, AMD and Intel all have it), which shows each
pixel as a sharp block at a whole multiple (640 × 480 is exactly 3× on a 1440p screen, with bars at the sides).
Without `-nofbo`, a resolution the screen shows 1:1 (the desktop's, or 4:3 at its height, such as 1920 × 1440 on a
1440p screen) isn't stretched either.

## HUD scale
`jhl_hud_scale` (0 = automatic by screen height)

The sprite HUD is laid out on a virtual screen (real size divided by the scale) and drawn scaled. Engine
text keeps its size; VGUI and the spectator HUD stay in real pixels. Crosshair size stays on
`crosshair_scale` (Half-Life Updated's), 0 by default: automatic, as the HUD scale, 1 at 600 tall and below, 2
above, 3 from 2160. The crosshair itself is vanilla.

**Half size** (`0.5`; automatic, `0`, at 600 tall and below; and always below 640 wide, as vanilla): Valve's half-size HUD, the "320" sprites
vanilla used at low resolutions, which the Unified SDK left out. Each icon is Valve's own half-size one where the
Unified SDK's icon is Valve's (Opposing Force's in its game, from its own sprites); the Unified SDK's own icons (its
Half-Life-style Opposing Force weapon icons, the game titles) are halved exactly. Crosshairs keep their size. It
changes as the next map loads (`scripts/make_half_hud.py`).

## Games

Each game is its own mod folder with its own name, main menu, New Game / Hazard Course and saves, the way Valve
ships Opposing Force and Blue Shift on top of Half-Life.

| Folder | Name | Start / training map |
|---|---|---|
| `johnhalflife` | John Half-Life | `c0a0` / `t0a0` |
| `johnhalflife_op4` | John Half-Life: Opposing Force | `of0a0` / `ofboot0` |
| `johnhalflife_bs` | John Half-Life: Blue Shift | `ba_tram1` / `ba_hazard1` |
| `johnhalflife_uplink` | John Half-Life: Uplink | `hldemo1` / `t0a0` |

## Versions

The latest is `v0.23.0-alpha`. Every version and what it brought: [changelog](docs/CHANGELOG.md).

## Building from source

Windows (Visual Studio) and Linux (Ubuntu 22.04, or WSL) builds, then one script that makes the zip from both:
[Making a release](docs/release.md#making-a-release). Testing goes the same way, the zip extracted into
the game, so what's played is what players get.

## Documentation

* [How John Half-Life works](docs/README.md): every system it changes and why, the changelog, making a release, and
  notes for working on the code. All of John Half-Life's documentation is in `docs/`.
* [The Half-Life Unified SDK](docs/unified-sdk/UNIFIED_SDK.md): the SDK this is built on, with its own
  documentation, building and installing notes and changelogs, as it ships them, in `docs/unified-sdk/`.

## Credits and licences

Made by John, on Valve's Half-Life (and Gearbox's Opposing Force and Blue Shift), the Half-Life Unified SDK and
Half-Life Updated. Free, not for sale, under Valve's [Half-Life SDK licence](LICENSE); John Half-Life's own sounds
and art are [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). The credits and every licence of what's
shipped: [config/licenses](config/licenses/CREDITS.txt), in the game as `johnhalflife/licenses`.
