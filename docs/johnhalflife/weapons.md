# Weapons

Reserved takes on MMod's weapon changes, keeping Valve's weapon models. Background on what MMod does is in
`re/specs/weapons_overview.md`, `weapons_recoil_accuracy.md`, `weapon_sounds.md` and `bullet_impacts.md`
(workspace). Settings are listed in the [index](README.md#settings).

## Feel

### Recoil
`jhl_weapons_smoothrecoil` (0 restores the original punches, which are kept at each call site)

Every weapon's view punch goes through the spring-damped smooth punch also used by the
[jump and landing jolts](camera_movement.md#jump-and-landing-jolts). Kicks are tuned so the visible peak
lands between MMod's and the original. The profiles are in `src/game/client/ev_hldm.cpp`
(`WeaponRecoil`: pitch min/max, jitter, spring, damping). Highlights:

| Weapon | Pitch kick | Spring / damping |
|---|---|---|
| Glock | 2.05–2.5 | 200 / 9 |
| MP5 | ±2.45 | 125 / 7.5 |
| Python | 6.6–8.1 | 85 / 12 |
| Shotgun (single / double) | 4.1–5.05 / 7.9–9.7 | 45 / 9, 10 / 12 |
| RPG | 4.65–5.7 | 85 / 12 |
| Sniper rifle | 2.5–3.05 | 85 / 12 |

### Melee, throws and the gluon gun
Also `jhl_weapons_smoothrecoil`. Weapons that had no recoil get small camera motions, through
`CBasePlayerWeapon::WeaponKick` (`weapons_shared.cpp`): a smooth punch applied on the client the first time a
command is predicted (like the weapon animation), so it's instant and sends nothing over the network. It does
nothing on the server, so with client weapon prediction off (`cl_lw 0`) there's none.

* **Melee** (`MeleeKick`): every swing leans down and to a random side with a little roll; connecting adds a
  short sharp jolt back, which also settles the swing faster, matching the quicker recovery after a hit.
  Strength: knife 0.6, crowbar 1, pipe wrench 1.5 (heavier swings are wider and slower). The wrench's charged
  swing (hold secondary attack) is the biggest and slowest, 2.4, kicked when it's released, since the swing
  itself runs in a server-only think; it gets no hit jolt, so it always settles slowly.
* **Throws** (`ThrowKick`): the view dips forward and rolls slightly; a hand grenade throw at strength 1, the
  snark, penguin and satchel tosses at 1.1 (slower than a melee swing of the same size).
* With `developer 1`, every kick is printed to the console (`V_WeaponKick: ...`).
* **Gluon gun** (`V_AddEgonDrift` in `view.cpp`): no kick, but while firing the view wanders slowly and
  smoothly in any direction, at 40% from the moment it fires, growing to about half a degree after 2 seconds of
  sustained fire, fading out over 0.4 seconds after it stops.

### Scopes
The crossbow and the sniper rifle can toggle their scope every 0.3 seconds (`ScopeToggleDelay`; originally 1
and 0.5 seconds), and the crossbow plays the sniper rifle's `weapons/sniper_zoom.wav`.

### Accuracy
Spread grows per shot and recovers shortly after the weapon is ready again (`FireSpread` in
`weapons_shared.cpp`). It always ends at the vanilla value, so sustained fire is never less accurate
than vanilla; only the first shots are tighter. The current spread is predicted through
`weapon_data_t.fuser4`.

| Weapon | Spread (degrees) |
|---|---|
| MP5 | 2.5 → 3 (vanilla 3); 5 → 6 with `smg_wide_spread` (the multiplayer default) |
| Glock primary | Fixed, about 1.1 (vanilla) |
| Glock rapid fire | About 7 → 11.5 (vanilla 11.5) |
| Python | 0.75 → 1 (vanilla 1) |
| Shotgun | 8.5 → 10 (vanilla 10) |

The crosshair is vanilla. MMod's dynamic crosshair was tried and removed.

## Sounds

### Weapon switch
Switching to a weapon plays a draw sound from a table keyed by view model (`weapons_shared.cpp`).

* Half-Life weapons use MMod's draw sounds (15 files, tracked by `mmod_port`).
* Opposing Force weapons without an MMod counterpart use the crowbar's sound as a placeholder.
* The snark and shock rifle are left out; their own deploy animations already make a sound.
* Played from the server: weapon switches go through the `selectweapon` command and are never predicted,
  so a predicted sound would skip the local player.

### Weapon pickup
Picking up a weapon or a weapon box plays one of the stock `items/gunpickup1-4.wav` at random. Vanilla
only plays `gunpickup2`, which batteries still use. Ammo pickups are unchanged (`9mmclip1`).

### Grenade pin
Pulling the hand grenade's pin plays MMod's `weapons/grenade_pinpull.wav` (`mmod_port`, feature
`grenade-pin`) when the pin comes out: 0.29 seconds in, frame 13 of the 45 fps pinpull animation, as MMod's
model event does (its animation has the same timing as Valve's). Played by the server after that delay, timed
in `CHandGrenade::ItemPostFrame` rather than a think function, which would need a data map (see
[saves](README.md#saves)).

### RPG laser toggle
Turning the RPG's laser guidance on plays the Desert Eagle's `weapons/desert_eagle_sight.wav`, and turning
it off plays `desert_eagle_sight2.wav`, as the Desert Eagle does.

## Effects

The client side is in `src/game/client/weapon_effects.cpp`.

### Bullet impacts
`jhl_fx_bulletimpacts`

On top of the stock decal, specks and ricochet:

* **Metal, vents, grates, computers:** 3–5 spark streaks and a very short warm light.
* **Concrete, tile, dirt, wood:** one small puff of the stock `steam1.spr`, tinted per material, played
  once.
* **Glass, liquids, snow, flesh, Xen:** stock effects only.

The player's bullets are handled in the client weapon events. Bullets fired by the server (NPCs, mounted
guns) send a `BulletHit` message from `DecalGunshot`.

Melee strikes on metal (crowbar, knife, wrench) spark like a bullet does about 35% of the time
(`MeleeImpactSpark`, with `jhl_fx_particlesystem` too): with the decal, 0.2 s into the swing, or at once for the
wrench's big swing, which has no decal. Other surfaces keep the sound and decal only.

### Water splashes
`jhl_fx_bulletimpacts`

When a bullet enters water or slime, the surface point is found along its path and the client draws:

* the stock `wsplash3.spr` droplet sprite (grey-white), and
* MMod's ripple ring lying flat on the surface (optional, skipped if not installed).

NPC bullets send a `WaterSplash` message. A blue droplet spray, engine streaks and a splash sound were
tried and removed.

### Muzzle light and smoke
`jhl_fx_muzzleflash`

On the local player's firearm shots (not the gauss gun), on top of the engine's muzzle flash:

* a stronger, warm light at the muzzle (~0.06 s), and
* a faint wisp of the stock `steam1.spr` drifting up, on about 30% of shots.
  The shotgun rolls that chance once per pellet (single 6, double 12; 4 and 8 with the multiplayer spreads), so
  it smokes more, about 2 and 4 wisps a shot; every wisp after a shot's first starts up to 2 units off, drifts its
  own way, is up to 1.4× bigger and plays at 22-30 fps, so they don't stack exactly.
  The M249's build with sustained fire (`EV_M249MuzzleEffects`, with `jhl_fx_particlesystem`): a heat of 0-1,
  up a 25th a round and back to 0 after a pause over 0.25 s (tap fire stays cold). Cold, more than a gun's (37% of
  shots, opacity 0.13, 1.08×); hot, 70% of shots, opacity up to 0.33 and 1.2-1.45× at random; each up to 1.5 units
  off the muzzle so they don't stack.

The muzzle position is the view model's first attachment. That is only valid while the view model is
drawn, so the effects are skipped in third person and while zoomed.

**Sniper rifle:** the M40A1 view model's attachment sits behind the player, so the muzzle is placed
relative to the view instead (30 forward, 5 right, 5 down from the eyes; the view model is always drawn
in the same place on screen). Wisp on 60% of shots. While scoped, the light stays but the wisp is skipped
so it doesn't drift across the magnified view.

**RPG:** the view model has no attachments, so the front of the tube is placed relative to the view too
(36 forward, 8 right, 6 down).

* **Front:** the light and a wisp on every shot, 3.5× the size and twice the opacity of a gun's.
* **Backblast:** behind the right shoulder, placed from the shooter's position and aim, so other players
  and third person see it too.
  * A short orange light.
  * 6 puffs blasted backward.
  * A billow of large, slow puffs that start behind the shoulders and roll forward past the shooter into
    view, lasting about 2 seconds: two positions per side, three of the four used each shot.

### Tripmine light
`jhl_fx_particlesystem`

An armed tripmine lights the wall around it: a faint light in its laser's color at 80% (0,171,158), 72 units,
2 units in front of its emitter (the laser's start, the mine's origin, 8 units off the wall), as faint as the small
energy beams' lights. Found on the client from the mine's laser beam entity (`laserbeam.spr`, (0,214,198) at 64,
`TripmineLight` from `HUD_AddEntity`), which exists only while the mine is armed, so it comes on with the laser,
goes with it, and comes back on a loaded game. Kept on a frame at a time; only mines within 1024 units of the view,
to leave the engine's 32 dynamic lights for everything else. Nothing is sent over the network.

### Tracers
`jhl_fx_particlesystem`

Vanilla: the MP5 fires the engine's tracer every 2nd round (NPCs every 4th through the server's `TE_TRACER`; the
friendly Opposing Force M249 grunt every 2nd; turrets and the Apache every round), from 16 forward, 2 right and 4
down of the eye to where the bullet hit, a short streak at the `tracer*` cvars' pale color. In first person it flies
straight away from the view, so it's almost invisible; and the engine draws its streak (`tracerlength` times the
speed) only once all of it is on screen, so a long one only appears far away. Those cvars color other engine
streaks too, so they're left alone.

johnhalflife's tracers (`jhl_tracers.cpp`, `JhlTracer_Fire`): a beam (`laserbeam.spr`) whose ends are moved every
frame, growing out of the muzzle, flying as a streak of a set length at a set speed, shrinking into what was hit;
beams are clipped rather than skipped, so it shows from the muzzle on. In first person it starts at the view model's
muzzle, so it crosses the view; otherwise at the game's tracer start.

* **M249** (real tracer rounds; it had none): every 3rd round, a saturated red streak (255,60,30) at 0.85, width
  1.2, 200 units long (seen nearly end-on it looks a few feet), 4500 units/s, starting 0-22 units down its path, so
  mostly right out of the barrel; with a white-hot core in it (255,225,180) at full brightness, width 0.9, 130 units
  long, centered on the streak and shrinking with it as it grows out and goes in. Both are additive, so it reads as
  a hot center with a red edge rather than a laser. Where each hits, a burning speck skips off: a red glow
  (`hotglow` at 0.08) around a white-hot one (0.04), off the surface (a short trace finds its facing), up and to one
  side with a little of the tracer's way along it, falling at a fifth of gravity while twirling in 3-unit circles
  about twice a second, fading over the last half of 1-1.6 s. Its path is worked out each frame, so the two stay
  together. Only off walls and brush entities (the trace ignores models): a tracer that hits a creature buries
  itself, and its blood shows there.
* **MP5** (feedback on where its rounds go, not tracer rounds): on top of its vanilla tracer, every 2nd round, a
  faint pale orange streak (255,200,120) at 0.2, width 1.1, 100 units long (near the vanilla tracer's faintness
  and length), 4000 units/s, starting 0-180 units down its path.

`jhl_fx_test jhltracer` plays them with any settings; `jhl_fx_test tracer` plays the engine's.
