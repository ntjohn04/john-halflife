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

// jhl_fx_test: plays effects where the player is aiming, for designing particle effects
// (see docs/johnhalflife/effects_test.md). Client side only: nothing is sent to the server, and only the local
// player sees the result. Presets reproduce what the game does for an event with the original values; primitives
// play one building block with any parameters.

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "hud.h"
#include "fx_test.h"
#include "client_gibs.h"
#include "jhl_explosion.h"
#include "jhl_explosions.h"
#include "jhl_settings.h"
#include "com_model.h"
#include "event_api.h"
#include "r_efx.h"
#include "dlight.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "view.h"
#include "sound/ISoundSystem.h"

namespace
{
/**
 *	@brief key=value command arguments. Every value read is recorded, so what was played can be printed with all
 *	its parameters, defaults included.
 */
class Params
{
public:
	Params() = default;

	explicit Params(int firstArgument)
	{
		for (int i = firstArgument; i < gEngfuncs.Cmd_Argc(); ++i)
		{
			const std::string argument = gEngfuncs.Cmd_Argv(i);

			if (const auto equals = argument.find('='); equals != std::string::npos)
			{
				m_Values[argument.substr(0, equals)] = argument.substr(equals + 1);
			}
		}
	}

	// Sets a value, as if it had been given on the command line (for presets).
	Params& Set(const char* name, float value)
	{
		m_Values[name] = fmt::format("{:g}", value);
		return *this;
	}

	Params& Set(const char* name, const char* value)
	{
		m_Values[name] = value;
		return *this;
	}

	Params& Set(const char* name, const Vector& color)
	{
		m_Values[name] = fmt::format("{:g},{:g},{:g}", color.x, color.y, color.z);
		return *this;
	}

	float Float(const char* name, float defaultValue)
	{
		float value = defaultValue;

		if (auto it = m_Values.find(name); it != m_Values.end())
		{
			value = static_cast<float>(std::atof(it->second.c_str()));
		}

		Record(name, fmt::format("{:g}", value));
		return value;
	}

	int Int(const char* name, int defaultValue)
	{
		return static_cast<int>(Float(name, static_cast<float>(defaultValue)));
	}

	std::string String(const char* name, const char* defaultValue)
	{
		std::string value = defaultValue;

		if (auto it = m_Values.find(name); it != m_Values.end())
		{
			value = it->second;
		}

		Record(name, value);
		return value;
	}

	// "r,g,b", 0-255 each.
	Vector Color(const char* name, const Vector& defaultValue)
	{
		Vector value = defaultValue;

		if (auto it = m_Values.find(name); it != m_Values.end())
		{
			std::sscanf(it->second.c_str(), "%f,%f,%f", &value.x, &value.y, &value.z);
		}

		Record(name, fmt::format("{:g},{:g},{:g}", value.x, value.y, value.z));
		return value;
	}

	// Prints what was played and clears the record for the next part of the effect.
	void Print(const char* what)
	{
		gEngfuncs.Con_Printf("  %s %s\n", what, m_Used.c_str());
		m_Used.clear();
	}

private:
	void Record(const char* name, const std::string& value)
	{
		if (!m_Used.empty())
		{
			m_Used += ' ';
		}

		m_Used += fmt::format("{}={}", name, value);
	}

