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

struct JhlTracerStyle
{
	Vector Color{255, 60, 30};
	float Brightness = 1;
	float Width = 1.2f;
	float Length = 200;
	float Speed = 4500;
	float StartOffsetMax = 120;

	Vector CoreColor{255, 225, 180};
	float CoreBrightness = 1;
	float CoreWidth = 1;
	float CoreLength = 0;

	bool ImpactSpeck = false;

	float StartOffsetMin = 0;
};

enum class JhlTracerKind
{
	MP5 = 0,
	Glock,	 // 9mm pistols.
	Heavy,
	Sniper,
	Shotgun,
	M249,
};

const JhlTracerStyle& JhlTracers_Style(JhlTracerKind kind);

bool JhlTracersEnabled();

class BufferReader;

void JhlTracers_ParseNpcTracer(BufferReader& reader);

Vector JhlTracers_MuzzleStart(int shooter, const Vector& start);

void JhlTracer_Fire(const Vector& start, const Vector& end, const JhlTracerStyle& style);

void JhlSniperTrace_Fire(const Vector& start, const Vector& end, bool scoped);

void JhlTracers_Update(double time);

void JhlTracers_Reset();
