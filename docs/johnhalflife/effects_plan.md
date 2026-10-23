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

## Explosions (in progress)

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
