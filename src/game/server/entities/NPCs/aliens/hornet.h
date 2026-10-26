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

#define HORNET_TYPE_RED 0
#define HORNET_TYPE_ORANGE 1

// johnhalflife: hornet colours (CHornet::m_JhlColor): a red-type hornet red or purple-red, an orange one orange,
// yellow or light brown (so each colour is a fifth of all hornets).
constexpr int JhlRed = 0;
constexpr int JhlPurple = 1;
constexpr int JhlOrange = 2;
constexpr int JhlYellow = 3;
constexpr int JhlBrown = 4;

#define HORNET_RED_SPEED (float)600
#define HORNET_ORANGE_SPEED (float)800
#define HORNET_BUZZ_VOLUME (float)0.8

extern int iHornetPuff;

/**
 *	@brief this is the projectile that the Alien Grunt fires.
 */
class CHornet : public CBaseMonster
{
	DECLARE_CLASS(CHornet, CBaseMonster);
	DECLARE_DATAMAP();

public:
	void OnCreate() override;
	void Spawn() override;
	void Precache() override;

	bool IsBioWeapon() const override { return true; }

	/**
	 *	@brief hornets will never get mad at each other, no matter who the owner is.
	 */
	Relationship IRelationship(CBaseEntity* pTarget) override;

	void IgniteTrail();

	/**
	 *	@brief starts a hornet out tracking its target
	 */
	void StartTrack();

	/**
	 *	@brief starts a hornet out just flying straight.
	 */
	void StartDart();

	/**
	 *	@brief Hornet is flying, gently tracking target
	 */
	void TrackTarget();
	void TrackTouch(CBaseEntity* pOther);
	void DartTouch(CBaseEntity* pOther);
	void DieTouch(CBaseEntity* pOther);

	bool TakeDamage(CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType) override;

	float m_flStopAttack;
	int m_iHornetType;

	// johnhalflife (jhl_fx_particlesystem): its colour within its type, the same behaviour: a red hornet red or
	// purple-red, an orange one orange, yellow or light brown (each colour a fifth of all hornets). Its trail and its
	// dart splat.
	int m_JhlColor = 0;
	float m_flFlySpeed;
};
