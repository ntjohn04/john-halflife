# Physics

## Same at any frame rate (`jhl_physics_fpsfix`)

Some game code applies a fixed change once a frame, so it gets stronger or weaker with frame rate. Each fix makes it
act as at **72 fps**, Half-Life's 1998 `fps_max`. At 72 fps nothing changes.

Helpers in `shared/jhl_framerate.h`: `JhlReferenceDecay`, `JhlReferenceSettlingDecay`, `JhlReferenceFrames`.

| What | Problem | Where |
|---|---|---|
| Pushing and pulling boxes | Pulled boxes ran away, faster at high fps | `CPushable::Move` |
| Walking with +use | Much slower at high fps | `pm_shared.cpp` (`jfps` physics key) |
| Grenades, satchels, gibs, spores, sparks sliding | Stopped short at high fps | `BounceTouch`, `SlideTouch`, others |
| trigger_camera drift and turning | Frame-rate dependent | `CTriggerCamera::FollowTarget` |
| Client gibs, debris, shells settling | Extra tiny bounces at high fps | `HUD_TempEntUpdate` |
| Gun fire rate | Up to a frame lost each shot | `GetNextAttackDelay` |
| NPC turning, flier banking | Half-Life Updated's was as at 50 fps | `CBaseMonster::ChangeYaw` |
| Gargantua stomp speed | Half-Life Updated's was as at 40 fps | `CStomp::Think` |
| Electrified wires | Half-Life Updated's was 30 damage a second | `CRopeSegment::Touch` |
| Riding lifts | Float drift sank riders into the floor: crushed and hurt (ba_elevator) | `JhlRaiseRider`, `JhlRaiseRiders` |
| NPCs on lifts and trains | Lost their footing every frame: bouncing, left behind | `JhlRaiseRiders` |

## No extra speed along walls (`jhl_physics_wallfix`)

* Walking at an angle into a wall built speed up to 1.4×. Now ground acceleration stops at the speed asked for.
* Ladders: two keys or an angle climbed up to 1.4×. Now capped at the climbing speed.
* Air movement is untouched: bunny hopping and air strafing work as vanilla.
* `PM_WalkMove`, `PM_LadderMove`, through the `jwf` physics key. 0 restores vanilla.
