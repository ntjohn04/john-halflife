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
#include "jhl_effects.h"
#include "jhl_explosion.h"

LINK_ENTITY_TO_CLASS(grenade, CGrenade);

// Grenades flagged with this will be triggered when the owner calls detonateSatchelCharges
#define SF_DETONATE 0x0001

BEGIN_DATAMAP(CGrenade)
DEFINE_FUNCTION(Smoke),
	DEFINE_FUNCTION(BounceTouch),
	DEFINE_FUNCTION(SlideTouch),
	DEFINE_FUNCTION(ExplodeTouch),
	DEFINE_FUNCTION(DangerSoundThink),
	DEFINE_FUNCTION(PreDetonate),
	DEFINE_FUNCTION(Detonate),
	DEFINE_FUNCTION(DetonateUse),
	DEFINE_FUNCTION(TumbleThink),
	END_DATAMAP();

void CGrenade::OnCreate()
{
	CBaseMonster::OnCreate();

	pev->model = MAKE_STRING("models/grenade.mdl");
}

void CGrenade::Precache()
{
	// Grenade exploded after save, but hasn't been removed yet. Don't precache.
	if (!FStringNull(pev->model))
	{
		PrecacheModel(STRING(pev->model));
	}
}

void CGrenade::Explode(Vector vecSrc, Vector vecAim)
{
	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + Vector(0, 0, -32), ignore_monsters, edict(), &tr);

	Explode(&tr, DMG_BLAST);
}

void CGrenade::Explode(TraceResult* pTrace, int bitsDamageType)
{
	pev->model = string_t::Null; // invisible
	pev->solid = SOLID_NOT;		 // intangible

	pev->takedamage = DAMAGE_NO;

	// Pull out of the wall a bit
	if (pTrace->flFraction != 1.0)
	{
		pev->origin = pTrace->vecEndPos + (pTrace->vecPlaneNormal * 0.6);
	}

	// johnhalflife: Half-Life Updated pulls explosions only 0.6 units out of the surface so their damage doesn't go
	// through walls (halflife#3244), which also sank the fireball and smoke halfway into it. The damage stays there;
	// the effects come from vanilla's point, (dmg - 24) * 0.6 units out.
	Vector effectOrigin = pev->origin;

	if (pTrace->flFraction != 1.0)
	{
		effectOrigin = pTrace->vecEndPos + (pTrace->vecPlaneNormal * (pev->dmg - 24) * 0.6);
	}

	int iContents = UTIL_PointContents(effectOrigin);

	// johnhalflife: the client throws the spark showers (jhl_explosions.cpp).
	const bool clientSparks = UTIL_JhlParticleSystem() && iContents != CONTENTS_WATER;
	int flags = ClassnameIs("rpg_rocket") ? TE_EXPLFLAG_JHL_ROCKET : TE_EXPLFLAG_NONE;

	if (clientSparks)
		flags |= TE_EXPLFLAG_JHL_SPARKS;

	// This makes a dynamic light and the explosion sprites/sound
	// Send to PAS because of the sound
	UTIL_ExplosionEffect(effectOrigin, iContents != CONTENTS_WATER ? g_sModelIndexFireball : g_sModelIndexWExplosion,
		(pev->dmg - 50) * .60, 15, flags,
		MSG_PAS, effectOrigin);

	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0);
	CBaseEntity* owner = GetOwner();

	pev->owner = nullptr; // can't traceline attack owner if this is set

	// Counteract the + 1 in RadiusDamage.
	Vector origin = pev->origin;
	origin.z -= 1;

	RadiusDamage(origin, this, owner, pev->dmg, bitsDamageType);

	// The rest (debris sound, sparks, the smoke 0.3 seconds later) is effects.
	SetOrigin(effectOrigin);

	if (UTIL_JhlParticleSystem())
	{
		// As big as the client draws it (RPG rockets bigger).
		const float scale = (pev->dmg - 50) * 0.06f * ((flags & TE_EXPLFLAG_JHL_ROCKET) != 0 ? JhlRocketExplosionScale : 1);
		UTIL_JhlExplosionScorch(pTrace, scale);
		UTIL_JhlExplosionShake(effectOrigin, scale);
	}
	else if (RANDOM_FLOAT(0, 1) < 0.5)
	{
		UTIL_DecalTrace(pTrace, DECAL_SCORCH1);
	}
	else
	{
		UTIL_DecalTrace(pTrace, DECAL_SCORCH2);
	}

	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EmitSound(CHAN_VOICE, "weapons/debris1.wav", 0.55, ATTN_NORM);
		break;
	case 1:
		EmitSound(CHAN_VOICE, "weapons/debris2.wav", 0.55, ATTN_NORM);
		break;
	case 2:
		EmitSound(CHAN_VOICE, "weapons/debris3.wav", 0.55, ATTN_NORM);
		break;
	}

	pev->effects |= EF_NODRAW;
	SetThink(&CGrenade::Smoke);
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time + 0.3;

	if (iContents != CONTENTS_WATER && !clientSparks)
	{
		int sparkCount = RANDOM_LONG(0, 3);
		for (int i = 0; i < sparkCount; i++)
			Create("spark_shower", pev->origin, pTrace->vecPlaneNormal, nullptr);
	}
}

