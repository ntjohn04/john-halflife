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
#include <vector>

#include "hud.h"
#include "beamdef.h"
#include "event_api.h"
#include "r_efx.h"
#include "dlight.h"
#include "parsemsg.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "jhl_explosions.h"
#include "jhl_gargantua.h"
#include "jhl_settings.h"

namespace
{
float Random(float low, float high)
{
	return gEngfuncs.pfnRandomFloat(low, high);
}

// Its flames: lit as the RPG rocket's motor is (RocketGlow), flickering a frame at a time, on each hand (its
// attachments 1 and 2, the flames' ends) and, dimmer, where each lands; kept while the flames last (they're sent every
// tenth of a second; a little longer than that, they've gone).
constexpr int FlameLightKey = 24576; // + its index * 4 + which.
constexpr float FlameHandRadius = 180;
constexpr float FlameHitRadius = 150;
constexpr float FlameFlicker = 0.2f;
constexpr color24 FlameLightColor{255, 150, 60};
constexpr float FlameKept = 0.2f;

// And a hot core down the middle of each (its flames are two layers of xbeam3, 24 units wide): the same sprite, the
// same length, from its hand to where it lands, much thinner and paler, faint (additive: it adds up fast), scrolling
// faster.
constexpr float FlameCoreWidth = 9;
constexpr float FlameCoreBrightness = 0.4f;
constexpr float FlameCoreSpeed = 3.5f;
constexpr float FlameCoreColor[3] = {1, 0.88f, 0.7f};

// Where they land: a few embers each time (rising and falling, orange), and now and then a little dark heat smoke.
constexpr int FlameEmbersMin = 1;
constexpr int FlameEmbersMax = 3;
constexpr float FlameSmokeChance = 0.4f;

struct Flames
{
	int Index = 0;
	float Last = -100;
	Vector Hands[2]; // Where its flames start (its hands), as the server last had them.
	Vector Hits[2];
	bool Hit[2]{};
	BEAM* Cores[2]{};
};

std::vector<Flames> g_Flames;

// Its stomp: each stretch it covered between the server's moves, gone along smoothly over that time; a light low over
// the floor at its head (red, as its glow), and the ground torn up every StompKickSpacing along it.
constexpr int StompLightKey = 32768; // + its index.
constexpr float StompLightRadius = 200;
constexpr float StompLightFlicker = 0.15f;
constexpr color24 StompLightColor{255, 70, 40};
constexpr float StompLightAbove = 20;
constexpr float StompKickSpacing = 36;

struct Stomp
{
	int Index = 0;
	Vector From, To;
	float Start = 0;
	float Duration = 0.1f;
	Vector LastKick;
	bool Kicked = false;
};

std::vector<Stomp> g_Stomps;

void Ember(const Vector& position, const Vector& normal)
{
	const int model = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/hotglow.spr");

	if (model == 0)
		return;

	Vector origin = position + normal * 4;
	Vector velocity = normal * Random(20, 50) + Vector{Random(-35, 35), Random(-35, 35), Random(40, 110)};

	if (TEMPENTITY* ember = gEngfuncs.pEfxAPI->R_TempSprite(origin, velocity, Random(0.04f, 0.07f), model, kRenderTransAdd,
			kRenderFxNone, 1, 1, FTENT_SLOWGRAVITY | FTENT_COLLIDEWORLD);
		ember)
	{
		JhlSprite_PlayOnce(ember, Random(0.5f, 0.9f), 0.5f);
	}
}

void HeatSmoke(const Vector& position, const Vector& normal)
{
	const int model = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/steam1.spr");

	if (model == 0)
		return;

	Vector origin = position + normal * 8;
	Vector velocity = normal * Random(10, 25) + Vector{Random(-10, 10), Random(-10, 10), Random(30, 50)};

	if (TEMPENTITY* smoke = gEngfuncs.pEfxAPI->R_TempSprite(origin, velocity, Random(0.5f, 0.8f), model, kRenderTransAlpha,
			kRenderFxNone, 0.2f, 1.8f, 0);
		smoke)
	{
		smoke->entity.curstate.rendercolor = {70, 65, 60};
		smoke->entity.angles.z = Random(0, 359);
		JhlSprite_PlayOnce(smoke, Random(1.4f, 1.9f), 0.6f);
	}
}

void FlameLight(int key, const Vector& origin, float radius)
{
	if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(key); light)
	{
		light->origin = origin;
		light->radius = radius * Random(1 - FlameFlicker, 1 + FlameFlicker);
		light->color = FlameLightColor;
		light->die = gEngfuncs.GetClientTime() + 0.05f;
		light->decay = 0;
	}
}
} // namespace

