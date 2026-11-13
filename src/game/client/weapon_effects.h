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

class BufferReader;
struct TEMPENTITY;

void WeaponEffects_Initialize();

void EV_GunMuzzleEffects(int pellets = 1, bool doubleShot = false);

// johnhalflife: @brief johnhalflife
void EV_GlockRapidMuzzleEffects();

// johnhalflife: @brief johnhalflife
void EV_MP5GrenadeEffects();

// johnhalflife: @brief johnhalflife
void EV_EagleBarrelSmoke();

void EagleSmoke_Update(double time);

void JhlBrassLinger(TEMPENTITY* shell);

// johnhalflife: @brief johnhalflife
void EV_ShotgunPumpShells(int count, float delay);

void ShotgunShells_Update(double time);

enum class JhlImpact : unsigned char
{
	Bullet,
	Revolver,
	Sniper,
	Melee,
	Knife,
};

void EV_M249MuzzleEffects();

// johnhalflife
struct cl_entity_t;
void TripmineLight(cl_entity_t* beam, const char* modelname);

// johnhalflife: @brief johnhalflife
void SporeLight(cl_entity_t* spore, const char* modelname);

// johnhalflife: @brief johnhalflife
void RocketGlow(cl_entity_t* rocket, const char* modelname);

// johnhalflife: @brief johnhalflife
void MortarShellTrail(cl_entity_t* shell, const char* modelname);

// johnhalflife: @brief johnhalflife
void GrenadePivot(cl_entity_t* grenade, const char* modelname);

// johnhalflife: @brief johnhalflife
void RpgLaserDot(cl_entity_t* dot, const char* modelname);

void RpgLaserLine_Update(double time);

void SniperSmoke_Update(double time);

void RpgLaser_Reset();

void EV_SniperMuzzleEffects(const Vector& eyePosition, const Vector& forward, const Vector& right, const Vector& up);

void JhlTank_ParseFire(BufferReader& reader);

void EV_RpgFireEffects(bool isLocal, const Vector& eyePosition, const Vector& forward, const Vector& right, const Vector& up,
	const Vector& shooterVelocity);

void BulletImpactEffects(const Vector& position, const Vector& normal, char textureType, JhlImpact kind = JhlImpact::Bullet);

void BulletWaterSplashAlongPath(const Vector& start, const Vector& end);

void BulletWaterSplash(const Vector& surfacePosition);
