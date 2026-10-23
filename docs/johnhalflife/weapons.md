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
  snark, penguin and satchel tosses at 0.6.
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
model event does (its animation has the same timing as Valve's). Played by the server after that delay
(`CHandGrenade::PinPullSoundThink`).

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