void CGrenade::Smoke()
{
	if (UTIL_PointContents(pev->origin) == CONTENTS_WATER)
	{
		if (UTIL_JhlParticleSystem())
			UTIL_JhlExplosionBubbles(pev->origin, (pev->dmg - 50) * 0.06f);
		else
			UTIL_Bubbles(pev->origin - Vector(64, 64, 64), pev->origin + Vector(64, 64, 64), 100);
	}
	else
	{
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_SMOKE);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_SHORT(g_sModelIndexSmoke);
		WRITE_BYTE(UTIL_JhlExplosionSmokeScale((pev->dmg - 50) * 0.80)); // scale * 10
		WRITE_BYTE(12);						// framerate
		MESSAGE_END();
	}
	UTIL_Remove(this);
}

void CGrenade::Killed(CBaseEntity* attacker, int iGib)
{
	Detonate();
}

void CGrenade::DetonateUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	SetThink(&CGrenade::Detonate);
	pev->nextthink = gpGlobals->time;
}

void CGrenade::PreDetonate()
{
	CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, 400, 0.3);

	SetThink(&CGrenade::Detonate);
	pev->nextthink = gpGlobals->time + 1;
}

void CGrenade::Detonate()
{
	TraceResult tr;
	Vector vecSpot; // trace starts here!

	vecSpot = pev->origin + Vector(0, 0, 8);
	UTIL_TraceLine(vecSpot, vecSpot + Vector(0, 0, -40), ignore_monsters, edict(), &tr);

	Explode(&tr, DMG_BLAST);
}

void CGrenade::ExplodeTouch(CBaseEntity* pOther)
{
	TraceResult tr;
	Vector vecSpot; // trace starts here!

	pev->enemy = pOther->edict();

	vecSpot = pev->origin - pev->velocity.Normalize() * 32;
	UTIL_TraceLine(vecSpot, vecSpot + pev->velocity.Normalize() * 64, ignore_monsters, edict(), &tr);

	Explode(&tr, DMG_BLAST);
}

void CGrenade::DangerSoundThink()
{
	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin + pev->velocity * 0.5, pev->velocity.Length(), 0.2);
	pev->nextthink = gpGlobals->time + 0.2;

	if (pev->waterlevel != WaterLevel::Dry)
	{
		pev->velocity = pev->velocity * 0.5;
	}
}

// johnhalflife: a thrown grenade's tumble and roll, only to look at (where it goes is its velocity's, untouched), with
// jhl_fx_particlesystem. In the air it spins end over end (its angles turned by avelocity, which moves nothing) as fast
// as it was thrown, wobbling a little; each bounce flips and damps its spin; on the ground it lies level, faces where
// it's going and rolls (its model's roll animations) as fast as it's going, easing to a stop where it is. Vanilla
// played a slow fixed tumble animation in the air, kept its throw's facing (tilted, and sideways once it had bounced
// off a wall), and snapped its roll back to its first frame when it slowed.
namespace
{
constexpr float TumblePerSpeed = 1.1f; // Degrees a second per unit a second it's thrown at,
constexpr float TumbleMin = 300;		 // between these:
constexpr float TumbleMax = 1100;		 // about 1 to 3 turns a second.
constexpr float TumbleWobble = 60;
constexpr float BounceKeepMin = 0.4f; // Of its spin, each bounce, turned the other way.
constexpr float BounceKeepMax = 0.7f;
constexpr float RollSpeed = 200; // Its roll animation at its own rate at this speed (vanilla's).
constexpr float RollMost = 2;
constexpr float RollEase = 0.5f; // Of the way to its roll's rate each think (0.1 s).
constexpr float StillSpeed = 5;

bool IsRollSequence(int sequence)
{
	return sequence == 1 || sequence == 2; // roll1, roll2 (w_grenade.mdl).
}
} // namespace

