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

// Recommended settings for johnhalflife (see docs/README.md#settings).
// Vanilla settings are saved in config.cfg, so their defaults can't simply be changed without overriding what the
// player chose. Instead the recommended values are applied once, the first time a version of them is seen.

#include <chrono>
#include <span>
#include <vector>

#include "hud.h"
#include "jhl_settings.h"

namespace
{
struct RecommendedSetting
{
	const char* Name;
	const char* Value;
};

// The dialog's settings are applied on first launch too (the defaults are the recommended values). Raise the version
// when changing the list so existing players get the new values once. 2: all of them, not only crosshair_scale,
// m_rawinput and jhl_gore (a clean install's config had someone's own settings: 60 fps, a strafe roll three times ours).
// 3: crosshair, the engine's default being off. 4: auto-aim off (sv_aim, on by the engine's default), field of
// view 100. 5: gamma 1.92 and brightness 0 (else a new game folder took its first config's, Half-Life's, often an
// old default: gamma 2.5, brightness 1; the engine's are 2.2 and 0, and gamma can't go below 1.8). 6: the hidden
// settings below, texture filtering off among them, and the original (not HD) models. 7: jhl_hud_scale 0 (automatic:
// Valve's half-size HUD at 800x600 and below), and only that: a player's own settings aren't applied again, the HUD
// scale moved along only if still at the old default, 1. 8: crosshair_scale 0 (automatic: 1 at 800x600 and below, 2
// above, 3 from 2160 tall), likewise moved along only if still at the old default, 2.
constexpr int RecommendedSettingsVersion = 8;
constexpr int LastFullRecommendedVersion = 6;

// Applied on first launch and by a reset: every setting listed in the John Settings dialog (config/mod/user.scr), and
// the hidden ones below. Keep in sync with that file, whose defaults should match. jhl_gore: the content lock isn't used, so
// blood and gibs are on.
constexpr RecommendedSetting DialogSettings[] = {
	{"jhl_camera_bob", "1"},
	{"jhl_camera_viewroll", "1"},
	{"cl_rollangle", "0.65"},
	{"cl_rollspeed", "300"},
	{"jhl_camera_verticallag", "1"},
	{"jhl_camera_jumplandjolt", "1"},
	{"jhl_visuals_weaponlag", "1"},
	{"jhl_visuals_weaponwallpush", "1"},
	{"jhl_visuals_firstpersonbody", "1"},
	{"jhl_visuals_flushfaces", "1"},
	{"jhl_visuals_16bitcolor", "1"},
	{"jhl_visuals_softwarewater", "1"},
	{"jhl_visuals_waterlight", "1"},
	{"jhl_visuals_wateropacity", "0.96"},
	{"jhl_visuals_renderfixes", "1"},
	{"jhl_weapons_smoothrecoil", "1"},
	{"jhl_weapons_holster", "1"},
	{"jhl_physics_fpsfix", "1"},
	{"jhl_physics_wallfix", "1"},
	{"jhl_input_walktoggle", "1"},
	{"jhl_input_walkfix", "1"},
	{"jhl_fx_bulletimpacts", "1"},
	{"jhl_fx_muzzleflash", "1"},
	{"jhl_fx_particlesystem", "1"},
	{"jhl_fx_tracers", "1"},
	{"jhl_fx_keepcorpses", "1"},
	{"jhl_hud_scale", "0"},
	{"jhl_hud_minimal", "1"},
	{"jhl_hud_campaigncrosshair", "1"},
	{"jhl_snd_vanilla", "1"},
	{"jhl_snd_musicsaves", "1"},
	{"jhl_perf_preloadsounds", "0"},
	{"crosshair", "1"},
	{"crosshair_scale", "0"},
	{"default_fov", "100"},
	{"gamma", "1.92"},
	{"brightness", "0"},
	{"sv_aim", "0"},
	{"fps_override", "1"},
	{"fps_max", "240"},
	{"gl_vsync", "0"},
	{"m_rawinput", "1"},
	{"jhl_gore", "1"},
	{"mp_decals", "300"},
	{"cl_projectinfo_show", "0"},
	{"cl_bobtilt", "0"},
	{"cl_snd_hrtf_enabled", "0"},
	{"cl_snd_hrtf_implementation", ""},
};

// Applied with them but not in the dialog: settings the engine's own Video options already show (texture filtering,
// widescreen field of view), and technical ones a player would only change by mistake, kept at what johnhalflife is
// made and checked with. Mostly the engine's defaults; texture filtering is off (point sampled, as software drew; with
// jhl_visuals_renderfixes, mipmapped as software was). Only those the renderer has: the software one has no gl_*.
constexpr RecommendedSetting HiddenSettings[] = {
	{"r_prefertexturefiltering", "0"}, // Video options' texture filtering (gl_texturemode follows it).
	{"gl_ansio", "16"},				   // Anisotropic filtering: only seen with texture filtering on.
	{"gl_widescreen_yfov", "1"},	   // Video options: a widescreen shows more at the sides, the height as 4:3's.
	{"gl_overbright", "1"},			   // The world lit up to 4/3, as software did (and models with renderfixes).
	{"gl_use_shaders", "1"},
	{"gl_dither", "1"},
	{"gl_spriteblend", "1"},
	{"gl_fog", "1"},
	{"r_detailtextures", "0"}, // Detail textures (a fine pattern over walls up close, where a map has them): not 1998's.
	{"texgamma", "2.0"},	   // The textures' and the lighting's gamma, under gamma itself.
	{"lightgamma", "2.5"},
	{"cl_lw", "1"}, // Weapon and movement prediction, which the weapon effects are made for.
	{"cl_lc", "1"},
};

// The engine runs config.cfg from the command buffer before the client's first frames; wait a few to be safe.
constexpr int FramesBeforeFirstLaunchCheck = 5;

// The content lock's settings: Valve's lock sets all four to 0 and unlocking sets them back to 1, nothing else.
constexpr const char* GoreSettings[] = {"violence_hblood", "violence_hgibs", "violence_ablood", "violence_agibs"};

cvar_t* g_RecommendedVersion = nullptr;
cvar_t* g_ParticleSystem = nullptr;
cvar_t* g_FpsFix = nullptr;
cvar_t* g_Gore = nullptr;
float g_AppliedGore = -1;
cvar_t* g_ResetRecommended = nullptr;
int g_FrameCount = 0;

void Apply(std::span<const RecommendedSetting> settings)
{
	for (const auto& setting : settings)
	{
		gEngfuncs.Cvar_Set(setting.Name, setting.Value);
	}
}

// jhl_gore stands in for the four content lock settings, so the dialog can show them as one checkbox.
void SyncGoreFromContentLock()
{
	bool allOn = true;

	for (const char* name : GoreSettings)
	{
		allOn = allOn && 0 != gEngfuncs.pfnGetCvarFloat(name);
	}

	gEngfuncs.Cvar_Set(g_Gore->name, allOn ? "1" : "0");
	g_AppliedGore = allOn ? 1 : 0;
}

void ApplyGoreIfChanged()
{
	if (g_Gore->value == g_AppliedGore)
	{
		return;
	}

	g_AppliedGore = g_Gore->value;

	for (const char* name : GoreSettings)
	{
		gEngfuncs.Cvar_Set(name, 0 != g_AppliedGore ? "1" : "0");
	}
}

void ApplyRecommended()
{
	Apply(DialogSettings);

	for (const auto& setting : HiddenSettings)
	{
		if (gEngfuncs.pfnGetCvarPointer(setting.Name))
		{
			gEngfuncs.Cvar_Set(setting.Name, setting.Value);
		}
	}

	// The original models, not the HD pack (the 25th anniversary update's default too). A command, not a setting: it
	// saves the choice and, only if that changes it, reloads the game's files.
	gEngfuncs.pfnClientCmd("_sethdmodels 0\n");
}

// A reset from John Settings is applied as that dialog closes, but the Options window behind it still shows the old
// values (Video's gamma, brightness, texture filtering; Mouse's raw input), and its OK writes them all back. The menu
// is the engine's, so it can't be updated or asked whether it's open; instead, for a while after a reset, a setting
// going back to exactly its value before the reset is taken for that write, and the reset applied again. A setting
// changed to anything else is the player's own and left alone.
struct ResetWatch
{
	const char* Name;
	float Before;
	float After;
};

std::vector<ResetWatch> g_ResetChanged; // Every setting the reset changed.
std::vector<ResetWatch> g_ResetWatches; // Those still watched.
std::chrono::steady_clock::time_point g_ResetWatchEnd;
constexpr std::chrono::minutes ResetWatchTime{10};

// Once the Options window has written, a little longer, in case its writes come over more than one frame.
constexpr std::chrono::seconds ResetRewriteTime{2};

void WatchReset(const std::vector<ResetWatch>& before)
{
	g_ResetChanged.clear();

	for (const auto& watch : before)
	{
		const float after = gEngfuncs.pfnGetCvarFloat(watch.Name);

		if (after != watch.Before)
		{
			g_ResetChanged.push_back({watch.Name, watch.Before, after});
		}
	}

	g_ResetWatches = g_ResetChanged;
	g_ResetWatchEnd = std::chrono::steady_clock::now() + ResetWatchTime;
}

void ApplyAllRecommended()
{
	std::vector<ResetWatch> before;

	for (const auto& settings : {std::span<const RecommendedSetting>{DialogSettings}, std::span<const RecommendedSetting>{HiddenSettings}})
	{
		for (const auto& setting : settings)
		{
			if (gEngfuncs.pfnGetCvarPointer(setting.Name))
			{
				before.push_back({setting.Name, gEngfuncs.pfnGetCvarFloat(setting.Name), 0});
			}
		}
	}

	ApplyRecommended();
	WatchReset(before);
	gEngfuncs.Con_Printf("johnhalflife: recommended settings applied\n");
}

void CheckResetWatch()
{
	if (g_ResetWatches.empty())
	{
		return;
	}

	if (std::chrono::steady_clock::now() > g_ResetWatchEnd)
	{
		g_ResetWatches.clear();
		return;
	}

	bool written = false;

	for (auto watch = g_ResetWatches.begin(); watch != g_ResetWatches.end();)
	{
		const float value = gEngfuncs.pfnGetCvarFloat(watch->Name);

		if (value == watch->After)
		{
			++watch;
			continue;
		}

		// Back to its old value: the Options window's write. Anything else: the player's change, kept.
		written = written || value == watch->Before;
		watch = g_ResetWatches.erase(watch);
	}

	if (written)
	{
		ApplyRecommended();
		gEngfuncs.Con_Printf("johnhalflife: recommended settings applied again (the Options window wrote back its old values)\n");

		g_ResetWatches = g_ResetChanged;
		g_ResetWatchEnd = std::chrono::steady_clock::now() + ResetRewriteTime;
	}
}
}

