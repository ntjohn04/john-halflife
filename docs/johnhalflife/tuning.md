# Tuning sheet

The knobs on the changes made since `v0.7.0-prealpha`, while they settle: what each sets, its value, where it is,
and where to see it. They're sweeping changes to shared code, so each also lists what else it touches. Everything here
is behind `jhl_fx_particlesystem` unless it says otherwise. The full descriptions are in
[Effects plan](effects_plan.md) and [Weapons](weapons.md); this page is only the numbers.

To see things quickly: `sv_cheats 1`, `god`, `notarget` (to watch without being attacked), `impulse 101`,
`npc_create <classname>` (spawns where you aim).

## Shared systems

These touch everything that uses them, not just what they were made for.

### Bounce sounds (`client/sound/SoundSystem.cpp`, `JhlBounceSoundAllowed`)

Every temp entity's bounce sound: debris, gibs, shells, links.

| Knob | Value | Sets |
|---|---|---|
| `DebrisApart`, `DebrisSoon` | 64 units, 0.1 s | The same debris/gib sound this close, this soon: silent |
| `ShellApart`, `ShellSoon` | 48 units, 0.06 s | The same for shells |
| `DebrisBudget`, `DebrisBudgetTime` | 6 in 0.25 s | Most debris and gib bounce sounds in the whole scene |
| `DebrisSlowBounce` | 150 units/s | Debris bouncing slower plays half the time |

See: break a crate, a few grenades on a floor, an M249 burst. Too sparse: raise `DebrisBudget`. Still stacking:
widen `DebrisApart` or `DebrisSoon`.

### Floor dust (`client/jhl_explosions.cpp`, `DustBurst`)

Shared by the houndeye's blast (size 1, white, as it was), the gargantua's stomp (size 2.5) and the tentacles. Its
puffs are `steam1` at 0.5 × size, ±48 × size, ±120 × size units/s, 1.2 × √size s. Changing `DustBurst` changes all
three; change a caller's size or count instead to change one.

### View model blends (`client/rendering/StudioModelRenderer.cpp`, `JhlLatchViewModelSequence`)

| Knob | Value | Sets |
|---|---|---|
| Into a reload | 0.12 s | Glock, revolver, Desert Eagle, shotgun: sequences named `reload` or `pump` |
| Out of one, into idle | 0.2 s | The same guns |

Shared with the barnacle grapple's blends (its own times, above them in the same function). The blend's start is
kept off its animation's `animtime` by 1 ms: equal, the engine replays the animation's events (sounds) every frame.

## Gargantua

Server `server/entities/NPCs/aliens/gargantua.cpp`, client `client/jhl_gargantua.cpp`. See: `npc_create
monster_gargantua` anywhere open (`map c2a5`); it takes explosives (RPG, satchels, charged Gauss).

| Part | Knob | Value |
|---|---|---|
| Flame lights | `FlameHandRadius`, `FlameHitRadius`, `FlameFlicker`, `FlameLightColor` | 180, 150, ±20%, (255,150,60) |
| Flame core | `FlameCoreWidth`, `FlameCoreBrightness`, `FlameCoreSpeed`, `FlameCoreColor` | 9, 0.4, 3.5, (1,0.88,0.7) |
| Flame embers | `FlameEmbersMin`-`Max`, `FlameSmokeChance` | 1-3 a hit a tenth of a second, 40% heat smoke |
| Stomp light | `StompLightRadius`, `StompLightFlicker`, `StompLightColor` | 200, ±15%, (255,70,40) |
| Stomp ground | `StompKickSpacing` (client), `JhlMarkSpacing` (server) | Dust every 36 units, a scorch every 40 |
| Stomp rumble | `UTIL_ScreenShake` in `CStomp::Think` | 5, within 240 |
| Stomp sparks | `CStomp::Think` | 4 in 5 of vanilla's, ±150 sideways |
| Stomp foot cloud | `UTIL_JhlGroundDust` in `StompAttack` | 16 puffs at size 2.5 |
| Eye light | `EyeLightRadiusMin`-`Max`, `EyeLightFlicker`, `EyeLightColor` | 70-200 by its glow, ±8%, (230,40,30) |
| Death plates | `plates`, their life (`RunTask`, `TASK_DIE`) | 16, 35-60 s |
| Death shards | `Shards`, `Lives` | 17/17/16 at 2.5/5/8 s |

## Tentacles

Server `server/entities/NPCs/aliens/tentacle.cpp`, client `JhlExplosion_TentacleImpact` in
`client/jhl_explosions.cpp`. See: `map c1a4i` (metal silo), `map c4a1b` (Xen dirt); make noise near them.

| Part | Value |
|---|---|
| Metal tap | 3-5 spark streaks, half the taps |
| Metal slam | 8-12 streaks, 1 spark shower (1 in 4 two), 4 grey puffs at 0.8 |
| Dirt tap | 4 puffs at 0.6 |
| Dirt slam | 10 puffs at 1.3, 3 billows (0.75-0.8, rising 40-80, ×1.6 life), 1-2 clods |
| Killing swing, besides | 12 puffs at 1.6 |
| Slam shake (`JhlTentacleImpact`) | 4 within 500; killing swing 9 within 600 |
| Dirt sound (`pJhlSlamDirt`) | `debris/concrete1-3`: slams full volume pitch 85-92, taps 30-50% pitch 88-96 |

## Mounted guns (`func_tank` family)

Server `server/entities/func_tank.cpp` (`CFuncTank::Fire`, `CFuncTankMortar::Fire`), client `JhlTank_ParseFire` in
`client/weapon_effects.cpp`. Every `func_tank`, by kind; no map names.

| Kind | Knob | Value | See |
|---|---|---|---|
| Machine gun (`func_tank`) | `MuzzleEffectsAt` | Light and wisp 1.6× a gun's | `map c3a1b` / `c2a5f` (`biggun`, usable) |
| | Flash | The map's sprite and scale, client-drawn, ±15%, a random frame and turn, 0.07 s | |
| | Brass | Up to 2 a shot, right, from 24 behind the muzzle; rifle casings for 12 mm | |
| Rocket (`func_tankrocket`) | `TankLight`, `Flash`, `TankMuzzleSmoke` | 260 (255,170,90) 0.15 s; 0.5; 5 puffs 0.6-0.9 | `map c2a5b` (Bradley), `c2a2e` |
| Cannon (`func_tankmortar`) | `TankLight`, `Flash` ×2, `TankMuzzleSmoke` | 380 (255,180,100) 0.2 s; 1.0 and 0.7; 9 puffs 1.0-1.6, 3 s | `map c2a5b` (Abrams), `c2a5e`/`c2a5f` (howitzer) |
| | Shell's explosion shown | 2.5× its magnitude, 180-260 (Abrams 60: 180, Bradley's cannon 100: 250); magnitude 150+ (howitzers, 200): 0.91×, 182. Damage unchanged | |
| Laser (`func_tanklaser`) | `TankLight` | The beam's color: 140 at the emitter, 110 where it hits | `map c3a1b`, `map c4a1` |

Touches: every map's turrets and every usable one; the cannon's damage now comes from a second, silent, unseen
`env_explosion` at the map's magnitude.
