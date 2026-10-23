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
