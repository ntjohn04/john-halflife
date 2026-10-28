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

// Reserved takes on MMod's weapon effects, using engine particles, short lights and stock sprites:
// bullet impacts and water splashes (see re/specs/bullet_impacts.md) and muzzle light and smoke.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "hud.h"
#include "weapon_effects.h"
#include "Exports.h"
#include "event_api.h"
#include "r_efx.h"
#include "beamdef.h"
#include "dlight.h"
#include "pm_materials.h"
#include "parsemsg.h"
#include "customentity.h"
#include "jhl_explosions.h"
#include "jhl_settings.h"
#include "view.h"
#include "networking/ClientUserMessages.h"


// Dynamic light keys for tripmine lights (TripmineLight), clear of the engine's entity-index keys.
constexpr int TripmineLightKey = 8192;
constexpr float TripmineLightDistance = 1024;

// Dynamic light keys for lobbed spores' lights (SporeLight).
constexpr int SporeLightKey = 12288;

// Dynamic light keys for RPG laser dots (RpgLaserDot).
constexpr int RpgDotLightKey = 16384;

// Dynamic light keys for RPG rockets' motors (RocketGlow).
constexpr int RocketLightKey = 20480;

static cvar_t* g_cl_bulletimpacts = nullptr;
static cvar_t* g_cl_muzzleflash = nullptr;

static bool MuzzleEffectsEnabled()
{
	// In third person the effects would be placed relative to a first person view that isn't there.
	return 0 != g_cl_muzzleflash->value && 0 == CL_IsThirdPerson();
}

static bool IsZoomed()
{
	return gHUD.m_iFOV != 0 && gHUD.m_iFOV < static_cast<int>(CVAR_GET_FLOAT("default_fov"));
}

static void MuzzleEffectsAt(const Vector& position, float wispChance, float wispScale = 1, float wispAlpha = 0.1f, int rolls = 1,
	float lightScale = 1, float lightTime = 0.06f);

// Smoke leaves the gun moving with it, and the air stops it: it's given the shooter's velocity on top of its own
// (kept in curstate.vuser1), and sheds it, halving every 0.5 s (CarriedTime), so it hangs where it was let out a
// moment after (a sprite's velocity otherwise never changes but for gravity).
constexpr float CarriedTime = 0.72f;

static void ShedCarried(TEMPENTITY* ent, float frametime, float currenttime)
{
	const float keep = std::exp(-frametime / CarriedTime);
	Vector carried = ent->entity.curstate.vuser1;
	ent->entity.baseline.origin = ent->entity.baseline.origin - carried * (1 - keep);
	ent->entity.curstate.vuser1 = carried * keep;
}

static void Carry(TEMPENTITY* ent, const Vector& velocity)
{
	ent->entity.baseline.origin = ent->entity.baseline.origin + velocity;
	ent->entity.curstate.vuser1 = velocity;
	ent->flags |= FTENT_CLIENTCUSTOM;
	ent->callback = ShedCarried;
}

// Where the view model's muzzle is, if muzzle effects should be drawn there now.
static bool FindMuzzle(Vector& muzzle)
{
	// Zoomed in, the view model isn't drawn, so its attachment is stale and the effects would appear where the
	// muzzle last was.
	if (!MuzzleEffectsEnabled() || IsZoomed())
	{
		return false;
	}

	cl_entity_t* viewModel = gEngfuncs.GetViewModel();

	if (!viewModel || !viewModel->model)
	{
		return false;
	}

	// The muzzle is the first attachment on the stock view models; it's set while the model is drawn.
	muzzle = viewModel->attachment[0];

	return muzzle != g_vecZero;
}

void EV_GunMuzzleEffects(int pellets, bool doubleShot)
{
	Vector muzzle;

	if (!FindMuzzle(muzzle))
	{
		return;
	}

	// Now and then a wisp of smoke; real guns barely smoke. A shotgun's chance is per pellet, so it smokes more.
	// The shotgun's double shot, with jhl_fx_particlesystem, a little more of a blast: its light a little bigger, and
	// two small puffs pushed out ahead, carrying the player's movement.
	const bool blast = doubleShot && JhlParticleSystemEnabled();
	MuzzleEffectsAt(muzzle, 0.3f, 1, 0.1f, std::max(1, pellets), blast ? 1.2f : 1);

	if (!blast)
		return;

	const int modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/steam1.spr");
	Vector forward;
	AngleVectors(v_angles, &forward, nullptr, nullptr);

	for (int i = 0; modelIndex != 0 && i < 2; ++i)
	{
		Vector position = muzzle + forward * gEngfuncs.pfnRandomFloat(2, 6);
		Vector velocity = forward * gEngfuncs.pfnRandomFloat(40, 70) +
						  Vector{gEngfuncs.pfnRandomFloat(-6, 6), gEngfuncs.pfnRandomFloat(-6, 6), gEngfuncs.pfnRandomFloat(0, 10)};

		if (TEMPENTITY* puff = gEngfuncs.pEfxAPI->R_TempSprite(position, velocity, gEngfuncs.pfnRandomFloat(0.15f, 0.2f), modelIndex,
				kRenderTransAlpha, kRenderFxNone, 0.09f, 2.0f, FTENT_SPRANIMATE);
			puff)
		{
			puff->entity.curstate.rendercolor = {190, 190, 190};
			puff->entity.curstate.framerate = gEngfuncs.pfnRandomFloat(20, 26);
			puff->entity.angles.z = gEngfuncs.pfnRandomFloat(0, 359);
			Carry(puff, v_sim_vel);
		}
	}
}

void EV_GlockRapidMuzzleEffects()
{
	// As the M249's, much lighter (EV_M249MuzzleEffects): how hot a burst has got it, 0-1, up a 12th a round, back to 0
	// after a pause; its wisps from a gun's (30% of shots, 0.1, 1x) to 45%, 0.15, up to 1.15x.
	constexpr float HeatPerRound = 1.f / 12;
	constexpr float CoolAfter = 0.3f;

	static float heat = 0;
	static float lastShot = 0;

	const float now = gEngfuncs.GetClientTime();

	if (now < lastShot || now - lastShot > CoolAfter)
		heat = 0;

	lastShot = now;
	heat = std::min(1.f, heat + HeatPerRound);

	Vector muzzle;

	if (!FindMuzzle(muzzle))
		return;

	if (!JhlParticleSystemEnabled())
	{
		MuzzleEffectsAt(muzzle, 0.3f);
		return;
	}

	MuzzleEffectsAt(muzzle, 0.3f + heat * 0.15f, 1 + heat * gEngfuncs.pfnRandomFloat(0, 0.15f), 0.1f + heat * 0.05f);
}

