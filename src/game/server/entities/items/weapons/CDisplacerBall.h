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

class CDisplacerBall : public CBaseEntity
{
	DECLARE_CLASS(CDisplacerBall, CBaseEntity);
	DECLARE_DATAMAP();

private:
	static const size_t NUM_BEAMS = 8;

public:
	void Precache() override;
	void Spawn() override;

	void BallTouch(CBaseEntity* pOther);

	void FlyThink();
	void FlyThink2();
	void FizzleThink();
	void ExplodeThink();
	void KillThink();

	// johnhalflife: its implosion (TE_IMPLOSION), timed to end as it goes.
	void ImplodeThink();
	static constexpr float ImplosionLife = 0.3f;

	void InitBeams();

	void ClearBeams();

	void ArmBeam(int iSide);

	bool ClassifyTarget(CBaseEntity* pTarget);

	static CDisplacerBall* CreateDisplacerBall(const Vector& vecOrigin, const Vector& vecAngles, CBaseEntity* pOwner);

private:
	int m_iTrail;

	CBeam* m_pBeam[NUM_BEAMS];

	size_t m_uiBeams;

	// johnhalflife: a strand on each beam (UTIL_JhlBeamStrand), and which of its beam re-aims lights where they touch
	// (every other); the sprite's frames, animated as the Lambda Core's portals (vanilla shows only the first). Not
	// saved: beams are temporary, and the ball lives a second or two.
	EHANDLE m_pStrands[NUM_BEAMS];
	int m_iArmBeamCount = 0;

	/**
	 *	@brief Steps exit1.spr's frames, as the Lambda Core's env_sprites play it (40 fps).
	 */
	void Animate();

	EHANDLE m_hDisplacedTarget;
};
