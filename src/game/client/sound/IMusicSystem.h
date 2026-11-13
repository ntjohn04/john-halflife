/***
 *
 *	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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

#include <string>

namespace sound
{
// johnhalflife: @brief johnhalflife
struct MusicState
{
	std::string FileName; // Empty: nothing playing.
	bool Looping = false;
	bool Paused = false;
	float Position = 0;
	float FadeRemaining = 0;
	float FadeDuration = 0;
};

/**
 *	@brief Handles music playback.
 */
struct IMusicSystem
{
	virtual ~IMusicSystem() = default;

	virtual void Play(std::string&& fileName, bool looping) = 0;
	virtual void Stop() = 0;
	virtual void Pause() = 0;
	virtual void Resume() = 0;

	virtual MusicState GetState() = 0;

	// johnhalflife: @brief johnhalflife
	virtual void PlayFrom(const MusicState& state) = 0;
};
}
