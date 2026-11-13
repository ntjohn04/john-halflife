# Camera and movement

Code: `client/view.cpp` (camera), `server/entities/player.cpp` (jolts), `shared/player_movement/pm_shared.cpp`.

## Camera

* **Jump and landing jolts** (`jhl_camera_jumplandjolt`): a spring-damped kick (`V_SmoothPunch`) instead of the
  instant punch. Landing dips by fall speed (from 75 u/s; spring 50, damping 8). Sent by `SmoothPunch`. Also a sound
  for the long jump module's jump.
* **Strafe roll** (`jhl_camera_viewroll`): four times the normal roll, plus the normal roll.
  `cl_rollangle 0.65`, `cl_rollspeed 300`.
* **View bob** (`jhl_camera_bob`): a main bob and a side bob at twice its rate, for a figure-eight weapon sway.
  Scaled to 100 fps.
* **Vertical lag** (`jhl_camera_verticallag`): the eyes lift slightly moving up (jumps, stairs, lifts).
* **Weapon lag** (`jhl_visuals_weaponlag`): the weapon trails the view when turning.
* **Wall push** (`jhl_visuals_weaponwallpush`): the weapon pulls back near walls.

## Movement

* **Walk toggle** (`jhl_input_walktoggle`): the walk key toggles walking.
* **Walk fix** (`jhl_input_walkfix`): walking diagonally no faster than straight (vanilla: 1.41×).
* **Flesh footsteps:** `STEP_FLESH`, the stock `pl_slosh1-4` pitched down.
* Computer floors step like metal.
