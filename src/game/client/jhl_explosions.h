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
 *	@brief Draws johnhalflife's fireballs for a TE_EXPLOSION (jhl_fx_particlesystem): the vanilla fireball with a little
 *	randomness, and a second one from a pool of vanilla explosion sprites.
 *	@return false if this explosion keeps the vanilla fireball (the caller draws it).
 */
bool JhlExplosion_Fireballs(const Vector& origin, int model, float scale, float framerate, int flags);

/**
 *	@brief A big, faint smoke cloud that lingers after an explosion's vanilla smoke (steam1, which the server sends
 *	0.3 s in: 20 units up, rising 30 units/s for 1.3 s, about fireball scale × 43 units wide). Widths are per unit of
 *	the fireball's scale; opacity and gray of 255.
 */
struct JhlLingeringSmoke
{
	float Delay = 0.4f;
	float LifeMin = 5;
	float LifeMax = 7;
	float StartWidth = 64; // 1.5× the vanilla smoke.
	float EndWidth = 128;
	float RiseMin = 8;
	float RiseMax = 12;
	float Drift = 5;
	int Opacity = 64; // Peak.
	int GrayMin = 30;
	int GrayMax = 40;
};

/**
 *	@return false if the smoke sprite isn't loaded (it's precached in W_Precache, so a map loaded before that change
 *	doesn't have it).
 */
bool JhlExplosion_LingeringSmoke(const Vector& origin, float scale, const JhlLingeringSmoke& settings = {});
