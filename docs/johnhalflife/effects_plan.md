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
  death (4 for large aliens: alien grunt, vortigaunt, bullsquid, controller, zombies, gonome, Big Momma,
  ichthyosaur, by model, `IsLargeAlien`): in practice the cap decides, 15 human pieces plus the skull, 16 large
  alien, 12 small alien, 21 pit drone, 24 shock trooper; voltigore limits kept. Each gib still leaves up to 5 blood decals as it bounces.
* 60 s on the ground before fading, instead of `giblife`.
* Each gib ±15% size and ±20% speed, and a wider random sideways direction (±0.7 instead of ±0.25 per
  horizontal axis), so gibs fan out instead of flying one way when a blast is level with the body. Vanilla's model renderer ignores scale; `StudioSetUpTransform` now applies it
  to entities marked with `ScaledModelMarker` in `iuser4` only, so map models with a scale keyvalue are unchanged.
* Bounce sounds: 4 gibs per death squelch (`debris/flesh*.wav`, through the SDK's temp entity bounce sounds:
  louder the harder they land, random pitch) on their first 3 bounces only, so at most 12 sounds over the first
  second or so instead of dozens.

## Blood (in progress)

* **Gib blood trails** (`client_gibs.cpp`, `EmitBloodTrail`): 7 gibs per death drop blood particles along their
  path for the first second after being thrown, a drop every 4 units at first and thinning out. Each drop is
  an engine particle (`R_AllocParticle`, `pt_grav`) in the blood stream's colors (red palette 70+, yellow 195+),
  carrying 15% of the gib's speed, lasting 0.6-1 s. Calling `R_BloodStream` every frame would emit 100+ particles
  each time, so the trail is built from single drops instead. Respects `violence_hblood`/`violence_ablood`. Engine particles have one fixed
  size (no size field), so the drops can't grow or shrink; small shrinking `blood.spr` sprites would be the way.

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
