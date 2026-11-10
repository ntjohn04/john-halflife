# John Half-Life's model sources

John Half-Life ships the Unified SDK's models (the release packager takes them from its release), except the ones built
here, which go over them: each `models/<folder>/<name>.qc`, built by `scripts/build_models.py` into `config/mod/models`. Each
builds against the Unified SDK's shared model sources (its scientists' body, animations and textures), so the build
needs a checkout of its assets repository (https://github.com/twhl-community/halflife-unified-sdk-assets); the built
models are committed, so the game's build and the packager don't.

```
python scripts/build_models.py ../halflife-unified-sdk-assets [folder ...]
```

## icons

Each game folder's icon, `icons/<folder>.png` (square, 256x256 or more), written by `scripts/make_icons.py` into
its config as `game.ico`, `game.tga` (16x16) and `game_big.tga` (32x32, Steam's library icon). A hand-made small
version, `icons/<folder>_16.png` or `_32.png`, is used as it is for that size. The four must differ from each other.
`icons/johnhalflife_icons.pdn` is the Paint.NET source, its layers the four pictures: export each as its PNG.

## rosenberg

The Unified SDK's Rosenberg (its `modelsrc/models/rosenberg/rosenberg_ld.qc`) with a low-definition head in place of
Blue Shift's, whose face (128x128) was twice the detail of any other low-definition head: "Custom LD Rosenberg", from
[Improved LD Scientists + Civilians](https://gamebanana.com/mods/238622) by Paynamia, CC BY-NC 4.0 (credited in
`config/licenses/CREDITS.txt`). Its body, needle, items, hitboxes and all 209 animations are the Unified SDK's.

The head (`meshes/sci_ld_rosehead_roseglass.smd`) and its textures came out of that mod's `bshift_addon/models/
scientist.mdl` (its `bs_improved_scis_ld_rose.zip`) by `scripts/mdl2smd.py`:

```
python scripts/mdl2smd.py scientist.mdl sci_ld_rosehead_roseglass "<assets>/modelsrc/models/rosenberg/meshes/ld/dc_sci_(head_ROSENBERG).smd" sci_ld_rosehead_roseglass.smd textures --offset 0 0.4289 -0.3316 "Mouth=Bone01" "Bip01 Spine3=Bip02 Spine2"
```

It's placed as the Unified SDK placed Blue Shift's head on its low-definition body: where it sits on Blue Shift's
skeleton, moved by (0, 0.4289, -0.3316), on the matching bones of Half-Life's. That offset is the Unified SDK's own,
measured: Blue Shift's Rosenberg head taken out the same way matches its head mesh to 0.03 units on average. The mod's
head sits where Blue Shift's did (within 0.06 units), on a body whose neck is Blue Shift's.

The mod made its body's neck hole meet its simplified neck; the Unified SDK's body (Half-Life's) has a wider one,
and not a simple ring: the Unified SDK's Blue Shift head isn't welded to it, its neck reaches down into the collar
and its V and covers it. So the new head's neck is lengthened down to where that head's ended, by
`scripts/smd_neck_bridge.py` (a strip from its bottom edge to the other's, that edge's places and bones, textured with
the neck's last row of skin):

```
python scripts/smd_neck_bridge.py sci_ld_rosehead_roseglass.smd "<assets>/modelsrc/models/rosenberg/meshes/ld/dc_sci_(head_ROSENBERG).smd" sci_ld_rosehead_roseglass.smd
```

The walk and run speeds are given their vanilla values by `restore_npc_speeds.py` (`npc_speeds.txt`, Rosenberg on the
scientists' animations), as every Unified SDK model is.

## harold

Blue Shift's Harold (ba_yard3's wounded scientist, `dying_scientist_1`, and `dying_scientist_fake`, the stand-in the
map swaps him for as he dies) was the glasses scientist's head, like any other. John Half-Life gives him his own:
that head made bald and without his glasses, on the Unified SDK's low-definition scientist (`harold.qc`, from its
`scientist_ld.qc`) and its cowering one (`harold_cower.qc`, from `scientist_cower_ld.qc`). The first start's map
conversion points those two entities at them (`BaYard3HaroldModel`, `client/jhl_mapconvert.cpp`); no other
scientist changes.

The head (`meshes/harold_head.smd`) is the Unified SDK's `SCI3_Template_Biped1(NerdHead).smd` reshaped in Blender:
the hair, a raised shelf round the back of the head, brought down to the skull, and the glasses removed. Its side
and back texture (`textures/harold_side.bmp`, from `SCI3X_HSide1.bmp`) was painted over to bare skin. The round
trip went through `scripts/smd_obj.py` (an SMD as an OBJ for Blender, the bones put back from the original's
nearest vertices):

```
python scripts/smd_obj.py export <folder> harold <head.smd> <body.smd> --textures <texture folders>
python scripts/smd_obj.py import <edited.obj> <head.smd> harold_head.smd
```
