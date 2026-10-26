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
 *	@brief How a johnhalflife tracer looks: a streak @p Length units long flying @p Speed units a second, a beam
 *	(laserbeam.spr) @p Width wide in @p Color (0-255) at @p Brightness (0-1), starting a random 0 to
 *	@p StartOffsetMax units down its path (some come right out of the barrel, some further out).
 */
struct JhlTracerStyle
{
	Vector Color{255, 60, 30};
	float Brightness = 1;
	float Width = 1.2f;
	float Length = 200; // Seen nearly end-on, it looks a few feet long.
	float Speed = 4500;
	float StartOffsetMax = 120;

	// A hotter core drawn inside the streak, centered on it and CoreLength / Length of it (shrinking with it as it grows
	// out and goes in), or none if CoreLength is 0. Both are additive, so the core reads as a white-hot center with
	// the streak's color around it.
	Vector CoreColor{255, 225, 180};
	float CoreBrightness = 1;
	float CoreWidth = 1;
	float CoreLength = 0;

	// Whether it throws a burning speck where it hits: skipping off up and to the side, slowly, twirling, falling
	// gently and fading (in its colors, Color around CoreColor), as a tracer round's burning compound does.
	bool ImpactSpeck = false;

	// The least way down its path it starts (StartOffsetMax the most).
	float StartOffsetMin = 0;
};

/**
 *	@brief Each gun's tracer (JhlTracers_Style). The numbers are sent for NPCs' tracers (UTIL_JhlTracer, server
 *	jhl_effects.h), so they must match there.
 */
enum class JhlTracerKind
{
	MP5 = 0, // The MP5's, and NPCs' MP5s: faint pale orange feedback.
	Glock,	 // 9mm pistols.
	Heavy,	 // The revolver, Desert Eagle, 12mm (turrets, the Apache).
	Sniper,
	Shotgun,
	M249, // Real tracer rounds: red with a white-hot core, a burning speck where they hit.
};

const JhlTracerStyle& JhlTracers_Style(JhlTracerKind kind);

/**
 *	@brief Whether johnhalflife's tracers are drawn: jhl_fx_particlesystem and jhl_fx_tracers.
 */
bool JhlTracersEnabled();

class BufferReader;

/**
 *	@brief An NPC's tracer (UTIL_JhlTracer): the SDK's own temp entity message's TE_TRACER, start, end, kind and muzzle. The
 *	vanilla tracer, and johnhalflife's on top (the M249's instead of it).
 */
void JhlTracers_ParseNpcTracer(BufferReader& reader);

/**
 *	@brief Where a player's tracer drawn from outside first person starts: the player model's muzzle (its first
 *	attachment, set while it's drawn), if it's set and near @p start, where the game starts its tracers (by the eyes);
 *	otherwise @p start. NPCs' come from the server (UTIL_JhlTracer).
 */
Vector JhlTracers_MuzzleStart(int shooter, const Vector& start);

/**
 *	@brief A tracer from @p start to @p end: grows out of @p start, flies as a fixed-length streak and shrinks into
 *	@p end. Drawn as a beam whose ends it moves every frame (JhlTracers_Update), so unlike the engine's tracers (whose
 *	streak is only drawn once all of it is on screen) it shows from the moment it leaves the muzzle, and its length
 *	and speed are independent.
 */
void JhlTracer_Fire(const Vector& start, const Vector& end, const JhlTracerStyle& style);

/**
 *	@brief Moves the tracers along (HUD_TempEntUpdate, before the engine draws beams).
 */
void JhlTracers_Update(double time);

/**
 *	@brief Forgets the tracers: a new map frees the engine's beams (HUD_VidInit).
 */
void JhlTracers_Reset();
