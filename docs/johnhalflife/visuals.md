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