void CGrenade::BounceTouch(CBaseEntity* pOther)
{
	// don't hit the guy that launched this grenade
	if (pOther == GetOwner())
		return;

	// only do damage if we're moving fairly fast
	if (m_flNextAttack < gpGlobals->time && pev->velocity.Length() > 100)
	{
		CBaseEntity* owner = GetOwner();
		if (owner)
		{
			TraceResult tr = UTIL_GetGlobalTrace();
			ClearMultiDamage();
			pOther->TraceAttack(owner, 1, gpGlobals->v_forward, &tr, DMG_CLUB);
			ApplyMultiDamage(this, owner);
		}
		m_flNextAttack = gpGlobals->time + 1.0; // debounce
	}

	Vector vecTestVelocity;
	// pev->avelocity = Vector (300, 300, 300);

	// this is my heuristic for modulating the grenade velocity because grenades dropped purely vertical
	// or thrown very far tend to slow down too quickly for me to always catch just by testing velocity.
	// trimming the Z velocity a bit seems to help quite a bit.
	vecTestVelocity = pev->velocity;
	vecTestVelocity.z *= 0.45;

	if (!m_fRegisteredSound && vecTestVelocity.Length() <= 60)
	{
		// CBaseEntity::Logger->debug("Grenade Registered!: {}", vecTestVelocity.Length());

		// grenade is moving really slow. It's probably very close to where it will ultimately stop moving.
		// go ahead and emit the danger sound.

		// register a radius louder than the explosion, so we make sure everyone gets out of the way
		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, pev->dmg / 0.4, 0.3);
		m_fRegisteredSound = true;
	}

	const bool jhl = UTIL_JhlParticleSystem();

	if ((pev->flags & FL_ONGROUND) != 0)
	{
		// add a bit of static friction
		// johnhalflife (jhl_physics_fpsfix): touched every frame it slides, so taken off as at 72 fps.
		pev->velocity = pev->velocity * (UTIL_JhlFpsFix() ? JhlReferenceDecay(0.8f, gpGlobals->frametime) : 0.8f);

		if (jhl)
		{
			// Down: level, done spinning, rolling one of its two ways (vanilla only ever the first), its rate eased in
			// TumbleThink.
			pev->avelocity = g_vecZero;
			pev->angles.x = 0;
			pev->angles.z = 0;

			if (!IsRollSequence(pev->sequence))
			{
				pev->sequence = RANDOM_LONG(1, 2);
				ResetSequenceInfo();
				pev->framerate = 0;
			}

			return;
		}

		pev->sequence = RANDOM_LONG(1, 1);
		ResetSequenceInfo();
	}
	else
	{
		// play bounce sound
		BounceSound();

		// A bounce flips its spin and takes some of it.
		if (jhl)
		{
			pev->avelocity = pev->avelocity * -RANDOM_FLOAT(BounceKeepMin, BounceKeepMax) +
							 Vector(RANDOM_FLOAT(-1, 1), RANDOM_FLOAT(-1, 1), RANDOM_FLOAT(-1, 1)) * TumbleWobble;
			return;
		}
	}
	pev->framerate = pev->velocity.Length() / 200.0;
	if (pev->framerate > 1.0)
		pev->framerate = 1;
	else if (pev->framerate < 0.5)
	{
		pev->framerate = 0;
		pev->frame = 0;
	}
}

