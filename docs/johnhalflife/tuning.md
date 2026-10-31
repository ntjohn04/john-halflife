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

### Explosion particles (`client/jhl_explosions.cpp`, `ParticleShare`)

Every explosion's embers (the pale specks) and blob burst, from the engine's one pool of particles (shared with
sparks, tracers and blood; when it runs out it prints "not enough free particles" and drops what asked). It can't be
asked how many are free, so the explosions count their own.

| Knob | Value | Sets |
|---|---|---|
| `EmberCountMin`-`Max`, `EmberCountMore` | 21-30, +13 for the biggest | Embers an explosion (15% under the first 25-35, +15) |
| `ParticlesSoft` | 500 | Under this many of theirs out, explosions throw all theirs |
| `ParticlesHard`, `ParticlesLeastShare` | 1100, 20% | From Soft to Hard, each new one throws less, down to this share |
| Blob | 64, 256 for large (`JhlExplosion_Blob` callers) | Also under the budget |

See: chain satchels or several grenades at once. Still running out: lower `ParticlesHard` or `ParticlesSoft`. Big
explosions looking thin early in a chain: raise `ParticlesSoft`.

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

| NPC turret (`turret.cpp`, kind 4) | `JhlTurretShot`, `MuzzleEffectsAt` | Flash 0.35 (client-drawn, ±15%, 0.07 s), light 0.8 a gun's on top of the engine's, a wisp 35%, 1 casing a shot (rifle for the ceiling turret's 12 mm) | Ceiling: `map c3a1`; mini: `map c1a2`, `t0a0d`; sentry: `map c1a3`, `c2a2d`, `c3a1a`; or `npc_create monster_sentry` |

Touches: every map's turrets and every usable one; the cannon's damage now comes from a second, silent, unseen
`env_explosion` at the map's magnitude.

## Mortar fields (`func_mortar_field`)

Server `server/entities/mortar.cpp` (`CMortar`), client `JhlExplosion_ParseMortarIncoming` in
`client/jhl_explosions.cpp`. See: `map c1a3b` or `c1a3c` (We've Got Hostiles), `map c2a5g` (Surface Tension).

| Part | Knob | Value |
|---|---|---|
| Incoming streak's lead | `JhlIncomingLead` (server) | 0.35 s before it lands; keep the client's speed at about 1024 / this |
| Incoming streak | `Incoming` style (client) | (255,200,140) 0.55, 2.5 wide, 260 long, 140 core, 2900 units/s from 1024 up |
| Impact column | `UTIL_JhlGroundDust` in `MortarExplode` | 12 puffs at size 2 (shared `DustBurst`) |

The shells' explosions are the shared explosion code's (`CGrenade::Explode`), as every grenade's and rocket's.

## Chargers and gib shooters

| Part | Knob | Value | See |
|---|---|---|---|
| Charger light (`chargers.cpp`, `CBaseCharger::Use`) | `TE_DLIGHT` | 60-70 units, 0.2 s, refreshed every 0.1 s it gives; health (48,164,144), HEV (204,136,56) | Any charger: `map c1a1` on |
| Charger running dry | `UTIL_Sparks` | A spark burst as it empties | Drain one |
| Shooter pieces' life (`effects.cpp`, `ShootThink`) | `JhlShooterGibLife` | At least 45 s, ±20% (models only) | `map c1a1f` (computers, glass, plates), `c2a4e`, `c1a1a` (cans) |
| Shooter pieces kept | `JhlShooterGibsKept` | 64 at once; past that the oldest (by its `fuser4`, when thrown) fades | |

Touches: every map's scripted debris (`env_shooter`) and `gibshooter`s; their pieces are entities, so the cap matters.
Keep shooters close to vanilla: their count is vanilla's (extra pieces stuck together in c1a1a's vending machine), and
no handles to their pieces are kept between maps (a reused slot named other entities).

## Opposing Force's mortar (`op4mortar`)

Server `server/entities/op4mortar.cpp`, client `MortarShellTrail` and `JhlTank_ParseFire` (kind 1) in
`client/weapon_effects.cpp`. See: `map of6a2`.

| Part | Knob | Value |
|---|---|---|
| Explosion | `MortarExplodeTouch` | As a grenade's of its damage: scale 90, 15 fps (vanilla's overflowed to about 24× a grenade's) |
| Launch | `JhlMortarLaunch` | The rocket launcher's: light 260, flash 0.5, 5 puffs along its shell's way |
| Trail | `MortarTrailSpacing`, `MortarTrailScaleMin`-`Max`, `MortarTrailLifeScale`, `MortarTrailAlpha` | 40 units, 0.45-0.7, 0.7 the RPG trail's life, 0.4 |

## Pit worm

Server `server/entities/NPCs/racex/pitworm_up.cpp`, client `JhlPitWorm_Drawn` in `client/jhl_gargantua.cpp`. See:
`map of4a4`.

| Part | Knob | Value |
|---|---|---|
| Eye light, on the pit | `PitWormEyeDim`, `PitWormEyeBlazing`, `PitWormEyeFlicker`, `PitWormEyeColor` | 220 open, 480 firing, ±10%, (90,255,110); out while blinking |
| Eye light, on its face | `PitWormEyeModelDim`, `PitWormEyeModelBlazing`, `PitWormEyeAhead` | 50 open, 90 firing, 24 units ahead of its eye |
| Beam core | `ShootBeam` | `laserbeam`, width 10 (its beam 32), (210,255,200), 110, scroll 40 |
| Beam burn | `JhlBeamBurn` | Small scorch; light 110-140 (60,255,70) for 0.2 s each update |

## Gene worm and laser turrets

Server `server/entities/NPCs/racex/geneworm.cpp`, `server/entities/func_tank.cpp` (`CFuncTankLaser`); client
`JhlGeneWorm_Drawn` in `client/jhl_gargantua.cpp`, `JhlTank_ParseFire` in `client/weapon_effects.cpp`. See:
`map of6a5`; the laser turrets also `map c3a1b`, `map c4a1`.

| Part | Knob | Value |
|---|---|---|
| Eye lights | `GeneWormEyeRadius`, `GeneWormEyeModelRadius`, `GeneWormEyeAhead`, `GeneWormEyeColor` | 260, 70 on its face 30 ahead, (255,205,60); each out once shot |
| Acid clouds | `LaunchCloud`, `RunGeneWormCloud`, `GeneWormCloudTouch` | Spin 60-140°/s; 3 droplets a tenth in flight; a splash of 18, a spit mark, a sizzle |
| Laser turret beam | `CFuncTankLaser::Fire` | On 0.15 s longer, 40% wider (all laser turrets) |
| Real laser's core and flash | `CFuncTankLaser::Fire`, kind 3 | Core a third its width, its color toward white, 110; flash `glow_blu` 1.1-1.4 |
| Laser start on the barrel | `CFuncTankLaser::Think` | Moved to the barrel each update while on |
| Heart light | `JhlHeartLight` | 240-280 on the cavern, the orb's color |
| Smacks | `AE_GENEWORM_HIT_WALL`, `JhlGeneWormSmack` | Cavern: `tentacle/te_strike2.wav` 75-85, heard across it; swipes: `te_strike1.wav` 82-92 at 0.85, heard across it, on its own channel |
| Recoil when manned | `CFuncTank::Fire` | Machine gun 0.3°, rocket 2, cannon 3, laser 1.5 |

## Gonarch

Client `JhlGonarchSpit_Track` in `client/jhl_gargantua.cpp`. See: `map c4a2`.

| Part | Knob | Value |
|---|---|---|
| Spit droplets | `GonarchDropSpacing`, `GonarchDropsMostAFrame`, `GonarchDropColors` | Every 8 units, at most 6 a frame, palette 254/254/15 (white), 0.6-1 s |

## Ichthyosaur

Server `CIchthyosaur::HandleAnimEvent` in `server/entities/NPCs/aliens/ichthyosaur.cpp`; client
`JhlExplosion_ParseBiteBubbles` in `client/jhl_explosions.cpp`. See: `map c2a3`.

| Part | Knob | Value |
|---|---|---|
| Bite bubbles | `BiteBubbleCount`, `BiteBubbleSpeedMin`/`Max` | 26, thrown out at 50-130 from 32 ahead of it, mostly forward (vanilla 16 rising in a 16-unit cube) |
| Their slowing and rise | `BiteBubbleDrag`, `BiteBubbleBuoyancy` | 3, 2.5 (exponential, a second); rising at 80-140 as vanilla's |
| Their life | `BiteBubbleLife`, `BiteBubbleFade` | 1.6 s ±25%, fading over the last 40% |

## Sound system

Client `GameSoundSystem` in `client/sound/GameSoundSystem.cpp` (the Unified SDK's OpenAL sound system; not gated: it only
acts where a sound would otherwise have played silently). Every sound playing holds an OpenAL source; OpenAL Soft allows
256 unless asked, and the SDK neither asked nor checked, so past that new sounds (gunfire, explosions) were silent until
enough ended. `cl_snd_channels` lists what's playing.

| Part | Knob | Value |
|---|---|---|
| Sources asked for | `RequestedMonoSources`, `RequestedStereoSources` | 1024 mono, 64 stereo |
| Out of sources | `StealChannel` | A one-shot out of earshot, else the one closest to ending, is taken over (never a loop or sentence) |
| Out of sources, said | `m_NextStealWarning` | A console warning at most every 5 s: how many took over, and the sounds playing most |

Room types (`client/sound/HalfLifeSoundEffects.cpp`): the SDK plays them through the old EAX presets (room 22: an arena
reverb, 7.3 s); Steam Half-Life plays them through the engine's own software DSP (room 22: no reverb, an echo every
0.38 s). `jhl_snd_room_vanilla 1` (default) plays the engine's: its presets (Xash3D's `s_dsp.c`) made into an EAX reverb
on the first send and an echo effect on a second; `0` the SDK's.

