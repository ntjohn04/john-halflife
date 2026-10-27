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
#include <vector>

#include "hud.h"
#include "beamdef.h"
#include "com_model.h"
#include "event_api.h"
#include "r_efx.h"
#include "jhl_settings.h"
#include "jhl_grapple.h"

namespace
{
// A tongue is drawn in this many pieces (its sag and wiggle need them), from its player's attachment 1 (as the
// server's was, CGrapple::CreateEffect: tongue.spr, 16 wide in tenths, brightness 100, solid) to its tip.
constexpr int Pieces = 8;
constexpr float Width = 1.6f;
constexpr float Brightness = 100 / 255.f;

// Flying out it hangs a little slack, sagging at its middle by this much of its length, up to SagMax; once latched on
// it pulls taut, straight and this much thinner (over TautTime).
constexpr float Sag = 0.07f;
constexpr float SagMax = 14;
constexpr float TautWidth = 1.25f;
constexpr float TautTime = 0.08f;

// It's a little springy, only to look at (where its tip goes is the server's): each point along it keeps where it is
// when the line from the gun to the tip moves under it (as the player moves), most at its middle (none at its ends),
// and springs back to its shape (slack, or straight once latched) at this stiffness (radians a second) and damping (1
// none overshooting), the latched one stiffer and bouncier (it twangs taut). Moving while it's out bows it behind;
// it's bowed at most this much of its length, up to BendMax.
constexpr float FlyStiffness = 16;
constexpr float FlyDamping = 0.4f;
constexpr float TautStiffness = 60;
constexpr float TautDamping = 0.3f;
constexpr float BendLimit = 0.2f;
constexpr float BendMax = 40;
constexpr float StepTime = 1 / 200.f;

// When it lets go it reels back into its gun over this long, quickening, from at most this far out (so it's always
// seen to, at the same pace).
constexpr float RetractTime = 0.2f;
constexpr float RetractLength = 150;

// Its wiggle as it reels in, a measuring tape's (so it's plainly not the tongue in play any more): a wave this far to
// the side at its middle (at full length; less as it shortens, none at its ends), this many waves along it, running
// in this many times a second.
constexpr float WiggleSize = 4;
constexpr float WiggleWaves = 1.5f;
constexpr float WiggleSpeed = 4;

// A tongue's pieces: the first from its player's attachment (the engine keeps it there), the rest along it.
struct Chain
{
	int Owner = 0;
	BEAM* Beams[Pieces]{};

	// Where it meets the gun: where the engine has the first piece's start once it's drawn it, roughly its player's
	// eyes until then.
	Vector Mouth() const;

	// Placed along these points (made if need be: the engine won't make one out of view).
	void Place(const Vector (&points)[Pieces + 1], float width, float now);
	void Free();
};

struct Tip
{
	int Index;
	Vector Origin;
	float LastSeen;
	float LatchedAt; // 0 while flying.
	Chain Tongue;

	// Its points' springing, off the straight line from the gun to the tip, and where that line had them last frame.
	Vector Offsets[Pieces + 1]{};
	Vector Velocities[Pieces + 1]{};
	Vector LastLine[Pieces + 1]{};
	bool Drawn = false;
	float LastTime = 0;
};

struct Retraction
{
	Chain Tongue;
	Vector Direction;
	Vector Side;
	float Length;
	float Width;
	float Phase;
	float Start;
};

std::vector<Tip> g_Tips;
std::vector<Retraction> g_Retractions;
float g_LastTime = 0;

Vector Chain::Mouth() const
{
	if (Beams[0] && Beams[0]->source != g_vecZero)
		return Beams[0]->source;

	cl_entity_t* local = gEngfuncs.GetLocalPlayer();

	if (local && local->index == Owner)
	{
		Vector view;
		gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view);
		return local->origin + view;
	}

	cl_entity_t* player = gEngfuncs.GetEntityByIndex(Owner);
	return player ? player->origin + Vector{0, 0, 28} : g_vecZero;
}

