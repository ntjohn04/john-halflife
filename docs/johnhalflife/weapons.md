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
`jhl_fx_particlesystem` and `jhl_fx_tracers`

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
  and length), 4000 units/s, starting 0-120 units down its path.
* **Glock, revolver, Desert Eagle, sniper rifle** (semi-automatic; they had none): the MP5's streak on every shot, the
  Glock's a little quicker and shorter (4250 units/s, 85 long), the
  hard-hitting revolver's and Desert Eagle's faster (5000 units/s), the sniper rifle's faster still (5500) and a
  little wider (1.19), starting 20-120 units out.
  The sniper rifle's starts where its muzzle shows on screen (its view model's attachment sits behind the player)
  and is skipped while scoped; the revolver's, scoped, starts at the game's tracer start.
* **Shotgun** (a hint of the spread, in the background; it had none): a set number of each shot's pellets, 3-4 for a
  single shot and 6-8 for a double (the extra pellets show), each as the MP5's streak (255,200,120 at 0.2, width
  1.1, 100 units long, 4000 units/s) but starting 0-55 units down its path.
* **NPCs:** the server sends their tracers through the SDK's own message (`UTIL_JhlTracer`, `TE_TRACER` with a kind)
  in place of the engine's, as often as vanilla (the same number of messages): the client draws the vanilla tracer
  and the matching streak on top (MP5s the MP5's, 9mm pistols the Glock's, the .357 and 12mm (turrets, the Apache) the
  revolver's, shotguns the shotgun's, the Black Ops assassin's rifle the sniper rifle's). Opposing Force's friendly M249
  grunt fires the M249's red tracer rounds instead of the vanilla one, one in 3 (vanilla every 2nd).

Outside first person a streak starts at the shooter's muzzle, the model's first attachment (the grunts' and the
player's: 20-25 units out from the right hand, where its muzzle flash is), not where the game starts its tracers (by
the eyes, or an NPC's gun position, often behind the muzzle, which hid the M249's red in the gun): for NPCs the server
finds it (`FireBullets`; the client doesn't set NPCs' attachments, leaving them at their feet) and sends it with the
tracer; for players the client uses the player model's (`JhlTracers_MuzzleStart`). Either only when it's set (not at
the origin) and within 96 units of the game's start. The vanilla tracer keeps its own start.

