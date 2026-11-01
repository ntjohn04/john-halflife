# Physics

## The same at any frame rate (`jhl_physics_fpsfix`, default 1)

Some of Half-Life's game code applies a fixed change once a frame against forces that act over time. A frame
here is a player input frame, or the touch the engine sends every frame something slides along the floor. So
how strong the change is depends on the frame rate, and it gets worse the higher the frame rate goes. Each
of these is made to act at any frame rate as it did at **72 fps**: Half-Life's default `fps_max` in 1998
(inherited from QuakeWorld), the rate the game was made to run at. At 72 fps nothing changes.

The helpers live in `shared/jhl_framerate.h`:
* **`JhlReferenceDecay`:** for friction applied once a frame. Over time it takes off as much as at 72 fps, so a
  sliding thing stops where it did.
* **`JhlReferenceSettlingDecay`:** for a slowdown each frame against a pull over time. The speed they settle at
  matches 72 fps.
* **`JhlReferenceFrames`:** for something added each frame. It adds as much a second as at 72 fps, and below
  72 fps it isn't scaled up.

| What | Vanilla | Was | Where |
|---|---|---|---|
| Pushing and pulling boxes and barrels (`func_pushable`) | The player's velocity added every frame they push or pull (+use). Pulling had no speed limit, and the player is given the box's speed back each frame, so it ran away, faster the higher the fps. | Adds as at 72 fps; pulled no faster than pushed | `CPushable::Move` |
| Walking while holding +use | Velocity × 0.3 every input frame: about 6 u/s at 240 fps against about 19 at 72 | Settles at the 72 fps speed | `pm_shared.cpp`, through the `jfps` physics key so prediction agrees |
| Grenades, satchels, gibs, spores, sparks sliding to a stop | Their friction (× 0.8, 0.95, 0.9, 0.5, 0.1) on every touch, and they touch the floor every frame they slide: at 240 fps a grenade stops in about a third of the 72 fps distance | Stops where it did at 72 fps | `BounceTouch`, `SlideTouch`, `SatchelSlide`, `BounceGibTouch`, `CSpore`, `CShower` |
| trigger_camera drift (c2a3e, c5a1) | Velocity × 0.8 every frame | As at 72 fps | `CTriggerCamera::FollowTarget` |
| Client gibs, debris and shells coming to rest | Only when they hit the floor slower than three frames of gravity, so at high fps they make extra tiny bounces (and bounce sounds) first | No slower than three 72 fps frames of gravity | `HUD_TempEntUpdate` |
| Fire rate of the guns | A shot due partway through a frame goes with the frame and the time it waited is dropped: up to a frame lost each shot | The time it waited comes off the next shot (client and server, so prediction agrees) | `GetNextAttackDelay`, used by the automatic and repeating guns |

The engine itself is fine:
* Current builds carry the leftover fraction of each millisecond into the player's input frames, so there's no
  slow motion between 1000/n fps.
* Half-Life Updated and the Unified SDK already made NPC turning, the gargantua's stomp, trigger_camera
  rotation, ropes and electrified wire independent of frame rate.
