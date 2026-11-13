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
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "cbase.h"
#include "cvardef.h"
#include "jhl_hitch.h"
#include "utils/filesystem_utils.h"

bool g_JhlHitchOn = false;

namespace
{
using Clock = std::chrono::steady_clock;

#ifdef CLIENT_DLL
constexpr int Side = 0;
#else
constexpr int Side = 1;
#endif

constexpr double SlowCallMs = 1;
constexpr std::size_t PartsShown = 10;
constexpr std::size_t SlowCallsKept = 64;
constexpr std::size_t SlowCallsShown = 12;
constexpr double SkipSeconds = 2;
constexpr double PauseMs = 5000;
constexpr int MostMoves = 20;
// The frames a report's "usually" is the median of.
constexpr std::size_t RecentFrames = 240;
constexpr std::size_t NameSize = 64;

std::int64_t Ticks(Clock::time_point time)
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count();
}

double Ms(std::int64_t ticks)
{
	return ticks / 1e6;
}

void Copy(char (&to)[NameSize], const char* from)
{
	std::strncpy(to, from, NameSize - 1);
	to[NameSize - 1] = '\0';
}

struct Timeline
{
	std::int64_t LastCallEnd = 0;
	char LastCallName[NameSize]{};
	bool LastWasRedraw = false;
	double CodeMs[2]{};			// The client's, the server's.
	double EngineMs[2]{};
	double PresentMs = 0;
	double GapMs = 0;
	char GapAfter[NameSize]{};
	char GapBefore[NameSize]{};
	std::int64_t SkipUntil = 0;
};

struct Entry
{
	char Name[NameSize];
	double Ms;
	int Calls;
};

// One library's frame, by part.
struct Summary
{
	Entry Parts[PartsShown];
	int PartCount = 0;
	Entry Details[PartsShown];
	int DetailCount = 0;
	Entry Slow[SlowCallsShown];
	int SlowCount = 0;
	double SaveMs = 0;
	int SavedEntities = 0;
	int Moves = 0;
};

constexpr std::uint32_t LinkMagic = 0x4A484854; // "JHHT"

struct Shared
{
	cvar_t Cvar;
	std::uint32_t Magic;
	std::uint32_t Size;
	Timeline Frame;
	void (*Summarize)(Summary* summary);
	std::int64_t ClientDrove;
};

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

struct Recent
{
	float FrameMs, CodeMs, PresentMs, OtherMs;
};

std::unordered_map<const char*, Part> g_Parts;
std::unordered_map<std::string, Part> g_DetailParts; // By name and detail.
std::vector<SlowCall> g_SlowCalls;
int g_Depth = 0;
int g_Moves = 0;
Shared* g_Linked = nullptr;
std::int64_t g_NextLinkTry = 0;
std::int64_t g_FrameStart = 0;
bool g_HaveFrame = false;
std::thread::id g_MainThread;
std::vector<Recent> g_Recent;
std::size_t g_NextRecent = 0;
bool g_LoggedSession = false;

void ClearOwnParts()
{
	g_Parts.clear();
	g_DetailParts.clear();
	g_SlowCalls.clear();
	g_Moves = 0;
}

void Summarize(Summary* summary)
{
	if (summary)
	{
		*summary = {};

		const auto top = [](auto& map, Entry* into, int& count)
		{
			std::vector<std::pair<std::string, Part>> sorted;

			for (const auto& [name, part] : map)
			{
				sorted.emplace_back(name, part);
			}

			std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b)
				{ return a.second.Ms > b.second.Ms; });

			for (const auto& [name, part] : sorted)
			{
				if (count == static_cast<int>(PartsShown))
				{
					break;
				}

				Copy(into[count].Name, name.c_str());
				into[count].Ms = part.Ms;
				into[count].Calls = part.Calls;
				++count;
			}
		};

		top(g_Parts, summary->Parts, summary->PartCount);
		top(g_DetailParts, summary->Details, summary->DetailCount);

		std::sort(g_SlowCalls.begin(), g_SlowCalls.end(), [](const auto& a, const auto& b)
			{ return a.Ms > b.Ms; });

		for (const auto& call : g_SlowCalls)
		{
			if (summary->SlowCount == static_cast<int>(SlowCallsShown))
			{
				break;
			}

			Entry& entry = summary->Slow[summary->SlowCount++];
			Copy(entry.Name, call.What.c_str());
			entry.Ms = call.Ms;
			entry.Calls = 1;
		}

		for (const auto& [name, part] : g_Parts)
		{
			if (std::strcmp(name, "DispatchSave") == 0 || std::strcmp(name, "SaveGlobalState") == 0)
			{
				summary->SaveMs += part.Ms;
			}

			if (std::strcmp(name, "DispatchSave") == 0)
			{
				summary->SavedEntities = part.Calls;
			}
		}

		summary->Moves = g_Moves;
	}

	ClearOwnParts();
}

