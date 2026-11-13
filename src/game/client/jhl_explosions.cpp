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
#include "jhl_models.h"
#include "client_gibs.h"
#include "com_model.h"
#include "dlight.h"
#include "event_api.h"
#include "jhl_debris.h"
#include "jhl_explosion.h"
#include "jhl_explosions.h"
#include "jhl_settings.h"
#include "jhl_tracers.h"
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
	float Width;
	float Height;
	int Frames;
	float Fill = 1;
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

constexpr Fireball ShapedFireballs[] = {
	{"sprites/fexplo.spr", 80, 96, 30},
	{"sprites/gexplo.spr", 80, 96, 23},
	{"sprites/hexplo.spr", 64, 96, 30},
};

constexpr float JhlFramerate = 15;

constexpr float FallingFramerate = 12;
constexpr float FallingSizeMin = 1;
constexpr float FallingSizeMax = 1.2f;

constexpr float LargeScale = 6;
constexpr float BiggestScale = 9;
constexpr float RocketScale = JhlRocketExplosionScale;

constexpr float Jitter = 0.1f;
constexpr float PositionJitter = 4; // Units, the vanilla fireball.

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

constexpr float SmallDurationMin = 0.9f;
constexpr float SmallDurationMax = 1.3f;
constexpr float LargeDurationMin = 1.6f;
constexpr float LargeDurationMax = 2.0f;

constexpr float ShapedSink = 0.05f;

constexpr const char* LingeringSmokeSprite = "sprites/wep_smoke_01.spr";
constexpr float LingeringSmokeSpriteWidth = 64;
constexpr float LingeringSmokeHeight = 20;
constexpr float LingeringSmokeFadeIn = 0.05f; // Of its life.
constexpr float LingeringSmokeDriftDecay = 0.3f;

constexpr int SparkShowersMin = 0;
constexpr int SparkShowersMaxMetal = 8;
constexpr int SparkShowersMaxOther = 3;
constexpr int SparkShowersMaxUnknown = 4;
constexpr float SparkSpeedMin = 50;
constexpr float SparkSpeedMax = 600;
constexpr float SparkSpeedSkew = 1.5f;
constexpr float SparkSpread = 1.2f;
constexpr float SparkUpMin = 0;
constexpr float SparkUpMax = 300;
constexpr float SparkLifeMin = 0.4f;
constexpr float SparkLifeMax = 2.25f;
constexpr float SparkFullGravityChance = 0.5f; // Otherwise half, as vanilla.
constexpr float SparkIntervalMin = 0.07f;
constexpr float SparkIntervalMax = 0.11f;
constexpr float SparkBounce = 1;

constexpr int SparkBurstStreaksMin = 4;
constexpr int SparkBurstStreaksMax = 10;
constexpr int SparkBurstSpeedMin = 120;
constexpr int SparkBurstSpeedMax = 220;
constexpr float SparkBurstFlashMin = 0.4f;
constexpr float SparkBurstFlashMax = 1;


constexpr int RicochetSparksMin = 4;
constexpr int RicochetSparksMax = 7;
constexpr int RicochetSparkSpeedMin = 100;
constexpr int RicochetSparkSpeedMax = 180;

constexpr int EmberCountMin = 21;
constexpr int EmberCountMax = 30;
constexpr int EmberCountMore = 13;
constexpr float EmberScale = 3;
constexpr float EmberMoreMax = 2.5f;
constexpr int EmberColors[] = {252, 252, 253, 253, 254};
constexpr float EmberSidewaysMin = 20;
constexpr float EmberSidewaysMax = 80;
constexpr float EmberUpMin = 80;
constexpr float EmberUpMax = 220;
constexpr float EmberLifeMin = 1.5f;
constexpr float EmberLifeMax = 2.5f;
constexpr float LightMoreMax = 2;
constexpr float LightVariation = 0.05f;

constexpr float FillRadius = 1.7f;
constexpr float FillBrightness = 0.3f;
constexpr color24 FillColor{255, 150, 40};
constexpr float FillTime = 1.2f;
constexpr float FillTimeLarge = 1.9f;

constexpr const char* UnderwaterSprite = "sprites/WXplo1.spr";
constexpr int UnderwaterFrames = 15;

