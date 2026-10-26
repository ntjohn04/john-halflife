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
#include "cbase.h"
#include "customentity.h"

#include "CShockBeam.h"
#include "jhl_effects.h"

BEGIN_DATAMAP(CShockBeam)
DEFINE_FIELD(m_pBeam1, FIELD_CLASSPTR),
	DEFINE_FIELD(m_pBeam2, FIELD_CLASSPTR),
	DEFINE_FIELD(m_pSprite, FIELD_CLASSPTR),
	DEFINE_FUNCTION(FlyThink),
	DEFINE_FUNCTION(ExplodeThink),
	DEFINE_FUNCTION(WaterExplodeThink),
	DEFINE_FUNCTION(BallTouch),
	END_DATAMAP();

LINK_ENTITY_TO_CLASS(shock_beam, CShockBeam);

void CShockBeam::Precache()
{
	PrecacheModel("sprites/flare3.spr");
	PrecacheModel("sprites/lgtning.spr");
	PrecacheModel("sprites/glow01.spr");
	PrecacheModel("models/shock_effect.mdl");
	PrecacheSound("weapons/shock_impact.wav");
}

void CShockBeam::Spawn()
{
	Precache();

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SetModel("models/shock_effect.mdl");

	SetOrigin(pev->origin);

	SetSize(Vector(-4, -4, -4), Vector(4, 4, 4));

	SetTouch(&CShockBeam::BallTouch);
	SetThink(&CShockBeam::FlyThink);

	// johnhalflife: the client draws its glow and its spark instead, circling its path 180 degrees apart, with their
	// wakes a double helix (jhl_shockbeams.cpp); its crackle across it goes (it ran between its model's attachments,
	// 80 units apart, which can't make a small circle).
	// It's tagged renderamt 1 for the client (the barnacle grapple's tongue tip has the same model; renderamt does
	// nothing for a model drawn normally).
	if (UTIL_JhlParticleSystem())
	{
		pev->renderamt = 1;
		pev->nextthink = gpGlobals->time + 0.01;
		return;
	}

	m_pSprite = CSprite::SpriteCreate("sprites/flare3.spr", pev->origin, false);

	m_pSprite->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxDistort);

	m_pSprite->SetScale(0.35);

	m_pSprite->SetAttachment(edict(), 0);

	m_pBeam1 = CBeam::BeamCreate("sprites/lgtning.spr", 60);

	if (m_pBeam1)
	{
		m_pBeam1->SetOrigin(pev->origin);

		m_pBeam1->EntsInit(entindex(), entindex());

		m_pBeam1->SetStartAttachment(1);
		m_pBeam1->SetEndAttachment(2);

		m_pBeam1->SetColor(0, 253, 253);

		m_pBeam1->SetFlags(BEAM_FSHADEOUT);
		m_pBeam1->SetBrightness(180);
		m_pBeam1->SetNoise(0);

		m_pBeam1->SetScrollRate(10);

		if (g_pGameRules->IsMultiplayer())
		{
			pev->nextthink = gpGlobals->time + 0.01;
			return;
		}

		m_pBeam2 = CBeam::BeamCreate("sprites/lgtning.spr", 20);

		if (m_pBeam2)
		{
			m_pBeam2->SetOrigin(pev->origin);

			m_pBeam2->EntsInit(entindex(), entindex());

			m_pBeam2->SetStartAttachment(1);
			m_pBeam2->SetEndAttachment(2);

			m_pBeam2->SetColor(255, 255, 157);

			m_pBeam2->SetFlags(BEAM_FSHADEOUT);
			m_pBeam2->SetBrightness(180);
			m_pBeam2->SetNoise(30);

			m_pBeam2->SetScrollRate(30);

			pev->nextthink = gpGlobals->time + 0.01;
		}
	}
}

void CShockBeam::FlyThink()
{
	if (pev->waterlevel == WaterLevel::Head)
	{
		SetThink(&CShockBeam::WaterExplodeThink);
	}

	pev->nextthink = gpGlobals->time + 0.01;
}

