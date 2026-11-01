# johnhalflife

An MMod-inspired mod for Half-Life, Opposing Force and Blue Shift, built on the
[Half-Life Unified SDK](https://github.com/twhl-community/halflife-unified-sdk). It keeps Valve's look and
models and adds reserved takes on MMod's camera, movement and weapon feel; every feature can be switched
off in the console to compare with vanilla.

Status: pre-alpha ([versions](#versions)). MMod's files are not included in this repository; a few features use
them when installed (see [MMod assets](#mmod-assets)), and work without them.

## Principles

* **Written from specs, not from MMod's code.** MMod's binaries were reverse engineered into behaviour
  specs (`re/specs/` in the workspace), and the code here was written from those specs.
* **Reserved.** Each feature is a toned-down take on MMod's version. Vanilla visuals and Valve's models stay.
* **Never worse than vanilla.** Sustained play (accuracy, recoil, readability) is never worse than the
  original game.
* **Vanilla assets first.** MMod files are used only where nothing stock fits. They are never committed;
  see [MMod assets](#mmod-assets).
* **Every feature can be switched off.** Each has a `jhl_*` client setting (default 1, saved in
  `config.cfg`), so it can be compared with vanilla in game.

## Settings

All are client settings: in John Settings (below) or the console.

| Setting | Default | Turns on/off | Page |
|---|---|---|---|
| `jhl_camera_bob` | 1 | Dual-rate view bob and figure-eight weapon sway | [Camera](docs/johnhalflife/camera_movement.md#view-bob-and-weapon-sway) |
| `jhl_camera_viewroll` | 1 | Stronger strafe roll | [Camera](docs/johnhalflife/camera_movement.md#strafe-roll) |
| `jhl_camera_verticallag` | 1 | Eye lift while moving upward | [Camera](docs/johnhalflife/camera_movement.md#vertical-lag) |
| `jhl_camera_jumplandjolt` | 1 | Jump and landing view jolts | [Camera](docs/johnhalflife/camera_movement.md#jump-and-landing-jolts) |
| `jhl_visuals_weaponlag` | 1 | Weapon lags behind when turning | [Camera](docs/johnhalflife/camera_movement.md#weapon-lag-and-wall-push) |
| `jhl_visuals_weaponwallpush` | 1 | Weapon pulls back near walls | [Camera](docs/johnhalflife/camera_movement.md#weapon-lag-and-wall-push) |
| `jhl_visuals_flushfaces` | 1 | Signs, posters and panels built flush with walls drawn steady (they flickered in OpenGL and vanished in software) | [Visuals](docs/johnhalflife/visuals.md) |
| `jhl_visuals_16bitcolor` | 1 | The software renderer's 16-bit color: every frame cut to 565, as it drew | [Visuals](docs/johnhalflife/visuals.md#16-bit-color-jhl_visuals_16bitcolor) |
| `jhl_visuals_softwarewater` | 1 | The software renderer's water: its ripple simulation in place of OpenGL's sine wobble, and under water its 320 x 200 view and fog by depth | [Visuals](docs/johnhalflife/visuals.md#software-renderer-touches) |
| `jhl_visuals_renderfixes` | 1 | Models lit up to 4/3 as the world is; with texture filtering off, textures point sampled with mipmaps and the sky pixelated as the world is | [Visuals](docs/johnhalflife/visuals.md#software-renderer-touches) |
| `jhl_physics_fpsfix` | 1 | Pushables, +use walking, grenades and gibs sliding, debris settling and fire rates acting at any frame rate as at 72 fps | [Physics](docs/johnhalflife/physics.md) |
| `jhl_weapons_smoothrecoil` | 1 | Spring-damped recoil (0 = original punches) | [Weapons](docs/johnhalflife/weapons.md#recoil) |
| `jhl_fx_bulletimpacts` | 1 | Bullet impact sparks/dust and water splashes | [Weapons](docs/johnhalflife/weapons.md#bullet-impacts) |
| `jhl_fx_muzzleflash` | 1 | Muzzle light and smoke, RPG backblast | [Weapons](docs/johnhalflife/weapons.md#muzzle-light-and-smoke) |
| `jhl_fx_particlesystem` | 1 | The particle system changes (gibs, blood, explosions, debris, beams, rings) | [Effects plan](docs/johnhalflife/effects_plan.md#gibs-done) |
| `jhl_fx_tracers` | 1 | johnhalflife's tracers (with the particle system on): the M249's tracer rounds, every gun's faint streaks, NPCs' | [Weapons](docs/johnhalflife/weapons.md#tracers) |
| `jhl_hud_scale` | 1 | Sprite HUD scale (0 = automatic by screen height) | [HUD](#hud-scale) |
| `jhl_snd_vanilla` | 1 | Half-Life's own mixing (22 kHz, its pitch grain, ear volumes, clipping) and room effects, in place of the SDK's OpenAL sound | [Sound](docs/johnhalflife/sound.md) |

Changed defaults of existing settings: `cl_rollangle` 0.65 and `cl_rollspeed` 300 (see
[strafe roll](docs/johnhalflife/camera_movement.md#strafe-roll)).

Options → Multiplayer → **John Settings...** lists the settings above plus a few vanilla ones (crosshair
scale, field of view, frame rate limit, vertical sync, raw mouse input), with **Reset all to recommended
settings** (also the `jhl_recommended` command). Every johnhalflife setting is on by default, as recommended;
"Vanilla | Blood and gibs" (`jhl_gore`) stands in for the content lock's four violence settings.

## Recommended launch options and display

Set in Steam: right-click Half-Life (or the mod), Properties, Launch Options. These are the engine's own, read
once as it starts, before any of the mod's code runs, so the mod can't set them itself.

* **`-particles 8192`**: room for more of the engine's small particles (explosion specks, spark streaks, blood
  specks, tracer particles), which the effects use more of than vanilla. Without it, a chain of big explosions can
  run the engine's pool dry: the console says "not enough free particles", and a few specks don't appear for a
  moment. Harmless either way (nothing breaks; the explosions also thin their own out as they near it:
  [tuning sheet](docs/johnhalflife/tuning.md#explosion-particles-clientjhl_explosionscpp-particleshare)); this just keeps them all.
  Modern PCs don't feel it.
* **`-condebug`** (optional, for troubleshooting): writes the console to `qconsole.log` in the game folder as you
  play, so its last lines survive a crash. With the crash's details from Windows' event log, that usually shows
  what happened.
* OpenGL draws the view at the chosen resolution and stretches it to the desktop smoothly (hard-coded in the engine's
  present); software set the monitor to its resolution. Most of OpenGL's blur at a resolution below the desktop's is
  this stretch: a resolution the screen shows 1:1 (the desktop's, or 4:3 at its height, such as 1920 × 1440 on a
  1440p screen) has none.
* OpenGL multisamples (anti-aliases) the view by default, with no setting; the `-nomsaa` launch option turns it off,
  as software had none. With `developer 1`, the console says how many samples the view is drawn with.

## HUD scale
`jhl_hud_scale` (0 = automatic by screen height)

The sprite HUD is laid out on a virtual screen (real size divided by the scale) and drawn scaled. Engine
text keeps its size; VGUI and the spectator HUD stay in real pixels. Crosshair size stays on
`crosshair_scale`. The crosshair itself is vanilla.

## Games

Each game is its own mod folder with its own name, main menu, New Game / Hazard Course and saves, the way Valve
ships Opposing Force and Blue Shift on top of Half-Life.

| Folder | Name | Start / training map |
|---|---|---|
| `johnhalflife` | John Half-Life | `c0a0` / `t0a0` |
| `johnhalflife_op4` | John Half-Life: Opposing Force | `of0a0` / `ofboot0` |
| `johnhalflife_bs` | John Half-Life: Blue Shift | `ba_tram1` / `ba_hazard1` |
| `johnhalflife_uplink` | John Half-Life: Uplink | `hldemo1` / `t0a0` |

## MMod assets

A few features use MMod's files when they're installed (they're never in this repository); without them,
those features work with vanilla's. They are:

* **Movement:** jump, landing, long jump and wood footstep sounds, and MMod's texture material table.
* **Weapon switch:** 15 draw sounds.
* **Grenade pin:** the pin pull sound.
* **Water splash:** the ripple ring sprite (optional; the splash works without it).

## Versions

| Tag | What it covers |
|---|---|
| `v0.1.0-prealpha` | Camera and movement, weapons (recoil, accuracy, effects, sounds), John Settings and the menus on the 25th anniversary engine, separate game folders for Half-Life, Opposing Force, Blue Shift and Uplink |
| `v0.2.0-prealpha` | Gibs (counts, lifetime, bounce sounds, blood trails, per-NPC profiles, central blood splash with red and yellow chunks, blood pools) and hit blood (sizes, colors, exit spurts, melee bursts, headshot streams), `jhl_fx_test`, the `jhl_fx_particlesystem` toggle |
| `v0.3.0-prealpha` | Explosions: vanilla placement restored (Half-Life Updated sank them into surfaces), two-fireball pool with randomness, bigger RPG rockets and explosive breakables, helicopter deaths (varied falling fireballs, crash mushroom cloud), smoke (bigger puff, lingering cloud), client spark showers (by surface material), surface debris by material, ember specks, size-scaled lights, scorches, screen shake, underwater fireballs and bubbles; debris lasting 60 s with a shared budget (barrels, helicopters, gibs) |
| `v0.4.0-prealpha` | Beams and rings: the vortigaunt's strand pattern (a thin, wild, dimmer strand, endpoint and impact lights, sparks by what was hit) on vortigaunts, voltigores, the displacer and shock rifle, controllers (round animated balls) and the Nihilanth (zaps, death bolts, a finale of flashes and spark showers), map arcs, Gene Worm warp beams and electrified wires; a noisy torus on every ring (houndeye, helicopter crashes, displacer), houndeye light, shake and dust, the displacer's implosion; Xen spire rings layered three deep, drifting in and out of step; breakable shards lasting like gibs (crates throw wood, computers spark), varied spark bursts, armor ricochet sparks, blob particles and fewer non-metal spark showers on explosions; fixes: controller energy ball NaN velocity, gluon view drift after a map change |
| `v0.5.0-prealpha` | Weapons: tracers (the M249's red tracer rounds with a white-hot core and burning specks off walls; faint streaks on every gun, NPCs' too, from the muzzle; `jhl_fx_tracers`), the M249's muzzle smoke building with sustained fire, per-pellet shotgun smoke, the gluon gun's dragged, bending beam (vanilla colors restored) with scorches, leaking sparks, a fizzle on release and lights, the Gauss gun's visible charge (glow, arcs, unstable sparks), brighter layered beams, release bursts, afterimages of charged shots and impact and reflection lights, a crossbow bolt wake, melee sparks on metal, the tripmine light; corpse gibbing by bullet damage (headcrabs, shock roaches, houndeyes, pit drones, bullsquids, controllers), displacer specks; fixes: controller spark burst on corpse gib |
| `v0.6.0-prealpha` | Weapons, continued: hornets' twirling trails in five colours (calm and flourishing moods, straight on the strike), their spit, dart splats, blood on stings and a death light; the shock rifle's bolt as a glow and spark circling its path, tightening with range, half spinning each way, with a double-helix wake, purple fizzles or cyan arcs on impact and a muzzle light; projectiles aimed at the crosshair (shock rifle, RPG, hornet gun, spore rocket); the barnacle grapple's springy tongue (slack flying out, bowing as you move, twanging taut on latching, swallowing as it pulls), reeling back in on letting go, a bite of blood, a wet mark where it latches, blended view model transitions; fixes: shock rifle crash on firing, shock bolt helix on the grapple's tongue tip, grapple animation flicker from prediction |
| `v0.7.0-prealpha` | Weapons, finished: the spore launcher's lights, burst off the surface and a blast ring showing its reach; weapon particles carrying the player's movement; the RPG's fuller launch (backblast, billows, flame flashes), laser dot light and a laser line from its housing, the rocket's flickering motor glow and smoke-puff trail; the sniper rifle's trace with a hot start, bolt smoke, barrel flash and heavy impacts; small touches on the standard weapons (Glock smoke heat, revolver impacts, Desert Eagle barrel wisps, MP5 grenade launch, double-shot blast, melee dust); the hand grenade tumbling and rolling; the snark's death burst; explosion fill light; spent casings (the revolver's and sniper rifle's handed off from their animations, .50 AE and 5.56 shapes, the M249's casing every shot with links, lying along the gun then tumbling, Half-Life's brass on Half-Life's guns, shotgun shells on the pump from the receiver, brass lingering 6-10 s, silent under water); the gluon bend fixed at 0.25; fixes: Desert Eagle and M249 shells thrown from behind the player's head, sniper smoke stopping after a map change |
| `v0.8.0-prealpha` | Monsters and the world: the gargantua's flickering flame lights, embers and heat smoke, its stomp as a wave down a lane, eye light, stomp dust and lingering death plates; tentacle strikes throwing up what they hit by material, slams cratering; the pit worm's lit eye, beam core and scorches; the gene worm's eye lights, lit heart, smacks and acid clouds; the Gonarch's spit dripping white droplets; the ichthyosaur's bite bubbles bursting from its mouth; mounted guns' (`func_tank`) muzzle lights, flashes, smoke and brass, and NPC turrets' flashes and brass; laser turrets' fuller beams and recoil for whoever mans a turret; mortar fields' falling streaks, the Opposing Force mortar's launch and trail; chargers' soft light and sparks running dry; env_shooter pieces lingering 45 s; Opposing Force and Blue Shift view models getting the weapon effects; reload blending (Glock, revolver, Desert Eagle, shotgun); bounce sounds thinned; fixes: the Opposing Force mortar's oversized fireball, floating pieces over breakable floors (c4a2b's Gonarch), gibs stuck in each other, satchel switch sounds, MMod's satchel flap sound |
| `v0.9.0-prealpha` | Sound: Half-Life's own mixing and room effects on the SDK's OpenAL sound system (`jhl_snd_vanilla`), checked against recordings of vanilla: sounds at the engine's 22 kHz, pitched the engine's way (the faint grain of vanilla's sound), its ear volumes (sounds ahead and the player's own full in both ears), clipping rather than a limiter, and its room types in place of the SDK's EAX presets (reverb and echo per room set to vanilla's, water rooms muffling the whole mix); `cl_snd_channels`; fixes: sounds (gunfire, explosions) going silent when OpenAL ran out of sources |
| `v0.9.1-prealpha` | Sound fix: 22 kHz sounds (the guns, many effects) no longer muffled with `jhl_snd_vanilla`: converted to the device's rate flat to 10.5 kHz, as the system converts the engine's output (OpenAL's resamplers rolled off from 6 kHz) |
| `v0.10.0-prealpha` | Visuals: brush entities built flush with walls (signs, posters, panels, gears, rails: about 2,700 faces in Half-Life's maps) found from each map's BSP and drawn just out of the wall, so they no longer flicker in OpenGL or vanish in software (`jhl_visuals_flushfaces`) |
| `v0.11.0-prealpha` | Physics: Half-Life's game code acting at any frame rate as it did at 72 fps, its 1998 `fps_max` (`jhl_physics_fpsfix`): boxes and barrels pushed and pulled at the 72 fps rate, pulling no faster than pushing (it ran away, worse the higher the fps), walking with +use, grenades, satchels, gibs and spores sliding as far as they did, client gibs, debris and shells settling without extra bounces, trigger_camera drift, and guns keeping the time a shot waited (up to a frame was lost each shot) |
| `v0.12.0-prealpha` | Visuals: the software renderer's touches on OpenGL, found in its code and measured on screenshots of the same view: its rippling water (the ripple simulation in place of OpenGL's sine wobble) and under water its 320 x 200 view and fog by depth (`jhl_visuals_softwarewater`); models lit up to 4/3 as the world is, and with texture filtering off textures point sampled with mipmaps and the sky pixelated like the world (`jhl_visuals_renderfixes`); its 16-bit color (`jhl_visuals_16bitcolor`); every johnhalflife setting on by default, as recommended; the map's BSP read in one place (`jhl_bsp`) |

## Open items

* Opposing Force weapons without an MMod counterpart still use the crowbar's switch sound as a placeholder.

## Documentation

* [How johnhalflife works](docs/johnhalflife/README.md): the pages on each system, and notes for working on the code.
* [The Half-Life Unified SDK](UNIFIED_SDK.md): the SDK this is built on, its building and installing
  ([BUILDING.md](BUILDING.md), [INSTALL.md](INSTALL.md)), its documentation ([docs/README.md](docs/README.md)),
  changelogs and Valve's SDK license.
