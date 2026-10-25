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

#include <cmath>
#include <deque>
#include <vector>

#include "hud.h"
#include "beamdef.h"
#include "com_model.h"
#include "r_efx.h"
#include "parsemsg.h"
#include "jhl_rings.h"

namespace
{
// Where the two entities were at a time, for a strand trailing them.
struct Sample
{
	double Time;
	Vector Start;
	Vector End;
};

struct SpireRing
{
	BEAM* Beam; // Null until made: the engine won't make a beam out of view.
	int StartEntity;
	int EndEntity;
	float Scale;
	int Sprite;
	int FrameStart;
	float Framerate;
	float Width;
	float Noise;
	float R, G, B;
	float Brightness;
	float Speed;
	// Trailing the entities by 0 to LagMax seconds and back, every LagPeriod seconds, from LagPhase (0-1).
	float LagMax;
	float LagPeriod;
	float LagPhase;
	std::deque<Sample> History;
};

// c4a1 has three rings, of3a1b the same, two strands each; a few to spare.
constexpr std::size_t MaxRings = 16;

// The strand is kept alive this far ahead each frame, so it dies by itself soon after it's no longer tracked.
constexpr float KeepAlive = 1;

// An entity moving further than this between two samples jumped (a train sent back to its path's start): a strand
// trailing it jumps too, rather than sweeping across between.
constexpr float JumpDistance = 128;

std::vector<SpireRing> g_Rings;
double g_LastTime = 0;

// Whether an entity came in this frame's update (out of view, the engine stops drawing a ring on it too).
cl_entity_t* ActiveEntity(int index)
{
	cl_entity_t* entity = gEngfuncs.GetEntityByIndex(index);
	cl_entity_t* player = gEngfuncs.GetLocalPlayer();

	if (!entity || !player || !entity->model || entity->curstate.messagenum != player->curstate.messagenum)
		return nullptr;

	return entity;
}

// Where the entities were @p lag seconds ago (the oldest known, if not that far back).
Sample Trailing(const SpireRing& ring, double time, float lag)
{
	const double when = time - lag;

	for (std::size_t i = ring.History.size() - 1; i > 0; --i)
	{
		const Sample& newer = ring.History[i];
		const Sample& older = ring.History[i - 1];

		if (older.Time > when)
			continue;

		if ((newer.Start - older.Start).Length() > JumpDistance || (newer.End - older.End).Length() > JumpDistance)
			return older;

		const float f = newer.Time > older.Time ? static_cast<float>((when - older.Time) / (newer.Time - older.Time)) : 1;
		return {when, older.Start + (newer.Start - older.Start) * f, older.End + (newer.End - older.End) * f};
	}

	return ring.History.front();
}
} // namespace

void JhlRings_ParseSpireRing(BufferReader& reader)
{
	const int startEntity = reader.ReadShort();
	const int endEntity = reader.ReadShort();
	const int sprite = reader.ReadShort();
	const float frameStart = reader.ReadByte();
	const float framerate = reader.ReadByte() * 0.1f;
	const float width = reader.ReadByte() * 0.1f;
	const float noise = reader.ReadByte() * 0.01f;
	const float r = reader.ReadByte() / 255.f;
	const float g = reader.ReadByte() / 255.f;
	const float b = reader.ReadByte() / 255.f;
	const float brightness = reader.ReadByte() / 255.f;
	const float speed = reader.ReadByte() * 0.1f;
	const float scale = reader.ReadShort() * 0.01f;
	const float lagMax = reader.ReadByte() * 0.01f;
	const float lagPeriod = reader.ReadByte() * 0.1f;

	if (g_Rings.size() >= MaxRings)
		return;

	gEngfuncs.Con_DPrintf("johnhalflife: spire ring strand between entities %d and %d\n", startEntity, endEntity);

	g_Rings.push_back({nullptr, startEntity, endEntity, scale, sprite, static_cast<int>(frameStart), framerate, width,
		noise, r, g, b, brightness, speed, lagMax, lagPeriod, gEngfuncs.pfnRandomFloat(0, 1), {}});
}

void JhlRings_Update(double time)
{
	// A new map or a loaded game restarts the clock (and frees the beams).
	if (time < g_LastTime)
		g_Rings.clear();

	g_LastTime = time;

	for (auto& ring : g_Rings)
	{
		if (ring.Beam)
			ring.Beam->die = static_cast<float>(time) + KeepAlive;

		cl_entity_t* start = ActiveEntity(ring.StartEntity);
		cl_entity_t* end = ActiveEntity(ring.EndEntity);

		if (!start || !end)
		{
			if (ring.Beam)
				ring.Beam->brightness = 0;

			ring.History.clear();
			continue;
		}

		// Trailing them by a lag swinging smoothly from 0 to LagMax and back, so it falls behind the map's ring and
		// catches up again.
		ring.History.push_back({time, start->origin, end->origin});

		while (ring.History.size() > 2 && ring.History[1].Time < time - ring.LagMax - 0.1)
			ring.History.pop_front();

		float lag = 0;

		if (ring.LagMax > 0 && ring.LagPeriod > 0)
		{
			const float cycle = static_cast<float>(std::fmod(time / ring.LagPeriod + ring.LagPhase, 1.0));
			lag = ring.LagMax * 0.5f * (1 - std::cos(cycle * 6.2831853f));
		}

		const Sample where = Trailing(ring, time, lag);

		// Concentric with the map's ring (its diameter is the line between the two), Scale times as wide.
		const Vector center = (where.Start + where.End) * 0.5f;
		const Vector half = (where.End - where.Start) * (0.5f * ring.Scale);
		Vector source = center - half;
		Vector target = center + half;

		// A ring between two points this moves; drawn as the map's own (TE_BEAMRING), not attached to the entities so
		// the engine leaves its ends where they're put. Made where it is, as the engine culls one made out of view.
		if (!ring.Beam)
		{
			ring.Beam = gEngfuncs.pEfxAPI->R_BeamPoints(source, target, ring.Sprite, KeepAlive, ring.Width, ring.Noise,
				ring.Brightness, ring.Speed, ring.FrameStart, ring.Framerate, ring.R, ring.G, ring.B);

			if (!ring.Beam)
				continue;

			gEngfuncs.Con_DPrintf("johnhalflife: spire ring strand made, %.0f units across\n", (target - source).Length());
			ring.Beam->type = TE_BEAMRING;
		}

		ring.Beam->source = source;
		ring.Beam->target = target;
		ring.Beam->delta = target - source;
		ring.Beam->brightness = ring.Brightness;
		ring.Beam->flags |= FBEAM_ISACTIVE;
	}
}

void JhlRings_Reset()
{
	g_Rings.clear();
	g_LastTime = 0;
}
