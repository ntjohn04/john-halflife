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
the brightest light is 1.33× (OpenGL's `gl_overbright`, on by default, matches), the same tone curve. Two touches
differ, each its own setting, off by default. The OpenGL drawing they need is in `jhl_gl.cpp` (looked up through SDL;
the client doesn't link OpenGL), the map reading in `jhl_bsp.cpp` (shared with the flush faces above).

### 16-bit color (`jhl_visuals_16bitcolor`)

Software drew into a 16-bit frame: red and blue to multiples of 8, green to multiples of 4, cut down, never rounded or
dithered (measured: software's screenshot has exactly 32, 64 and 32 levels). Gradients step, and the darkest shades go;
this cut also accounts for most of software looking a little darker (what's left is a flat 2%). Each frame, once the HUD
is drawn (software's HUD was 16-bit too), the client copies the frame and draws it back cut the same way
(`jhl_softwarelook.cpp`).

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

Not reproduced: software's lower-resolution view under water (it rendered at most 640 wide and stretched it).
`jhl_softwarewater_info` (temporary) lists what was made of the map's water.
