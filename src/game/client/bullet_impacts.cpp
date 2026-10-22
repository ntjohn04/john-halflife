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

// A reserved take on MMod's bullet impact effects (see re/specs/bullet_impacts.md):
// MMod adds debris, glowing sprites and per-material decals; this keeps to engine particles,
// a short light and one stock smoke sprite.

#include "hud.h"
#include "bullet_impacts.h"
#include "event_api.h"
#include "r_efx.h"
#include "dlight.h"
#include "pm_materials.h"
#include "parsemsg.h"
#include "networking/ClientUserMessages.h"

static cvar_t* g_cl_bulletimpacts = nullptr;

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
	// Our own copy of the stock steam sprite, trimmed to a single short puff (source in assetsrc/sprites).
	int modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/jhl_impact_puff.spr");

	if (modelIndex == 0)
	{
		modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/steam1.spr");

		if (modelIndex == 0)
		{
			return;
		}
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

void BulletImpacts_Initialize()
{
	g_cl_bulletimpacts = gEngfuncs.pfnRegisterVariable("jhl_fx_bulletimpacts", "1", FCVAR_ARCHIVE);

	g_ClientUserMessages.RegisterHandler("BulletHit", &MsgFunc_BulletImpact);
}