void EV_MP5GrenadeEffects()
{
	// A grenade launch, with jhl_fx_particlesystem: always two wisps, more than twice a gun's and more opaque, and its
	// light a little bigger and longer (it had nothing but the sound).
	Vector muzzle;

	if (!JhlParticleSystemEnabled() || !FindMuzzle(muzzle))
		return;

	MuzzleEffectsAt(muzzle, 1, 2.2f, 0.18f, 2, 1.1f, 0.08f);
}

void EV_M249MuzzleEffects()
{
	// How hot it is from sustained fire, 0-1: up a 25th a round, back to 0 after a pause (tap fire stays cold).
	constexpr float HeatPerRound = 1.f / 25;
	constexpr float CoolAfter = 0.25f;

	static float heat = 0;
	static float lastShot = 0;

	const float now = gEngfuncs.GetClientTime();

	// A new map restarts the clock.
	if (now < lastShot || now - lastShot > CoolAfter)
	{
		heat = 0;
	}

	lastShot = now;
	heat = std::min(1.f, heat + HeatPerRound);

	Vector muzzle;

	if (!FindMuzzle(muzzle))
	{
		return;
	}

	// More than a gun's even cold (37% of shots, opacity 0.13, 1.08x); hot, more often (70%), more opaque (0.33) and
	// bigger on average, varied (120-145%); and a little off the muzzle so wisps don't stack.
	const float chance = 0.37f + heat * 0.33f;
	const float scale = 1.08f + heat * gEngfuncs.pfnRandomFloat(0.12f, 0.37f);
	const float alpha = 0.13f + heat * 0.2f;
	const Vector drift{gEngfuncs.pfnRandomFloat(-1.5f, 1.5f), gEngfuncs.pfnRandomFloat(-1.5f, 1.5f), gEngfuncs.pfnRandomFloat(-1.5f, 1.5f)};

	if (!JhlParticleSystemEnabled())
	{
		MuzzleEffectsAt(muzzle, 0.3f);
		return;
	}

	MuzzleEffectsAt(muzzle + drift, chance, scale, alpha);
}

static void Flash(const Vector& origin, float scale, float brightness);

// A thin wisp off a barrel after a shot (SniperSmoke_Update, EagleSmoke_Update): @p left of its stream to go (1 to 0),
// thinning as it does; rising, carrying the player's movement, shed as smoke's (Carry); lasting @p life of its sprite's
// run.
static void BarrelWisp(const Vector& muzzle, float left, float scaleMin, float scaleMax, float alphaBase, float alphaMore, float life = 1)
{
	const int modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/steam1.spr");

	if (modelIndex == 0)
		return;

	Vector position = muzzle;
	Vector velocity{gEngfuncs.pfnRandomFloat(-3, 3), gEngfuncs.pfnRandomFloat(-3, 3), gEngfuncs.pfnRandomFloat(10, 16)};

	if (TEMPENTITY* wisp = gEngfuncs.pEfxAPI->R_TempSprite(position, velocity, gEngfuncs.pfnRandomFloat(scaleMin, scaleMax) * (0.6f + 0.4f * left),
			modelIndex, kRenderTransAlpha, kRenderFxNone, alphaBase + alphaMore * left, 2.0f * life, FTENT_SPRANIMATE);
		wisp)
	{
		wisp->entity.curstate.rendercolor = {190, 190, 190};
		wisp->entity.curstate.framerate = gEngfuncs.pfnRandomFloat(24, 30) / life;
		wisp->entity.angles.z = gEngfuncs.pfnRandomFloat(0, 359);
		Carry(wisp, v_sim_vel);
	}
}

// The Desert Eagle's barrel smoking a moment after a shot (EagleSmoke_Update): a smaller, shorter, fainter stream than
// the sniper rifle's, a big-bore pistol's, its wisps gone a little sooner.
constexpr float EagleSmokeLife = 0.93f;
constexpr float EagleSmokeTime = 0.35f;
constexpr float EagleSmokeEvery = 0.1f; // Four: at 0, 0.1, 0.2 and 0.3 s.
static float g_EagleSmokeStart = -1;
static float g_EagleSmokeNext = 0;

// The sniper rifle's barrel venting smoke through the bolt's working (SniperSmoke_Update): a thin wisp every
// SniperSmokeEvery for SniperSmokeTime after a shot, thinning out, from its barrel's tip as the view model's drawn (its
// attachment sits behind the player; the tip, found in its vertices, in its rifle's bone, m40a1_stock), so it stays on
// the barrel as the bolt's worked; carrying the player's movement, shed as smoke's (Carry).
constexpr float SniperSmokeTime = 1.2f;
constexpr float SniperSmokeEvery = 0.07f;
constexpr int SniperBarrelBone = 5;
const Vector SniperMuzzle{-25.6f, 0, 1.33f};
static float g_SniperSmokeStart = -1;
static float g_SniperSmokeNext = 0;

void EV_SniperMuzzleEffects(const Vector& eyePosition, const Vector& forward, const Vector& right, const Vector& up)
{
	if (!MuzzleEffectsEnabled())
	{
		return;
	}

	// The M40A1 view model's attachment sits behind the player, so place the muzzle relative to the view instead:
	// the view model is always drawn in the same place on screen.
	// Offset from the eyes, checked in game.
	constexpr float Forward = 30;
	constexpr float Right = 5;
	constexpr float Up = -5;

	// The light; its smoke comes off the barrel through the bolt's working instead (SniperSmoke_Update), with
	// jhl_fx_particlesystem; otherwise a slow, heavy rifle shot more often leaves a wisp, except when scoped, where it
	// would drift across the magnified view.
	// With jhl_fx_particlesystem its light's bigger and a little longer, a heavy round's (190-230 units, 0.08 s; a gun's
	// 140-170, 0.06 s), and its flash is shown: the engine draws a gun's flash at its view model's first attachment,
	// which on this one sits behind the player, so it never showed; it's drawn on its barrel's tip instead.
	const bool stream = JhlParticleSystemEnabled();
	MuzzleEffectsAt(eyePosition + forward * Forward + right * Right + up * Up, stream || IsZoomed() ? 0 : 0.6f, 1, 0.1f, 1,
		stream ? 1.35f : 1, stream ? 0.08f : 0.06f);

	if (stream)
	{
		g_SniperSmokeStart = gEngfuncs.GetClientTime();

		if (Vector barrel; !IsZoomed() && JhlViewModelPoint("models/v_m40a1.mdl", SniperBarrelBone, SniperMuzzle, barrel))
			Flash(barrel, 0.3f, 0.8f);
	}
}

