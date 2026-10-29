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

class BufferReader;

/**
 *	@brief A spark burst, and spark showers as explosions throw them going up from a point (UTIL_JhlSparkShowers:
 *	UTIL_Sparks, broken computers): the SDK's own temp entity message's TE_SPARKS, the position and then how many
 *	showers.
 */
void JhlExplosion_ParseSparkShowers(BufferReader& reader);
/**
 *	@brief A few sparks with an armor ricochet's flash (UTIL_Ricochet's @p scale, 0.5-2).
 */
void JhlExplosion_RicochetSparks(const Vector& position, float scale);

/**
 *	@brief @p count small chunks of a surface of @p material thrown off where a heavy round hit it (as an explosion's
 *	surface debris, smaller), off @p normal.
 */
void JhlExplosion_ImpactChunks(const Vector& position, const Vector& normal, char material, int count);

/**
 *	@brief The floor under @p point torn up by something racing along it going @p direction (a gargantua's stomp):
 *	dust of the floor's color thrown off each side, now and then a chunk of it, or sparks off metal.
 */
void JhlExplosion_GroundKick(const Vector& point, const Vector& direction);

/**
 *	@brief A tentacle striking the floor under @p position (CTentacle::HandleAnimEvent; TE_LARGEFUNNEL), by the floor's
 *	material: @p strength 0 a light tap, 1 a slam, 2 its killing swing's.
 */
void JhlExplosion_TentacleImpact(const Vector& position, int strength);

/**
 *	@brief A big, faint smoke cloud that lingers after an explosion's vanilla smoke (steam1, which the server sends
 *	0.3 s in: 20 units up, rising 30 units/s for 1.3 s, about fireball scale × 43 units wide). Widths are per unit of
 *	the fireball's scale; opacity and gray of 255.
 */
struct JhlLingeringSmoke
{
	float Delay = 0.4f;
	float LifeMin = 9;
	float LifeMax = 12;
	float StartWidth = 85; // 2× the vanilla smoke.
	float EndWidth = 340;
	float RiseMin = 8;
	float RiseMax = 12;
	float Drift = 40; // Random each way, plus this much away from a nearby wall; dies down over its life.
	int Opacity = 200; // Peak, right after it fades in; it thins out from there as it spreads.
	int GrayMin = 70;
	int GrayMax = 80;
};

/**
 *	@return false if the smoke sprite isn't loaded (it's precached in W_Precache, so a map loaded before that change
 *	doesn't have it).
 */
bool JhlExplosion_LingeringSmoke(const Vector& origin, float scale, const JhlLingeringSmoke& settings = {});

/**
 *	@brief The engine's blob explosion (R_BlobExplosion, Quake's tarbaby's) with @p count particles (the engine's: 1024).
 */
void JhlExplosion_Blob(const Vector& origin, int count);

struct TEMPENTITY;

/**
 *	@brief Plays a temp entity sprite's frames once over @p life seconds (not looping, not dying early), fading out
 *	over the last @p fade of it (0-1), from its baseline.renderamt.
 */
void JhlSprite_PlayOnce(TEMPENTITY* sprite, float life, float fade);

/**
 *	@brief Low dust puffs kicked out along the floor (a houndeye's blast): the SDK's own temp entity message's TE_SMOKE,
 *	the position on the floor and how many.
 */
void JhlExplosion_ParseGroundDust(BufferReader& reader);

/**
 *	@brief A few specks of a displacer ball left falling where it went (UTIL_JhlDisplacerSpecks): the SDK's own temp
 *	entity message's TE_SPRITETRAIL, the position.
 */
void JhlExplosion_ParseDisplacerSpecks(BufferReader& reader);
