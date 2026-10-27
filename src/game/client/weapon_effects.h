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

#include "vector.h"

/**
 *	@brief Registers the effect cvars and the BulletHit and WaterSplash messages, which the server sends for
 *	bullets it fires (NPCs, mounted guns).
 */
void WeaponEffects_Initialize();

/**
 *	@brief A stronger, warmer muzzle light and a faint smoke wisp at the local player's view model muzzle.
 *	Call alongside EV_MuzzleFlash for firearms.
 *	@param pellets A shotgun's: a chance of a wisp for each (others: one).
 */
void EV_GunMuzzleEffects(int pellets = 1, bool doubleShot = false);

/**
 *	@brief johnhalflife: the Glock's rapid fire's muzzle smoke, building over a burst (a much lighter M249's).
 */
void EV_GlockRapidMuzzleEffects();

/**
 *	@brief johnhalflife: the MP5's grenade launch: two big wisps and a bigger light (jhl_fx_particlesystem).
 */
void EV_MP5GrenadeEffects();

/**
 *	@brief johnhalflife: the Desert Eagle's barrel smoking a moment after a shot (jhl_fx_particlesystem).
 */
void EV_EagleBarrelSmoke();

/**
 *	@brief Its smoke (V_CalcRefdef, once the view model's placed).
 */
void EagleSmoke_Update(double time);

/**
 *	@brief What a bullet impact is (BulletImpactEffects): a gun's, the revolver's (a little heavier), the sniper
 *	rifle's (heavy), or a melee blow's (the knife's smaller). Sent as a byte (the BulletHit message).
 */
enum class JhlImpact : unsigned char
{
	Bullet,
	Revolver,
	Sniper,
	Melee,
	Knife,
};

/**
 *	@brief EV_GunMuzzleEffects for the M249: its wisps build with sustained fire (bigger, more opaque, a little apart),
 *	back to a touch over a gun's when it stops.
 */
void EV_M249MuzzleEffects();

/**
 *	@brief johnhalflife (jhl_fx_particlesystem): a faint light in the laser's color at an armed tripmine's emitter,
 *	for a beam entity being added this frame (HUD_AddEntity). Only a tripmine's laser is lit: its beam exists only
 *	while the mine is armed (CTripmineGrenade::MakeBeam, laserbeam.spr, (0,214,198) at 64).
 */
struct cl_entity_t;
void TripmineLight(cl_entity_t* beam, const char* modelname);

/**
 *	@brief johnhalflife: a spore's light in flight (jhl_fx_particlesystem), in the spore launcher's lime
 *	(HUD_AddEntity).
 */
void SporeLight(cl_entity_t* spore, const char* modelname);

/**
 *	@brief johnhalflife: an RPG rocket's motor (jhl_fx_particlesystem; HUD_AddEntity): an orange glow with a white-hot
 *	core and a warm light, flickering, fading out at burnout, in place of the engine's (EF_LIGHT).
 */
void RocketGlow(cl_entity_t* rocket, const char* modelname);

/**
 *	@brief johnhalflife: the RPG's laser dot lit, faintly red (jhl_fx_particlesystem; HUD_AddEntity), and where it is
 *	for its line.
 */
void RpgLaserDot(cl_entity_t* dot, const char* modelname);

/**
 *	@brief johnhalflife: the local player's RPG is reloading (CRpg::Reload, predicted): its dot is hidden 2.1 s, and
 *	coming back isn't coming on.
 */
void RpgLaser_Reloading();

/**
 *	@brief Draws the RPG's laser line when its dot comes on (turned on, or the RPG taken out with it on): a thin red line
 *	from the laser's housing to the dot, fading out over a second (V_CalcRefdef, once the view model's placed).
 */
void RpgLaserLine_Update(double time);

/**
 *	@brief The sniper rifle's barrel venting smoke through the bolt's working after a shot (V_CalcRefdef, once the view
 *	model's placed).
 */
void SniperSmoke_Update(double time);

/**
 *	@brief Forgets the line (HUD_VidInit: a new map frees the engine's beams).
 */
void RpgLaser_Reset();

/**
 *	@brief EV_GunMuzzleEffects for the sniper rifle, whose view model attachment isn't at its muzzle:
 *	the muzzle is placed relative to the view instead, and more often leaves a wisp.
 */
void EV_SniperMuzzleEffects(const Vector& eyePosition, const Vector& forward, const Vector& right, const Vector& up);

/**
 *	@brief RPG launch: a wisp and light at the front of the tube (local player only) and a backblast of smoke and
 *	light behind the shooter's shoulder (everyone).
 */
void EV_RpgFireEffects(bool isLocal, const Vector& eyePosition, const Vector& forward, const Vector& right, const Vector& up,
	const Vector& shooterVelocity);

/**
 *	@brief Subtle extra feedback where a bullet hits a surface, on top of the usual decal, specks and ricochet:
 *	sparks and a brief flash on metal, a small dust puff on stone, dirt and wood. Uses only stock sprites.
 *	@param textureType Material of the surface hit (CHAR_TEX_*).
 */
void BulletImpactEffects(const Vector& position, const Vector& normal, char textureType, JhlImpact kind = JhlImpact::Bullet);

/**
 *	@brief Adds a splash where a bullet travelling from @p start to @p end enters water or slime, if it does.
 */
void BulletWaterSplashAlongPath(const Vector& start, const Vector& end);

/**
 *	@brief A small splash at a point on a water surface: a droplet sprite and a ripple ring.
 */
void BulletWaterSplash(const Vector& surfacePosition);
