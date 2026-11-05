# Effects test command

`jhl_fx_test <effect> [key=value ...]` plays an effect where you're aiming, on your own screen only (nothing is
sent to the server). It's for designing particle effects: play an event as the game does it now, then try
variations of its building blocks until it looks right. Code: `src/game/client/fx_test.cpp`.

* `jhl_fx_test` alone lists every effect and its parameters.
* Each run prints what it played with **every** parameter, defaults included, so a printed line can be copied,
  edited and run again.
* Parameters left out use the defaults below. Colors are `r,g,b` (0-255).
* Handy: `bind f "jhl_fx_test grenade"`; `cheat_god` or `cheat_notarget` to test in peace.

## Presets: the game's own effects

These reproduce an event with the original values. Each lists its parts, which are the primitives below.

| Preset | Parameters | Event | Parts (original values) |
|---|---|---|---|
| `grenade` | `dmg=100` | Hand grenade, MP5 grenade, RPG (100); satchel, tripmine (150) | **explosion** `zerogxplode.spr`, scale (dmg−50)×0.06 (3 at 100), fps 15, with the engine's light, particles and sound; **smoke** 0.3 s later, `steam1.spr`, scale (dmg−50)×0.08 (4 at 100), fps 12; both (dmg−24)×0.6 off the surface (46 at 100), as vanilla; **shower** ×0–3; a scorch decal and a debris sound |
| `barrel` | `magnitude=100` | Explosive barrels and crates (`func_breakable`), `func_tank` shells, `env_explosion` | **explosion** scale max(1, (magnitude−50)×0.06), fps 15; **smoke** 0.3 s later at the same scale, fps 12; both (magnitude−24)×0.6 off the surface; **shower** ×0–3 |
| `blood` | `dmg=30 type=human\|alien` | Any hit on a monster | **blood_sprite** color 247 (red) or 195 (yellow), size dmg/10 clamped to 3–16; blood decals behind the hit |
| `headshot` | `dmg=30 type=` | A head hit | Identical to `blood`, with the 3× head damage making it bigger. There's no headshot-specific effect yet. |
| `sniperhit` | `type=human\|alien` | An M40A1 hit on something that bleeds | **bloodstream** color 70 (yellow: 195), speed 150, aimed back at the shooter with the **full distance** as its direction, which stretches it into a long line; the normal hit blood (size 10 for the sniper's 100 damage) |
| `apache` | `time=4` | An Apache crashing, every 0.1 s for the 4 s it falls | **explosion** at a random point ±150 sideways, 50–150 below, scale 3.0–5.9, fps 12; **smoke** at another, scale 10, fps 10; **debris** `metalplategibs_green.mdl`, 4 pieces, box 400×400×132, spread 50, life 3, metal |
| `vortzap` | | A vortigaunt's attack | **beam** `lgtning.spr`, color 180,255,96, brightness 1, noise 20 (0.2 here); **light** radius 80, color 255,180,96, 1 s. The beam's width, CBeam 50, is matched by eye (5 here). |
| `houndeye` | `squad=1` | A houndeye's blast | Two **rings** (cylinder) `shockwave.spr`, radius 384 and 192, 0.2 s, width 1.6, 16 up; color by squad size 1-4: 188,220,255 / 101,133,221 / 67,85,255 / 62,33,211. |
| `heliring` | `type=apache` | A helicopter's crash ring | **ring** (cylinder) `white.spr` (Osprey `rope.spr`), radius 800, 0.4 s, width 3.2, color 255,255,192, brightness 0.5. |
| `dispring` | | The displacer ball's impact ring | **ring** (cylinder) `disp_ring.spr`, radius 240, 0.3 s, width 1.6, white. |

## Primitives: one building block each

| Primitive | Engine effect | Parameters (defaults) |
|---|---|---|
| `sprite` | Any sprite as a particle (`R_TempSprite`), the general building block | `sprite=sprites/steam1.spr count=1 scale=0.3 alpha=0.35 render=alpha color=255,255,255 fps=24 life=2 loop=0 offset=4 radius=0 speed=0 spread=0 up=0 gravity=0 fade=0 collide=0` |
| `explosion` | The engine explosion (`TE_EXPLOSION`) | `sprite=sprites/zerogxplode.spr scale=3 fps=15 offset=45.6 additive=1 light=1 sound=1 particles=1` (the sprite must be one the map loaded; preview others with `sprite render=add alpha=0.7`) |
| `smoke` | The engine smoke puff (`TE_SMOKE`) | `sprite=sprites/steam1.spr scale=4 fps=12 offset=45.6` |
| `sparks` | The engine spark burst (`TE_SPARKS`); fixed look | `count=1` |
| `streaks` | Spark streaks (`R_SparkStreaks`) | `count=4 vmin=-120 vmax=120 offset=2` |
| `shower` | A flying spark emitter (`spark_shower`) | `count=1 speedmin=200 speedmax=300 lifemin=0.5 lifemax=1.5` |
| `blood_sprite` | The engine blood (`TE_BLOODSPRITE`) | `color=247 size=10 spray=sprites/bloodspray.spr drop=sprites/blood.spr` |
| `bloodstream` | A stream of blood particles (`TE_BLOODSTREAM`) | `color=70 speed=100 dir=normal length=1` (`dir=player` aims back at you, `up`; `length=dist` uses the distance to you) |
| `debris` | Model debris (`TE_BREAKMODEL`) | `model=models/metalplategibs.mdl count=8 size=32 speed=100 spread=100 life=3 material=metal` |
| `light` | A dynamic light | `radius=200 color=255,180,96 life=1 decay=0 offset=8` |
| `beam` | A beam from you to the target (`TE_BEAMPOINTS`) | `sprite=sprites/lgtning.spr width=5 noise=0.2 brightness=1 color=180,255,96 life=0.5 speed=0 fps=10` |
| `ring` | A ring around the target, growing to its radius over its life (`TE_BEAMCYLINDER`, `TE_BEAMTORUS`, `TE_BEAMDISK`); a sprite the map didn't load falls back to `laserbeam.spr` | `type=cylinder sprite=sprites/shockwave.spr radius=384 life=0.2 width=1.6 noise=0 brightness=1 color=188,220,255 fps=0 speed=0 offset=16 axis=up` |
| `particles` | The engine's built-in particle effects | `type=burst` (`size=16 color=0 life=1`), `explosion`, `explosion2` (`color=0 length=16`), `blob`, `lava`, `teleport`, `implosion` (`radius=64 count=32 life=1`), `effect` (`color=0 count=32`) |
| `ricochet` | The engine ricochet flash and sound (`TE_ARMOR_RICOCHET`) | `scale=1 sprite=sprites/richo1.spr` |
| `gibs` | The game's gibs | `type=human count=0 velocity=quadruple head=1 sound=1` (`count=0` uses `cl_gibcount`) |

## Recipes: blood as the game does it

Hit blood is one `blood_sprite`, sized by the hit's damage ÷ 10, clamped to 3–16 (× 3 for a head hit). Red is
palette color 247, yellow alien blood 195.

| Hit | Command |
|---|---|
| Glock (8), MP5 (5), shotgun pellet (5), crowbar (10) | `jhl_fx_test blood_sprite size=3` |
| .357 (40) | `jhl_fx_test blood_sprite size=4` |
| Desert Eagle (34), M249 (15) | `jhl_fx_test blood_sprite size=3` |
| Sniper rifle (100) | `jhl_fx_test blood_sprite size=10` |
| .357 headshot (120) | `jhl_fx_test blood_sprite size=12` |
| Alien blood | add `color=195` |

The **line of blood** is a different effect, a `bloodstream`, which the game only uses in a few places:

| Where | Command |
|---|---|
| Sniper rifle hit (`sniperhit` preset) | `jhl_fx_test bloodstream color=70 speed=150 dir=player length=dist` |
| A spurt out of a surface (`env_blood` in maps) | `jhl_fx_test bloodstream color=70 speed=100` |
| Upward fountain | `jhl_fx_test bloodstream color=70 speed=100 dir=up` |

Two things make a stream look right:

* **Color 70 for red.** A stream picks colors from `color` to `color`+9, so the blood sprite's 247 runs off the
  end of the palette; the game uses 70 for red streams.
* **Direction length × speed is the particle speed.** The engine doesn't normalize the direction. With length 1,
  speed is roughly how far it arcs; the sniper rifle passes the whole distance to the shooter, which turns the
  stream into a fast line.

## What each parameter does

| Parameter | Meaning |
|---|---|
| `sprite`, `model` | Any `.spr` or `.mdl` path; loaded if the map didn't precache it |
| `scale` | Size multiplier (a sprite's pixel size × scale, in game units) |
| `alpha` | Opacity, 0–1 (`sprite`) |
| `render` | `alpha`: translucent, darkens and lightens; `add`: additive, glows, black is invisible; `glow`: additive, same size at any distance; `normal`: opaque |
| `color` | Tint, multiplied with the sprite's own colors |
| `fps` | Animation speed in frames per second; with `loop=0` the sprite plays once and disappears on its last frame, so fps also sets how long it shows |
| `life` | Maximum time in seconds (`loop=1` keeps animating until then) |
| `fade` | Fades out toward the end of `life`; higher fades faster |
| `count` | How many |
| `offset` | Distance off the surface |
| `radius` | Random scatter of starting positions |
| `speed` | Speed straight out of the surface (units per second) |
| `spread` | Random extra speed in every direction |
| `up` | Extra upward speed (smoke rises with ~10–30) |
| `gravity` | 0: floats; 1: falls slowly; 2: falls |
| `collide` | Bounces off the world |
| `vmin`, `vmax` | Spark speed range |
| Particle `color` | An index into the game palette (0–255), not r,g,b: 0–15 is a gray ramp, 247 is blood red, 195 blood yellow |
| `width`, `noise`, `brightness`, `speed` (beam) | Thickness, jaggedness, opacity 0–1, texture scroll speed |
| `radius`, `decay` (light) | Reach in units; how fast the radius shrinks per second |

Stock sprites worth trying (all in Half-Life's `sprites/`): `steam1.spr` (soft smoke), `smoke.spr`, `xsmoke1.spr`,
`zerogxplode.spr` (fireball), `explode1.spr`, `fexplo.spr`, `fexplo1.spr`, `eexplo.spr`, `WXplo1.spr` (underwater
explosion), `richo1.spr` (flash), `glow01.spr`, `hotglow.spr`, `flare1.spr`, `animglow01.spr`, `xspark1.spr`,
`xspark4.spr`, `muzzleflash1.spr`, `wsplash3.spr`, `bloodspray.spr`, `blood.spr`, `bigspit.spr`, `gargeye1.spr`,
`exit1.spr`. Models: `metalplategibs.mdl`, `woodgibs.mdl`, `glassgibs.mdl`, `cindergibs.mdl`, `rockgibs.mdl`,
`fleshgibs.mdl`, `computergibs.mdl`.
