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
