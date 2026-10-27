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

// johnhalflife's barnacle grapple (jhl_fx_particlesystem): its tongue, drawn here instead of the server's beam
// (CGrapple::CreateEffect): a little slack flying out, pulling taut once latched on, and reeling back into the gun when
// it lets go (a miss, or fire let go of) instead of vanishing.

struct cl_entity_t;

/**
 *	@brief An entity being added this frame (HUD_AddEntity): records where a grapple's tip is, and whether it's latched
 *	on.
 */
void JhlGrapple_Track(cl_entity_t* entity, const char* modelname);

/**
 *	@brief Draws the tongues, and reels in those of tips gone (HUD_TempEntUpdate).
 */
void JhlGrapple_Update(double time);

/**
 *	@brief Forgets them: a new map frees the engine's beams (HUD_VidInit).
 */
void JhlGrapple_Reset();
