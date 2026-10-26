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

// johnhalflife's shock rifle wake (jhl_fx_particlesystem): behind each bolt, a smooth purplish strand along the path of
// its glow (its centre) and a jagged cyan one along the path of its crackle's end (its model's first attachment, 40
// units to its side), which the bolt's spin (CShockBeam::CreateShockBeam) winds around it.

struct cl_entity_t;

/**
 *	@brief An entity being added this frame (HUD_AddEntity): records where a shock bolt's two points are.
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
