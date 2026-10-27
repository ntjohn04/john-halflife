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

#include <span>

#include "hud.h"
#include "client_gibs.h"
#include "com_model.h"
#include "dlight.h"
#include "event_api.h"
#include "jhl_debris.h"
#include "jhl_explosion.h"
#include "jhl_explosions.h"
#include "jhl_settings.h"
#include "parsemsg.h"
#include "pm_defs.h"
#include "pm_materials.h"
#include "pmtrace.h"
#include "sound/MaterialSystem.h"
#include "r_efx.h"
#include "view.h"

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
constexpr float RocketScale = JhlRocketExplosionScale;

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
// streaks at ±200 and a richo1 flash of scale 0.5-1). Every shower picks each of its values on its own, so no two
// fly alike.
// Showers: 0 to the most for the surface's material; off metal or an unknown surface the higher of two rolls
// (averaging about 5.5 and 2.8), off anything else one (about 1.5, as vanilla's 0-3).
constexpr int SparkShowersMin = 0;
constexpr int SparkShowersMaxMetal = 8;
constexpr int SparkShowersMaxOther = 3;
constexpr int SparkShowersMaxUnknown = 4; // Mid-air, or a brush entity (their materials aren't known on the client).
constexpr float SparkSpeedMin = 50; // Off the surface; skewed (SparkSpeedSkew) so most are moderate, some fast.
constexpr float SparkSpeedMax = 600;
constexpr float SparkSpeedSkew = 1.5f;
constexpr float SparkSpread = 1.2f; // How far from the surface's direction they may leave: up to nearly along it.
constexpr float SparkUpMin = 0;		// Added straight up (down off a ceiling).
constexpr float SparkUpMax = 300;
constexpr float SparkLifeMin = 0.4f;
constexpr float SparkLifeMax = 2.25f;
constexpr float SparkFullGravityChance = 0.5f; // Otherwise half, as vanilla.
constexpr float SparkIntervalMin = 0.07f;
constexpr float SparkIntervalMax = 0.11f;
// The client's temp entity physics halves this again for anything with gravity: 1 keeps half the speed per bounce,
// near vanilla's 0.6 in the air.
constexpr float SparkBounce = 1;

// Spark bursts (UTIL_Sparks: env_spark, buttons, beams, wires, computers... ; UTIL_JhlSparkShowers): the streaks and
// flash differ each time; showers, if any, live as long as the server asks (half an explosion's for most).
constexpr int SparkBurstStreaksMin = 4;
constexpr int SparkBurstStreaksMax = 10;
constexpr int SparkBurstSpeedMin = 120;
constexpr int SparkBurstSpeedMax = 220;
constexpr float SparkBurstFlashMin = 0.4f;
constexpr float SparkBurstFlashMax = 1;


// Armor ricochets (UTIL_Ricochet, scale 0.5-2): a few streaks with vanilla's flash.
constexpr int RicochetSparksMin = 4;
constexpr int RicochetSparksMax = 7;
constexpr int RicochetSparkSpeedMin = 100;
constexpr int RicochetSparkSpeedMax = 180;

// The specks and lights every explosion makes (instead of the engine's: 15 pure white specks, palette 254, ±32
// sideways and 80-143 up, 2 s; a 200 unit flash for 0.01 s and a 150 unit orange glow over 1 s; the same for every
// size). Embers: hot colors, more and wider for bigger explosions (scale over EmberScale, up to EmberMoreMax).
constexpr int EmberCountMin = 25; // A grenade's; up to EmberCountMore more for the biggest.
constexpr int EmberCountMax = 35;
constexpr int EmberCountMore = 15;
constexpr float EmberScale = 3;
constexpr float EmberMoreMax = 2.5f;
constexpr int EmberColors[] = {252, 252, 253, 253, 254}; // Pale yellow, cream, some white.
constexpr float EmberSidewaysMin = 20;
constexpr float EmberSidewaysMax = 80;
constexpr float EmberUpMin = 80;
constexpr float EmberUpMax = 220;
constexpr float EmberLifeMin = 1.5f;
constexpr float EmberLifeMax = 2.5f;
// Lights: the engine's radii times the explosion's size (scale over 3, 1 to LightMoreMax), ±LightVariation.
constexpr float LightMoreMax = 2;
constexpr float LightVariation = 0.05f;

// A third, filling the room: this much wider than the glow, at this much of its brightness (a dlight's at its middle
// grows with its radius too, so about half the glow's there, falling off gentler), a little redder (light gone further
// reads warmer), shrinking to nothing as its fireballs burn out (the glow's gone well before them): FillTime, or
// FillTimeLarge for a large explosion (its second fireball 0.9-1.3 s, a large one's 1.6-2 s).
constexpr float FillRadius = 1.7f;
constexpr float FillBrightness = 0.3f;
constexpr color24 FillColor{255, 150, 40};
constexpr float FillTime = 1.2f;
constexpr float FillTimeLarge = 1.9f;