constexpr int SurfaceDebrisMin = 6;
constexpr int SurfaceDebrisMax = 18;
constexpr float SurfaceDebrisScale = 3;
constexpr float SurfaceDebrisMoreMax = 2; // up to twice as many.
constexpr float SurfaceDebrisSpeedMin = 100;
constexpr float SurfaceDebrisSpeedMax = 500;
constexpr float SurfaceDebrisSpeedSkew = 1.3f; // Most moderate, some fast.
constexpr float SurfaceDebrisSpread = 0.9f;	   // As SparkSpread.
constexpr float SurfaceDebrisUpMin = 0;
constexpr float SurfaceDebrisUpMax = 200;
constexpr float SurfaceDebrisFullGravityChance = 0.75f;
constexpr float SurfaceDebrisLifeMin = 8;
constexpr float SurfaceDebrisLifeMax = 16;
constexpr float SurfaceDebrisSizeVariation = 0.15f; // Each piece ±15%.

constexpr float ImpactChunkSize = 0.5f;
constexpr float ImpactChunkSpeedMin = 80;
constexpr float ImpactChunkSpeedMax = 200;
constexpr float ImpactChunkUp = 60;
constexpr float ImpactChunkLifeMin = 4;
constexpr float ImpactChunkLifeMax = 8;

constexpr float SurfaceMatchTolerance = 12;
constexpr int SparkStreaksMin = 6; // Per burst.
constexpr int SparkStreaksMax = 12;
constexpr int SparkStreakSpeedMin = 150;
constexpr int SparkStreakSpeedMax = 300;
constexpr float SparkFlashMaxMin = 0.4f;
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

void DelayedFireball(TEMPENTITY* ent, float frametime, float currenttime)
{
	if (currenttime < ent->entity.curstate.fuser1)
		return;

	const auto& state = ent->entity.curstate;
	SpawnFireball(ent->entity.origin, state.iuser1, state.fuser2, state.fuser3, state.iuser2, state.fuser4);
	ent->die = currenttime;
}

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
	const int model = JhlEffectModel(LingeringSmokeSprite);

	if (model == 0)
		return false;

	const float life = Random(settings.LifeMin, settings.LifeMax);
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

void SparkShowerThink(TEMPENTITY* ent, float frametime, float currenttime)
{
	auto& state = ent->entity.baseline;

	if (currenttime < state.scale)
		return;

	Vector origin = ent->entity.origin;
	const int streaks = state.iuser1 > 0 ? gEngfuncs.pfnRandomLong(1, state.iuser1)
										 : gEngfuncs.pfnRandomLong(SparkStreaksMin, SparkStreaksMax);
	gEngfuncs.pEfxAPI->R_SparkStreaks(origin, streaks, -state.rendermode, state.rendermode);

	if (const HSPRITE flash = gEngfuncs.pfnSPR_Load("sprites/richo1.spr"); flash > 0)
	{
		auto model = const_cast<model_t*>(gEngfuncs.GetSpritePointer(flash));
		const float scale = Random(state.fuser2 * 0.5f, state.fuser2);

		if (state.iuser2 == 0)
		{
			gEngfuncs.pEfxAPI->R_RicochetSprite(origin, model, 0.1f, scale);
		}
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

Vector SurfaceNormal(const Vector& origin, float scale, pmtrace_t& hit)
{
	const float offset = scale * 10 + 15.6f;
	const float range = offset * 1.5f + 16;

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

char SurfaceMaterial(const pmtrace_t& hit)
{
	if (gEngfuncs.pEventAPI->EV_IndexFromTrace(const_cast<pmtrace_t*>(&hit)) != 0)
		return 0;

	Vector start = hit.endpos + hit.plane.normal * 4;
	Vector end = hit.endpos - hit.plane.normal * 4;

	if (const char* textureName = gEngfuncs.pEventAPI->EV_TraceTexture(hit.ent, start, end); textureName)
		return g_MaterialSystem.FindImpactType(g_MaterialSystem.StripTexturePrefix(textureName));

	return 0;
}

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

struct Surface
{
	bool Water = false;
	Vector Normal;		// Zero in mid-air.
	pmtrace_t Hit;
	char Material = 0;
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
		ent->entity.baseline.iuser2 = flashMax > 0 ? 1 : 0;
		ent->die = now + life + 1;
	}
}

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
		breakType = BREAK_WOOD;
		return WoodDebris;
	case CHAR_TEX_GLASS:
		breakType = BREAK_GLASS;
		return GlassDebris;
	case CHAR_TEX_FLESH:
		breakType = BREAK_FLESH;
		return FleshDebris;
	case CHAR_TEX_DIRT:
		breakType = BREAK_CONCRETE;
		return DirtDebris;
	case CHAR_TEX_TILE:
		breakType = BREAK_CONCRETE;
		return TileDebris;
	case CHAR_TEX_SLOSH:
	case CHAR_TEX_SNOW:
		return {};
	default:
		breakType = BREAK_CONCRETE;
		return ConcreteDebris;
	}
}

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

