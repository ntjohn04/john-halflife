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
#include "pm_defs.h"
#include "pmtrace.h"
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
	bool Hit = false; // Its head has reached the end (and thrown its speck).
};

// A tracer's burning speck (ThrowSpeck): its path, worked out each frame from where and how it was thrown, so its two
// sprites (outline and core) stay exactly together. vuser1 where, vuser2 how fast, fuser1 when, fuser2 how long it
// lives, fuser3 where in its twirl it starts.
constexpr float SpeckGravity = 160;		// Units/s², about a fifth of the world's: it drifts down.
constexpr float SpeckTwirlRadius = 3;
constexpr float SpeckTwirlSpeed = 13; // Radians a second: about 2 turns.
constexpr float SpeckFade = 0.5f;	  // Of its life, at the end.

void SpeckThink(TEMPENTITY* ent, float frametime, float currenttime)
{
	auto& state = ent->entity.curstate;
	const float t = currenttime - state.fuser1;
	const float life = std::max(0.01f, state.fuser2);

	const float angle = state.fuser3 + t * SpeckTwirlSpeed;
	const Vector twirl{std::cos(angle) * SpeckTwirlRadius, std::sin(angle) * SpeckTwirlRadius, 0};

	ent->entity.origin = state.vuser1 + state.vuser2 * t - Vector{0, 0, 0.5f * SpeckGravity * t * t} + twirl;

	const float left = std::clamp((1 - t / life) / SpeckFade, 0.f, 1.f);
	state.renderamt = static_cast<int>(ent->entity.baseline.renderamt * left);
}

void ThrowSpeck(const Vector& hit, const Vector& direction, const JhlTracerStyle& style, double time)
{
	const int sprite = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/hotglow.spr");

	if (sprite == 0)
		return;

	// The surface's facing, from a short trace across where it hit: the world and brush entities (walls, doors,
	// crates), not models. A tracer that hit a creature buries itself; its blood is what shows there.
	pmtrace_t tr;
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	Vector from = hit - direction * 8;
	Vector to = hit + direction * 8;
	gEngfuncs.pEventAPI->EV_PlayerTrace(from, to, PM_STUDIO_IGNORE, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.fraction >= 1 || tr.startsolid)
		return;

	const Vector normal = tr.plane.normal;

	// Off the surface, up, and to one side along it, keeping a little of the tracer's way along it (a glancing skip).
	Vector side = CrossProduct(normal, Vector{0, 0, 1});

	if (side.Length() < 0.1f)
	{
		const float a = gEngfuncs.pfnRandomFloat(0, 6.2831853f);
		side = {std::cos(a), std::sin(a), 0};
	}

	side = side.Normalize() * (gEngfuncs.pfnRandomLong(0, 1) != 0 ? 1.f : -1.f);
	const Vector along = direction - normal * DotProduct(direction, normal);

	const Vector velocity = normal * gEngfuncs.pfnRandomFloat(40, 80) + side * gEngfuncs.pfnRandomFloat(30, 70) +
							Vector{0, 0, gEngfuncs.pfnRandomFloat(80, 140)} + along * 30;

	const Vector origin = hit + normal * 2;
	const float life = gEngfuncs.pfnRandomFloat(1, 1.6f);
	const float phase = gEngfuncs.pfnRandomFloat(0, 6.2831853f);

	// The red outline around a white-hot core, as the tracer.
	const struct
	{
		Vector Color;
		float Scale;
	} layers[] = {{style.Color, 0.08f}, {style.CoreColor, 0.04f}};

	for (const auto& layer : layers)
	{
		Vector position = origin;
		Vector still{0, 0, 0};
		TEMPENTITY* speck = gEngfuncs.pEfxAPI->R_TempSprite(position, still, layer.Scale, sprite, kRenderTransAdd,
			kRenderFxNone, 1, life, 0);

		if (!speck)
			continue;

		speck->flags |= FTENT_CLIENTCUSTOM;
		speck->callback = SpeckThink;
		speck->entity.curstate.rendercolor = {static_cast<byte>(layer.Color.x), static_cast<byte>(layer.Color.y),
			static_cast<byte>(layer.Color.z)};
		speck->entity.curstate.vuser1 = origin;
		speck->entity.curstate.vuser2 = velocity;
		speck->entity.curstate.fuser1 = static_cast<float>(time);
		speck->entity.curstate.fuser2 = life;
		speck->entity.curstate.fuser3 = phase;
	}
}

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

	const float most = std::max(0.f, style.StartOffsetMax);
	const float offset = std::min(gEngfuncs.pfnRandomFloat(std::clamp(style.StartOffsetMin, 0.f, most), most), pathLength * 0.5f);
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

		if (!tracer.Hit && flown >= tracer.Distance)
		{
			tracer.Hit = true;

			if (tracer.Style.ImpactSpeck)
				ThrowSpeck(tracer.Start + tracer.Direction * tracer.Distance, tracer.Direction, tracer.Style, time);
		}

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
