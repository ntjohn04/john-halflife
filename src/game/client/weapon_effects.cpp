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

#include "hud.h"
#include "weapon_effects.h"
#include "Exports.h"
#include "event_api.h"
#include "r_efx.h"
#include "dlight.h"
#include "pm_materials.h"
#include "parsemsg.h"
#include "networking/ClientUserMessages.h"


static cvar_t* g_cl_bulletimpacts = nullptr;
static cvar_t* g_cl_muzzleflash = nullptr;
static cvar_t* g_cl_rpg_muzzle = nullptr; // Temporary, for tuning the RPG's muzzle position.

static bool MuzzleEffectsEnabled()
{
	// In third person the effects would be placed relative to a first person view that isn't there.
	return 0 != g_cl_muzzleflash->value && 0 == CL_IsThirdPerson();
}

static bool IsZoomed()
{
	return gHUD.m_iFOV != 0 && gHUD.m_iFOV < static_cast<int>(CVAR_GET_FLOAT("default_fov"));
}

static void MuzzleEffectsAt(const Vector& position, float wispChance, float wispScale = 1, float wispAlpha = 0.1f);

void EV_GunMuzzleEffects()
{
	// Zoomed in, the view model isn't drawn, so its attachment is stale and the effects would appear where the
	// muzzle last was.
	if (!MuzzleEffectsEnabled() || IsZoomed())
	{
		return;
	}

	cl_entity_t* viewModel = gEngfuncs.GetViewModel();

	if (!viewModel || !viewModel->model)
	{
		return;
	}

	// The muzzle is the first attachment on the stock view models; it's set while the model is drawn.
	Vector muzzle = viewModel->attachment[0];

	if (muzzle == g_vecZero)
	{
		return;
	}

	// Now and then a wisp of smoke; real guns barely smoke.
	MuzzleEffectsAt(muzzle, 0.3f);
}

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

	// A slow, heavy rifle shot: more often leaves a wisp, except when scoped, where it would drift across the
	// magnified view.
	MuzzleEffectsAt(eyePosition + forward * Forward + right * Right + up * Up, IsZoomed() ? 0 : 0.6f);
}

void EV_RpgFireEffects(bool isLocal, const Vector& eyePosition, const Vector& forward, const Vector& right, const Vector& up)
{
	if (0 == g_cl_muzzleflash->value)
	{
		return;
	}

	// The RPG view model has no attachments, so place the front of the tube relative to the view (see EV_SniperMuzzleEffects).
	if (isLocal && MuzzleEffectsEnabled())
	{
		Vector offset{36, 8, -6};
		sscanf(g_cl_rpg_muzzle->string, "%f %f %f", &offset.x, &offset.y, &offset.z);

		// Always a wisp, twice the size and opacity of a gun's.
		MuzzleEffectsAt(eyePosition + forward * offset.x + right * offset.y + up * offset.z, 1, 2, 0.2f);
	}

	// Backblast out of the rear of the tube, over the right shoulder. Placed from the player rather than the view
	// model, so other players and third person see it too.
	const Vector rear = eyePosition - forward * 24 + right * 8 - up * 4;

	if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(0); light)
	{
		light->origin = rear;
		light->radius = gEngfuncs.pfnRandomFloat(160, 200);
		light->color.r = 255;
		light->color.g = 170;
		light->color.b = 90;
		light->die = gEngfuncs.GetClientTime() + 0.1f;
		light->decay = light->radius * 8;
	}

	const int modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/steam1.spr");

	if (modelIndex == 0)
	{
		return;
	}

	for (int i = 0; i < 6; ++i)
	{
		Vector origin = rear - forward * gEngfuncs.pfnRandomFloat(0, 12);
		Vector velocity = forward * -gEngfuncs.pfnRandomFloat(90, 160) + right * gEngfuncs.pfnRandomFloat(-40, 40) + up * gEngfuncs.pfnRandomFloat(-10, 25);

		// Plays once and is removed on its last frame (see DustImpact); a slower frame rate lets the plume linger.
		TEMPENTITY* puff = gEngfuncs.pEfxAPI->R_TempSprite(origin, velocity, gEngfuncs.pfnRandomFloat(0.5f, 0.8f),
			modelIndex, kRenderTransAlpha, kRenderFxNone, 0.3f, 3.0f, FTENT_SPRANIMATE);

		if (puff)
		{
			puff->entity.curstate.rendercolor = {170, 170, 170};
			puff->entity.curstate.framerate = 14;
			puff->entity.angles.z = gEngfuncs.pfnRandomFloat(0, 359);
		}
	}
}

