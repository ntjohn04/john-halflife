# Visuals

## First-person body (`jhl_visuals_firstpersonbody`, default 1)

Looking down, you see your own body: Gordon in the HEV suit, Barney or Adrian, whichever player model the map's game
gives you (Unified's model replacement), Gordon in his lab coat before he has the suit, and Barney in his shirt and tie
while he has no armor. It's purely drawn: your
movement picks its pose and nothing goes back. It stands 14 units behind your eyes (18 crouched), so it's only seen
looking well down and never gets in the way.

`jhl_body.cpp` draws it on the client, as its own entity:

* **Each predicted move (`HUD_PlayerMove`):** the movement is read: speed and direction, crouching, on the ground or
  not, in water or on a ladder.
* **Each frame (`HUD_CreateEntities`):** the body is added, posed for that movement at once, with no blending, as
  Half-Life's movement is: standing, walking, running, crouched, crouch-walking, jumping or falling, treading water.
  The legs face the way you move (walking backwards past 120 degrees off), and the player model's back turns the rest of
  the way to where you look, as the engine turns players in third person. Walking and running play at their own speed,
  in step with the footsteps; crouch-walking goes as far as you move, in a loop of 1.8 seconds at the slowest.
* **As it's drawn (the studio renderer):** it's put on your feet where the movement has them, whatever the camera's
  smoothing does on stairs and landings; in the air it hangs from your eyes as in a standing jump, so a crouching jump
  looks like one. Its head is hidden, and its arms while a weapon is out (the view model's arms are yours). Crouched,
  only its legs are drawn. Everything hidden together is shrunk to the one point where it's cut off, so no stretched
  polygons are left floating.

**Jumping and falling:** the player model's jump has the jump's arc in it (the body rising 20 units, sinking 37 as it
lands, leaning forward and folding over the hips), which brought its back and shoulders out under the eyes, far below
them in a fall. It's drawn without it: hips and back upright as standing, only the legs and arms moving. The arms keep
the jump's lean, hold its frame 21 (later they come back in to hold a weapon), and sit a little forward (4 units, 2
down, raised 45 degrees), close to the eyes like view models; one arm alone (below) is swung back 90 degrees, 5 units
forward, out of the weapon's way. Its back is drawn up to its 4th spine bone with the arms
out; with a weapon out, as far as shows no shoulders moving without hands: the HEV suit's 3rd, Barney's 3rd, the lab
coat's 2nd, Adrian's 4th.

With no weapon (before you have one, or holstered on slot 8), its arms are drawn. The player models' own upper bodies
hold a weapon out in every pose, so standing, walking and running they take the lab coat Gordon's poses instead, with a
weapon out too (its arms hidden), measured from the hips and played half a loop on, his loops starting on the other
foot. Running, the arms swing raised 25 degrees back and a unit forward, so at a near-level view more of the swing
shows than fingertips popping up. Crouched, no arms. The lab coat Gordon's model is a scientist's with only walking
poses, so jumping and crouching he takes the player model's, bone for bone.

With a one-handed weapon its free arm is drawn: the left with the crowbar, wrench, knife, grapple, .357 (hidden
while its reload uses both hands), satchel
detonator, hornet gun, grenade (hidden while the view model's own left hand pulls the pin), snark, penguin and shock
rifle; the right with the satchel itself. Chosen by the view model's file, any campaign's copy. Crouched and on
ladders, none.

On a ladder, no arms (none of the games has a climbing pose: the hazard course hologram's `ladder` is her pointing
at one), and walking's pose stepping only as it climbs, up or down (a loop each 1.3 of its strides climbed), held
mid-step when it stops.

Treading water, every body takes the hazard course hologram's pose as she treads water (`models/holo.mdl`'s
`treadwater`), legs, back and arms, with a weapon out or not (its arms hidden then): the player models' own hold a
weapon out, and lean their back into view. Bones are matched by name, the same biped's first (the hologram's and the
player models' are Bip01, the lab coat's Bip02).

The lab coat Gordon is Blue Shift's (`models/gordon_scientist.mdl`). The server precaches it and the hologram with the
body on, last and only while the map has room, like the effect sprites (W_Precache). Where they aren't, Gordon has no
body before the suit, the other models' weaponless arms keep their own poses, and in water each its own treading. With
the body off, nothing is added or precached.

Barney without his vest is Blue Shift's guard with no vest or helmet (Unified's `models/dead_barney.mdl`: despite the
name a full model with idles, a walk and a jog), drawn the way the lab coat is: his own standing, walking and running,
the player model's jumping, crouching and treading water. Blue Shift starts with the HUD and its vest is its armor, so
he's in his shirt from the moment his armor reaches 0 until he picks up a vest (`item_armorvest`); a helmet alone or a
charger doesn't put it on. A level change keeps it as it was; a loaded save shows the vest if there's any armor. The
server precaches him only in maps where the player is Barney, under the same room rule; where he isn't, Barney keeps
his vest. His hands are the vested model's: dead_barney's own are pale, low-poly blocks, so they're hidden and the
player model's hands and fingers are drawn on his wrists, bone for bone by name (the sleeves are the same in both).