void SniperSmoke_Update(double time)
{
	const float now = static_cast<float>(time);
	const float age = now - g_SniperSmokeStart;

	if (g_SniperSmokeStart < 0 || age > SniperSmokeTime || age < 0)
	{
		g_SniperSmokeStart = -1;
		return;
	}

	// (A new map or a loaded game restarts the clock: one due further off than that is from before.)
	if (now < g_SniperSmokeNext && g_SniperSmokeNext - now <= SniperSmokeEvery)
		return;

	g_SniperSmokeNext = now + SniperSmokeEvery;

	// Not scoped (it would drift across the magnified view), and only on the barrel as it's drawn.
	Vector muzzle;

	if (IsZoomed() || !MuzzleEffectsEnabled() || !JhlViewModelPoint("models/v_m40a1.mdl", SniperBarrelBone, SniperMuzzle, muzzle))
		return;

	// Thinning out as it goes.
	BarrelWisp(muzzle, 1 - age / SniperSmokeTime, 0.06f, 0.09f, 0.04f, 0.1f);
}

void EV_EagleBarrelSmoke()
{
	if (JhlParticleSystemEnabled())
		g_EagleSmokeStart = gEngfuncs.GetClientTime();
}

void EagleSmoke_Update(double time)
{
	const float now = static_cast<float>(time);
	const float age = now - g_EagleSmokeStart;

	if (g_EagleSmokeStart < 0 || age > EagleSmokeTime || age < 0)
	{
		g_EagleSmokeStart = -1;
		return;
	}

	// (A new map or a loaded game restarts the clock: one due further off than that is from before.)
	if (now < g_EagleSmokeNext && g_EagleSmokeNext - now <= EagleSmokeEvery)
		return;

	g_EagleSmokeNext = now + EagleSmokeEvery;

	// Its muzzle is its view model's first attachment, as the other guns' (not while scoped or in third person).
	if (Vector muzzle; FindMuzzle(muzzle))
		BarrelWisp(muzzle, 1 - age / EagleSmokeTime, 0.05f, 0.07f, 0.03f, 0.07f, EagleSmokeLife);
}

// A flash of flame for a moment (the engine's own muzzle flash, muzzleflash2, additive), turned at random.
static void Flash(const Vector& origin, float scale, float brightness)
{
	const int model = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/muzzleflash2.spr");

	if (model == 0)
		return;

	Vector position = origin;
	Vector still{0, 0, 0};

	if (TEMPENTITY* flash = gEngfuncs.pEfxAPI->R_TempSprite(position, still, scale, model, kRenderTransAdd, kRenderFxNone,
			brightness, 0.07f, 0);
		flash)
	{
		flash->entity.curstate.frame = static_cast<float>(gEngfuncs.pfnRandomLong(0, 1));
		flash->entity.angles.z = gEngfuncs.pfnRandomFloat(0, 359);
	}
}

void EV_RpgFireEffects(bool isLocal, const Vector& eyePosition, const Vector& forward, const Vector& right, const Vector& up,
	const Vector& shooterVelocity)
{
	if (0 == g_cl_muzzleflash->value)
	{
		return;
	}

	// The RPG view model has no attachments, so place the front of the tube relative to the view (see EV_SniperMuzzleEffects).
	if (isLocal && MuzzleEffectsEnabled())
	{
		// Offset from the eyes, checked in game.
		constexpr float Forward = 36;
		constexpr float Right = 8;
		constexpr float Up = -6;

		// Always two wisps, much larger than a gun's and twice as opaque.
		const Vector front = eyePosition + forward * Forward + right * Right + up * Up;
		MuzzleEffectsAt(front, 1, 4, 0.2f, 2);

		// The launch charge's small flash out of the front.
		Flash(front, 0.25f, 0.6f);
	}

	// Backblast out of the rear of the tube, over the right shoulder. Placed from the player rather than the view
	// model, so other players and third person see it too.
	const Vector rear = eyePosition - forward * 24 + right * 8 - up * 4;

	if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(0); light)
	{
		light->origin = rear;
		light->radius = gEngfuncs.pfnRandomFloat(200, 240);
		light->color.r = 255;
		light->color.g = 170;
		light->color.b = 90;
		light->die = gEngfuncs.GetClientTime() + 0.15f;
		light->decay = light->radius / 0.15f;
	}

	// A short flame out of the back: two flashes along it (a sprite faces the view, so two make it long seen side on).
	Flash(rear, 0.45f, 0.8f);
	Flash(rear - forward * 14, 0.35f, 0.6f);

	const int modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/steam1.spr");

	if (modelIndex == 0)
	{
		return;
	}

	// The blast itself, out the back.
	for (int i = 0; i < 9; ++i)
	{
		Vector origin = rear - forward * gEngfuncs.pfnRandomFloat(0, 12);
		Vector velocity = forward * -gEngfuncs.pfnRandomFloat(90, 160) + right * gEngfuncs.pfnRandomFloat(-40, 40) + up * gEngfuncs.pfnRandomFloat(-10, 25);

		// Plays once and is removed on its last frame (see DustImpact); a slower frame rate lets the plume linger.
		TEMPENTITY* puff = gEngfuncs.pEfxAPI->R_TempSprite(origin, velocity, gEngfuncs.pfnRandomFloat(0.6f, 0.95f),
			modelIndex, kRenderTransAlpha, kRenderFxNone, 0.3f, 3.0f, FTENT_SPRANIMATE);

		if (puff)
		{
			puff->entity.curstate.rendercolor = {170, 170, 170};
			puff->entity.curstate.framerate = 11; // 16 frames: about 1.45 s.
			puff->entity.angles.z = gEngfuncs.pfnRandomFloat(0, 359);
			Carry(puff, shooterVelocity);
		}
	}

	// Smoke billowing back around the shooter: large, slow puffs that start behind on either side and roll forward
	// past the shoulders, into view. Three per side, one of the six left out at random.
	const int skipped = gEngfuncs.pfnRandomLong(0, 5);

	for (int i = 0; i < 6; ++i)
	{
		if (i == skipped)
		{
			continue;
		}

		const float side = (i % 2) == 0 ? 1.f : -1.f;

		Vector origin = rear + right * side * gEngfuncs.pfnRandomFloat(12, 22) - up * gEngfuncs.pfnRandomFloat(0, 12);
		Vector velocity = forward * gEngfuncs.pfnRandomFloat(35, 55) + right * side * gEngfuncs.pfnRandomFloat(15, 30) + up * gEngfuncs.pfnRandomFloat(0, 12);

		// 16 frames at 6.5 per second: about two and a half seconds.
		TEMPENTITY* puff = gEngfuncs.pEfxAPI->R_TempSprite(origin, velocity, gEngfuncs.pfnRandomFloat(1.0f, 1.5f),
			modelIndex, kRenderTransAlpha, kRenderFxNone, 0.19f, 3.0f, FTENT_SPRANIMATE);

		if (puff)
		{
			puff->entity.curstate.rendercolor = {170, 170, 170};
			puff->entity.curstate.framerate = 6.5f;
			puff->entity.angles.z = gEngfuncs.pfnRandomFloat(0, 359);
			Carry(puff, shooterVelocity);
		}
	}
}

