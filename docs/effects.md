# Effects

Everything here is behind `jhl_fx_particlesystem` unless noted. Vanilla sprites and models first.

* Server effects send small messages; the client builds the effect (`weapon_effects.cpp`, `jhl_explosions.cpp`).
* Many reuse the Unified SDK's `TempEntity` message with John Half-Life's contents (see [networking](README.md#networking)).

## Gibs

Code: `client/client_gibs.cpp`.

* About 7× as many gibs (each piece used at most 3 times), lasting 60 s.
* Per-monster profiles (`GibProfiles`): bigger for large aliens, fewer and smaller for headcrabs and rats.
* Size and speed varied, spread wider. Bounce sounds capped.
* Some small corpses gib from added-up bullet damage.
* A gibbed body leaves a splash of blood puffs and chunks, and a blood pool on the floor.
* Gibs leave blood trails for a second.

## Blood

* Hit blood scales with damage (`1 + sqrt(damage)`), never below vanilla.
* Darker red and yellow, like the gib splash.
* A second, smaller burst on every hit. Heavy hits spurt out the far side.
* A killing headshot streams back toward the shooter. Melee hits throw extra blood sideways.
* Every death that bleeds leaves a blood pool under the body.

## Debris

* Breakable shards last 60 s, in a shared budget of 250 (`client/jhl_debris.cpp`).
* Explosive barrels throw 2.5× the shards. Glass 1.5×. Crates of goods also throw wood. Computers spark.
* Bounce sounds thinned so piles don't clatter dozens of times.
* Breakable floors wake everything resting on them. Gibs pass through each other.

## Explosions

Code: `client/jhl_explosions.cpp`.

* Vanilla placement restored (Half-Life Updated sank fireballs into surfaces; damage unchanged).
* Two fireballs: vanilla's plus a varied second one. Large ones stand on the floor, with a mushroom cloud.
* RPG rockets and explosive barrels bigger.
* A lingering smoke cloud drifting away from walls.
* Spark showers by the surface's material. Surface debris thrown from what was hit.
* Ember specks, a room-filling light, bigger scorches, a screen shake.
* Underwater: two fireballs and more bubbles.
* Particle budget (`ParticleShare`): chains of explosions throw less so the engine doesn't run out.

## Sparks and ricochets

* Spark bursts vary in size, now and then with a shower.
* Armor ricochets (helmets, armor, turrets) add a few sparks.

## Beams

Most beams' pattern: a thin, wild, dimmer strand on each beam (`UTIL_JhlBeamStrand`), a faint light where
small beams touch, and where an attack hits a light shrinking away with sparks.

* Vortigaunts, voltigores, the displacer, shock rifle, controllers, the Nihilanth, the Gene Worm, electrified wires.
* Map arcs (`env_beam`, `env_lightning` with `lgtning.spr` and noise): a strand each.
* Set-piece beams (c4a3's finale) are left as vanilla.
* Controller balls round and animated. Nihilanth death and finale: bolts, flashes, spark showers.
* Helicopter deaths: varied falling fireballs, a mushroom cloud on the crash.

## Rings

* Every ring gets a noisy torus strand (houndeye, helicopter crashes, displacer).
* Houndeye: a light, a shake, floor dust. Displacer: an implosion, falling specks.
* Xen spire rings (c4a1, of3a1b): layered three deep, drifting in and out of step (`client/jhl_rings.cpp`).

## Kept corpses (`jhl_fx_keepcorpses`)

* Corpses vanilla faded (spawned monsters, flagged ones) stay like any other.
* At most 40 kept; the oldest fade. A spawner whose spot has one fades it and spawns anyway.

## Monsters and set pieces

* **Gargantua:** flickering flame lights, embers, flame cores, a stomp wave tearing the floor, an eye light, death
  plates that linger.
* **Tentacles:** strikes throw up what they hit by material; slams crater and shake.
* **Pit worm:** lit eye, beam core, scorches. **Gene worm:** eye lights, lit heart, smack sounds, acid clouds.
* **Gonarch:** spit drips droplets. **Ichthyosaur:** bite bubbles.
* **Mounted guns** (`func_tank` family): muzzle lights, flashes, smoke and brass by kind; recoil for the gunner.
* **NPC turrets:** flashes and brass. **Mortars:** an incoming streak, a dust column; Opposing Force's mortar fixed
  (its fireball overflowed to 24× a grenade's).
* **Chargers:** a soft light while giving (blue-green health, amber HEV, green Opposing Force armor); sparks when dry.
* **Gib shooters:** pieces linger at least 45 s, at most 64 at once.