// Under water (WXplo1): a second fireball like the first's, 80-100% its size, a little later.
constexpr const char* UnderwaterSprite = "sprites/WXplo1.spr";
constexpr int UnderwaterFrames = 15;

// Surface debris (SurfaceDebris): pieces of what an explosion hit, thrown off it, each picking its own values.
constexpr int SurfaceDebrisMin = 6; // For a grenade; bigger explosions more (by scale over SurfaceDebrisScale),
constexpr int SurfaceDebrisMax = 18;
constexpr float SurfaceDebrisScale = 3;
constexpr float SurfaceDebrisMoreMax = 2; // up to twice as many.
constexpr float SurfaceDebrisSpeedMin = 100;
constexpr float SurfaceDebrisSpeedMax = 500;
constexpr float SurfaceDebrisSpeedSkew = 1.3f; // Most moderate, some fast.
constexpr float SurfaceDebrisSpread = 0.9f;	   // As SparkSpread.
constexpr float SurfaceDebrisUpMin = 0;
constexpr float SurfaceDebrisUpMax = 200;
constexpr float SurfaceDebrisFullGravityChance = 0.75f; // Heavier than sparks; the rest half gravity.
constexpr float SurfaceDebrisLifeMin = 8;
constexpr float SurfaceDebrisLifeMax = 16;
constexpr float SurfaceDebrisSizeVariation = 0.15f; // Each piece ±15%.

// A heavy round's chunks off what it hit (JhlExplosion_ImpactChunks): as surface debris, smaller, slower, gone sooner.
constexpr float ImpactChunkSize = 0.5f;
constexpr float ImpactChunkSpeedMin = 80;
constexpr float ImpactChunkSpeedMax = 200;
constexpr float ImpactChunkUp = 60;
constexpr float ImpactChunkLifeMin = 4;
constexpr float ImpactChunkLifeMax = 8;

// How far (units) from the expected distance a surface can be and still count as the one an explosion hit.
constexpr float SurfaceMatchTolerance = 12;
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
// rendermode, streak speed; fuser2, biggest flash; iuser1, most streaks a burst, or 0 for SparkStreaksMin-Max; iuser2,
// flashes at full brightness).
void SparkShowerThink(TEMPENTITY* ent, float frametime, float currenttime)
{
	auto& state = ent->entity.baseline;

	if (currenttime < state.scale)
		return;

	Vector origin = ent->entity.origin;
	const int streaks = state.iuser1 > 0 ? gEngfuncs.pfnRandomLong(1, state.iuser1)
										 : gEngfuncs.pfnRandomLong(SparkStreaksMin, SparkStreaksMax);
	gEngfuncs.pEfxAPI->R_SparkStreaks(origin, streaks, -state.rendermode, state.rendermode);

	// The engine's own bursts use a copy of richo1 that maps don't precache; load it as a HUD sprite.
	if (const HSPRITE flash = gEngfuncs.pfnSPR_Load("sprites/richo1.spr"); flash > 0)
	{
		auto model = const_cast<model_t*>(gEngfuncs.GetSpritePointer(flash));
		const float scale = Random(state.fuser2 * 0.5f, state.fuser2);

		if (state.iuser2 == 0)
		{
			gEngfuncs.pEfxAPI->R_RicochetSprite(origin, model, 0.1f, scale);
		}
		// Showers asking for their own flash size get it at full brightness, white: as R_RicochetSprite makes its
		// flash (glow, the same size on screen at any distance, fading out over an eighth of a second, a random 45
		// degree turn), which it draws at 200.
		else if (TEMPENTITY* sprite = gEngfuncs.pEfxAPI->CL_TempEntAlloc(origin, model); sprite)
		{
			sprite->flags |= FTENT_FADEOUT;
			sprite->die = currenttime;
			sprite->fadeSpeed = 8;
			sprite->entity.curstate.rendermode = kRenderGlow;
			sprite->entity.curstate.renderfx = kRenderFxNoDissipation;
			sprite->entity.curstate.renderamt = sprite->entity.baseline.renderamt = 255;
			sprite->entity.curstate.rendercolor = {255, 255, 255};
			sprite->entity.curstate.scale = scale;
			sprite->entity.angles.z = gEngfuncs.pfnRandomLong(0, 7) * 45.f;
		}
	}

	state.scale = currenttime + state.fuser1;
	state.framerate -= state.fuser1;

	if (state.framerate <= 0)
		ent->die = currenttime;
}

