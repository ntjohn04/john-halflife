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
 *	@brief Registers the effect cvars and the BulletHit and WaterSplash messages, which the server sends for
 *	bullets it fires (NPCs, mounted guns).
 */
void WeaponEffects_Initialize();

/**
 *	@brief A stronger, warmer muzzle light and a faint smoke wisp at the local player's view model muzzle.
 *	Call alongside EV_MuzzleFlash for firearms.
 */
void EV_GunMuzzleEffects();

/**
 *	@brief EV_GunMuzzleEffects for the sniper rifle, whose view model attachment isn't at its muzzle:
 *	the muzzle is placed relative to the view instead, and more often leaves a wisp.
 */
void EV_SniperMuzzleEffects(const Vector& eyePosition, const Vector& forward, const Vector& right, const Vector& up);

/**
 *	@brief RPG launch: a wisp and light at the front of the tube (local player only) and a backblast of smoke and
 *	light behind the shooter's shoulder (everyone).
 */
void EV_RpgFireEffects(bool isLocal, const Vector& eyePosition, const Vector& forward, const Vector& right, const Vector& up);

/**
 *	@brief Subtle extra feedback where a bullet hits a surface, on top of the usual decal, specks and ricochet:
 *	sparks and a brief flash on metal, a small dust puff on stone, dirt and wood. Uses only stock sprites.
 *	@param textureType Material of the surface hit (CHAR_TEX_*).
 */
void BulletImpactEffects(const Vector& position, const Vector& normal, char textureType);

/**
 *	@brief Adds a splash where a bullet travelling from @p start to @p end enters water or slime, if it does.
 */
void BulletWaterSplashAlongPath(const Vector& start, const Vector& end);

/**
 *	@brief A small splash at a point on a water surface: a droplet sprite and a ripple ring.
 */
void BulletWaterSplash(const Vector& surfacePosition);