bool JhlParticleSystemEnabled()
{
	return 0 != g_ParticleSystem->value;
}

bool JhlFpsFixEnabled()
{
	return 0 != g_FpsFix->value;
}

void JhlSettings_Initialize()
{
	// One switch for all of johnhalflife's particle system changes (see docs/effects.md).
	g_ParticleSystem = gEngfuncs.pfnRegisterVariable("jhl_fx_particlesystem", "1", FCVAR_ARCHIVE);

	// Half-Life's game code acting at any frame rate as it did at 72 fps (see jhl_framerate.h); the server reads it too.
	g_FpsFix = gEngfuncs.pfnRegisterVariable("jhl_physics_fpsfix", "1", FCVAR_ARCHIVE);

	// Walking along a wall no faster than in the open (pm_shared.cpp's PM_WalkMove); the server reads it too.
	gEngfuncs.pfnRegisterVariable("jhl_physics_wallfix", "1", FCVAR_ARCHIVE);

	// Corpses vanilla fades (a monstermaker's, or flagged by the map) kept (CBaseMonster::JhlFadeCorpse); the server reads it.
	gEngfuncs.pfnRegisterVariable("jhl_fx_keepcorpses", "1", FCVAR_ARCHIVE);

	// johnhalflife's tracers on their own (jhl_tracers.cpp); they also need the particle system.
	gEngfuncs.pfnRegisterVariable("jhl_fx_tracers", "1", FCVAR_ARCHIVE);

	// Archived, so it survives in config.cfg and the first launch values are only applied once per version.
	g_RecommendedVersion = gEngfuncs.pfnRegisterVariable("jhl_recommended_version", "0", FCVAR_ARCHIVE);

	// Ticked from the settings dialog; cleared again once applied.
	g_ResetRecommended = gEngfuncs.pfnRegisterVariable("jhl_reset_recommended", "0", 0);

	// Not archived: its value comes from the content lock settings, which the engine saves.
	g_Gore = gEngfuncs.pfnRegisterVariable("jhl_gore", "1", 0);

	gEngfuncs.pfnAddCommand("jhl_recommended", &ApplyAllRecommended);
}

