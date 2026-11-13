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

extern bool g_JhlHitchOn;

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

void JhlHitch_EndFrame(float thresholdMs, std::string (*where)());

void JhlHitch_Skip();

void JhlHitch_CountMove();

cvar_t* JhlHitch_ServerLink();

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

#define JHL_TIMED(function) (&JhlTimed<#function, function>::Call)