// The normal of the surface an explosion hit, as far as the client can tell; zero in mid-air (as vanilla's trace that
// hits nothing). Explosions are placed (dmg - 24) * 0.6 units out of what they hit, which is scale * 10 + 15.6, so
// of the surfaces around (down, up and 8 ways around), it's the one at the distance closest to that. The nearest
// surface isn't always it: a wall hit low down has the floor closer.
Vector SurfaceNormal(const Vector& origin, float scale, pmtrace_t& hit)
{
	const float offset = scale * 10 + 15.6f;
	const float range = offset * 1.5f + 16; // Room for hitting a surface at an angle to the 8 directions.

	Vector directions[10] = {{0, 0, -1}, {0, 0, 1}};

	for (int i = 0; i < 8; ++i)
	{
		const float angle = i * 0.7853982f; // 45 degrees.
		directions[2 + i] = {std::cos(angle), std::sin(angle), 0};
	}

	Vector normal = g_vecZero;
	float bestMismatch = SurfaceMatchTolerance;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	for (const Vector& direction : directions)
	{
		pmtrace_t tr;
		gEngfuncs.pEventAPI->EV_PlayerTrace(origin, origin + direction * range, PM_WORLD_ONLY, -1, &tr);

		if (tr.fraction >= 1)
			continue;

		// The distance straight out from the surface, whichever way the trace met it.
		const float distance = std::fabs(DotProduct(tr.endpos - origin, tr.plane.normal));

		if (const float mismatch = std::fabs(distance - offset); mismatch < bestMismatch)
		{
			bestMismatch = mismatch;
			normal = tr.plane.normal;
			hit = tr;
		}
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();

	return normal;
}

// The material of the surface a trace hit (pm_materials.h), or 0 if it isn't known: only the world's are on the
// client, as for bullet impacts (ev_hldm.cpp).
char SurfaceMaterial(const pmtrace_t& hit)
{
	if (gEngfuncs.pEventAPI->EV_IndexFromTrace(const_cast<pmtrace_t*>(&hit)) != 0)
		return 0;

	Vector start = hit.endpos + hit.plane.normal * 4;
	Vector end = hit.endpos - hit.plane.normal * 4;

	if (const char* textureName = gEngfuncs.pEventAPI->EV_TraceTexture(hit.ent, start, end); textureName)
		return g_MaterialSystem.FindTextureType(g_MaterialSystem.StripTexturePrefix(textureName));

	return 0;
}

// Metal, as bullet impacts count it: metal, vents, grates, computers.
bool IsMetal(char material)
{
	switch (material)
	{
	case CHAR_TEX_METAL:
	case CHAR_TEX_VENT:
	case CHAR_TEX_GRATE:
	case CHAR_TEX_COMPUTER:
		return true;
	default:
		return false;
	}
}

// The most spark showers an explosion throws: more off metal.
int SparkShowersMax(char material)
{
	if (material == 0)
		return SparkShowersMaxUnknown;

	return IsMetal(material) ? SparkShowersMaxMetal : SparkShowersMaxOther;
}

// Spark showers as vanilla's spark_shower throws them, each with its own values.
// The surface an explosion hit, for its sparks and debris.
struct Surface
{
	bool Water = false; // The explosion is under water: no sparks or debris.
	Vector Normal;		// Zero in mid-air.
	pmtrace_t Hit;		// Where it was found (Normal isn't zero).
	char Material = 0;	// pm_materials.h, or 0 if unknown (mid-air, brush entities).
};

Surface FindSurface(const Vector& origin, float scale)
{
	Surface surface;

	if (Vector point = origin; gEngfuncs.PM_PointContents(point, nullptr) == CONTENTS_WATER)
	{
		surface.Water = true;
		return surface;
	}

	surface.Normal = SurfaceNormal(origin, scale, surface.Hit);

	if (surface.Normal != g_vecZero)
		surface.Material = SurfaceMaterial(surface.Hit);

	return surface;
}

// @param showers How many, or -1 for an explosion's (by the surface's material).
// @param lifeScale Times each shower's life.
// @param flashMax, streaksMax Each shower's biggest flash and most streaks a burst, or 0 for an explosion's.
void SparkShowers(const Vector& origin, const Surface& surface, int showers = -1, float lifeScale = 1, float flashMax = 0,
	int streaksMax = 0)
{
	if (surface.Water)
		return;

	Vector normal = surface.Normal;

	const int most = SparkShowersMax(surface.Material);
	int count = showers;

	if (count < 0)
	{
		count = gEngfuncs.pfnRandomLong(SparkShowersMin, most);

		if (surface.Material == 0 || IsMetal(surface.Material))
			count = std::max(count, static_cast<int>(gEngfuncs.pfnRandomLong(SparkShowersMin, most)));
	}

	if (count == 0)
		return;

	// In mid-air, vanilla's sparks go up (its trace's normal is zero, and they get the upward push).
	const bool ceiling = normal.z < 0;

	if (normal == g_vecZero)
		normal = {0, 0, 1};

	const float now = gEngfuncs.GetClientTime();

	for (int i = 0; i < count; ++i)
	{
		TEMPENTITY* ent = gEngfuncs.pEfxAPI->CL_TempEntAllocNoModel(origin);

		if (!ent)
			return;

		Vector scatter{Random(-1, 1), Random(-1, 1), Random(-1, 1)};

		// Never back into the surface.
		if (DotProduct(scatter, normal) < 0)
			scatter = scatter - normal * (2 * DotProduct(scatter, normal));

		const Vector direction = (normal + scatter * SparkSpread).Normalize();
		const float speed = SparkSpeedMin + (SparkSpeedMax - SparkSpeedMin) * std::pow(Random(0, 1), SparkSpeedSkew);
		Vector velocity = direction * speed;
		velocity.z += (ceiling ? -1 : 1) * Random(SparkUpMin, SparkUpMax);

		const float life = Random(SparkLifeMin, SparkLifeMax) * lifeScale;
		const float interval = Random(SparkIntervalMin, SparkIntervalMax);

		ent->flags |= FTENT_NOMODEL | FTENT_CLIENTCUSTOM | FTENT_COLLIDEWORLD;
		ent->flags |= Random(0, 1) < SparkFullGravityChance ? FTENT_GRAVITY : FTENT_SLOWGRAVITY;
		ent->callback = SparkShowerThink;
		ent->bounceFactor = SparkBounce;
		ent->entity.baseline.origin = velocity;
		ent->entity.baseline.scale = now + interval;
		ent->entity.baseline.framerate = life;
		ent->entity.baseline.fuser1 = interval;
		ent->entity.baseline.rendermode = gEngfuncs.pfnRandomLong(SparkStreakSpeedMin, SparkStreakSpeedMax);
		ent->entity.baseline.fuser2 = flashMax > 0 ? Random(flashMax * 0.5f, flashMax) : Random(SparkFlashMaxMin, SparkFlashMaxMax);
		ent->entity.baseline.iuser1 = streaksMax;
		ent->entity.baseline.iuser2 = flashMax > 0 ? 1 : 0; // Its own flash size: drawn at full brightness, white.
		ent->die = now + life + 1;
	}
}

// A vanilla gib model for surface debris, and the scale that makes its typical piece about 10 units (they were made for
// breakables of every size, from 4 unit pebbles to 33 unit planks).
struct DebrisModel
{
	const char* Model;
	float Scale;
};

constexpr DebrisModel ConcreteDebris[] = {{"models/concretegibs.mdl", 1.1f}};
constexpr DebrisModel DirtDebris[] = {{"models/rockgibs.mdl", 1.6f}};
constexpr DebrisModel MetalDebris[] = {{"models/metalplategibs.mdl", 0.45f}, {"models/metalgibs.mdl", 0.7f}, {"models/chromegibs.mdl", 0.8f}};
constexpr DebrisModel VentDebris[] = {{"models/ventgibs.mdl", 0.55f}};
constexpr DebrisModel GrateDebris[] = {{"models/metalgibs.mdl", 0.7f}, {"models/catwalkgibs.mdl", 0.35f}};
constexpr DebrisModel TileDebris[] = {{"models/ceilinggibs.mdl", 0.85f}, {"models/concretegibs.mdl", 1.1f}};
constexpr DebrisModel WoodDebris[] = {{"models/woodgibs.mdl", 0.35f}};
constexpr DebrisModel ComputerDebris[] = {{"models/computergibs.mdl", 0.45f}, {"models/metalgibs.mdl", 0.7f}};
constexpr DebrisModel GlassDebris[] = {{"models/glassgibs.mdl", 0.5f}};
constexpr DebrisModel FleshDebris[] = {{"models/fleshgibs.mdl", 0.65f}};

// The debris a surface material throws, and how its pieces sound bouncing (BREAK_*); none off water and snow.
std::span<const DebrisModel> SurfaceDebrisModels(char material, int& breakType)
{
	switch (material)
	{
	case CHAR_TEX_METAL:
		breakType = BREAK_METAL;
		return MetalDebris;
	case CHAR_TEX_VENT:
		breakType = BREAK_METAL;
		return VentDebris;
	case CHAR_TEX_GRATE:
		breakType = BREAK_METAL;
		return GrateDebris;
	case CHAR_TEX_COMPUTER:
		breakType = BREAK_METAL;
		return ComputerDebris;
	case CHAR_TEX_WOOD:
	case CHAR_TEX_SWAMP_BOARDS:
		breakType = BREAK_WOOD;
		return WoodDebris;
	case CHAR_TEX_GLASS:
		breakType = BREAK_GLASS;
		return GlassDebris;
	case CHAR_TEX_FLESH:
	case CHAR_TEX_ORGANIC:
		breakType = BREAK_FLESH;
		return FleshDebris;
	case CHAR_TEX_DIRT:
	case CHAR_TEX_XEN_GROUND:
	case CHAR_TEX_XEN_SHELL:
		breakType = BREAK_CONCRETE;
		return DirtDebris;
	case CHAR_TEX_TILE:
		breakType = BREAK_CONCRETE;
		return TileDebris;
	case CHAR_TEX_SLOSH:
	case CHAR_TEX_SNOW:
		return {};
	default: // Concrete, and unknown (brush entities; unlisted textures are concrete too).
		breakType = BREAK_CONCRETE;
		return ConcreteDebris;
	}
}

// Pieces of the surface an explosion hit, thrown off it; each its own model, size, speed, direction, gravity and life.
void SurfaceDebris(const Surface& surface, float scale)
{
	if (surface.Water || surface.Normal == g_vecZero)
		return;

	int breakType = BREAK_CONCRETE;
	const auto models = SurfaceDebrisModels(surface.Material, breakType);

	if (models.empty())
		return;

	const Vector& normal = surface.Normal;
	const bool ceiling = normal.z < 0;
	const float bigger = std::clamp(scale / SurfaceDebrisScale, 1.f, SurfaceDebrisMoreMax);
	const int count = static_cast<int>(gEngfuncs.pfnRandomLong(SurfaceDebrisMin, SurfaceDebrisMax) * bigger);

	for (int i = 0; i < count; ++i)
	{
		const DebrisModel& debris = models[gEngfuncs.pfnRandomLong(0, models.size() - 1)];
		model_t* model = JhlDebris_LoadModel(debris.Model);

		if (!model)
			continue;

		Vector scatter{Random(-1, 1), Random(-1, 1), Random(-1, 1)};

		// Never back into the surface.
		if (DotProduct(scatter, normal) < 0)
			scatter = scatter - normal * (2 * DotProduct(scatter, normal));

		const Vector direction = (normal + scatter * SurfaceDebrisSpread).Normalize();
		const float speed = SurfaceDebrisSpeedMin + (SurfaceDebrisSpeedMax - SurfaceDebrisSpeedMin) * std::pow(Random(0, 1), SurfaceDebrisSpeedSkew);
		Vector velocity = direction * speed;
		velocity.z += (ceiling ? -1 : 1) * Random(SurfaceDebrisUpMin, SurfaceDebrisUpMax);

		const Vector origin = surface.Hit.endpos + normal * 4 + Vector{Random(-4, 4), Random(-4, 4), Random(-4, 4)};
		TEMPENTITY* piece = JhlDebris_Piece(origin, model, breakType, velocity, Random(SurfaceDebrisLifeMin, SurfaceDebrisLifeMax));

		if (!piece)
			return;

		if (Random(0, 1) < SurfaceDebrisFullGravityChance)
		{
			piece->flags &= ~FTENT_SLOWGRAVITY;
			piece->flags |= FTENT_GRAVITY;
		}

		// The model renderer only scales entities marked so (StudioSetUpTransform).
		piece->entity.curstate.scale = debris.Scale * Random(1 - SurfaceDebrisSizeVariation, 1 + SurfaceDebrisSizeVariation);
		piece->entity.curstate.iuser4 = ScaledModelMarker;
	}
}

void ImpactChunks(const Vector& position, const Vector& normal, char material, int count)
{
	int breakType = BREAK_CONCRETE;
	const auto models = SurfaceDebrisModels(material, breakType);

	if (models.empty() || normal == g_vecZero)
		return;

	for (int i = 0; i < count; ++i)
	{
		const DebrisModel& debris = models[gEngfuncs.pfnRandomLong(0, models.size() - 1)];
		model_t* model = JhlDebris_LoadModel(debris.Model);

		if (!model)
			continue;

		Vector scatter{Random(-1, 1), Random(-1, 1), Random(-1, 1)};

		if (DotProduct(scatter, normal) < 0)
			scatter = scatter - normal * (2 * DotProduct(scatter, normal));

		const Vector direction = (normal + scatter * SurfaceDebrisSpread).Normalize();
		Vector velocity = direction * Random(ImpactChunkSpeedMin, ImpactChunkSpeedMax);
		velocity.z += Random(0, ImpactChunkUp);

		TEMPENTITY* piece = JhlDebris_Piece(position + normal * 3, model, breakType, velocity, Random(ImpactChunkLifeMin, ImpactChunkLifeMax));

		if (!piece)
			return;

		piece->entity.curstate.scale = debris.Scale * ImpactChunkSize * Random(1 - SurfaceDebrisSizeVariation, 1 + SurfaceDebrisSizeVariation);
		piece->entity.curstate.iuser4 = ScaledModelMarker;
	}
}

// An explosion's specks and lights, by its size (see EmberCountMin).
void EmbersAndLights(const Vector& origin, float scale, int flags, bool embers)
{
	const float more = std::clamp(scale / EmberScale, 1.f, EmberMoreMax);
	const float now = gEngfuncs.GetClientTime();

	if (embers && (flags & TE_EXPLFLAG_NOPARTICLES) == 0)
	{
		// A grenade 25-35, an RPG rocket 30-40, a satchel 35-45, a mortar 40-50.
		const int added = static_cast<int>(EmberCountMore * (more - 1) / (EmberMoreMax - 1));
		const int count = gEngfuncs.pfnRandomLong(EmberCountMin + added, EmberCountMax + added);

		for (int i = 0; i < count; ++i)
		{
			particle_t* ember = gEngfuncs.pEfxAPI->R_AllocParticle(nullptr);

			if (!ember)
				break;

			const float sideways = Random(EmberSidewaysMin, EmberSidewaysMax) * std::sqrt(more);
			const float angle = Random(0, 6.2831853f);

			ember->org = origin;
			ember->vel = {std::cos(angle) * sideways, std::sin(angle) * sideways, Random(EmberUpMin, EmberUpMax)};
			ember->color = static_cast<short>(EmberColors[gEngfuncs.pfnRandomLong(0, std::size(EmberColors) - 1)]);
			gEngfuncs.pEfxAPI->R_GetPackedColor(&ember->packedColor, ember->color);
			ember->type = pt_blob2; // As the engine's: drifting out and falling.
			ember->die = now + Random(EmberLifeMin, EmberLifeMax);
		}
	}

	if ((flags & TE_EXPLFLAG_NODLIGHTS) == 0)
	{
		const float lights = std::clamp(scale / 3, 1.f, LightMoreMax);

		if (dlight_t* flash = gEngfuncs.pEfxAPI->CL_AllocDlight(0); flash)
		{
			flash->origin = origin;
			flash->radius = 200 * lights * Random(1 - LightVariation, 1 + LightVariation);
			flash->color = {250, 250, 150};
			flash->die = now + 0.01f;
			flash->decay = 800;
		}

		// It fades by 200 units a second, so it lasts until it has (vanilla's 150 lasts 0.75 s).
		if (dlight_t* glow = gEngfuncs.pEfxAPI->CL_AllocDlight(0); glow)
		{
			glow->origin = origin;
			glow->radius = 150 * lights * Random(1 - LightVariation, 1 + LightVariation);
			glow->color = {255, 190, 40};
			glow->decay = 200;
			glow->die = now + std::max(1.f, glow->radius / glow->decay);

			if (dlight_t* fill = gEngfuncs.pEfxAPI->CL_AllocDlight(0); fill)
			{
				fill->origin = origin;
				fill->radius = glow->radius * FillRadius;
				fill->color = {static_cast<byte>(FillColor.r * FillBrightness), static_cast<byte>(FillColor.g * FillBrightness),
					static_cast<byte>(FillColor.b * FillBrightness)};
				const bool large = scale >= LargeScale || (flags & (TE_EXPLFLAG_JHL_ROCKET | TE_EXPLFLAG_JHL_BREAKABLE)) != 0;
				const float life = large ? FillTimeLarge : FillTime;
				fill->decay = fill->radius / life;
				fill->die = now + life;
			}
		}
	}
}

// An underwater explosion's fireballs: the vanilla one, with a little randomness, and a second like it.
void UnderwaterFireballs(const Vector& origin, int model, float scale)
{
	SpawnFireball(origin, model, scale * Random(1 - Jitter, 1 + Jitter), JhlFramerate * Random(1 - Jitter, 1 + Jitter), 180,
		Random(0, 360));

	if (TEMPENTITY* timer = gEngfuncs.pEfxAPI->CL_TempEntAllocNoModel(origin); timer)
	{
		const float now = gEngfuncs.GetClientTime();
		const float size = scale * Random(0.8f, 1.f);

		timer->flags |= FTENT_NOMODEL | FTENT_CLIENTCUSTOM;
		timer->callback = DelayedFireball;
		timer->entity.origin = origin + Vector{Random(-1, 1), Random(-1, 1), Random(0, 1)} * (size * 64 * SecondSideways);
		timer->entity.curstate.fuser1 = now + Random(0, SecondDelay);
		timer->entity.curstate.iuser1 = model;
		timer->entity.curstate.fuser2 = size;
		timer->entity.curstate.fuser3 = UnderwaterFrames / Random(SmallDurationMin, SmallDurationMax);
		timer->entity.curstate.iuser2 = SecondBrightness;
		timer->entity.curstate.fuser4 = Random(0, 360);
		timer->die = now + SecondDelay + 1;
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

void JhlExplosion_ParseSparkShowers(BufferReader& reader)
{
	Vector origin = reader.ReadCoordVector();
	const int count = reader.ReadByte();
	const float lifeScale = reader.ReadByte() / 10.f;
	Vector direction;

	for (int i = 0; i < 3; ++i)
		direction[i] = reader.ReadChar() / 100.f;

	const float flashMax = reader.ReadByte() / 10.f;
	const int streaksMax = reader.ReadByte();

	// The engine's own burst, as vanilla.
	if (!JhlParticleSystemEnabled())
	{
		gEngfuncs.pEfxAPI->R_SparkShower(origin);
		return;
	}

	// A burst like the engine's (8 streaks at ±200, a richo1 flash of 0.5-1), different each time.
	const int speed = gEngfuncs.pfnRandomLong(SparkBurstSpeedMin, SparkBurstSpeedMax);
	gEngfuncs.pEfxAPI->R_SparkStreaks(origin, gEngfuncs.pfnRandomLong(SparkBurstStreaksMin, SparkBurstStreaksMax), -speed, speed);

	// The engine's own bursts use a copy of richo1 that maps don't precache; load it as a HUD sprite.
	if (const HSPRITE flash = gEngfuncs.pfnSPR_Load("sprites/richo1.spr"); flash > 0)
	{
		gEngfuncs.pEfxAPI->R_RicochetSprite(origin, const_cast<model_t*>(gEngfuncs.GetSpritePointer(flash)), 0.1f,
			Random(SparkBurstFlashMin, SparkBurstFlashMax));
	}

	Surface surface;
	surface.Normal = direction.Length() > 0.1f ? direction.Normalize() : Vector{0, 0, 1};

	if (gEngfuncs.PM_PointContents(origin, nullptr) == CONTENTS_WATER)
		surface.Water = true;

	// Lives as the server asks: shorter than an explosion's for sparking machinery or a computer shorting out.
	SparkShowers(origin, surface, count, lifeScale, flashMax, streaksMax);
}

void JhlExplosion_RicochetSparks(const Vector& position, float scale)
{
	if (!JhlParticleSystemEnabled())
		return;

	// A little toward the viewer, so the armor they hit doesn't hide them.
	Vector origin = position + (v_origin - position).Normalize() * 2;
	const int speed = gEngfuncs.pfnRandomLong(RicochetSparkSpeedMin, RicochetSparkSpeedMax);
	const int streaks = std::clamp(static_cast<int>(RicochetSparksMin + scale * 2), RicochetSparksMin, RicochetSparksMax);
	gEngfuncs.pEfxAPI->R_SparkStreaks(origin, streaks, -speed, speed);
}

bool JhlExplosion_Fireballs(const Vector& origin, int model, float scale, float framerate, int flags)
{
	if (!JhlParticleSystemEnabled() || (flags & TE_EXPLFLAG_NOADDITIVE) != 0)
		return false;

	// Drawing the fireballs, this draws the specks and lights too (R_Explosion leaves them to it).
	const int vanillaModel = gEngfuncs.pEventAPI->EV_FindModelIndex(Vanilla.Sprite);

	if (model == vanillaModel && framerate == FallingFramerate)
	{
		FallingFireball(origin, model, scale);
		EmbersAndLights(origin, scale, flags, true);
		return true;
	}

	if (framerate != JhlFramerate)
		return false;

	if (model == gEngfuncs.pEventAPI->EV_FindModelIndex(UnderwaterSprite))
	{
		UnderwaterFireballs(origin, model, scale);
		EmbersAndLights(origin, scale, flags, false);
		return true;
	}

	if (model != vanillaModel)
		return false;

	// RPG rockets and explosive breakables are drawn bigger than their damage says, and as large explosions.
	const bool rocket = (flags & TE_EXPLFLAG_JHL_ROCKET) != 0;
	const bool breakable = (flags & TE_EXPLFLAG_JHL_BREAKABLE) != 0;
	const float bigger = rocket ? RocketScale : breakable ? JhlBreakableExplosionScale : 1;
	const bool large = rocket || breakable || scale >= LargeScale;
	const float firstScale = scale * bigger * Random(1 - Jitter, 1 + Jitter);
	const float size = firstScale * Vanilla.Width;

	const Vector jitter{Random(-PositionJitter, PositionJitter), Random(-PositionJitter, PositionJitter),
		Random(-PositionJitter, PositionJitter)};
	SpawnFireball(origin + jitter, model, firstScale, JhlFramerate * Random(1 - Jitter, 1 + Jitter), 180, Random(0, 360));

	JhlExplosion_LingeringSmoke(origin, scale * bigger);

	const Surface surface = FindSurface(origin, scale);

	if ((flags & TE_EXPLFLAG_JHL_SPARKS) != 0)
		SparkShowers(origin, surface);

	SurfaceDebris(surface, scale);
	EmbersAndLights(origin, scale * bigger, flags, true);

	// The engine's blob explosion, a sixteenth of its particles, a quarter for large ones (more ran a few satchels out).
	if ((flags & TE_EXPLFLAG_NOPARTICLES) == 0)
		JhlExplosion_Blob(origin, large ? 256 : 64);

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

void JhlExplosion_Blob(const Vector& origin, int count)
{
	// The engine's R_BlobExplosion (Quake's), with any count: half pt_blob (colors 66-71, swelling and slowing),
	// half pt_blob2 (150-155, spreading flat), living 1 or 1.4 s.
	const float now = gEngfuncs.GetClientTime();

	for (int i = 0; i < count; ++i)
	{
		particle_t* particle = gEngfuncs.pEfxAPI->R_AllocParticle(nullptr);

		if (!particle)
			break;

		particle->die = now + 1 + (gEngfuncs.pfnRandomLong(0, 1) != 0 ? 0.4f : 0);
		particle->type = (i & 1) != 0 ? pt_blob : pt_blob2;
		particle->color = static_cast<short>(((i & 1) != 0 ? 66 : 150) + gEngfuncs.pfnRandomLong(0, 5));
		gEngfuncs.pEfxAPI->R_GetPackedColor(&particle->packedColor, particle->color);

		for (int j = 0; j < 3; ++j)
		{
			particle->org[j] = origin[j] + gEngfuncs.pfnRandomLong(-16, 15);
			particle->vel[j] = static_cast<float>(gEngfuncs.pfnRandomLong(-256, 255));
		}
	}
}

namespace
{
// A sprite playing its frames once over its life and fading out over the end of it (JhlSprite_PlayOnce):
// fuser1 the start, fuser2 the life, fuser3 how much of the life it fades over, baseline.renderamt its alpha.
void PlayOnceThink(TEMPENTITY* ent, float frametime, float currenttime)
{
	auto& state = ent->entity.curstate;
	const float life = std::max(0.01f, state.fuser2);
	const float t = std::clamp((currenttime - state.fuser1) / life, 0.f, 1.f);

	state.frame = std::min(ent->frameMax, t * (ent->frameMax + 1));

	const float fade = std::clamp(state.fuser3, 0.f, 1.f);
	const float left = fade > 0 ? std::clamp((1 - t) / fade, 0.f, 1.f) : 1;
	state.renderamt = static_cast<int>(ent->entity.baseline.renderamt * left);
}
} // namespace

void JhlSprite_PlayOnce(TEMPENTITY* sprite, float life, float fade)
{
	sprite->flags &= ~(FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP | FTENT_SPRCYCLE | FTENT_FADEOUT);
	sprite->flags |= FTENT_CLIENTCUSTOM;
	sprite->callback = PlayOnceThink;
	sprite->entity.curstate.fuser1 = gEngfuncs.GetClientTime();
	sprite->entity.curstate.fuser2 = life;
	sprite->entity.curstate.fuser3 = fade;
	sprite->entity.curstate.frame = 0;
	sprite->die = gEngfuncs.GetClientTime() + life;
}

void JhlExplosion_ParseGroundDust(BufferReader& reader)
{
	const Vector origin = reader.ReadCoordVector();
	const int count = reader.ReadByte();

	if (!JhlParticleSystemEnabled())
		return;

	const int model = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/steam1.spr");

	if (model == 0)
		return;

	// Low puffs kicked out along the floor: steam1 at 0.5, alpha 0.25, each playing once over 1.2 s and fading
	// over the last 60% of it.
	for (int i = 0; i < count; ++i)
	{
		Vector position = origin + Vector{Random(-48, 48), Random(-48, 48), 4};
		Vector velocity{Random(-120, 120), Random(-120, 120), 10 + Random(-20, 20)};

		TEMPENTITY* puff = gEngfuncs.pEfxAPI->R_TempSprite(position, velocity, 0.5f, model, kRenderTransAlpha,
			kRenderFxNone, 0.25f, 1.2f, 0);

		if (!puff)
			continue;

		puff->entity.angles.z = Random(0, 359);
		JhlSprite_PlayOnce(puff, 1.2f, 0.6f);
	}
}

void JhlExplosion_ParseDisplacerSpecks(BufferReader& reader)
{
	const Vector origin = reader.ReadCoordVector();

	if (!JhlParticleSystemEnabled())
		return;

	const int model = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/exit1.spr");

	if (model == 0)
		return;

	// 4 tiny specks of the ball's own sprite (exit1 at 0.04, additive), scattered ±40 with a pop of 40 up, falling at
	// half gravity onto what's below, each playing its frames once over 1.5 s and fading over the last 60%.
	for (int i = 0; i < 4; ++i)
	{
		Vector position = origin + Vector{Random(-2, 2), Random(-2, 2), Random(-2, 2)};
		Vector velocity{Random(-40, 40), Random(-40, 40), 40 + Random(-40, 40)};

		TEMPENTITY* speck = gEngfuncs.pEfxAPI->R_TempSprite(position, velocity, 0.04f, model, kRenderTransAdd,
			kRenderFxNone, 1, 1.5f, FTENT_SLOWGRAVITY | FTENT_COLLIDEWORLD);

		if (!speck)
			continue;

		speck->entity.angles.z = Random(0, 359);
		JhlSprite_PlayOnce(speck, 1.5f, 0.6f);
	}
}

void JhlExplosion_ImpactChunks(const Vector& position, const Vector& normal, char material, int count)
{
	ImpactChunks(position, normal, material, count);
}
