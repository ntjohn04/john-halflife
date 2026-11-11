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

struct cl_entity_t;

/**
 *	@brief johnhalflife's displacer (jhl_fx_particlesystem): for the local player, its charge seen at the muzzle each
 *	time it fires (a ball of its portal sprite and its light, growing over the spin-up), and for every ball a light
 *	going with it.
 */

/**
 *	@brief The local player's displacer started spinning up (EV_FireDisplacer).
 */
void JhlDisplacer_SpinUp();

/**
 *	@brief The local player's displacer fired (EV_FireDisplacer): its charge goes out.
 */
void JhlDisplacer_Fired();

/**
 *	@brief An entity about to be drawn (HUD_AddEntity): a displacer ball gets its light.
 */
void JhlDisplacer_Track(cl_entity_t* entity, const char* modelname);

/**
 *	@brief Each frame (HUD_TempEntUpdate): the charge.
 */
void JhlDisplacer_Update(double time);

/**
 *	@brief A new map.
 */
void JhlDisplacer_Reset();
