# Effects plan

A map of the game's visual effects, used to work through them one category at a time. Each category gets the
same treatment the bullet impacts did: look at what MMod does (`re/specs/`), decide a reserved version, build
it, then tune it in game.

## How effects reach the screen

Most world effects are **temp entities**: the server sends a `TE_*` message (explosion sprite, smoke, sparks,
blood sprite, beams, model debris...) and the engine draws it. The engine owns that drawing, so the client
can't change a temp entity after it arrives. That leaves three levers:

| Lever | What it is | Per-player toggle | Network | Effort |
|---|---|---|---|---|
| **Tune** | Change the parameters the server already sends (count, scale, lifetime, sprite) | No: everyone sees the server's choice | None new | Trivial |
| **Add on the server** | Send extra `TE_*` messages (a second smoke puff, a light, sparks) | No | More of the existing kinds | Easy |
| **Client enrichment** | Send one small user message ("explosion here, this big"); the client builds the effect from the effects API (`weapon_effects.cpp` style) | Yes (`jhl_fx_*`) | One new small message, flag it | Medium |

Effects of the local player's own weapons are already client-side events (muzzle effects, impacts).

**Default choice:** client enrichment for the shared paths below, all behind **one** global toggle,
`jhl_fx_particlesystem` (the user's choice over one per theme; created with the first effect that uses it), with
richer combinations; tune or add on the server for one-off set pieces. Vanilla sprites and models only, as with
the impacts: more, longer or layered particles, short lights, reused stock sprites. MMod assets only where
nothing stock fits, through `mmod_port`.

## Shared paths (change once, applies everywhere)

The most effective changes are at the few functions everything calls:

| Path | Where | Used by |
|---|---|---|
| `SpawnBlood`, `TraceBleed`, `UTIL_BloodDrips`, `UTIL_BloodStream` | `CBaseMonster::TraceAttack` (`combat.cpp`) and ~10 monster files | Every monster hit |
| Hit group switch (`HITGROUP_HEAD` ...) | `CBaseMonster::TraceAttack`; some monsters remap it (grunt helmets, Barney) | Headshots, limb hits |
| `UTIL_ExplosionEffect` (`TE_EXPLOSION`, fireball or underwater sprite) + `CGrenade::Smoke` | `util.cpp`, `ggrenade.cpp` | Grenades, rockets, satchels, tripmines, mortars |
| `ExplosionCreate` / `env_explosion` (fireball, smoke, `spark_shower`) | `explode.cpp` | Explosive barrels and crates (`func_breakable`), `func_tank`, scripted explosions |
| `GibMonster`, `CGib::SpawnRandomGibs`, client gibs (`client_gibs.cpp`) | 19 monster files | Gibbing |
| `UTIL_Sparks`, `UTIL_Ricochet`, `spark_shower` | 12 / 14 files | Sparks, ricochets, electrical |
| `TE_BREAKMODEL` | `func_break.cpp`, set pieces | Breakables, vehicle debris |

## Categories

| # | Theme | Examples | Mechanism | Difficulty | MMod reference |
|---|---|---|---|---|---|
| 0 | **Done** | Bullet impacts, water splashes, muzzle light and smoke, RPG backblast | Client events, `BulletHit`, `WaterSplash` | — | `bullet_impacts.md` |
| 1 | **Explosions and fire** | Grenades, RPG, satchel, tripmine, mortars, explosive barrels, `env_explosion` | Two shared paths (`UTIL_ExplosionEffect`, `ExplosionCreate`) | Low to medium | Explosion style, fiery trails |
| 2 | **Blood and wounds** | Hits, headshots, bleeding | `SpawnBlood`/`TraceBleed` in `TraceAttack`; hit group | Low to medium | Alpha blood sprites, blood style, corpse blood pools |
| 3 | **Gibs and death** | Gibbing, corpses | `GibMonster`, client gibs | Medium | Gib bounce sounds, gib blood pools, corpse fade/flinch |
| 4 | **Sparks and debris** | Breakables, electrical sparks, ricochets | `UTIL_Sparks`, `spark_shower`, `TE_BREAKMODEL` | Low | — |
| 5 | **Energy and alien attacks** | Vortigaunt zap, controller, houndeye, gargantua flame, displacer, shock rifle, spore | Per-monster beams and lights (`TE_BEAM*`, `TE_DLIGHT`, `TE_ELIGHT`) | Medium, one file each | — |
| 6 | **Set pieces** | Apache and Osprey destruction, gargantua death, Nihilanth | Per-entity sequences (explosions, smoke, `TE_BREAKMODEL`); partly inherit from 1 and 4 | Medium, one file each | — |
| 7 | **Water and environment** | Bubbles, underwater explosions | `UTIL_Bubbles`, `UTIL_BubbleTrail` | Low | Water impacts (done) |

Suggested order: **1 → 2 → 3 → 4 → 5 → 6 → 7**. Explosions first because one change reaches grenades, rockets,
barrels and part of every set piece; blood second because it's on screen the most; set pieces late because
they inherit from the shared work.

## Gibs (done)

`jhl_fx_particlesystem`, client side (`client_gibs.cpp`). How gibbing works:

* Every NPC dies through `Killed` → `GibMonster`. The shared version picks human or alien gibs (each NPC class
  overrides `HasHumanGibs`/`HasAlienGibs`), adds a skull for humans and sends one `ClientGibs` message; each
  client spawns the gibs itself. Grunts, assassins, grunt allies and controllers do extra work and then call it;
  the pit drone (6), shock trooper (6) and voltigore (12) send their own model and count; the barnacle 1; the
  Apache, Osprey, Nihilanth and penguin throw no flesh gibs.
* Gib sets: human `hgibs.mdl` (5 random pieces, submodels 1-5, plus the skull), alien `agibs.mdl` (4), pit drone
  (7), shock trooper (8), voltigore (9 with per-piece limits, 12 in all).
* Count `gibcount` (4) for the shared path, life on the ground `giblife` (25 s), speed `gibvelscale` (1): client
  cvars.
* When: below −30 health from explosions, melee, crushing and falls; always from the displacer, shock rifle,
  spores, houndeye blast, tail whips, the barnacle and NPC bullets over 16 damage; never from the player's
  bullets, hornets or the shock roach; corpses gib from accumulated explosion, melee and crush damage. Speed by
  overkill: 0.7× above −50, 2× above −200, 4× below. The player's bullets never gibbing stays: there's nothing
  between a corpse and gibs.

Changes:

* Count × 7 for death gibs (a barnacle's single bite gib stays one), with each piece used at most 3 times per
  death: in practice the cap decides, 15 human pieces plus the skull, 12 alien, 21 pit drone, 24 shock trooper;
  voltigore limits kept. Per-model exceptions (`GibProfiles`, looked up from the dying monster's model on the
  client): large aliens (alien grunt, vortigaunt, bullsquid, controller, zombies, gonome, Big Momma,
  ichthyosaur) 4 uses per piece, 16 gibs; headcrab the vanilla 4; baby crab 1 half-size gib; rat 2 gibs at a
  third of the size, no skull (it has human gibs); barnacle 2 per gib it coughs up (8 as it dies: 4 events in its
  death animation). Each gib still leaves up to 5 blood decals as it bounces.
* 60 s on the ground before fading, instead of `giblife`.
* Each gib ±15% size and ±20% speed, and a wider random sideways direction (±0.7 instead of ±0.25 per
  horizontal axis), so gibs fan out instead of flying one way when a blast is level with the body. Vanilla's model renderer ignores scale; `StudioSetUpTransform` now applies it
  to entities marked with `ScaledModelMarker` in `iuser4` only, so map models with a scale keyvalue are unchanged.
* Bounce sounds: 4 gibs per death squelch (`debris/flesh*.wav`, through the SDK's temp entity bounce sounds:
  louder the harder they land, random pitch) on their first 3 bounces only, so at most 12 sounds over the first
  second or so instead of dozens.

## Blood (done)

* **Hit blood** (server: `UTIL_BloodDrips` in `util.cpp`, `CBaseMonster::TraceAttack` and `Killed` in
  `combat.cpp`, constants in `jhl_effects.h`). Server effects, so the toggle is read from the client's
  `jhl_fx_particlesystem` (shared cvars in single player; a dedicated server always uses them). Vanilla: one
  `TE_BLOODSPRITE` (`bloodspray.spr` puff + 14 `blood.spr` droplets) sized damage/10 clamped to 3-16, so nearly
  every gun hit sat at 3 (a Desert Eagle body shot puffed like an MP5's); only the head multiplier (3x) varies.
  * Size `1 + sqrt(damage)`, never below vanilla, capped at 16: MP5 3, Glock 4, crowbar 4, M249 5, wrench 6,
    Desert Eagle 7, .357 7, sniper 11; headshots 5 (MP5) to 16.
  * Colors 74 (red) and 198 (yellow), like the gib splash, instead of 247/195 and their bright neighbours.
  * Every blood stream (ours, the sniper rifle's, `env_blood`, Nihilanth, pit worm) goes through
    `UTIL_JhlBloodStreamColor`: red 68, yellow 198. Colors in use, all from dark ramps: hit and splash puffs
    74/198 (sprites: the tint multiplies a shaded texture, so they render darker than their index), streams 68/198
    and gib trails 68/200 (particles: flat squares of one palette color at full brightness, so they start 2 darker
    than vanilla's red streams at 70 to look alike), chunks baked red, decals unchanged.
  * A second burst at 0.6x size on every hit: the engine's droplet count is fixed at 14, so this adds 14 smaller,
    shorter-flying ones (its puff hides in the first).
  * Hits of 30+ damage (after the head multiplier: Desert Eagle, .357, sniper, heavier headshots) spurt a short
    blood stream out the far side along the bullet's path (length 1.5, speed 100).
  * A killing bullet headshot streams back toward the shooter (length 4, speed 150): `TraceAttack` remembers the
    last bullet headshot and `Killed` checks it was this monster in this frame, so non-fatal headshots and later
    explosion kills don't. Not for the sniper rifle, which has its own stream.
  * The player's melee hits (`DMG_CLUB` from a player: crowbar, wrench, knife): 2 more blood bursts at half the
    damage (each 14+14 droplets, so 84 instead of 28) and a short stream flung to a random side and slightly up
    (length 1.2, speed 90), since a swing throws blood across rather than through.
* **Gib splash** (`client_gibs.cpp`, `SpawnGibSplash`): vanilla draws nothing where a gibbed body was (and
  explosion damage skips the hit blood), so the body seemed to vanish. Now, on each death that gibs: 3 of the
  game's hit blood puffs (`R_BloodSprite`, size 16, the game's maximum) spread over a body-shaped area (±8
  sideways, −12 to +18 vertically), the extra 2 over the first 0.15 s, in darker colors than the game's (red 74,
  yellow 198: effects use a few palette entries from the one given, and 247 is followed by bright reds while 195
  is the brightest yellow); and 30 of Opposing Force's `blood_chnk.spr` (scale 0.4, 90% opaque, 1-2.7 fps at random so they last 1.5-4 s, gravity, colliding,
  random roll) from points in the same area, thrown at ±80 plus 60 up and 40 the way the gibs fly. Alien blood uses
  `blood_chnk_yellow.spr`, the same sprite with its palette recolored to mustard yellow (brightest 160,135,40) by
  `scripts/make_yellow_blood_chunk.py` and committed in `config/mod/sprites`. Sized by the gib profile's `Splash` (headcrab 0.5, baby crab and rat 0.25, large aliens 1.25);
  a barnacle's single coughed-up gibs don't splash. Both sprites are precached in `W_Precache`.
* **Blood pool** (`SpawnBloodPool`, as MMod's gibbing blood pools): a decal on the floor under a gibbed body,
  traced straight down (floors only, up to 256 units): `{bigblood1`/`{bigblood2` (96/80 px) for red, three
  overlapping `{yblood1` for yellow (its largest is 48 px); `{blood5`/`{yblood1` at splash 0.5 (headcrab),
  `{blood7`/`{yblood2` below (rat, baby crab).
  Every other death too: a bleeding NPC that dies without gibbing sends `BloodPool` (entity, blood color) from
  `CBaseMonster::Killed`, and the client puts the pool under wherever the body is 1 s later, once it has fallen,
  sized by the same gib profiles.
* The palette's reds are all pure (R,0,0), so blood colors can't be desaturated by index; the chunks are 90% opaque
  instead. The puffs' sprite is made inside `R_BloodSprite`, out of reach; desaturating them would mean building
  them from tinted sprites.
* **Gib blood trails** (`client_gibs.cpp`, `EmitBloodTrail`): 7 gibs per death drop blood particles along their
  path for the first second after being thrown, a drop every 4 units at first and thinning out. Each drop is
  an engine particle (`R_AllocParticle`, `pt_grav`) in the blood stream's colors (red palette 68+, yellow 200+),
  carrying 15% of the gib's speed, lasting 0.6-1 s. Calling `R_BloodStream` every frame would emit 100+ particles
  each time, so the trail is built from single drops instead. Respects `violence_hblood`/`violence_ablood`. Engine particles have one fixed
  size (no size field), so the drops can't grow or shrink; small shrinking `blood.spr` sprites would be the way.

## Debris (gibs and shards)

The engine has 500 temp entities for every effect (both engines: "Overflow %d temporary ents!"); when they're
gone, new effects (shells, smoke, sparks, gibs) silently don't appear. Debris that lasts `JhlDebrisLife` (60 s;
`jhl_debris.cpp`) counts against a shared budget of 250: past it, the oldest pieces fade out early. NPC gibs and
the shards below are tracked (a serial in `curstate.iuser3`; the engine clears temp entities when it reuses them).

Shards (`TE_BREAKMODEL`): the engine makes `count` (0: (x·y + y·z + x·z) / 432 of the size, at most 100) at random
points in the box, flying `velocity` ± `random` × 10 (up only: 0 to that), half gravity, colliding, fading, 200 in
256 tumbling, glass/`BREAK_TRANS` translucent at 128, living `life` / 10 + 0-1 s. `UTIL_JhlBreakModel` sends it
through the SDK's own temp entity message instead with `jhl_fx_particlesystem` on, plus a count multiplier, and
the client (`JhlDebris_ParseBreakModel`) makes the same shards for 60 s, up to 200:

| Debris | Vanilla | Now |
|---|---|---|
| Explosive breakables (barrels, crates, pushables; their own magnitude, or firing an `env_explosion` directly or through up to 2 relays (`multi_manager`, `trigger_relay`), as most of Half-Life's barrels do: 135 directly, 89 through relays, c2a5's all through a `multi_manager`) | by size (a barrel ~9), ±100 sideways, 0-100 up, 2.5 s | 2.5× by size (a barrel ~20), at most 40, ±350 sideways, 150-500 up, 60 s |
| Apache falling, every 0.2 s | 4, 3 s | 4-8, 60 s |
| Apache crash | 200 asked, 100 made (engine cap), 20 s | 200, 60 s |
| Osprey falling, every 0.2 s | 8, 20 s | 8-16, 60 s |
| Osprey crash | 128 asked, 100 made, 20 s | 128, 60 s |
| Glass | by size, 2.5 s | 1.5× by size, at most 60, 60 s |
| Every other breakable (wood, metal, computers, cabinets, signs, vents, tiles...) | by size, 2.5 s | as many, 60 s; of wood, crates whose gibs are their contents (`mil_`, `med_`, `tech_crategibs`, which have no wood) also throw `woodgibs`, half as many (4-16) |
| Computers | by size, 2.5 s | as other breakables, plus 2-3 of the explosions' spark showers going up, living half as long (`UTIL_JhlSparkShowers`: TE_SPARKS through the SDK's message, position and count) |
| NPC gibs | (see Gibs) | unchanged: 7×, 60 s, now in the budget |

**Surface debris** (`SurfaceDebris`, `jhl_explosions.cpp`): every two-fireball explosion that hit a surface (not
in water or mid-air) throws pieces of it, from where it hit: 6-18 for a grenade, up to twice that for bigger
explosions (scale / 3). Each piece picks its own model (from the material's), size (the model's scale ±15%),
speed (100-500, skewed to moderate), direction (as sparks: scatter 0.9, never into the surface), 0-200 up
(down off a ceiling), gravity (full 75%, half 25%), tumbling, and life (8-16 s, then fades), counted against the
debris budget. Models are loaded by the client if the map didn't precache them (`JhlDebris_LoadModel`), so
they don't use the map's model precache slots. Base scales make a typical piece about 10 units:

| Material | Models (scale) | Bounce sounds |
|---|---|---|
| Concrete, unknown (brush entities) | `concretegibs` (1.1); not `cindergibs`, whose cinder blocks don't fit roads and plain concrete | concrete |
| Dirt, Xen ground and shell | `rockgibs` (1.6) | concrete |
| Metal | `metalplategibs` (0.45), `metalgibs` (0.7), `chromegibs` (0.8) | metal |
| Vent | `ventgibs` (0.55) | metal |
| Grate | `metalgibs` (0.7), `catwalkgibs` (0.35) | metal |
| Computer | `computergibs` (0.45), `metalgibs` (0.7) | metal |
| Tile | `ceilinggibs` (0.85), `concretegibs` (1.1) | concrete |
| Wood, swamp boards | `woodgibs` (0.35) | wood |
| Glass | `glassgibs` (0.5), translucent | glass |
| Flesh, organic | `fleshgibs` (0.65) | flesh |
| Water, snow | none | — |

**Specks, lights, scorch, shake, underwater** (with `jhl_fx_particlesystem`; scales are the drawn fireball's, a
grenade 3; RPG rockets 1.5×, explosive breakables 1.3×):

* Specks: instead of the engine's 15 pure white (palette 254) ones, `EmbersAndLights` throws 25-35 embers (a grenade;
  up to 15 more with size: an RPG rocket 30-40, a satchel 35-45, a mortar 40-50), pale yellow, cream and some white (252, 253, 254), 20-80 sideways (× √ that), 80-220 up, 1.5-2.5 s,
  drifting and falling as the engine's (`pt_blob2`). Not under water.
