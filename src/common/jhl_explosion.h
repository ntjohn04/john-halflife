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
 *	@brief johnhalflife TE_EXPLOSION flag (after the engine's TE_EXPLFLAG_* in const.h): an RPG rocket, which the client
 *	gives a bigger fireball than a grenade of the same damage. The explosion message is the SDK's own, so the engine
 *	never sees it.
 */
constexpr int TE_EXPLFLAG_JHL_ROCKET = 128;

/**
 *	@brief johnhalflife TE_EXPLOSION flag: the explosion throws spark showers, which the client makes (instead of the
 *	server's spark_shower entities) with jhl_fx_particlesystem on.
 */
constexpr int TE_EXPLFLAG_JHL_SPARKS = 64;

/**
 *	@brief johnhalflife TE_EXPLOSION flag: an explosive breakable (barrels, crates, pushables), which the client draws
 *	bigger than its magnitude alone would.
 */
constexpr int TE_EXPLFLAG_JHL_BREAKABLE = 32;

// How much bigger explosive breakables' explosions are drawn (the client's fireballs and lingering smoke, the server's
// smoke puff).
constexpr float JhlBreakableExplosionScale = 1.3f;
