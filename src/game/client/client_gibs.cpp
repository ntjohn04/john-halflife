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

#include "hud.h"
#include "client_gibs.h"
#include "ClientLibrary.h"
#include "com_model.h"
#include "event_api.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "networking/ClientUserMessages.h"
#include "jhl_settings.h"

enum class GibState
{
	WaitTillLand = 0,
	WaitTillExpire,
	StartFadeOut,
	FadeOut,
	Remove
};

static cvar_t* g_cl_gibcount = nullptr;
static cvar_t* g_cl_giblife = nullptr;
static cvar_t* g_cl_gibvelscale = nullptr;

// johnhalflife (jhl_fx_particlesystem): more gibs, lasting longer, each a little different in size and speed.
constexpr int GibCountMultiplier = 7;		   // Death gibs; a single gib (a barnacle bite) stays one.
constexpr int MaxUsesPerGibPiece = 3;		   // Each gib model piece at most this many times per death,
constexpr int MaxUsesPerGibPieceLarge = 4;	   // or this many for large aliens, which have only 4 pieces (16 gibs).
constexpr int SoundingGibsPerDeath = 4;		   // Gibs that make bounce sounds,
constexpr int BounceSoundsPerGib = 3;		   // each on its first bounces only.
constexpr float JhlGibLife = 60;			   // Seconds on the ground before fading (the giblife cvar otherwise).
constexpr float JhlGibScaleVariation = 0.15f;  // Size +-15%.
constexpr float JhlGibSpeedVariation = 0.2f;   // Speed +-20%.
constexpr float JhlGibSidewaysSpread = 0.7f;   // Random sideways direction, +-0.7 (vanilla 0.25).
constexpr int TrailingGibsPerDeath = 7;		   // Gibs that leave a trail of blood drops as they fly,
constexpr float BloodTrailTime = 1;			   // for this long after being thrown, thinning out,
constexpr float BloodTrailSpacing = 4;		   // a drop every this many units at first.
constexpr int BloodStreamRed = 70;			   // Red blood particles: palette 70 and up, as the game's red blood streams.
// Effects pick colors a few palette entries up from the one they're given. 247 (the game's red) is followed by
// bright reds up to 255,0,0 and 195 (yellow) is the brightest of its ramp, so darker starts match the decals.
constexpr int SplashPuffRed = 74;			   // 87,0,0 and up.
constexpr int DarkYellow = 198;				   // 155,131,7 and darker.
constexpr int SplashBloodPuffs = 3;			   // A burst where the body was: the game's hit blood,
constexpr float SplashBloodSize = 16;		   // at its largest size,
constexpr float SplashStagger = 0.15f;		   // the extra puffs over this long;
constexpr int SplashChunks = 30;
constexpr float BloodPoolDelay = 1;			   // Seconds after a death without gibbing before the pool appears.			   // and blood chunk sprites thrown out (red blood only).

static cvar_t* g_violence_hblood = nullptr;
static cvar_t* g_violence_ablood = nullptr;

// How a particular monster gibs, where it differs from the defaults (GibCountMultiplier, MaxUsesPerGibPiece, full
// size, a skull for humans). Looked up by the dying monster's model, which the client still has when the gibs
// message arrives.
struct GibProfile
{
	std::string_view Model;
	int Count = 0;		// Exactly this many gibs, if set.
	int Multiplier = 0; // The gib count times this, if set (instead of GibCountMultiplier, which skips single gibs).
	int MaxUsesPerPiece = MaxUsesPerGibPiece;
	float Scale = 1;
	bool Skull = true;
	float Splash = 1; // Size of the blood splash where the body was.
};