void CGrenade::SlideTouch(CBaseEntity* pOther)
{
	// don't hit the guy that launched this grenade
	if (pOther->edict() == pev->owner)
		return;

	// pev->avelocity = Vector (300, 300, 300);

	if ((pev->flags & FL_ONGROUND) != 0)
	{
		// add a bit of static friction
		// johnhalflife (jhl_physics_fpsfix): touched every frame it slides, so taken off as at 72 fps.
		pev->velocity = pev->velocity * (UTIL_JhlFpsFix() ? JhlReferenceDecay(0.95f, gpGlobals->frametime) : 0.95f);

		if (pev->velocity.x != 0 || pev->velocity.y != 0)
		{
			// maintain sliding sound
		}
	}
	else
	{
		BounceSound();
	}
}

void CGrenade::BounceSound()
{
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EmitSound(CHAN_VOICE, "weapons/grenade_hit1.wav", 0.25, ATTN_NORM);
		break;
	case 1:
		EmitSound(CHAN_VOICE, "weapons/grenade_hit2.wav", 0.25, ATTN_NORM);
		break;
	case 2:
		EmitSound(CHAN_VOICE, "weapons/grenade_hit3.wav", 0.25, ATTN_NORM);
		break;
	}
}

void CGrenade::TumbleThink()
{
	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	// johnhalflife: rolling on the ground, facing where it's going, as fast as it's going, easing to a stop where it is
	// (see BounceTouch).
	if (UTIL_JhlParticleSystem() && IsRollSequence(pev->sequence) && pev->waterlevel == WaterLevel::Dry)
	{
		const float speed = Vector(pev->velocity.x, pev->velocity.y, 0).Length();

		if (speed > StillSpeed)
			pev->angles.y = UTIL_VecToAngles(pev->velocity).y;

		const float rate = std::min(speed / RollSpeed, RollMost);
		pev->framerate += (rate - pev->framerate) * RollEase;

		if (speed <= StillSpeed && pev->framerate < 0.05f)
			pev->framerate = 0;
	}

	if (pev->dmgtime - 1 < gpGlobals->time)
	{
		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin + pev->velocity * (pev->dmgtime - gpGlobals->time), 400, 0.1);
	}

	if (pev->dmgtime <= gpGlobals->time)
	{
		SetThink(&CGrenade::Detonate);
	}
	if (pev->waterlevel != WaterLevel::Dry)
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2;
		pev->avelocity = pev->avelocity * 0.5;
	}
}

void CGrenade::Spawn()
{
	Precache();

	pev->movetype = MOVETYPE_BOUNCE;

	pev->solid = SOLID_BBOX;

	SetModel(STRING(pev->model));
	SetSize(Vector(0, 0, 0), Vector(0, 0, 0));

	pev->dmg = 100;
	m_fRegisteredSound = false;
}

CGrenade* CGrenade::ShootContact(CBaseEntity* owner, Vector vecStart, Vector vecVelocity)
{
	CGrenade* pGrenade = g_EntityDictionary->Create<CGrenade>("grenade");
	pGrenade->Spawn();
	// contact grenades arc lower
	pGrenade->pev->gravity = 0.5; // lower gravity since grenade is aerodynamic and engine doesn't know it.
	pGrenade->SetOrigin(vecStart);
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = UTIL_VecToAngles(pGrenade->pev->velocity);
	pGrenade->pev->owner = owner->edict();

	// make monsters afaid of it while in the air
	pGrenade->SetThink(&CGrenade::DangerSoundThink);
	pGrenade->pev->nextthink = gpGlobals->time;

	// Tumble in air
	pGrenade->pev->avelocity.x = RANDOM_FLOAT(-100, -500);

	// Explode on contact
	pGrenade->SetTouch(&CGrenade::ExplodeTouch);

	pGrenade->pev->dmg = GetSkillFloat("plr_9mmAR_grenade"sv);

	return pGrenade;
}

