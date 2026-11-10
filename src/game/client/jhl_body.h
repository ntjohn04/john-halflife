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

// johnhalflife's first-person body (jhl_visuals_firstpersonbody): the player's own model seen looking down, purely
// drawn: the player's movement (as just predicted) picks its pose, nothing goes back. Its own entity, added each frame;
// with the setting off it's never added, and nothing else changes.

struct cl_entity_t;
struct playermove_t;
struct studiohdr_t;

/**
 *	@brief The body's entity's iuser4, for the studio renderer (JhlBody_Place, JhlBody_Pose).
 */
constexpr int JhlBodyMarker = 0x4A4842; // "JHB"

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
 *	@brief Puts the body under the camera being drawn from (@p viewOrigin), before it's drawn; false to leave it out
 *	(the camera somewhere else: a cutscene's, third person).
 */
bool JhlBody_Place(cl_entity_t* entity, const float* viewOrigin);

/**
 *	@brief Shrinks its neck, head and arms to nothing once posed: the view model's arms are the player's.
 */
void JhlBody_Pose(const studiohdr_t* header, float (*bones)[3][4], float (*lightBones)[3][4]);

/**
 *	@brief Forgets the last map's movement (HUD_VidInit).
 */
void JhlBody_Reset();