static void MuzzleEffectsAt(const Vector& position, float wispChance, float wispScale, float wispAlpha, int rolls,
	float lightScale, float lightTime)
{
	Vector muzzle = position;

	// A warm flash on top of the engine's own muzzle light, making it stronger and slightly longer.
	if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(0); light)
	{
		light->origin = muzzle;
		light->radius = gEngfuncs.pfnRandomFloat(140, 170) * lightScale;
		light->color.r = 255;
		light->color.g = 190;
		light->color.b = 110;
		light->die = gEngfuncs.GetClientTime() + lightTime;
		light->decay = light->radius / (lightTime * 1.4f);
	}

	const int modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/steam1.spr");

	if (modelIndex == 0)
	{
		return;
	}

	// A faint wisp of smoke drifting up from the muzzle, with a chance for each roll (a shotgun's pellets).
	bool first = true;

	for (int i = 0; i < rolls; ++i)
	{
		if (gEngfuncs.pfnRandomFloat(0, 1) >= wispChance)
		{
			continue;
		}

		Vector origin = muzzle;
		Vector velocity{gEngfuncs.pfnRandomFloat(-4, 4), gEngfuncs.pfnRandomFloat(-4, 4), gEngfuncs.pfnRandomFloat(12, 20)};
		float scale = gEngfuncs.pfnRandomFloat(0.08f, 0.12f) * wispScale;
		float framerate = 28;

		// Later wisps of the same shot spread out from the first, so they don't play over it: a little apart, drifting
		// their own ways, some bigger and slower.
		if (!first)
		{
			origin = origin + Vector{gEngfuncs.pfnRandomFloat(-2, 2), gEngfuncs.pfnRandomFloat(-2, 2), gEngfuncs.pfnRandomFloat(-1, 2)};
			velocity = velocity + Vector{gEngfuncs.pfnRandomFloat(-6, 6), gEngfuncs.pfnRandomFloat(-6, 6), gEngfuncs.pfnRandomFloat(-4, 6)};
			scale *= gEngfuncs.pfnRandomFloat(0.9f, 1.4f);
			framerate = gEngfuncs.pfnRandomFloat(22, 30);
		}

		first = false;

		// Plays once and is removed on its last frame (see DustImpact).
		TEMPENTITY* wisp = gEngfuncs.pEfxAPI->R_TempSprite(origin, velocity, scale,
			modelIndex, kRenderTransAlpha, kRenderFxNone, wispAlpha, 2.0f, FTENT_SPRANIMATE);

		if (wisp)
		{
			wisp->entity.curstate.rendercolor = {190, 190, 190};
			wisp->entity.curstate.framerate = framerate;
			wisp->entity.angles.z = gEngfuncs.pfnRandomFloat(0, 359);

			// Only the local player's guns smoke here (first person): theirs is the velocity.
			Carry(wisp, v_sim_vel);
		}
	}
}

// The RPG's laser dot: where it is now (RpgLaserDot), and its line when it comes on (RpgLaserLine_Update); when the
// local player's last started reloading (its dot is hidden a moment then, and coming back isn't coming on).
static Vector g_RpgDot;
static float g_RpgDotSeen = -1;
static BEAM* g_RpgLine = nullptr;
static float g_RpgLineStart = -1;
static float g_RpgReloaded = -100;

// A dot not seen for this long that's seen again has come on (turned on, or the RPG taken out with it on), unless a
// reload started within ReloadHide (its dot hidden 2.1 s then, CRpg::Reload).
constexpr float DotAbsent = 0.15f;
constexpr float ReloadHide = 3;

// Its line: thin, red, as bright as this, holding LineHold then fading out by LineTime.
constexpr float LineWidth = 0.5f;
constexpr float LineBrightness = 0.4f;
constexpr float LineHold = 0.15f;
constexpr float LineTime = 1;

// The front of the laser's housing in v_rpg.mdl, in its launcher's bone (Dummy22): the middle of the eight-sided ring
// at the front of the small cylinder alongside the front of the tube, found in its vertices.
constexpr int RpgLaserBone = 12;
const Vector RpgLaserHousing{3.37f, -9.88f, -3.39f};

void RpgLaserDot(cl_entity_t* dot, const char* modelname)
{
	// The RPG's (the Desert Eagle's laser has the same dot, drawn at half size: CEagleLaser).
	if (!JhlParticleSystemEnabled() || !modelname || 0 != std::strcmp(modelname, "sprites/laserdot.spr") ||
		(dot->curstate.scale > 0 && dot->curstate.scale < 0.75f))
	{
		return;
	}

	const float now = gEngfuncs.GetClientTime();

	// Come on: its line, from now.
	if ((now - g_RpgDotSeen > DotAbsent || now < g_RpgDotSeen) && now - g_RpgReloaded > ReloadHide)
		g_RpgLineStart = now;

	g_RpgDot = dot->origin;
	g_RpgDotSeen = now;

	// As faint as the tripmine's (72 units, its color at 80%): the dot's red.
	if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(RpgDotLightKey + dot->index); light)
	{
		light->origin = dot->origin;
		light->radius = 72;
		light->color = {204, 0, 0};
		light->die = gEngfuncs.GetClientTime() + 0.05f;
		light->decay = 0;
	}
}