void Chain::Place(const Vector (&points)[Pieces + 1], float width, float now)
{
	const int model = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/tongue.spr");

	for (int i = 0; i < Pieces; ++i)
	{
		BEAM*& beam = Beams[i];
		Vector from = points[i];
		Vector to = points[i + 1];

		if (!beam && model != 0)
		{
			beam = i == 0 ? gEngfuncs.pEfxAPI->R_BeamEntPoint(Owner | 0x1000, to, model, 1, width, 0, Brightness, 0, 0, 0, 1, 1, 1)
						  : gEngfuncs.pEfxAPI->R_BeamPoints(from, to, model, 1, width, 0, Brightness, 0, 0, 0, 1, 1, 1);

			if (beam)
				beam->flags |= FBEAM_SOLID;
		}

		if (!beam)
			continue;

		// The first's start is the engine's.
		if (i > 0)
			beam->source = from;

		beam->target = to;
		beam->delta = to - (i > 0 ? from : beam->source);
		beam->width = width;
		beam->die = now + 0.2f;
		beam->flags |= FBEAM_ISACTIVE;
	}
}

void Chain::Free()
{
	for (BEAM*& beam : Beams)
	{
		if (beam)
		{
			beam->die = 0;
			beam->brightness = 0;
			beam = nullptr;
		}
	}
}

void DrawTongue(Tip& tip, float now)
{
	const Vector mouth = tip.Tongue.Mouth();
	const Vector along = tip.Origin - mouth;
	const float length = along.Length();
	const Vector direction = length > 1 ? along / length : Vector{0, 0, 0};

	const bool latched = tip.LatchedAt != 0;
	const float taut = latched ? std::clamp((now - tip.LatchedAt) / TautTime, 0.f, 1.f) : 0;
	const float eased = taut * taut * (3 - 2 * taut);
	const float sag = latched ? 0 : std::min(length * Sag, SagMax);
	const float stiffness = latched ? TautStiffness : FlyStiffness;
	const float damping = latched ? TautDamping : FlyDamping;
	const float limit = std::min(length * BendLimit, BendMax);

	// Square to the line (springing along it doesn't bend it).
	const auto square = [&](const Vector& v)
	{ return v - direction * DotProduct(v, direction); };

	const float elapsed = tip.Drawn ? std::clamp(now - tip.LastTime, 0.f, 0.1f) : 0;
	const int steps = static_cast<int>(std::ceil(elapsed / StepTime));

	Vector points[Pieces + 1];

	for (int i = 0; i <= Pieces; ++i)
	{
		const float s = static_cast<float>(i) / Pieces;
		const float middle = std::sin(3.1415927f * s);
		const Vector line = mouth + along * s;

		Vector& offset = tip.Offsets[i];
		Vector& velocity = tip.Velocities[i];

		if (i == 0 || i == Pieces || length <= 1)
		{
			offset = velocity = g_vecZero;
		}
		else
		{
			// Where it was stays put as the line moves under it.
			if (tip.Drawn)
				offset = offset - (line - tip.LastLine[i]) * middle;

			offset = square(offset);
			velocity = square(velocity);

			const Vector rest = Vector{0, 0, -sag * middle};

			for (int step = 0; step < steps; ++step)
			{
				const float dt = elapsed / steps;
				velocity = velocity + ((rest - offset) * (stiffness * stiffness) - velocity * (2 * damping * stiffness)) * dt;
				offset = offset + velocity * dt;
			}

			if (const float bend = offset.Length(); bend > limit)
				offset = offset * (limit / bend);
		}

		tip.LastLine[i] = line;
		points[i] = line + offset;
	}

	tip.Drawn = true;
	tip.LastTime = now;

	tip.Tongue.Place(points, Width + (TautWidth - Width) * eased, now);
}