static constexpr GibProfile GibProfiles[] = {
	// Full-size aliens have only 4 alien gib pieces, so they may repeat a little more (16 gibs).
	{.Model = "models/agrunt.mdl", .MaxUsesPerPiece = MaxUsesPerGibPieceLarge, .Splash = 1.25f},
	{.Model = "models/islave.mdl", .MaxUsesPerPiece = MaxUsesPerGibPieceLarge, .Splash = 1.25f},
	{.Model = "models/bullsquid.mdl", .MaxUsesPerPiece = MaxUsesPerGibPieceLarge, .Splash = 1.25f},
	{.Model = "models/controller.mdl", .MaxUsesPerPiece = MaxUsesPerGibPieceLarge, .Splash = 1.25f},
	{.Model = "models/zombie.mdl", .MaxUsesPerPiece = MaxUsesPerGibPieceLarge, .Splash = 1.25f},
	{.Model = "models/zombie_barney.mdl", .MaxUsesPerPiece = MaxUsesPerGibPieceLarge, .Splash = 1.25f},
	{.Model = "models/zombie_soldier.mdl", .MaxUsesPerPiece = MaxUsesPerGibPieceLarge, .Splash = 1.25f},
	{.Model = "models/gonome.mdl", .MaxUsesPerPiece = MaxUsesPerGibPieceLarge, .Splash = 1.25f},
	{.Model = "models/big_mom.mdl", .MaxUsesPerPiece = MaxUsesPerGibPieceLarge, .Splash = 1.25f},
	{.Model = "models/icky.mdl", .MaxUsesPerPiece = MaxUsesPerGibPieceLarge, .Splash = 1.25f},
	// Small creatures.
	{.Model = "models/headcrab.mdl", .Multiplier = 1, .Splash = 0.5f},
	{.Model = "models/baby_headcrab.mdl", .Count = 1, .Scale = 0.5f, .Splash = 0.25f},
	{.Model = "models/bigrat.mdl", .Count = 2, .Scale = 0.35f, .Skull = false, .Splash = 0.25f},
	// Each gib a barnacle coughs up (4 in its death animation) becomes 2.
	{.Model = "models/barnacle.mdl", .Multiplier = 2},
};

static const GibProfile& FindGibProfile(int entindex)
{
	static constexpr GibProfile Default{};

	const cl_entity_t* entity = gEngfuncs.GetEntityByIndex(entindex);

	if (!entity || !entity->model)
		return Default;

	const auto it = std::ranges::find(GibProfiles, std::string_view{entity->model->name}, &GibProfile::Model);
	return it != std::end(GibProfiles) ? *it : Default;
}

static bool ShowsBlood(int bloodColor)
{
	if (bloodColor == DONT_BLEED)
		return false;

	return 0 != (bloodColor == BLOOD_COLOR_RED ? g_violence_hblood : g_violence_ablood)->value;
}

// A trailing gib drops blood particles along its path (curstate.fuser2: when the trail ends; vuser1: where the
// last drop was left), like a thin blood stream following it.
static void EmitBloodTrail(TEMPENTITY* ent, float currenttime)
{
	const float remaining = ent->entity.curstate.fuser2 - currenttime;
	const int bloodColor = ent->entity.curstate.iuser1;

	if (remaining <= 0 || !ShowsBlood(bloodColor))
		return;

	const Vector from = ent->entity.curstate.vuser1;
	const Vector delta = ent->entity.origin - from;
	const float distance = delta.Length();

	// Drops get further apart as the trail runs out.
	const float spacing = BloodTrailSpacing / std::max(0.2f, remaining / BloodTrailTime);

	if (distance < spacing)
		return;

	const int baseColor = bloodColor == BLOOD_COLOR_RED ? BloodStreamRed : bloodColor == BLOOD_COLOR_YELLOW ? DarkYellow : bloodColor;
	const int drops = std::min(static_cast<int>(distance / spacing), 8);
	const Vector& gibVelocity = ent->entity.baseline.origin;

	for (int i = 0; i < drops; ++i)
	{
		particle_t* drop = gEngfuncs.pEfxAPI->R_AllocParticle(nullptr);

		if (!drop)
			break;

		drop->org = from + delta * ((i + RANDOM_FLOAT(0, 1)) / drops);
		drop->vel = gibVelocity * 0.15f + Vector{RANDOM_FLOAT(-10, 10), RANDOM_FLOAT(-10, 10), RANDOM_FLOAT(-10, 10)};
		drop->color = static_cast<short>(baseColor + RANDOM_LONG(0, 3));
		gEngfuncs.pEfxAPI->R_GetPackedColor(&drop->packedColor, drop->color);
		drop->type = pt_grav;
		drop->die = currenttime + RANDOM_FLOAT(0.6f, 1.0f);
	}

	ent->entity.curstate.vuser1 = ent->entity.origin;
}