* Lights: the engine's flash (200, 0.01 s) and orange glow (150, fading 200 a second) times scale / 3 (1 to 2), ±5%;
  the glow lasts until it has faded.
* Scorch (`UTIL_JhlExplosionScorch`): one of `{scorch1`-`3` (vanilla 1-2; decals.wad has a third, placed by name),
  and for scale 6+ one more, 9+ two more, overlapping, 16-32 × (scale / 3) units along the surface.
* Shake (`UTIL_JhlExplosionShake`; vanilla: none): amplitude scale × 1.3, 0.4 + scale × 0.05 s, within
  150 + scale × 70 units (a grenade 4, 0.55 s, 360 units).
* Under water: the `WXplo1` fireball gets the vanilla one's randomness and a second, 80-100% its size, up to
  0.1 s later; the lights as above. Bubbles (`UTIL_JhlExplosionBubbles`): vanilla's 100 in a 128 unit box, times
  scale / 3 in count and box, and 40 × that more in a 1.5× box, rising slowly (speed 3, vanilla's 8).

## Explosions (done)

Vanilla, from Valve's source and the engine (`hw.dll`; the SDK's client `R_Explosion` in `SoundSystem.cpp`
matches it): every explosion is `TE_EXPLOSION` from `CGrenade::Explode` (grenades, MP5 grenades, RPG, satchel,
tripmine, NPC grenades, `func_mortar`) or `env_explosion` (barrels, `func_tank`, map explosions):