#ifndef CLIENT_DLL
Shared g_Shared{
	{"jhl_debug_hitch_link", "0", 0, 0, nullptr},
	LinkMagic,
	sizeof(Shared),
	{},
	&Summarize,
	0};

Timeline* g_Frame = &g_Shared.Frame;
#else
Timeline g_OwnFrame;
Timeline* g_Frame = &g_OwnFrame;
#endif

void NoteGap(std::int64_t now, const char* before)
{
	Timeline& frame = *g_Frame;

	if (frame.LastWasRedraw)
	{
		frame.PresentMs += Ms(now - frame.LastCallEnd);
		frame.LastWasRedraw = false;
	}

	if (const double gap = Ms(now - frame.LastCallEnd); gap > frame.GapMs)
	{
		frame.GapMs = gap;
		std::memcpy(frame.GapAfter, frame.LastCallName, NameSize);
		Copy(frame.GapBefore, before);
	}
}

void ResetFrame(std::int64_t now, const char* start)
{
	Timeline& frame = *g_Frame;
	const std::int64_t skipUntil = frame.SkipUntil;

	frame = {};
	frame.SkipUntil = skipUntil;
	frame.LastCallEnd = now;
	Copy(frame.LastCallName, start);
}

double Median(float Recent::*field)
{
	if (g_Recent.empty())
	{
		return 0;
	}

	std::vector<float> values;
	values.reserve(g_Recent.size());

	for (const auto& recent : g_Recent)
	{
		values.push_back(recent.*field);
	}

	std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
	return values[values.size() / 2];
}

