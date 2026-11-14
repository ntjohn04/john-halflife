# Models

## Changes to the Unified SDK's models

Its models keep vanilla's textures and sequences and add some. Credits: `config/licenses/CREDITS.txt`.

* **NPC speeds:** its recompiled models walked 2-6% slow; `scripts/restore_npc_speeds.py` restores vanilla's.
* **Rosenberg:** a low-definition head (Paynamia, CC BY-NC 4.0).
* **Blue Shift helmet, vest and chair:** low-definition (Paynamia's helmet, Evy Houstuvk's vest and chair).
* **Harold** (ba_yard3): his own bald head, without glasses.
* **Blue Shift viewmodel arms:** the small arm textures (`scripts/lowdef_arms.py`).
* **Glass shards:** chrome flag set (the texture was misnamed `chome_glass`).
* **Shells:** Half-Life's `shell.mdl` restored over Opposing Force's.

## Model sources (`assets/models/`)

Built by `scripts/build_models.py` into `config/mod/models`. Needs the Unified SDK's assets repository:

```
python scripts/build_models.py ../halflife-unified-sdk-assets [folder ...]
```

* **rosenberg:** the Unified SDK's Rosenberg with Paynamia's head, extracted by `scripts/mdl2smd.py`, placed at the
  Unified SDK's offset (0, 0.4289, -0.3316), neck lengthened by `scripts/smd_neck_bridge.py`.
* **harold:** the Unified SDK's low-definition scientist with the nerd head made bald in Blender
  (`scripts/smd_obj.py export` / `import`). Used by the first start's map conversion (`BaYard3HaroldModel`).

## Icons (`assets/icons/`)

* `icons/<folder>.png` (256×256 or more) → `game.ico`, `game.tga`, `game_big.tga` by `scripts/make_icons.py`.
* Optional hand-made small versions: `<folder>_16.png`, `<folder>_32.png`.
* Source: `icons/johnhalflife_icons.pdn`.