void JhlSettings_RunFrame()
{
	if (g_FrameCount < FramesBeforeFirstLaunchCheck)
	{
		if (++g_FrameCount < FramesBeforeFirstLaunchCheck)
		{
			return;
		}

		SyncGoreFromContentLock();

		if (g_RecommendedVersion->value < LastFullRecommendedVersion)
		{
			ApplyRecommended();
			gEngfuncs.Cvar_Set(g_RecommendedVersion->name, std::to_string(RecommendedSettingsVersion).c_str());
			gEngfuncs.Con_Printf("johnhalflife: first launch, recommended settings applied\n");
		}
		else if (g_RecommendedVersion->value < RecommendedSettingsVersion)
		{
			if (g_RecommendedVersion->value < 7 && gEngfuncs.pfnGetCvarFloat("jhl_hud_scale") == 1)
			{
				gEngfuncs.Cvar_Set("jhl_hud_scale", "0");
			}

			if (g_RecommendedVersion->value < 8 && gEngfuncs.pfnGetCvarFloat("crosshair_scale") == 2)
			{
				gEngfuncs.Cvar_Set("crosshair_scale", "0");
			}

			gEngfuncs.Cvar_Set(g_RecommendedVersion->name, std::to_string(RecommendedSettingsVersion).c_str());
		}
	}

	if (0 != g_ResetRecommended->value)
	{
		gEngfuncs.Cvar_Set(g_ResetRecommended->name, "0");
		ApplyAllRecommended();
	}

	CheckResetWatch();
	ApplyGoreIfChanged();
}
