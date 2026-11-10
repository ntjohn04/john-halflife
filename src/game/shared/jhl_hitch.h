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
#include <chrono>
#include <cstddef>
#include <string>

struct cvar_t;

// johnhalflife (jhl_debug_hitch, a debugging aid): what makes a frame slow. Each library times the calls the engine
// makes into it and the parts of them that might be slow (JhlHitchScope, JHL_TIMED); a frame longer than
// jhl_debug_hitch milliseconds is reported to the console and jhl_hitches.txt in the game's folder, with a verdict:
// John Half-Life's and the Unified SDK's code (the two libraries), the engine presenting the frame (the driver), the
// engine's other work, or a save. On a listen server the client reports both libraries' frame as one: they share a
// thread, and the server's timeline is the client's (JhlHitch_Link). Off (0), a scope costs a test of a flag.

extern bool g_JhlHitchOn;

/**
 *	@brief Times its own life as one call of @p name (a string literal); @p detail (copied) says which, e.g. a
 *	sound's or an entity's name, for the slowest calls.
 */
class JhlHitchScope final
{
public:
	explicit JhlHitchScope(const char* name, const char* detail = nullptr)
	{
		if (g_JhlHitchOn)
		{
			Begin(name, detail);
		}
	}

	~JhlHitchScope()
	{
		if (m_On)
		{
			End();
		}
	}

	JhlHitchScope(const JhlHitchScope&) = delete;
	JhlHitchScope& operator=(const JhlHitchScope&) = delete;

private:
	void Begin(const char* name, const char* detail);
	void End();

	const char* m_Name{};
	const char* m_Detail{};
	std::chrono::steady_clock::time_point m_Start;
	bool m_On{false};
};

/**
 *	@brief Ends a frame: if it took longer than @p thresholdMs (0: off), what it was spent on is printed and logged, with
 *	what @p where gives (map and position; asked only then, the frame being one that was played). Called as each frame
 *	starts. While the client's linked to the server (JhlHitch_Link), the server's frames are the client's.
 */
void JhlHitch_EndFrame(float thresholdMs, std::string (*where)());

/**
 *	@brief Frames for the next 2 seconds aren't reported (a map loading).
 */
void JhlHitch_Skip();

/**
 *	@brief The server ran a player's move: a server "frame" with several isn't a hitch, the engine not having started
 *	frames meanwhile (it runs the player's moves, not frames, while the game is paused, e.g. by the console); a few are let through.
 */
void JhlHitch_CountMove();

/**
 *	@brief The server's console variable through which the client finds it (registered in GameDLLInit): the engine
 *	keeps the cvar_t it's given, the rest of what's shared following it.
 */
cvar_t* JhlHitch_ServerLink();

/**
 *	@brief The client finds the server running in its process (a listen server) through @p find (the engine's
 *	pfnGetCvarPointer), once a second until found, while the hitch finder is on. Called before each JhlHitch_EndFrame.
 */
void JhlHitch_Link(cvar_t* (*find)(const char* name));

template <std::size_t N>
struct JhlHitchName
{
	constexpr JhlHitchName(const char (&text)[N])
	{
		std::copy_n(text, N, Text);
	}

	char Text[N];
};

template <JhlHitchName Name, auto Function>
struct JhlTimed;

template <JhlHitchName Name, typename R, typename... Args, R (*Function)(Args...)>
struct JhlTimed<Name, Function>
{
	static R Call(Args... args)
	{
		JhlHitchScope scope{Name.Text};
		return Function(args...);
	}
};

/**
 *	@brief @p function, timed under its own name: for the engine's function tables.
 */
#define JHL_TIMED(function) (&JhlTimed<#function, function>::Call)