* Fireball: `zerogxplode.spr` (`WXplo1.spr` in water), scale (dmg−50)×0.06, 15 fps (1 s), additive at 180,
  10 units up. Lights: 200 radius (250,250,150) for 0.01 s, 150 radius (255,190,40) fading over 1 s.
  15 particles (color 254, ±32 sideways, 80-143 up, 2 s). `explode3-5` at attenuation 0.3.
* Smoke 0.3 s later: `steam1.spr`, scale (dmg−50)×0.08 (`env_explosion`: the fireball's), 12 fps, gray 20-35,
  rising 30 units/s. In water, bubbles instead.
* 0-3 `spark_shower`s (not in water), a scorch decal, a debris sound.
* Placed (dmg−24)×0.6 units out of the surface hit (46 for 100 damage).

Fixed: Half-Life Updated places `CGrenade::Explode` only 0.6 units out (halflife#3244, damage going through
walls), which sank grenade, rocket, satchel and tripmine fireballs and smoke halfway into the floor or wall.
The damage stays at 0.6; the effects use vanilla's point again. `env_explosion` was never changed.

**Fireballs** (client `jhl_explosions.cpp`, called from `R_Explosion`; `jhl_fx_particlesystem`): every
`zerogxplode` explosion sent at 15 fps (grenades, rockets, satchels, tripmines, `func_mortar`, `env_explosion`, the
crossbow bolt; not the Apache/Osprey deaths at 12 fps, Opposing Force's mortar at 10 or underwater `WXplo1`) draws:

* The vanilla fireball, with random roll, scale and frame rate ±10% and position ±4 units. Lights, particles and
  sound are the engine's, once.
* A second fireball 0-0.1 s later, at 140 brightness (the vanilla one is 180; both are additive):
  * Small explosions: round `bexplo`/`cexplo`/`dexplo`/`eexplo`, 0.9-1.1× the first's size (corrected for how
    much of each frame the fire fills), random roll, set off ±6% of its size sideways and, on a floor, 5-15% down
    (elsewhere 0-10% up), playing 0.9-1.3 s.
  * Large ones (scale 6+: satchel, tripmine, Apache rocket, magnitude 150+; and RPG rockets) on a floor: shaped
    `fexplo`/`gexplo` standing on it, plus the `hexplo` mushroom cloud at scale 9+ (`func_mortar`, magnitude
    200+); 0.8-1× the first's size, upright, playing 1.6-2 s. Off a floor (walls, ceilings, mid-air), round ones.
* RPG rockets: the server flags them (`TE_EXPLFLAG_JHL_ROCKET`, `jhl_explosion.h`); the first fireball is 1.5×
  (4.5) and they count as large.

The pool sprites are precached in `W_Precache`.

**Explosive breakables** (barrels, crates, pushables: `func_breakable`/`func_pushable` with an explosion
magnitude, through `env_explosion`): damage is the magnitude, radius 2.5× it, the fireball (magnitude−50)×0.06 (at
least 1.0), so a magnitude 100 barrel is a grenade. The maps use 75-200 in Half-Life, 1-150 in Opposing Force
(many 1-50, all at the minimum fireball), 64-100 for pushables. `ExplosionCreate(..., breakable)` from
`CBreakable::Die` flags them (`TE_EXPLFLAG_JHL_BREAKABLE`; also an `env_explosion` a breakable fires as it breaks, as most of Half-Life's barrels do): the first fireball and lingering smoke are 1.3×
(`JhlBreakableExplosionScale`), the smoke puff too, and they count as large (shaped fireballs on a floor).
Damage unchanged. The sent scale stays the magnitude's, which the client needs to find the surface hit.

**Smoke:** the vanilla puff (`TE_SMOKE`, drawn by the engine: `steam1`, 20 units up, gray 20-35, rising 30 units/s)
is sent 7% bigger (`UTIL_JhlExplosionSmokeScale`, `CGrenade::Smoke` and `CEnvExplosion::Smoke`). No other
vanilla smoke sprite fits it: the rest are additive (they glow) or columns. With every two-fireball explosion
the client adds a lingering cloud (`JhlExplosion_LingeringSmoke`; `jhl_fx_test lingersmoke`): Opposing Force's
single-frame `wep_smoke_01`, gray 70-80, from 0.4 s, for 9-12 s. It fades in over the first 5% to 200/255, then
thins out (squared) the whole time it spreads, from 2× to 8× the vanilla puff's width, fast at first. It rises
8-12 units/s and drifts ±40 units/s plus 40 away from walls within reach (8 horizontal traces), the drift
dying down 30% a second. Random roll.

**Spark showers:** vanilla's `spark_shower` (0-3 per explosion from `CGrenade::Explode`, `env_explosion` and
Opposing Force's mortar; thrown off the surface at 200-300 ±100 sideways +200 up, half gravity, a `TE_SPARKS`
burst every 0.1 s for 0.5-1.5 s: 8 streaks at ±200, a `richo1` flash of scale 0.5-1). With
`jhl_fx_particlesystem` on, grenades and `env_explosion` (with a fireball, sparks allowed) send
`TE_EXPLFLAG_JHL_SPARKS` instead and the client throws them (not in water). The surface hit is found by
tracing down, up and 8 ways around: the one whose distance best matches where explosions are placed off what
they hit (scale × 10 + 15.6, within 12 units), with its real normal; not simply the nearest, since a wall hit low
down has the floor closer. showers by the surface's material (as bullet impacts read it; metal, vents, grates, computers count as metal):
0-8 off metal, 0-5 off anything else, 0-7 if unknown (mid-air, brush entities), the higher of two rolls; each picking every
value on its own: leaving up to nearly along the surface (its direction plus a random scatter of 1.2, never into
it), 50-600 units/s skewed toward moderate (u^1.5, about 270 on average), plus 0-300 straight up (down off a
ceiling), half or full gravity (50/50), 0.4-2.25 s, a burst every 0.07-0.11 s, keeping half their speed per
bounce; each burst 6-12 streaks at ±150-300 and a flash up to 0.4-1 (half that to that). Opposing Force's mortar keeps vanilla's.

**Spark bursts** (`UTIL_Sparks`: `env_spark`/`env_debris`, buttons that spark when off, `env_beam`/`env_laser` spark
ends, shot computers and computer-textured walls, Opposing Force's electrified wires, damaged turrets, the Osprey
hit, crossbow bolts, the Gene Worm, NPCs' "task failed" indicator; vanilla: the engine's `TE_SPARKS`, 8 streaks at
±200 and a `richo1` flash of 0.5-1, the same every time): with `jhl_fx_particlesystem` they go through
`UTIL_JhlSparkShowers` to the client, which makes 4-10 streaks at ±120-220 and a flash of 0.4-1, and now and then
spark showers as explosions' (none 75%, one 20%, two 5%), living half as long. Kept reserved: it's scenery.

**Armor ricochets** (`UTIL_Ricochet`: helmets of grunts and Barney, alien grunt armor, the Gargantua, Big Momma, the
Apache, turrets, the loader, the G-Man, the worms, unbreakable glass; vanilla: a `richo1` glow of the given scale
0.5-2 for 0.1 s and a ricochet sound, no sparks): through the SDK's message, the client adds 4-7 spark streaks
(by scale) at ±100-180, set 2 units toward the viewer. Not aimed: `UTIL_Ricochet` doesn't get the bullet's direction.

## Beams (in progress)

**Vortigaunts** (`islave.cpp`; vanilla: `lgtning` beams, charging arm beams width 30 noise 80 dark green brightness
64 ramping by beam count × 32 (BeamGlow), the zap one per hand width 50 noise 20 bright green 255, the revive
one per hand width 30 noise 80): with `jhl_fx_particlesystem`, a thin, wild, dimmer strand layered on each
(`LayerBeam`, kept out of `m_pBeam` whose count sets the eye glow and the charging pitch):

* Charging: the vanilla beam at noise 60 (80), and a strand width 18 noise 210 at 80% of its brightness, brightening
  with it (BeamGlow).
* Zap: the vanilla beam, and a strand width 20 noise 100 brightness 102 (0.4).
* Revive: the vanilla beam, and a strand width 12 noise 210 brightness 102 (0.4).
* Each strand's noise ±25% per strike.
* The charging beams light where they touch: 72 units, green (80,170,30), 8 units off the surface, about 1 s.
* Where the zap hits, once its damage is done (`ZapHitEffects`): a pale green light (120,255,64), 200 units,
  shrinking away evenly over 1.5 s (vanilla lights only the vortigaunt, orange, as it starts charging), and a
  spark burst (`UTIL_JhlSparkShowers`) with showers by what it hit: a player whose suit lost charge to it 2-3 (the
  suit taking it, as armor ricochets spark), a player it went straight into or a creature none, a wall one half
  the time.

**The vortigaunt's pattern** is the template for other beams: a thin, wild, dimmer strand on each
(`UTIL_JhlBeamStrand`, effects.cpp: the beam's ends, type and color, its own width, noise ±25% each time,
brightness; called again to follow a beam that moved), a faint light where small beams touch, and where an attack
hits a big light shrinking away over 1.5 s with sparks by what it hit (a player whose suit lost charge 2-3 showers,
a player it went straight into or a creature none, a wall one half the time).

**Voltigore** (Opposing Force, `voltigore.cpp`; `lgtning`, purple):

* Charging (3 beams converging where its ball forms, width 50 noise 20): strands width 20 noise 100 brightness
  102, and a faint purple light (90,10,130) of 72 units where they meet, about 1 s.
* The ball (`charged_bolt`, `blueflare2`), re-aiming 2 beams at what's near every 0.05 s (to surfaces width 30
  noise 80, to creatures noise 20, reused): strands width 12 noise 210 (100 to creatures) brightness 102 following
  them, and every other re-aim a faint purple light where they touch, 0.15 s.
