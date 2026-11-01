# Camera and movement

Reimplemented from MMod's behaviour as specified in `re/specs/camera_movement.md`. The code lives in
`src/game/client/view.cpp` (camera), `src/game/server/entities/player.cpp` (jolts) and
`src/game/shared/player_movement/pm_shared.cpp` (sounds). Settings are listed on the [front page](../../README.md#settings).

## Camera

### Jump and landing jolts
`jhl_camera_jumplandjolt`

* The view gets a short spring-damped kick (`V_SmoothPunch`) on jumping and on landing, instead of the
  vanilla instant punch.
* The landing jolt dips the view in proportion to fall speed (from 75 units/s up), with spring 50 and
  damping 8.
* The server sends the jolt to the client with the `SmoothPunch` message; the vanilla landing and long
  jump punches in `pm_shared.cpp` are removed. The client decides whether to show it, so the setting is
  per player.
* The same spring drives [weapon recoil](weapons.md#recoil).

### Strafe roll
`jhl_camera_viewroll`

* Strafing tilts the view noticeably more: the roll is four times the normal view-relative roll, with
  the normal roll added on top.
* Defaults changed to `cl_rollangle 0.65` and `cl_rollspeed 300`.

### View bob and weapon sway
`jhl_camera_bob`

* Two bob signals: a slower main bob and a side bob at twice its rate, giving the weapon a figure-eight
  sway while walking.
* Rates are scaled to 100 fps so the motion doesn't change with frame rate.

### Vertical lag
`jhl_camera_verticallag`

* The eyes lift slightly while moving upward (jumping, climbing stairs, lifts).

### Weapon lag and wall push
`jhl_visuals_weaponlag`, `jhl_visuals_weaponwallpush`

* The weapon trails the view when turning and catches up faster the further the view has turned away.
* Close to a wall, the weapon pulls back instead of clipping into it.

## Movement sounds

Uses MMod sounds tracked by `mmod_port` (see the [index](README.md#mmod-assets-the-checklist)).

* **Jump:** an effort sound (`pl_jump1-3`), followed by a quieter push-off footstep.
* **Landing:** `pl_jumpland1-3` on landing from a jump or fall.
* **Long jump:** `items/power_jump.wav`.
* **Wood footsteps:** new `STEP_WOOD` type with `pl_wood1-4`.
* **Flesh footsteps:** new `STEP_FLESH` type, using the stock `pl_slosh1-4` pitched down for a squelchy
  sound, on flesh and organic surfaces.
* **More surface types:** MMod's texture material table is converted to JSON
  (`sound/mmod_materials.json`) and loaded after the SDK's own, so its assignments take precedence. Its
  extra material letters are mapped to step types and volumes.
