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

#include <vector>

#include "hud.h"
#include "beamdef.h"
#include "com_model.h"
#include "r_efx.h"
#include "parsemsg.h"
#include "jhl_rings.h"

namespace
{
struct SpireRing
{
	BEAM* Beam;
	int StartEntity;
	int EndEntity;
	float Scale;
	float Brightness;
};

// c4a1 has three, of3a1b the same; a few to spare.
constexpr std::size_t MaxRings = 16;

// The strand is kept alive this far ahead each frame, so it dies by itself soon after it's no longer tracked.
constexpr float KeepAlive = 1;

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
	const float scale = reader.ReadByte() * 0.1f;

	if (g_Rings.size() >= MaxRings)
		return;

	// A ring between two points the update moves; drawn as the map's own (TE_BEAMRING), not attached to the entities
	// so the engine leaves its ends where they're put.
	BEAM* beam = gEngfuncs.pEfxAPI->R_BeamPoints(Vector{0, 0, 0}, Vector{0, 0, 1}, sprite, KeepAlive, width, noise,
		brightness, speed, static_cast<int>(frameStart), framerate, r, g, b);

	if (!beam)
		return;

	beam->type = TE_BEAMRING;
	beam->brightness = 0; // Until the update has placed it.
	g_Rings.push_back({beam, startEntity, endEntity, scale, brightness});
}

void JhlRings_Update(double time)
{
	// A new map or a loaded game restarts the clock (and frees the beams).
	if (time < g_LastTime)
		g_Rings.clear();

	g_LastTime = time;

	for (auto& ring : g_Rings)
	{
		ring.Beam->die = static_cast<float>(time) + KeepAlive;

		cl_entity_t* start = ActiveEntity(ring.StartEntity);
		cl_entity_t* end = ActiveEntity(ring.EndEntity);

		if (!start || !end)
		{
			ring.Beam->brightness = 0;
			continue;
		}

		// Concentric with the map's ring (its diameter is the line between the two), Scale times as wide.
		const Vector center = (start->origin + end->origin) * 0.5f;
		const Vector half = (end->origin - start->origin) * (0.5f * ring.Scale);

		ring.Beam->source = center - half;
		ring.Beam->target = center + half;
		ring.Beam->delta = ring.Beam->target - ring.Beam->source;
		ring.Beam->brightness = ring.Brightness;
	}
}

void JhlRings_Reset()
{
	g_Rings.clear();
	g_LastTime = 0;
}
