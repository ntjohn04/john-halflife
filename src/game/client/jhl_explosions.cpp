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
#include "event_api.h"
#include "jhl_explosion.h"
#include "jhl_explosions.h"
#include "jhl_settings.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "r_efx.h"

namespace
{
struct Fireball
{
	const char* Sprite;
	float Width; // Frame size in pixels; frames are centered on the origin.
	float Height;
	int Frames;
	float Fill = 1; // How much of the frame the fire fills, relative to the vanilla fireball (by eye).
};

// The vanilla fireball: 64 px, 15 frames at 15 fps (1 s).
constexpr Fireball Vanilla{"sprites/zerogxplode.spr", 64, 64, 15};

// Round fireballs, which look right anywhere.
constexpr Fireball RoundFireballs[] = {
	{"sprites/bexplo.spr", 64, 64, 24, 0.8f},
	{"sprites/cexplo.spr", 80, 80, 25, 0.85f},
	{"sprites/dexplo.spr", 80, 80, 26, 0.7f},
	{"sprites/eexplo.spr", 80, 80, 25, 0.85f},
};

// Fireballs that rise from the bottom of the frame (a wide blast, a fire column, a mushroom cloud), so they stand on a
// floor. The mushroom (the last) is for the biggest explosions only.
constexpr Fireball ShapedFireballs[] = {
	{"sprites/fexplo.spr", 80, 96, 30},
	{"sprites/gexplo.spr", 80, 96, 23},
	{"sprites/hexplo.spr", 64, 96, 30},
};

// Explosions sent at 15 fps get two fireballs: grenades, rockets, satchels, tripmines, mortars, env_explosion and the
// crossbow bolt. Opposing Force's mortar (10 fps) keeps the vanilla fireball.
constexpr float JhlFramerate = 15;

// A falling helicopter (Apache, Osprey) sends one fireball every 0.2 s under its hull, scale 3-5.9 at 12 fps (1.25 s);
// each is drawn from the vanilla and round fireballs instead.
constexpr float FallingFramerate = 12;
constexpr float FallingSizeMin = 1;
constexpr float FallingSizeMax = 1.2f;

constexpr float LargeScale = 6;	  // Satchel, tripmine, Apache rocket, env_explosion magnitude 150+.
constexpr float BiggestScale = 9; // func_mortar, env_explosion magnitude 200+: the mushroom cloud joins the pool.
constexpr float RocketScale = 1.5f;

constexpr float Jitter = 0.1f;		 // Scale and frame rate, both fireballs.
constexpr float PositionJitter = 4; // Units, the vanilla fireball.

// The second fireball, relative to the first's size; delayed a little, set off sideways and up (on a floor, down) by a
// fraction of the first's size, dimmer (the vanilla fireball is additive at 180) since the two add up.
constexpr float RoundSecondSizeMin = 0.9f;
constexpr float RoundSecondSizeMax = 1.1f;
constexpr float ShapedSecondSizeMin = 0.8f;
constexpr float ShapedSecondSizeMax = 1.0f;
constexpr float SecondDelay = 0.1f;
constexpr float SecondSideways = 0.06f;
constexpr float SecondUp = 0.1f;
constexpr float SecondDownOnFloorMin = 0.05f;
constexpr float SecondDownOnFloorMax = 0.15f;
constexpr int SecondBrightness = 140;

// How long the second fireball plays: about as long as the vanilla one, longer for large explosions.
constexpr float SmallDurationMin = 0.9f;
constexpr float SmallDurationMax = 1.3f;
constexpr float LargeDurationMin = 1.6f;
constexpr float LargeDurationMax = 2.0f;

// The sprite's bottom rows are mostly empty, so a shaped fireball sinks this much of its height into the floor.
constexpr float ShapedSink = 0.05f;

// A big, faint smoke cloud that lingers after the vanilla smoke (steam1, which the server sends 0.3 s in: 20 units up,
// rising 30 units/s for 1.3 s, about (fireball scale × 43) units wide). Opposing Force's single-frame puff, so it can
// grow and fade smoothly; widths relative to the fireball's scale.
constexpr const char* LingeringSmokeSprite = "sprites/wep_smoke_01.spr";
constexpr float LingeringSmokeSpriteWidth = 64;
constexpr float LingeringSmokeDelay = 0.4f;
constexpr float LingeringSmokeLifeMin = 5;
constexpr float LingeringSmokeLifeMax = 7;
constexpr float LingeringSmokeStartWidth = 64; // 1.5× the vanilla smoke.
constexpr float LingeringSmokeEndWidth = 128;
constexpr float LingeringSmokeHeight = 20;
constexpr float LingeringSmokeRiseMin = 8;
constexpr float LingeringSmokeRiseMax = 12;
constexpr float LingeringSmokeDrift = 5;
constexpr int LingeringSmokeOpacity = 64; // Peak, of 255.
constexpr int LingeringSmokeGrayMin = 30;
constexpr int LingeringSmokeGrayMax = 40;
constexpr float LingeringSmokeFadeIn = 0.15f; // Fractions of its life.
constexpr float LingeringSmokeFadeOut = 0.5f;

float Random(float low, float high)
{
	return gEngfuncs.pfnRandomFloat(low, high);
}

void SpawnFireball(const Vector& origin, int model, float scale, float framerate, int brightness, float roll)
{
	TEMPENTITY* sprite = gEngfuncs.pEfxAPI->R_DefaultSprite(origin, model, framerate);

	if (!sprite)
		return;

	gEngfuncs.pEfxAPI->R_Sprite_Explode(sprite, scale, TE_EXPLFLAG_NONE);
	sprite->entity.curstate.renderamt = brightness;
	sprite->entity.angles.z = roll;
}

// A fireball that plays a moment later (curstate.fuser1: when; iuser1: model; fuser2: scale; fuser3: frame rate;
// iuser2: brightness; fuser4: roll).
void DelayedFireball(TEMPENTITY* ent, float frametime, float currenttime)
{
	if (currenttime < ent->entity.curstate.fuser1)
		return;

	const auto& state = ent->entity.curstate;
	SpawnFireball(ent->entity.origin, state.iuser1, state.fuser2, state.fuser3, state.iuser2, state.fuser4);
	ent->die = currenttime;
}

// Where a floor is under an explosion, if one is close enough for it to have hit it: explosions are placed
// (dmg - 24) * 0.6 units out of what they hit, which is scale * 10 + 15.6.
bool FindFloor(const Vector& origin, float scale, Vector& floor)
{
	const Vector end = origin - Vector{0, 0, scale * 10 + 48};

	pmtrace_t tr;
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(origin, end, PM_WORLD_ONLY, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.fraction >= 1 || tr.plane.normal.z < 0.7f)
		return false;

	floor = tr.endpos;
	return true;
}

// Grows and fades the lingering smoke (curstate.fuser1: start time; fuser2: life; fuser3, fuser4: start and end scale).
void LingeringSmokeThink(TEMPENTITY* ent, float frametime, float currenttime)
{
	const auto& state = ent->entity.curstate;
	const float t = (currenttime - state.fuser1) / state.fuser2;

	if (t < 0)
	{
		ent->entity.curstate.renderamt = 0;
		return;
	}

	float opacity = 1;

	if (t < LingeringSmokeFadeIn)
		opacity = t / LingeringSmokeFadeIn;
	else if (t > 1 - LingeringSmokeFadeOut)
		opacity = std::max(0.f, (1 - t) / LingeringSmokeFadeOut);

	ent->entity.curstate.renderamt = static_cast<int>(LingeringSmokeOpacity * opacity);
	ent->entity.curstate.scale = state.fuser3 + (state.fuser4 - state.fuser3) * std::min(1.f, t);
}

void LingeringSmoke(const Vector& origin, float scale)
{
	const int model = gEngfuncs.pEventAPI->EV_FindModelIndex(LingeringSmokeSprite);

	if (model == 0)
		return;

	const float life = Random(LingeringSmokeLifeMin, LingeringSmokeLifeMax);
	const Vector velocity{Random(-LingeringSmokeDrift, LingeringSmokeDrift), Random(-LingeringSmokeDrift, LingeringSmokeDrift),
		Random(LingeringSmokeRiseMin, LingeringSmokeRiseMax)};

	TEMPENTITY* smoke = gEngfuncs.pEfxAPI->R_TempSprite(origin + Vector{0, 0, LingeringSmokeHeight}, velocity, 0, model,
		kRenderTransAlpha, kRenderFxNone, 0, LingeringSmokeDelay + life, 0);

	if (!smoke)
		return;

	const float now = gEngfuncs.GetClientTime();
	const auto gray = static_cast<byte>(gEngfuncs.pfnRandomLong(LingeringSmokeGrayMin, LingeringSmokeGrayMax));

	smoke->flags |= FTENT_CLIENTCUSTOM;
	smoke->callback = LingeringSmokeThink;
	smoke->entity.curstate.rendercolor = {gray, gray, gray};
	smoke->entity.curstate.renderamt = 0;
	smoke->entity.curstate.fuser1 = now + LingeringSmokeDelay;
	smoke->entity.curstate.fuser2 = life;
	smoke->entity.curstate.fuser3 = scale * LingeringSmokeStartWidth / LingeringSmokeSpriteWidth;
	smoke->entity.curstate.fuser4 = scale * LingeringSmokeEndWidth / LingeringSmokeSpriteWidth;
	smoke->entity.curstate.scale = smoke->entity.curstate.fuser3;
	smoke->entity.angles.z = Random(0, 360);
}

// One of a falling helicopter's fireballs: the vanilla one or a round one, the same size and length.
void FallingFireball(const Vector& origin, int model, float scale)
{
	const int pick = gEngfuncs.pfnRandomLong(0, std::size(RoundFireballs));
	const Fireball& fireball = pick == 0 ? Vanilla : RoundFireballs[pick - 1];
	const int fireballModel = pick == 0 ? model : gEngfuncs.pEventAPI->EV_FindModelIndex(fireball.Sprite);

	if (fireballModel == 0)
		return;

	const float size = scale * Vanilla.Width * Random(FallingSizeMin, FallingSizeMax);
	const float duration = Vanilla.Frames / FallingFramerate * Random(1 - Jitter, 1 + Jitter);

	SpawnFireball(origin, fireballModel, size / (fireball.Width * fireball.Fill), fireball.Frames / duration, 180,
		Random(0, 360));
}
} // namespace

