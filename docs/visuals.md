# Visuals

## First-person body (`jhl_visuals_firstpersonbody`)

Your own body when you look down. Drawn only: nothing changes in play. Code: `client/jhl_body.cpp`.

* **Model:** the map's player model:
  * Gordon with labcoat
  * Gordon with HEV
  * Adrian with boot camp clothes
  * Adrian with PCV
  * Barney with no vest
  * Barney with vest
* Labcoat Gordon is Blue Shift's model. Vestless Barney borrows the vested model's hands.
* **Poses:** standing, walking, running, crouching, jumping (upright, without the arc), ladders, treading water (the
  hazard course hologram's pose). Legs face the way you move.
* **Arms:** hidden while a weapon is out (the view model's arms are yours); one-handed weapons show the free arm.
  With no weapon, labcoat Gordon's poses.
* **Hidden:** the head; anything cut off shrinks to a point.
* Extra models are precached last, only with room in the map.

## Flush faces (`jhl_visuals_flushfaces`)

* About 2,700 signs, panels and rails in the games are brush entities flat against a wall, on the same plane.
* They flickered in OpenGL and vanished in software since 1998.
* Each is drawn just off the wall (0.1 to 2 units, by distance) while at rest, then put back. Code:
  `client/jhl_flushfaces.cpp`.
* Not handled: entities placed already rotated, two entities flush with each other.

## Software look

Software's look brought to OpenGL, compared in code and on screenshots. OpenGL only: nothing with `-soft`.

* **16-bit color** (`jhl_visuals_16bitcolor`): every frame cut to 565 color.
* **Model lighting** (`jhl_visuals_renderfixes`): models lit up to 4/3 brightness like the world (OpenGL capped
  them at 1).
* **Texture sampling** (`jhl_visuals_renderfixes`): with filtering off, point sampled with mipmaps; the sky
  pixelated like the world.
* **Under water** (`jhl_visuals_softwarewater`): software's 320×200 view and fog by depth.

## Water (`jhl_visuals_softwarewater`)

Code: `client/jhl_softwarewater.cpp`.

* **Ripples:** software's ripple simulation instead of OpenGL's sine wobble.
* **Waves:** OpenGL's vertical waves kept, using the engine's formula (`EmitWaterPolys`). Entities by their
  WaveHeight, world water by worldspawn's (Blue Shift).
* World water drawn over the engine's; all-water entities instead of the engine's.
* Cut into the engine's 64-unit pieces over waved water so waves match; a 32-unit grid elsewhere.
* Each water surface exists twice (up and down); only the one facing you is drawn.

### Water light (`jhl_visuals_waterlight`)

Water has no lightmap, so it was full bright even in pitch-black corners.

* Each grid point reads the map's lightmaps on the walls (16 rays, 192 units) and floor (8 rays at 45°) around it.
  Nearer surfaces count more.
* Full bright from light 96, down to 0.2. Full bright under open sky (any of 5 upward rays).
* Traces see the world only, not entities.
* Per texture:
  * `!fluid*` (Xen), `!toxicgrn*`: never darkened.
  * `!gwater3` (On A Rail): darkest 0.8.
  * `!bwater2`, `!gwater2` (hazard courses, On A Rail): darkest 0.6.
  * `!coolant5` (Blue Shift coolant): full from 48, smoothed over 128 units.
* Moving water fades to an even 0.85, then relights where it stops.
* Sampled once a map, at most 1.5 ms a frame.

### Water opacity (`jhl_visuals_wateropacity`, 0.85)

* Solid water entities (render mode normal) drawn at this opacity. See-through ones keep the map's value.
* World water stays solid: the map's visibility doesn't draw what's under it.

## HUD

* `jhl_hud_scale`: 0 automatic (Valve's half-size HUD at 600 tall and below, 2 from 1440, 3 from 2160).
* Half size uses Valve's "320" sprites, which the Unified SDK left out (`scripts/make_half_hud.py`).
* `jhl_hud_minimal`: no ammo bars in the weapon menu; the flashlight icon fades out when off and full.
* `jhl_hud_campaigncrosshair`: Blue Shift's crosshair in its blue HUD color.
* `crosshair_scale` 0: sized by screen height.

## Engine fixes

Patched in memory at client start (`client/jhl_enginefixes.cpp`), only where the engine's code is exactly as
expected. Found by pattern on Windows, by symbol on Linux. `-dev` prints which applied.

* **White sprite by color-mode brushes:** a rendermode 1 brush left the second texture unit on, so the next sprite
  drew opaque white. Now turned off first.
* **"AllocBlock: full" in long sessions:** a reused model slot kept the old map's surfaces, overrunning the lightmap
  blocks. The lightmap loops now take brush models only.
* **Hang on a map change in long sessions:** the same reused slot freed the wrong cache block. Reused slots now
  start clean.
