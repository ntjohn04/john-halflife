# johnhalflife

An MMod-inspired mod for Half-Life, Opposing Force and Blue Shift, built on the Half-Life Unified SDK.
These pages record what johnhalflife changes compared with the stock SDK, and why.

* [Camera and movement](camera_movement.md)
* [Weapons](weapons.md)

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

All are client settings; set them in the console.

| Setting | Default | Turns on/off | Page |
|---|---|---|---|
| `jhl_camera_bob` | 1 | Dual-rate view bob and figure-eight weapon sway | [Camera](camera_movement.md#view-bob-and-weapon-sway) |
| `jhl_camera_viewroll` | 1 | Stronger strafe roll | [Camera](camera_movement.md#strafe-roll) |
| `jhl_camera_verticallag` | 1 | Eye lift while moving upward | [Camera](camera_movement.md#vertical-lag) |
| `jhl_camera_jumplandjolt` | 1 | Jump and landing view jolts | [Camera](camera_movement.md#jump-and-landing-jolts) |
| `jhl_visuals_weaponlag` | 1 | Weapon lags behind when turning | [Camera](camera_movement.md#weapon-lag-and-wall-push) |
| `jhl_visuals_weaponwallpush` | 1 | Weapon pulls back near walls | [Camera](camera_movement.md#weapon-lag-and-wall-push) |
| `jhl_weapons_smoothrecoil` | 1 | Spring-damped recoil (0 = original punches) | [Weapons](weapons.md#recoil) |
| `jhl_fx_bulletimpacts` | 1 | Bullet impact sparks/dust and water splashes | [Weapons](weapons.md#bullet-impacts) |
| `jhl_fx_muzzleflash` | 1 | Muzzle light and smoke, RPG backblast | [Weapons](weapons.md#muzzle-light-and-smoke) |
| `jhl_hud_scale` | 1 | Sprite HUD scale (0 = automatic by screen height) | [HUD](#hud) |

Changed defaults of existing settings: `cl_rollangle` 0.65 and `cl_rollspeed` 300 (see
[strafe roll](camera_movement.md#strafe-roll)).

## HUD

### HUD scale
`jhl_hud_scale` (0 = automatic by screen height)

The sprite HUD is laid out on a virtual screen (real size divided by the scale) and drawn scaled. Engine
text keeps its size; VGUI and the spectator HUD stay in real pixels. Crosshair size stays on
`crosshair_scale`. The crosshair itself is vanilla.

## MMod assets

`mmod_port/` tracks every MMod file the mod uses (28 files): `assets.json` lists them with SHA-256 hashes,
`ASSETS.md` is the generated report, and `port_assets.py install` copies them from a local MMod
installation into the mod directory. Each must be replaced, removed or cleared with its author before
anything is published. In short:

* **Movement:** jump, landing, long jump and wood footstep sounds, and MMod's texture material table.
* **Weapon switch:** 15 draw sounds.
* **Water splash:** the ripple ring sprite (optional; the splash works without it).

## Networking

Three user messages were added; everything else is client side or reuses existing sounds.

| Message | Size | Sent by | Purpose |
|---|---|---|---|
| `SmoothPunch` | 20 bytes | Server, on jump and landing | Spring-damped view jolt |
| `BulletHit` | 10 bytes | Server, `DecalGunshot` (NPCs, mounted guns) | Bullet impact effects |
| `WaterSplash` | 6 bytes | Server, NPC bullets entering water | Water splash |

User message names must be 11 characters or fewer; a longer name fails to register and sending it is
fatal ("bogus message type (0)").

Weapon prediction also syncs the current spread through `weapon_data_t.fuser4` (added to `delta.lst`).

## Open items

* Opposing Force weapons without an MMod counterpart still use the crowbar's switch sound as a placeholder.
* Settings menu and HUD work are next.
