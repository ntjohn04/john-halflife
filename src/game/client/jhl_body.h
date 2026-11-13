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

#include <cstddef>
#include <cstring>

struct cl_entity_t;
struct playermove_t;
struct studiohdr_t;

constexpr int JhlBodyMarker = 0x4A4842; // "JHB"

inline const char* JhlBoneName(const char* name, char (&out)[32])
{
	static constexpr const char* Names[][2] = {{"Thigh", "Leg"}, {"Calf", "Leg1"}, {"Clavicle", "Arm"}, {"UpperArm", "Arm1"}, {"Forearm", "Arm2"}};
	const char* last = std::strrchr(name, ' ');

	if (!last)
		return name;

	for (const auto& names : Names)
	{
		const std::size_t prefix = static_cast<std::size_t>(last + 1 - name);

		if (0 == std::strcmp(last + 1, names[0]) && prefix + std::strlen(names[1]) < sizeof(out))
		{
			std::memcpy(out, name, prefix);
			std::strcpy(out + prefix, names[1]);
			return out;
		}
	}

	return name;
}

void JhlBody_Initialize();

void JhlBody_Moved(const playermove_t* move);

void JhlBody_CreateEntities();

bool JhlBody_Place(cl_entity_t* entity, const float* viewOrigin);

void JhlBody_Pose(const cl_entity_t* entity, const studiohdr_t* header, float (*bones)[3][4], float (*lightBones)[3][4]);

int JhlBody_HandsModel();

void JhlBody_Reset();

void JhlBody_Armor(int armor);
void JhlBody_ItemPickedUp(const char* className);
