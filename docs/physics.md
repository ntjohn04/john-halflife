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
| NPCs turning (every NPC), the ichthyosaur and fliers banking | A turn's step every think of yaw speed × frametime × 10 (banking 220 × frametime): turning slower the higher the fps. Half-Life Updated made it steady, but as at 50 fps (banking at 10) | As at 72 fps: grunts turn 0.7× as fast as Half-Life Updated's | `CBaseMonster::ChangeYaw`, `CFlyingMonster::ChangeYaw`, `CIchthyosaur` |
| The gargantua's stomp speeding up | Its speed added to every think by frametime. Half-Life Updated: steady, as at 40 fps | As at 72 fps | `CStomp::Think` |
| Electrified wires (Opposing Force) | 1 damage a touch, every frame: 72 a second at 72 fps. Half-Life Updated: 30 a second (30 fps) | 36 every half second (72 a second) | `CRopeSegment::Touch` |
| trigger_camera turning | Error × 40 × frametime. Half-Life Updated: as at 100 fps | As at 72 fps | `CTriggerCamera::FollowTarget` |
| Riding elevators | A lift adds its move to its riders' origins each frame. Far from the map's centre a float holds an origin to about 1/4000 unit, but the lift's (near 0) much finer, so a rider's height above the floor drifts. With small moves the drift builds one way until the rider is a hair into the floor, and every push after ends stuck: the lift is blocked every frame and the rider hurt (ba_elevator at 240 fps: the essential scientist crushed, the player hurt a point at a time). Once the lift stops nothing blocks it, and an NPC left in the floor never stands: it falls a frame's gravity every frame, and its corpse sinks through the floor touching triggers as it goes (ba_elevator's scientist, killed by the map's shock trigger, hurt by it again dead and gibbed) | A rider over the mover's floor that a raise of at most a unit frees is raised at the start of the next frame (the engine puts it back after `Blocked`) and not hurt; one blocked otherwise is blocked and hurt as before. Each frame, an NPC or corpse stuck in nothing but the floor of the mover it's on, moving or not, is raised out by at most a unit | `JhlRaiseRider`, in `DispatchBlocked`; `JhlRaiseRiders`, `JhlUnstickRider` |
| NPCs carried by lifts and trains | The engine takes a walking NPC's standing flag away each time a mover carries it (players keep theirs), so the next frame it isn't carried: a lift going down drops away and the NPC falls after it, about 1.5 units, landing eight times a second (ba_elevator's scientists bouncing); a train leaves it behind until a wall shoves it along (the rubber banding of NPCs in trains). | Each frame, before movers move, a walking NPC whose ground is a moving mover and that's no more than 4 units over its floor is set down on it standing, so it's carried every frame. With `jhl_debug_blocked` on, one left off is printed with why | `JhlRaiseRiders` |

The engine itself is fine:
* Current builds carry the leftover fraction of each millisecond into the player's input frames, so there's no
  slow motion between 1000/n fps.
* Half-Life Updated and the Unified SDK already made NPC turning, the gargantua's stomp, trigger_camera
  rotation, ropes and electrified wire independent of frame rate.

## No extra speed walking into a wall or climbing (`jhl_physics_wallfix`, default 1)

Walking at an angle into a wall, the wall takes away the speed going into it each frame and accelerating puts it
back. So along the wall the speed builds until its part in the direction pressed is full walking speed: about
1.4 times as fast at 45 degrees. It's the ground version of air strafing, and some players use it on purpose.

With the setting on, accelerating on the ground goes no faster than the speed asked for, or than the player was
already going: a landing, a push or an explosion keeps its speed and loses it to friction as before. Air
movement isn't touched, so bunny hopping and air strafing are as vanilla. `pm_shared.cpp`'s `PM_WalkMove`,
through the `jwf` physics key so prediction agrees. Set it to 0 for vanilla's wall running.

Ladders have the same kind of extra speed. Each movement key asks for the full climbing speed (200), so two at once
ask for 1.4 times it, and the part going into the ladder's face is turned into climbing on top of any already along
it. Turned about 45 degrees to the ladder with two keys held, or looking up or down at an angle, you climbed up to
1.4 times as fast. With the setting on, climbing goes no faster than the climbing speed, whatever's pressed, its
direction kept; jumping off and stepping away at the bottom are as before (`PM_LadderMove`).