static void MuzzleEffectsAt(const Vector& position, float wispChance, float wispScale, float wispAlpha)
{
	Vector muzzle = position;

	// A warm flash on top of the engine's own muzzle light, making it stronger and slightly longer.
	if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(0); light)
	{
		light->origin = muzzle;
		light->radius = gEngfuncs.pfnRandomFloat(140, 170);
		light->color.r = 255;
		light->color.g = 190;
		light->color.b = 110;
		light->die = gEngfuncs.GetClientTime() + 0.06f;
		light->decay = light->radius * 12;
	}

	// A faint wisp of smoke drifting up from the muzzle.
	if (gEngfuncs.pfnRandomFloat(0, 1) >= wispChance)
	{
		return;
	}

	const int modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/steam1.spr");

	if (modelIndex == 0)
	{
		return;
	}

	Vector velocity{gEngfuncs.pfnRandomFloat(-4, 4), gEngfuncs.pfnRandomFloat(-4, 4), gEngfuncs.pfnRandomFloat(12, 20)};

	// Plays once and is removed on its last frame (see DustImpact).
	TEMPENTITY* wisp = gEngfuncs.pEfxAPI->R_TempSprite(muzzle, velocity, gEngfuncs.pfnRandomFloat(0.08f, 0.12f) * wispScale,
		modelIndex, kRenderTransAlpha, kRenderFxNone, wispAlpha, 2.0f, FTENT_SPRANIMATE);

	if (wisp)
	{
		wisp->entity.curstate.rendercolor = {190, 190, 190};
		wisp->entity.curstate.framerate = 28;
		wisp->entity.angles.z = gEngfuncs.pfnRandomFloat(0, 359);
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

static void DustImpact(const Vector& position, const Vector& normal, const color24& color)
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

	TEMPENTITY* puff = gEngfuncs.pEfxAPI->R_TempSprite(origin, velocity, gEngfuncs.pfnRandomFloat(0.25f, 0.35f),
		modelIndex, kRenderTransAlpha, kRenderFxNone, 0.35f, 2.0f, FTENT_SPRANIMATE);

	if (!puff)
	{
		return;
	}

	puff->entity.curstate.rendercolor = color;
	puff->entity.curstate.framerate = PuffFrameRate;
	puff->entity.angles.z = gEngfuncs.pfnRandomFloat(0, 359);
}

void BulletImpactEffects(const Vector& position, const Vector& normal, char textureType)
{
	if (0 == g_cl_bulletimpacts->value)
	{
		return;
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
		DustImpact(position, normal, {120, 105, 85});
		break;

	case CHAR_TEX_WOOD:
		DustImpact(position, normal, {130, 115, 95});
		break;

	case CHAR_TEX_CONCRETE:
	case CHAR_TEX_TILE:
		DustImpact(position, normal, {150, 150, 145});
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

	BulletImpactEffects(position, normal, textureType);
}

void WeaponEffects_Initialize()
{
	g_cl_bulletimpacts = gEngfuncs.pfnRegisterVariable("jhl_fx_bulletimpacts", "1", FCVAR_ARCHIVE);
	g_cl_muzzleflash = gEngfuncs.pfnRegisterVariable("jhl_fx_muzzleflash", "1", FCVAR_ARCHIVE);
	g_cl_rpg_muzzle = gEngfuncs.pfnRegisterVariable("jhl_fx_rpg_muzzle", "36 8 -6", 0);

	g_ClientUserMessages.RegisterHandler("BulletHit", &MsgFunc_BulletImpact);
	g_ClientUserMessages.RegisterHandler("WaterSplash", &MsgFunc_WaterSplash);
}