void JhlGargantua_ParseFlame(BufferReader& reader)
{
	const int index = reader.ReadShort();

	// Gone out: its lights and cores go now, as its flames do.
	if (reader.ReadByte() == 0)
	{
		if (auto it = std::find_if(g_Flames.begin(), g_Flames.end(), [&](const Flames& f)
				{ return f.Index == index; });
			it != g_Flames.end())
		{
			for (BEAM* core : it->Cores)
			{
				if (core)
				{
					core->die = 0;
					core->brightness = 0;
				}
			}

			g_Flames.erase(it);
		}

		return;
	}

	Flames flames{index, static_cast<float>(gEngfuncs.GetClientTime())};

	for (int i = 0; i < 2; ++i)
	{
		flames.Hit[i] = reader.ReadByte() != 0;
		flames.Hands[i] = reader.ReadCoordVector();
		flames.Hits[i] = reader.ReadCoordVector();
		const float x = reader.ReadChar() / 100.f;
		const float y = reader.ReadChar() / 100.f;
		const float z = reader.ReadChar() / 100.f;

		if (!JhlParticleSystemEnabled() || !flames.Hit[i])
			continue;

		const Vector normal{x, y, z};

		for (int e = gEngfuncs.pfnRandomLong(FlameEmbersMin, FlameEmbersMax); e > 0; --e)
			Ember(flames.Hits[i], normal);

		if (Random(0, 1) < FlameSmokeChance)
			HeatSmoke(flames.Hits[i], normal);
	}

	if (!JhlParticleSystemEnabled())
		return;

	if (auto it = std::find_if(g_Flames.begin(), g_Flames.end(), [&](const Flames& f)
			{ return f.Index == index; });
		it != g_Flames.end())
	{
		std::copy(std::begin(it->Cores), std::end(it->Cores), std::begin(flames.Cores));
		*it = flames;
	}
	else
	{
		g_Flames.push_back(flames);
	}
}