constexpr int ParticlesSoft = 500;
constexpr int ParticlesHard = 1100;
constexpr float ParticlesLeastShare = 0.2f;

struct ParticleBatch
{
	float Gone = 0;
	int Count = 0;
};

ParticleBatch g_ParticleBatches[128];
int g_NextParticleBatch = 0;

// How many of @p count an explosion throws now.
int ParticleShare(int count)
{
	const float now = gEngfuncs.GetClientTime();
	int out = 0;

	for (ParticleBatch& batch : g_ParticleBatches)
	{
		if (batch.Gone > now + 5)
			batch = {};

		if (batch.Gone > now)
			out += batch.Count;
	}

	const float over = std::clamp(static_cast<float>(out - ParticlesSoft) / (ParticlesHard - ParticlesSoft), 0.f, 1.f);
	return static_cast<int>(std::round(count * (1 - over * (1 - ParticlesLeastShare))));
}

// @p count particles thrown, going by @p gone.
void ParticlesThrown(int count, float gone)
{
	g_ParticleBatches[g_NextParticleBatch] = {gone, count};
	g_NextParticleBatch = (g_NextParticleBatch + 1) % std::size(g_ParticleBatches);
}

void EmbersAndLights(const Vector& origin, float scale, int flags, bool embers)
{
	const float more = std::clamp(scale / EmberScale, 1.f, EmberMoreMax);
	const float now = gEngfuncs.GetClientTime();

	if (embers && (flags & TE_EXPLFLAG_NOPARTICLES) == 0)
	{
		const int added = static_cast<int>(EmberCountMore * (more - 1) / (EmberMoreMax - 1));
		const int count = ParticleShare(gEngfuncs.pfnRandomLong(EmberCountMin + added, EmberCountMax + added));
		ParticlesThrown(count, now + EmberLifeMax);

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
			ember->type = pt_blob2;
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

void FallingFireball(const Vector& origin, int model, float scale)
{
	const int pick = gEngfuncs.pfnRandomLong(0, std::size(RoundFireballs));
	const Fireball& fireball = pick == 0 ? Vanilla : RoundFireballs[pick - 1];
	const int fireballModel = pick == 0 ? model : JhlEffectModel(fireball.Sprite);

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

	const int speed = gEngfuncs.pfnRandomLong(SparkBurstSpeedMin, SparkBurstSpeedMax);
	gEngfuncs.pEfxAPI->R_SparkStreaks(origin, gEngfuncs.pfnRandomLong(SparkBurstStreaksMin, SparkBurstStreaksMax), -speed, speed);

	if (const HSPRITE flash = gEngfuncs.pfnSPR_Load("sprites/richo1.spr"); flash > 0)
	{
		gEngfuncs.pEfxAPI->R_RicochetSprite(origin, const_cast<model_t*>(gEngfuncs.GetSpritePointer(flash)), 0.1f,
			Random(SparkBurstFlashMin, SparkBurstFlashMax));
	}

	Surface surface;
	surface.Normal = direction.Length() > 0.1f ? direction.Normalize() : Vector{0, 0, 1};

	if (gEngfuncs.PM_PointContents(origin, nullptr) == CONTENTS_WATER)
		surface.Water = true;

	SparkShowers(origin, surface, count, lifeScale, flashMax, streaksMax);
}

void JhlExplosion_RicochetSparks(const Vector& position, float scale)
{
	if (!JhlParticleSystemEnabled())
		return;

	Vector origin = position + (v_origin - position).Normalize() * 2;
	const int speed = gEngfuncs.pfnRandomLong(RicochetSparkSpeedMin, RicochetSparkSpeedMax);
	const int streaks = std::clamp(static_cast<int>(RicochetSparksMin + scale * 2), RicochetSparksMin, RicochetSparksMax);
	gEngfuncs.pEfxAPI->R_SparkStreaks(origin, streaks, -speed, speed);
}

bool JhlExplosion_Fireballs(const Vector& origin, int model, float scale, float framerate, int flags)
{
	if (!JhlParticleSystemEnabled() || (flags & TE_EXPLFLAG_NOADDITIVE) != 0)
		return false;

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

	if ((flags & TE_EXPLFLAG_NOPARTICLES) == 0)
		JhlExplosion_Blob(origin, large ? 256 : 64);

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

	const int secondModel = JhlEffectModel(second->Sprite);

	if (secondModel == 0)
		return true;

	const float secondSize = size * (shaped ? Random(ShapedSecondSizeMin, ShapedSecondSizeMax) : Random(RoundSecondSizeMin, RoundSecondSizeMax));
	const float secondScale = secondSize / (second->Width * second->Fill);
	const float duration = large ? Random(LargeDurationMin, LargeDurationMax) : Random(SmallDurationMin, SmallDurationMax);

	Vector secondOrigin = origin + Vector{Random(-1, 1) * SecondSideways * size, Random(-1, 1) * SecondSideways * size, 0};

	if (shaped)
	{
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
	const float now = gEngfuncs.GetClientTime();

	count = ParticleShare(count);
	ParticlesThrown(count, now + 1.4f);

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

namespace
{
color24 DustColor(char material)
{
	switch (material)
	{
	case CHAR_TEX_DIRT: return {120, 105, 85};
	case CHAR_TEX_WOOD: return {130, 115, 95};
	case CHAR_TEX_CONCRETE:
	case CHAR_TEX_TILE: return {150, 150, 145};
	default: return {140, 135, 130};
	}
}

struct Billows
{
	int Count = 0;
	float ScaleMin = 0.6f; // × the burst's size.
	float ScaleMax = 0.8f;
	float RiseMin = 50;
	float RiseMax = 110;
	float LifeScale = 1.3f; // × the low puffs' life.
};

void DustBurst(const Vector& origin, int count, float size, color24 dust, const Billows& billows = {})
{
	const int model = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/steam1.spr");

	if (model == 0)
		return;

	const float life = 1.2f * std::sqrt(size);

	for (int i = 0; i < count; ++i)
	{
		Vector position = origin + Vector{Random(-48, 48), Random(-48, 48), 4} * size;
		position.z = origin.z + 4;
		Vector velocity{Random(-120, 120) * size, Random(-120, 120) * size, 10 + Random(-20, 20)};

		TEMPENTITY* puff = gEngfuncs.pEfxAPI->R_TempSprite(position, velocity, 0.5f * size, model, kRenderTransAlpha,
			kRenderFxNone, 0.25f, life, 0);

		if (!puff)
			continue;

		puff->entity.curstate.rendercolor = dust;
		puff->entity.angles.z = Random(0, 359);
		JhlSprite_PlayOnce(puff, life, 0.6f);
	}

	for (int i = 0; i < billows.Count; ++i)
	{
		Vector position = origin + Vector{Random(-24, 24) * size, Random(-24, 24) * size, 16};
		Vector velocity{Random(-30, 30), Random(-30, 30), Random(billows.RiseMin, billows.RiseMax)};
		const float billowLife = life * billows.LifeScale;

		TEMPENTITY* puff = gEngfuncs.pEfxAPI->R_TempSprite(position, velocity, Random(billows.ScaleMin, billows.ScaleMax) * size,
			model, kRenderTransAlpha, kRenderFxNone, 0.22f, billowLife, 0);

		if (!puff)
			continue;

		puff->entity.curstate.rendercolor = dust;
		puff->entity.angles.z = Random(0, 359);
		JhlSprite_PlayOnce(puff, billowLife, 0.6f);
	}
}

bool FloorUnder(const Vector& origin, float below, pmtrace_t& tr, char& material)
{
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(origin + Vector{0, 0, 8}, origin - Vector{0, 0, below}, PM_WORLD_ONLY, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.fraction >= 1)
		return false;

	material = SurfaceMaterial(tr);
	return true;
}
} // namespace

void JhlExplosion_ParseGroundDust(BufferReader& reader)
{
	const Vector origin = reader.ReadCoordVector();
	const int count = reader.ReadByte();
	const float size = std::max(reader.ReadByte(), 1) / 10.f;

	if (!JhlParticleSystemEnabled())
		return;

	if (size <= 1)
	{
		DustBurst(origin, count, size, {255, 255, 255});
		return;
	}

	pmtrace_t tr;
	char material = 0;
	FloorUnder(origin, 32, tr, material);
	DustBurst(origin, count, size, DustColor(material), {.Count = count / 3});
}

void JhlExplosion_ParseMortarIncoming(BufferReader& reader)
{
	const Vector impact = reader.ReadCoordVector();

	if (!JhlParticleSystemEnabled())
		return;

	static const JhlTracerStyle Incoming{
		.Color = {255, 200, 140},
		.Brightness = 0.55f,
		.Width = 2.5f,
		.Length = 260,
		.Speed = 2900,
		.StartOffsetMax = 0,
		.CoreColor = {255, 240, 220},
		.CoreBrightness = 0.8f,
		.CoreWidth = 1,
		.CoreLength = 140,
	};

	JhlTracer_Fire(impact + Vector{0, 0, 1024}, impact, Incoming);
}

void JhlExplosion_TentacleImpact(const Vector& position, int strength)
{
	if (!JhlParticleSystemEnabled())
		return;

	pmtrace_t tr;
	char material = 0;

	if (!FloorUnder(position, 96, tr, material))
		return;

	const Vector floor = tr.endpos;
	const Vector normal = tr.plane.normal;
	const bool slam = strength > 0;

	if (IsMetal(material))
	{
		if (!slam)
		{
			if (gEngfuncs.pfnRandomLong(0, 1) == 0)
				gEngfuncs.pEfxAPI->R_SparkStreaks(floor + normal * 2, gEngfuncs.pfnRandomLong(3, 5), -120, 120);
			return;
		}

		gEngfuncs.pEfxAPI->R_SparkStreaks(floor + normal * 2, gEngfuncs.pfnRandomLong(8, 12), -200, 200);

		Surface surface;
		surface.Normal = normal;
		surface.Hit = tr;
		surface.Material = material;
		SparkShowers(floor + normal * 4, surface, gEngfuncs.pfnRandomLong(0, 3) == 0 ? 2 : 1);

		DustBurst(floor, 4, 0.8f, DustColor(material));
	}
	else
	{
		if (!slam)
		{
			DustBurst(floor, 4, 0.6f, DustColor(material));
			return;
		}

		DustBurst(floor, 10, 1.3f, DustColor(material),
			{.Count = 3, .ScaleMin = 0.75f, .ScaleMax = 0.8f, .RiseMin = 40, .RiseMax = 80, .LifeScale = 1.6f});
		ImpactChunks(floor, normal, material, gEngfuncs.pfnRandomLong(1, 2));
	}

	if (strength >= 2)
		DustBurst(floor, 12, 1.6f, DustColor(material));
}

constexpr int BiteBubbleCount = 26;
constexpr float BiteBubbleSpeedMin = 50;
constexpr float BiteBubbleSpeedMax = 130;
constexpr float BiteBubbleDrag = 3;
constexpr float BiteBubbleBuoyancy = 2.5f;
constexpr float BiteBubbleLife = 1.6f;
constexpr float BiteBubbleFade = 0.4f;

static bool InWater(Vector position)
{
	const int contents = gEngfuncs.PM_PointContents(position, nullptr);
	return contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA;
}

static void BiteBubbleThink(TEMPENTITY* bubble, float frametime, float currenttime)
{
	auto& state = bubble->entity.curstate;

	if (!InWater(bubble->entity.origin))
	{
		bubble->die = currenttime;
		return;
	}

	Vector& velocity = bubble->entity.baseline.origin;
	const float drag = std::exp(-BiteBubbleDrag * frametime);
	velocity.x *= drag;
	velocity.y *= drag;
	velocity.z += (state.fuser3 - velocity.z) * (1 - std::exp(-BiteBubbleBuoyancy * frametime));

	const float p = std::clamp((currenttime - state.fuser1) / state.fuser2, 0.f, 1.f);
	state.renderamt = static_cast<int>(255 * std::clamp((1 - p) / BiteBubbleFade, 0.f, 1.f));
}

void JhlExplosion_ParseBiteBubbles(BufferReader& reader)
{
	const Vector origin = reader.ReadCoordVector();
	Vector forward = reader.ReadCoordVector();

	if (!JhlParticleSystemEnabled())
		return;

	const int model = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/bubble.spr");

	if (model == 0 || !InWater(origin))
		return;

	forward = forward.Normalize();
	const float now = gEngfuncs.GetClientTime();

	for (int i = 0; i < BiteBubbleCount; ++i)
	{
		const Vector scatter{gEngfuncs.pfnRandomFloat(-1, 1), gEngfuncs.pfnRandomFloat(-1, 1), gEngfuncs.pfnRandomFloat(-1, 1)};
		const Vector direction = (forward * 0.7f + scatter.Normalize()).Normalize();
		Vector velocity = direction * gEngfuncs.pfnRandomFloat(BiteBubbleSpeedMin, BiteBubbleSpeedMax);
		Vector position = origin + Vector{gEngfuncs.pfnRandomFloat(-4, 4), gEngfuncs.pfnRandomFloat(-4, 4), gEngfuncs.pfnRandomFloat(-4, 4)};
		const float life = BiteBubbleLife * gEngfuncs.pfnRandomFloat(0.75f, 1.25f);

		TEMPENTITY* bubble = gEngfuncs.pEfxAPI->R_TempSprite(position, velocity, 1 / gEngfuncs.pfnRandomFloat(4, 16), model,
			kRenderTransAlpha, kRenderFxNone, 1, life, FTENT_COLLIDEWORLD | FTENT_COLLIDEKILL);

		if (!bubble)
			break;

		bubble->entity.curstate.frame = gEngfuncs.pfnRandomLong(0, std::max(0, static_cast<int>(bubble->frameMax) - 1));
		bubble->entity.curstate.fuser1 = now;
		bubble->entity.curstate.fuser2 = life;
		bubble->entity.curstate.fuser3 = gEngfuncs.pfnRandomFloat(80, 140);
		bubble->flags |= FTENT_CLIENTCUSTOM;
		bubble->callback = BiteBubbleThink;
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

void JhlExplosion_GroundKick(const Vector& point, const Vector& direction)
{
	if (!JhlParticleSystemEnabled())
		return;

	pmtrace_t tr;
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(point + Vector{0, 0, 16}, point - Vector{0, 0, 128}, PM_WORLD_ONLY, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	// Only off a floor.
	if (tr.fraction >= 1 || tr.plane.normal.z < 0.5f)
		return;

	const char material = SurfaceMaterial(tr);

	color24 dust{140, 135, 130};

	switch (material)
	{
	case CHAR_TEX_DIRT: dust = {120, 105, 85}; break;
	case CHAR_TEX_WOOD: dust = {130, 115, 95}; break;
	case CHAR_TEX_CONCRETE:
	case CHAR_TEX_TILE: dust = {150, 150, 145}; break;
	default: break;
	}

	const int model = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/steam1.spr");

	Vector side = CrossProduct(direction, Vector{0, 0, 1});
	side = side.Length() > 0.1f ? side.Normalize() : Vector{1, 0, 0};

	for (int i = 0; model != 0 && i < 2; ++i)
	{
		const float away = (i == 0 ? 1 : -1) * Random(50, 110);
		Vector position = tr.endpos + Vector{0, 0, 6} + side * Random(-10, 10);
		Vector velocity = side * away + direction * Random(10, 40) + Vector{0, 0, Random(30, 70)};

		TEMPENTITY* puff = gEngfuncs.pEfxAPI->R_TempSprite(position, velocity, Random(0.5f, 0.75f), model, kRenderTransAlpha,
			kRenderFxNone, 0.3f, 1.6f, 0);

		if (!puff)
			continue;

		puff->entity.curstate.rendercolor = dust;
		puff->entity.angles.z = Random(0, 359);
		JhlSprite_PlayOnce(puff, Random(1.2f, 1.6f), 0.6f);
	}

	// Now and then a chunk of it, or off metal, sparks.
	if (IsMetal(material))
	{
		if (Random(0, 1) < 0.3f)
			gEngfuncs.pEfxAPI->R_SparkStreaks(tr.endpos + Vector{0, 0, 2}, 6, -200, 200);
	}
	else if (Random(0, 1) < 0.35f)
	{
		ImpactChunks(tr.endpos, tr.plane.normal, material, 1);
	}
}
