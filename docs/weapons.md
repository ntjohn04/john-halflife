# Weapons

Valve's weapon models and crosshair are kept. Client effects: `client/weapon_effects.cpp`. Shared code:
`shared/.../weapons_shared.cpp`.

## All weapons

* **Recoil** (`jhl_weapons_smoothrecoil`): every punch spring-damped, peaking near the original. Profiles:
  `WeaponRecoil` in `client/ev_hldm.cpp`.
* **Accuracy:** spread grows per shot and ends at vanilla's, so only the first shots are tighter (`FireSpread`).
* **Projectiles** (RPG, hornets, shock bolts, spores) aim at the crosshair, not parallel to it.
* **Melee and throws:** small view kicks (`MeleeKick`, `ThrowKick`), client only.
* **Holster** (`jhl_weapons_holster`): slot 8 puts the weapon away; `lastinv` takes it out.
* **Sounds:** a draw sound for weapons without one (`config/mod/sound/weapons/`); pickups pick from `gunpickup1-4`
  (vanilla: always 2).
* **Bullet impacts** (`jhl_fx_bulletimpacts`): sparks and a short light on metal, tinted dust on concrete, dirt,
  wood and tile, a droplet where bullets enter water. Melee on metal sparks now and then.
* **Impact materials:** `config/mod/sound/jhl_impact_materials.json` sorts about 1,600 textures Valve's table left
  as concrete. Impacts only; footsteps keep Valve's.
* **Muzzle light and smoke** (`jhl_fx_muzzleflash`): a warm light and a faint wisp that carries the shooter's
  movement.
* **Tracers** (`jhl_fx_tracers`): faint streaks, NPCs' guns included (`client/jhl_tracers.cpp`).
* **Casings:** linger 6-10 s. Half-Life's own brass restored.

## Per weapon

* **Glock, revolver, Desert Eagle:** reloads blend instead of snapping. Glock rapid fire smokes more as it heats.
  Revolver casings become real shells; Desert Eagle shells come from the gun. Desert Eagle laser stub.
* **MP5:** spread 2.5→3°.
* **Python:** spread 0.75→1°.
* **Shotgun:** spread 8.5→10°; reload blends; smoke per pellet; shells eject on the pump.
* **M249:** red tracer rounds with burning specks; smokes more as it heats; shells from the gun.
* **Sniper rifle:** scope toggles every 0.3 s (vanilla 0.5); smoke through the bolt, a barrel flash, an air trace,
  heavy impacts (glowing holes, chunks); real shells.
* **Crossbow:** scope toggles every 0.3 s (vanilla 1); a faint wake behind the bolt.
* **RPG:** backblast (light, flame, smoke billowing forward), flickering motor glow, smoke trail, laser stub, lit
  laser dot. Reload sound at 1.2 s as the rocket goes home; laser on and off use the Desert Eagle's sight sounds.
* **Hand grenade:** pin pull at 0.29 s; tumbles and rolls (looks only; path unchanged).
* **Gluon gun:** view drifts while firing (`V_AddEgonDrift`); the beam bends as you sweep, scorches, sparks,
  lights. Half-Life's own colors restored.
* **Gauss gun:** a visible charge (glow, arcs, sparks), layered beams, afterimages, reflection lights.
* **Hornet gun:** twirling colored trails, spit, dart splats, strike flares, bleeding stings.
* **Snark:** a small blood burst when it pops.
* **Tripmine:** lights the wall in its laser's color.
* **Shock rifle:** a glow and spark circling the bolt, a double-helix wake, two impact styles.
* **Spore launcher:** lime lights on shot, spore and burst; a ring showing the blast's reach.
* **Barnacle grapple:** a springy tongue that reels back in, swallows, bites, leaves a wet mark.
