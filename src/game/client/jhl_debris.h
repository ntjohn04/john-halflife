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

#pragma once

class BufferReader;
struct TEMPENTITY;
struct model_t;

constexpr float JhlDebrisLife = 60;

void JhlDebris_Track(TEMPENTITY* ent, void (*expire)(TEMPENTITY* ent));

void JhlDebris_ParseBreakModel(BufferReader& reader);

model_t* JhlDebris_LoadModel(const char* name);

void JhlDebris_BreakModel(const Vector& position, const Vector& size, const Vector& velocity, float random, int count,
	model_t* model, int flags, float life);

TEMPENTITY* JhlDebris_Piece(const Vector& origin, model_t* model, int flags, const Vector& velocity, float life);
