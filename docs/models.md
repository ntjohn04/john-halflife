# Models

## Changes to the Unified SDK's models

The Unified SDK's models keep vanilla's textures and sequences and add some.

* **NPC speeds:** its recompiled models walked 2-6% slow. `scripts/restore_npc_speeds.py` puts vanilla's back.
* **Rosenberg:** a low-definition head instead of Blue Shift's high-definition one (Paynamia, CC BY-NC 4.0).
* **Blue Shift helmet and vest:** low-definition ones (Paynamia's helmet, Evy Houstuvk's vest).
* **Blue Shift chair:** Evy Houstuvk's low-definition one.
* **Harold** (ba_yard3): his own bald head, without glasses.
* **Blue Shift viewmodel arms:** the small arm textures swapped in (`scripts/lowdef_arms.py`).
* **Glass shards:** the chrome flag set (its texture was misnamed `chome_glass`).
* **Shells:** Half-Life's own `shell.mdl` restored over Opposing Force's.

Credits: `config/licenses/CREDITS.txt`.

## Model sources (`assets/models/`)

Built by `scripts/build_models.py` into `config/mod/models`. Needs the Unified SDK's assets repository:

```
python scripts/build_models.py ../halflife-unified-sdk-assets [folder ...]
```

* **rosenberg:** the Unified SDK's Rosenberg with Paynamia's head. Head extracted by `scripts/mdl2smd.py`, placed with
  the Unified SDK's own offset (0, 0.4289, -0.3316), neck lengthened by `scripts/smd_neck_bridge.py`.
* **harold:** the Unified SDK's low-definition scientist with the nerd head made bald in Blender (round trip:
  `scripts/smd_obj.py export` / `import`). Pointed to by the first start's map conversion (`BaYard3HaroldModel`).

## Icons (`assets/icons/`)

* `icons/<folder>.png` (256×256 or more), written by `scripts/make_icons.py` as `game.ico`, `game.tga`,
  `game_big.tga`.
* Optional hand-made small versions: `<folder>_16.png`, `<folder>_32.png`.
* Source: `icons/johnhalflife_icons.pdn`.
