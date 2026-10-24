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

/**
 *	@brief Whether johnhalflife's particle system changes are on for effects the server sends (hit blood).
 *	It's the client's jhl_fx_particlesystem, which the server can read in single player since the engine's cvars are
 *	shared; a server without it (dedicated) always uses them.
 */
inline bool UTIL_JhlParticleSystem()
{
	const cvar_t* cvar = g_engfuncs.pfnCVarGetPointer("jhl_fx_particlesystem");
	return !cvar || cvar->value != 0;
}

// johnhalflife explosion extras (util.cpp), by the explosion's fireball scale (a grenade 3), for callers to use with
// jhl_fx_particlesystem on (vanilla's otherwise).

/**
 *	@brief The scorch where an explosion hit: one of {scorch1-3} (vanilla 1-2), and for big explosions (scale 6+, 9+)
 *	one or two more, overlapping.
 */
void UTIL_JhlExplosionScorch(TraceResult* trace, float scale);

/**
 *	@brief Shakes the screens of players near an explosion, more and further for bigger ones (vanilla: none).
 */
void UTIL_JhlExplosionShake(const Vector& origin, float scale);

/**
 *	@brief An underwater explosion's bubbles: vanilla's, more and over a bigger area for bigger explosions, and some
 *	lingering after them, rising slowly.
 */
void UTIL_JhlExplosionBubbles(const Vector& origin, float scale);

class CBeam;

/**
 *	@brief johnhalflife's beam pattern (the vortigaunt's): a thin, wild, dimmer strand layered on a beam, with the
 *	same ends, type, flags and color, its own @p width, @p noise (±25% each time) and @p brightness. Call it again
 *	with the strand it returned to follow a beam that moved (effects.cpp). Temporary, as monsters' beams are.
 *	@return The strand (@p strand, or a new one), or null if it couldn't be made.
 */
CBeam* UTIL_JhlBeamStrand(CBeam* strand, CBeam* beam, int width, int noise, int brightness);

/**
 *	@brief A dynamic light (TE_DLIGHT): @p radius in units, @p life in seconds, fading @p decay units a second.
 */
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

/**
 *	@brief Spark showers, as explosions throw them (the client's, jhl_explosions.cpp), going up from a point: TE_SPARKS
 *	through the SDK's own temp entity message (position, then the number of showers).
 */
inline void UTIL_JhlSparkShowers(const Vector& position, int count, float lifeScale = 0.5f)
{
	MESSAGE_BEGIN(MSG_PVS, gmsgTempEntity, position);
	WRITE_BYTE(TE_SPARKS);
	WRITE_COORD_VECTOR(position);
	WRITE_BYTE(count);
	WRITE_BYTE(std::clamp(static_cast<int>(lifeScale * 10), 1, 255)); // The showers' life, tenths of an explosion's.
	MESSAGE_END();
}

/**
 *	@brief A burst of ember specks (as explosions throw, jhl_explosions.cpp) from a point, spraying out at up to
 *	@p speed units a second and drifting down for 2-4 s: TE_PARTICLEBURST through the SDK's own temp entity message
 *	(position, count, speed / 4).
 */
inline void UTIL_JhlEmberBurst(const Vector& position, int count, float speed)
{
	MESSAGE_BEGIN(MSG_PVS, gmsgTempEntity, position);
	WRITE_BYTE(TE_PARTICLEBURST);
	WRITE_COORD_VECTOR(position);
	WRITE_BYTE(std::clamp(count, 0, 255));
	WRITE_BYTE(std::clamp(static_cast<int>(speed / 4), 0, 255));
	MESSAGE_END();
}

/**
 *	@brief TE_BREAKMODEL (breakable shards, helicopter debris). With jhl_fx_particlesystem on, it goes through the SDK's
 *	own temp entity message and the client makes the shards (jhl_debris.cpp): lasting 60 seconds like gibs,
 *	@p multiplier times the count, counted against its debris budget. Otherwise the engine's, as vanilla.
 *	@param count 0: by the size. @param life Tenths of a second (vanilla only).
 */
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

/**
 *	@brief The scale (× 10) an explosion's smoke puff is sent with: 7% bigger than vanilla's (the client adds a big
 *	lingering cloud behind it, jhl_explosions.cpp).
 */
inline int UTIL_JhlExplosionSmokeScale(float vanillaScale)
{
	if (!UTIL_JhlParticleSystem())
		return static_cast<int>(vanillaScale);

	return std::min(255, static_cast<int>(vanillaScale * 1.07f));
}

/**
 *	@brief A mushroom cloud (hexplo.spr, precached in W_Precache) rising from the ground under a crashing helicopter,
 *	alongside its vanilla fexplo fireball. Sent as TE_SPRITE like that fireball, so it plays once at the engine's
 *	default speed (30 frames, about as long as fexplo's).
 */
inline void UTIL_JhlCrashMushroom(CBaseEntity* helicopter, const Vector& wreck, float scale)
{
	if (!UTIL_JhlParticleSystem())
		return;

	TraceResult tr;
	UTIL_TraceLine(wreck, wreck - Vector(0, 0, 1024), ignore_monsters, helicopter->edict(), &tr);

	// The sprite is 96 px tall and centered; its base goes on the ground (slightly sunk, the bottom rows are empty).
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

// Blood colors that don't pick up the palette's bright reds and yellows (as the client's gib splash).
constexpr int JhlBloodRed = 74;
constexpr int JhlBloodYellow = 198;

// Blood streams pick colors from the given one to 9 above it: red 68-77, yellow 198-207. Their particles are flat and
// drawn at full brightness, so they start 2 darker than vanilla's red streams (70) to match the textured puffs.
constexpr int JhlBloodStreamRed = 68;
constexpr int JhlBloodStreamYellow = 198;
constexpr int VanillaBloodStreamRed = 70;

/**
 *	@brief The color to give a blood stream for a blood color: red 247 would run into the palette's bright reds, so
 *	streams use JhlBloodStreamRed and JhlBloodStreamYellow. Others, and everything with the particle system
 *	off, are unchanged.
 */
inline int UTIL_JhlBloodStreamColor(int color)
{
	if (!UTIL_JhlParticleSystem())
		return color;

	// Vanilla passes 70 for red streams (env_blood, the sniper rifle), others the blood color.
	if (color == BLOOD_COLOR_RED || color == VanillaBloodStreamRed)
		return JhlBloodStreamRed;

	if (color == BLOOD_COLOR_YELLOW)
		return JhlBloodStreamYellow;

	return color;
}
