# John Half-Life

A mod for Half-Life, Opposing Force and Blue Shift, built on the [Half-Life Unified SDK](https://github.com/twhl-community/halflife-unified-sdk).

It keeps Half-Life's original look and makes careful changes to how the game feels.
Every change has its own setting and can be turned off.
Multiplayer included but not fully supported.

## What it changes

* **Camera:** improved viewmodels and camera movement.
* **Weapons:** smoother recoil, new weapon draw sounds, and a new holster-weapon slot.
* **Effects:** bullet impacts, muzzle light and smoke, tracers, explosions, gibs, blood, sparks, and beams.
* **First-person body:** your own body and arms when you look down.
* **Water:** the software renderer's ripples with OpenGL's waves, darker in dark corners.
* **Rendering:** 1998 software look on OpenGL (16-bit color, model lighting, skybox filtering, etc.).
* **Sound:** Half-Life's original mixing and room effects in OpenAL; saves keep the music.
* **Physics:** decoupled from fps; movement speed-boost bugs fixed.
* **Fixes:** vanilla bugs like z-fighting, broken sprites and engine crashes.

## Install

Requires Half-Life, Opposing Force and Blue Shift on Steam.

1. Extract the zip into your Half-Life folder.
2. Restart Steam.

## Principles

* Faithful to the original WON experience.
* Modernize subtly and cohesively.
* Every change can be turned off.

## Games

| Folder | Game |
|---|---|
| `johnhalflife` | John Half-Life |
| `johnhalflife_op4` | John Half-Life: Opposing Force |
| `johnhalflife_bs` | John Half-Life: Blue Shift |
| `johnhalflife_uplink` | John Half-Life: Uplink |

## Recommended launch options

In Steam: right-click the game → Properties → Launch Options.

* `-particles 8192`: room for the extra explosion particles.

For the rough 1998 look at low resolutions:

* `-nofbo`: real low-resolution video mode, no blur.
* `-nomsaa`: no anti-aliasing.

## Settings

Options → Multiplayer → **John Settings...** lists them all, with a reset to recommended. All default to on.

| Setting | What it does |
|---|---|
| `jhl_camera_bob` | View bob and weapon sway |
| `jhl_camera_viewroll` | Stronger strafe roll |
| `jhl_camera_verticallag` | Eyes lift slightly moving up |
| `jhl_camera_jumplandjolt` | Jump and landing jolts |
| `jhl_visuals_weaponlag` | Weapon lags behind when turning |
| `jhl_visuals_weaponwallpush` | Weapon pulls back near walls |
| `jhl_visuals_firstpersonbody` | Your own body when you look down |
| `jhl_visuals_flushfaces` | Fixes vanilla brush z-fighting |
| `jhl_visuals_16bitcolor` | Software renderer's 16-bit color |
| `jhl_visuals_softwarewater` | Software renderer's water |
| `jhl_visuals_waterlight` | Water darker in dark corners |
| `jhl_visuals_wateropacity` | Makes solid water entities slightly transparent |
| `jhl_visuals_renderfixes` | Model lighting and sky texture fixes |
| `jhl_physics_fpsfix` | Decouples physics from fps |
| `jhl_physics_wallfix` | Prevents extra speed on walls and ladders |
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
| `jhl_snd_vanilla` | Half-Life's original sound mixing |
| `jhl_snd_musicsaves` | Saves keep the music state |
| `jhl_perf_preloadsounds` | Load all sounds with the map (default off) |

## Documentation

* [docs/](docs/README.md): detailed docs, building, releasing.
* [docs/unified-sdk/](docs/unified-sdk/UNIFIED_SDK.md): the Unified SDK's own documentation.

## AI Usage Disclaimer

Most code, reverse engineering and documentation written with AI assistance (Anthropic's Claude).
Directed, reviewed and tested by John.

## Credits and licences

Made by John, on Valve's Half-Life, Gearbox's Opposing Force and Blue Shift, the Half-Life Unified SDK and
Half-Life Updated.
Free, not for sale, under the [Half-Life SDK licence](LICENSE). John Half-Life's own sounds and art:
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
Full credits and licences: [config/licenses](config/licenses/CREDITS.txt) (in the game: `johnhalflife/licenses`).
