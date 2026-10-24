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

#include <deque>

#include "hud.h"
#include "com_model.h"
#include "event_api.h"
#include "jhl_debris.h"
#include "parsemsg.h"
#include "r_efx.h"
#include "r_studioint.h"
#include "studio.h"

extern engine_studio_api_t IEngineStudio;

namespace
{
// Pieces of long-lived debris at once; the oldest fade past this (the engine has 500 temp entities for everything).
constexpr std::size_t DebrisBudget = 250;

// The most shards one breakable makes (the engine's own cap is 100).
constexpr int MaxShards = 200;
// A tracked piece: the temp entity and the serial number it was tagged with (curstate.iuser3), which tells whether
// the temp entity is still that piece or has been reused for something else.
struct TrackedDebris
{
	TEMPENTITY* Ent;
	int Serial;
	void (*Expire)(TEMPENTITY*);
};

std::deque<TrackedDebris> g_Debris;
int g_NextSerial = 1;

// Gib models pick a body, sprites a frame: how many there are.
int Variations(model_t* model)
{
	if (model->type == mod_sprite)
		return std::max(1, model->numframes);

	if (model->type != mod_studio)
		return 1;

	const auto header = static_cast<studiohdr_t*>(IEngineStudio.Mod_Extradata(model));

	if (!header)
		return 1;

	const auto parts = reinterpret_cast<mstudiobodyparts_t*>(reinterpret_cast<byte*>(header) + header->bodypartindex);
	int count = 1;

	for (int i = 0; i < header->numbodyparts; ++i)
		count *= std::max(1, parts[i].nummodels);

	return count;
}

void ExpireShard(TEMPENTITY* ent)
{
	// FTENT_FADEOUT fades it out from here.
	ent->die = gEngfuncs.GetClientTime();
}

} // namespace

void JhlDebris_Track(TEMPENTITY* ent, void (*expire)(TEMPENTITY* ent))
{
	const int serial = g_NextSerial++;
	ent->entity.curstate.iuser3 = serial;
	g_Debris.push_back({ent, serial, expire});

	while (g_Debris.size() > DebrisBudget)
	{
		const TrackedDebris oldest = g_Debris.front();
		g_Debris.pop_front();

		if (oldest.Ent->entity.curstate.iuser3 == oldest.Serial)
			oldest.Expire(oldest.Ent);
	}
}

void JhlDebris_ParseBreakModel(BufferReader& reader)
{
	const Vector position = reader.ReadCoordVector();
	const Vector size = reader.ReadCoordVector();
	const Vector velocity = reader.ReadCoordVector();
	const float random = reader.ReadByte() * 10.f;
	const int modelIndex = reader.ReadShort();
	int count = reader.ReadByte();
	reader.ReadByte(); // Life: JhlDebrisLife instead.
	const int flags = reader.ReadByte();
	const int multiplier = std::max(1, reader.ReadByte());

	// As the engine's R_BreakModel: no count means one by the size (its surface area over 432), at most 100.
	if (count == 0)
		count = std::min(100, static_cast<int>((size.x * size.y + size.y * size.z + size.z * size.x) / 432));

	JhlDebris_BreakModel(position, size, velocity, random, std::min(MaxShards, count * multiplier),
		gEngfuncs.hudGetModelByIndex(modelIndex), flags, JhlDebrisLife);
}

model_t* JhlDebris_LoadModel(const char* name)
{
	if (const int index = gEngfuncs.pEventAPI->EV_FindModelIndex(name); index != 0)
		return gEngfuncs.hudGetModelByIndex(index);

	return IEngineStudio.Mod_ForName(name, 0);
}

void JhlDebris_BreakModel(const Vector& position, const Vector& size, const Vector& velocity, float random, int count,
	model_t* model, int flags, float life)
{
	if (!model)
		return;

	for (int i = 0; i < count; ++i)
	{
		const Vector origin{position.x + gEngfuncs.pfnRandomFloat(-0.5f, 0.5f) * size.x,
			position.y + gEngfuncs.pfnRandomFloat(-0.5f, 0.5f) * size.y,
			position.z + gEngfuncs.pfnRandomFloat(-0.5f, 0.5f) * size.z};

		const Vector shardVelocity{velocity.x + gEngfuncs.pfnRandomFloat(-random, random),
			velocity.y + gEngfuncs.pfnRandomFloat(-random, random), velocity.z + gEngfuncs.pfnRandomFloat(0, random)};

		if (!JhlDebris_Piece(origin, model, flags, shardVelocity, life + gEngfuncs.pfnRandomFloat(0, 1)))
			return;
	}
}

TEMPENTITY* JhlDebris_Piece(const Vector& origin, model_t* model, int flags, const Vector& velocity, float life)
{
	TEMPENTITY* shard = gEngfuncs.pEfxAPI->CL_TempEntAlloc(origin, model);

	if (!shard)
		return nullptr;

	const int type = flags & BREAK_TYPEMASK;
	const int variations = Variations(model);

	shard->hitSound = type;

	if (model->type == mod_sprite)
		shard->entity.curstate.frame = gEngfuncs.pfnRandomLong(0, variations - 1);
	else
		shard->entity.curstate.body = gEngfuncs.pfnRandomLong(0, variations - 1);

	shard->flags |= FTENT_COLLIDEWORLD | FTENT_FADEOUT | FTENT_SLOWGRAVITY;

	if (gEngfuncs.pfnRandomLong(0, 255) < 200)
	{
		shard->flags |= FTENT_ROTATE;
		shard->entity.baseline.angles = {gEngfuncs.pfnRandomFloat(-256, 255), gEngfuncs.pfnRandomFloat(-256, 255),
			gEngfuncs.pfnRandomFloat(-256, 255)};
	}

	if (gEngfuncs.pfnRandomLong(0, 255) < 100 && (flags & BREAK_SMOKE) != 0)
		shard->flags |= FTENT_SMOKETRAIL;

	if (type == BREAK_GLASS || (flags & BREAK_TRANS) != 0)
	{
		shard->entity.curstate.rendermode = kRenderTransTexture;
		shard->entity.curstate.renderamt = shard->entity.baseline.renderamt = 128;
	}
	else
	{
		shard->entity.curstate.rendermode = kRenderNormal;
		shard->entity.baseline.renderamt = 255;
	}

	shard->entity.baseline.origin = velocity;
	shard->die = gEngfuncs.GetClientTime() + life;

	JhlDebris_Track(shard, ExpireShard);
	return shard;
}
