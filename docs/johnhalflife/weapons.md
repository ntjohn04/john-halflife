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

### Projectile aim
Projectiles fired from the gun, off to the side of the eye, are aimed from there at what the crosshair is on (at
least 64 units out, so nothing close up turns them sharply) (`UTIL_JhlAimAtCrosshair`, `util.cpp`; with
`jhl_fx_particlesystem` on). Vanilla fires them parallel to the aim, so they land as far off the crosshair as the gun
is from the eye at any range: the shock rifle 9 right and 7 down, the RPG (unguided) and the spore launcher's rocket
8 and 8, the hornet gun 8 and 12 (its rapid fire's volley keeps its spread, around the crosshair). The crossbow's
bolt starts 2 below the eye and is left as it is, as are the spore launcher's lobbed spores.

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

With `jhl_fx_particlesystem`:

* **Dot:** 7% smaller than vanilla's (0.93; `CRpg::UpdateSpot`).
* **Dot light:** the RPG's laser dot lights the surface around it faintly, as the tripmine's light (72 units, its red
  at 80%, (204,0,0)); not the Desert Eagle's, whose dot is the same drawn at half size (`RpgLaserDot`,
  `weapon_effects.cpp`). It goes out with the dot while it's hidden after a shot.
* **Line when it comes on:** its player sees a thin red line (0.5 wide, 0.4) from the laser's housing to the dot,
  holding 0.15 s and fading out by 1 s, following the dot: the targeting locking on, not a beam always on screen.
  Whenever the dot comes on: the client starts it when the dot's seen after 0.15 s unseen (turned on, or the RPG
  taken out with it on), not within 3 s of a reload starting (the dot's hidden 2.1 s then, and coming back isn't
  coming on; `CRpg::Reload` tells the client, predicted);
  first person. (A server message when the dot was made was tried: quick toggles sometimes went without a line.)
  Turned off before it's faded, it goes with the dot. The housing is the small cylinder alongside the front of the
  tube; the view model has no attachments, so its front, found in the model's vertices (the middle of its front ring,
  (3.37, -9.88, -3.39) in the launcher's bone, `Dummy22`), is placed from that bone as the view model was last drawn (`JhlViewModelPoint`,
  `StudioModelRenderer.cpp`): where it was on the view model then, put where the view model is now, once the view's
  placed for the frame (`V_CalcRefdef`), so it stays on the housing as the camera, the player and the gun's sway
  move (set before, it was a frame behind them).

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

Smoke leaves the gun moving with it and the air stops it: each wisp, and the RPG's backblast and shoulder smoke, is
given the shooter's velocity on top of its own and sheds it, halving every 0.5 s: a gun's wisp (about 0.57 s) keeps most of it
its whole life, the RPG's longer-lived smoke hangs where it was let out a moment after. A flat share of the velocity was considered: it never slows.

The muzzle position is the view model's first attachment. That is only valid while the view model is
drawn, so the effects are skipped in third person and while zoomed.

**Sniper rifle:** the M40A1 view model's attachment sits behind the player, so the muzzle is placed
relative to the view instead (30 forward, 5 right, 5 down from the eyes; the view model is always drawn
in the same place on screen). Wisp on 60% of shots. While scoped, the light stays but the wisp is skipped
so it doesn't drift across the magnified view. With `jhl_fx_particlesystem` the barrel vents smoke through the
bolt's working instead: a thin wisp every 0.07 s for 1.2 s after a shot, thinning out, from the tip of its barrel as
the view model's drawn (found in its vertices: (-25.6, 0, 1.33) in its rifle's bone, `m40a1_stock`;
`JhlViewModelPoint`), so it stays on the barrel as the bolt's worked; carrying the player's movement, shed as
smoke's; not while scoped (`SniperSmoke_Update`).
Its light is bigger and a little longer, a heavy round's (190-230 units, 0.08 s; a gun's 140-170, 0.06 s), and its
flash shows: the engine draws a gun's flash at its view model's first attachment, which on this one sits behind the
player, so it never showed; it's drawn on its barrel's tip instead (`muzzleflash2`, 0.3), not while scoped.