The crossbow's bolt is a real projectile, so no streak: instead a faint grey wake follows it (`TE_BEAMFOLLOW` on the
bolt, `laserbeam`, (170,170,170) at 16, width 0.4, 0.2 s of its flight, about 400 units), starting a few feet out
(sent 0.03 s after it's fired, about 60 units, `WakeThink`), not underwater.

Into water, a streak stops about a foot under the surface (found as bullet splashes find it, only when it ends in
water). `jhl_fx_tracers` turns all of them off by itself, leaving the vanilla tracers.

`jhl_fx_test jhltracer` plays them with any settings; `jhl_fx_test tracer` plays the engine's.

### Gluon gun
`jhl_fx_particlesystem` (`jhl_egon.cpp`, the local player's beam)

Its colors are Half-Life's again (always, as a fix): Half-Life Updated divided all three by 255 ("too bright",
halflife issue #3033), a dim, see-through blue; Half-Life divides red and green by 100 and leaves blue at 125, which
the engine saturates into its bright, textured white-blue.

* **Dragged beam:** its two beams are drawn as 8 straight pieces each along a curve from the gun (its view model's
  muzzle) to where it hits, the engine's own beams kept but unseen. The curve's middle pulls toward where the hit
  point was a moment ago (a copy of it that follows by `jhl_egon_bend` seconds, 0.1 by default, saved; 0 straight),
  so sweeping or strafing bows it behind the motion and holding still straightens it; the bow is at most 30% of the
  beam's length. The hit point and the damage are the game's, unchanged. Each piece draws the engine's noise over its
  own length, the spiral's turns restarting on each, so the spiral beam is drawn in fewer, longer pieces
  (`jhl_egon_spiralpieces`, 2 by default, like vanilla's spiral, 1-8, saved) than the smooth one (8). Scaling the pieces' noise to the whole
  beam's length to continue one spiral across them didn't work: the noise went wild.
* **Scorches:** small scorches (`{smscorch1`-`3`) where it burns walls and brush entities, not creatures, at least 14
  units apart and 0.12 s apart.
* **Leaking sparks:** every 0.1-0.3 s a small blue speck (the gun's own flare, `XSpark1`, cyan-blue, at 0.06,
  additive; additive sprites aren't tinted) flung from the
  muzzle out to the side and up, falling at half gravity, 0.4-0.7 s.
* **Fizzle on release:** 4-6 blue specks (0.1) where it was hitting, scattered ±50 with a pop of 10-70 up, falling at
  half gravity onto what's below, each fading over the last 60% of 1-1.5 s.
* **Lights while firing:** one each, moved every frame (a keyed dynamic light, kept alive a frame at a time, not
  made and removed): a faint cyan glow around the player just above the feet (60,110,150), 160 units, and a brighter
  one 8 units off the wall where it hits (100,180,230), 140 units flickering ±10%; both fade out over 0.25 s when it
  stops. The specks' and flare's blue.

### Gauss gun
`jhl_fx_particlesystem` (`jhl_gauss.cpp`, the local player)

* **A charge you can see**, from the spin events (their pitch, 110 as it starts to 250 at full charge, 4 s in single
  player; the overcharge zaps 10 s after it started), in stages at the muzzle (the view model's; in third person the
  player model's):
  * From the start: a glow on the muzzle (`hotglow`, the gun's own orange, growing 0.036 to 0.144 and brightening) and
    its light on the hands and around (one keyed light, 30 to 110 units, (255,140,40)).
  * From a third of the charge: short arcs crackling off the muzzle (`smoke.spr`, the gun's beam sprite, 5-10
    units, 0.05-0.12 s), 4 to 12 a second as it charges.
  * At full charge, unstable: the arcs 20-30 a second, longer and wilder, the glow and light flickering, and sparks
    spitting out of the muzzle (`hotglow` at 0.04, falling at half gravity onto what's below, 0.6-1 s), 3 to 9 a
    second, more of everything as the overcharge nears.
* **Primary beam:** a little brighter (153, vanilla 128), with a core in it: the same path, half the width (0.5),
  a pale hot orange (255,200,110) at 0.35, so the edges stay orange and the center is just a little hotter.
* **Secondary (charged) beam:** still white and stronger with its charge, but even a small charge at least the
  primary's strength (vanilla drew it at damage / 255: a half-second charge's 25 damage, more than a primary shot's
  20, at a fifth of its brightness). By its damage c (0-1 at 200; what's left after reflections and walls): the beam
  1.1-2.0 wide (a little over the primary's 1 when small; vanilla 2.5 at any charge) at 0.42-0.62; a saturated
  orange core (255,110,0) 40% as wide at 0.35-0.65 (orange even on a small charge; a full one stays near white), warming its center (the white is kept well below full so it
  shows: added onto bright white it only makes white); past half charge a soft white halo twice as wide, up to 0.25.
* **A lesser charge letting go:** a few of those sparks thrown out of the muzzle, more the more it held: 2 for the
  least, 7 just short of full (by the charge the spin events show; the predicted shot's damage can come in far over
  what was held, giving a tap a full charge's sparks). The muzzle flash scales by it too.
* **Afterimage** (secondary only, `JhlGauss_Afterimage`): the path it took, every reflection and wall, left glowing
  warm white like an incandescent bulb (255,196,136), worked out every frame as it dissipates: its brightness easing
  out (lingering, then thinning away), spreading 80% wider, and its sine noise growing from a wavy line to a swirl
  (turning by itself). By the charge c: 0.81-3.3 s (weighted to the top: a tap's is gone before the next can be
  fired), 0.1-0.3 bright, 0.95-1.35 wide; its noise starts at 0.03-0.06 and grows 0.103 a second whatever the charge,
  so they all swirl alike and a longer one just gets further (a tap's to about 0.11, a full charge's to 0.4). Stretches
  under 16 units aren't drawn (a beam through a creature traces on from just past each hit, finding it again: tiny
  stretches that made a star of noise). In first person from the muzzle it was fired from, not following the gun.
  (The engine's own beam fade-out didn't fade; it vanished.)
* **A full charge letting go:** 8-12 of the unstable charge's sparks thrown out of the muzzle (forward 80-200, out
  ±110, up to 110), and 2-3 of the gun's own bouncing orbs (`TE_SPRITETRAIL` of `hotglow`, as at its impacts, 0.15-0.25).
* **Muzzle light** when it fires (as the guns' muzzle light): the charge's light becomes it, so they don't clash;
  primary 161 units (255,160,60), charged up to 299 units and whiter.
* **Reflections:** a brief light where a beam reflects (anyone's), 72 units in the gun's orange, fading over 0.4 s;
  where it ends up hitting something, a slightly bigger and brighter one, 100 units (255,170,70), fading over 0.5 s.

### Hornet gun
`jhl_fx_particlesystem`

* **Stings bleed:** a hornet that hits a creature deals its damage straight (`DieTouch`), past the code that would
  bleed, so vanilla shows nothing; now `SpawnBlood` at the hornet in the target's blood color, sized by its damage
  (a light hit).
* **Spit:** each shot sprays 2-4 droplets of creature spittle (`tinyspit.spr`, yellow-green, the bullsquid's, pit
  drone's and spore launcher's) out of the muzzle, falling (the engine's sprite spray, as theirs); first person.