void RpgLaser_Reloading()
{
	g_RpgReloaded = gEngfuncs.GetClientTime();
}

void RpgLaserLine_Update(double time)
{
	const float now = static_cast<float>(time);
	const float age = now - g_RpgLineStart;

	// Done, or a new map (the clock restarted).
	if (g_RpgLineStart < 0 || age > LineTime || age < 0 || 0 != CL_IsThirdPerson())
	{
		if (g_RpgLine)
		{
			g_RpgLine->die = 0;
			g_RpgLine->brightness = 0;
			g_RpgLine = nullptr;
		}

		if (age > LineTime || age < 0)
			g_RpgLineStart = -1;

		return;
	}

	const auto gone = []()
	{
		if (g_RpgLine)
		{
			g_RpgLine->die = 0;
			g_RpgLine->brightness = 0;
			g_RpgLine = nullptr;
		}
	};

	// The dot's gone (the laser turned off): no line (the engine would free it if kept but not kept up).
	if (now - g_RpgDotSeen > 0.05f)
	{
		gone();
		return;
	}

	// From the laser's housing, the small cylinder under the front of the tube: the view model has no attachments, so
	// its front (in the launcher's bone, Dummy22, 12) is placed from its bone as last drawn, moved with the view model
	// since (JhlViewModelPoint; this is run once the view's placed, V_CalcRefdef).
	Vector from;

	if (!JhlViewModelPoint("models/v_rpg.mdl", RpgLaserBone, RpgLaserHousing, from))
	{
		gone();
		return;
	}

	Vector to = g_RpgDot;

	const float fade = age < LineHold ? 1 : 1 - (age - LineHold) / (LineTime - LineHold);
	const float brightness = LineBrightness * fade * fade;

	if (!g_RpgLine)
	{
		const int sprite = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/laserbeam.spr");

		if (sprite == 0)
			return;

		g_RpgLine = gEngfuncs.pEfxAPI->R_BeamPoints(from, to, sprite, 1, LineWidth, 0, brightness, 0, 0, 0, 1, 0.12f, 0.12f);

		if (!g_RpgLine)
			return;
	}

	g_RpgLine->source = from;
	g_RpgLine->target = to;
	g_RpgLine->delta = to - from;
	g_RpgLine->brightness = brightness;
	g_RpgLine->die = now + 0.2f;
	g_RpgLine->flags |= FBEAM_ISACTIVE;
}

void RpgLaser_Reset()
{
	g_RpgLine = nullptr;
	g_RpgLineStart = -1;
	g_RpgReloaded = -100;
	g_RpgDotSeen = -1;
}

// An RPG rocket's motor: its glow (an orange glow, hotglow, with a white-hot core, glow02, on top), and its light, warm
// orange; flickering as it burns, the core the most, fading out over BurnoutFade once it's burnt out (the engine's,
// EF_LIGHT, was a big flat white flare and light, gone at once).
constexpr float RocketGlowScale = 0.9f;
constexpr float RocketCoreScale = 0.27f;
constexpr float RocketLightRadius = 200;
constexpr color24 RocketLightColor{255, 150, 60};

// Each frame its size times between these (the glow evenly about its size; the core mostly flaring bigger, now and then
// a little smaller), and its brightness down to the first.
constexpr float RocketFlickerMin[2] = {0.65f, 0.8f};
constexpr float RocketFlickerMax[2] = {1.35f, 1.6f};
constexpr float RocketLightFlicker = 0.2f;
constexpr float BurnoutFade = 0.25f;

// Its trail, smoke laid along its path from when its motor lights (in place of the engine's follow beam, which only
// adds light, and drawn in straight pieces showed seams and bright overlaps where it turned): grey puffs (steam1, as
// the backblast's), one every TrailSpacing units, slowly rising and coming apart over about TrailLife, starting fairly
// thick (TrailAlpha) and fading out as they spread (to TrailGrowth times their size); not under water (vanilla's
// bubbles there).
constexpr float TrailSpacing = 32;
constexpr float TrailLife = 3.75f;
constexpr float TrailAlpha = 0.5f;
constexpr float TrailGrowth = 2.2f;
constexpr int TrailMostAFrame = 12;

// A trail puff as it goes (curstate.fuser1: when it was made; fuser2: its life; fuser3: its size at first).
static void SpreadPuff(TEMPENTITY* puff, float frametime, float currenttime)
{
	const auto& state = puff->entity.curstate;
	const float p = std::clamp((currenttime - state.fuser1) / state.fuser2, 0.f, 1.f);

	puff->entity.curstate.scale = state.fuser3 * (1 + (TrailGrowth - 1) * p);
	puff->entity.curstate.renderamt = static_cast<int>(255 * TrailAlpha * std::pow(1 - p, 1.3f));
}

struct Rocket
{
	int Index;
	float LastSeen;
	float BurntOut; // When it was first seen burnt out; -1 while it burns.
	TEMPENTITY* Glows[2];
	Vector LastPuff;
	bool Puffed;
};

static void TrailPuff(const Vector& origin)
{
	const int model = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/steam1.spr");

	Vector position = origin;

	if (model == 0 || gEngfuncs.PM_PointContents(position, nullptr) == CONTENTS_WATER)
		return;

	position = position + Vector{gEngfuncs.pfnRandomFloat(-3, 3), gEngfuncs.pfnRandomFloat(-3, 3), gEngfuncs.pfnRandomFloat(-3, 3)};

	// Rising about 18 units over its life, wandering a little.
	Vector velocity{gEngfuncs.pfnRandomFloat(-3, 3), gEngfuncs.pfnRandomFloat(-3, 3), gEngfuncs.pfnRandomFloat(2.7f, 6.8f)};

	// Plays once and is removed on its last frame (16 of them): slowly, so it lingers.
	const float scale = gEngfuncs.pfnRandomFloat(0.6f, 0.9f);
	const float life = TrailLife * gEngfuncs.pfnRandomFloat(0.85f, 1.15f);

	if (TEMPENTITY* puff = gEngfuncs.pEfxAPI->R_TempSprite(position, velocity, scale, model, kRenderTransAlpha, kRenderFxNone,
			TrailAlpha, life + 1, FTENT_SPRANIMATE);
		puff)
	{
		puff->entity.curstate.rendercolor = {170, 170, 170};
		puff->entity.curstate.framerate = 16 / life;
		puff->entity.angles.z = gEngfuncs.pfnRandomFloat(0, 359);
		puff->entity.curstate.fuser1 = gEngfuncs.GetClientTime();
		puff->entity.curstate.fuser2 = life;
		puff->entity.curstate.fuser3 = scale;
		puff->flags |= FTENT_CLIENTCUSTOM;
		puff->callback = SpreadPuff;
	}
}

