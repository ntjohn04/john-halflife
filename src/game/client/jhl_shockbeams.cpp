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
#include "jhl_settings.h"
#include "jhl_shockbeams.h"

namespace
{
// Each strand: this many pieces, each this long in time (the wake is their product: about 300 units at its 2000 a
// second), for up to MaxWakes bolts at once, to keep within the engine's beams; every bolt (up to MaxBolts) has its
// glow and spark.
constexpr int Pieces = 8;
constexpr float PieceTime = 0.02f;
constexpr std::size_t MaxBolts = 16;
constexpr std::size_t MaxWakes = 4;

// Its glow and spark circle its path this far out, this many turns a second.
constexpr float Radius = 10;
constexpr float TurnsPerSecond = 5;

// The glow (vanilla's, flare3) and the spark (the gluon gun's flare, XSpark1: cyan-blue by itself, sparkling).
constexpr const char* GlowSprite = "sprites/flare3.spr";
constexpr float GlowScale = 0.35f;
constexpr const char* SparkSprite = "sprites/XSpark1.spr";
constexpr float SparkScale = 0.45f;

// A bolt not seen for this long is gone: its wake fades out over what's left.
constexpr float LostAfter = 0.05f;

struct Strand
{
	Vector Color;
	float Width;
	float Brightness;
	float Noise; // The engine's random noise, rerolled every frame: jagged, crackling.
};

// The smooth purplish one along its glow's path, the jagged cyan one along its spark's.
const Strand Strands[2] = {{{170, 110, 220}, 1.6f, 0.35f, 0}, {{0, 253, 253}, 0.8f, 0.6f, 0.15f}};

struct Sample
{
	float Time;
	Vector Points[2];
};

struct Bolt
{
	int Index;
	float LastSeen;
	float Phase;
	std::deque<Sample> History;
	BEAM* Beams[2][Pieces]{};
	TEMPENTITY* Sprites[2]{}; // Its glow and its spark.
};

std::vector<Bolt> g_Bolts;
float g_LastTime = 0;

bool At(const Bolt& bolt, float when, int strand, Vector& out)
{
	const auto& history = bolt.History;

	if (history.empty() || when < history.front().Time)
		return false;

	for (std::size_t i = history.size() - 1; i > 0; --i)
	{
		const Sample& newer = history[i];
		const Sample& older = history[i - 1];

		if (older.Time > when)
			continue;

		const float f = newer.Time > older.Time ? std::min(1.f, (when - older.Time) / (newer.Time - older.Time)) : 1;
		out = older.Points[strand] + (newer.Points[strand] - older.Points[strand]) * f;
		return true;
	}

	out = history.front().Points[strand];
	return true;
}

// A sprite of it, kept on it a frame at a time (Track).
TEMPENTITY* MakeSprite(const char* name, const Vector& origin, float scale, int renderfx, bool animated)
{
	const int model = gEngfuncs.pEventAPI->EV_FindModelIndex(name);

	if (model == 0)
		return nullptr;

	Vector position = origin;
	Vector still{0, 0, 0};
	TEMPENTITY* sprite = gEngfuncs.pEfxAPI->R_TempSprite(position, still, scale, model, kRenderTransAdd, renderfx, 1, 1,
		animated ? FTENT_SPRCYCLE : 0);

	if (sprite && animated)
		sprite->entity.curstate.framerate = 20;

	return sprite;
}

void Kill(BEAM*& beam)
{
	if (beam)
	{
		beam->die = 0;
		beam->brightness = 0;
		beam = nullptr;
	}
}
} // namespace

