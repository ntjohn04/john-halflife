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

struct JhlMusicState
{
	char FileName[260]; // Empty: none.
	bool Looping;
	bool Paused;
	float Position;		   // Seconds into the track.
	float FadeRemaining;
	float FadeDuration;	   // and the whole fade's.
};

using JhlMusicGetStateFn = bool (*)(JhlMusicState* state);

constexpr const char* JhlMusicGetStateName = "JhlMusic_GetState";
