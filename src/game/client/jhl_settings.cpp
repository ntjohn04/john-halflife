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

constexpr int RecommendedSettingsVersion = 8;
constexpr int LastFullRecommendedVersion = 6;

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
	{"jhl_visuals_wateropacity", "0.85"},
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

constexpr RecommendedSetting HiddenSettings[] = {
	{"r_prefertexturefiltering", "0"},
	{"gl_ansio", "16"},
	{"gl_widescreen_yfov", "1"},
	{"gl_overbright", "1"},
	{"gl_use_shaders", "1"},
	{"gl_dither", "1"},
	{"gl_spriteblend", "1"},
	{"gl_fog", "1"},
	{"r_detailtextures", "0"},
	{"texgamma", "2.0"},
	{"lightgamma", "2.5"},
	{"cl_lw", "1"},
	{"cl_lc", "1"},
};

constexpr int FramesBeforeFirstLaunchCheck = 5;

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

	gEngfuncs.pfnClientCmd("_sethdmodels 0\n");
}

struct ResetWatch
{
	const char* Name;
	float Before;
	float After;
};

std::vector<ResetWatch> g_ResetChanged;
std::vector<ResetWatch> g_ResetWatches; // Those still watched.
std::chrono::steady_clock::time_point g_ResetWatchEnd;
constexpr std::chrono::minutes ResetWatchTime{10};

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
	g_ParticleSystem = gEngfuncs.pfnRegisterVariable("jhl_fx_particlesystem", "1", FCVAR_ARCHIVE);

	g_FpsFix = gEngfuncs.pfnRegisterVariable("jhl_physics_fpsfix", "1", FCVAR_ARCHIVE);

	gEngfuncs.pfnRegisterVariable("jhl_physics_wallfix", "1", FCVAR_ARCHIVE);

	gEngfuncs.pfnRegisterVariable("jhl_fx_keepcorpses", "1", FCVAR_ARCHIVE);

	gEngfuncs.pfnRegisterVariable("jhl_fx_tracers", "1", FCVAR_ARCHIVE);

	g_RecommendedVersion = gEngfuncs.pfnRegisterVariable("jhl_recommended_version", "0", FCVAR_ARCHIVE);

	// Ticked from the settings dialog; cleared again once applied.
	g_ResetRecommended = gEngfuncs.pfnRegisterVariable("jhl_reset_recommended", "0", 0);

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
