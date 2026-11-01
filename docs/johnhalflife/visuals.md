# Visuals

## Faces flush with walls (`jhl_visuals_flushfaces`, default 1)

Many of Half-Life's signs, posters, panels, gears and rails are brush entities (`func_wall`, `func_illusionary`,
`func_breakable`, doors) built flat against a wall, with a face on the very plane of the wall face behind it. That's
about 2,700 faces across the campaign's maps. Nothing in the engine separates the two: in OpenGL they flicker as
they fight for each pixel, and in software the entity's face disappears into the wall. That has been true of
every renderer since 1998. `gl_polyoffset` doesn't help, because the engine only uses it for decals.

`jhl_flushfaces.cpp` fixes it on the client, in the same spirit as `gl_polyoffset`:

* **When a map loads:** the map's BSP is read once. Each brush entity with a face that covers a world face on the
  same plane, facing the same way, is noted, along with where the map placed it.
* **Each frame (`HUD_AddEntity`):** such an entity, while you can see it, is drawn moved out of the wall along
  that face's normal. The distance is just more than the depth buffer can tell apart at that range (near plane at
  4 units, 24 bits): 0.1 unit up close, about 0.25 at 1,000 units, at most 2. It is only moved while it sits where
  the map placed it, unrotated, so a door that opens or a train on its way is left alone.
* **After the view is drawn (`HUD_Redraw`):** the entity is put back, so collision, prediction and sounds never see
  it moved. Each frame the engine resets brush entities to their network position before the client sees them
  (checked in hw.dll), so the offset can't build up.

It works the same in both renderers, and on Blue Shift's map format. With `developer 1`, each map load prints how
many entities were found.

**Not handled:**
* Entities the map places already rotated (very few).
* Two entities flush with each other.
* At more than about 2,900 units, the 2 unit cap may not be enough.

## Software renderer touches

Half-Life's software renderer (sw.dll) and its OpenGL renderer (hw.dll) were compared in their code and on
screenshots of the same view. They light the world identically: the same light gamma table, light × texture ÷ 192 so
the brightest light is 1.33× (OpenGL's `gl_overbright`, on by default, matches), the same tone curve. Several touches
differ, each its own setting. The OpenGL drawing they need is in `jhl_gl.cpp` (looked up through SDL;
the client doesn't link OpenGL), the map reading in `jhl_bsp.cpp` (shared with the flush faces above).

### Model lighting (`jhl_visuals_modeloverbright`, default 1)

Both renderers light the world texture × light ÷ 192, so the brightest light makes a surface 4/3 as bright as its
texture. Software lit models the same way; OpenGL never let a model's light go past its texture, so its models look
duller than the room they stand in (measured: software's models 1.33 times as bright at every brightness, its world
the same). While the engine draws a model's polygons (`StudioDrawPoints`), the second texture unit multiplies their
lit colour by 4/3 (a constant 2/3, scaled by 2), clamped as software clamps; additive models and glow shells, which
aren't lit, are left alone. The engine's model drawing never touches that unit, and it's put back as it was after.

### 16-bit color (`jhl_visuals_16bitcolor`)

Software drew into a 16-bit frame: red and blue to multiples of 8, green to multiples of 4, cut down, never rounded or
dithered (measured: software's screenshot has exactly 32, 64 and 32 levels). Gradients step, and the darkest shades go;
this cut also accounts for most of software looking a little darker (what's left is a flat 2%). Each frame, once the HUD
is drawn (software's HUD was 16-bit too), the client copies the frame and draws it back cut the same way
(`jhl_softwarelook.cpp`).

### Under water (`jhl_visuals_underwaterview`, `jhl_visuals_underwaterfog`)

In water, slime or lava, software drew the view into Quake's warp buffer, 320 × 200, never raised: narrowed to 320
wide keeping its shape, then no more than 200 tall (its width kept, so its pixels aren't square: 4.5 × 5.4 at
1440 × 1080, measured on a screenshot), stretched over the screen, the HUD drawn sharp on top. And it fogged the view as
it stretched it: each pixel mixed toward the water's colour by one of 64 blends picked by its depth (32768 ÷ distance,
the last from 520 units in), `((64 − blend) × far >> 6) + near` of 256, never less than 32, near = density ÷ 2,
far = (512 − near) × density >> 8. OpenGL fogs linearly from the eye to 1536 − 4 × density instead. Both take colour
and density from the water's palette (entry 3's colour, entry 4's red), from the water drawn last.

* `jhl_visuals_underwaterview`: once the view is drawn (before the HUD), it's drawn again from the middles of those
  320 × 200 blocks (`JhlSoftwareLook_ViewDrawn`).
* `jhl_visuals_underwaterfog`: the engine's own fog is kept off under water without touching the engine: OpenGL is
  told to fog by a fog coordinate of 0 (`GL_FOG_COORD_SRC`), so linear fog starting at 0 covers nothing, put back as
  soon as the view is out of the water (`JhlSoftwareLook_SetupView`). The view's depth is copied and software's fog
  put on by it, with the projection taken while the world is drawn; its colour from the water drawn here (the
  software water), else from the engine's fog set up. (Taking the engine's fog off a finished frame can't work: far
  enough out it has covered all there was.)

### Rippling water (`jhl_visuals_softwarewater`)

Software's water isn't Quake's sine wobble: it's a ripple simulation. A 128 × 128 field of heights shared by all water
steps as a wave (each cell: half its four neighbours, less what it was, losing 1/64), once 0.05 s have passed since the
last step (18 a second at 72 fps), a drop of random size landing at a random cell every other step. Each step the
water's texture is drawn again into a 128 × 128 tile, each texel taken from as far off as its cell's height ÷ 16. OpenGL
instead bends a fixed sine wave through the corners of the water's polygons.

`jhl_softwarewater.cpp` runs the same simulation and draws the tiles from the engine's own water textures (read back,
so gamma and filtering match). Water is what the engine treats as water: names starting with `!`, `laser` or `water`.
* The world's water is drawn over the engine's.
* Water entities (`func_water`, a `func_train` of water...) that are all water are hidden from the engine
  (`HUD_AddEntity` returns 0) and drawn here, turned and moved as the engine places them, as see-through as they were
  (render modes normal, texture and additive). Opaque entities partly water have their water drawn over the engine's;
  see-through ones partly water are left to the engine.

`jhl_softwarewater_info` (temporary) lists what was made of the map's water.