void JhlGargantua_Track(cl_entity_t* ent, const char* modelname)
{
	if (g_Flames.empty() || !JhlParticleSystemEnabled())
		return;

	const float now = gEngfuncs.GetClientTime();

	// Gone out (or a new map's clock); their cores die by themselves (kept on a frame at a time; by now they may have
	// been freed and used again, so they're left alone).
	std::erase_if(g_Flames, [&](const Flames& f)
		{ return now - f.Last > FlameKept || now < f.Last; });

	auto it = std::find_if(g_Flames.begin(), g_Flames.end(), [&](const Flames& f)
		{ return f.Index == ent->index; });

	if (it == g_Flames.end())
		return;

	for (int i = 0; i < 2; ++i)
	{
		// Its hands where the server last had them; moved onto them as drawn once it's drawn (JhlGargantua_Drawn).
		Vector hand = it->Hands[i];
		FlameLight(FlameLightKey + ent->index * 4 + i, hand, FlameHandRadius);

		if (it->Hit[i])
			FlameLight(FlameLightKey + ent->index * 4 + 2 + i, it->Hits[i], FlameHitRadius);

		BEAM*& core = it->Cores[i];

		if (!core)
		{
			if (const int model = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/xbeam3.spr"); model != 0)
			{
				core = gEngfuncs.pEfxAPI->R_BeamPoints(it->Hits[i], hand, model, 1, FlameCoreWidth, 0, FlameCoreBrightness,
					FlameCoreSpeed, 0, 0, FlameCoreColor[0], FlameCoreColor[1], FlameCoreColor[2]);
			}
		}

		if (core)
		{
			// As its flames are drawn: from where it lands to its hand, fading in from nothing there (BEAM_FSHADEIN).
			core->source = it->Hits[i];
			core->target = hand;
			core->flags |= FBEAM_SHADEIN;
			core->delta = core->target - core->source;
			core->die = now + 0.05f;
			core->flags |= FBEAM_ISACTIVE;
		}
	}
}

void JhlGargantua_Drawn(cl_entity_t* ent)
{
	if (g_Flames.empty() || !JhlParticleSystemEnabled())
		return;

	auto it = std::find_if(g_Flames.begin(), g_Flames.end(), [&](const Flames& f)
		{ return f.Index == ent->index; });

	if (it == g_Flames.end())
		return;

	for (int i = 0; i < 2; ++i)
	{
		// Its hands as just drawn (its flames' ends, attachments 1 and 2, which its flames follow, its arms blending up
		// into the attack over a moment while the server's are already up), if anywhere near where the server had them.
		const Vector& hand = ent->attachment[i + 1];

		if (hand == g_vecZero || (hand - it->Hands[i]).Length() > 200)
			continue;

		FlameLight(FlameLightKey + ent->index * 4 + i, hand, FlameHandRadius);

		if (BEAM* core = it->Cores[i]; core)
		{
			core->target = hand;
			core->delta = core->target - core->source;
		}
	}
}

void JhlGargantua_ParseStomp(BufferReader& reader)
{
	const int index = reader.ReadShort();
	const Vector from = reader.ReadCoordVector();
	const Vector to = reader.ReadCoordVector();
	const float duration = std::max(reader.ReadByte() / 100.f, 0.01f);

	if (!JhlParticleSystemEnabled())
		return;

	const float now = gEngfuncs.GetClientTime();

	auto it = std::find_if(g_Stomps.begin(), g_Stomps.end(), [&](const Stomp& s)
		{ return s.Index == index; });

	if (it == g_Stomps.end())
	{
		g_Stomps.push_back({index, from, to, now, duration});
		return;
	}

	// On from where its head had got to (it may not have reached the last stretch's end yet).
	it->From = from;
	it->To = to;
	it->Start = now;
	it->Duration = duration;
}

void JhlGargantua_Update(double time)
{
	const float now = static_cast<float>(time);

	std::erase_if(g_Stomps, [&](const Stomp& s)
		{ return now < s.Start || now - s.Start > s.Duration + 0.15f; });

	if (!JhlParticleSystemEnabled())
		return;

	for (Stomp& stomp : g_Stomps)
	{
		const float t = std::clamp((now - stomp.Start) / stomp.Duration, 0.f, 1.f);
		const Vector head = stomp.From + (stomp.To - stomp.From) * t;

		// Its head over the floor (it travels about 30 units up).
		pmtrace_t tr;
		gEngfuncs.pEventAPI->EV_PushPMStates();
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(head, head - Vector{0, 0, 256}, PM_WORLD_ONLY, -1, &tr);
		gEngfuncs.pEventAPI->EV_PopPMStates();
		const Vector floor = tr.fraction < 1 ? tr.endpos : head;

		if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(StompLightKey + stomp.Index); light)
		{
			light->origin = floor + Vector{0, 0, StompLightAbove};
			light->radius = StompLightRadius * Random(1 - StompLightFlicker, 1 + StompLightFlicker);
			light->color = StompLightColor;
			light->die = now + 0.05f;
			light->decay = 0;
		}

		// The ground torn up evenly along where it's gone.
		if (!stomp.Kicked)
		{
			stomp.LastKick = head;
			stomp.Kicked = true;
			continue;
		}

		Vector moved = head - stomp.LastKick;
		moved.z = 0;
		float distance = moved.Length();

		if (distance < StompKickSpacing)
			continue;

		const Vector direction = moved / distance;
		int kicks = 0;

		while (distance >= StompKickSpacing && kicks++ < 6)
		{
			stomp.LastKick = stomp.LastKick + direction * StompKickSpacing;
			distance -= StompKickSpacing;
			JhlExplosion_GroundKick(stomp.LastKick, direction);
		}

		// A hitch: on from where it is.
		if (distance >= StompKickSpacing)
			stomp.LastKick = head;
	}
}

void JhlGargantua_Reset()
{
	g_Flames.clear();
	g_Stomps.clear();
}
