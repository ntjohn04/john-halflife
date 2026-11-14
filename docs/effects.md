# Effects

All behind `jhl_fx_particlesystem` unless noted. Vanilla sprites and models first.

* Server effects send small messages; the client builds the effect (`weapon_effects.cpp`, `jhl_explosions.cpp`).
* Many reuse the Unified SDK's `TempEntity` message ([networking](README.md#networking)).

## Gibs and blood

Code: `client/client_gibs.cpp`.

* About 7× the gibs (each piece used at most 3 times), lasting 60 s, with blood trails for a second.
* Per-monster profiles (`GibProfiles`): bigger for large aliens, fewer and smaller for headcrabs and rats.
* Size, speed and spread varied. Bounce sounds capped.
* Some small corpses gib from added-up bullet damage.
* A gibbed body leaves blood puffs, chunks and a blood pool.
* Hit blood scales with damage (`1 + sqrt(damage)`), never below vanilla. Darker red and yellow.
* A second, smaller burst on every hit. Heavy hits spurt out the far side; melee throws blood sideways.
* A killing headshot streams back toward the shooter.
* Every death that bleeds leaves a blood pool.

## Debris

* Breakable shards last 60 s, 250 at most (`client/jhl_debris.cpp`).
* Explosive barrels throw 2.5× the shards, glass 1.5×. Crates of goods also throw wood. Computers spark.
* Bounce sounds thinned so piles don't clatter.
* Breakable floors wake what rests on them. Gibs pass through each other.

## Explosions

Code: `client/jhl_explosions.cpp`.

* Vanilla placement restored (Half-Life Updated sank fireballs into surfaces; damage unchanged).
* Two fireballs: vanilla's plus a varied second one. Large ones stand on the floor with a mushroom cloud.
* RPG rockets and explosive barrels bigger.
* Lingering smoke drifting away from walls; embers, a room-filling light, bigger scorches, a screen shake.
* Sparks and debris by the surface's material.
* Underwater: two fireballs and more bubbles.
* Particle budget (`ParticleShare`): chained explosions throw less so the engine doesn't run out.

## Sparks and ricochets

* Spark bursts vary in size, now and then with a shower.
* Armor ricochets (helmets, armor, turrets) add a few sparks.

## Beams and rings

* Most beams get a thin, wild, dimmer strand (`UTIL_JhlBeamStrand`), a faint light where small beams touch, and a
  shrinking light with sparks where an attack hits.
* Vortigaunts, voltigores, the displacer, shock rifle, controllers, the Nihilanth, the Gene Worm, electrified wires.
* Map arcs (`env_beam`, `env_lightning` with `lgtning.spr` and noise): a strand each. Set pieces (c4a3's finale)
  stay vanilla.
* Controller balls round and animated. Nihilanth death and finale: bolts, flashes, spark showers.
* Helicopter deaths: varied falling fireballs, a mushroom cloud on the crash.
* Every ring gets a noisy torus strand (houndeye, helicopter crashes, displacer).
* Houndeye: a light, a shake, floor dust. Displacer: an implosion, falling specks.
* Xen spire rings (c4a1, of3a1b): three deep, drifting in and out of step (`client/jhl_rings.cpp`).

## Kept corpses (`jhl_fx_keepcorpses`)

* Corpses vanilla faded (spawned or flagged monsters) stay.
* At most 40; the oldest fade. A spawner blocked by one fades it and spawns anyway.

## Monsters and set pieces

* **Gargantua:** flickering flame lights, embers, flame cores, a floor-tearing stomp wave, an eye light, lingering
  death plates.
* **Tentacles:** strikes throw up what they hit; slams crater and shake.
* **Pit worm:** lit eye, beam core, scorches. **Gene worm:** eye lights, lit heart, smack sounds, acid clouds.
* **Gonarch:** spit drips droplets. **Ichthyosaur:** bite bubbles.
* **Mounted guns** (`func_tank` family): muzzle lights, flashes, smoke and brass by kind; recoil for the gunner.
* **NPC turrets:** flashes and brass.
* **Mortars:** an incoming streak, a dust column. Opposing Force's fireball fixed (24× a grenade's).
* **Chargers:** a soft light while giving (blue-green health, amber HEV, green Opposing Force armor); sparks when dry.
* **Gib shooters:** pieces linger at least 45 s, 64 at most.
