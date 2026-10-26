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
#include "com_model.h"
#include "dlight.h"
#include "event_api.h"
#include "r_efx.h"
#include "Exports.h"
#include "jhl_explosions.h"
#include "jhl_gauss.h"
#include "jhl_settings.h"

namespace
{
// The gun's own sprites: hotglow (orange, as its balls and glows) and smoke (its beams).
constexpr const char* GlowSprite = "sprites/hotglow.spr";
constexpr const char* ArcSprite = "sprites/smoke.spr";

// The stages, by charge (0-1): the glow from the start, arcs from ArcsFrom, unstable at full charge.
constexpr float ArcsFrom = 0.33f;

// The charge's light at the muzzle (one, keyed; it becomes the muzzle flash when it fires).
constexpr int LightKey = 8192 + 4096 + 16;
constexpr float LightRadiusMin = 30;
constexpr float LightRadiusMax = 110;

// Overcharge: the gun zaps its holder 10 s after the charge started (CGauss); full charge is 4 s in.
constexpr float OverchargeTime = 10;

// The charge counts as stopped if it hears nothing for this long (a missed stop).
constexpr float SpinTimeout = 0.5f;

bool g_Charging = false;
float g_Charge = 0;		   // 0-1.
float g_ChargeStart = 0;   // When it started.
float g_FullSince = -1;	   // When it reached full charge, or -1.
float g_LastSpin = 0;
TEMPENTITY* g_Glow = nullptr;
float g_NextArc = 0;
float g_NextSpit = 0;

float Random(float low, float high)
{
	return gEngfuncs.pfnRandomFloat(low, high);
}

Vector RandomDirection()
{
	Vector direction{Random(-1, 1), Random(-1, 1), Random(-1, 1)};
	return direction.Length() > 0.01f ? direction.Normalize() : Vector{0, 0, 1};
}

// The muzzle: the view model's in first person; in third person the player model's, if it's set.
bool FindMuzzle(Vector& muzzle)
{
	if (0 == CL_IsThirdPerson())
	{
		cl_entity_t* viewModel = gEngfuncs.GetViewModel();

		if (!viewModel || viewModel->attachment[0] == g_vecZero)
			return false;

		muzzle = viewModel->attachment[0];
		return true;
	}

	cl_entity_t* player = gEngfuncs.GetLocalPlayer();

	if (!player || player->attachment[0] == g_vecZero || (player->attachment[0] - player->origin).Length() < 8 ||
		(player->attachment[0] - player->origin).Length() > 96)
		return false;

	muzzle = player->attachment[0];
	return true;
}

void EndGlow()
{
	if (g_Glow)
	{
		g_Glow->die = gEngfuncs.GetClientTime();
		g_Glow = nullptr;
	}
}

void Light(const Vector& origin, float radius, const color24& color, float life, float decay)
{
	if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(LightKey); light)
	{
		light->origin = origin;
		light->radius = radius;
		light->color = color;
		light->die = gEngfuncs.GetClientTime() + life;
		light->decay = decay;
	}
}

// A short arc crackling off the muzzle; wilder when unstable.
void Arc(const Vector& muzzle, bool unstable)
{
	const int sprite = gEngfuncs.pEventAPI->EV_FindModelIndex(ArcSprite);

	if (sprite == 0)
		return;

	Vector from = muzzle + RandomDirection() * Random(1, 3);
	Vector to = muzzle + RandomDirection() * Random(5, unstable ? 16 : 10);

	gEngfuncs.pEfxAPI->R_BeamPoints(from, to, sprite, Random(0.05f, 0.12f), unstable ? 0.6f : 0.4f, unstable ? 0.7f : 0.35f,
		Random(0.5f, 0.8f), 0, 0, 0, 1, 0.75f, 0.4f);
}

// A spark of it spitting out of the muzzle, falling and fading (the gun's orange glow, small).
void Spit(const Vector& muzzle)
{
	const int sprite = gEngfuncs.pEventAPI->EV_FindModelIndex(GlowSprite);

	if (sprite == 0)
		return;

	Vector forward, right, up;
	AngleVectors(gHUD.m_vecAngles, forward, right, up);

	Vector position = muzzle;
	Vector velocity = forward * Random(30, 90) + right * Random(-60, 60) + up * Random(10, 60);
	const float life = Random(0.6f, 1);

	TEMPENTITY* spark = gEngfuncs.pEfxAPI->R_TempSprite(position, velocity, 0.04f, sprite, kRenderTransAdd, kRenderFxNone, 1,
		life, FTENT_SLOWGRAVITY | FTENT_COLLIDEWORLD);

	if (spark)
	{
		spark->entity.angles.z = Random(0, 359);
		JhlSprite_PlayOnce(spark, life, 0.6f);
	}
}
} // namespace

void JhlGauss_Spin(int pitch)
{
	const float now = gEngfuncs.GetClientTime();

	if (!g_Charging || now < g_LastSpin)
	{
		g_Charging = true;
		g_ChargeStart = now;
		g_FullSince = -1;
		g_NextArc = now;
		g_NextSpit = now;
	}

	g_LastSpin = now;
	g_Charge = std::clamp((pitch - 100) / 150.f, 0.f, 1.f);

	if (g_Charge >= 1 && g_FullSince < 0)
		g_FullSince = now;
}