* Its impact (vanilla: a scorch, damage, silent): a purple light (200,60,255) of 200 units shrinking away over
  1.5 s, sparks as above, and Opposing Force's `weapons/shock_impact.wav` pitched down to 80-95.
* Its death beams (to what's near, noise 128-192, 0.6 s): strands width 12 noise 210, faint lights 0.6 s.

**Displacer** (Opposing Force):

* The ball (`CDisplacerBall`, `exit1.spr`; re-aims 2 beams at what's near every 0.05 s, to surfaces dark green
  width 30 noise 80, to creatures white): strands width 12 noise 210 (100 to creatures) brightness 102 following
  them, and every other re-aim a faint green light (80,170,30) of 72 units where they touch, 0.15 s. It already
  has its own light and sounds.
* The ball's sprite is the Lambda Core portals' (`exit1.spr`, 25 frames; c3a2b/c/f's `env_sprite`s play it at
  40 fps), but vanilla never steps its frame (the reconstructed Opposing Force code doesn't either), so it showed
  frame 0 only. It now plays at 40 fps (`Animate`).
* The spin-up arcs between the prongs (client `EV_FireDisplacer`: `lgtning`, width 1, noise 0.6, brightness 0.75,
  dark green, 1 s): a strand on each, width 0.4, noise 1.5 ±25%, brightness 0.3 (faint: at the player's eye).
* Its impact ring (`disp_ring`, `TE_BEAMCYLINDER`): left for a pass over every ring (the houndeye's, the
  helicopters' crash rings, `func_mortar`'s).

**Helicopter deaths** (Apache, Osprey and the Black Ops versions):

* Falling: vanilla sends one `zerogxplode` every 0.2 s at a random point under the hull (scale 3-5.9, 12 fps),
  with smoke and metal gibs (Apache 4, Osprey 8). Each one is now `zerogxplode` or a round pool sprite at random,
  100-120% the size (corrected for fill), the same length (1.25 s ±10%), with random roll; the gibs are 4-8
  (Osprey 8-16).
* Crash: vanilla's single `fexplo` (`TE_SPRITE`; Apache scale 12 at +256, Osprey 25 at +512), huge smoke, a ring
  and 200 gibs. Added: the `hexplo` mushroom cloud standing on the ground under the wreck (`UTIL_JhlCrashMushroom`,
  `TE_SPRITE` too, brightness 200; Apache scale 10, Osprey 18).

## Testing

| Need | How |
|---|---|
| A monster to shoot, gib or kill | `npc_create monster_human_grunt` (any classname; spawns where you aim) |
| An explosion on demand | `ent_create env_explosion iMagnitude 100`, then `ent_fire` it |
| Weapons and ammo | `give weapon_rpg`, `cheat_givemagazine`; `cheat_god` to survive |
| A set piece | `npc_create monster_apache`, then the RPG |
| Before/after | The category's `jhl_fx_*` toggle |

**`jhl_fx_test`** plays any event's effect, or any building block with any parameters, where you aim: see
[effects_test.md](effects_test.md). Design an effect with it first, then build it into the game.

## Per-category checklist

1. List the call sites and what they send now.
2. Look at what MMod does (decompile, or its options and assets) and what vanilla already has.
3. Decide the reserved version and the lever (tune, add, enrich); name the toggle.
4. Build, test with the commands above, tune with the user.
5. Write it up in `weapons.md` or a new `effects.md`; flag any new network message.
