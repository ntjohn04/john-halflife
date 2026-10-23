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

// Blood streams pick colors from the given one to 9 above it: red 70-79, yellow 196-205.
constexpr int JhlBloodStreamRed = 70;
constexpr int JhlBloodStreamYellow = 196;
