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

#include "hud.h"
#include "beamdef.h"
#include "com_model.h"
#include "event_api.h"
#include "dlight.h"
#include "r_efx.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "jhl_egon.h"
#include "jhl_explosions.h"
#include "jhl_settings.h"
#include "view.h"
#include "Exports.h"

namespace
{
constexpr int Segments = 8;

// The curve's bow is at most this share of the beam's length.
constexpr float MaxBow = 0.3f;

constexpr const char* SpeckSprite = "sprites/XSpark1.spr";

// Scorches: no closer together than this, nor more often.
constexpr float ScorchSpacing = 14;
constexpr float ScorchInterval = 0.12f;

constexpr int PlayerLightKey = 8192 + 4096;
constexpr int ImpactLightKey = PlayerLightKey + 1;
const color24 PlayerLightColor{60, 110, 150};
const color24 ImpactLightColor{100, 180, 230};
constexpr float PlayerLightRadius = 160;
constexpr float ImpactLightRadius = 140;
constexpr float LightFade = 0.25f;

// Sparks leaking from the barrel, one every so often.
constexpr float VentIntervalMin = 0.1f;
constexpr float VentIntervalMax = 0.3f;

struct Layer
{
	BEAM* Source = nullptr;
	float Brightness = 0;
	BEAM* Pieces[Segments]{};
};

constexpr float Bend = 0.25f;

constexpr int SpiralPieces = 2;

Layer g_Layers[2];
Vector g_Lagged;
Vector g_LastHit;
Vector g_LastFeet;
Vector g_LastImpactLight;
bool g_Active = false;
float g_LastTime = 0;
Vector g_LastScorch;
float g_NextScorch = 0;
float g_NextVent = 0;

float Random(float low, float high)
{
	return gEngfuncs.pfnRandomFloat(low, high);
}

// Kills a layer's pieces (the engine frees them).
void ClearLayer(Layer& layer)
{
	for (BEAM*& piece : layer.Pieces)
	{
		if (piece)
		{
			piece->die = 0;
			piece->brightness = 0;
			piece = nullptr;
		}
	}

	layer.Source = nullptr;
}

Vector Bezier(const Vector& a, const Vector& b, const Vector& c, float t)
{
	const float u = 1 - t;
	return a * (u * u) + b * (2 * u * t) + c * (t * t);
}

void DrawLayer(Layer& layer, BEAM* source, const Vector& start, const Vector& control, const Vector& end, float now)
{
	if (layer.Source != source)
	{
		ClearLayer(layer);
		layer.Source = source;
		layer.Brightness = source->brightness;
	}

	source->brightness = 0;

	const bool spiral = (source->flags & FBEAM_SINENOISE) != 0;
	const int pieces = spiral ? SpiralPieces : Segments;

	for (int i = pieces; i < Segments; ++i)
	{
		if (BEAM*& unused = layer.Pieces[i]; unused)
		{
			unused->die = 0;
			unused->brightness = 0;
			unused = nullptr;
		}
	}

	for (int i = 0; i < pieces; ++i)
	{
		Vector from = Bezier(start, control, end, static_cast<float>(i) / pieces);
		Vector to = Bezier(start, control, end, static_cast<float>(i + 1) / pieces);

		BEAM*& piece = layer.Pieces[i];

		if (!piece)
		{
			piece = gEngfuncs.pEfxAPI->R_BeamPoints(from, to, source->modelIndex, 1, source->width, source->amplitude,
				layer.Brightness, source->speed, 0, source->frameRate, source->r, source->g, source->b);

			if (!piece)
				continue;

			piece->flags |= source->flags & FBEAM_SINENOISE;
		}

		piece->source = from;
		piece->target = to;
		piece->delta = to - from;
		piece->brightness = layer.Brightness;
		piece->die = now + 0.1f;
		piece->flags |= FBEAM_ISACTIVE;
	}
}

void Speck(const Vector& origin, const Vector& velocity, float scale, float life)
{
	const int sprite = gEngfuncs.pEventAPI->EV_FindModelIndex(SpeckSprite);

	if (sprite == 0)
		return;

	Vector position = origin;
	Vector moving = velocity;
	TEMPENTITY* speck = gEngfuncs.pEfxAPI->R_TempSprite(position, moving, scale, sprite, kRenderTransAdd, kRenderFxNone, 1,
		life, FTENT_SLOWGRAVITY | FTENT_COLLIDEWORLD);

	if (!speck)
		return;

	speck->entity.angles.z = Random(0, 359);
	JhlSprite_PlayOnce(speck, life, 0.6f);
}

void Scorch(const pmtrace_t& hit, float now)
{
	if (hit.fraction >= 1 || now < g_NextScorch || (hit.endpos - g_LastScorch).Length() < ScorchSpacing)
		return;

	physent_t* entity = gEngfuncs.pEventAPI->EV_GetPhysent(hit.ent);

	if (!entity || !(entity->solid == SOLID_BSP || entity->movetype == MOVETYPE_PUSHSTEP))
		return;

	static const char* const Scorches[] = {"{smscorch1", "{smscorch2", "{smscorch3"};
	const int decal = gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(Scorches[gEngfuncs.pfnRandomLong(0, 2)]);
	pmtrace_t trace = hit;
	gEngfuncs.pEfxAPI->R_DecalShoot(gEngfuncs.pEfxAPI->Draw_DecalIndex(decal), gEngfuncs.pEventAPI->EV_IndexFromTrace(&trace), 0,
		&trace.endpos.x, 0);

	g_LastScorch = hit.endpos;
	g_NextScorch = now + ScorchInterval;
}

void Light(int key, const Vector& origin, float radius, const color24& color, float life, float decay)
{
	if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(key); light)
	{
		light->origin = origin;
		light->radius = radius;
		light->color = color;
		light->die = gEngfuncs.GetClientTime() + life;
		light->decay = decay;
	}
}

