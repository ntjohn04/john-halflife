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

#include "vector.h"

/**
 *	@brief Registers the BulletHit message, which the server sends for bullets it fires (NPCs, mounted guns).
 */
void BulletImpacts_Initialize();

/**
 *	@brief Subtle extra feedback where a bullet hits a surface, on top of the usual decal, specks and ricochet:
 *	sparks and a brief flash on metal, a small dust puff on stone, dirt and wood. Uses only stock sprites.
 *	@param textureType Material of the surface hit (CHAR_TEX_*).
 */
void BulletImpactEffects(const Vector& position, const Vector& normal, char textureType);