void JhlGauss_Fire(bool primary, float damage)
{
	const float charge = primary ? 0.f : std::clamp(damage / 200, 0.f, 1.f);

	JhlGauss_StopCharge();

	if (!JhlParticleSystemEnabled())
		return;

	// The muzzle flash's light (as the guns' muzzle light), the charge's light becoming it: bigger and whiter the more
	// it was charged.
	if (Vector muzzle; FindMuzzle(muzzle))
	{
		const float radius = (primary ? 140 : 140 + 120 * charge) * 1.15f;
		const color24 color = primary ? color24{255, 160, 60} : color24{255, static_cast<byte>(170 + 60 * charge), static_cast<byte>(80 + 120 * charge)};
		Light(muzzle, radius, color, 0.1f, radius * 8);
	}
}

void JhlGauss_StopCharge()
{
	g_Charging = false;
	g_Charge = 0;
	EndGlow();
}

void JhlGauss_ReflectLight(const Vector& position)
{
	if (!JhlParticleSystemEnabled())
		return;

	// As faint as the small beams' lights, in the gun's orange, fading over 0.4 s.
	if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(0); light)
	{
		light->origin = position;
		light->radius = 72;
		light->color = {255, 140, 40};
		light->die = gEngfuncs.GetClientTime() + 0.4f;
		light->decay = 72 / 0.4f;
	}
}

void JhlGauss_ImpactLight(const Vector& position)
{
	if (!JhlParticleSystemEnabled())
		return;

	// A little bigger and brighter than a reflection's, fading over 0.5 s.
	if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(0); light)
	{
		light->origin = position;
		light->radius = 100;
		light->color = {255, 170, 70};
		light->die = gEngfuncs.GetClientTime() + 0.5f;
		light->decay = 100 / 0.5f;
	}
}

void JhlGauss_Update(double time)
{
	const float now = static_cast<float>(time);

	if (!g_Charging)
		return;

	// A missed stop, a new map: it ends.
	if (now < g_LastSpin || now - g_LastSpin > SpinTimeout || !JhlParticleSystemEnabled())
	{
		JhlGauss_StopCharge();
		return;
	}

	Vector muzzle;

	if (!FindMuzzle(muzzle))
	{
		EndGlow();
		return;
	}

	// Unstable at full charge, more so on its way to the overcharge.
	const bool unstable = g_FullSince >= 0;
	const float instability = unstable ? std::clamp((now - g_FullSince) / std::max(1.f, OverchargeTime - (g_FullSince - g_ChargeStart)), 0.f, 1.f) : 0;
	const float flicker = unstable ? Random(1 - 0.35f * (0.5f + instability), 1 + 0.2f) : 1;

	// a) A glow on the muzzle, growing with the charge, and its light on the hands and around.
	const int glowSprite = gEngfuncs.pEventAPI->EV_FindModelIndex(GlowSprite);

	if (!g_Glow && glowSprite != 0)
	{
		Vector still{0, 0, 0};
		g_Glow = gEngfuncs.pEfxAPI->R_TempSprite(muzzle, still, 0.03f, glowSprite, kRenderTransAdd, kRenderFxNone, 0.3f, 1, 0);
	}

	if (g_Glow)
	{
		// A little above the muzzle, so the barrel doesn't hide it while it's still small.
		Vector forward, right, up;
		AngleVectors(gHUD.m_vecAngles, forward, right, up);

		g_Glow->entity.origin = muzzle + up * 2.5f;
		g_Glow->entity.curstate.scale = (0.036f + 0.108f * g_Charge) * flicker;
		g_Glow->entity.curstate.renderamt = static_cast<int>(255 * std::min(1.f, (0.3f + 0.5f * g_Charge) * flicker));
		g_Glow->die = now + 0.2f;
	}

	Light(muzzle, (LightRadiusMin + (LightRadiusMax - LightRadiusMin) * g_Charge) * flicker, {255, 140, 40}, 0.1f, 0);

	// b) Arcs crackling off the muzzle, more often as it charges; twice as often, and wilder, when unstable.
	if (g_Charge >= ArcsFrom && now >= g_NextArc)
	{
		const float stage = (g_Charge - ArcsFrom) / (1 - ArcsFrom);
		const float rate = unstable ? 20 + 10 * instability : 4 + 8 * stage;
		Arc(muzzle, unstable);
		g_NextArc = now + Random(0.5f, 1.5f) / rate;
	}

	// c) Unstable: sparks spitting out of the muzzle, more as the overcharge nears.
	if (unstable && now >= g_NextSpit)
	{
		Spit(muzzle);
		g_NextSpit = now + Random(0.5f, 1.5f) / (3 + 6 * instability);
	}
}

void JhlGauss_Reset()
{
	g_Charging = false;
	g_Glow = nullptr;
}
