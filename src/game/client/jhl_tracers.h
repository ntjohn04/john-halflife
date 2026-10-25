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
	float StartOffsetMax = 180;
};

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
