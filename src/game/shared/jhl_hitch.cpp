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

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "cbase.h"
#include "jhl_hitch.h"
#include "utils/filesystem_utils.h"

bool g_JhlHitchOn = false;

namespace
{
using Clock = std::chrono::steady_clock;

struct Part
{
	double Ms = 0;
	int Calls = 0;
};

struct SlowCall
{
	std::string What;
	double Ms;
};

// Calls of one part slower than this are listed one by one, with their detail.
constexpr double SlowCallMs = 1;
constexpr std::size_t PartsShown = 10;
constexpr std::size_t SlowCallsKept = 64;
constexpr std::size_t SlowCallsShown = 12;
constexpr double SkipSeconds = 2;
// Longer than this is a pause (the game paused with the console or menu open stops the server's frames), not a hitch.
constexpr double PauseMs = 5000;
// A server "frame" that ran more of the player's moves than this is the engine not having started frames (paused).
constexpr int MostMoves = 20;

std::unordered_map<const char*, Part> g_Parts;		  // By name (string literals, so by address).
std::unordered_map<std::string, Part> g_DetailParts; // By name and detail.
std::vector<SlowCall> g_SlowCalls;
double g_TopLevelMs = 0;
int g_Depth = 0;
int g_Moves = 0;
// The longest stretch between two of this library's calls (the engine's own work), and the calls either side.
Clock::time_point g_LastCallEnd;
const char* g_LastCallName = "";
double g_GapMs = 0;
Clock::time_point g_GapStart;
const char* g_GapAfter = "";
const char* g_GapBefore = "";
Clock::time_point g_FrameStart;
Clock::time_point g_SkipUntil;
bool g_HaveFrame = false;
std::thread::id g_MainThread;

double Ms(Clock::duration duration)
{
	return std::chrono::duration<double, std::milli>(duration).count();
}

void Clear()
{
	g_Parts.clear();
	g_DetailParts.clear();
	g_SlowCalls.clear();
	g_TopLevelMs = 0;
	g_Moves = 0;
	g_GapMs = 0;
}

void NoteGap(Clock::time_point now, const char* before)
{
	if (const double gap = Ms(now - g_LastCallEnd); gap > g_GapMs)
	{
		g_GapMs = gap;
		g_GapStart = g_LastCallEnd;
		g_GapAfter = g_LastCallName;
		g_GapBefore = before;
	}
}

void Report(const char* side, double frameMs, const std::string& where)
{
	std::string text = fmt::format("[jhl_hitch {}] {}: frame {:.1f} ms; {} code {:.1f} ms, the engine and the rest {:.1f} ms\n",
		side, where, frameMs, side[0] == 'c' ? "client" : "server", g_TopLevelMs, std::max(0.0, frameMs - g_TopLevelMs));

	// Clock times (ms, the same clock in both libraries) so the client's and the server's reports can be lined up.
	const auto clockMs = [](Clock::time_point time)
	{
		return std::fmod(std::chrono::duration<double, std::milli>(time.time_since_epoch()).count(), 1000000.0);
	};

	text += fmt::format("  longest stretch in the engine: {:.1f} ms, after {}, before {} (clock {:.1f} to {:.1f}; frame from {:.1f})\n",
		g_GapMs, g_GapAfter, g_GapBefore, clockMs(g_GapStart), clockMs(g_GapStart) + g_GapMs, clockMs(g_FrameStart));

	if (side[0] == 's')
	{
		text += fmt::format("  the player's moves run this frame: {}{}\n", g_Moves, g_Moves > 1 ? " (more than one: the engine may have run no frames meanwhile, e.g. paused)" : "");

		// The engine saving the game (trigger_autosave, a quick save): it asks each entity for its data, then writes the
		// file itself, a hitch vanilla has as well (the client's report of the same frame can't tell).
		if (std::any_of(g_Parts.begin(), g_Parts.end(), [](const auto& part)
				{ return std::strcmp(part.first, "DispatchSave") == 0; }))
		{
			text += "  this frame saved the game (an autosave or a quick save): the engine writing it, as vanilla does\n";
		}
	}

	std::vector<std::pair<const char*, Part>> parts{g_Parts.begin(), g_Parts.end()};
	std::sort(parts.begin(), parts.end(), [](const auto& a, const auto& b)
		{ return a.second.Ms > b.second.Ms; });

	if (!parts.empty())
	{
		text += "  parts (each with what it calls):";

		for (std::size_t i = 0; i < std::min(parts.size(), PartsShown); ++i)
		{
			text += fmt::format(" {} {:.1f} (x{}){}", parts[i].first, parts[i].second.Ms, parts[i].second.Calls,
				i + 1 < std::min(parts.size(), PartsShown) ? ";" : "");
		}

		text += '\n';
	}

	std::vector<std::pair<std::string, Part>> details{g_DetailParts.begin(), g_DetailParts.end()};
	std::sort(details.begin(), details.end(), [](const auto& a, const auto& b)
		{ return a.second.Ms > b.second.Ms; });

	if (!details.empty() && details.front().second.Ms >= SlowCallMs)
	{
		text += "  by what:";

		for (std::size_t i = 0; i < std::min(details.size(), PartsShown) && details[i].second.Ms >= SlowCallMs; ++i)
		{
			text += fmt::format(" {} {:.1f} (x{});", details[i].first, details[i].second.Ms, details[i].second.Calls);
		}

		text.back() = '\n';
	}

	std::sort(g_SlowCalls.begin(), g_SlowCalls.end(), [](const auto& a, const auto& b)
		{ return a.Ms > b.Ms; });

	if (!g_SlowCalls.empty())
	{
		text += "  slowest calls:";

		for (std::size_t i = 0; i < std::min(g_SlowCalls.size(), SlowCallsShown); ++i)
		{
			text += fmt::format(" {} {:.1f};", g_SlowCalls[i].What, g_SlowCalls[i].Ms);
		}

		text.back() = '\n';
	}

	// The console prints at most so much at once.
	for (std::size_t start = 0; start < text.size(); start += 900)
	{
		Con_Printf("%s", text.substr(start, 900).c_str());
	}

	if (std::ofstream log{FileSystem_GetGameDirectory() + "/" + FileSystem_GetModDirectoryName() + "/jhl_hitches.txt", std::ios::app}; log)
	{
		log << text;
	}
}
} // namespace

