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
 *	streams use 70 (as vanilla's own red streams do) and yellow 196. Others, and everything with the particle system
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