static std::vector<Rocket> g_Rockets;

static TEMPENTITY* RocketSprite(const char* name, const Vector& origin, float scale)
{
	const int model = gEngfuncs.pEventAPI->EV_FindModelIndex(name);

	if (model == 0)
		return nullptr;

	Vector position = origin;
	Vector still{0, 0, 0};
	return gEngfuncs.pEfxAPI->R_TempSprite(position, still, scale, model, kRenderTransAdd, kRenderFxNone, 1, 1, 0);
}

void RocketGlow(cl_entity_t* rocket, const char* modelname)
{
	// Tagged renderamt 1 burning, 2 burnt out (CRpgRocket::IgniteThink, FollowThink).
	if (!JhlParticleSystemEnabled() || !modelname || 0 != std::strcmp(modelname, "models/rpgrocket.mdl") ||
		(rocket->curstate.renderamt != 1 && rocket->curstate.renderamt != 2))
	{
		return;
	}

	const float now = gEngfuncs.GetClientTime();

	// Those gone (and a new map's clock): their glows die on their own (kept on a frame at a time; by now they may have
	// been freed and their slots used again, so they're left alone).
	std::erase_if(g_Rockets, [&](const Rocket& r)
		{ return now - r.LastSeen > 0.1f || now < r.LastSeen; });

	auto it = std::find_if(g_Rockets.begin(), g_Rockets.end(), [&](const Rocket& r)
		{ return r.Index == rocket->index; });

	if (it == g_Rockets.end())
	{
		g_Rockets.push_back({rocket->index, now, -1, {}, rocket->origin, false});
		it = g_Rockets.end() - 1;
	}

	Rocket& r = *it;
	r.LastSeen = now;

	if (rocket->curstate.renderamt == 2 && r.BurntOut < 0)
		r.BurntOut = now;

	// Its trail, evenly along where it's been since last frame.
	if (!r.Puffed)
	{
		TrailPuff(rocket->origin);
		r.LastPuff = rocket->origin;
		r.Puffed = true;
	}
	else
	{
		const Vector moved = rocket->origin - r.LastPuff;
		float distance = moved.Length();

		if (distance >= TrailSpacing)
		{
			const Vector direction = moved / distance;
			int puffs = 0;

			while (distance >= TrailSpacing && puffs++ < TrailMostAFrame)
			{
				r.LastPuff = r.LastPuff + direction * TrailSpacing;
				distance -= TrailSpacing;
				TrailPuff(r.LastPuff);
			}

			// A hitch: on from where it is.
			if (distance >= TrailSpacing)
				r.LastPuff = rocket->origin;
		}
	}

	// How much is left of it (fading at burnout).
	const float left = r.BurntOut < 0 ? 1 : std::clamp(1 - (now - r.BurntOut) / BurnoutFade, 0.f, 1.f);

	const char* const names[2] = {"sprites/hotglow.spr", "sprites/glow02.spr"};
	const float scales[2] = {RocketGlowScale, RocketCoreScale};

	for (int i = 0; i < 2; ++i)
	{
		TEMPENTITY*& glow = r.Glows[i];

		if (left <= 0)
		{
			if (glow)
			{
				glow->die = now;
				glow = nullptr;
			}

			continue;
		}

		if (!glow)
			glow = RocketSprite(names[i], rocket->origin, scales[i]);

		if (glow)
		{
			glow->entity.origin = rocket->origin;
			glow->entity.curstate.scale = scales[i] * gEngfuncs.pfnRandomFloat(RocketFlickerMin[i], RocketFlickerMax[i]) * std::sqrt(left);
			glow->entity.curstate.renderamt = static_cast<int>(255 * std::min(left * gEngfuncs.pfnRandomFloat(RocketFlickerMin[i], 1), 1.f));
			glow->die = now + 0.1f;
		}
	}

	if (left > 0)
	{
		if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(RocketLightKey + rocket->index); light)
		{
			light->origin = rocket->origin;
			light->radius = RocketLightRadius * left * gEngfuncs.pfnRandomFloat(1 - RocketLightFlicker, 1 + RocketLightFlicker);
			light->color = RocketLightColor;
			light->die = now + 0.05f;
			light->decay = 0;
		}
	}
}

// A thrown hand grenade turns about its model's origin, at its base (it sits on the floor), its middle this far above
// it (w_grenade.mdl, from its vertices): drawn as it is, tumbling it swings round a point under it. While it bounces
// about (MOVETYPE_BOUNCE: thrown, not lying to be picked up) it's drawn moved so its middle is where its origin's
// middle would be unturned: round its middle; lying level it isn't moved at all.
const Vector GrenadeMiddle{0, 0, 6.3f};

void GrenadePivot(cl_entity_t* grenade, const char* modelname)
{
	if (!JhlParticleSystemEnabled() || !modelname || 0 != std::strcmp(modelname, "models/w_grenade.mdl") ||
		grenade->curstate.movetype != MOVETYPE_BOUNCE)
	{
		return;
	}

	// Its turn as the model's drawn (a studio model's pitch is the other way round).
	Vector forward, right, up;
	AngleVectors(Vector{-grenade->angles.x, grenade->angles.y, grenade->angles.z}, forward, right, up);
	const Vector turned = forward * GrenadeMiddle.x - right * GrenadeMiddle.y + up * GrenadeMiddle.z;

	grenade->origin = grenade->origin + GrenadeMiddle - turned;
}

// The spore launcher's light color: its burst sprite's lime (spore_exp_c_01), at full brightness.
constexpr color24 SporeLime{221, 255, 54};