Adrian in boot camp before he puts on his vest (its suit, picked up in ofboot2) wears the recruits' training clothes
(Opposing Force's `models/recruit.mdl`). He starts ofboot0 standing at ease as the other recruits do (the recruit's own
standing, hands behind his back) until he first moves; from then on he stands, walks and runs exactly as Adrian does,
Adrian's legs and the lab coat's arms over them (the recruit's own walk has its hips 17 units ahead of its standing,
and its run twists them), and jumps, crouches and treads water as the others. Only in boot camp's maps: the campaign's Osprey ride and
its crash, before the vest is found again, have no suit either, but there he's in his fatigues. The recruit's skeleton
uses 3ds Max Biped's newer bone names (Thigh, Calf, Clavicle, UpperArm, Forearm); they're read as the player models'
older ones (Leg, Leg1, Arm, Arm1, Arm2), so the borrowed poses and the hidden arms find the same bones.

**Left as it is:** a little of the HEV suit's and Barney's neck skin looking straight down while running (it would
take edited copies of Valve's textures), and for a frame or two at the end of a jump, a little of Barney's back.

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
  4 units, 24 bits): 0.1 unit up close, about 0.25 at 1,000 units, at most 2. It is only moved while at rest: where the
  map placed it (a door closed), or anywhere it has stood still a quarter of a second, moved or turned (a door open, a
  lift at the top, c1a0's door wheels turned against the wall), worked out the first time it stops there, up to 32
  places an entity. Moving, it's left alone. Turned about its side (pitch: a door hinged on its Y axis) it's left alone
  too, as the renderer may turn those the other way from the collision code this follows. The map's `angles` aren't
  read: doors take them as the way they move.
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
differ, in two settings, and 16-bit color on its own:
* `jhl_visuals_renderfixes` (default 1): models lit as the world is, and with texture filtering off textures
  point sampled with mipmaps and the sky pixelated like the world: OpenGL made consistent with itself.
* `jhl_visuals_softwarewater` (default 1): the water's ripples, and under water the 320 × 200 view and fog.
* `jhl_visuals_16bitcolor` (default 1): the 16-bit frame.

The OpenGL drawing they need is in `jhl_gl.cpp` (looked up through SDL;
the client doesn't link OpenGL), the map reading in `jhl_bsp.cpp` (shared with the flush faces above).

All three only act on the OpenGL renderer: everything they do goes through `JhlGL_Available`, which is false unless
`IEngineStudio.IsHardware()` says OpenGL (and the functions were found), and `gl_texturemode`, which the software
renderer doesn't have. On the software renderer (`-soft`) they do nothing, whatever they're set to, and it draws as
it always did. The flush faces above act on both, as both had the problem.

They work with the `-nofbo` launch option too (README, "For the 1998 look"): with it the engine draws straight to the
window, framebuffer 0, and the passes that copy the view or its depth read that framebuffer as they find it
(`CopyDepth` asks for the default framebuffer's `GL_DEPTH` there).

### Model lighting (`jhl_visuals_renderfixes`)

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

### Under water (`jhl_visuals_softwarewater`)

In water, slime or lava, software drew the view into Quake's warp buffer, 320 × 200, never raised: narrowed to 320
wide keeping its shape, then no more than 200 tall (its width kept, so its pixels aren't square: 4.5 × 5.4 at
1440 × 1080, measured on a screenshot), stretched over the screen, the HUD drawn sharp on top. And it fogged the view as
it stretched it: each pixel mixed toward the water's colour by one of 64 blends picked by its depth (32768 ÷ distance,
the last from 520 units in), `((64 − blend) × far >> 6) + near` of 256, never less than 32, near = density ÷ 2,
far = (512 − near) × density >> 8. OpenGL fogs linearly from the eye to 1536 − 4 × density instead. Both take colour
and density from the water's palette (entry 3's colour, entry 4's red), from the water drawn last.

* The view: once it's drawn (before the HUD), it's drawn again from the middles of those
  320 × 200 blocks (`JhlSoftwareLook_ViewDrawn`).
* The fog: the engine's own fog is kept off under water without touching the engine: OpenGL is
  told to fog by a fog coordinate of 0 (`GL_FOG_COORD_SRC`), so linear fog starting at 0 covers nothing, put back as
  soon as the view is out of the water (`JhlSoftwareLook_SetupView`). The view's depth is copied and software's fog
  put on by it, with the projection taken while the world is drawn; its colour from the water drawn here (the
  software water), else from the engine's fog set up. (Taking the engine's fog off a finished frame can't work: far
  enough out it has covered all there was.)

### Texture sampling and the sky (`jhl_visuals_renderfixes`)

The 25th anniversary menu's texture filtering is `gl_texturemode`: on, `GL_LINEAR_MIPMAP_LINEAR`; off, `GL_NEAREST`
(and `r_prefertexturefiltering 0`), point sampled without mipmaps, so distant textures sparkle. Software point sampled
with mipmaps: with filtering off, it's made `GL_NEAREST_MIPMAP_NEAREST` (filtering on
sets its own mode as ever; only this setting's change is undone).

The engine uploads the sky's six images smooth and without mipmaps, and `gl_texturemode` only reaches textures with
mipmaps, so with filtering off the world was pixelated and the sky smooth. Now the client finds the
engine's sky textures (the GL textures of the sky images' size whose picture is the image's, `gfx/env/<sky>*.tga`)
and gives them the world's filtering (`jhl_skyboxes.cpp`).

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

### Water in the dark (`jhl_visuals_waterlight`, default 1)

The map compiler gives water no lightmap, so both renderers draw it full bright: in a pitch-black corner (c1a1b's
tunnel) the water stood out of the dark. With the software water on, the water drawn here takes the light under it
(the engine's `LightAtPoint`: the lightmap of the floor below, its light styles as they are) on a 32-unit grid, each
point averaged with the light 24 units around it. Water brushes run into the walls, and light taken inside a wall is
black, so those samples are left out (a point all in a wall takes the light found stepping in toward its face's middle). From a light of 32 up
(c1a1b's lit tunnel is about 94) it's exactly as before; below that it darkens smoothly, to 0.25 with no light. The light is
taken once a map, at most 300 points a frame (water not yet taken is as bright as ever), and again
where a water entity moves (as gradually, its old light kept meanwhile); lights switched later aren't followed. The
flashlight and flashes aren't taken: dark water stays dark under them. `jhl_debug_waterlight` prints the light under
the view and the water's brightness for it, for tuning. With the software water off (or `-soft`) water is the engine's,
full bright as ever.

For the display (the stretch to the desktop, anti-aliasing), see the [front page](../README.md#recommended-launch-options-and-display).

## Known engine quirks, left as they are

* **A strip of water surface at the mouth of c3a2a/c3a2b's coolant tunnels.** The water brush's top (z -408) is just
  above the tunnel's ceiling (-416), so where the mouth's frame rises above it there's a real air gap, and its surface
  is drawn. The map's geometry, drawn correctly.

## Engine bugs fixed

Patched in memory as the client starts (`jhl_enginefixes.cpp`), on Windows and Linux, each only where the engine's code
is byte for byte as expected; the developer console says which were applied. Windows finds the places by their code;
Linux finds the engine's functions by name in hw.so's symbol table, then the places in them (checked against the real
hw.so and sw.so, not yet in a game on Linux).

* **A bright white, opaque sprite next to a colour-mode brush.** A brush entity drawn in rendermode 1 (Color) leaves the
  engine's second texture unit on and active. The next sprite drawn then set its texture mode on that unit just before
  the engine turned it off, so it was drawn with the previous sprite's `GL_REPLACE`: no tint, no transparency (hw.dll's
  R_DrawBrushModel and R_DrawSpriteModel). Vanilla has the same bug but few sprites to show it; johnhalflife's impact
  puffs and smoke showed it beside c3a1b's green goo, and could in the 9 other maps with such brushes (c1a2, c1a2b,
  c1a2c, c1a4b, c1a4d, c3a2b, of2a4, of2a6, of3a6). Now the sprite's drawing starts by turning that unit off (its first
  call goes through the client, which calls the engine's own GL_DisableMultitexture).

* **"AllocBlock: full" on a map change late in a long session.** The engine keeps
  every model loaded in a session in a table of 1024; full, it reuses an unused slot (an old map's brush models first)
  and copies in only the new name, so a sprite loaded there keeps the old map's surface count. Building lightmaps, the
  engine takes every model in the map's list with a surface count, walks the old map's freed surfaces, and overruns
  its 64 lightmap blocks with their garbage sizes. johnhalflife loads far more models than vanilla, so a session
  reaches 1024 sooner (seen at c2a3a; the map itself needs 10 blocks). Now those two engine loops take models of
  brush type instead, the same models as before minus the stale ones.

* **A hang on a map change late in a long session.** The same reused slots also kept the old model's claim on a block
  of the model cache. The engine then freed the wrong block for it, and a map's loading could go round forever moving
  the old one (seen loading of6a4b after 184 maps in one `sv_load_all_maps` sweep). Now a reused slot starts clean:
  the old model's cache block is freed and the rest of it cleared before the new name goes in (Mod_FindName's name
  copy goes through the client). With `-dev` the console names each reuse. Both renderers.
