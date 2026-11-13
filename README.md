# John Half-Life

A mod for Half-Life, Opposing Force and Blue Shift, built on the
[Half-Life Unified SDK](https://github.com/twhl-community/halflife-unified-sdk).
It keeps Valve's look and models and makes small, careful changes to how the game feels.
Every change has its own setting and can be turned off to compare with vanilla.

Status: alpha ([changelog](docs/CHANGELOG.md)).

## What it changes

* **Camera:** smoother view bob, strafe roll, jump and landing jolts, weapon lag.
* **Weapons:** spring-damped recoil, tighter first shots, draw and reload sounds, a holster slot.
* **Effects:** bullet impacts, muzzle light and smoke, tracers, explosions, gibs, blood, sparks, beams.
* **First-person body:** your own body when you look down.
* **Water:** the software renderer's ripples, OpenGL's waves, darker in dark corners.
* **Software look:** the 1998 software renderer's look on OpenGL (16-bit color, model lighting).
* **Sound:** Half-Life's own mixing and room effects; music kept in saves.
* **Physics:** the same at any frame rate; no wall-running speed boost.
* **Fixes:** flickering signs on walls, engine crashes and hangs in long sessions.

## Install

1. Extract the zip into your Half-Life folder.
2. Restart Steam.

Opposing Force and Blue Shift appear if you own them. The first start shows a black screen for a few seconds.

## Principles

* Vanilla first: Valve's look, models and assets.
* Never worse than vanilla.
* Every change can be turned off.

## Recommended launch options

In Steam: right-click the game → Properties → Launch Options.

* `-particles 8192`: room for the extra explosion particles.

For the rough 1998 look at low resolutions:

* `-nofbo`: real low-resolution video mode, no blur.
* `-nomsaa`: no anti-aliasing.
* Texture filtering off (Options → Video).

## Settings

Options → Multiplayer → **John Settings...** lists them all, with a reset to recommended. All default to on.

| Setting | What it does |
|---|---|
| `jhl_camera_bob` | View bob and figure-eight weapon sway |
| `jhl_camera_viewroll` | Stronger strafe roll |
| `jhl_camera_verticallag` | Eyes lift slightly moving up |
| `jhl_camera_jumplandjolt` | Jump and landing jolts |
| `jhl_visuals_weaponlag` | Weapon lags behind when turning |
| `jhl_visuals_weaponwallpush` | Weapon pulls back near walls |
| `jhl_visuals_firstpersonbody` | Your own body when you look down |
| `jhl_visuals_flushfaces` | Signs and panels on walls drawn steady |
| `jhl_visuals_16bitcolor` | Software renderer's 16-bit color |
| `jhl_visuals_softwarewater` | Software renderer's water |
| `jhl_visuals_waterlight` | Water darker in dark corners |
| `jhl_visuals_wateropacity` | Water entities' opacity (0.85; 1 is solid) |
| `jhl_visuals_renderfixes` | Models lit like the world; sharp sky |
| `jhl_physics_fpsfix` | Physics the same at any frame rate |
| `jhl_physics_wallfix` | No extra speed along walls or on ladders |
| `jhl_input_walkfix` | Walking diagonally no faster |
| `jhl_input_walktoggle` | Walk key toggles |
| `jhl_weapons_smoothrecoil` | Spring-damped recoil |
| `jhl_weapons_holster` | Slot 8 holsters the weapon |
| `jhl_fx_bulletimpacts` | Impact sparks, dust and splashes |
| `jhl_fx_muzzleflash` | Muzzle light and smoke |
| `jhl_fx_particlesystem` | All other effect changes |
| `jhl_fx_tracers` | Tracer streaks |
| `jhl_fx_keepcorpses` | Corpses stay instead of fading |
| `jhl_hud_scale` | HUD size (0: automatic) |
| `jhl_hud_minimal` | Minimal weapon menu and flashlight icon |
| `jhl_hud_campaigncrosshair` | Crosshair in the campaign's HUD color |
| `jhl_snd_vanilla` | Half-Life's own sound mixing |
| `jhl_snd_musicsaves` | Saves keep the music |
| `jhl_perf_preloadsounds` | Load all sounds with the map (default off) |

## Games

| Folder | Game |
|---|---|
| `johnhalflife` | John Half-Life |
| `johnhalflife_op4` | John Half-Life: Opposing Force |
| `johnhalflife_bs` | John Half-Life: Blue Shift |
| `johnhalflife_uplink` | John Half-Life: Uplink |

## Documentation

* [docs/](docs/README.md): how each system works, building, releasing.
* [docs/unified-sdk/](docs/unified-sdk/UNIFIED_SDK.md): the Unified SDK's own documentation.

## Credits and licences

Made by John, on Valve's Half-Life, Gearbox's Opposing Force and Blue Shift, the Half-Life Unified SDK and
Half-Life Updated. Free, not for sale, under the [Half-Life SDK licence](LICENSE). John Half-Life's own sounds and
art: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Full credits and licences:
[config/licenses](config/licenses/CREDITS.txt) (in the game: `johnhalflife/licenses`).
