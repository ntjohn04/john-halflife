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
#include "event_api.h"
#include "r_efx.h"
#include "jhl_tracers.h"

namespace
{
struct Tracer
{
	BEAM* Beam;
	Vector Start;
	Vector Direction;
	float Distance;
	JhlTracerStyle Style;
	double StartTime;
};

// A machine gun fires about 15 a second and each lives well under a second.
constexpr std::size_t MaxTracers = 64;

std::vector<Tracer> g_Tracers;
double g_LastTime = 0;
} // namespace

void JhlTracer_Fire(const Vector& start, const Vector& end, const JhlTracerStyle& style)
{
	const Vector path = end - start;
	const float distance = path.Length();

	if (distance < 1 || style.Speed <= 0 || g_Tracers.size() >= MaxTracers)
		return;

	const int sprite = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/laserbeam.spr");

	if (sprite == 0)
		return;

	const Vector direction = path / distance;

	// Made where it starts (the engine won't make a beam out of view, as for another player's shot behind you); the
	// update places it from the next frame.
	Vector from = start;
	Vector to = start + direction * std::min(style.Length, distance);
	BEAM* beam = gEngfuncs.pEfxAPI->R_BeamPoints(from, to, sprite,
		1, style.Width, 0, style.Brightness, 0, 0, 0, style.Color.x / 255, style.Color.y / 255, style.Color.z / 255);

	if (!beam)
		return;

	beam->brightness = 0;
	g_Tracers.push_back({beam, start, direction, distance, style, gEngfuncs.GetClientTime()});
}

void JhlTracers_Update(double time)
{
	// A new map or a loaded game restarts the clock (and frees the beams).
	if (time < g_LastTime)
		g_Tracers.clear();

	g_LastTime = time;

	for (auto it = g_Tracers.begin(); it != g_Tracers.end();)
	{
		Tracer& tracer = *it;
		const float flown = static_cast<float>(time - tracer.StartTime) * tracer.Style.Speed;
		const float head = std::min(flown, tracer.Distance);
		const float tail = std::clamp(flown - tracer.Style.Length, 0.f, tracer.Distance);

		// Reached the end: gone (the engine frees it).
		if (tail >= tracer.Distance)
		{
			tracer.Beam->die = static_cast<float>(time);
			tracer.Beam->brightness = 0;
			it = g_Tracers.erase(it);
			continue;
		}

		tracer.Beam->source = tracer.Start + tracer.Direction * tail;
		tracer.Beam->target = tracer.Start + tracer.Direction * head;
		tracer.Beam->delta = tracer.Beam->target - tracer.Beam->source;
		tracer.Beam->brightness = head - tail >= 1 ? tracer.Style.Brightness : 0;
		tracer.Beam->die = static_cast<float>(time) + 0.5f;
		tracer.Beam->flags |= FBEAM_ISACTIVE;
		++it;
	}
}

void JhlTracers_Reset()
{
	g_Tracers.clear();
	g_LastTime = 0;
}