void JhlShockBeams_Track(cl_entity_t* entity, const char* modelname)
{
	if (!JhlParticleSystemEnabled() || !modelname || 0 != std::strcmp(modelname, "models/shock_effect.mdl"))
		return;

	const float now = gEngfuncs.GetClientTime();

	Bolt* bolt = nullptr;

	for (auto& candidate : g_Bolts)
	{
		if (candidate.Index == entity->index)
			bolt = &candidate;
	}

	if (!bolt)
	{
		if (g_Bolts.size() >= MaxBolts)
			return;

		g_Bolts.push_back({entity->index, now, gEngfuncs.pfnRandomFloat(0, 6.2831853f), {}, {}, {}});
		bolt = &g_Bolts.back();
	}

	bolt->LastSeen = now;

	// Its glow and spark, circling its path (its angles' way; a studio model's pitch is the other way round).
	Vector forward, right, up;
	AngleVectors(Vector{-entity->angles.x, entity->angles.y, 0}, forward, right, up);
	const float angle = bolt->Phase + 6.2831853f * TurnsPerSecond * now;
	const Vector offset = (right * std::cos(angle) + up * std::sin(angle)) * Radius;
	const Vector points[2] = {entity->origin + offset, entity->origin - offset};

	if (!bolt->Sprites[0])
		bolt->Sprites[0] = MakeSprite(GlowSprite, points[0], GlowScale, kRenderFxDistort, false);

	if (!bolt->Sprites[1])
		bolt->Sprites[1] = MakeSprite(SparkSprite, points[1], SparkScale, kRenderFxNone, true);

	for (int i = 0; i < 2; ++i)
	{
		if (TEMPENTITY* sprite = bolt->Sprites[i]; sprite)
		{
			sprite->entity.origin = points[i];
			sprite->die = now + 0.1f;
		}
	}

	if (bolt->History.empty() || bolt->History.back().Time < now)
		bolt->History.push_back({now, {points[0], points[1]}});
}

void JhlShockBeams_Update(double time)
{
	const float now = static_cast<float>(time);

	// A new map restarts the clock (and frees the beams).
	if (now < g_LastTime)
		g_Bolts.clear();

	g_LastTime = now;

	const int sprite = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/laserbeam.spr");
	std::size_t wakes = 0;

	for (auto it = g_Bolts.begin(); it != g_Bolts.end();)
	{
		Bolt& bolt = *it;
		const bool wake = wakes++ < MaxWakes;

		while (!bolt.History.empty() && bolt.History.front().Time < now - Pieces * PieceTime - 0.05f)
			bolt.History.pop_front();

		// Gone: its glow and spark go with it.
		if (now - bolt.LastSeen > LostAfter)
		{
			for (TEMPENTITY*& sprite : bolt.Sprites)
			{
				if (sprite)
				{
					sprite->die = now;
					sprite = nullptr;
				}
			}
		}

		// Gone, and its wake faded out.
		if (bolt.History.size() < 2 && now - bolt.LastSeen > LostAfter)
		{
			for (auto& strand : bolt.Beams)
			{
				for (BEAM*& beam : strand)
					Kill(beam);
			}

			it = g_Bolts.erase(it);
			continue;
		}

		const float head = std::min(now, bolt.LastSeen);
		const float gone = now - bolt.LastSeen > LostAfter ? std::clamp(1 - (now - bolt.LastSeen) / (Pieces * PieceTime), 0.f, 1.f) : 1;

		for (int s = 0; s < 2; ++s)
		{
			const Strand& strand = Strands[s];

			for (int i = 0; i < Pieces; ++i)
			{
				Vector from, to;
				BEAM*& beam = bolt.Beams[s][i];

				if (!wake || sprite == 0 || !At(bolt, head - (i + 1) * PieceTime, s, from) || !At(bolt, head - i * PieceTime, s, to) ||
					(to - from).Length() < 0.5f)
				{
					if (beam)
						beam->brightness = 0;

					continue;
				}

				// Fading toward the tail, and out once it's gone.
				const float brightness = strand.Brightness * (1 - static_cast<float>(i) / Pieces) * gone;

				if (!beam)
				{
					beam = gEngfuncs.pEfxAPI->R_BeamPoints(from, to, sprite, 1, strand.Width, strand.Noise, brightness, 0, 0, 0,
						strand.Color.x / 255, strand.Color.y / 255, strand.Color.z / 255);

					if (!beam)
						continue;
				}

				beam->source = from;
				beam->target = to;
				beam->delta = to - from;
				beam->brightness = brightness;
				beam->die = now + 0.2f;
				beam->flags |= FBEAM_ISACTIVE;
			}
		}

		++it;
	}
}

void JhlShockBeams_Reset()
{
	g_Bolts.clear();
	g_LastTime = 0;
}
