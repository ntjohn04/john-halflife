# Visuals

## First-person body (`jhl_visuals_firstpersonbody`)

Your own body when you look down. Drawn only: nothing changes in play. Code: `client/jhl_body.cpp`.

* **Model:** the map's player model (Gordon, Barney, Adrian), with three stand-ins:
  * Gordon before the suit: Blue Shift's lab coat Gordon.
  * Barney with no armor: his vestless guard model, with the vested model's hands. A vest pickup puts the vest on.
  * Adrian in boot camp before his suit: the recruits' training clothes. At ease at ofboot0's start until he moves.
* **Placement:** 14 units behind the eyes (18 crouched), on your feet, so it only shows looking well down.
* **Poses:** standing, walking, running, crouching, jumping, ladders, treading water. Picked from your movement at
  once, legs facing the way you move.
* **Arms:** hidden while a weapon is out (the view model's arms are yours). With no weapon, drawn in the lab coat
  Gordon's poses. One-handed weapons show the free arm.
* **Jumping:** drawn upright, without the jump animation's arc.
* **Water:** the hazard course hologram's treading pose.
* **Hidden:** the head, and anything cut off is shrunk to a point (no stretched polygons).
* Extra models are precached last, only with room in the map.

## Flush faces (`jhl_visuals_flushfaces`)

* About 2,700 signs, panels and rails are brush entities built flat against a wall, on the same plane.
* They flickered in OpenGL and vanished in software, since 1998.
* Now each is drawn moved just off the wall (0.1 to 2 units, by distance), only while at rest, and put back after
  drawing so nothing else sees it moved. Code: `client/jhl_flushfaces.cpp`.
* Not handled: entities placed already rotated, two entities flush with each other.

## Software look

Half-Life's software renderer and OpenGL renderer were compared in code and on screenshots. These bring the
software look to OpenGL. OpenGL only: they do nothing with `-soft`.

* **16-bit color** (`jhl_visuals_16bitcolor`): every frame cut to 565 color, as software drew.
* **Model lighting** (`jhl_visuals_renderfixes`): models lit up to 4/3 brightness, like the world. OpenGL capped
  them at 1, so they looked dull.
* **Texture sampling** (`jhl_visuals_renderfixes`): with filtering off, textures point sampled with mipmaps (no
  sparkle), and the sky pixelated like the world.
* **Under water** (`jhl_visuals_softwarewater`): software's 320×200 view and its fog by depth.

## Water (`jhl_visuals_softwarewater`)

Code: `client/jhl_softwarewater.cpp`.

* **Ripples:** software's ripple simulation instead of OpenGL's sine wobble.
* **Waves:** OpenGL's vertical waves kept, exactly as the engine draws them (`EmitWaterPolys`'s formula).
  Water entities by their WaveHeight; world water by worldspawn's (Blue Shift).
* **What's drawn:** world water over the engine's; all-water entities instead of the engine's.
* Over engine-waved water, cut into the engine's own 64-unit pieces so the waves match; elsewhere a 32-unit grid.
* A water brush has each surface twice (up and down); only the one facing you is drawn.

### Water light (`jhl_visuals_waterlight`)

Water has no lightmap, so it was full bright even in pitch-black corners.

* Each grid point reads the light of the walls (16 rays, 192 units) and floor (8 rays at 45°) around it, from the
  map's own lightmaps. Nearer surfaces count more.
* Full bright from light 96. Darker below, to 0.2.
* Under open sky (any of 5 upward rays): full bright.
* Traces see the world only, not entities.
* Per texture:
  * `!fluid*` (Xen), `!toxicgrn*`: never darkened.
  * `!gwater3` (On A Rail): darkest 0.8.
  * `!bwater2`, `!gwater2` (hazard courses, On A Rail): darkest 0.6.
  * `!coolant5` (Blue Shift coolant): full from 48, smoothed over 128 units.
* Moving water fades to an even 0.85, then relights where it stops.
* Sampled once a map, at most 1.5 ms a frame.

### Water opacity (`jhl_visuals_wateropacity`, 0.85)

* Solid water entities (render mode normal) drawn at this opacity.
* Water a map already made see-through keeps its value.
* World water stays solid: the map's visibility doesn't draw what's under it.

## HUD

* `jhl_hud_scale`: 0 automatic (Valve's half-size HUD at 600 tall and below, 2 from 1440, 3 from 2160).
* Half size uses Valve's own "320" sprites, which the Unified SDK left out (`scripts/make_half_hud.py`).
* `jhl_hud_minimal`: no ammo bars in the weapon menu; the flashlight icon fades out when off and full.
* `jhl_hud_campaigncrosshair`: Blue Shift's crosshair in its blue HUD color.
* `crosshair_scale` 0: automatic, by screen height.

## Engine fixes

Patched in memory as the client starts (`client/jhl_enginefixes.cpp`), only where the engine's code is exactly as
expected. Windows finds the code by pattern, Linux by symbol name. `-dev` prints which were applied.

* **White sprite by color-mode brushes:** a rendermode 1 brush left the second texture unit on, so the next sprite
  drew opaque white. The sprite drawing now turns it off first.
* **"AllocBlock: full" late in a long session:** a reused model slot kept the old map's surfaces, overrunning the
  lightmap blocks. The lightmap loops now only take brush models.
* **Hang on a map change late in a long session:** the same reused slot freed the wrong cache block. A reused slot
  now starts clean.

## Known quirks, left alone

* c3a2a/c3a2b: a strip of water surface at the coolant tunnels' mouth. The map's geometry, drawn correctly.