void CShockBeam::ExplodeThink()
{
	Explode();
	UTIL_Remove(this);
}

void CShockBeam::WaterExplodeThink()
{
	auto pOwner = GetOwner();

	Explode();

	::RadiusDamage(pev->origin, this, pOwner, 100.0, 150.0, DMG_ALWAYSGIB | DMG_BLAST);

	UTIL_Remove(this);
}

void CShockBeam::BallTouch(CBaseEntity* pOther)
{
	SetTouch(nullptr);
	SetThink(nullptr);

	if (pOther->pev->takedamage != DAMAGE_NO)
	{
		TraceResult tr = UTIL_GetGlobalTrace();

		ClearMultiDamage();

		const auto damage = g_pGameRules->IsMultiplayer() ? GetSkillFloat("plr_shockroachm"sv) : GetSkillFloat("plr_shockroachs"sv);

		auto bitsDamageTypes = DMG_ALWAYSGIB | DMG_SHOCK;

		auto pMonster = pOther->MyMonsterPointer();

		if (pMonster)
		{
			bitsDamageTypes = DMG_BLAST;

			if (pMonster->m_flShockDuration > 1)
			{
				bitsDamageTypes = DMG_ALWAYSGIB;
			}
		}

		pOther->TraceAttack(GetOwner(), damage, pev->velocity.Normalize(), &tr, bitsDamageTypes);

		if (pMonster)
		{
			pMonster->AddShockEffect(63.0, 152.0, 208.0, 16.0, 0.5);
		}

		ApplyMultiDamage(this, GetOwner());

		pev->velocity = g_vecZero;
	}

	SetThink(&CShockBeam::ExplodeThink);
	pev->nextthink = gpGlobals->time + 0.01;

	if (pOther->pev->takedamage == DAMAGE_NO)
	{
		TraceResult tr;
		UTIL_TraceLine(pev->origin, pev->origin + pev->velocity * 10, dont_ignore_monsters, edict(), &tr);

		UTIL_DecalTrace(&tr, DECAL_OFSCORCH1 + RANDOM_LONG(0, 2));

		if (tr.flFraction < 1)
			m_JhlHitNormal = tr.vecPlaneNormal;

		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
		g_engfuncs.pfnWriteByte(TE_SPARKS);
		g_engfuncs.pfnWriteCoord(pev->origin.x);
		g_engfuncs.pfnWriteCoord(pev->origin.y);
		g_engfuncs.pfnWriteCoord(pev->origin.z);
		MESSAGE_END();
	}
}