// Where the player's feet are, a little above them.
Vector Feet()
{
	cl_entity_t* player = gEngfuncs.GetLocalPlayer();
	return player ? player->origin - Vector{0, 0, 24} : g_LastFeet;
}

void Vent(const Vector& muzzle, float now)
{
	if (now < g_NextVent)
		return;

	g_NextVent = now + Random(VentIntervalMin, VentIntervalMax);

	Vector forward, right, up;
	AngleVectors(gHUD.m_vecAngles, forward, right, up);

	const Vector velocity = right * Random(-90, 90) + up * Random(20, 90) - forward * Random(0, 40) + v_sim_vel;
	Speck(muzzle, velocity, 0.06f, Random(0.4f, 0.7f));
}
} // namespace

void JhlEgon_Update(BEAM* beam, BEAM* beam2, const pmtrace_t& hit)
{
	const float now = gEngfuncs.GetClientTime();

	if (!JhlParticleSystemEnabled() || !beam)
	{
		// Turned off while firing: its engine beams show again.
		for (Layer& layer : g_Layers)
		{
			if (layer.Source && (layer.Source == beam || layer.Source == beam2))
				layer.Source->brightness = layer.Brightness;
		}

		JhlEgon_Reset();
		return;
	}

	Vector start = beam->source;

	if (cl_entity_t* viewModel = gEngfuncs.GetViewModel();
		0 == CL_IsThirdPerson() && viewModel && viewModel->attachment[0] != g_vecZero)
		start = viewModel->attachment[0];

	if (start == g_vecZero)
		return;
	const Vector end = hit.endpos;

	if (!g_Active || now < g_LastTime)
	{
		g_Lagged = end;
		g_LastImpactLight = end;
		g_LastScorch = Vector{0, 0, 0};
		g_NextScorch = 0;
		g_NextVent = now + Random(VentIntervalMin, VentIntervalMax);
		g_LastTime = now;
	}

	const float frametime = std::max(0.f, now - g_LastTime);
	g_Active = true;
	g_LastTime = now;
	g_LastHit = end;

	g_Lagged = g_Lagged + (end - g_Lagged) * (1 - std::exp(-frametime / Bend));

	const Vector middle = (start + end) * 0.5f;
	const float length = (end - start).Length();
	Vector bow = (start + g_Lagged) * 0.5f - middle;

	if (bow.Length() > length * MaxBow)
		bow = bow.Normalize() * (length * MaxBow);

	const Vector control = middle + bow;

	DrawLayer(g_Layers[0], beam, start, control, end, now);

	if (beam2)
		DrawLayer(g_Layers[1], beam2, start, control, end, now);

	Scorch(hit, now);
	Vent(start, now);

	g_LastFeet = Feet();
	Light(PlayerLightKey, g_LastFeet, PlayerLightRadius, PlayerLightColor, 0.1f, 0);

	if (hit.fraction < 1)
	{
		g_LastImpactLight = hit.endpos + Vector{hit.plane.normal} * 8;
		Light(ImpactLightKey, g_LastImpactLight, ImpactLightRadius * Random(0.9f, 1.1f), ImpactLightColor, 0.1f, 0);
	}
}

void JhlEgon_Stop()
{
	if (!g_Active)
		return;

	JhlEgon_Reset();

	if (!JhlParticleSystemEnabled())
		return;

	// The lights fade out quickly.
	Light(PlayerLightKey, g_LastFeet, PlayerLightRadius, PlayerLightColor, LightFade, PlayerLightRadius / LightFade);
	Light(ImpactLightKey, g_LastImpactLight, ImpactLightRadius, ImpactLightColor, LightFade, ImpactLightRadius / LightFade);

	for (int i = 0, count = gEngfuncs.pfnRandomLong(4, 6); i < count; ++i)
	{
		const Vector origin = g_LastHit + Vector{Random(-3, 3), Random(-3, 3), Random(-3, 3)};
		const Vector velocity{Random(-50, 50), Random(-50, 50), Random(10, 70)};
		Speck(origin, velocity, 0.1f, Random(1, 1.5f));
	}
}

void JhlEgon_Reset()
{
	for (Layer& layer : g_Layers)
		ClearLayer(layer);

	g_Active = false;
}