**Sniper rifle impacts** (`jhl_fx_particlesystem`): its round hits heavier. On metal, computers, vents and grates the
hole glows (`hotglow`, orange, 0.096) a moment, fading out over 1 s, as hot spall, on top of the metal sparks; on
dirt, wood, concrete and tile a puff twice the size (0.45) and two small chunks of the surface (an explosion's
surface debris at half its size, 80-200 units/s, 4-8 s; `JhlExplosion_ImpactChunks`); on glass the two chunks
(`HeavyImpact`, `weapon_effects.cpp`).

**RPG:** the view model has no attachments, so the front of the tube is placed relative to the view too
(36 forward, 8 right, 6 down).

The launch is the first of its two beats (a recoilless launcher's soft launch; its motor lights 0.4 s later): its
punch is in its light, smoke and flame, not a push on the shooter.

* **Front:** the light, two wisps on every shot, 4× the size and twice the opacity of a gun's, and the launch
  charge's small flash (the engine's `muzzleflash2`, 0.25, 0.07 s).
* **Backblast:** behind the right shoulder, placed from the shooter's position and aim, so other players
  and third person see it too.
  * An orange light, 200-240 units, 0.15 s.
  * A short flame out the back: two flashes along it (`muzzleflash2`, 0.45 and 0.35, 0.07 s; a sprite faces the
    view, so two make it long seen side on). The server precaches the sprite (`CRpg::Precache`).
  * 9 puffs blasted backward, 0.6-0.95, about 1.45 s.
  * A billow of large, slow puffs that start behind the shoulders and roll forward past the shooter into
    view, lasting about 2.5 seconds: three positions per side, five of the six used each shot.

The rocket coasts 0.4 s, then its motor lights (its sound, light and trail), burns 1 s and burns out (its sound and
light off), flying on with its trail (`CRpgRocket`). With `jhl_fx_particlesystem`:

* **Motor glow and light:** drawn by the client in place of the engine's (`EF_LIGHT`: a big flat white flare and
  light, gone at once at burnout): an orange glow (`hotglow`, 0.9) with a white-hot core (`glow02`, 0.27) on top, and
  a warm orange light (255,150,60), 200 units; flickering as it burns, each frame (the glow 0.65-1.35× its size, the
  core 0.8-1.6×, mostly flaring bigger; their brightness down to the least of that; the light ±20%), fading out over
  0.25 s at burnout
  (`RocketGlow`, `weapon_effects.cpp`). The server tags the rocket `renderamt` 1 burning, 2 burnt out.
