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

constexpr float JhlReferenceFrameRate = 72;

inline float JhlReferenceFrames(float frametime)
{
	return std::max(0.0f, frametime) * JhlReferenceFrameRate;
}

inline float JhlReferenceDecay(float factor, float frametime)
{
	const float frames = JhlReferenceFrames(frametime);

	if (frames >= 1)
	{
		return std::pow(factor, frames);
	}

	return 1 - (1 - factor) * frames;
}

inline float JhlReferenceSettlingDecay(float factor, float frametime)
{
	const float frames = JhlReferenceFrames(frametime);

	if (frames <= 0)
	{
		return 1;
	}

	const float ratio = factor / (1 - factor) / frames;
	return ratio / (1 + ratio);
}