void JhlHitchScope::Begin(const char* name, const char* detail)
{
	if (std::this_thread::get_id() != g_MainThread)
	{
		return;
	}

	m_On = true;
	m_Name = name;
	m_Detail = detail;
	m_Start = Clock::now();

	if (g_Depth == 0)
	{
		NoteGap(m_Start, name);
	}

	++g_Depth;
}

void JhlHitchScope::End()
{
	const auto now = Clock::now();
	const double ms = Ms(now - m_Start);

	--g_Depth;

	if (g_Depth == 0)
	{
		g_TopLevelMs += ms;
		g_LastCallEnd = now;
		g_LastCallName = m_Name;
	}

	Part& part = g_Parts[m_Name];
	part.Ms += ms;
	++part.Calls;

	if (m_Detail && *m_Detail)
	{
		std::string what = m_Name;
		what += ' ';
		what += m_Detail;

		Part& detailPart = g_DetailParts[what];
		detailPart.Ms += ms;
		++detailPart.Calls;

		if (ms >= SlowCallMs && g_SlowCalls.size() < SlowCallsKept)
		{
			g_SlowCalls.push_back({std::move(what), ms});
		}
	}
	else if (ms >= SlowCallMs && g_SlowCalls.size() < SlowCallsKept)
	{
		g_SlowCalls.push_back({m_Name, ms});
	}
}

void JhlHitch_EndFrame(const char* side, float thresholdMs, std::string (*where)())
{
	const auto now = Clock::now();

	g_MainThread = std::this_thread::get_id();

	if (thresholdMs <= 0)
	{
		if (g_JhlHitchOn)
		{
			g_JhlHitchOn = false;
			g_HaveFrame = false;
			Clear();
		}

		return;
	}

	g_JhlHitchOn = true;

	if (g_HaveFrame && g_Depth == 0 && now >= g_SkipUntil)
	{
		const double frameMs = Ms(now - g_FrameStart);

		NoteGap(now, "the next frame");

		if (frameMs > thresholdMs && frameMs < PauseMs && g_Moves <= MostMoves)
		{
			Report(side, frameMs, where());
		}
	}

	g_HaveFrame = true;
	g_FrameStart = now;
	Clear();
	g_LastCallEnd = now;
	g_LastCallName = side[0] == 'c' ? "the frame's start (HUD_Frame)" : "the frame's start (StartFrame)";
}

void JhlHitch_CountMove()
{
	++g_Moves;
}

void JhlHitch_Skip()
{
	g_SkipUntil = Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(SkipSeconds));
}
