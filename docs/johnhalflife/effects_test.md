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
| `grenade` | `dmg=100` | Hand grenade, MP5 grenade, RPG (100); satchel, tripmine (150) | **explosion** `zerogxplode.spr`, scale (dmg−50)×0.06 (3 at 100), fps 15, with the engine's light, particles and sound; **smoke** 0.3 s later, `steam1.spr`, scale (dmg−50)×0.08 (4 at 100), fps 12; **shower** ×0–3; a scorch decal and a debris sound |
| `barrel` | `magnitude=100` | Explosive barrels and crates (`func_breakable`), `func_tank` shells, `env_explosion` | **explosion** scale max(1, (magnitude−50)×0.06), fps 15; **smoke** 0.3 s later at the same scale, fps 12; **shower** ×0–3 |
| `blood` | `dmg=30 type=human\|alien` | Any hit on a monster | **blood_sprite** color 247 (red) or 195 (yellow), size dmg/10 clamped to 3–16; blood decals behind the hit |
| `headshot` | `dmg=30 type=` | A head hit | Identical to `blood`, with the 3× head damage making it bigger. There's no headshot-specific effect yet. |
| `apache` | `time=4` | An Apache crashing, every 0.1 s for the 4 s it falls | **explosion** at a random point ±150 sideways, 50–150 below, scale 3.0–5.9, fps 12; **smoke** at another, scale 10, fps 10; **debris** `metalplategibs_green.mdl`, 4 pieces, box 400×400×132, spread 50, life 3, metal |
| `vortzap` | | A vortigaunt's attack | **beam** `lgtning.spr`, color 180,255,96, brightness 1, noise 20 (0.2 here); **light** radius 80, color 255,180,96, 1 s. The beam's width, CBeam 50, is matched by eye (5 here). |

## Primitives: one building block each

| Primitive | Engine effect | Parameters (defaults) |
|---|---|---|
| `sprite` | Any sprite as a particle (`R_TempSprite`), the general building block | `sprite=sprites/steam1.spr count=1 scale=0.3 alpha=0.35 render=alpha color=255,255,255 fps=24 life=2 loop=0 offset=4 radius=0 speed=0 spread=0 up=0 gravity=0 fade=0 collide=0` |
| `explosion` | The engine explosion (`TE_EXPLOSION`) | `sprite=sprites/zerogxplode.spr scale=3 fps=15 offset=0.6 additive=1 light=1 sound=1 particles=1` |
| `smoke` | The engine smoke puff (`TE_SMOKE`) | `sprite=sprites/steam1.spr scale=4 fps=12 offset=0` |
| `sparks` | The engine spark burst (`TE_SPARKS`); fixed look | `count=1` |
| `streaks` | Spark streaks (`R_SparkStreaks`) | `count=4 vmin=-120 vmax=120 offset=2` |
| `shower` | A flying spark emitter (`spark_shower`) | `count=1 speedmin=200 speedmax=300 lifemin=0.5 lifemax=1.5` |
| `blood_sprite` | The engine blood (`TE_BLOODSPRITE`) | `color=247 size=10 spray=sprites/bloodspray.spr drop=sprites/blood.spr` |
| `bloodstream` | A stream of blood particles (`TE_BLOODSTREAM`) | `color=247 speed=100` |
| `debris` | Model debris (`TE_BREAKMODEL`) | `model=models/metalplategibs.mdl count=8 size=32 speed=100 spread=100 life=3 material=metal` |
| `light` | A dynamic light | `radius=200 color=255,180,96 life=1 decay=0 offset=8` |
| `beam` | A beam from you to the target (`TE_BEAMPOINTS`) | `sprite=sprites/lgtning.spr width=5 noise=0.2 brightness=1 color=180,255,96 life=0.5 speed=0 fps=10` |
| `particles` | The engine's built-in particle effects | `type=burst` (`size=16 color=0 life=1`), `explosion`, `explosion2` (`color=0 length=16`), `blob`, `lava`, `teleport`, `implosion` (`radius=64 count=32 life=1`), `effect` (`color=0 count=32`) |
| `ricochet` | The engine ricochet flash and sound (`TE_ARMOR_RICOCHET`) | `scale=1 sprite=sprites/richo1.spr` |
| `gibs` | The game's gibs | `type=human count=0 velocity=quadruple head=1 sound=1` (`count=0` uses `cl_gibcount`) |

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
