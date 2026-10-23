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

/**
 *	@brief iuser4 value of a client-side model the renderer should scale by curstate.scale (vanilla ignores model scale,
 *	so only entities marked this way get it).
 */
constexpr int ScaledModelMarker = 0x4A484C; // "JHL"

void ClientGibs_Initialize();

/**
 *	@brief Spawns gibs as the server's ClientGibs message does.
 *	@param flags A GibType combined with GibFlag_* values.
 */
void ClientGibs_Spawn(int entindex, int bloodColor, int numGibs, const Vector& origin, const Vector& angles,
	GibVelocityMultiplier multiplier, int flags);
