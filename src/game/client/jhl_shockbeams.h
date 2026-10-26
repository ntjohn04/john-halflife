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

// johnhalflife's shock rifle bolt (jhl_fx_particlesystem): its glow (flare3, as vanilla's) and a blue spark (XSpark1)
// circling its path 180 degrees apart (the server leaves out its own glow and crackle, CShockBeam::Spawn), and behind
// them their wakes, a double helix: the glow's smooth and purplish, the spark's jagged and cyan.

struct cl_entity_t;

/**
 *	@brief An entity being added this frame (HUD_AddEntity): places a shock bolt's glow and spark and records where they
 *	are.
 */
void JhlShockBeams_Track(cl_entity_t* entity, const char* modelname);

/**
 *	@brief Draws the wakes (HUD_TempEntUpdate).
 */
void JhlShockBeams_Update(double time);

/**
 *	@brief Forgets the wakes: a new map frees the engine's beams (HUD_VidInit).
 */
void JhlShockBeams_Reset();
