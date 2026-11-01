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

#include <algorithm>
#include <cmath>

// johnhalflife (jhl_physics_fpsfix): Half-Life's game code applies some changes once a frame (a player's input frame,
// or a touch every frame something slides along the floor) against forces that act over time, so how strong they are
// follows the frame rate: grenades stop shorter, boxes pulled with +use run away faster, the higher it is. Each is
// made to act at any frame rate as it did at 72 fps, Half-Life's own fps_max in 1998 (from QuakeWorld): the rate the
// game was made to run at. At 72 fps nothing changes.
constexpr float JhlReferenceFrameRate = 72;

/**
 *	@brief How many 72 fps frames a frame this long is.
 */
inline float JhlReferenceFrames(float frametime)
{
	return std::max(0.0f, frametime) * JhlReferenceFrameRate;
}

/**
 *	@brief A factor velocity is multiplied by once a frame (friction), for a frame this long: what it takes off adds up
 *	the same over time (a sliding thing stops where it did at 72 fps). Faster frames take that much less off; slower
 *	ones compound it.
 */
inline float JhlReferenceDecay(float factor, float frametime)
{
	const float frames = JhlReferenceFrames(frametime);

	if (frames >= 1)
	{
		return std::pow(factor, frames);
	}

	return 1 - (1 - factor) * frames;
}

/**
 *	@brief A factor velocity is multiplied by once a frame while something else adds to it over time (the +use
 *	slowdown against walking): the speed they settle at is what it was at 72 fps.
 */
inline float JhlReferenceSettlingDecay(float factor, float frametime)
{
	const float frames = JhlReferenceFrames(frametime);

	if (frames <= 0)
	{
		return 1;
	}

	// Settles where factor / (1 - factor) * added a frame is the same: added a frame goes with the frame's length.
	const float ratio = factor / (1 - factor) / frames;
	return ratio / (1 + ratio);
}