void CShockBeam::Explode()
{
	if (m_pSprite)
	{
		UTIL_Remove(m_pSprite);
		m_pSprite = nullptr;
	}

	if (m_pBeam1)
	{
		UTIL_Remove(m_pBeam1);
		m_pBeam1 = nullptr;
	}

	if (m_pBeam2)
	{
		UTIL_Remove(m_pBeam2);
		m_pBeam2 = nullptr;
	}

	pev->dmg = 40;

	// johnhalflife: its impact in one of its two colours, half the time each: the spark's cyan (vanilla's light, and
	// arcs crackling), or the glow's purple (a purple light, and a few of its glow fizzling off); in rapid fire the two
	// tumble over each other.
	const bool jhl = UTIL_JhlParticleSystem();
	const bool purple = jhl && RANDOM_LONG(0, 1) != 0;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_DLIGHT);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_BYTE(8);
	WRITE_BYTE(purple ? 170 : 0);
	WRITE_BYTE(purple ? 110 : 253);
	WRITE_BYTE(purple ? 220 : 253);
	WRITE_BYTE(5);
	WRITE_BYTE(10);
	MESSAGE_END();

	pev->owner = nullptr;

	EmitSound(CHAN_WEAPON, "weapons/shock_impact.wav", RANDOM_FLOAT(0.8, 0.9), ATTN_NORM);

	// johnhalflife: a purple impact fizzles: 3-4 of its glow sprites (flare3, pinkish grey) thrown up and out off where
	// it hit, falling and bouncing (the engine's sprite trail). Out from the surface it hit, and from a little off it: a
	// sprite that hits a wall stops dead on it, and on some walls ends up just inside, stuck there.
	if (purple)
	{
		Vector start = pev->origin;
		Vector direction{0, 0, 1};

		if (m_JhlHitNormal != g_vecZero)
		{
			start = start + m_JhlHitNormal * 4;

			if (const Vector out = m_JhlHitNormal + direction; out.Length() > 0.5f)
				direction = out.Normalize();
			else
				direction = m_JhlHitNormal;
		}

		const Vector end = start + direction;

		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, start);
		WRITE_BYTE(TE_SPRITETRAIL);
		WRITE_COORD_VECTOR(start);
		WRITE_COORD_VECTOR(end);
		WRITE_SHORT(g_engfuncs.pfnModelIndex("sprites/flare3.spr"));
		WRITE_BYTE(RANDOM_LONG(3, 4)); // count
		WRITE_BYTE(8);				   // life, tenths
		WRITE_BYTE(1);				   // scale, tenths
		WRITE_BYTE(8);				   // velocity along it, tens
		WRITE_BYTE(10);				   // random velocity, tens
		MESSAGE_END();
	}

	// johnhalflife: a cyan impact crackles: a few short arcs off it, 0.4-0.6 s each and up to 35% more (to the
	// tenth; their noise changes every frame, so they crackle while they last), each impact's 5-30% bigger than the
	// smallest (longer and wider), off the surface it hit rather than into it.
	else if (jhl)
	{
		const int sprite = g_engfuncs.pfnModelIndex("sprites/lgtning.spr");
		const float size = RANDOM_FLOAT(1.05f, 1.3f);

		for (int i = 0, count = RANDOM_LONG(3, 4); i < count; ++i)
		{
			Vector direction = Vector(RANDOM_FLOAT(-1, 1), RANDOM_FLOAT(-1, 1), RANDOM_FLOAT(-1, 1)).Normalize();

			if (DotProduct(direction, m_JhlHitNormal) < 0)
				direction = direction - m_JhlHitNormal * (2 * DotProduct(direction, m_JhlHitNormal));

			const Vector end = pev->origin + direction * (RANDOM_FLOAT(12, 30) * size);

			MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
			WRITE_BYTE(TE_BEAMPOINTS);
			WRITE_COORD_VECTOR(pev->origin);
			WRITE_COORD_VECTOR(end);
			WRITE_SHORT(sprite);
			WRITE_BYTE(0);						// framestart
			WRITE_BYTE(10);						// framerate
			WRITE_BYTE(static_cast<int>(std::lround(RANDOM_LONG(4, 6) * RANDOM_FLOAT(1, 1.35f)))); // life, tenths
			WRITE_BYTE(static_cast<int>(10 * size)); // width, tenths
			WRITE_BYTE(RANDOM_LONG(50, 80));	// noise, hundredths
			WRITE_BYTE(0);						// r, g, b
			WRITE_BYTE(253);
			WRITE_BYTE(253);
			WRITE_BYTE(200);					// brightness
			WRITE_BYTE(30);						// speed
			MESSAGE_END();
		}
	}
}

CShockBeam* CShockBeam::CreateShockBeam(const Vector& vecOrigin, const Vector& vecAngles, CBaseEntity* pOwner)
{
	auto pBeam = g_EntityDictionary->Create<CShockBeam>("shock_beam");

	pBeam->pev->angles = vecAngles;
	pBeam->pev->angles.x = -pBeam->pev->angles.x;

	pBeam->SetOrigin(vecOrigin);

	UTIL_MakeVectors(pBeam->pev->angles);

	pBeam->pev->velocity = gpGlobals->v_forward * 2000.0;
	pBeam->pev->velocity.z = -pBeam->pev->velocity.z;

	pBeam->Spawn();

	pBeam->pev->owner = pOwner->edict();


	return pBeam;
}
