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

bool JhlExplosion_Fireballs(const Vector& origin, int model, float scale, float framerate, int flags);

class BufferReader;

void JhlExplosion_ParseSparkShowers(BufferReader& reader);
void JhlExplosion_RicochetSparks(const Vector& position, float scale);

void JhlExplosion_ImpactChunks(const Vector& position, const Vector& normal, char material, int count);

void JhlExplosion_GroundKick(const Vector& point, const Vector& direction);

void JhlExplosion_TentacleImpact(const Vector& position, int strength);

void JhlExplosion_ParseBiteBubbles(BufferReader& reader);

void JhlExplosion_ParseMortarIncoming(BufferReader& reader);

struct JhlLingeringSmoke
{
	float Delay = 0.4f;
	float LifeMin = 9;
	float LifeMax = 12;
	float StartWidth = 85; // 2× the vanilla smoke.
	float EndWidth = 340;
	float RiseMin = 8;
	float RiseMax = 12;
	float Drift = 40;
	int Opacity = 200;
	int GrayMin = 70;
	int GrayMax = 80;
};

bool JhlExplosion_LingeringSmoke(const Vector& origin, float scale, const JhlLingeringSmoke& settings = {});

void JhlExplosion_Blob(const Vector& origin, int count);

struct TEMPENTITY;

void JhlSprite_PlayOnce(TEMPENTITY* sprite, float life, float fade);

void JhlExplosion_ParseGroundDust(BufferReader& reader);

void JhlExplosion_ParseDisplacerSpecks(BufferReader& reader);