void SporeLight(cl_entity_t* spore, const char* modelname)
{
	// A spore in flight, fired or lobbed, tagged renderamt 1 by the server (CSpore::Spawn).
	if (!JhlParticleSystemEnabled() || !modelname || 0 != std::strcmp(modelname, "models/spore.mdl") ||
		spore->curstate.renderamt != 1)
	{
		return;
	}

	// Kept on a frame at a time, one per spore: enough to glow on the walls it's near, smaller than its burst's.
	if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(SporeLightKey + spore->index); light)
	{
		light->origin = spore->origin;
		light->radius = 120;
		light->color = SporeLime;
		light->die = gEngfuncs.GetClientTime() + 0.05f;
		light->decay = 0;
	}
}

void TripmineLight(cl_entity_t* beam, const char* modelname)
{
	if (!JhlParticleSystemEnabled() || !modelname || 0 != std::strcmp(modelname, "sprites/laserbeam.spr"))
	{
		return;
	}

	const auto& state = beam->curstate;

	// Its laser, a beam between two points: no other beam is this color and brightness.
	if ((state.rendermode & 0x0F) != BEAM_POINTS || state.rendercolor.r != 0 || state.rendercolor.g != 214 ||
		state.rendercolor.b != 198 || state.renderamt != 64)
	{
		return;
	}

	// It starts at the mine's origin, its emitter, 8 units off the wall; the light sits just in front of it.
	const Vector start = state.origin;
	const Vector end = state.angles;
	const Vector direction = (end - start).Normalize();

	// Only mines near the view: the engine has 32 dynamic lights, and one this faint can't be seen from further.
	if ((start - v_origin).Length() > TripmineLightDistance)
	{
		return;
	}

	// Kept on a frame at a time, one per beam (keyed past the entity lights the engine keys by entity index).
	if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(TripmineLightKey + beam->index); light)
	{
		// As faint as the small energy beams' lights where they touch (the vortigaunt's charge): 72 units, the laser's
		// color at 80%.
		light->origin = start + direction * 2;
		light->radius = 72;
		light->color.r = 0;
		light->color.g = 171;
		light->color.b = 158;
		light->die = gEngfuncs.GetClientTime() + 0.05f;
		light->decay = 0;
	}
}

static void MetalImpact(const Vector& position, const Vector& normal)
{
	auto efx = gEngfuncs.pEfxAPI;

	Vector origin = position + normal * 2;

	efx->R_SparkStreaks(origin, gEngfuncs.pfnRandomLong(3, 5), -120, 120);

	// A very short warm flash; MMod's lasts a quarter second with up to twice the radius.
	if (dlight_t* light = efx->CL_AllocDlight(0); light)
	{
		light->origin = origin;
		light->radius = gEngfuncs.pfnRandomFloat(24, 40);
		light->color.r = 255;
		light->color.g = 170;
		light->color.b = 80;
		light->die = gEngfuncs.GetClientTime() + 0.05f;
		light->decay = light->radius * 10;
	}
}

static void DustImpact(const Vector& position, const Vector& normal, const color24& color, float bigger, float alpha = 0.35f)
{
	const int modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/steam1.spr");

	if (modelIndex == 0)
	{
		return;
	}

	Vector origin = position + normal * 4;

	// Drifts away from the wall and slightly up.
	Vector velocity = normal * gEngfuncs.pfnRandomFloat(8, 16);
	velocity.z += gEngfuncs.pfnRandomFloat(2, 6);

	// Plays the sprite once and removes it on its last frame: the frames fade out by themselves, so the length of
	// the puff is (number of frames) / PuffFrameRate. The lifetime is only a cap, kept longer than any animation.
	// No FTENT_FADEOUT: fading keeps a finished sprite on screen while its animation wraps back to frame 0.
	constexpr float PuffFrameRate = 24;

	TEMPENTITY* puff = gEngfuncs.pEfxAPI->R_TempSprite(origin, velocity * bigger, gEngfuncs.pfnRandomFloat(0.25f, 0.35f) * bigger,
		modelIndex, kRenderTransAlpha, kRenderFxNone, alpha, 2.0f, FTENT_SPRANIMATE);

	if (!puff)
	{
		return;
	}

	puff->entity.curstate.rendercolor = color;
	puff->entity.curstate.framerate = PuffFrameRate / std::sqrt(bigger);
	puff->entity.angles.z = gEngfuncs.pfnRandomFloat(0, 359);
}

// A heavy round's impact (the sniper rifle's, jhl_fx_particlesystem): on metal, computers, vents and grates, the hole
// glows (hotglow, orange) a moment, fading out over HotHoleTime, as hot spall; on others, a bigger puff and two small
// chunks of the surface (as an explosion's surface debris, smaller).
constexpr float HotHoleTime = 1;
constexpr float HotHoleScale = 0.096f;

static void HotHole(const Vector& position, const Vector& normal)
{
	const int model = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/hotglow.spr");

	if (model == 0)
		return;

	Vector origin = position + normal;
	Vector still{0, 0, 0};

	if (TEMPENTITY* glow = gEngfuncs.pEfxAPI->R_TempSprite(origin, still, HotHoleScale * gEngfuncs.pfnRandomFloat(0.85f, 1.15f), model,
			kRenderTransAdd, kRenderFxNone, 1, HotHoleTime, FTENT_FADEOUT);
		glow)
	{
		glow->fadeSpeed = 1 / HotHoleTime;
	}
}

static bool IsMetalTexture(char textureType)
{
	return textureType == CHAR_TEX_METAL || textureType == CHAR_TEX_VENT || textureType == CHAR_TEX_GRATE || textureType == CHAR_TEX_COMPUTER;
}

static void HeavyImpact(const Vector& position, const Vector& normal, char textureType)
{
	switch (textureType)
	{
	case CHAR_TEX_METAL:
	case CHAR_TEX_VENT:
	case CHAR_TEX_GRATE:
	case CHAR_TEX_COMPUTER:
		HotHole(position, normal);
		break;

	case CHAR_TEX_DIRT:
		DustImpact(position, normal, {120, 105, 85}, 2, 0.45f);
		JhlExplosion_ImpactChunks(position, normal, textureType, 2);
		break;

	case CHAR_TEX_WOOD:
		DustImpact(position, normal, {130, 115, 95}, 2, 0.45f);
		JhlExplosion_ImpactChunks(position, normal, textureType, 2);
		break;

	case CHAR_TEX_CONCRETE:
	case CHAR_TEX_TILE:
		DustImpact(position, normal, {150, 150, 145}, 2, 0.45f);
		JhlExplosion_ImpactChunks(position, normal, textureType, 2);
		break;

	case CHAR_TEX_GLASS:
		JhlExplosion_ImpactChunks(position, normal, textureType, 2);
		break;

	default:
		break;
	}
}