CGrenade* CGrenade::ShootTimed(CBaseEntity* owner, Vector vecStart, Vector vecVelocity, float time)
{
	CGrenade* pGrenade = g_EntityDictionary->Create<CGrenade>("grenade");
	pGrenade->Spawn();
	pGrenade->SetOrigin(vecStart);
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = UTIL_VecToAngles(pGrenade->pev->velocity);
	pGrenade->pev->owner = owner->edict();

	pGrenade->SetTouch(&CGrenade::BounceTouch); // Bounce if touched

	// Take one second off of the desired detonation time and set the think to PreDetonate. PreDetonate
	// will insert a DANGER sound into the world sound list and delay detonation for one second so that
	// the grenade explodes after the exact amount of time specified in the call to ShootTimed().

	pGrenade->pev->dmgtime = gpGlobals->time + time;
	pGrenade->SetThink(&CGrenade::TumbleThink);
	pGrenade->pev->nextthink = gpGlobals->time + 0.1;
	if (time < 0.1)
	{
		pGrenade->pev->nextthink = gpGlobals->time;
		pGrenade->pev->velocity = Vector(0, 0, 0);
	}

	pGrenade->SetModel("models/w_grenade.mdl");
	pGrenade->pev->sequence = RANDOM_LONG(3, 6);
	pGrenade->pev->framerate = 1.0;
	pGrenade->ResetSequenceInfo();

	// Tumble through the air
	// pGrenade->pev->avelocity.x = -400;

	// johnhalflife: end over end, as fast as it's thrown, wobbling a little, in place of its slow tumble animation
	// (see BounceTouch).
	if (UTIL_JhlParticleSystem())
	{
		pGrenade->pev->sequence = 0;
		pGrenade->ResetSequenceInfo();
		pGrenade->pev->framerate = 0;

		// Each thrown its own way: turned about its length at random (flying flat, upright or slanted), its spin mostly
		// end over end, some of it turned toward a roll, as hard as it's thrown more or less, now and then a lazy lob.
		pGrenade->pev->angles.z = RANDOM_FLOAT(0, 360);

		const float spin = RANDOM_LONG(0, 9) == 0 ? RANDOM_FLOAT(0.3f, 0.5f) : RANDOM_FLOAT(0.55f, 1.35f);
		const float tumble = std::clamp(vecVelocity.Length() * TumblePerSpeed, TumbleMin, TumbleMax) * spin;
		const float toward = RANDOM_FLOAT(0, 60) * 3.1415927f / 180;
		pGrenade->pev->avelocity = Vector(-tumble * std::cos(toward), RANDOM_FLOAT(-TumbleWobble, TumbleWobble),
			tumble * std::sin(toward) * (RANDOM_LONG(0, 1) != 0 ? 1 : -1));
	}

	pGrenade->pev->gravity = 0.5;
	pGrenade->pev->friction = 0.8;

	pGrenade->pev->dmg = 100;

	return pGrenade;
}

CGrenade* CGrenade::ShootSatchelCharge(CBaseEntity* owner, Vector vecStart, Vector vecVelocity)
{
	CGrenade* pGrenade = g_EntityDictionary->Create<CGrenade>("grenade");
	pGrenade->pev->movetype = MOVETYPE_BOUNCE;

	pGrenade->pev->solid = SOLID_BBOX;

	pGrenade->SetModel("models/grenade.mdl"); // Change this to satchel charge model

	pGrenade->SetSize(Vector(0, 0, 0), Vector(0, 0, 0));

	pGrenade->pev->dmg = 200;
	pGrenade->SetOrigin(vecStart);
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = g_vecZero;
	pGrenade->pev->owner = owner->edict();

	// Detonate in "time" seconds
	pGrenade->SetThink(nullptr);
	pGrenade->SetUse(&CGrenade::DetonateUse);
	pGrenade->SetTouch(&CGrenade::SlideTouch);
	pGrenade->pev->spawnflags = SF_DETONATE;

	pGrenade->pev->friction = 0.9;

	return pGrenade;
}

void CGrenade::UseSatchelCharges(CBaseEntity* owner, SATCHELCODE code)
{
	if (!owner)
		return;

	for (auto pEnt : UTIL_FindEntitiesByClassname("grenade"))
	{
		if (FBitSet(pEnt->pev->spawnflags, SF_DETONATE) && pEnt->GetOwner() == owner)
		{
			if (code == SATCHEL_DETONATE)
				pEnt->Use(owner, owner, USE_ON, 0);
			else // SATCHEL_RELEASE
				pEnt->pev->owner = nullptr;
		}
	}
}