// Its tongue gone with it: one reels back in from where it was, starting this frame.
void Retract(Tip& tip, float now)
{
	const Vector mouth = tip.Tongue.Mouth();
	Vector direction = tip.Origin - mouth;
	const float length = std::min(direction.Length(), RetractLength);
	const float width = tip.LatchedAt != 0 ? TautWidth : Width;

	if (length < 1)
	{
		tip.Tongue.Free();
		return;
	}

	direction = direction.Normalize();

	// Its wiggle side to side as seen along it (level; any way, for one straight up or down).
	Vector side = CrossProduct(direction, Vector{0, 0, 1});
	side = side.Length() > 0.1f ? side.Normalize() : Vector{1, 0, 0};

	if (gEngfuncs.pfnRandomLong(0, 1) != 0)
		side = side * -1;

	g_Retractions.push_back({tip.Tongue, direction, side, length, width, gEngfuncs.pfnRandomFloat(0, 6.2831853f), now});
	tip.Tongue = {};
}

void DrawRetraction(Retraction& retraction, float now)
{
	const float f = (now - retraction.Start) / RetractTime;
	const Vector mouth = retraction.Tongue.Mouth();
	const float length = retraction.Length * (1 - f * f);
	const float wiggle = WiggleSize * (length / RetractLength);

	Vector points[Pieces + 1];

	for (int i = 0; i <= Pieces; ++i)
	{
		const float s = static_cast<float>(i) / Pieces;
		const float wave = std::sin(3.1415927f * s) * std::sin(6.2831853f * (WiggleWaves * s + WiggleSpeed * (now - retraction.Start)) + retraction.Phase);
		points[i] = mouth + retraction.Direction * (length * s) + retraction.Side * (wiggle * wave);
	}

	retraction.Tongue.Place(points, retraction.Width, now);
}
} // namespace

void JhlGrapple_Track(cl_entity_t* entity, const char* modelname)
{
	// The grapple's tip has the shock bolt's model; a shock bolt is tagged renderamt 1 (CShockBeam::Spawn), a latched
	// tip 2 (CGrappleTip::TongueTouch).
	if (!JhlParticleSystemEnabled() || !modelname || 0 != std::strcmp(modelname, "models/shock_effect.mdl") ||
		entity->curstate.renderamt == 1 || entity->curstate.owner <= 0)
		return;

	const float now = gEngfuncs.GetClientTime();

	Tip* tip = nullptr;

	for (auto& candidate : g_Tips)
	{
		if (candidate.Index == entity->index)
			tip = &candidate;
	}

	if (!tip)
	{
		g_Tips.push_back({entity->index, entity->origin, now, 0, {}});
		tip = &g_Tips.back();
	}

	tip->Tongue.Owner = entity->curstate.owner;
	tip->Origin = entity->origin;
	tip->LastSeen = now;

	if (entity->curstate.renderamt == 2 && tip->LatchedAt == 0)
		tip->LatchedAt = now;
}

void JhlGrapple_Update(double time)
{
	const float now = static_cast<float>(time);

	// A new map restarts the clock (and frees the beams).
	if (now < g_LastTime)
	{
		g_Tips.clear();
		g_Retractions.clear();
	}

	g_LastTime = now;

	// A tip not added this frame is gone: its tongue reels back in, starting this frame.
	for (auto it = g_Tips.begin(); it != g_Tips.end();)
	{
		if (it->LastSeen >= now)
		{
			DrawTongue(*it, now);
			++it;
			continue;
		}

		Retract(*it, now);
		it = g_Tips.erase(it);
	}

	for (auto it = g_Retractions.begin(); it != g_Retractions.end();)
	{
		if (now - it->Start >= RetractTime)
		{
			it->Tongue.Free();
			it = g_Retractions.erase(it);
			continue;
		}

		DrawRetraction(*it, now);
		++it;
	}
}

void JhlGrapple_Reset()
{
	g_Tips.clear();
	g_Retractions.clear();
	g_LastTime = 0;
}
