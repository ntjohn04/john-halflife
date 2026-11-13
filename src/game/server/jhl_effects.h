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

#pragma once

#include "UserMessages.h"
#include "jhl_framerate.h"

struct JhlCvarCache
{
	const cvar_t* Cvar = nullptr;
	bool Looked = false;
};

inline const cvar_t* UTIL_JhlClientCvar(const char* name, JhlCvarCache& cache)
{
	if (!cache.Looked)
	{
		cache.Cvar = g_engfuncs.pfnCVarGetPointer(name);
		cache.Looked = true;
	}

	return cache.Cvar;
}

inline bool UTIL_JhlFpsFix()
{
	static JhlCvarCache cache;
	const cvar_t* cvar = UTIL_JhlClientCvar("jhl_physics_fpsfix", cache);
	return !cvar || cvar->value != 0;
}

inline bool UTIL_JhlWallFix()
{
	static JhlCvarCache cache;
	const cvar_t* cvar = UTIL_JhlClientCvar("jhl_physics_wallfix", cache);
	return !cvar || cvar->value != 0;
}

inline bool UTIL_JhlBulletImpacts()
{
	static JhlCvarCache cache;
	const cvar_t* cvar = UTIL_JhlClientCvar("jhl_fx_bulletimpacts", cache);
	return !cvar || cvar->value != 0;
}

inline bool UTIL_JhlKeepCorpses()
{
	static JhlCvarCache cache;
	const cvar_t* cvar = UTIL_JhlClientCvar("jhl_fx_keepcorpses", cache);
	return !cvar || cvar->value != 0;
}

inline bool UTIL_JhlJumpLandJolt()
{
	static JhlCvarCache cache;
	const cvar_t* cvar = UTIL_JhlClientCvar("jhl_camera_jumplandjolt", cache);
	return !cvar || cvar->value != 0;
}

inline float UTIL_JhlVanillaFrametime()
{
	return UTIL_JhlFpsFix() ? 1.0f / JhlReferenceFrameRate : gpGlobals->frametime;
}

inline bool UTIL_JhlParticleSystem()
{
	static JhlCvarCache cache;
	const cvar_t* cvar = UTIL_JhlClientCvar("jhl_fx_particlesystem", cache);
	return !cvar || cvar->value != 0;
}

Vector UTIL_JhlAimAtCrosshair(CBasePlayer* player, const Vector& source, const Vector& angles);

void UTIL_JhlExplosionScorch(TraceResult* trace, float scale);

void UTIL_JhlExplosionShake(const Vector& origin, float scale);

void UTIL_JhlExplosionBubbles(const Vector& origin, float scale);

class CBeam;

CBeam* UTIL_JhlBeamStrand(CBeam* strand, CBeam* beam, int width, int noise, int brightness);

inline void UTIL_JhlRingStrand(const Vector& center, const Vector& axis, int sprite, float life, int width,
	const Vector& color, int brightness)
{
	if (!UTIL_JhlParticleSystem())
		return;

	MESSAGE_BEGIN(MSG_PAS, gmsgTempEntity, center);
	WRITE_BYTE(TE_BEAMTORUS);
	WRITE_COORD_VECTOR(center);
	WRITE_COORD_VECTOR(axis);
	WRITE_SHORT(sprite);
	WRITE_SHORT(static_cast<int>(life * 1.2f * 100));								 // life, hundredths
	WRITE_BYTE(std::clamp(static_cast<int>(width * 1.25f), 1, 255));				 // width, tenths
	WRITE_BYTE(std::clamp(static_cast<int>(50 * RANDOM_FLOAT(0.75f, 1.25f)), 0, 255)); // noise, hundredths
	WRITE_BYTE(static_cast<int>(color.x));
	WRITE_BYTE(static_cast<int>(color.y));
	WRITE_BYTE(static_cast<int>(color.z));
	WRITE_BYTE(brightness / 2);
	MESSAGE_END();
}

inline void UTIL_JhlGroundDust(const Vector& position, int count, int size = 10)
{
	if (!UTIL_JhlParticleSystem())
		return;

	MESSAGE_BEGIN(MSG_PVS, gmsgTempEntity, position);
	WRITE_BYTE(TE_SMOKE);
	WRITE_COORD_VECTOR(position);
	WRITE_BYTE(count);
	WRITE_BYTE(size);
	MESSAGE_END();
}

inline void UTIL_JhlDisplacerSpecks(const Vector& position)
{
	if (!UTIL_JhlParticleSystem())
		return;

	MESSAGE_BEGIN(MSG_PVS, gmsgTempEntity, position);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD_VECTOR(position);
	MESSAGE_END();
}

inline bool UTIL_JhlTracers()
{
	static JhlCvarCache cache;
	const cvar_t* cvar = UTIL_JhlClientCvar("jhl_fx_tracers", cache);
	return UTIL_JhlParticleSystem() && (!cvar || cvar->value != 0);
}

