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

// johnhalflife's hornet trails (jhl_fx_particlesystem), drawn here instead of the engine's follow beam: a twirl around
// each hornet's path while it hunts, in moods (calm, its own, a flourish), straight once it commits to a strike. The
// server tags them (CHornet::IgniteTrail): rendercolor its trail's colour, renderamt 1 hunting, 2 committed.

struct cl_entity_t;

/**
 *	@brief A hornet being added this frame (HUD_AddEntity): records where it is.
 */
void JhlHornets_Track(cl_entity_t* hornet, const char* modelname);

/**
 *	@brief Draws the trails (HUD_TempEntUpdate).
 */
void JhlHornets_Update(double time);

/**
 *	@brief Forgets the trails: a new map frees the engine's beams (HUD_VidInit).
 */
void JhlHornets_Reset();
