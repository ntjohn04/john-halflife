# Weapons

Valve's weapon models are kept. Client effects: `client/weapon_effects.cpp`. Shared code: `shared/.../weapons_shared.cpp`.

## Feel

* **Recoil** (`jhl_weapons_smoothrecoil`): every punch goes through the spring-damped smooth punch, peaking near
  the original. Profiles: `WeaponRecoil` in `client/ev_hldm.cpp`. 0 restores the original punches.
* **Melee and throws:** small view motions (`MeleeKick`, `ThrowKick`). Predicted on the client only.
* **Gluon gun:** the view drifts slowly while firing (`V_AddEgonDrift`).
* **Accuracy:** spread grows per shot and always ends at vanilla's, so only the first shots are tighter
  (`FireSpread`). MP5 2.5→3°, Python 0.75→1°, shotgun 8.5→10°.
* **Scopes:** crossbow and sniper rifle toggle every 0.3 s (vanilla 1 and 0.5).
* **Reload blending:** Glock, revolver, Desert Eagle and shotgun reloads blend instead of snapping.
* **Projectile aim:** shock rifle, RPG, hornets and spore rockets aim at the crosshair, not parallel to it.
* **Holster** (`jhl_weapons_holster`): slot 8 puts the weapon away; `lastinv` takes it out.
* The crosshair is vanilla.

## Sounds

* Draw sound for every weapon (`config/mod/sound/weapons/`); not for weapons that already make one.
* Weapon pickups: one of `gunpickup1-4` at random (vanilla: always 2).
* Grenade pin pull at 0.29 s.
* RPG reload at 1.2 s, as the rocket goes home. RPG laser on and off: the Desert Eagle's sight sounds.

## Effects

* **Bullet impacts** (`jhl_fx_bulletimpacts`): sparks and a short light on metal; a tinted dust puff on concrete,
  dirt, wood and tile. Melee on metal sparks now and then.
* **Impact materials:** `config/mod/sound/jhl_impact_materials.json` classifies about 1,600 textures Valve's table
  left as concrete. Impacts only; footstep sounds keep Valve's.
* **Water splashes:** a droplet sprite where bullets enter water.
* **Muzzle light and smoke** (`jhl_fx_muzzleflash`): a warm light and a faint wisp. Shotgun smokes per pellet.
  M249 and Glock rapid fire smoke more as they heat up. Smoke carries the shooter's movement.
* **Sniper rifle:** barrel smoke through the bolt, a flash on its barrel, heavy impacts (glowing holes, chunks).
* **RPG:** backblast (light, flame, smoke billowing forward), a flickering motor glow, a smoke trail, a laser stub
  from its housing, a lit laser dot.
* **Casings:** revolver and sniper casings become real shells. Desert Eagle and M249 shells come from the gun, not
  behind the head. Half-Life's own brass restored. Shells linger 6-10 s, shotgun shells eject on the pump.
* **Hand grenade:** tumbles and rolls realistically (looks only; its path is unchanged).
* **Snark:** a small blood burst when it pops.
* **Tripmine:** lights the wall in its laser's color.
* **Tracers** (`jhl_fx_tracers`): M249 red tracer rounds with burning specks; faint streaks on the other guns and
  NPCs' guns; the sniper rifle's air trace. Code: `client/jhl_tracers.cpp`.
* **Gluon gun:** a beam that bends as you sweep, scorches, sparks, lights. Half-Life's own colors restored.
* **Gauss gun:** a visible charge (glow, arcs, unstable sparks), layered beams, afterimages, reflection lights.
* **Hornet gun:** twirling colored trails, spit, dart splats, a strike flare, bleeding stings.
* **Shock rifle:** a glow and spark circling the bolt's path, a double-helix wake, two impact styles.
* **Barnacle grapple:** a springy tongue that reels back in, swallows, bites and leaves a wet mark.
* **Spore launcher:** lime lights on shot, spore and burst; a blast ring showing its reach.
* **Crossbow:** a faint wake behind the bolt.