	std::unordered_map<std::string, std::string> m_Values;
	std::string m_Used;
};

// Where the player is aiming: the surface point, its normal (facing the player if nothing was hit) and the view.
struct Target
{
	Vector Position;
	Vector Normal;
	Vector Eye;
	Vector Forward;
};

Target FindTarget()
{
	Vector forward, right, up;
	AngleVectors(v_angles, forward, right, up);

	Vector start = v_origin;
	Vector end = start + forward * 8192;

	pmtrace_t tr;
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	// Other players are solid, but not the local one, or the trace would stop inside them at once.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(gEngfuncs.GetLocalPlayer()->index - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(start, end, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	return {tr.endpos, tr.fraction < 1 ? Vector{tr.plane.normal} : -forward, start, forward};
}

// Finds a sprite or model, loading it if the map didn't precache it.
int ModelIndex(const std::string& name)
{
	if (const int index = gEngfuncs.pEventAPI->EV_FindModelIndex(name.c_str()); index != 0)
	{
		return index;
	}

	// A model the map didn't load gets index -1, which effects can't use (the engine reports "no model -1").
	int index = 0;
	gEngfuncs.CL_LoadModel(name.c_str(), &index);

	if (index <= 0)
	{
		gEngfuncs.Con_Printf("jhl_fx_test: %s isn't loaded by this map, so this effect can't use it here\n", name.c_str());
		return 0;
	}

	return index;
}

// R_TempSprite for any sprite file: one the map didn't load has no index, so its temp entity is built from the model,
// loaded as a HUD sprite (CL_LoadModel only finds what the map precached).
TEMPENTITY* TempSprite(const std::string& name, Vector origin, const Vector& velocity, float scale, int render, float alpha,
	float life, int flags)
{
	if (const int index = gEngfuncs.pEventAPI->EV_FindModelIndex(name.c_str()); index != 0)
	{
		return gEngfuncs.pEfxAPI->R_TempSprite(origin, velocity, scale, index, render, kRenderFxNone, alpha, life, flags);
	}

	const HSPRITE handle = gEngfuncs.pfnSPR_Load(name.c_str());
	auto model = handle > 0 ? const_cast<model_t*>(gEngfuncs.GetSpritePointer(handle)) : nullptr;

	if (!model)
	{
		gEngfuncs.Con_Printf("jhl_fx_test: can't load %s\n", name.c_str());
		return nullptr;
	}

	TEMPENTITY* sprite = gEngfuncs.pEfxAPI->CL_TempEntAlloc(origin, model);

	if (!sprite)
		return nullptr;

	sprite->flags |= flags;
	sprite->frameMax = static_cast<float>(model->numframes - 1);
	sprite->entity.baseline.origin = velocity;
	sprite->entity.curstate.scale = scale;
	sprite->entity.curstate.rendermode = render;
	sprite->entity.curstate.renderfx = kRenderFxNone;
	sprite->entity.curstate.renderamt = sprite->entity.baseline.renderamt = static_cast<int>(alpha * 255);
	sprite->die = gEngfuncs.GetClientTime() + life;

	return sprite;
}

float Random(float range)
{
	return gEngfuncs.pfnRandomFloat(-range, range);
}

int RenderMode(const std::string& name)
{
	if (name == "add")
		return kRenderTransAdd;
	if (name == "glow")
		return kRenderGlow;
	if (name == "normal")
		return kRenderNormal;
	return kRenderTransAlpha;
}

constexpr int BloodRed = 247;
constexpr int BloodYellow = 195;
constexpr int BloodStreamRed = 70;

// Parts of an effect that play later (smoke after an explosion, a falling helicopter's sequence).
struct Pending
{
	float Time;
	std::function<void()> Action;
};

std::vector<Pending> g_Pending;

void Later(float delay, std::function<void()> action)
{
	g_Pending.push_back({gEngfuncs.GetClientTime() + delay, std::move(action)});
}

// ---- Primitives: one building block each ------------------------------------------------------------------------

// The general particle: any sprite, moving, animating and fading.
void Sprite(const Target& target, Params& p)
{
	const auto name = p.String("sprite", "sprites/steam1.spr");
	const int count = p.Int("count", 1);
	const float scale = p.Float("scale", 0.3f);
	const float alpha = p.Float("alpha", 0.35f);
	const int render = RenderMode(p.String("render", "alpha"));
	const Vector color = p.Color("color", {255, 255, 255});
	const float fps = p.Float("fps", 24);
	const float life = p.Float("life", 2);
	const bool loop = 0 != p.Int("loop", 0);
	const float offset = p.Float("offset", 4);
	const float radius = p.Float("radius", 0);
	const float speed = p.Float("speed", 0);
	const float spread = p.Float("spread", 0);
	const float upward = p.Float("up", 0);
	const int gravity = p.Int("gravity", 0);
	const float fade = p.Float("fade", 0);
	const bool collide = 0 != p.Int("collide", 0);
	p.Print("sprite");

	int flags = loop ? FTENT_SPRCYCLE : FTENT_SPRANIMATE;

	if (gravity == 1)
		flags |= FTENT_SLOWGRAVITY;
	else if (gravity >= 2)
		flags |= FTENT_GRAVITY;

	if (fade > 0)
		flags |= FTENT_FADEOUT;

	if (collide)
		flags |= FTENT_COLLIDEWORLD;

	for (int i = 0; i < count; ++i)
	{
		Vector origin = target.Position + target.Normal * offset + Vector{Random(radius), Random(radius), Random(radius)};
		Vector velocity = target.Normal * speed + Vector{Random(spread), Random(spread), Random(spread) + upward};

		TEMPENTITY* sprite = TempSprite(name, origin, velocity, scale, render, alpha, life, flags);

		if (!sprite)
			continue;

		sprite->entity.curstate.rendercolor = {static_cast<byte>(color.x), static_cast<byte>(color.y), static_cast<byte>(color.z)};
		sprite->entity.curstate.framerate = fps;
		sprite->entity.angles.z = gEngfuncs.pfnRandomFloat(0, 359);
		sprite->fadeSpeed = fade;
	}
}

// The engine's explosion (TE_EXPLOSION): an additive animated sprite, a light, particles and a sound.
void Explosion(const Target& target, Params& p)
{
	const auto name = p.String("sprite", "sprites/zerogxplode.spr");
	const float scale = p.Float("scale", 3);
	const float fps = p.Float("fps", 15);
	// Out from the surface as far as a 100 damage grenade's: (dmg - 24) * 0.6.
	const float offset = p.Float("offset", 45.6f);
	int flags = TE_EXPLFLAG_NONE;
	flags |= 0 == p.Int("additive", 1) ? TE_EXPLFLAG_NOADDITIVE : 0;
	flags |= 0 == p.Int("light", 1) ? TE_EXPLFLAG_NODLIGHTS : 0;
	flags |= 0 == p.Int("sound", 1) ? TE_EXPLFLAG_NOSOUND : 0;
	flags |= 0 == p.Int("particles", 1) ? TE_EXPLFLAG_NOPARTICLES : 0;
	flags |= 0 != p.Int("rocket", 0) ? TE_EXPLFLAG_JHL_ROCKET : 0;
	p.Print("explosion");

	if (const int model = ModelIndex(name); model != 0)
	{
		R_Explosion(target.Position + target.Normal * offset, model, scale, fps, flags);
	}
}

// The engine's smoke puff (TE_SMOKE): a sprite rising slowly, drawn translucent.
void Smoke(const Target& target, Params& p)
{
	const auto name = p.String("sprite", "sprites/steam1.spr");
	const float scale = p.Float("scale", 4);
	const float fps = p.Float("fps", 12);
	const float offset = p.Float("offset", 45.6f);
	p.Print("smoke");

	if (const int model = ModelIndex(name); model != 0)
	{
		const Vector origin = target.Position + target.Normal * offset;

		if (TEMPENTITY* smoke = gEngfuncs.pEfxAPI->R_DefaultSprite(origin, model, fps); smoke)
		{
			gEngfuncs.pEfxAPI->R_Sprite_Smoke(smoke, scale);
		}
	}
}

// The lingering cloud after an explosion's smoke (jhl_explosions.cpp), by itself; scale is the fireball's.
void LingeringSmoke(const Target& target, Params& p)
{
	JhlLingeringSmoke settings;
	const float scale = p.Float("scale", 3);
	const float offset = p.Float("offset", 45.6f);
	settings.Delay = p.Float("delay", 0);
	settings.LifeMin = settings.LifeMax = p.Float("life", 6);
	settings.StartWidth = p.Float("start", settings.StartWidth);
	settings.EndWidth = p.Float("end", settings.EndWidth);
	settings.RiseMin = settings.RiseMax = p.Float("rise", 10);
	settings.Drift = p.Float("drift", settings.Drift);
	settings.Opacity = p.Int("opacity", settings.Opacity);
	settings.GrayMin = settings.GrayMax = p.Int("gray", 35);
	p.Print("lingersmoke");

	if (!JhlExplosion_LingeringSmoke(target.Position + target.Normal * offset, scale, settings))
	{
		gEngfuncs.Con_Printf("jhl_fx_test: sprites/wep_smoke_01.spr isn't loaded; load a map again\n");
	}
}

// The engine's spark burst (TE_SPARKS): fixed look, no parameters but how many.
void Sparks(const Target& target, Params& p)
{
	const int count = p.Int("count", 1);
	p.Print("sparks");

	Vector origin = target.Position;

	for (int i = 0; i < count; ++i)
	{
		gEngfuncs.pEfxAPI->R_SparkShower(origin);
	}
}

// Spark streaks (R_SparkStreaks), as used on metal by the bullet impacts.
void Streaks(const Target& target, Params& p)
{
	const int count = p.Int("count", 4);
	const int minimum = p.Int("vmin", -120);
	const int maximum = p.Int("vmax", 120);
	const float offset = p.Float("offset", 2);
	p.Print("streaks");

	Vector origin = target.Position + target.Normal * offset;
	gEngfuncs.pEfxAPI->R_SparkStreaks(origin, count, minimum, maximum);
}

// A flying spark emitter like the game's spark_shower: thrown off the surface, sparking every 0.1 seconds while it
// slows down.
void ShowerCallback(TEMPENTITY* ent, float frametime, float currenttime)
{
	if (currenttime < ent->entity.baseline.scale)
		return;

	Vector origin = ent->entity.origin;
	gEngfuncs.pEfxAPI->R_SparkShower(origin);

	ent->entity.baseline.scale = currenttime + 0.1f;
	ent->entity.baseline.framerate -= 0.1f;

	if (ent->entity.baseline.framerate <= 0)
	{
		ent->die = currenttime;
	}
}

void Shower(const Target& target, Params& p)
{
	const int count = p.Int("count", 1);
	const float speedMin = p.Float("speedmin", 200);
	const float speedMax = p.Float("speedmax", 300);
	const float lifeMin = p.Float("lifemin", 0.5f);
	const float lifeMax = p.Float("lifemax", 1.5f);
	p.Print("shower");

	for (int i = 0; i < count; ++i)
	{
		TEMPENTITY* ent = gEngfuncs.pEfxAPI->CL_TempEntAllocNoModel(target.Position);

		if (!ent)
			continue;

		Vector velocity = target.Normal * gEngfuncs.pfnRandomFloat(speedMin, speedMax);
		velocity.x += Random(100);
		velocity.y += Random(100);
		velocity.z += velocity.z >= 0 ? 200 : -200;

		ent->flags |= FTENT_NOMODEL | FTENT_CLIENTCUSTOM | FTENT_GRAVITY | FTENT_COLLIDEWORLD;
		ent->callback = ShowerCallback;
		ent->bounceFactor = 0.6f;
		ent->entity.baseline.origin = velocity;
		ent->entity.baseline.scale = gEngfuncs.GetClientTime() + 0.1f;
		ent->entity.baseline.framerate = gEngfuncs.pfnRandomFloat(lifeMin, lifeMax);
		ent->die = gEngfuncs.GetClientTime() + lifeMax + 1;
	}
}

// The engine's blood (TE_BLOODSPRITE): a spray sprite plus droplets, in a palette color.
void Blood(const Target& target, Params& p)
{
	const int color = p.Int("color", BloodRed);
	const float size = p.Float("size", 10);
	const auto spray = p.String("spray", "sprites/bloodspray.spr");
	const auto drop = p.String("drop", "sprites/blood.spr");
	p.Print("blood");

	Vector origin = target.Position;
	gEngfuncs.pEfxAPI->R_BloodSprite(origin, color, ModelIndex(spray), ModelIndex(drop), size);
}

// A stream of blood particles arcing out of the surface (TE_BLOODSTREAM). The engine doesn't normalize the
// direction: particle speed is direction length x speed, so a long direction makes a long line.
void BloodStream(const Target& target, Params& p)
{
	// The stream picks colors color..color+9, so red is 70, not the blood sprite's 247.
	const int color = p.Int("color", BloodStreamRed);
	const int speed = p.Int("speed", 100);
	// normal: out of the surface; player: back toward you; up.
	const auto dir = p.String("dir", "normal");
	// Direction length; "dist" uses the distance to you, as the sniper rifle does.
	const auto length = p.String("length", "1");
	p.Print("bloodstream");

	Vector direction = dir == "player" ? (target.Eye - target.Position).Normalize()
					   : dir == "up"   ? Vector{0, 0, 1}
									   : target.Normal;

	direction = direction * (length == "dist" ? (target.Eye - target.Position).Length() : static_cast<float>(std::atof(length.c_str())));

	Vector origin = target.Position;
	gEngfuncs.pEfxAPI->R_BloodStream(origin, direction, color, speed);
}

int Material(const std::string& name)
{
	if (name == "glass")
		return BREAK_GLASS;
	if (name == "metal")
		return BREAK_METAL;
	if (name == "flesh")
		return BREAK_FLESH;
	if (name == "wood")
		return BREAK_WOOD;
	if (name == "concrete")
		return BREAK_CONCRETE;
	return 0;
}

// Model debris (TE_BREAKMODEL), as breakables and destroyed vehicles throw.
void Debris(const Target& target, Params& p)
{
	const auto name = p.String("model", "models/metalplategibs.mdl");
	const int count = p.Int("count", 8);
	const float size = p.Float("size", 32);
	const float speed = p.Float("speed", 100);
	const float spread = p.Float("spread", 100);
	const float life = p.Float("life", 3);
	const int material = Material(p.String("material", "metal"));
	p.Print("debris");

	if (const int model = ModelIndex(name); model != 0)
	{
		Vector origin = target.Position + target.Normal * (size * 0.5f);
		Vector box{size, size, size};
		Vector velocity = target.Normal * speed;
		gEngfuncs.pEfxAPI->R_BreakModel(origin, box, velocity, spread, life, count, model, static_cast<char>(material));
	}
}

// A dynamic light.
void Light(const Target& target, Params& p)
{
	const float radius = p.Float("radius", 200);
	const Vector color = p.Color("color", {255, 180, 96});
	const float life = p.Float("life", 1);
	const float decay = p.Float("decay", 0);
	const float offset = p.Float("offset", 8);
	p.Print("light");

	if (dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(0); light)
	{
		light->origin = target.Position + target.Normal * offset;
		light->radius = radius;
		light->color.r = static_cast<byte>(color.x);
		light->color.g = static_cast<byte>(color.y);
		light->color.b = static_cast<byte>(color.z);
		light->die = gEngfuncs.GetClientTime() + life;
		light->decay = decay;
	}
}

// A beam from just below the player's eyes to the target.
void Beam(const Target& target, Params& p)
{
	const auto name = p.String("sprite", "sprites/lgtning.spr");
	const float width = p.Float("width", 5);
	const float noise = p.Float("noise", 0.2f);
	const float brightness = p.Float("brightness", 1);
	const Vector color = p.Color("color", {180, 255, 96});
	const float life = p.Float("life", 0.5f);
	const float speed = p.Float("speed", 0);
	const float fps = p.Float("fps", 10);
	p.Print("beam");

	if (const int model = ModelIndex(name); model != 0)
	{
		Vector start = target.Eye + target.Forward * 16 - Vector{0, 0, 8};
		Vector end = target.Position;
		gEngfuncs.pEfxAPI->R_BeamPoints(start, end, model, life, width, noise, brightness, speed, 0, fps,
			color.x / 255, color.y / 255, color.z / 255);
	}
}

// The engine's built-in particle effects (palette colors 0-255).
void Particles(const Target& target, Params& p)
{
	const auto type = p.String("type", "burst");
	Vector origin = target.Position + target.Normal * 2;
	Vector direction = target.Normal;
	auto efx = gEngfuncs.pEfxAPI;

	if (type == "explosion")
	{
		p.Print("particles");
		efx->R_ParticleExplosion(origin);
	}
	else if (type == "explosion2")
	{
		const int color = p.Int("color", 0);
		const int length = p.Int("length", 16);
		p.Print("particles");
		efx->R_ParticleExplosion2(origin, color, length);
	}
	else if (type == "blob")
	{
		p.Print("particles");
		efx->R_BlobExplosion(origin);
	}
	else if (type == "lava")
	{
		p.Print("particles");
		efx->R_LavaSplash(origin);
	}
	else if (type == "teleport")
	{
		p.Print("particles");
		efx->R_TeleportSplash(origin);
	}
	else if (type == "implosion")
	{
		const float radius = p.Float("radius", 64);
		const int count = p.Int("count", 32);
		const float life = p.Float("life", 1);
		p.Print("particles");
		efx->R_Implosion(origin, radius, count, life);
	}
	else if (type == "effect")
	{
		const int color = p.Int("color", 0);
		const int count = p.Int("count", 32);
		p.Print("particles");
		efx->R_RunParticleEffect(origin, direction, color, count);
	}
	else
	{
		const int size = p.Int("size", 16);
		const int color = p.Int("color", 0);
		const float life = p.Float("life", 1);
		p.Print("particles");
		efx->R_ParticleBurst(origin, size, color, life);
	}
}

// The engine's armor ricochet (TE_ARMOR_RICOCHET): a flash sprite and a ricochet sound.
void Ricochet(const Target& target, Params& p)
{
	const float scale = p.Float("scale", 1);
	const auto name = p.String("sprite", "sprites/richo1.spr");
	p.Print("ricochet");

	if (const int model = ModelIndex(name); model != 0)
	{
		Vector origin = target.Position + target.Normal * 2;
		gEngfuncs.pEfxAPI->R_RicochetSprite(origin, gEngfuncs.hudGetModelByIndex(model), 0.1f, scale);
		R_RicochetSound(origin);
	}
}

// The game's gibs (the ClientGibs message): body part models thrown away from the player, bleeding on bounces.
void Gibs(const Target& target, Params& p)
{
	const auto type = p.String("type", "human");
	const int count = p.Int("count", 0);
	const auto velocity = p.String("velocity", "quadruple");
	const bool head = 0 != p.Int("head", 1);
	const bool sound = 0 != p.Int("sound", 1);
	p.Print("gibs");

	const bool alien = type == "alien";
	int flags = static_cast<int>(alien ? GibType::Alien : GibType::Human);
	flags |= head && !alien ? GibFlag_SpawnHead : 0;
	flags |= sound ? GibFlag_GibSound : 0;

	const auto multiplier = velocity == "fraction" ? GibVelocityMultiplier::Fraction
							: velocity == "double" ? GibVelocityMultiplier::Double
												   : GibVelocityMultiplier::Quadruple;

	// Lifted off the surface, as a body's center would be.
	const Vector origin = target.Position + target.Normal * 24;
	const int player = gEngfuncs.GetLocalPlayer()->index;
	ClientGibs_Spawn(player, alien ? BloodYellow : BloodRed, count, origin, -target.Forward, multiplier, flags);
}

// ---- Presets: what the game does for an event, with the original values -------------------------------------

// The server sends explosion smoke 7% bigger with jhl_fx_particlesystem on (UTIL_JhlExplosionSmokeScale).
float ExplosionSmokeFactor()
{
	return JhlParticleSystemEnabled() ? 1.07f : 1.f;
}

// A grenade, rocket, satchel or tripmine: CGrenade::Explode and CGrenade::Smoke, scaled by the damage.
void GrenadePreset(const Target& target, Params& p)
{
	const float damage = p.Float("dmg", 100);
	const int rocket = p.Int("rocket", 0);
	p.Print("grenade (CGrenade::Explode; hand grenade, MP5 grenade and RPG 100, satchel and tripmine 150)");

	const float explosionScale = (damage - 50) * 0.06f;
	const float smokeScale = (damage - 50) * 0.08f;
	const float offset = (damage - 24) * 0.6f;

	Params explosion;
	explosion.Set("scale", explosionScale);
	explosion.Set("offset", offset);
	explosion.Set("rocket", static_cast<float>(rocket));
	Explosion(target, explosion);

	Later(0.3f, [=]()
		{
		Params smoke;
		smoke.Set("scale", smokeScale * ExplosionSmokeFactor());
		smoke.Set("offset", offset);
		gEngfuncs.Con_Printf("  (0.3 s later)\n");
		Smoke(target, smoke); });

	Params shower;
	shower.Set("count", gEngfuncs.pfnRandomLong(0, 3));
	Shower(target, shower);

	gEngfuncs.Con_Printf("  plus a scorch decal and a debris sound\n");
}

// An env_explosion: explosive barrels and crates (func_breakable), func_tank shells, scripted explosions.
void BarrelPreset(const Target& target, Params& p)
{
	const float magnitude = p.Float("magnitude", 100);
	p.Print("barrel (env_explosion; explosive breakables use their explosion magnitude)");

	const float scale = std::max(10.f, (magnitude - 50) * 0.6f) / 10;
	const float offset = (magnitude - 24) * 0.6f;

	Params explosion;
	explosion.Set("scale", scale);
	explosion.Set("offset", offset);
	Explosion(target, explosion);

	Later(0.3f, [=]()
		{
		Params smoke;
		smoke.Set("scale", scale * ExplosionSmokeFactor());
		smoke.Set("offset", offset);
		gEngfuncs.Con_Printf("  (0.3 s later)\n");
		Smoke(target, smoke); });

	Params shower;
	shower.Set("count", gEngfuncs.pfnRandomLong(0, 3));
	Shower(target, shower);
}

// A bullet or melee hit on a monster: SpawnBlood (CBaseMonster::TraceAttack), sized by the damage.
void BloodPreset(const Target& target, Params& p, const char* what, float multiplier)
{
	const float damage = p.Float("dmg", 30);
	const bool alien = p.String("type", "human") == "alien";
	p.Print(what);

	Params blood;
	blood.Set("color", alien ? BloodYellow : BloodRed);
	blood.Set("size", std::clamp(damage * multiplier / 10, 3.f, 16.f));
	Blood(target, blood);

	gEngfuncs.Con_Printf("  plus blood decals behind the hit (TraceBleed)\n");
}

// An M40A1 sniper rifle hit on a bleeding target (CBaseEntity::FireBulletsPlayer): a blood stream aimed back at
// the shooter, with the full distance as its direction, which stretches it into a line.
void SniperHitPreset(const Target& target, Params& p)
{
	const bool alien = p.String("type", "human") == "alien";
	p.Print("sniperhit (the sniper rifle's blood stream, plus the normal hit blood)");

	Params stream;
	stream.Set("color", static_cast<float>(alien ? BloodYellow : BloodStreamRed));
	stream.Set("speed", 150.f);
	stream.Set("dir", "player");
	stream.Set("length", "dist");
	BloodStream(target, stream);

	Params blood;
	blood.Set("color", static_cast<float>(alien ? BloodYellow : BloodRed));
	// 100 damage (plr_762_bullet), times 3 on a headshot, size damage / 10 clamped to 16.
	blood.Set("size", 10.f);
	Blood(target, blood);
}

// An Apache crashing: CApache::DyingThink, every 0.1 seconds for the 4 seconds it falls. Played around a point in
// front of the surface, since there's no helicopter.
void ApachePreset(const Target& target, Params& p)
{
	const float duration = p.Float("time", 4);
	p.Print("apache (CApache::DyingThink, every 0.1 s while it falls)");

	const Target center{target.Position + target.Normal * 160 + Vector{0, 0, 100}, {0, 0, 1}, target.Eye, target.Forward};
	bool first = true;

	for (float time = 0; time < duration; time += 0.1f)
	{
		Later(time, [=]()
			{
			// Only the first tick's parameters are printed.
			auto print = [&](const char* text) { if (first) gEngfuncs.Con_Printf("%s", text); };

			Target explosionTarget = center;
			explosionTarget.Position = center.Position + Vector{Random(150), Random(150), gEngfuncs.pfnRandomFloat(-150, -50)};
			Params explosion;
			explosion.Set("scale", gEngfuncs.pfnRandomLong(30, 59) / 10.f);
			explosion.Set("fps", 12.f);
			explosion.Set("offset", 0.f);
			if (first)
				Explosion(explosionTarget, explosion);
			else
				R_Explosion(explosionTarget.Position, ModelIndex("sprites/zerogxplode.spr"), gEngfuncs.pfnRandomLong(30, 59) / 10.f, 12, TE_EXPLFLAG_NONE);

			Target smokeTarget = center;
			smokeTarget.Position = center.Position + Vector{Random(150), Random(150), gEngfuncs.pfnRandomFloat(-150, -50)};
			Params smoke;
			smoke.Set("scale", 10.f);
			smoke.Set("fps", 10.f);
			if (first)
				Smoke(smokeTarget, smoke);
			else if (const int model = ModelIndex("sprites/steam1.spr"); model != 0)
			{
				if (TEMPENTITY* ent = gEngfuncs.pEfxAPI->R_DefaultSprite(smokeTarget.Position, model, 10); ent)
					gEngfuncs.pEfxAPI->R_Sprite_Smoke(ent, 10);
			}

			if (const int model = ModelIndex("models/metalplategibs_green.mdl"); model != 0)
			{
				print("  debris model=models/metalplategibs_green.mdl count=4 size=400,400,132 speed=0 spread=50 life=3 material=metal\n");
				Vector origin = center.Position;
				Vector box{400, 400, 132};
				Vector velocity{0, 0, 0};
				gEngfuncs.pEfxAPI->R_BreakModel(origin, box, velocity, 50, 3, 4, model, BREAK_METAL);
			} });

		first = false;
	}
}

// A vortigaunt's zap: CISlave::ZapBeam and the light when it fires.
void VortZapPreset(const Target& target, Params& p)
{
	p.Print("vortzap (CISlave: zap beam and its light; the beam's width is CBeam 50, matched by eye)");

	Params beam;
	Beam(target, beam);

	Params light;
	light.Set("radius", 80.f);
	light.Set("color", {255, 180, 96});
	light.Set("life", 1.f);
	Target lightTarget = target;
	lightTarget.Position = target.Eye + target.Forward * 32;
	Light(lightTarget, light);
}

struct Command
{
	const char* Name;
	const char* Help;
	std::function<void(const Target&, Params&)> Play;
};

const std::vector<Command>& Commands()
{
	static const std::vector<Command> commands{
		{"grenade", "dmg=100 rocket=0|1", GrenadePreset},
		{"barrel", "magnitude=100", BarrelPreset},
		{"blood", "dmg=30 type=human|alien", [](const Target& t, Params& p)
			{ BloodPreset(t, p, "blood (SpawnBlood from CBaseMonster::TraceAttack)", 1); }},
		{"headshot", "dmg=30 type=human|alien", [](const Target& t, Params& p)
			{ BloodPreset(t, p, "headshot (the same blood as any hit, with monster_head 3x damage)", 3); }},
		{"sniperhit", "type=human|alien", SniperHitPreset},
		{"apache", "time=4", ApachePreset},
		{"vortzap", "", VortZapPreset},
		{"sprite", "sprite= count= scale= alpha= render=alpha|add|glow|normal color=r,g,b fps= life= loop= offset= radius= speed= spread= up= gravity=0|1|2 fade= collide=", Sprite},
		{"explosion", "sprite= scale= fps= offset= additive= light= sound= particles= rocket=", Explosion},
		{"smoke", "sprite= scale= fps= offset=", Smoke},
		{"lingersmoke", "scale=3 opacity=64 gray=35 life=6 start=64 end=128 rise=10 drift=5 delay=0 offset=45.6", LingeringSmoke},
		{"sparks", "count=", Sparks},
		{"streaks", "count= vmin= vmax= offset=", Streaks},
		{"shower", "count= speedmin= speedmax= lifemin= lifemax=", Shower},
		{"blood_sprite", "color= size= spray= drop=", Blood},
		{"bloodstream", "color= speed= dir=normal|player|up length=<number>|dist", BloodStream},
		{"debris", "model= count= size= speed= spread= life= material=metal|wood|glass|flesh|concrete|none", Debris},
		{"light", "radius= color=r,g,b life= decay= offset=", Light},
		{"beam", "sprite= width= noise= brightness= color=r,g,b life= speed= fps=", Beam},
		{"particles", "type=burst|explosion|explosion2|blob|lava|teleport|implosion|effect size= color= life= length= radius= count=", Particles},
		{"ricochet", "scale= sprite=", Ricochet},
		{"gibs", "type=human|alien count= velocity=fraction|double|quadruple head= sound=", Gibs},
	};

	return commands;
}

void FxTestCommand()
{
	if (gEngfuncs.Cmd_Argc() < 2)
	{
		gEngfuncs.Con_Printf("Usage: jhl_fx_test <effect> [key=value ...]  (plays where you aim; see docs/johnhalflife/effects_test.md)\n");
		gEngfuncs.Con_Printf("Presets (the game's own effects, original values):\n");

		for (const auto& command : Commands())
		{
			if (command.Name == std::string_view{"sprite"})
				gEngfuncs.Con_Printf("Primitives (one building block, any parameters):\n");

			gEngfuncs.Con_Printf("  %s %s\n", command.Name, command.Help);
		}

		return;
	}

	const std::string name = gEngfuncs.Cmd_Argv(1);

	for (const auto& command : Commands())
	{
		if (name == command.Name)
		{
			Params params(2);
			gEngfuncs.Con_Printf("jhl_fx_test:\n");
			command.Play(FindTarget(), params);
			return;
		}
	}

	gEngfuncs.Con_Printf("jhl_fx_test: unknown effect \"%s\"; run jhl_fx_test for the list\n", name.c_str());
}
}

void FxTest_Initialize()
{
	gEngfuncs.pfnAddCommand("jhl_fx_test", &FxTestCommand);
}

void FxTest_RunFrame()
{
	if (g_Pending.empty())
		return;

	const float now = gEngfuncs.GetClientTime();

	// Due actions are taken out first, since playing one can't add to the list but erasing while calling would be fragile.
	std::vector<Pending> due;

	for (auto it = g_Pending.begin(); it != g_Pending.end();)
	{
		// A new map restarts the client clock; drop anything left over.
		if (it->Time <= now || it->Time > now + 60)
		{
			if (it->Time <= now)
				due.push_back(std::move(*it));

			it = g_Pending.erase(it);
		}
		else
		{
			++it;
		}
	}

	for (auto& pending : due)
	{
		pending.Action();
	}
}
