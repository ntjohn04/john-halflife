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

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "dlight.h"
#include "event_api.h"
#include "r_efx.h"
#include "Exports.h"
#include "jhl_displacer.h"
#include "jhl_settings.h"

namespace
{
// Its own color: the portal sprite's (exit1.spr, the ball's), yellow-green.
constexpr const char* BallSprite = "sprites/exit1.spr";
constexpr color24 LightColor{140, 190, 40};

// The charge: from the spin-up to the shot (CDisplacer: fired 0.9 s in, on its next think 0.1 s later), a ball of the
// portal sprite at the muzzle (between its prongs, FindMuzzle) growing quickly
// then easing (ChargeCurve), a little bigger and brighter at the end than the Gauss gun's full charge (its glow 0.144
// of a 64-pixel sprite, 9 units; this 0.13 of an 80-pixel one, 10), and its light (the Gauss gun's to 110 units).
// Flickering a little; going out over FadeTime when it fires (the ball takes over), or if the shot never comes
// (holstered meanwhile) by GiveUpTime.
constexpr float ChargeTime = 1;
constexpr float ChargeCurve = 0.6f;
constexpr float GlowScaleMin = 0.015f;
constexpr float GlowScaleMax = 0.13f;
constexpr float GlowAlphaMin = 0.35f;
constexpr float GlowAlphaMax = 0.9f;
constexpr float GlowFrameRate = 40;
constexpr float LightRadiusMin = 30;
constexpr float LightRadiusMax = 125;
constexpr float Flicker = 0.1f;
constexpr float FadeTime = 0.1f;
constexpr float GiveUpTime = 1.5f;

// The ball in flight (CDisplacerBall: exit1.spr, flying): a light going with it, a little under the charge's.
constexpr float BallLightRadius = 115;
constexpr float BallLightFlicker = 0.08f;
constexpr int ChargeLightKey = 8192 + 4096 + 24;
constexpr int BallLightKey = 57344; // + its index.

float g_ChargeStart = -1;
float g_FiredAt = -1;
float g_FiredCharge = 0;
TEMPENTITY* g_Glow = nullptr;

float Random(float low, float high)
{
	return gEngfuncs.pfnRandomFloat(low, high);
}

// The middle of its three prongs (the view model's attachments 1-3, "reflector01-03"): its first, "spinner", is on the
// bone that spins up, off its axis, so a glow there circled the muzzle.
bool FindMuzzle(Vector& muzzle)
{
	if (0 != CL_IsThirdPerson())
		return false;

	cl_entity_t* viewModel = gEngfuncs.GetViewModel();

	if (!viewModel)
		return false;

	muzzle = g_vecZero;

	for (int i = 1; i <= 3; ++i)
	{
		if (viewModel->attachment[i] == g_vecZero)
			return false;

		muzzle = muzzle + viewModel->attachment[i];
	}

	muzzle = muzzle / 3;
	return true;
}

void EndGlow()
{
	if (g_Glow)
	{
		g_Glow->die = gEngfuncs.GetClientTime();
		g_Glow = nullptr;
	}

	g_ChargeStart = -1;
	g_FiredAt = -1;
}
} // namespace

void JhlDisplacer_SpinUp()
{
	if (!JhlParticleSystemEnabled())
		return;

	g_ChargeStart = gEngfuncs.GetClientTime();
	g_FiredAt = -1;
}

void JhlDisplacer_Fired()
{
	if (g_ChargeStart < 0 || g_FiredAt >= 0)
		return;

	const float now = gEngfuncs.GetClientTime();
	g_FiredAt = now;
	g_FiredCharge = std::clamp((now - g_ChargeStart) / ChargeTime, 0.f, 1.f);
}

void JhlDisplacer_Track(cl_entity_t* entity, const char* modelname)
{
	// The ball: its sprite, flying (the Lambda Core's portals are the same sprite, standing).
	if (!JhlParticleSystemEnabled() || !modelname || 0 != std::strcmp(modelname, BallSprite) ||
		entity->curstate.movetype != MOVETYPE_FLY)
	{
		return;
	}

	// The charge's shot: the server sends no event for a primary shot (CDisplacer::FireThink's is commented out), so a
	// ball seen as the charge ends is it.
	if (g_ChargeStart >= 0 && g_FiredAt < 0 && gEngfuncs.GetClientTime() - g_ChargeStart > ChargeTime * 0.8f)
		JhlDisplacer_Fired();

	if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(BallLightKey + entity->index); light)
	{
		light->origin = entity->origin;
		light->radius = BallLightRadius * Random(1 - BallLightFlicker, 1 + BallLightFlicker);
		light->color = LightColor;
		light->die = gEngfuncs.GetClientTime() + 0.05f;
		light->decay = 0;
	}
}

void JhlDisplacer_Update(double time)
{
	const float now = static_cast<float>(time);

	if (g_ChargeStart < 0)
		return;

	// A new map, the shot never came, or faded out after it.
	if (now < g_ChargeStart || now - g_ChargeStart > GiveUpTime || (g_FiredAt >= 0 && now - g_FiredAt > FadeTime) ||
		!JhlParticleSystemEnabled())
	{
		EndGlow();
		return;
	}

	Vector muzzle;

	if (!FindMuzzle(muzzle))
	{
		EndGlow();
		return;
	}

	const float charge = g_FiredAt >= 0 ? g_FiredCharge : std::clamp((now - g_ChargeStart) / ChargeTime, 0.f, 1.f);
	const float grown = std::pow(charge, ChargeCurve);
	const float fade = g_FiredAt >= 0 ? 1 - (now - g_FiredAt) / FadeTime : 1;
	const float flicker = Random(1 - Flicker, 1 + Flicker);

	const int sprite = gEngfuncs.pEventAPI->EV_FindModelIndex(BallSprite);

	if (!g_Glow && sprite != 0)
	{
		Vector still{0, 0, 0};
		g_Glow = gEngfuncs.pEfxAPI->R_TempSprite(muzzle, still, GlowScaleMin, sprite, kRenderTransAdd, kRenderFxNone, GlowAlphaMin, 2, FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP);

		if (g_Glow)
			g_Glow->entity.curstate.framerate = GlowFrameRate;
	}

	if (g_Glow)
	{
		g_Glow->entity.origin = muzzle;
		g_Glow->entity.curstate.scale = (GlowScaleMin + (GlowScaleMax - GlowScaleMin) * grown) * flicker;
		g_Glow->entity.curstate.renderamt = static_cast<int>(255 * std::min(1.f, (GlowAlphaMin + (GlowAlphaMax - GlowAlphaMin) * grown) * flicker * fade));
		g_Glow->die = now + 0.2f;
	}

	if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(ChargeLightKey); light)
	{
		light->origin = muzzle;
		light->radius = (LightRadiusMin + (LightRadiusMax - LightRadiusMin) * grown) * flicker * fade;
		light->color = LightColor;
		light->die = now + 0.05f;
		light->decay = 0;
	}
}

void JhlDisplacer_Reset()
{
	g_Glow = nullptr;
	g_ChargeStart = -1;
	g_FiredAt = -1;
}
