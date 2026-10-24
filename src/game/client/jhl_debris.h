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

// How long johnhalflife's debris (gibs, breakable shards) lies around before fading, in seconds.
constexpr float JhlDebrisLife = 60;

/**
 *	@brief Counts a long-lived piece of debris (gibs, breakable shards) against a shared budget. The engine has 500 temp
 *	entities for every effect, so past the budget the oldest pieces are made to fade now, before effects stop
 *	appearing.
 *	@param expire Makes a piece fade out (it's only called while the piece is still the one tracked).
 */
void JhlDebris_Track(TEMPENTITY* ent, void (*expire)(TEMPENTITY* ent));

/**
 *	@brief The server's TE_BREAKMODEL sent as the SDK's own temp entity message (UTIL_JhlBreakModel): the engine's
 *	breakable shards, made here so they can last JhlDebrisLife, be more and count against the budget. The same data
 *	as TE_BREAKMODEL, then a byte: how many times the engine's shard count to make.
 */
void JhlDebris_ParseBreakModel(BufferReader& reader);
