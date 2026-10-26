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

/**
 *	@brief johnhalflife's Gauss gun (jhl_fx_particlesystem), for the local player: a charge you can see, building in
 *	stages at the muzzle (a glow and its light, then arcs, then at full charge unstable arcs and sparks spitting out),
 *	a muzzle light when it fires (the charge's light becoming it), and a light where a beam reflects.
 */

/**
 *	@brief A charge update (EV_SpinGauss): @p pitch is the spin sound's, 110 as it starts to 250 at full charge.
 */
void JhlGauss_Spin(int pitch);

/**
 *	@brief The local player fired (EV_FireGauss): the charge ends, its light becoming the muzzle's flash, a charged shot
 *	throwing sparks by how much it held (from the spin events, not @p damage: the predicted shot's can come in far over
 *	what was held).
 */
void JhlGauss_Fire(bool primary, float damage);

/**
 *	@brief The charge stopped without firing (holstered, overcharged): it ends.
 */
void JhlGauss_StopCharge();

/**
 *	@brief A beam reflected at @p position (anyone's): a brief light in the gun's orange.
 */
void JhlGauss_ReflectLight(const Vector& position);

/**
 *	@brief A beam hit something at @p position (anyone's): a light, a little bigger and brighter than a reflection's.
 */
void JhlGauss_ImpactLight(const Vector& position);

/**
 *	@brief A charged shot's afterimage along a stretch of its path, from @p from to @p to (the beam's @p sprite), by its
 *	@p charge (0-1): warm white, starting as a wavy line and dissipating, swirling, spreading and dimming as it goes,
 *	for 0.6 s (a tap) to 3 s (a full charge).
 */
void JhlGauss_Afterimage(const Vector& from, const Vector& to, int sprite, float charge);

/**
 *	@brief Draws the charge and the afterimages (HUD_TempEntUpdate).
 */
void JhlGauss_Update(double time);

/**
 *	@brief Forgets the charge: a new map frees its glow (HUD_VidInit).
 */
void JhlGauss_Reset();
