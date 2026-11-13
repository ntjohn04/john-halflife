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

#include <cstring>
#include <deque>
#include <vector>

#include "hud.h"
#include "beamdef.h"
#include "com_model.h"
#include "event_api.h"
#include "r_efx.h"
#include "jhl_hornets.h"

namespace
{
constexpr int Pieces = 8;
constexpr float PieceTime = 0.0375f;
constexpr std::size_t MaxHornets = 10;

constexpr float Width = 1.2f;
constexpr float Brightness = 0.5f;

constexpr float TwirlPeriod = 0.24f;
constexpr float TwirlRadius = 2.42f;
constexpr float TwirlSpeedDrift = 0.33f;  // ± of its turning speed,
constexpr float TwirlSpeedCycle = 0.9f;	  // over about this long.
constexpr float TwirlRadiusDrift = 0.42f; // ± of its radius,
constexpr float TwirlRadiusCycleA = 0.7f; // over two cycles out of step.
constexpr float TwirlRadiusCycleB = 1.3f;

constexpr float FlourishTime = 0.45f;
constexpr float FlourishRadius = 2;
constexpr float FlourishSpeed = 1.25f;
constexpr float FlourishFirstMin = 0.3f;
constexpr float FlourishFirstMax = 1.5f;
constexpr float FlourishEveryMin = 0.8f;
constexpr float FlourishEveryMax = 1.8f;

constexpr float CalmTime = 0.6f;
constexpr float CalmRadius = 0.5f;
constexpr float CalmSpeed = 0.58f;

constexpr float LostAfter = 0.15f;

struct Sample
{
	float Time;
	Vector Position;
	bool Committed;
};

struct Hornet
{
	int Index;
	float LastSeen;
	float Phase;
	float SpeedPhase;
	float RadiusPhaseA;
	float RadiusPhaseB;
	std::vector<float> Flourishes; // When each started.
	float NextFlourish;
	std::vector<float> Calms;
	float NextCalm;
	Vector Color;
	std::deque<Sample> History;
	BEAM* Beams[Pieces]{};
};

std::vector<Hornet> g_Hornets;

struct Overflow
{
	int Index;
	float LastSeen;
};

std::vector<Overflow> g_Overflow;
float g_LastTime = 0;

void Kill(BEAM*& beam)
{
	if (beam)
	{
		beam->die = 0;
		beam->brightness = 0;
		beam = nullptr;
	}
}

void Spells(const std::vector<float>& starts, float duration, float when, float& swell, float& turned)
{
	constexpr float Pi = 3.14159265f;
	swell = 0;
	turned = 0;

	for (const float start : starts)
	{
		const float x = when - start;

		if (x <= 0)
			continue;

		if (x < duration)
		{
			const float s = std::sin(Pi * x / duration);
			swell = std::max(swell, s * s);
			turned += x / 2 - duration / (4 * Pi) * std::sin(2 * Pi * x / duration);
		}
		else
		{
			turned += duration / 2;
		}
	}
}

// The next spell on a schedule, while it's flying.
void Schedule(std::vector<float>& starts, float& next, float now)
{
	if (now >= next)
	{
		starts.push_back(next);
		next += gEngfuncs.pfnRandomFloat(FlourishEveryMin, FlourishEveryMax);
	}
}

// Where the hornet was at @p when, between samples.
bool At(const Hornet& hornet, float when, Sample& out)
{
	const auto& history = hornet.History;

	if (history.empty() || when < history.front().Time)
		return false;

	for (std::size_t i = history.size() - 1; i > 0; --i)
	{
		const Sample& newer = history[i];
		const Sample& older = history[i - 1];

		if (older.Time > when)
			continue;

		const float f = newer.Time > older.Time ? (when - older.Time) / (newer.Time - older.Time) : 1;
		out = {when, older.Position + (newer.Position - older.Position) * std::min(1.f, f), newer.Committed};
		return true;
	}

	out = history.front();
	return true;
}
} // namespace

