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
#include "r_efx.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "jhl_egon.h"
#include "jhl_explosions.h"
#include "jhl_settings.h"
#include "Exports.h"

namespace
{
// Each beam is drawn as this many straight pieces along a curve.
constexpr int Segments = 8;

// The curve's bow is at most this share of the beam's length.
constexpr float MaxBow = 0.3f;

// The specks: the gun's own impact flare (XSpark1, cyan-blue like the beam; additive sprites aren't tinted), 32 px.
constexpr const char* SpeckSprite = "sprites/XSpark1.spr";

// Scorches: no closer together than this, nor more often.
constexpr float ScorchSpacing = 14;
constexpr float ScorchInterval = 0.12f;

// Sparks leaking from the barrel, one every so often.
constexpr float VentIntervalMin = 0.1f;
constexpr float VentIntervalMax = 0.3f;

struct Layer
{
	BEAM* Source = nullptr; // The engine's beam it's drawn for.
	float Brightness = 0;	// The engine beam's own, before it's hidden.
	BEAM* Pieces[Segments]{};
};

cvar_t* g_Bend = nullptr;
cvar_t* g_SpiralPieces = nullptr;

Layer g_Layers[2];
Vector g_Lagged; // Where the hit point was a moment ago (the curve's pull).
Vector g_LastHit;
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

	// The engine's beam stays (its updates and lifetime are the game's), unseen.
	source->brightness = 0;

	// Each piece draws the engine's noise over its own length (the spiral's turns restart on each), so the spiral layer
	// uses fewer, longer pieces (jhl_egon_spiralpieces) than the smooth one.
	const bool spiral = (source->flags & FBEAM_SINENOISE) != 0;
	const int pieces = spiral ? std::clamp(g_SpiralPieces ? static_cast<int>(g_SpiralPieces->value) : 2, 1, Segments) : Segments;

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

// A blue speck of it, falling slowly onto what's below, sparkling (its frames once over its life) and fading.
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

// A small scorch where it's burning a wall (not creatures), spaced out so they don't pile up.
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

// Now and then a spark of it leaking out of the barrel, flung out to the side.
void Vent(const Vector& muzzle, float now)
{
	if (now < g_NextVent)
		return;

	g_NextVent = now + Random(VentIntervalMin, VentIntervalMax);

	Vector forward, right, up;
	AngleVectors(gHUD.m_vecAngles, forward, right, up);

	const Vector velocity = right * Random(-90, 90) + up * Random(20, 90) - forward * Random(0, 40);
	Speck(muzzle, velocity, 0.06f, Random(0.4f, 0.7f));
}
} // namespace

void JhlEgon_Initialize()
{
	// How long (seconds) the beam's pull lags behind where it hits: 0 straight, more a stronger bend.
	g_Bend = gEngfuncs.pfnRegisterVariable("jhl_egon_bend", "0.08", FCVAR_ARCHIVE);

	// How many pieces its spiral is drawn in along the bend (1-8): each restarts the spiral's turns.
	g_SpiralPieces = gEngfuncs.pfnRegisterVariable("jhl_egon_spiralpieces", "2", FCVAR_ARCHIVE);
}

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

	// It starts on the gun, where the engine starts it: in first person its view model's muzzle (the beam's own start
	// is only set as it's drawn, so not yet on the first frame); in third person the engine's start, on the player
	// model's gun (the view model isn't drawn, so its muzzle stays where it last was).
	Vector start = beam->source;

	if (cl_entity_t* viewModel = gEngfuncs.GetViewModel();
		0 == CL_IsThirdPerson() && viewModel && viewModel->attachment[0] != g_vecZero)
		start = viewModel->attachment[0];

	if (start == g_vecZero)
		return;
	const Vector end = hit.endpos;

	// A new beam, or a restarted clock: the pull starts on the hit point.
	if (!g_Active || now < g_LastTime)
	{
		g_Lagged = end;
		g_LastScorch = Vector{0, 0, 0};
		g_NextScorch = 0;
		g_NextVent = now + Random(VentIntervalMin, VentIntervalMax);
		g_LastTime = now;
	}

	const float frametime = std::max(0.f, now - g_LastTime);
	g_Active = true;
	g_LastTime = now;
	g_LastHit = end;

	// The pull follows the hit point, a little behind: it catches up when still.
	const float lag = g_Bend ? g_Bend->value : 0;
	g_Lagged = lag > 0 ? g_Lagged + (end - g_Lagged) * (1 - std::exp(-frametime / lag)) : end;

	// The curve leaves the gun toward where it was hitting and bends onto where it hits: its middle point halfway to
	// the pull, kept within MaxBow of the straight line.
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
}

void JhlEgon_Stop()
{
	if (!g_Active)
		return;

	JhlEgon_Reset();

	if (!JhlParticleSystemEnabled())
		return;

	// A few blue specks fizzling off where it was hitting, falling and fading.
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