void BulletImpactEffects(const Vector& position, const Vector& normal, char textureType, JhlImpact kind)
{
	if (0 == g_cl_bulletimpacts->value)
	{
		return;
	}

	if (kind == JhlImpact::Sniper && JhlParticleSystemEnabled())
		HeavyImpact(position, normal, textureType);

	// The revolver's a little heavier than the other guns, and melee blows a little puff (not just on metal): smaller
	// versions of a bullet's puff (and of the sniper rifle's, for the revolver), with jhl_fx_particlesystem.
	if (kind != JhlImpact::Bullet && kind != JhlImpact::Sniper && JhlParticleSystemEnabled())
	{
		const float size = kind == JhlImpact::Revolver ? 1.4f : kind == JhlImpact::Melee ? 0.7f : 0.5f;
		const float alpha = kind == JhlImpact::Revolver ? 0.4f : kind == JhlImpact::Melee ? 0.28f : 0.22f;

		switch (textureType)
		{
		case CHAR_TEX_DIRT:
			DustImpact(position, normal, {120, 105, 85}, size, alpha);
			return;

		case CHAR_TEX_WOOD:
			DustImpact(position, normal, {130, 115, 95}, size, alpha);
			return;

		case CHAR_TEX_CONCRETE:
		case CHAR_TEX_TILE:
			DustImpact(position, normal, {150, 150, 145}, size, alpha);
			return;

		default:
			// Melee sparks on metal as a bullet does (the server sends those only now and then).
			if (kind != JhlImpact::Revolver && !IsMetalTexture(textureType))
				return;
			break;
		}
	}

	switch (textureType)
	{
	case CHAR_TEX_METAL:
	case CHAR_TEX_VENT:
	case CHAR_TEX_GRATE:
	case CHAR_TEX_COMPUTER:
		MetalImpact(position, normal);
		break;

	case CHAR_TEX_DIRT:
		DustImpact(position, normal, {120, 105, 85}, 1);
		break;

	case CHAR_TEX_WOOD:
		DustImpact(position, normal, {130, 115, 95}, 1);
		break;

	case CHAR_TEX_CONCRETE:
	case CHAR_TEX_TILE:
		DustImpact(position, normal, {150, 150, 145}, 1);
		break;

	// Glass, liquids, snow, flesh and Xen surfaces keep the stock effects only.
	default:
		break;
	}
}

static bool IsLiquid(Vector position)
{
	const int contents = gEngfuncs.PM_PointContents(position, nullptr);
	return contents == CONTENTS_WATER || contents == CONTENTS_SLIME;
}

void BulletWaterSplashAlongPath(const Vector& start, const Vector& end)
{
	// Bullet traces pass through water, so a bullet entered it if it ended in liquid but started outside.
	if (!IsLiquid(end) || IsLiquid(start))
	{
		return;
	}

	// Narrow down the surface between the last point outside and the first point inside.
	Vector outside = start;
	Vector inside = end;

	for (int i = 0; i < 12; ++i)
	{
		const Vector middle = (outside + inside) * 0.5f;

		if (IsLiquid(middle))
		{
			inside = middle;
		}
		else
		{
			outside = middle;
		}
	}

	BulletWaterSplash(inside);
}

static TEMPENTITY* SplashSprite(const char* spriteName, Vector origin, float scale, float alpha, int renderMode, float frameRate)
{
	const int modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex(spriteName);

	if (modelIndex == 0)
	{
		return nullptr;
	}

	// Plays once and is removed on its last frame (see DustImpact).
	TEMPENTITY* sprite = gEngfuncs.pEfxAPI->R_TempSprite(origin, vec3_origin, scale, modelIndex, renderMode, kRenderFxNone, alpha, 2.0f, FTENT_SPRANIMATE);

	if (sprite)
	{
		sprite->entity.curstate.framerate = frameRate;
	}

	return sprite;
}

void BulletWaterSplash(const Vector& surfacePosition)
{
	if (0 == g_cl_bulletimpacts->value)
	{
		return;
	}

	// No R_StreakSplash spray: its streaks are opaque, and their only alpha control is the global traceralpha cvar.
	const Vector up{0, 0, 1};

	// Stock droplet spray (grey-white).
	SplashSprite("sprites/wsplash3.spr", surfacePosition + up * 4, gEngfuncs.pfnRandomFloat(0.25f, 0.35f), 0.5f, kRenderTransAdd, 20);

	// Ripple ring lying flat on the surface (MMod's sprite, see mmod_port; skipped if it isn't installed).
	if (TEMPENTITY* ring = SplashSprite("sprites/water_splash_ring.spr", surfacePosition + up, gEngfuncs.pfnRandomFloat(0.35f, 0.5f), 0.3f, kRenderTransAdd, 18); ring)
	{
		ring->entity.angles = {90, 0, 0};
		ring->entity.curstate.rendercolor = {200, 210, 220};
	}
}

static void MsgFunc_WaterSplash(const char* pszName, BufferReader& reader)
{
	BulletWaterSplash(reader.ReadCoordVector());
}

static void MsgFunc_BulletImpact(const char* pszName, BufferReader& reader)
{
	const Vector position = reader.ReadCoordVector();

	Vector normal;
	normal.x = reader.ReadChar() / 127.f;
	normal.y = reader.ReadChar() / 127.f;
	normal.z = reader.ReadChar() / 127.f;

	const char textureType = static_cast<char>(reader.ReadByte());
	const auto kind = static_cast<JhlImpact>(reader.ReadByte());

	BulletImpactEffects(position, normal, textureType, kind);
}

void WeaponEffects_Initialize()
{
	g_cl_bulletimpacts = gEngfuncs.pfnRegisterVariable("jhl_fx_bulletimpacts", "1", FCVAR_ARCHIVE);
	g_cl_muzzleflash = gEngfuncs.pfnRegisterVariable("jhl_fx_muzzleflash", "1", FCVAR_ARCHIVE);

	g_ClientUserMessages.RegisterHandler("BulletHit", &MsgFunc_BulletImpact);
	g_ClientUserMessages.RegisterHandler("WaterSplash", &MsgFunc_WaterSplash);
}