* **Trail:** smoke laid along its path from when its motor lights, in place of the engine's follow beam (it only adds
  light, and drawn in straight pieces showed seams and bright overlaps where it turned; toning it down was tried):
  grey puffs (`steam1`, as the backblast's, (170,170,170), 0.6-0.9), one every 32 units, slowly rising (about 18
  units over their life) and coming apart over about 3.75 s (±15%; vanilla's trail 4 s, near solid), starting at 0.5
  and fading out as they spread to 2.2× their size; at most 12 a
  frame; not under water (vanilla's bubbles there).

### Standard weapons
`jhl_fx_particlesystem`

Small, reserved versions of the flair weapons' effects (noticeable when looked for, never standing out):

* **Glock, rapid fire:** its smoke builds over a burst, a much lighter M249's (`EV_GlockRapidMuzzleEffects`): up a
  12th a round, cool again after 0.3 s; wisps from a gun's (30% of shots, 0.1, 1x) to 45%, 0.15, up to 1.15x (the
  M249's to 70%, 0.33, 1.45x).
* **Revolver:** a little heavier impacts: on dirt, wood, concrete and tile its puff is 1.4x a bullet's (0.4) in place
  of a bullet's (the sniper rifle's 2x and chunks).
* **Desert Eagle:** its barrel smokes a moment after a shot, a shorter, fainter sniper rifle's: four wisps, one every
  0.1 s, thinning (`EV_EagleBarrelSmoke`, `EagleSmoke_Update`; from its view model's first attachment).
* **MP5 grenade:** a launch (it had only the sound): two wisps, 2.2x a gun's at 0.18, and its light 1.1x, 0.08 s
  (`EV_MP5GrenadeEffects`).
* **Shotgun, double shot:** its light 1.2x and two small puffs (0.15-0.2, 0.09) pushed out ahead at 40-70 units/s,
  carrying the player's movement, on top of its per-pellet wisps.
* **Crowbar, pipe wrench, knife:** besides the sparks now and then on metal, every blow on dirt, wood, concrete or
  tile now and then blows a little puff (the crowbar's half its blows, the pipe wrench's 80%, the knife's 40%), 0.7x a
  bullet's (0.28), the knife's 0.5x (0.22) (`MeleeImpactSpark`; the `BulletHit`
  message now carries what made the impact, `JhlImpact`).

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
  hard-hitting revolver's and Desert Eagle's faster (5000 units/s); the revolver's, scoped, starts at the game's
  tracer start.
* **Sniper rifle:** a trace rather than a tracer (`JhlSniperTrace_Fire`): the faint pale line a heavy round leaves in
  the air, (220,220,215) at 0.15, 1 wide, along all its path in one beam (no seams), growing out at 5500 units/s and
  fading out over 0.45 s; and for its first 120-300 units (about 10-25 feet), hot: an orange streak (255,150,60, 1.4
  wide, 0.7) cooling away from the barrel in three pieces end to end (1, 0.6, 0.3), with a white-hot core (255,235,200,
  0.5 wide) over the first; how hot varies shot to shot (25-100%: its brightness, and how long it lasts, 0.12-0.22 s).
  It starts where its muzzle shows on screen (its view model's attachment sits behind the player); scoped, from just
  under the eye, 60 units ahead, as a barrel under the scope would be: on the magnified view it rises from below its
  bottom edge and meets the crosshair (from the eye itself it would be a dot; started on screen, it began in mid
  air), hot start and all. The Black Ops assassins' rifle
  leaves the same.
* **Shotgun** (a hint of the spread, in the background; it had none): a set number of each shot's pellets, 3-4 for a
  single shot and 6-8 for a double (the extra pellets show), each as the MP5's streak (255,200,120 at 0.2, width
  1.1, 100 units long, 4000 units/s) but starting 0-55 units down its path.
* **NPCs:** the server sends their tracers through the SDK's own message (`UTIL_JhlTracer`, `TE_TRACER` with a kind)
  in place of the engine's, as often as vanilla (the same number of messages): the client draws the vanilla tracer
  and the matching streak on top (MP5s the MP5's, 9mm pistols the Glock's, the .357 and 12mm (turrets, the Apache) the
  revolver's, shotguns the shotgun's, the Black Ops assassin's rifle the sniper rifle's trace). Opposing Force's friendly M249
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
  muzzle out to the side and up, moving with the player (their velocity added), falling at half gravity, 0.4-0.7 s.
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
    spitting out of the muzzle (`hotglow` at 0.04, moving with the player (their velocity added), falling at half
    gravity onto what's below, 0.6-1 s), 3 to 9 a
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

The hornet gun's flair is organic (blood, spit, splats) rather than energy, and its signature is its hornets' trails.

* **Stings bleed:** a hornet that hits a creature deals its damage straight (`DieTouch`), past the code that would
  bleed, so vanilla shows nothing; now `SpawnBlood` at the hornet in the target's blood color, sized by its damage
  (a light hit).
* **Spit:** each shot flings 3-5 small droplets of spittle in the gun's own brownish red (110,42,30) out of its mouth:
  where the hornets launch (16 forward, 8 right, 12 down of the eyes), since its view model has no attachments.
  `hornet_spit.spr` is the bullsquid's `tinyspit` recoloured (an index-alpha sprite, whose colour is its palette's
  last entry; `scripts/make_hornet_spit.py`, committed in `config/mod/sprites`). At 0.2-0.3, forward 150-260, out ±70,
  plus the player's own velocity (as ejected shells: running forward doesn't overtake it), falling at full gravity
  onto what's below, each playing once over 0.6-0.9 s; first person.
* **Colours:** each hornet type gets more colours, its behaviour (speed, turning) unchanged: red-type hornets (40%)
  red (179,39,14) or purple-red (130,42,48: ted's suggested colour in the code, 161,25,97, browner); orange-type
  (60%) orange (255,128,0), yellow (255,200,40) or light brown (190,140,80); so each colour is a fifth of all hornets
  (`m_JhlColor`, saved).
* **Trails** (`jhl_hornets.cpp`): drawn by the client instead of the engine's follow beam (which can't twirl), from
  the path each hornet flies; the server tags each hornet (`IgniteTrail`: `rendercolor` its colour, `renderamt` 1
  hunting, 2 committed; a normal render mode ignores both) instead of sending the trail.
  * A small corkscrew around its path while it hunts, laid down where it flew so it holds still: a turn every 0.24 s,
    2.42 units out, both drifting smoothly at random (turning ±33% over about 0.9 s, radius ±42% over two cycles out
    of step); width 1.2 at 0.5, 0.3 s long, dimming toward the tail and fading out when the hornet goes.
  * In moods: a calm spell (0.6 s, loops half as big, turning 0.58 as fast: nearly a wave) and a flourish (0.45 s,
    loops twice as big, turning 1.25 as fast: a showy twirl), each easing in and out on its own schedule (the first
    0.3-1.5 s into the flight, as most hornets live under 2 s, then one every 0.8-1.8 s); a third of hornets leave
    the gun calm, a third in a flourish. Its angle is its turning speed's integral, worked out the same for any point,
    so the drift and spells never make it jump.
  * Once it commits to a strike the twirl stops, unwinding into its straight dive.
  * 8 pieces (0.0375 s each) for up to 10 hornets at once, to keep within the engine's beams; a turn needs about 5 of
    them to stay round (faster loops alias into a zigzag, hence the flourish's 1.25). Any more hornets get the
    engine's own follow trail in their colour instead (never none).
* **Strike flare:** the hunt made visible. When a homing hornet commits to its strike (vanilla: lined up on its
  target within 300 units, it doubles its speed, buzzes and dives straight; single player), its trail flares: a
  wider glow in its colour and a hot core (its colour whitened 55%), with a 0.4 s tail, drawing only the straight line
  of the dive (`StrikeFlare`, two `TE_BEAMFOLLOW`s). Twirling while hunting, a hot straight line on the strike. It
  stings or expires after it, never hunting again, so the flare rides it to the end. Alien grunts' hornets flare too.
* **Death light:** where a hornet dies on something (a sting, a dart's splat; only ever once, not when a homing one
  turns off a wall, which can be several times in a row), a small pop of its colour, as faint as the energy weapons'
  small lights (72 units), fading out over 0.5 s.
* **Dart splats:** a dart that hits a wall or brush entity splats on it in its colour, muted and dark (organic, not
  paint: red 90,50,42, purple-red 80,48,55, orange 105,80,50, yellow 110,98,62, light brown 100,85,65) and small (the
  shape at 75%), with the bullsquid's spit hitting (`bc_spithit1`/`2`) higher (125-140) and quieter (0.5). The decals
  (`{hspit_red1`/`2`, `pur`, `org`, `yel`, `brn`) are the bullsquid's spit splats `{spit1`, `{spit2` recoloured and
  shrunk (a decal's colour is its palette's last entry). The engine reads one `decals.wad`, so the install adds them
  to the installed one (`scripts/make_hornet_decals.py`, run by CMake when Python is found; it keeps the Unified
  SDK's original as `decals_unified.wad` and starts from it each time, so nothing of Valve's is committed).
  `UTIL_DecalTraceName` places a decal by name.

### Shock rifle
`jhl_fx_particlesystem`

The vanilla bolt (`CShockBeam`): a tiny chrome model (`shock_effect.mdl`, attachments 5 forward and 40 to either side)
with a pale glow sprite (`flare3`, distorting) at its centre and two lightning beams (cyan, and a thin noisy pale
yellow) from one side attachment to the other, fading toward the far end, so its crackle shows 40 units to one side
of its glow. Spinning it can't make the two circle each other equally (the glow is on its axis, the crackle's end 40
out), so:

* **Glow and spark circling:** the server leaves out its glow and crackle (`Spawn`); the client draws its glow
  (`flare3` at 0.35, distorting, as vanilla's) and a blue spark (the gluon gun's flare, `XSpark1` at 0.45, cyan-blue
  by itself, sparkling) circling its path 180 degrees apart, 3 turns a second, half the bolts one way and half the
  other (so a burst has no lean and its straight path shows), 6.85 units out easing in (smoothstep) to 4 by a
  medium-range hit (25 feet, about 300 units, 0.15 s) and 3.3 by 35 feet (0.21 s) (`jhl_shockbeams.cpp`). Vanilla's glow and crackle were 40
  units apart (the crackle starting at its model's side attachment); the pair is closer, by look.
  Up to 16 bolts at once.
* **Aim:** at the crosshair (see Projectile aim).
* **Wake, a double helix:** behind them, from where they've been, about 300 units (0.16 s), fading toward the tail
  and out after it hits: the glow's smooth and purplish (170,110,220, 1.6 wide, 0.5), the spark's jagged and cyan
  (0,253,253, 0.8, 0.6, the engine's random noise at 0.15, rerolled every frame). 8 pieces a strand, for up to 4
  bolts at once (their beams), the newest; the rest have their glow and spark only. Each fades out over 0.1 s,
  done 0.1 s before the 5th bolt after it in held fire: 0.6-0.7 s old (about 100-117 feet) usually, 0.2-0.3 s (about
  33-50 feet) with `shockrifle_fast` (the multiplayer default), so a new bolt never takes a wake still showing.
* **Its two colours, half the time each,** so rapid fire tumbles the two over each other (both are its own, its
  spark's and its glow's):
  * **Muzzle light:** its impact's light, as big as a firearm's (140-170 units), a brief 0.15 s, cyan (0,253,253) or purple (170,110,220); first
    person.
  * **Impact:** cyan, vanilla's light and 3-4 short cyan arcs (`lgtning`, 12-30 units and 1 wide, each impact's 5-30% bigger, noise 0.5-0.8, 0.4-0.6 s and up to 35% more, off the surface hit)
    crackling off where it hits; or purple, a purple light and 3-4 of its glow sprites (`flare3`, pinkish grey)
    fizzling up and out off the surface it hit (from 4 units off it, so none lodge in a wall), falling and bouncing
    (the engine's sprite trail, `TE_SPRITETRAIL`). Besides its sparks and
    scorch.

### Barnacle grapple
`jhl_fx_particlesystem`

* **Tongue:** drawn by the client instead of the server's beam (hidden, `EF_NODRAW`), in 8 pieces from the player's
  attachment 1 to the tip, as the server's (`tongue.spr`, 1.6 wide, solid) (`jhl_grapple.cpp`). Flying out it hangs
  a little slack (sagging at its middle by 7% of its length, up to 14 units); once latched on (the server tags the tip
  `renderamt` 2, `CGrappleTip::TongueTouch`) it twangs taut, straight and thinner (1.25, over 0.08 s). It's springy,
  only to look at (where the tip goes and when it latches are the server's): each point along it keeps where it is as
  the line from the gun to the tip moves under it (as the player moves; most at its middle, none at its ends) and
  springs back to its shape, flying 16 rad/s and damping 0.4, latched 60 and 0.3 (stiffer, bouncier), bowed at most
  20% of its length, up to 40 units. Moving while it's out bows it behind. The tip has the shock bolt's model; shock
  bolts are tagged `renderamt` 1 to tell them apart.
* **Swallowing:** latched on and taut, a bulge 0.65 wider runs along the tongue from the tip to the gun over 0.3 s,
  one every 0.7 s, spread over about 22% of its length so it rolls from piece to piece.
* **Retraction:** when its tongue lets go (a miss, or fire let go of), it reels back into the gun instead of
  vanishing, starting the frame the tip goes: from at most 150 units out (so it's always seen to, at the same pace),
  over 0.2 s, quickening, wiggling side to side like a measuring tape reeling in (a wave 4 units out at full length,
  1.5 waves along it, running in 4 times a second), so it's plainly not the tongue in play any more.
* **Bite:** latching onto something that bleeds draws a little of its blood (`SpawnBlood`, 10), as a hornet's sting.
* **Latch mark:** latching onto a surface leaves a small dull wet mark: the hornet's dart splat in grey
  (`{hspit_wet1`/`2`, (65,63,58), made by `scripts/make_hornet_decals.py`) (`CGrappleTip::TongueTouch`).
* **Blending:** the view model blends from its last pose instead of snapping (the engine never latches the view
  model's sequence changes, so its renderer's usual blend never ran; `JhlLatchViewModelSequence`,
  `StudioModelRenderer.cpp`): into idle (and `firereached`, which `EndAttack` shows a frame before idle) over 0.2 s,
  between its firing animations over 0.1 s, and into `fire` not at all (it snaps, at once). Only how it looks: its
  timings are as they were. (The model's unused `firerelease` was tried for letting go: it's `fire`'s mouth opening
  wide again, then a close.)
* **Prediction:** its fire state is told to the client (`weapon_data_t.iuser3`, as the gluon gun's;
  `CGrapple::GetWeaponData`). Latching, missing and letting go are the server's alone, so the client's prediction
  otherwise went on thinking it was firing after the server had stopped, and their two views of the animation flipped
  it for a frame (`ClientPredictionSystem` forces the server's), worst holding fire through misses.

### Spore launcher
`jhl_fx_particlesystem`

Vanilla's spit sprays, glow, trail, splat and burst sprites are kept; its lights are added to.

All its lights are the launcher's lime (221,255,54): its burst sprite's (`spore_exp_c_01`), at full brightness.

* **Muzzle light:** bigger than a firearm's (180-210 units), 0.15 s as the shock rifle's; first person
  (`EV_FireSpore`).
* **Spore light:** a steady 120 units on a spore in flight, fired or lobbed, enough to glow on the walls it's near
  (`SporeLight`, `weapon_effects.cpp`; the server tags a spore `renderamt` 1, `CSpore::Spawn`). A throb quickening
  toward a lobbed spore's burst was tried and couldn't be noticed.
* **Burst light:** 180 units fading out over 1 s at 60% (133,153,32) (vanilla's green (15,220,40), 100 units for
  0.5 s, hidden behind its spray), still glowing once most of the spray's gone.
* **Burst placement:** its burst's sprite, sprays and light are 12 units off the surface it hit, not on it (the
  spore is a point, so vanilla burst exactly at the surface: its 128-unit burst sprite half behind it, its upward
  spray into a ceiling); its damage is where vanilla's is (`CSpore::IgniteThink`). A lobbed spore bursting at rest on
  its fuse looks down for its surface (vanilla found none: no splat, and its spray no way to go).
* **Blast ring:** so its reach can be read (the same for both fire modes: 200 units, falling off evenly to nothing),
  a faint ring facing the view, the outline of its blast's sphere from wherever it's seen, from where its damage is,
  growing out to 180 units over 0.265 s, widening and fading out as it goes, all three evenly together (halfway at
  half its time): a lime glow
  (3 to 12.8 wide, 0.235) around a thinner paler core (240,255,170; 1 to 3.2 wide, 0.12: it adds onto the glow, only a
  hint of texture); lumpy, a circle at a glance, a lumpy one looked at: its edge wavers by up to 12% of its radius (three
  waves around it, 2, 3 and 5 to a turn, drifting), both layers the same shape, the core's a little turned; a little toward the view so it doesn't flicker in a wall seen straight on. Drawn as
  one band of triangles, 64 points round, with the beams' texture (`laserbeam.spr`), not beams: their straight pieces
  with square ends overlapped at every joint, seams brighter where they added up (`jhl_blastring.cpp`, from
  `HUD_DrawTransparentTriangles`; sent as the `TempEntity` message's `TE_BEAMDISK`).
