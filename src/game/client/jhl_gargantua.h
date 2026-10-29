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

class BufferReader;
struct cl_entity_t;

/**
 *	@brief johnhalflife: a gargantua's flames this moment (CGargantua::FlameUpdate; the SDK's own temp entity
 *	message's TE_FIREFIELD): where each hand's lands and whether it hit something. Embers and heat smoke where they
 *	do; the lights are kept on it every frame (JhlGargantua_Track).
 */
void JhlGargantua_ParseFlame(BufferReader& reader);

/**
 *	@brief Its flames' flickering lights, on its hands and where they land (HUD_AddEntity).
 */
void JhlGargantua_Track(cl_entity_t* ent, const char* modelname);

/**
 *	@brief Its flames' cores and hand lights onto its hands as just drawn, its attachments just worked out
 *	(CStudioModelRenderer::StudioDrawModel): they aren't yet when it's added (HUD_AddEntity).
 */
void JhlGargantua_Drawn(cl_entity_t* ent);

/**
 *	@brief johnhalflife: the pit worm's eye lit, as just drawn (CStudioModelRenderer::StudioDrawModel): dim while open,
 *	out as it blinks, blazing as it fires.
 */
void JhlPitWorm_Drawn(cl_entity_t* ent);

/**
 *	@brief johnhalflife: the stretch a gargantua's stomp covered since it was last moved (CStomp::Think; TE_LAVASPLASH):
 *	its head is drawn moving along it smoothly, a light low over the floor, and the ground's torn up along it.
 */
void JhlGargantua_ParseStomp(BufferReader& reader);

/**
 *	@brief Moves the stomps' heads on, kicking up the ground (HUD_TempEntUpdate).
 */
void JhlGargantua_Update(double time);

/**
 *	@brief Forgets them (HUD_VidInit).
 */
void JhlGargantua_Reset();