void JhlHornets_Track(cl_entity_t* entity, const char* modelname)
{
	const int state = entity->curstate.renderamt;

	if (!modelname || 0 != std::strcmp(modelname, "models/hornet.mdl") || (state != 1 && state != 2))
		return;

	const float now = gEngfuncs.GetClientTime();

	Hornet* hornet = nullptr;

	for (auto& candidate : g_Hornets)
	{
		if (candidate.Index == entity->index)
			hornet = &candidate;
	}

	if (!hornet)
	{
		if (g_Hornets.size() >= MaxHornets)
		{
			for (auto& overflow : g_Overflow)
			{
				if (overflow.Index == entity->index)
				{
					overflow.LastSeen = now;
					return;
				}
			}

			if (const int sprite = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/laserbeam.spr"); sprite != 0)
			{
				const auto& color = entity->curstate.rendercolor;
				gEngfuncs.pEfxAPI->R_BeamFollow(entity->index, sprite, 1, 2, color.r / 255.f, color.g / 255.f, color.b / 255.f,
					Brightness);
			}

			g_Overflow.push_back({entity->index, now});
			return;
		}

		const auto random = []
		{ return gEngfuncs.pfnRandomFloat(0, 6.2831853f); };
		g_Hornets.push_back({entity->index, now, random(), random(), random(), random(), {},
			now + gEngfuncs.pfnRandomFloat(FlourishFirstMin, FlourishFirstMax), {},
			now + gEngfuncs.pfnRandomFloat(FlourishFirstMin, FlourishFirstMax), {}, {}, {}});
		hornet = &g_Hornets.back();

		switch (gEngfuncs.pfnRandomLong(0, 2))
		{
		case 0:
			hornet->Calms.push_back(now - 0.05f);
			break;
		case 2:
			hornet->Flourishes.push_back(now - 0.05f);
			break;
		default:
			break;
		}
	}

	hornet->LastSeen = now;
	hornet->Color = {static_cast<float>(entity->curstate.rendercolor.r), static_cast<float>(entity->curstate.rendercolor.g),
		static_cast<float>(entity->curstate.rendercolor.b)};

	if (hornet->History.empty() || hornet->History.back().Time < now)
		hornet->History.push_back({now, entity->origin, state == 2});
}

void JhlHornets_Update(double time)
{
	const float now = static_cast<float>(time);

	// A new map restarts the clock (and frees the beams).
	if (now < g_LastTime)
	{
		g_Hornets.clear();
		g_Overflow.clear();
	}

	g_LastTime = now;

	// Overflow hornets gone (their follow trails go with them).
	std::erase_if(g_Overflow, [&](const Overflow& overflow)
		{ return now - overflow.LastSeen > LostAfter; });

	const int sprite = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/laserbeam.spr");

	for (auto it = g_Hornets.begin(); it != g_Hornets.end();)
	{
		Hornet& hornet = *it;

		while (!hornet.History.empty() && hornet.History.front().Time < now - Pieces * PieceTime - 0.05f)
			hornet.History.pop_front();

		// Its next spells, while it's flying.
		if (now - hornet.LastSeen <= LostAfter)
		{
			Schedule(hornet.Flourishes, hornet.NextFlourish, now);
			Schedule(hornet.Calms, hornet.NextCalm, now);
		}

		// Gone, and its trail faded out.
		if (hornet.History.size() < 2 && now - hornet.LastSeen > LostAfter)
		{
			for (BEAM*& beam : hornet.Beams)
				Kill(beam);

			it = g_Hornets.erase(it);
			continue;
		}

		const float head = std::min(now, hornet.LastSeen);
		Vector points[Pieces + 1];
		bool known[Pieces + 1];

		for (int k = 0; k <= Pieces; ++k)
		{
			const float when = head - k * PieceTime;
			Sample here, before, after;
			known[k] = At(hornet, when, here);

			if (!known[k])
				continue;

			points[k] = here.Position;

			if (here.Committed || !At(hornet, when - PieceTime * 0.5f, before) || !At(hornet, std::min(head, when + PieceTime * 0.5f), after))
				continue;

			// Around its way of flying there.
			Vector along = after.Position - before.Position;

			if (along.Length() < 0.1f)
				continue;

			along = along.Normalize();
			Vector side = CrossProduct(along, Vector{0, 0, 1});

			if (side.Length() < 0.1f)
				side = CrossProduct(along, Vector{1, 0, 0});

			side = side.Normalize();
			const Vector over = CrossProduct(side, along);
			constexpr float Tau = 6.2831853f;
			const float speedRate = Tau / TwirlSpeedCycle;
			float swell, turned, calm, calmed;
			Spells(hornet.Flourishes, FlourishTime, when, swell, turned);
			Spells(hornet.Calms, CalmTime, when, calm, calmed);

			const float angle = hornet.Phase +
								(Tau / TwirlPeriod) * (when - (TwirlSpeedDrift / speedRate) * std::cos(speedRate * when + hornet.SpeedPhase) +
														  (FlourishSpeed - 1) * turned + (CalmSpeed - 1) * calmed);
			const float radius = TwirlRadius * (1 + (FlourishRadius - 1) * swell) * (1 + (CalmRadius - 1) * calm) *
								 (1 + TwirlRadiusDrift * 0.5f *
										  (std::sin(Tau / TwirlRadiusCycleA * when + hornet.RadiusPhaseA) +
											  std::sin(Tau / TwirlRadiusCycleB * when + hornet.RadiusPhaseB)));
			points[k] = points[k] + (side * std::cos(angle) + over * std::sin(angle)) * radius;
		}

		for (int i = 0; i < Pieces; ++i)
		{
			Sample from, to;
			from.Position = points[i + 1];
			to.Position = points[i];

			if (sprite == 0 || !known[i] || !known[i + 1] || (to.Position - from.Position).Length() < 0.5f)
			{
				if (hornet.Beams[i])
					hornet.Beams[i]->brightness = 0;

				continue;
			}

			// Fading toward the tail, and out once it's gone.
			const float gone = now - hornet.LastSeen > LostAfter ? std::clamp(1 - (now - hornet.LastSeen) / (Pieces * PieceTime), 0.f, 1.f) : 1;
			const float brightness = Brightness * (1 - static_cast<float>(i) / Pieces) * gone;

			BEAM*& beam = hornet.Beams[i];

			if (!beam)
			{
				beam = gEngfuncs.pEfxAPI->R_BeamPoints(from.Position, to.Position, sprite, 1, Width, 0, brightness, 0, 0, 0,
					hornet.Color.x / 255, hornet.Color.y / 255, hornet.Color.z / 255);

				if (!beam)
					continue;
			}

			beam->source = from.Position;
			beam->target = to.Position;
			beam->delta = to.Position - from.Position;
			beam->brightness = brightness;
			beam->die = now + 0.2f;
			beam->flags |= FBEAM_ISACTIVE;
		}

		++it;
	}
}

void JhlHornets_Reset()
{
	g_Hornets.clear();
	g_Overflow.clear();
	g_LastTime = 0;
}
