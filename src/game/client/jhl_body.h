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

// johnhalflife's first-person body (jhl_visuals_firstpersonbody): the player's own model seen looking down, purely
// drawn: the player's movement (as just predicted) picks its pose, nothing goes back. Its own entity, added each frame;
// with the setting off it's never added, and nothing else changes.

struct cl_entity_t;
struct playermove_t;
struct studiohdr_t;

/**
 *	@brief The body's entity's iuser4, for the studio renderer (JhlBody_Place, JhlBody_Pose). A pose taken from another
 *	model (CStudioModelRenderer::JhlSetupBorrowedBones): its model iuser1, its sequence iuser2 less one, its frame
 *	fuser3; the whole pose, its origin fuser4 above the feet (with playerclass 2 its root as standing, without its
 *	motion), or with playerclass 1 only the upper body.
 */
constexpr int JhlBodyMarker = 0x4A4842; // "JHB"

/**
 *	@brief A biped bone's name in the older of 3ds Max Biped's two namings, the player models': the newer's Thigh, Calf,
 *	Clavicle, UpperArm and Forearm (Opposing Force's recruit) are their Leg, Leg1, Arm, Arm1 and Arm2. Others as they are.
 */
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

/**
 *	@brief Registers jhl_visuals_firstpersonbody.
 */
void JhlBody_Initialize();

/**
 *	@brief The player's movement just predicted (HUD_PlayerMove): the last of a frame is the body's state.
 */
void JhlBody_Moved(const playermove_t* move);

/**
 *	@brief Adds the body for this frame, posed for the player's movement (HUD_CreateEntities).
 */
void JhlBody_CreateEntities();

/**
 *	@brief Puts the body on the player's feet, before it's drawn; false to leave it out, the camera it's drawn from
 *	(@p viewOrigin) somewhere else (a cutscene's, third person).
 */
bool JhlBody_Place(cl_entity_t* entity, const float* viewOrigin);

/**
 *	@brief Hides its head once posed, its arms while there's a view model (its arms are the player's), and its back
 *	above its iuser3 spine bones.
 */
void JhlBody_Pose(const cl_entity_t* entity, const studiohdr_t* header, float (*bones)[3][4], float (*lightBones)[3][4]);

/**
 *	@brief Forgets the last map's movement (HUD_VidInit).
 */
void JhlBody_Reset();

/**
 *	@brief The armor the HUD was just told (its Battery message), and an item just picked up (ItemPickup's class name):
 *	whether Barney wears his vest (Blue Shift's armor), drawn in it or in his shirt.
 */
void JhlBody_Armor(int armor);
void JhlBody_ItemPickedUp(const char* className);