| Part | Knob | Value |
|---|---|---|
| Reverb (two delay lines: size, 0.71 size, feedback refl) | `SetupVanillaReverb`, `VanillaReverbLevel` | Decay their -60 dB time, averaged; level 0.029 each from the engine's mix, x1 |
| Reverb loop lowpass (rvblp) | `TwoTapAverageHF` | 0.937 a time round at 5 kHz, as the decay's high frequency ratio |
| Echo (delay, feedback, dlylp) | `SetupVanillaEcho`, `VanillaEchoLevel` | In at a quarter, x1; damping 0.2 with dlylp |
| Echo past OpenAL's longest | `MaxEchoDelay` | 0.207 s (outdoors, caverns: 0.3-0.38), feedback lowered to die as fast |
| Each room's level, measured | `VanillaReverbTrim`, `VanillaEchoTrim` | A click in every room type, vanilla and here (`jhl_roomtest.cfg` in `valve/` and the mod, run at the menu; loopback recordings), each room's response against room 0's: the reverb came 7-23 dB under vanilla's (now within ~3 in most rooms), echoes over it outdoors (20-22, -9 to -11) and in room 1 (-15) |
| Feedback | `EngineFeedback` | Kept as the engine keeps it, (int)(255 x value) / 256: 0.999 is 0.992 (room 27 dies away at ~8 dB a second, as measured; room 28's reverb had run away) |
| Water rooms (14-16; 14 under water) | `VanillaLowpass`, `VanillaLowpassHFGain` | The whole sound lowpassed, direct too (the engine's five-tap average: about a tenth left by 4-5 kHz): 0.1 |
| Not done | | The left channel delay (vanilla delays the left 2-40 ms in many rooms, confirmed by the recordings; an effect send can't delay the dry sound) |

Mixing (`jhl_snd_mix_vanilla 1`, default; `0` OpenAL's own): as the engine's software mixer (Xash3D's `s_main.c`,
`s_mix.c`), switched live (the device is reset; sounds already loaded keep how they were loaded until the next map
load).

| Part | Knob | Value |
|---|---|---|
| Mixing rate | `SoundCache::EngineMixRate` | 22050 Hz: each sound stepped to it as it loads, interpolating linearly, as the engine's mixer (a loopback recording of vanilla playing `scientist/absolutely.wav`: 5.5-8 kHz -29.2 dB; linear stepping -28.8, not interpolating -23.5); the device itself stays at its own rate (OpenAL's Windows output ignores a 22050 request: `cl_snd_channels` shows it) |
| Pitch | `SoundCache::GetPitched` | Off its own pitch (nearly everything: voices 100-103 a line, guns 98-102), a sound is pitched as the engine pitches it: stepped through at its own rate without interpolating, then to 22050 Hz linearly, made once a sound and pitch, played at that pitch's own rate (OpenAL pitches only a change while playing). The uneven steps scatter distortion through the band, vanilla's grain: a scientist recorded in vanilla, between his voice's harmonics 1.5-8 kHz, -16.2/-16.1/-14.3 dB; simulated at pitch 102 this way -15.5/-15.6/-14.4, pitched cleanly -18.0/-18.0/-16.8 (the same at pitch 100, why a console `play` matched). Dialogue words too, and mouths read the pitched samples |
| 22050 Hz to the device | `jhl_snd_mix_vanilla_upsample` | 0 (default): OpenAL's 23rd order sinc, as the recording (nothing above 11 kHz, -85 dB: DirectSound's conversion, hw.dll's output, is clean); 1 Linear, 2 Nearest leave highs above 11 kHz. Switched live |
| Ear volumes | `ApplyVanillaSpatialization` | Each ear volume x (1 - distance x attenuation / 1000) x (1 -+ side), 0-1; the player's own sounds and sounds without attenuation full in both (OpenAL: 0.6 each centred, constant-ish power) |
| Played through OpenAL's panning | `StereoPanGains` | OpenAL Soft 1.22's plain stereo panning, measured on a loopback device (0-60 degrees), inverted: each ear gets the engine's volume, within 0.0002 |
| Output | `ALC_OUTPUT_LIMITER_SOFT`, `ALC_OUTPUT_MODE_SOFT` | No limiter (clips, as the engine's CLIP16), plain stereo; no HRTF |