// A blood puff that plays a moment later (curstate.fuser1: when; iuser1-3: color, spray and drop sprites;
// fuser2: size).
static void DelayedBloodPuff(TEMPENTITY* ent, float frametime, float currenttime)
{
	if (currenttime < ent->entity.curstate.fuser1)
		return;

	Vector origin = ent->entity.origin;
	gEngfuncs.pEfxAPI->R_BloodSprite(origin, ent->entity.curstate.iuser1, ent->entity.curstate.iuser2,
		ent->entity.curstate.iuser3, ent->entity.curstate.fuser2);
	ent->die = currenttime;
}

// Puts a decal where a downward trace from a point lands on a floor, if it does.
static void FloorDecal(const Vector& point, const char* decalName)
{
	Vector start = point;
	Vector end = point - Vector{0, 0, 256};

	pmtrace_t tr;
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(start, end, PM_WORLD_ONLY, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	// Floors only: not off a ledge, not up a wall or a steep slope.
	if (tr.fraction >= 1 || tr.plane.normal.z < 0.7f || r_decals->value == 0)
		return;

	const int decalId = gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(decalName);
	const int textureIndex = gEngfuncs.pEfxAPI->Draw_DecalIndex(decalId);
	gEngfuncs.pEfxAPI->R_DecalShoot(textureIndex, gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, &tr.endpos.x, 0);
}

// A pool of blood on the floor where a gibbed body was (as MMod does): the game's big blood splat, or for yellow
// blood, whose largest decal is smaller, three overlapping. Smaller for small creatures.
static void SpawnBloodPool(const Vector& origin, int bloodColor, float scale)
{
	if (scale <= 0 || !ShowsBlood(bloodColor))
		return;

	if (bloodColor == BLOOD_COLOR_RED)
	{
		FloorDecal(origin, scale >= 1 ? (RANDOM_LONG(0, 1) != 0 ? "{bigblood1" : "{bigblood2") : scale >= 0.5f ? "{blood5" : "{blood7");
		return;
	}

	if (scale < 1)
	{
		FloorDecal(origin, scale >= 0.5f ? "{yblood1" : "{yblood2");
		return;
	}

	for (int i = 0; i < 3; ++i)
	{
		FloorDecal(origin + Vector{RANDOM_FLOAT(-16, 16), RANDOM_FLOAT(-16, 16), 0}, "{yblood1");
	}
}

// A pool under a body that died without gibbing, once it has fallen (curstate.fuser1: when; iuser1: the body's
// entity; iuser2: blood color; fuser2: size).
static void DelayedBloodPool(TEMPENTITY* ent, float frametime, float currenttime)
{
	if (currenttime < ent->entity.curstate.fuser1)
		return;

	ent->die = currenttime;

	if (const cl_entity_t* body = gEngfuncs.GetEntityByIndex(ent->entity.curstate.iuser1); body && body->model)
	{
		SpawnBloodPool(body->origin + Vector{0, 0, 8}, ent->entity.curstate.iuser2, ent->entity.curstate.fuser2);
	}
}

static void MsgFunc_BloodPool(const char* name, BufferReader& reader)
{
	const int entindex = reader.ReadShort();
	const int bloodColor = reader.ReadByte();

	if (!JhlParticleSystemEnabled())
		return;

	const cl_entity_t* body = gEngfuncs.GetEntityByIndex(entindex);

	if (!body)
		return;

	if (TEMPENTITY* timer = gEngfuncs.pEfxAPI->CL_TempEntAllocNoModel(body->origin); timer)
	{
		const float now = gEngfuncs.GetClientTime();
		timer->flags |= FTENT_NOMODEL | FTENT_CLIENTCUSTOM;
		timer->callback = DelayedBloodPool;
		timer->entity.curstate.fuser1 = now + BloodPoolDelay;
		timer->entity.curstate.iuser1 = entindex;
		timer->entity.curstate.iuser2 = bloodColor;
		timer->entity.curstate.fuser2 = FindGibProfile(entindex).Splash;
		timer->die = now + BloodPoolDelay + 1;
	}
}

// A random point in a body around its center: wider than deep, and taller upward.
static Vector RandomInBody(float scale)
{
	return Vector{RANDOM_FLOAT(-8, 8), RANDOM_FLOAT(-8, 8), RANDOM_FLOAT(-12, 18)} * scale;
}

// A burst of blood where a gibbed body was, so it doesn't just vanish: a few of the game's hit blood puffs,
// staggered, and red blood chunks thrown out the way the gibs fly.
static void SpawnGibSplash(const Vector& origin, const Vector& attackdir, int bloodColor, float scale)
{
	if (scale <= 0 || !ShowsBlood(bloodColor))
		return;

	auto efx = gEngfuncs.pEfxAPI;
	const float now = gEngfuncs.GetClientTime();
	const int spray = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/bloodspray.spr");
	const int drop = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/blood.spr");

	const int puffColor = bloodColor == BLOOD_COLOR_RED ? SplashPuffRed : bloodColor == BLOOD_COLOR_YELLOW ? DarkYellow : bloodColor;

	if (spray != 0 && drop != 0)
	{
		const int puffs = std::max(1, static_cast<int>(std::lround(SplashBloodPuffs * scale)));
		const float size = std::max(3.f, SplashBloodSize * scale);

		for (int i = 0; i < puffs; ++i)
		{
			Vector position = origin + RandomInBody(scale);

			if (i == 0)
			{
				efx->R_BloodSprite(position, puffColor, spray, drop, size);
				continue;
			}

			if (TEMPENTITY* puff = efx->CL_TempEntAllocNoModel(position); puff)
			{
				puff->flags |= FTENT_NOMODEL | FTENT_CLIENTCUSTOM;
				puff->callback = DelayedBloodPuff;
				puff->entity.curstate.fuser1 = now + RANDOM_FLOAT(0, SplashStagger);
				puff->entity.curstate.iuser1 = puffColor;
				puff->entity.curstate.iuser2 = spray;
				puff->entity.curstate.iuser3 = drop;
				puff->entity.curstate.fuser2 = size;
				puff->die = now + SplashStagger + 1;
			}
		}
	}

	// Opposing Force's blood chunk sprite is red, so alien blood gets the puffs alone.
	const int chunk = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/blood_chnk.spr");

	if (bloodColor != BLOOD_COLOR_RED || chunk == 0)
		return;

	const int chunks = static_cast<int>(std::lround(SplashChunks * scale));

	for (int i = 0; i < chunks; ++i)
	{
		Vector position = origin + RandomInBody(scale);
		// Random, a little upward, and pushed the way the gibs fly (away from the attack).
		Vector velocity = Vector{RANDOM_FLOAT(-80, 80), RANDOM_FLOAT(-80, 80), RANDOM_FLOAT(-80, 80) + 60} - attackdir * 40;

		// Slightly translucent, so a little of the surroundings shows through the sprite's pure red, as with the decals.
		TEMPENTITY* sprite = efx->R_TempSprite(position, velocity, 0.4f, chunk, kRenderTransAlpha, kRenderFxNone, 0.9f, 4.5f,
			FTENT_SPRANIMATE | FTENT_GRAVITY | FTENT_COLLIDEWORLD);

		if (sprite)
		{
			sprite->entity.curstate.rendercolor = {255, 255, 255};
			// It disappears on its last frame, so a random frame rate staggers the chunks' lifetimes: its 4 frames
			// last 1.5-4 seconds.
			sprite->entity.curstate.framerate = 4 / RANDOM_FLOAT(1.5f, 4);
			sprite->entity.angles.z = RANDOM_FLOAT(0, 359);
		}
	}
}

static void EV_TFC_BloodDecalTrace(pmtrace_t* pTrace, int bloodColor)
{
	if (bloodColor == DONT_BLEED)
	{
		return;
	}

	const char* format;

	if (bloodColor == BLOOD_COLOR_RED)
	{
		if (g_violence_hblood->value == 0)
			return;
		format = "{{blood{}";
	}
	else
	{
		if (g_violence_ablood->value == 0)
			return;
		format = "{{yblood{}";
	}

	eastl::fixed_string<char, 32> decalname;
	fmt::format_to(std::back_inserter(decalname), fmt::runtime(format), RANDOM_LONG(0, 5) + 1);

	if (decalname.empty())
	{
		return;
	}

	if (physent_t* ent = gEngfuncs.pEventAPI->EV_GetPhysent(pTrace->ent); ent)
	{
		if (ent->solid == SOLID_BSP || ent->movetype == MOVETYPE_PUSHSTEP)
		{
			if (r_decals->value != 0)
			{
				const int entIndex = gEngfuncs.pEventAPI->EV_IndexFromTrace(pTrace);
				const int decalId = gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(decalname.c_str());
				const int textureIndex = gEngfuncs.pEfxAPI->Draw_DecalIndex(decalId);
				gEngfuncs.pEfxAPI->R_DecalShoot(textureIndex, entIndex, 0, &pTrace->endpos.x, 0);
			}
		}
	}
}

// State-based version of the server's CGib class lifetime.
static void EV_TFC_GibCallback(TEMPENTITY* ent, float frametime, float currenttime)
{
	switch (GibState(ent->entity.curstate.playerclass))
	{
		// WaitTillLand
	default:
		EmitBloodTrail(ent, currenttime);

		if (ent->entity.baseline.origin.Length() == 0)
		{
			ent->entity.curstate.playerclass = int(GibState::WaitTillExpire);
			ent->entity.baseline.fuser1 = JhlParticleSystemEnabled() ? JhlGibLife : g_cl_giblife->value;
		}

		ent->die = gEngfuncs.GetClientTime() + 1;
		return;

	case GibState::WaitTillExpire:
		ent->entity.baseline.fuser1 = ent->entity.baseline.fuser1 - frametime;

		if (ent->entity.baseline.fuser1 <= 0)
			ent->entity.curstate.playerclass = int(GibState::StartFadeOut);
		ent->die = gEngfuncs.GetClientTime() + 1;
		return;

	case GibState::StartFadeOut:
		ent->entity.curstate.playerclass = int(GibState::FadeOut);
		ent->entity.curstate.renderamt = 255;
		ent->entity.curstate.rendermode = kRenderTransTexture;
		ent->die = gEngfuncs.GetClientTime() + 2.5f;
		return;

	case GibState::FadeOut:
		if (ent->entity.curstate.renderamt <= 7)
		{
			ent->entity.curstate.renderamt = 0;
			ent->die = gEngfuncs.GetClientTime() + 0.2f;
			ent->entity.curstate.playerclass = int(GibState::Remove);
		}
		else
		{
			ent->die = gEngfuncs.GetClientTime() + 1;
			ent->entity.curstate.renderamt -= frametime * 70;
		}
		return;

	case GibState::Remove: return;
	}
}

static void EV_TFC_GibTouchCallback(TEMPENTITY* ent, pmtrace_t* ptr)
{
	// Bounce sounds play after this callback, so a gib out of bounce sounds is silenced before its next one.
	if (0 != ent->hitSound)
	{
		if (ent->entity.curstate.iuser2 <= 0)
			ent->hitSound = 0;
		else
			--ent->entity.curstate.iuser2;
	}

	if (ent->entity.baseline.origin.Length() > 0 && ent->entity.curstate.team > 0)
	{
		EV_TFC_BloodDecalTrace(ptr, ent->entity.curstate.iuser1);
		--ent->entity.curstate.team;
	}
}

static void EV_TFC_GibVelocityCheck(Vector& vel)
{
	if (const float length = vel.Length(); length > 1500)
	{
		vel = vel * (1500 / length);
	}
}

static TEMPENTITY* EV_TFC_CreateGib(const char* modelName, int body, byte bloodColor,
	const Vector& origin, const Vector& attackdir, GibVelocityMultiplier multiplier, bool ishead)
{
	int modelindex;
	model_t* model = gEngfuncs.CL_LoadModel(modelName, &modelindex);

	if (!model)
	{
		return nullptr;
	}

	Vector mins0, maxs0;
	gEngfuncs.pEventAPI->EV_LocalPlayerBounds(0, mins0, maxs0);
	TEMPENTITY* ent = gEngfuncs.pEfxAPI->CL_TentEntAllocCustom(origin, model, 0, EV_TFC_GibCallback);

	if (!ent)
	{
		return nullptr;
	}

	ent->entity.curstate.body = body;

	for (int i = 0; i < 3; ++i)
	{
		ent->entity.origin[i] += RANDOM_FLOAT(-1.f, 1.f) * (maxs0[i] - mins0[i]);
	}

	if (ishead)
	{
		ent->entity.curstate.velocity.x = RANDOM_FLOAT(-100, 100);
		ent->entity.curstate.velocity.y = RANDOM_FLOAT(-100, 100);
		ent->entity.curstate.velocity.z = RANDOM_FLOAT(200, 300);
		ent->entity.baseline.angles.x = RANDOM_FLOAT(100, 200);
		ent->entity.baseline.angles.y = RANDOM_FLOAT(100, 300);
	}
	else
	{
		const float scale = [&]()
		{
			// Equivalent of server-side health multiplier.
			switch (multiplier)
			{
			case GibVelocityMultiplier::Fraction: return 0.7f;
			case GibVelocityMultiplier::Double: return 2.f;
			default: return 4.f;
			}
		}();

		const float vmultiple = scale * g_cl_gibvelscale->value;

		Vector& velocity = ent->entity.curstate.velocity;

		// make the gib fly away from the attack vector
		velocity = attackdir * -1;

		// A wider sideways spread (johnhalflife), so gibs fan out rather than all flying one way when the blast is
		// level with the body.
		const float sidewaysSpread = JhlParticleSystemEnabled() ? JhlGibSidewaysSpread : 0.25f;

		for (int i = 0; i < 3; ++i)
		{
			const float spread = i < 2 ? sidewaysSpread : 0.25f;
			velocity[i] = (velocity[i] + RANDOM_FLOAT(-spread, spread)) * vmultiple * RANDOM_FLOAT(300, 400);
		}

		ent->entity.baseline.angles.x = RANDOM_FLOAT(100, 200);
		ent->entity.baseline.angles.y = RANDOM_FLOAT(100, 200);
	}

	if (JhlParticleSystemEnabled())
	{
		ent->entity.curstate.velocity = ent->entity.curstate.velocity * RANDOM_FLOAT(1 - JhlGibSpeedVariation, 1 + JhlGibSpeedVariation);

		// The model renderer only applies scale to entities marked like this (see StudioSetUpTransform).
		ent->entity.curstate.scale = RANDOM_FLOAT(1 - JhlGibScaleVariation, 1 + JhlGibScaleVariation);
		ent->entity.curstate.iuser4 = ScaledModelMarker;
	}

	EV_TFC_GibVelocityCheck(ent->entity.curstate.velocity);

	ent->flags |= FTENT_GRAVITY | FTENT_ROTATE | FTENT_COLLIDEWORLD;
	ent->entity.baseline.origin = ent->entity.curstate.velocity;
	ent->hitcallback = EV_TFC_GibTouchCallback;
	ent->die += 1;

	ent->entity.curstate.team = 5; // how many blood decals this gib can place (1 per bounce until none remain).
	ent->entity.curstate.solid = SOLID_NOT;
	ent->entity.curstate.playerclass = int(GibState::WaitTillLand);
	ent->entity.curstate.iuser1 = bloodColor;

	return ent;
}

void ClientGibs_Spawn(int entindex, int bloodColor, int numGibs, const Vector& origin, const Vector& angles,
	GibVelocityMultiplier multiplier, int flags)
{
	const auto type = GibType(flags & ~GibFlag_Mask);
	flags &= GibFlag_Mask;
	const int requestedGibs = numGibs;

	const auto gibData = [&]() -> const GibData*
	{
		switch (type)
		{
		case GibType::None: return nullptr;
		case GibType::Human: return &HumanGibs;
		case GibType::Alien: return &AlienGibs;
		case GibType::Pitdrone: return &PitDroneGibs;
		case GibType::Voltigore: return &VoltigoreGibs;
		case GibType::ShockTrooper: return &ShockTrooperGibs;
		default:
			g_GameLogger->error("Invalid gib type {}", int(type));
			return nullptr;
		}
	}();

	if ((flags & GibFlag_GibSound) != 0)
	{
		gEngfuncs.pEventAPI->EV_PlaySound(
			entindex, origin, CHAN_WEAPON, "common/bodysplat.wav", VOL_NORM, ATTN_NORM, 0, PITCH_NORM);
	}

	if (!gibData)
	{
		return;
	}

	if (numGibs == 0)
	{
		numGibs = std::clamp(int(g_cl_gibcount->value), 0, MAX_TEMPENTS);
	}

	const bool jhl = JhlParticleSystemEnabled();
	static constexpr GibProfile Vanilla{.MaxUsesPerPiece = MAX_TEMPENTS};
	const GibProfile& profile = jhl ? FindGibProfile(entindex) : Vanilla;

	if (jhl)
	{
		if (0 != profile.Count)
			numGibs = profile.Count;
		else if (0 != profile.Multiplier)
			numGibs *= profile.Multiplier;
		else if (numGibs > 1)
			numGibs *= GibCountMultiplier;

		numGibs = std::clamp(numGibs, 0, MAX_TEMPENTS);
	}

	// A death, not a single gib coughed up by a barnacle.
	if (jhl && requestedGibs != 1)
	{
		SpawnGibSplash(origin, angles, bloodColor, profile.Splash);
		SpawnBloodPool(origin, bloodColor, profile.Splash);
	}

	const int maxUsesPerPiece = profile.MaxUsesPerPiece;
	int soundingGibs = 0;
	int trailingGibs = 0;

	// Uses of each randomly picked piece, so none repeats more than MaxUsesPerGibPiece times in one death.
	eastl::fixed_vector<int, 64> pieceUses;
	pieceUses.resize(gibData->SubModelCount);

	// Track the number of uses of a particular submodel so we can avoid spawning too many of the same submodel
	eastl::fixed_vector<int, 64> limitTracking;

	if (gibData->Limits)
	{
		limitTracking.resize(gibData->SubModelCount);
	}

	int currentBody = 0;

	for (int i = 0; i < numGibs; ++i)
	{
		int body;

		if (gibData->Limits)
		{
			if (limitTracking[currentBody] >= gibData->Limits[currentBody].MaxGibs)
			{
				++currentBody;

				// We've hit the limit, stop spawning gibs.
				if (currentBody >= gibData->SubModelCount)
				{
					break;
				}
			}

			body = currentBody;

			++limitTracking[currentBody];
		}
		else if (jhl)
		{
			// A random piece among those not used up yet; stop once every piece has been used enough.
			eastl::fixed_vector<int, 64> available;

			for (int piece = gibData->FirstSubModel; piece < gibData->SubModelCount; ++piece)
			{
				if (pieceUses[piece] < maxUsesPerPiece)
				{
					available.push_back(piece);
				}
			}

			if (available.empty())
			{
				break;
			}

			body = available[RANDOM_LONG(0, int(available.size()) - 1)];
			++pieceUses[body];
		}
		else
		{
			body = RANDOM_LONG(gibData->FirstSubModel, gibData->SubModelCount - 1);
		}

		TEMPENTITY* gib = EV_TFC_CreateGib(gibData->ModelName, body, bloodColor, origin, angles, multiplier, false);

		if (gib && profile.Scale != 1)
		{
			gib->entity.curstate.scale *= profile.Scale;
		}

		// A few gibs squelch when they bounce (debris/flesh*.wav, louder the harder they hit).
		if (jhl && gib && soundingGibs < SoundingGibsPerDeath)
		{
			gib->hitSound = BOUNCE_FLESH;
			gib->entity.curstate.iuser2 = BounceSoundsPerGib;
			++soundingGibs;
		}

		if (jhl && gib && trailingGibs < TrailingGibsPerDeath)
		{
			gib->entity.curstate.fuser2 = gEngfuncs.GetClientTime() + BloodTrailTime;
			gib->entity.curstate.vuser1 = gib->entity.origin;
			++trailingGibs;
		}
	}

	if ((flags & GibFlag_SpawnHead) != 0 && profile.Skull)
	{
		EV_TFC_CreateGib(HumanHeadGibs.ModelName, 0, bloodColor, origin, angles, multiplier, true);
	}
}

static void MsgFunc_ClientGibs(const char* name, BufferReader& reader)
{
	const int entindex = reader.ReadShort();
	const byte bloodColor = reader.ReadByte();
	const byte numGibs = reader.ReadByte();
	const Vector origin = reader.ReadCoordVector();
	const Vector angles = reader.ReadCoordVector();

	const auto multiplier = GibVelocityMultiplier(reader.ReadByte());

	const int flags = reader.ReadByte();

	ClientGibs_Spawn(entindex, bloodColor, numGibs, origin, angles, multiplier, flags);
}

void ClientGibs_Initialize()
{
	g_cl_gibcount = g_ConCommands.CreateCVar("gibcount", "4", FCVAR_ARCHIVE);
	g_cl_giblife = g_ConCommands.CreateCVar("giblife", "25", FCVAR_ARCHIVE);
	g_cl_gibvelscale = g_ConCommands.CreateCVar("gibvelscale", "1.0", FCVAR_ARCHIVE);

	g_violence_hblood = g_ConCommands.GetCVar("violence_hblood");
	g_violence_ablood = g_ConCommands.GetCVar("violence_ablood");

	g_ClientUserMessages.RegisterHandler("ClientGibs", &MsgFunc_ClientGibs);
	g_ClientUserMessages.RegisterHandler("BloodPool", &MsgFunc_BloodPool);
}