constexpr int JhlTracerMP5 = 0;
constexpr int JhlTracerGlock = 1;
constexpr int JhlTracerHeavy = 2;
constexpr int JhlTracerSniper = 3;
constexpr int JhlTracerShotgun = 4;
constexpr int JhlTracerM249 = 5;

inline void UTIL_JhlTracer(const Vector& start, const Vector& end, int kind, const Vector& muzzle)
{
	MESSAGE_BEGIN(MSG_PAS, gmsgTempEntity, start);
	WRITE_BYTE(TE_TRACER);
	WRITE_COORD_VECTOR(start);
	WRITE_COORD_VECTOR(end);
	WRITE_BYTE(kind);
	WRITE_COORD_VECTOR(muzzle);
	MESSAGE_END();
}

inline void UTIL_JhlDynamicLight(const Vector& position, float radius, int r, int g, int b, float life, float decay)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, position);
	WRITE_BYTE(TE_DLIGHT);
	WRITE_COORD_VECTOR(position);
	WRITE_BYTE(static_cast<int>(radius / 10)); // radius * 0.1
	WRITE_BYTE(r);
	WRITE_BYTE(g);
	WRITE_BYTE(b);
	WRITE_BYTE(static_cast<int>(life * 10)); // time * 10
	WRITE_BYTE(static_cast<int>(decay / 10)); // decay * 0.1
	MESSAGE_END();
}

inline void UTIL_JhlSparkShowers(const Vector& position, int count, float lifeScale = 0.5f, const Vector& direction = Vector(0, 0, 1),
	float flashMax = 0, int streaksMax = 0)
{
	MESSAGE_BEGIN(MSG_PVS, gmsgTempEntity, position);
	WRITE_BYTE(TE_SPARKS);
	WRITE_COORD_VECTOR(position);
	WRITE_BYTE(count);
	WRITE_BYTE(std::clamp(static_cast<int>(lifeScale * 10), 1, 255));

	for (int i = 0; i < 3; ++i)
		WRITE_CHAR(std::clamp(static_cast<int>(direction[i] * 100), -100, 100));

	WRITE_BYTE(std::clamp(static_cast<int>(flashMax * 10), 0, 255));
	WRITE_BYTE(std::clamp(streaksMax, 0, 255));

	MESSAGE_END();
}

inline void UTIL_JhlBreakModel(const Vector& position, const Vector& size, const Vector& velocity, int random,
	int model, int count, int life, int flags, int multiplier = 1)
{
	const bool jhl = UTIL_JhlParticleSystem();

	MESSAGE_BEGIN(MSG_PVS, jhl ? gmsgTempEntity : SVC_TEMPENTITY, position);
	WRITE_BYTE(TE_BREAKMODEL);
	WRITE_COORD_VECTOR(position);
	WRITE_COORD_VECTOR(size);
	WRITE_COORD_VECTOR(velocity);
	WRITE_BYTE(random); // × 10 units/s
	WRITE_SHORT(model);
	WRITE_BYTE(count);
	WRITE_BYTE(life);
	WRITE_BYTE(flags);

	if (jhl)
		WRITE_BYTE(multiplier);

	MESSAGE_END();
}

inline int UTIL_JhlExplosionSmokeScale(float vanillaScale)
{
	if (!UTIL_JhlParticleSystem())
		return static_cast<int>(vanillaScale);

	return std::min(255, static_cast<int>(vanillaScale * 1.07f));
}

inline void UTIL_JhlCrashMushroom(CBaseEntity* helicopter, const Vector& wreck, float scale)
{
	if (!UTIL_JhlParticleSystem())
		return;

	TraceResult tr;
	UTIL_TraceLine(wreck, wreck - Vector(0, 0, 1024), ignore_monsters, helicopter->edict(), &tr);

	Vector origin = tr.vecEndPos;
	origin.z += 96 * scale * 0.45f;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, origin);
	WRITE_BYTE(TE_SPRITE);
	WRITE_COORD_VECTOR(origin);
	WRITE_SHORT(g_engfuncs.pfnModelIndex("sprites/hexplo.spr"));
	WRITE_BYTE(static_cast<int>(scale * 10)); // scale * 10
	WRITE_BYTE(200);						   // brightness
	MESSAGE_END();
}

constexpr int JhlBloodRed = 74;
constexpr int JhlBloodYellow = 198;

constexpr int JhlBloodStreamRed = 68;
constexpr int JhlBloodStreamYellow = 198;
constexpr int VanillaBloodStreamRed = 70;

inline int UTIL_JhlBloodStreamColor(int color)
{
	if (!UTIL_JhlParticleSystem())
		return color;

	if (color == BLOOD_COLOR_RED || color == VanillaBloodStreamRed)
		return JhlBloodStreamRed;

	if (color == BLOOD_COLOR_YELLOW)
		return JhlBloodStreamYellow;

	return color;
}