std::string WallTime(std::string* date)
{
	const auto now = std::chrono::system_clock::now();
	const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
	const int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
	std::tm local{};
#ifdef _WIN32
	localtime_s(&local, &seconds);
#else
	localtime_r(&seconds, &local);
#endif

	if (date)
	{
		*date = fmt::format("{:04}-{:02}-{:02}", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
	}

	return fmt::format("{:02}:{:02}:{:02}.{:03}", local.tm_hour, local.tm_min, local.tm_sec, ms);
}

void AppendParts(std::string& text, const char* label, const Entry* entries, int count, bool calls)
{
	if (count == 0)
	{
		return;
	}

	text += fmt::format("  {}:", label);

	for (int i = 0; i < count; ++i)
	{
		text += calls ? fmt::format(" {} {:.1f} (x{});", entries[i].Name, entries[i].Ms, entries[i].Calls)
					  : fmt::format(" {} {:.1f};", entries[i].Name, entries[i].Ms);
	}

	text.back() = '\n';
}

void Report(double frameMs, const std::string& where, const Summary* client, const Summary* server)
{
	const Timeline& frame = *g_Frame;
	const double engineInCode = frame.EngineMs[0] + frame.EngineMs[1];
	const double codeMs = frame.CodeMs[0] + frame.CodeMs[1] - engineInCode;
	const double presentMs = frame.PresentMs;
	const double otherMs = std::max(0.0, frameMs - codeMs - presentMs);
	const double usualFrame = Median(&Recent::FrameMs);
	const double usualCode = Median(&Recent::CodeMs);
	const double usualPresent = Median(&Recent::PresentMs);
	const double usualOther = Median(&Recent::OtherMs);
	const double excess = std::max(0.0, frameMs - usualFrame);
	const double saveMs = server ? server->SaveMs : 0;

	std::string date;
	const std::string time = WallTime(&date);

	std::string text = fmt::format("[jhl_hitch] {} {}: a {:.1f} ms frame, usually {:.1f}\n", time, where, frameMs, usualFrame);

	// The verdict: what took the time beyond a usual frame.
	const std::string ours = fmt::format("their code {:.1f} ms, usually {:.1f}", codeMs, usualCode);

	if (server && server->SavedEntities > 0)
	{
		text += fmt::format("  verdict: a save (an autosave or a quick save): the engine writing it, as vanilla does, {:.1f} ms; "
							"the {} entities' data from John Half-Life and the Unified SDK {:.1f} ms\n",
			otherMs, server->SavedEntities, saveMs);
	}
	else if (codeMs - usualCode >= excess / 2 && codeMs >= 1)
	{
		const Entry* top = nullptr;

		for (const Summary* summary : {client, server})
		{
			if (!summary)
			{
				continue;
			}

			const Entry* candidate = summary->DetailCount > 0 && summary->Details[0].Ms >= SlowCallMs ? &summary->Details[0]
									 : summary->PartCount > 0										 ? &summary->Parts[0]
																									 : nullptr;

			if (candidate && (!top || candidate->Ms > top->Ms))
			{
				top = candidate;
			}
		}

		text += fmt::format("  verdict: John Half-Life or the Unified SDK ({}), most in {} ({:.1f} ms)\n",
			ours, top ? top->Name : "?", top ? top->Ms : 0.0);
	}
	else if (client && presentMs - usualPresent >= otherMs - usualOther)
	{
		text += fmt::format("  verdict: not John Half-Life or the Unified SDK ({}): the engine presenting the frame {:.1f} ms, "
							"usually {:.1f}: the driver, the GPU or Windows\n",
			ours, presentMs, usualPresent);
	}
	else
	{
		text += fmt::format("  verdict: not John Half-Life or the Unified SDK ({}): the engine's own work {:.1f} ms, usually {:.1f}{}\n",
			ours, otherMs, usualOther,
			engineInCode >= 1 ? fmt::format(", {:.1f} of it drawing models (a model's first draw this map is named below)", engineInCode) : "");
	}

	// Where all of it went.
	text += fmt::format("  time: John Half-Life and the Unified SDK {:.1f} ms (client {:.1f}, server {:.1f})", codeMs,
		frame.CodeMs[0] - frame.EngineMs[0], frame.CodeMs[1] - frame.EngineMs[1]);

	if (client)
	{
		text += fmt::format("; the engine presenting the frame (the driver's swap, vsync, fps_max) {:.1f}", presentMs);
	}

	text += fmt::format("; the engine's other work {:.1f}\n", otherMs);
	text += fmt::format("  longest stretch outside them: {:.1f} ms, after {}, before {}\n", frame.GapMs, frame.GapAfter, frame.GapBefore);

	if (server && server->Moves > 1)
	{
		text += fmt::format("  the player's moves run this frame: {} (more than one: the engine may have run no frames meanwhile, e.g. paused)\n", server->Moves);
	}

	for (const auto& [label, summary] : {std::pair{"client", client}, std::pair{"server", server}})
	{
		if (summary)
		{
			AppendParts(text, fmt::format("{} (each part with what it calls)", label).c_str(), summary->Parts, summary->PartCount, true);
		}
	}

	for (const auto& [label, summary] : {std::pair{"client", client}, std::pair{"server", server}})
	{
		if (summary)
		{
			int slow = 0;

			while (slow < summary->DetailCount && summary->Details[slow].Ms >= SlowCallMs)
			{
				++slow;
			}

			AppendParts(text, fmt::format("{} by what", label).c_str(), summary->Details, slow, true);
			AppendParts(text, fmt::format("{} slowest calls", label).c_str(), summary->Slow, summary->SlowCount, false);
		}
	}

	// The console prints at most so much at once.
	for (std::size_t start = 0; start < text.size(); start += 900)
	{
		Con_Printf("%s", text.substr(start, 900).c_str());
	}

	if (std::ofstream log{FileSystem_GetGameDirectory() + "/" + FileSystem_GetModDirectoryName() + "/jhl_hitches.txt", std::ios::app}; log)
	{
		if (!g_LoggedSession)
		{
			g_LoggedSession = true;
			log << fmt::format("--- {} {}: John Half-Life's hitch finder, frames over jhl_debug_hitch\n", date, time);
		}

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
		NoteGap(Ticks(m_Start), name);
	}

	++g_Depth;
}

void JhlHitchScope::End()
{
	const auto now = Clock::now();
	const double ms = std::chrono::duration<double, std::milli>(now - m_Start).count();

	--g_Depth;

	if (g_Depth == 0)
	{
		Timeline& frame = *g_Frame;
		frame.CodeMs[Side] += ms;
		frame.LastCallEnd = Ticks(now);
		Copy(frame.LastCallName, m_Name);
		frame.LastWasRedraw = Side == 0 && std::strcmp(m_Name, "HUD_Redraw") == 0;
	}

	Part& part = g_Parts[m_Name];
	part.Ms += ms;
	++part.Calls;

	if (std::strncmp(m_Name, "engine", 6) == 0)
		g_Frame->EngineMs[Side] += ms;

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

void JhlHitch_EndFrame(float thresholdMs, std::string (*where)())
{
	const std::int64_t now = Ticks(Clock::now());

	g_MainThread = std::this_thread::get_id();

	if (thresholdMs <= 0)
	{
		if (g_JhlHitchOn)
		{
			g_JhlHitchOn = false;
			g_HaveFrame = false;
			g_Recent.clear();
			ClearOwnParts();
		}

		return;
	}

	g_JhlHitchOn = true;

#ifndef CLIENT_DLL
	if (now - g_Shared.ClientDrove < 1'000'000'000)
	{
		g_HaveFrame = false;
		return;
	}
#endif

	const char* start = Side == 0 ? "the frame's start (HUD_Frame)" : "the frame's start (StartFrame)";
	bool reported = false;

	if (g_HaveFrame && g_Depth == 0)
	{
		const double frameMs = Ms(now - g_FrameStart);

		NoteGap(now, "the next frame");

		Summary own;
		Summarize(&own);
		Summary server;

		if (g_Linked)
		{
			g_Linked->Summarize(&server);
		}

		const Summary* clientSummary = Side == 0 ? &own : nullptr;
		const Summary* serverSummary = Side == 1 ? &own : g_Linked ? &server : nullptr;
		const int moves = serverSummary ? serverSummary->Moves : 0;
		const Timeline& frame = *g_Frame;
		const double codeMs = frame.CodeMs[0] + frame.CodeMs[1] - frame.EngineMs[0] - frame.EngineMs[1];

		if (frameMs > thresholdMs && frameMs < PauseMs && moves <= MostMoves && now >= frame.SkipUntil)
		{
			Report(frameMs, where(), clientSummary, serverSummary);
			reported = true;
		}

		const Recent recent{static_cast<float>(frameMs), static_cast<float>(codeMs), static_cast<float>(frame.PresentMs),
			static_cast<float>(std::max(0.0, frameMs - codeMs - frame.PresentMs))};

		if (g_Recent.size() < RecentFrames)
		{
			g_Recent.push_back(recent);
		}
		else
		{
			g_Recent[g_NextRecent] = recent;
			g_NextRecent = (g_NextRecent + 1) % RecentFrames;
		}
	}
	else
	{
		ClearOwnParts();

		if (g_Linked)
		{
			g_Linked->Summarize(nullptr);
		}
	}

	g_HaveFrame = true;
	g_FrameStart = now;
	ResetFrame(now, start);

	if (reported)
	{
		const std::int64_t after = Ticks(Clock::now());
		g_Parts["jhl_debug_hitch's report"] = {Ms(after - now), 1};
		g_Frame->CodeMs[Side] += Ms(after - now);
		g_Frame->LastCallEnd = after;
	}

	if (g_Linked)
	{
		g_Linked->ClientDrove = now;
	}
}

void JhlHitch_CountMove()
{
	++g_Moves;
}

void JhlHitch_Skip()
{
	g_Frame->SkipUntil = Ticks(Clock::now()) + static_cast<std::int64_t>(SkipSeconds * 1e9);
}

cvar_t* JhlHitch_ServerLink()
{
#ifndef CLIENT_DLL
	return &g_Shared.Cvar;
#else
	return nullptr;
#endif
}

void JhlHitch_Link(cvar_t* (*find)(const char* name))
{
	if (!g_JhlHitchOn || g_Linked || g_Depth != 0)
	{
		return;
	}

	const std::int64_t now = Ticks(Clock::now());

	if (now < g_NextLinkTry)
	{
		return;
	}

	g_NextLinkTry = now + 1'000'000'000;

	if (auto* shared = reinterpret_cast<Shared*>(find("jhl_debug_hitch_link"));
		shared && shared->Magic == LinkMagic && shared->Size == sizeof(Shared))
	{
		const std::int64_t skipUntil = std::max(g_Frame->SkipUntil, shared->Frame.SkipUntil);

		g_Linked = shared;
		g_Frame = &shared->Frame;
		shared->Summarize(nullptr);
		g_HaveFrame = false;
		g_Frame->SkipUntil = skipUntil;
		Con_Printf("jhl_debug_hitch: the server's frames reported with the client's\n");
	}
}
