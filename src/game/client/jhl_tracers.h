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
 *	(laserbeam.spr) @p Width wide in @p Color (0-255) at @p Brightness (0-1).
 */
struct JhlTracerStyle
{
	Vector Color{255, 60, 30};
	float Brightness = 1;
	float Width = 1.2f;
	float Length = 32;
	float Speed = 4500;
};

/**
 *	@brief A tracer from @p start to @p end: grows out of @p start, flies as a fixed-length streak and shrinks into
 *	@p end. Drawn as a beam whose ends it moves every frame (JhlTracers_Update), so unlike the engine's tracers (whose
 *	streak is only drawn once all of it is on screen) it shows from the moment it leaves the muzzle, and its length
 *	and speed are independent.
 */
void JhlTracer_Fire(const Vector& start, const Vector& end, const JhlTracerStyle& style);

/**
 *	@brief Registers the tuning cvars (jhl_tracer_color, _brightness, _width, _length, _speed, _freq), temporary:
 *	the M249's tracers (and, for now, the MP5's) are played from them while their look is chosen.
 */
void JhlTracers_Initialize();

/**
 *	@brief The tracer style from the tuning cvars.
 */
JhlTracerStyle JhlTracers_TuningStyle();

/**
 *	@brief One round in this many is a tracer, from jhl_tracer_freq (at least 1).
 */
int JhlTracers_TuningFrequency();

/**
 *	@brief Moves the tracers along (HUD_TempEntUpdate, before the engine draws beams).
 */
void JhlTracers_Update(double time);

/**
 *	@brief Forgets the tracers: a new map frees the engine's beams (HUD_VidInit).
 */
void JhlTracers_Reset();
