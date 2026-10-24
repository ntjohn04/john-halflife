/***
 *
 *	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

#include "hud.h"
#include "com_model.h"
#include "event_api.h"
#include "jhl_explosion.h"
#include "jhl_explosions.h"
#include "jhl_settings.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "r_efx.h"

namespace
{
struct Fireball
{
	const char* Sprite;
	float Width; // Frame size in pixels; frames are centered on the origin.
	float Height;
	int Frames;
	float Fill = 1; // How much of the frame the fire fills, relative to the vanilla fireball (by eye).
};

// The vanilla fireball: 64 px, 15 frames at 15 fps (1 s).
constexpr Fireball Vanilla{"sprites/zerogxplode.spr", 64, 64, 15};

// Round fireballs, which look right anywhere.
constexpr Fireball RoundFireballs[] = {
	{"sprites/bexplo.spr", 64, 64, 24, 0.8f},
	{"sprites/cexplo.spr", 80, 80, 25, 0.85f},
	{"sprites/dexplo.spr", 80, 80, 26, 0.7f},
	{"sprites/eexplo.spr", 80, 80, 25, 0.85f},
};

// Fireballs that rise from the bottom of the frame (a wide blast, a fire column, a mushroom cloud), so they stand on a
// floor. The mushroom (the last) is for the biggest explosions only.
constexpr Fireball ShapedFireballs[] = {
	{"sprites/fexplo.spr", 80, 96, 30},
	{"sprites/gexplo.spr", 80, 96, 23},
	{"sprites/hexplo.spr", 64, 96, 30},
};

// Explosions sent at 15 fps get two fireballs: grenades, rockets, satchels, tripmines, mortars, env_explosion and the
// crossbow bolt. Opposing Force's mortar (10 fps) keeps the vanilla fireball.
constexpr float JhlFramerate = 15;

// A falling helicopter (Apache, Osprey) sends one fireball every 0.2 s under its hull, scale 3-5.9 at 12 fps (1.25 s);
// each is drawn from the vanilla and round fireballs instead.
constexpr float FallingFramerate = 12;
constexpr float FallingSizeMin = 1;
constexpr float FallingSizeMax = 1.2f;

constexpr float LargeScale = 6;	  // Satchel, tripmine, Apache rocket, env_explosion magnitude 150+.
constexpr float BiggestScale = 9; // func_mortar, env_explosion magnitude 200+: the mushroom cloud joins the pool.
constexpr float RocketScale = 1.5f;

constexpr float Jitter = 0.1f;		 // Scale and frame rate, both fireballs.
constexpr float PositionJitter = 4; // Units, the vanilla fireball.

// The second fireball, relative to the first's size; delayed a little, set off sideways and up (on a floor, down) by a
// fraction of the first's size, dimmer (the vanilla fireball is additive at 180) since the two add up.
constexpr float RoundSecondSizeMin = 0.9f;
constexpr float RoundSecondSizeMax = 1.1f;
constexpr float ShapedSecondSizeMin = 0.8f;
constexpr float ShapedSecondSizeMax = 1.0f;
constexpr float SecondDelay = 0.1f;
constexpr float SecondSideways = 0.06f;
constexpr float SecondUp = 0.1f;
constexpr float SecondDownOnFloorMin = 0.05f;
constexpr float SecondDownOnFloorMax = 0.15f;
constexpr int SecondBrightness = 140;

// How long the second fireball plays: about as long as the vanilla one, longer for large explosions.
constexpr float SmallDurationMin = 0.9f;
constexpr float SmallDurationMax = 1.3f;
constexpr float LargeDurationMin = 1.6f;
constexpr float LargeDurationMax = 2.0f;

// The sprite's bottom rows are mostly empty, so a shaped fireball sinks this much of its height into the floor.
constexpr float ShapedSink = 0.05f;

// The lingering smoke (settings in jhl_explosions.h): Opposing Force's single-frame puff, so it can grow and fade
// smoothly.
constexpr const char* LingeringSmokeSprite = "sprites/wep_smoke_01.spr";
constexpr float LingeringSmokeSpriteWidth = 64;
constexpr float LingeringSmokeHeight = 20;
constexpr float LingeringSmokeFadeIn = 0.05f; // Of its life.
constexpr float LingeringSmokeDriftDecay = 0.3f; // Per second: drifting 40 units/s, it goes about 130 units.

// Spark showers, instead of the server's spark_shower entities (vanilla: 0-3, thrown off the surface at 200-300
// ±100 sideways +200 up, half gravity, sparking every 0.1 s for 0.5-1.5 s; each burst the engine's TE_SPARKS, 8
// streaks at ±200 and a richo1 flash of scale 0.5-1). Each shower is one of these kinds, so an explosion mixes
// sparks that fly far, ones like vanilla's and ones that drop; then picks its own values from the kind's ranges.
struct SparkKind
{
	float Chance;
	float SpeedMin, SpeedMax; // Off the surface.
	float UpMin, UpMax;		  // Added straight up (down off a ceiling).
	float LifeMin, LifeMax;
	bool FullGravity; // Otherwise half, as vanilla.
	float Cone;		  // How far from the surface's direction it may leave: 0 straight out, 1 up to about 55 degrees.
};

constexpr SparkKind Flyer{0.3f, 350, 550, 150, 300, 1, 2.25f, false, 0.35f};
constexpr SparkKind VanillaLike{0.4f, 200, 300, 150, 250, 0.5f, 1.5f, false, 0.5f};
constexpr SparkKind Dropper{0.3f, 50, 150, 0, 100, 0.4f, 1.2f, true, 0.9f};

constexpr int SparkShowersMin = 0;
constexpr int SparkShowersMax = 10;
constexpr int SparkShowersForEveryKind = 3; // From this many, there's always a flyer and a dropper.
constexpr float SparkIntervalMin = 0.07f;
constexpr float SparkIntervalMax = 0.11f;
constexpr float SparkBounce = 0.5f;
constexpr int SparkStreaksMin = 6; // Per burst.
constexpr int SparkStreaksMax = 12;
constexpr int SparkStreakSpeedMin = 150;
constexpr int SparkStreakSpeedMax = 300;
constexpr float SparkFlashMaxMin = 0.4f; // Each shower's biggest flash; each flash is half that to that.
constexpr float SparkFlashMaxMax = 1;

float Random(float low, float high)
{
	return gEngfuncs.pfnRandomFloat(low, high);
}

void SpawnFireball(const Vector& origin, int model, float scale, float framerate, int brightness, float roll)
{
	TEMPENTITY* sprite = gEngfuncs.pEfxAPI->R_DefaultSprite(origin, model, framerate);

	if (!sprite)
		return;

	gEngfuncs.pEfxAPI->R_Sprite_Explode(sprite, scale, TE_EXPLFLAG_NONE);
	sprite->entity.curstate.renderamt = brightness;
	sprite->entity.angles.z = roll;
}

// A fireball that plays a moment later (curstate.fuser1: when; iuser1: model; fuser2: scale; fuser3: frame rate;
// iuser2: brightness; fuser4: roll).
void DelayedFireball(TEMPENTITY* ent, float frametime, float currenttime)
{
	if (currenttime < ent->entity.curstate.fuser1)
		return;

	const auto& state = ent->entity.curstate;
	SpawnFireball(ent->entity.origin, state.iuser1, state.fuser2, state.fuser3, state.iuser2, state.fuser4);
	ent->die = currenttime;
}

// Where a floor is under an explosion, if one is close enough for it to have hit it: explosions are placed
// (dmg - 24) * 0.6 units out of what they hit, which is scale * 10 + 15.6.
bool FindFloor(const Vector& origin, float scale, Vector& floor)
{
	const Vector end = origin - Vector{0, 0, scale * 10 + 48};

	pmtrace_t tr;
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(origin, end, PM_WORLD_ONLY, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.fraction >= 1 || tr.plane.normal.z < 0.7f)
		return false;

	floor = tr.endpos;
	return true;
}

// Grows and fades the lingering smoke (curstate.fuser1: start time; fuser2: life; fuser3, fuser4: start and end scale;
// iuser1: peak opacity).
void LingeringSmokeThink(TEMPENTITY* ent, float frametime, float currenttime)
{
	const auto& state = ent->entity.curstate;
	const float t = (currenttime - state.fuser1) / state.fuser2;

	if (t < 0)
	{
		ent->entity.curstate.renderamt = 0;
		return;
	}

	// Fades in quickly, then thins out the whole time it spreads.
	float opacity;

	if (t < LingeringSmokeFadeIn)
	{
		opacity = t / LingeringSmokeFadeIn;
	}
	else
	{
		const float remaining = std::max(0.f, (1 - t) / (1 - LingeringSmokeFadeIn));
		opacity = remaining * remaining;
	}

	// Spreads fast at first, then slower.
	const float spread = 1 - (1 - std::min(1.f, t)) * (1 - std::min(1.f, t));

	ent->entity.curstate.renderamt = static_cast<int>(state.iuser1 * opacity);
	ent->entity.curstate.scale = state.fuser3 + (state.fuser4 - state.fuser3) * spread;

	// The drift dies down (the rise doesn't).
	const float slowdown = std::max(0.f, 1 - LingeringSmokeDriftDecay * frametime);
	ent->entity.baseline.origin.x *= slowdown;
	ent->entity.baseline.origin.y *= slowdown;
}

// Which way is away from walls near a point (horizontally; zero if there are none in range): the sum of the directions
// to the walls found by 8 traces, reversed and weighted by closeness.
Vector AwayFromWalls(const Vector& origin, float range)
{
	Vector away = g_vecZero;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	for (int i = 0; i < 8; ++i)
	{
		const float angle = i * 0.7853982f; // 45 degrees.
		const Vector direction{std::cos(angle), std::sin(angle), 0};

		pmtrace_t tr;
		gEngfuncs.pEventAPI->EV_PlayerTrace(origin, origin + direction * range, PM_WORLD_ONLY, -1, &tr);

		if (tr.fraction < 1)
			away = away - direction * (1 - tr.fraction);
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();

	return away.Length() > 0.01f ? away.Normalize() : g_vecZero;
}

} // namespace

bool JhlExplosion_LingeringSmoke(const Vector& origin, float scale, const JhlLingeringSmoke& settings)
{
	const int model = gEngfuncs.pEventAPI->EV_FindModelIndex(LingeringSmokeSprite);

	if (model == 0)
		return false;

	const float life = Random(settings.LifeMin, settings.LifeMax);
	// Drifts a random way, pushed away from a wall the explosion hit (explosions are (dmg - 24) * 0.6 units out of
	// what they hit, which is scale * 10 + 15.6).
	const Vector away = AwayFromWalls(origin, scale * 10 + 48);
	const Vector velocity{(Random(-1, 1) + away.x) * settings.Drift, (Random(-1, 1) + away.y) * settings.Drift,
		Random(settings.RiseMin, settings.RiseMax)};

	TEMPENTITY* smoke = gEngfuncs.pEfxAPI->R_TempSprite(origin + Vector{0, 0, LingeringSmokeHeight}, velocity, 0, model,
		kRenderTransAlpha, kRenderFxNone, 0, settings.Delay + life, 0);

	if (!smoke)
		return false;

	const float now = gEngfuncs.GetClientTime();
	const auto gray = static_cast<byte>(gEngfuncs.pfnRandomLong(settings.GrayMin, settings.GrayMax));

	smoke->flags |= FTENT_CLIENTCUSTOM;
	smoke->callback = LingeringSmokeThink;
	smoke->entity.curstate.rendercolor = {gray, gray, gray};
	smoke->entity.curstate.renderamt = 0;
	smoke->entity.curstate.iuser1 = settings.Opacity;
	smoke->entity.curstate.fuser1 = now + settings.Delay;
	smoke->entity.curstate.fuser2 = life;
	smoke->entity.curstate.fuser3 = scale * settings.StartWidth / LingeringSmokeSpriteWidth;
	smoke->entity.curstate.fuser4 = scale * settings.EndWidth / LingeringSmokeSpriteWidth;
	smoke->entity.curstate.scale = smoke->entity.curstate.fuser3;
	smoke->entity.angles.z = Random(0, 360);
	return true;
}

namespace
{

// A spark shower bursting as it flies (entity.baseline: scale, next burst; framerate, time left; fuser1, interval;
// rendermode, streak speed; fuser2, biggest flash).
void SparkShowerThink(TEMPENTITY* ent, float frametime, float currenttime)
{
	auto& state = ent->entity.baseline;

	if (currenttime < state.scale)
		return;

	Vector origin = ent->entity.origin;
	gEngfuncs.pEfxAPI->R_SparkStreaks(origin, gEngfuncs.pfnRandomLong(SparkStreaksMin, SparkStreaksMax), -state.rendermode,
		state.rendermode);

	// The engine's own bursts use a copy of richo1 that maps don't precache; load it as a HUD sprite.
	if (const HSPRITE flash = gEngfuncs.pfnSPR_Load("sprites/richo1.spr"); flash > 0)
	{
		gEngfuncs.pEfxAPI->R_RicochetSprite(origin, const_cast<model_t*>(gEngfuncs.GetSpritePointer(flash)), 0.1f,
			Random(state.fuser2 * 0.5f, state.fuser2));
	}

	state.scale = currenttime + state.fuser1;
	state.framerate -= state.fuser1;

	if (state.framerate <= 0)
		ent->die = currenttime;
}

// Which way the surface an explosion hit faces, as far as the client can tell: up off a floor, away from a wall, down
// off a ceiling; zero in mid-air (as vanilla's trace that hits nothing).
Vector SurfaceNormal(const Vector& origin, float scale)
{
	const float range = scale * 10 + 48;

	if (Vector floor; FindFloor(origin, scale, floor))
		return {0, 0, 1};

	if (const Vector away = AwayFromWalls(origin, range); away != g_vecZero)
		return away;

	pmtrace_t tr;
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(origin, origin + Vector{0, 0, range}, PM_WORLD_ONLY, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	return tr.fraction < 1 ? Vector{0, 0, -1} : g_vecZero;
}

// Spark showers as vanilla's spark_shower throws them, each with its own values.
void SparkShowers(const Vector& origin, float scale)
{
	if (Vector point = origin; gEngfuncs.PM_PointContents(point, nullptr) == CONTENTS_WATER)
		return;

	const int count = gEngfuncs.pfnRandomLong(SparkShowersMin, SparkShowersMax);

	if (count == 0)
		return;

	// In mid-air, vanilla's sparks go up (its trace's normal is zero, and they get the upward push).
	Vector normal = SurfaceNormal(origin, scale);
	const bool ceiling = normal.z < 0;

	if (normal == g_vecZero)
		normal = {0, 0, 1};

	const float now = gEngfuncs.GetClientTime();

	for (int i = 0; i < count; ++i)
	{
		TEMPENTITY* ent = gEngfuncs.pEfxAPI->CL_TempEntAllocNoModel(origin);

		if (!ent)
			return;

		const SparkKind* kind;

		if (count >= SparkShowersForEveryKind && i < 2)
			kind = i == 0 ? &Flyer : &Dropper;
		else if (const float roll = Random(0, 1); roll < Flyer.Chance)
			kind = &Flyer;
		else if (roll < Flyer.Chance + VanillaLike.Chance)
			kind = &VanillaLike;
		else
			kind = &Dropper;

		const Vector direction = (normal + Vector{Random(-1, 1), Random(-1, 1), Random(-1, 1)} * kind->Cone).Normalize();
		Vector velocity = direction * Random(kind->SpeedMin, kind->SpeedMax);
		velocity.z += (ceiling ? -1 : 1) * Random(kind->UpMin, kind->UpMax);

		const float life = Random(kind->LifeMin, kind->LifeMax);
		const float interval = Random(SparkIntervalMin, SparkIntervalMax);

		ent->flags |= FTENT_NOMODEL | FTENT_CLIENTCUSTOM | FTENT_COLLIDEWORLD;
		ent->flags |= kind->FullGravity ? FTENT_GRAVITY : FTENT_SLOWGRAVITY;
		ent->callback = SparkShowerThink;
		ent->bounceFactor = SparkBounce;
		ent->entity.baseline.origin = velocity;
		ent->entity.baseline.scale = now + interval;
		ent->entity.baseline.framerate = life;
		ent->entity.baseline.fuser1 = interval;
		ent->entity.baseline.rendermode = gEngfuncs.pfnRandomLong(SparkStreakSpeedMin, SparkStreakSpeedMax);
		ent->entity.baseline.fuser2 = Random(SparkFlashMaxMin, SparkFlashMaxMax);
		ent->die = now + life + 1;
	}
}

// One of a falling helicopter's fireballs: the vanilla one or a round one, the same size and length.
void FallingFireball(const Vector& origin, int model, float scale)
{
	const int pick = gEngfuncs.pfnRandomLong(0, std::size(RoundFireballs));
	const Fireball& fireball = pick == 0 ? Vanilla : RoundFireballs[pick - 1];
	const int fireballModel = pick == 0 ? model : gEngfuncs.pEventAPI->EV_FindModelIndex(fireball.Sprite);

	if (fireballModel == 0)
		return;

	const float size = scale * Vanilla.Width * Random(FallingSizeMin, FallingSizeMax);
	const float duration = Vanilla.Frames / FallingFramerate * Random(1 - Jitter, 1 + Jitter);

	SpawnFireball(origin, fireballModel, size / (fireball.Width * fireball.Fill), fireball.Frames / duration, 180,
		Random(0, 360));
}
} // namespace

bool JhlExplosion_Fireballs(const Vector& origin, int model, float scale, float framerate, int flags)
{
	if (!JhlParticleSystemEnabled() || (flags & TE_EXPLFLAG_NOADDITIVE) != 0)
		return false;

	// Not the underwater fireball.
	if (model != gEngfuncs.pEventAPI->EV_FindModelIndex(Vanilla.Sprite))
		return false;

	if (framerate == FallingFramerate)
	{
		FallingFireball(origin, model, scale);
		return true;
	}

	if (framerate != JhlFramerate)
		return false;

	const bool rocket = (flags & TE_EXPLFLAG_JHL_ROCKET) != 0;
	const bool large = rocket || scale >= LargeScale;
	const float firstScale = scale * (rocket ? RocketScale : 1) * Random(1 - Jitter, 1 + Jitter);
	const float size = firstScale * Vanilla.Width;

	const Vector jitter{Random(-PositionJitter, PositionJitter), Random(-PositionJitter, PositionJitter),
		Random(-PositionJitter, PositionJitter)};
	SpawnFireball(origin + jitter, model, firstScale, JhlFramerate * Random(1 - Jitter, 1 + Jitter), 180, Random(0, 360));

	JhlExplosion_LingeringSmoke(origin, scale * (rocket ? RocketScale : 1));

	if ((flags & TE_EXPLFLAG_JHL_SPARKS) != 0)
		SparkShowers(origin, scale);

	// The second fireball: shaped for large explosions on a floor, round otherwise.
	Vector floor;
	const bool onFloor = FindFloor(origin, scale, floor);
	const bool shaped = large && onFloor;

	const Fireball* second;

	if (shaped)
	{
		const int count = scale >= BiggestScale ? std::size(ShapedFireballs) : std::size(ShapedFireballs) - 1;
		second = &ShapedFireballs[gEngfuncs.pfnRandomLong(0, count - 1)];
	}
	else
	{
		second = &RoundFireballs[gEngfuncs.pfnRandomLong(0, std::size(RoundFireballs) - 1)];
	}

	const int secondModel = gEngfuncs.pEventAPI->EV_FindModelIndex(second->Sprite);

	if (secondModel == 0)
		return true;

	const float secondSize = size * (shaped ? Random(ShapedSecondSizeMin, ShapedSecondSizeMax) : Random(RoundSecondSizeMin, RoundSecondSizeMax));
	const float secondScale = secondSize / (second->Width * second->Fill);
	const float duration = large ? Random(LargeDurationMin, LargeDurationMax) : Random(SmallDurationMin, SmallDurationMax);

	Vector secondOrigin = origin + Vector{Random(-1, 1) * SecondSideways * size, Random(-1, 1) * SecondSideways * size, 0};

	if (shaped)
	{
		// Stand it on the floor (R_Sprite_Explode raises sprites 10 units).
		const float height = second->Height * secondScale;
		secondOrigin.z = floor.z + height * (0.5f - ShapedSink) - 10;
	}
	else if (onFloor)
	{
		secondOrigin.z -= Random(SecondDownOnFloorMin, SecondDownOnFloorMax) * size;
	}
	else
	{
		secondOrigin.z += Random(0, SecondUp * size);
	}

	if (TEMPENTITY* timer = gEngfuncs.pEfxAPI->CL_TempEntAllocNoModel(secondOrigin); timer)
	{
		const float now = gEngfuncs.GetClientTime();

		timer->flags |= FTENT_NOMODEL | FTENT_CLIENTCUSTOM;
		timer->callback = DelayedFireball;
		timer->entity.curstate.fuser1 = now + Random(0, SecondDelay);
		timer->entity.curstate.iuser1 = secondModel;
		timer->entity.curstate.fuser2 = secondScale;
		timer->entity.curstate.fuser3 = second->Frames / duration;
		timer->entity.curstate.iuser2 = SecondBrightness;
		timer->entity.curstate.fuser4 = shaped ? 0 : Random(0, 360);
		timer->die = now + SecondDelay + 1;
	}

	return true;
}
