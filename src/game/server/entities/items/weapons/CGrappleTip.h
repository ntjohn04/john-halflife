/***
 *
 *	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   This source code contains proprietary and confidential information of
 *   Valve LLC and its suppliers.  Access to this code is restricted to
 *   persons who have executed a written SDK license with Valve.  Any access,
 *   use or distribution of this code by or to any unlicensed person is illegal.
 *
 ****/

#pragma once

class CGrappleTip : public CBaseEntity
{
	DECLARE_CLASS(CGrappleTip, CBaseEntity);
	DECLARE_DATAMAP();

public:
	enum class TargetClass
	{
		NOT_A_TARGET = 0,
		SMALL = 1,
		MEDIUM = 2,
		LARGE = 3,
		FIXED = 4,
	};

public:
	void Precache() override;

	void Spawn() override;

	// johnhalflife: neither carried to the next map nor saved. The grapple doesn't save which tip is its own, so one
	// carried or loaded was left behind, flying or latched on for good, the client's tongue drawn to it
	// (jhl_grapple.cpp) from the gun until the game was restarted.
	int ObjectCaps() override { return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_DONT_SAVE; }

	void FlyThink();

	void OffsetThink();

	void TongueTouch(CBaseEntity* pOther);

	TargetClass ClassifyTarget(CBaseEntity* pTarget);

	// johnhalflife: its player's grapple isn't out or isn't firing it (a save made before ObjectCaps above).
	bool IsOrphaned();

	void SetPosition(const Vector& vecOrigin, const Vector& vecAngles, CBaseEntity* pOwner);

	TargetClass GetGrappleType() const { return m_GrappleType; }

	bool IsStuck() const { return m_bIsStuck; }

	bool HasMissed() const { return m_bMissed; }

	EHANDLE& GetGrappleTarget() { return m_hGrappleTarget; }

	void SetGrappleTarget(CBaseEntity* pTarget)
	{
		m_hGrappleTarget = pTarget;
	}

private:
	TargetClass m_GrappleType;
	bool m_bIsStuck;
	bool m_bMissed;

	EHANDLE m_hGrappleTarget;
	Vector m_vecOriginOffset;
};