bool JhlExplosion_Fireballs(const Vector& origin, int model, float scale, float framerate, int flags)
{
	if (!JhlParticleSystemEnabled() || (flags & TE_EXPLFLAG_NOADDITIVE) != 0)
		return false;

	// Not the underwater fireball.
	if (model != gEngfuncs.pEventAPI->EV_FindModelIndex(Vanilla.Sprite))
		return false;

	if (framerate == FallingFramerate)
	{
		FallingFireball(origin, model, scale);
		return true;
	}

	if (framerate != JhlFramerate)
		return false;

	const bool rocket = (flags & TE_EXPLFLAG_JHL_ROCKET) != 0;
	const bool large = rocket || scale >= LargeScale;
	const float firstScale = scale * (rocket ? RocketScale : 1) * Random(1 - Jitter, 1 + Jitter);
	const float size = firstScale * Vanilla.Width;

	const Vector jitter{Random(-PositionJitter, PositionJitter), Random(-PositionJitter, PositionJitter),
		Random(-PositionJitter, PositionJitter)};
	SpawnFireball(origin + jitter, model, firstScale, JhlFramerate * Random(1 - Jitter, 1 + Jitter), 180, Random(0, 360));

	LingeringSmoke(origin, scale * (rocket ? RocketScale : 1));

	// The second fireball: shaped for large explosions on a floor, round otherwise.
	Vector floor;
	const bool onFloor = FindFloor(origin, scale, floor);
	const bool shaped = large && onFloor;

	const Fireball* second;

	if (shaped)
	{
		const int count = scale >= BiggestScale ? std::size(ShapedFireballs) : std::size(ShapedFireballs) - 1;
		second = &ShapedFireballs[gEngfuncs.pfnRandomLong(0, count - 1)];
	}
	else
	{
		second = &RoundFireballs[gEngfuncs.pfnRandomLong(0, std::size(RoundFireballs) - 1)];
	}

	const int secondModel = gEngfuncs.pEventAPI->EV_FindModelIndex(second->Sprite);

	if (secondModel == 0)
		return true;

	const float secondSize = size * (shaped ? Random(ShapedSecondSizeMin, ShapedSecondSizeMax) : Random(RoundSecondSizeMin, RoundSecondSizeMax));
	const float secondScale = secondSize / (second->Width * second->Fill);
	const float duration = large ? Random(LargeDurationMin, LargeDurationMax) : Random(SmallDurationMin, SmallDurationMax);

	Vector secondOrigin = origin + Vector{Random(-1, 1) * SecondSideways * size, Random(-1, 1) * SecondSideways * size, 0};

	if (shaped)
	{
		// Stand it on the floor (R_Sprite_Explode raises sprites 10 units).
		const float height = second->Height * secondScale;
		secondOrigin.z = floor.z + height * (0.5f - ShapedSink) - 10;
	}
	else if (onFloor)
	{
		secondOrigin.z -= Random(SecondDownOnFloorMin, SecondDownOnFloorMax) * size;
	}
	else
	{
		secondOrigin.z += Random(0, SecondUp * size);
	}

	if (TEMPENTITY* timer = gEngfuncs.pEfxAPI->CL_TempEntAllocNoModel(secondOrigin); timer)
	{
		const float now = gEngfuncs.GetClientTime();

		timer->flags |= FTENT_NOMODEL | FTENT_CLIENTCUSTOM;
		timer->callback = DelayedFireball;
		timer->entity.curstate.fuser1 = now + Random(0, SecondDelay);
		timer->entity.curstate.iuser1 = secondModel;
		timer->entity.curstate.fuser2 = secondScale;
		timer->entity.curstate.fuser3 = second->Frames / duration;
		timer->entity.curstate.iuser2 = SecondBrightness;
		timer->entity.curstate.fuser4 = shaped ? 0 : Random(0, 360);
		timer->die = now + SecondDelay + 1;
	}

	return true;
}
