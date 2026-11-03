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
/**
 *	@brief johnhalflife: what's playing, for saving it with the game (jhl_music.cpp).
 */
struct MusicState
{
	std::string FileName; // Empty: nothing playing.
	bool Looping = false;
	bool Paused = false; // By the map (music pause), not the game being paused.
	float Position = 0;	 // Seconds into the track, as heard.
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

	/**
	 *	@brief johnhalflife: plays a track from a point, as it was saved (restarting it even if it's already playing).
	 */
	virtual void PlayFrom(const MusicState& state) = 0;
};
}
