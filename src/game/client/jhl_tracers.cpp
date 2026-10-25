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
	BEAM* Core; // Null if it has none.
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

void JhlTracer_Fire(const Vector& muzzle, const Vector& end, const JhlTracerStyle& style)
{
	if (style.Speed <= 0 || g_Tracers.size() >= MaxTracers)
		return;

	// Starting a random way down its path, never past half of it.
	const float pathLength = (end - muzzle).Length();

	if (pathLength < 1)
		return;

	const float offset = std::min(gEngfuncs.pfnRandomFloat(0, std::max(0.f, style.StartOffsetMax)), pathLength * 0.5f);
	const Vector start = muzzle + (end - muzzle) * (offset / pathLength);

	const Vector path = end - start;
	const float distance = path.Length();

	if (distance < 1)
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

	BEAM* core = nullptr;

	if (style.CoreLength > 0)
	{
		core = gEngfuncs.pEfxAPI->R_BeamPoints(from, to, sprite, 1, style.CoreWidth, 0, style.CoreBrightness, 0, 0, 0,
			style.CoreColor.x / 255, style.CoreColor.y / 255, style.CoreColor.z / 255);

		if (core)
			core->brightness = 0;
	}

	g_Tracers.push_back({beam, core, start, direction, distance, style, gEngfuncs.GetClientTime()});
}

namespace
{
// Puts a beam between two distances down a tracer's path, at a brightness while it has any length.
void PlaceBeam(BEAM* beam, const Tracer& tracer, float from, float to, float brightness, double time)
{
	beam->source = tracer.Start + tracer.Direction * from;
	beam->target = tracer.Start + tracer.Direction * to;
	beam->delta = beam->target - beam->source;
	beam->brightness = to - from >= 1 ? brightness : 0;
	beam->die = static_cast<float>(time) + 0.5f;
	beam->flags |= FBEAM_ISACTIVE;
}
} // namespace

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

		// Reached the end: gone (the engine frees them).
		if (tail >= tracer.Distance)
		{
			for (BEAM* beam : {tracer.Beam, tracer.Core})
			{
				if (beam)
				{
					beam->die = static_cast<float>(time);
					beam->brightness = 0;
				}
			}

			it = g_Tracers.erase(it);
			continue;
		}

		PlaceBeam(tracer.Beam, tracer, tail, head, tracer.Style.Brightness, time);

		// The core: centered on the streak, the same share of it whether it's whole or growing out or going in.
		if (tracer.Core)
		{
			const float half = (head - tail) * std::min(1.f, tracer.Style.CoreLength / std::max(1.f, tracer.Style.Length)) * 0.5f;
			const float middle = (head + tail) * 0.5f;
			PlaceBeam(tracer.Core, tracer, middle - half, middle + half, tracer.Style.CoreBrightness, time);
		}

		++it;
	}
}

void JhlTracers_Reset()
{
	g_Tracers.clear();
	g_LastTime = 0;
}
